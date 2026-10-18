"""Build the standalone NEXUS installer with the tested DLL and explicit files."""
from pathlib import Path
import hashlib, subprocess, zipfile, os

ROOT = Path(__file__).resolve().parent
DIST, BUILD = ROOT/'dist', ROOT/'build'
VERSION = '1.2.0'
CSC = Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
NAME = f'NEXUS-Reactive-Echo-{VERSION}-Windows-x64'

def digest(data):
    return hashlib.sha256(data).hexdigest().upper()

def compile_setup(output, extra=()):
    args = [str(CSC), '/nologo', '/target:winexe', '/platform:x64', '/optimize+',
            '/warnaserror+', '/out:'+str(output),
            '/win32icon:'+str(ROOT/'installer/nexus-icon.ico'),
            '/win32manifest:'+str(ROOT/'installer/setup.manifest')]
    args += ['/reference:'+s for s in ['System.Windows.Forms.dll','System.Drawing.dll',
                                      'System.IO.Compression.dll','System.IO.Compression.FileSystem.dll']]
    args += ['/resource:'+str(ROOT/'installer'/f)+','+f for f in ['nexus-branding.png','nexus-icon.ico']]
    args += list(extra)
    args += [str(ROOT/'installer'/f) for f in ['Setup.cs','InstallerText.cs','AssemblyInfo.cs']]
    subprocess.run(args, check=True)

def main():
    DIST.mkdir(exist_ok=True)
    BUILD.mkdir(exist_ok=True)
    dll = ROOT/'dist/nexus-reactive-echo/bin/64bit/nexus-reactive-echo.dll'
    if not dll.is_file():
        raise SystemExit('Compile primero con build.py y ejecute test.ps1.')
    files = {'bin/64bit/nexus-reactive-echo.dll':dll.read_bytes(),
             'data/README.txt':(ROOT/'installer/README.txt').read_bytes(),
             'data/COPYING.txt':(ROOT/'COPYING.txt').read_bytes()}
    for locale in sorted((ROOT/'plugin/locale').glob('*.ini')):
        files['data/locale/'+locale.name]=locale.read_bytes()
    manifest = ''.join(digest(data)+'\t'+name+'\n' for name,data in files.items())
    with zipfile.ZipFile(BUILD/'installer-payload.zip','w',zipfile.ZIP_DEFLATED) as z:
        for name,data in files.items():
            z.writestr(name,data)
        z.writestr('payload.tsv',manifest)
    compile_setup(BUILD/'uninstall.exe')
    compile_setup(DIST/(NAME+'-Setup.exe'),[
        '/resource:'+str(BUILD/'installer-payload.zip')+',payload.zip',
        '/resource:'+str(BUILD/'uninstall.exe')+',uninstall.exe'])
    with zipfile.ZipFile(DIST/(NAME+'.zip'),'w',zipfile.ZIP_DEFLATED) as z:
        for name,data in files.items():
            z.writestr('nexus-reactive-echo/'+name,data)
    with zipfile.ZipFile(DIST/(NAME+'-source.zip'),'w',zipfile.ZIP_DEFLATED) as z:
        for folder in ['src','plugin','installer','scripts']:
            for p in sorted((ROOT/folder).rglob('*')):
                if p.is_file():
                    z.write(p,'nexus-reactive-echo/'+p.relative_to(ROOT).as_posix())
        for p in sorted((ROOT/'tests').glob('*')):
            if p.suffix in {'.c','.py','.ps1','.cs'}:
                z.write(p,'nexus-reactive-echo/'+p.relative_to(ROOT).as_posix())
        for name in ['build.py','test.ps1','package.py','camera-study.ps1','benchmark.ps1',
                     'README.md','COPYING.txt','BENCHMARKS.md']:
            z.write(ROOT/name,'nexus-reactive-echo/'+name)
    outputs = [DIST/(NAME+s) for s in ['-Setup.exe','.zip','-source.zip']]
    (DIST/'SHA256SUMS.txt').write_text(''.join(digest(p.read_bytes()).lower()+'  '+p.name+'\n' for p in outputs),encoding='ascii')
    print('Packaged DLL SHA256:',digest(files['bin/64bit/nexus-reactive-echo.dll']))
    for p in outputs:
        print(p)

if __name__ == '__main__':
    main()
