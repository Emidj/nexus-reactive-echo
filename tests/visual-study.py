"""Export a reproducible visual study from actual OBS/D3D11 renders."""
from pathlib import Path
import json, math, os, struct, subprocess, zlib

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'tests/evidence/visual-study'
OUT.mkdir(parents=True, exist_ok=True)
W, H = 512, 320
pixels = bytearray()
for y in range(H):
    for x in range(W):
        head = math.hypot(x+.5-256, y+.5-84)-32
        body = max(abs(x+.5-256)-60, abs(y+.5-202)-70)
        a = round(255*min(1, max(0, .5-min(head, body))))
        pixels.extend((235, 221, 190, a) if head < body else (22, 37, 63, a))
input_path = OUT / 'input.rgba'
input_path.write_bytes(pixels)
obs = Path(os.environ.get('OBS_INSTALL_DIR', str(Path(os.environ['ProgramFiles'])/'obs-studio')))
dll = ROOT / 'dist/nexus-reactive-echo/bin/64bit/nexus-reactive-echo.dll'
env = dict(os.environ)
env['PATH'] = str(obs/'bin/64bit') + os.pathsep + env['PATH']
with (OUT/'render.log').open('w', encoding='utf8') as log:
    subprocess.run([str(ROOT/'build/visual-study.exe'), str(obs), str(dll),
                    str(input_path), str(OUT/'sample')], env=env,
                   stdout=log, stderr=subprocess.STDOUT, check=True)

def chunk(kind, data):
    return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))

cases = [('Actual', .94, 1.015), ('Mayor expansión', .94, 1.04),
         ('Mayor expansión y persistencia', .98, 1.04)]
cards = []
for i, (name, feedback, zoom) in enumerate(cases):
    raw = (OUT/f'sample-{i}.rgba').read_bytes()
    if len(raw) != W*H*4:
        raise ValueError('Incomplete GPU output')
    # GPU output is premultiplied; RGB already represents composition over black.
    rgb = b''.join(raw[j:j+3] for j in range(0,len(raw),4))
    scan = b''.join(b'\0'+rgb[y*W*3:(y+1)*W*3] for y in range(H))
    (OUT/f'sample-{i}.png').write_bytes(b'\x89PNG\r\n\x1a\n'+
        chunk(b'IHDR',struct.pack('>IIBBBBB',W,H,8,2,0,0,0))+
        chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b''))
    cards.append(f'<figure><img src="sample-{i}.png"><figcaption>{name}: '
                 f'persistencia {feedback}, expansión {zoom}</figcaption></figure>')
(OUT/'index.html').write_text('''<!doctype html><meta charset="utf-8">
<title>NEXUS · comparación visual</title><style>
body{background:#111;color:#eee;font:17px system-ui;margin:32px;max-width:1500px}
main{display:flex;flex-wrap:wrap;gap:16px}figure{margin:0;flex:1;min-width:300px}
img{width:100%;background:#000}figcaption{padding:12px 0}</style>
<h1>NEXUS Reactive Echo · estudio de expansión</h1>
<p>Salida real del filtro en Direct3D 11. Silueta sintética estática, degradado cálido,
sin audio; 90 fotogramas renderizados por muestra. No demuestra semejanza con una cámara real.</p>
<main>'''+''.join(cards)+'''</main><p>La expansión y la persistencia cambian el ancho y alcance
del halo. Para valorar bandas y movimiento se necesita una cámara recortada sin el efecto previo.</p>''',encoding='utf8')
(OUT/'settings.json').write_text(json.dumps([
    dict(name=n,feedback=f,zoom=z,rotation=0,offset_x=0,amount=1,
         silhouette=True,trail_color=True,color_mode=1,gradient_palette=0,
         audio_enabled=False,rendered_frames=90) for n,f,z in cases],indent=2),encoding='utf8')
print(OUT/'index.html')
