"""Independent Windows x64 build; official OBS headers and Zig compiler only."""
from pathlib import Path
import os, struct, subprocess, json
ROOT = Path(__file__).resolve().parent
BUILD = ROOT / 'build'
BUILD.mkdir(exist_ok=True)
TOOLS = Path(os.environ.get('NEXUS_TOOLS_DIR', str(ROOT/'build-tools')))
ZIG = Path(os.environ.get('ZIG_EXE', str(TOOLS/'zig-x86_64-windows-0.15.2/zig.exe')))
SDK = Path(os.environ.get('OBS_SDK_DIR', str(TOOLS/'obs-studio-32.2.2')))
OBS = Path(os.environ.get('OBS_INSTALL_DIR', str(Path(os.environ['ProgramFiles'])/'obs-studio')))
if not ZIG.is_file() or not (SDK/'libobs/obs.h').is_file():
    raise SystemExit('Set NEXUS_TOOLS_DIR or ZIG_EXE and OBS_SDK_DIR to official Zig 0.15.2 and OBS 32.2.2 headers. See README.md.')
os.environ['ZIG_GLOBAL_CACHE_DIR'] = str(BUILD/'zig-cache')
os.environ['ZIG_LOCAL_CACHE_DIR'] = str(BUILD/'zig-local')
# Build an import library from the exports of the installed, official obs.dll.
# This reads the PE export table; it does not execute the DLL.
raw = (OBS / 'bin/64bit/obs.dll').read_bytes()
u16 = lambda p: struct.unpack_from('<H', raw, p)[0]
u32 = lambda p: struct.unpack_from('<I', raw, p)[0]
pe = u32(0x3c)
assert raw[pe:pe+4] == b'PE\0\0' and u16(pe+4) == 0x8664
opt = pe + 24
sections = opt + u16(pe+20)
def offset(rva):
    for i in range(u16(pe+6)):
        s = sections + i*40
        va, size = u32(s+12), max(u32(s+8), u32(s+16))
        if va <= rva < va+size:
            return u32(s+20) + rva-va
    raise ValueError('RVA not found')
exp = offset(u32(opt+112))
names = offset(u32(exp+32))
exports = []
for i in range(u32(exp+24)):
    p = offset(u32(names+4*i))
    exports.append(raw[p:raw.index(b'\0', p)].decode('ascii'))
(BUILD/'obs.def').write_text('LIBRARY obs.dll\nEXPORTS\n'+'\n'.join(exports)+'\n')
subprocess.run([str(ZIG), 'dlltool', '-m', 'i386:x86-64', '-d', str(BUILD/'obs.def'), '-l', str(BUILD/'obs.lib')], check=True)
(BUILD/'obsconfig.h').write_text('#pragma once\n#define OBS_RELEASE_CANDIDATE 0\n#define OBS_BETA 0\n')

common = [str(ZIG), 'cc', '-target', 'x86_64-windows-gnu', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-I'+str(SDK/'libobs'), '-I'+str(BUILD)]
shader=(ROOT/'plugin/echo.effect').read_text(encoding='utf8')
(BUILD/'echo-shader.h').write_text('static const char echo_shader[] =\n'+ '\n'.join(json.dumps(line+'\n') for line in shader.splitlines())+';\n')
out = ROOT/'dist/nexus-reactive-echo/bin/64bit'
out.mkdir(parents=True, exist_ok=True)
subprocess.run(common + ['-shared', str(ROOT/'src/nexus-reactive-echo.c'), str(ROOT/'src/cube.c'),str(ROOT/'src/reactivity.c'), str(BUILD/'obs.lib'), '-o', str(out/'nexus-reactive-echo.dll')], check=True)
subprocess.run(common + [str(ROOT/'tests/passthrough-test.c'), str(BUILD/'obs.lib'), '-o', str(BUILD/'passthrough-test.exe')], check=True)
if (ROOT/'tests/feedback-test.c').exists():
    subprocess.run(common + [str(ROOT/'tests/feedback-test.c'), str(BUILD/'obs.lib'), '-o', str(BUILD/'feedback-test.exe')], check=True)
subprocess.run([os.sys.executable,str(ROOT/'tests/generate-luts.py')],check=True)
subprocess.run(common + [str(ROOT/'tests/cube-test.c'),str(ROOT/'src/cube.c'),str(BUILD/'obs.lib'),'-o',str(BUILD/'cube-test.exe')],check=True)
subprocess.run(common + [str(ROOT/'tests/color-test.c'),str(BUILD/'obs.lib'),'-o',str(BUILD/'color-test.exe')],check=True)
subprocess.run(common + [str(ROOT/'tests/audio-test.c'),str(BUILD/'obs.lib'),'-o',str(BUILD/'audio-test.exe')],check=True)



subprocess.run(common + [str(ROOT/"tests/silhouette-test.c"),str(BUILD/"obs.lib"),"-o",str(BUILD/"silhouette-test.exe")],check=True)
subprocess.run(common + [str(ROOT/"tests/trail-color-test.c"),str(BUILD/"obs.lib"),"-o",str(BUILD/"trail-color-test.exe")],check=True)
subprocess.run(common + [str(ROOT/"tests/chroma-chain-test.c"),str(BUILD/"obs.lib"),"-o",str(BUILD/"chroma-chain-test.exe")],check=True)
subprocess.run([os.sys.executable,str(ROOT/'tests/generate-chroma-fixture.py')],check=True)
subprocess.run(common + [str(ROOT/"tests/gradient-test.c"),str(BUILD/"obs.lib"),"-o",str(BUILD/"gradient-test.exe")],check=True)
subprocess.run(common + [str(ROOT/"tests/preset-test.c"),str(BUILD/"obs.lib"),"-o",str(BUILD/"preset-test.exe")],check=True)
subprocess.run(common + [str(ROOT/"tests/benchmark.c"),str(BUILD/"obs.lib"),"-lpsapi","-o",str(BUILD/"benchmark.exe")],check=True)
subprocess.run(common + [str(ROOT/"tests/compare-render.c"),str(BUILD/"obs.lib"),"-o",str(BUILD/"compare-render.exe")],check=True)
subprocess.run(common + [str(ROOT/"tests/visual-study.c"),str(BUILD/"obs.lib"),"-o",str(BUILD/"visual-study.exe")],check=True)
subprocess.run(common + [str(ROOT/"tests/camera-study.c"),str(BUILD/"obs.lib"),"-o",str(BUILD/"camera-study.exe")],check=True)

subprocess.run(common + [str(ROOT/"tests/controls-test.c"),str(BUILD/"obs.lib"),"-o",str(BUILD/"controls-test.exe")],check=True)

subprocess.run(common + [str(ROOT/"tests/reference-test.c"),str(BUILD/"obs.lib"),"-o",str(BUILD/"reference-test.exe")],check=True)
print('Build OK:', out/'nexus-reactive-echo.dll')
