"""Deterministic opaque green-screen input; Python standard library only."""
from pathlib import Path
import math
import struct
import zlib

OUT = Path(__file__).resolve().parent.parent / 'build'
OUT.mkdir(exist_ok=True)
W, H = 512, 320
pixels = bytearray()

def rectangle(x, y, cx, cy, rx, ry):
    dx, dy = abs(x-cx)-rx, abs(y-cy)-ry
    return math.hypot(max(dx, 0), max(dy, 0)) + min(max(dx, dy), 0)

for y in range(H):
    for x in range(W):
        head = math.hypot(x+.5-256, y+.5-84)-32
        body = rectangle(x+.5, y+.5, 256, 192, 48, 70)
        arms = min(rectangle(x+.5, y+.5, 197, 188, 12, 50),
                   rectangle(x+.5, y+.5, 315, 188, 12, 50))
        distance = min(head, body, arms)
        alpha = min(1, max(0, .5-distance))
        color = (235, 221, 190) if head <= min(body, arms) else (22, 37, 63)
        pixels.extend(round(color[c]*alpha+(255 if c == 1 else 0)*(1-alpha)) for c in range(3))
        pixels.append(255)

(OUT / 'chroma-input.rgba').write_bytes(pixels)

def chunk(kind, data):
    return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data))

scan = b''.join(b'\0'+pixels[y*W*4:(y+1)*W*4] for y in range(H))
png = (b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 6, 0, 0, 0))
       +chunk(b'IDAT', zlib.compress(scan))+chunk(b'IEND', b''))
(OUT / 'chroma-input.png').write_bytes(png)
print('Green-screen fixture prepared')
