"""Exercise the actual EXE only in newly created project-owned portable fixtures."""
from pathlib import Path
import hashlib, json, shutil, subprocess, tempfile, zipfile
import sys, os
sys.path.insert(0,str(Path(__file__).resolve().parent.parent))
import package

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT/f'dist/NEXUS-Reactive-Echo-{package.VERSION}-Windows-x64-Setup.exe'
OUT = ROOT/'tests/evidence/installer'
OUT.mkdir(exist_ok=True, parents=True)
checks = []

def check(ok, label):
    checks.append({'check':label,'pass':bool(ok)})
    print(label, 'PASS' if ok else 'FAIL')
    if not ok:
        raise AssertionError(label)

def call(action, target, success=True, exe=EXE):
    result = subprocess.run([str(exe),action,'--portable',str(target)],capture_output=True)
    check((result.returncode == 0) == success,action+' '+('accepted' if success else 'rejected'))

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    # tempfile creates a fresh unique root; no existing workspace tree is removed.
    temp = Path(tempfile.mkdtemp(prefix='installer-ñ-',dir=ROOT/'build'))
    target = temp/'OBS portátil áé'
    exe = target/'bin/64bit/obs64.exe'
    exe.parent.mkdir(parents=True)
    shutil.copy2(str(Path(os.environ.get('OBS_INSTALL_DIR',str(Path(os.environ['ProgramFiles'])/'obs-studio')))/'bin/64bit/obs64.exe'),exe)
    (target/'portable_mode.txt').write_text('')
    sentinel = target/'config/obs-studio/basic/scenes/Preservar.json'
    sentinel.parent.mkdir(parents=True)
    sentinel.write_text('{invalid fixture - installer must not parse or change scenes}')
    original = sentinel.read_bytes()
    dll = target/'obs-plugins/64bit/nexus-reactive-echo.dll'
    metadata = target/'data/obs-plugins/nexus-reactive-echo/setup'
    manifest = metadata/'installed.tsv'
    readme = target/'data/obs-plugins/nexus-reactive-echo/README.txt'
    call('--inspect',target)
    check(not dll.exists() and not metadata.exists(),'inspect performs no deployment')
    # Bad embedded DLL with the original expected digest must fail before writes.
    corrupt_zip = temp/'corrupt.zip'
    with zipfile.ZipFile(ROOT/'build/installer-payload.zip') as source, zipfile.ZipFile(corrupt_zip,'w') as dest:
        for entry in source.namelist():
            data = source.read(entry)
            if entry.endswith('.dll'):
                data = b'corrupted payload'
            dest.writestr(entry,data)
    corrupt_exe = temp/'corrupt-setup.exe'
    package.compile_setup(corrupt_exe,[
        '/resource:'+str(corrupt_zip)+',payload.zip',
        '/resource:'+str(ROOT/'build/uninstall.exe')+',uninstall.exe'])
    call('--install',target,False,exe=corrupt_exe)
    check(not dll.exists() and not manifest.exists(),'corrupt embedded payload rejected before deployment')
    call('--install',target)
    check(digest(dll)==digest(ROOT/'dist/nexus-reactive-echo/bin/64bit/nexus-reactive-echo.dll'),'installed DLL matches tested DLL')
    check((metadata/'uninstall.exe').is_file(),'standalone uninstaller installed')
    for language in ['es-ES','en-US','de-DE','fr-FR','it-IT']:
        locale=target/'data/obs-plugins/nexus-reactive-echo/locale'/(language+'.ini')
        check(locale.is_file() and 'T000=' in locale.read_text(encoding='utf-8'),'installed locale '+language)
    check(sentinel.read_bytes()==original,'unrelated malformed scene untouched')
    call('--install',target)
    check(digest(dll)==digest(ROOT/'dist/nexus-reactive-echo/bin/64bit/nexus-reactive-echo.dll'),'reinstall is idempotent')
    # Force failure on the second deployed file and prove the first was restored.
    old = b'previous DLL fixture'
    dll.write_bytes(old)
    collision = readme.with_name(readme.name+'.nre-new')
    collision.write_bytes(b'preexisting unrelated temporary file')
    call('--install',target,False)
    check(dll.read_bytes()==old and collision.read_bytes()==b'preexisting unrelated temporary file','failed update rolls back DLL and preserves collision')
    collision.unlink()
    call('--install',target)
    saved_manifest = manifest.read_bytes()
    manifest.write_text('0'*64+'\t../outside.dll\n')
    before = dll.read_bytes()
    call('--install',target,False)
    check(dll.read_bytes()==before,'invalid manifest refused before writes')
    manifest.write_text('0'*64+'\tdata/obs-plugins/nexus-reactive-echo/foreign.txt\n')
    call('--uninstall',target,False)
    check(dll.read_bytes()==before,'foreign manifest entry refused')
    manifest.write_bytes(saved_manifest)
    dll.write_bytes(b'modified DLL')
    call('--uninstall',target,False)
    check(dll.read_bytes()==b'modified DLL' and manifest.exists(),'modified DLL preserved')
    call('--install',target)
    readme.write_text('my modified documentation',encoding='utf8')
    other = dll.parent/'other-plugin.dll'
    other.write_bytes(b'foreign plugin')
    helper = temp/'uninstaller-test.exe'
    shutil.copy2(metadata/'uninstall.exe',helper)
    call('--uninstall',target,exe=helper)
    check(not dll.exists() and not manifest.exists(),'separate uninstaller removes managed files')
    check(readme.read_text()=='my modified documentation' and other.read_bytes()==b'foreign plugin','modified docs and unrelated plugin preserved')
    check(sentinel.read_bytes()==original,'scene preserved after uninstall')
    call('--uninstall',target,exe=helper)
    invalid = temp/'invalid OBS'
    invalid.mkdir()
    call('--install',invalid,False)
    check(not (invalid/'obs-plugins').exists(),'invalid OBS causes no deployment')
    # A controlled obs64.exe process proves the path-specific running guard.
    guard_target = temp/'OBS running fixture'
    guard_exe = guard_target/'bin/64bit/obs64.exe'
    guard_exe.parent.mkdir(parents=True)
    (guard_target/'portable_mode.txt').write_text('')
    guard_source = temp/'guard.cs'
    guard_source.write_text('using System.Reflection; [assembly:AssemblyVersion("32.2.2.0")] '
                           '[assembly:AssemblyFileVersion("32.2.2.0")] '
                           'class Guard { static void Main(){System.Threading.Thread.Sleep(30000);} }')
    subprocess.run([str(package.CSC),'/nologo','/target:exe','/platform:x64',
                    '/out:'+str(guard_exe),str(guard_source)],check=True)
    process = subprocess.Popen([str(guard_exe)],creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        check(process.poll() is None,'controlled OBS process is running')
        call('--install',guard_target,False)
        check(not (guard_target/'obs-plugins').exists(),'running OBS prevents deployment')
    finally:
        if process.poll() is None:
            process.terminate()
        process.wait()
    call('--install',target)
    (target/'portable_mode.txt').unlink()
    call('--install',target,False)
    check(dll.exists(),'missing portable marker safely rejected')
    with zipfile.ZipFile(ROOT/f'dist/NEXUS-Reactive-Echo-{package.VERSION}-Windows-x64.zip') as z:
        check(len(z.namelist())==8 and all(not n.endswith(('.pdb','.lib')) for n in z.namelist()),'binary ZIP contains runtime allowlist only')
    with zipfile.ZipFile(ROOT/f'dist/NEXUS-Reactive-Echo-{package.VERSION}-Windows-x64-source.zip') as z:
        check(not any('/evidence/' in n or n.endswith(('.mkv','.mp4')) for n in z.namelist()),'source ZIP excludes camera media and local evidence')
    (OUT/'results.json').write_text(json.dumps({'fixture':str(temp),'checks':checks},indent=2),encoding='utf8')
    print('PASS:',len(checks),'checks; fixture retained:',temp)

if __name__=='__main__':
    main()
