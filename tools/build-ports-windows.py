"""Cross-build macOS universal and Ubuntu x64 locally on Windows. No remote compilation."""
import argparse, hashlib, json, os, shlex, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(command, log):
    result=subprocess.run([str(x) for x in command],cwd=ROOT,capture_output=True,env=ENV)
    log.parent.mkdir(parents=True,exist_ok=True);log.write_bytes(result.stdout+result.stderr)
    if result.returncode:
        print((result.stdout+result.stderr).decode('utf-8',errors='replace')[-10000:]);raise RuntimeError('Cross-build failed: '+str(log))
    print((result.stdout+result.stderr).decode('utf-8',errors='replace')[-1500:])
def quote(path):return '"'+str(path).replace('\\','/')+'"'
def main():
    global ENV
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tools-root',type=Path,default=ROOT.parent/'KiTTY-NG/_tools')
    parser.add_argument('--target',choices=['all','macos','linux'],default='linux');args=parser.parse_args()
    if os.name!='nt':parser.error('This script executes compiler tools locally on Windows')
    if args.target != 'linux':
        parser.error('macOS builds/packaging moved to Mac; use tools/build-macos.sh')
    tools=args.tools_root.resolve();store=tools/'macos'
    llvm=sorted((store/'llvm').glob('*/bin/clang.exe'),key=lambda p:tuple(map(int,p.parents[1].name.split('.'))))[-1].parent
    cmake=next((store/'cmake').rglob('cmake.exe'));ninja=next((store/'ninja').rglob('ninja.exe'))
    ENV=dict(os.environ);ENV['PATH']=str(llvm)+os.pathsep+ENV['PATH'];ENV['LC_ALL']='C'
    logs=ROOT/'build/port-logs';base=[cmake,'-S',ROOT,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_MAKE_PROGRAM='+ninja.as_posix(),'-DDESKTOP_HOST_PYTHON='+Path(sys.executable).as_posix()]
    if args.target in ('all','macos'):
        sdk=sorted((store/'sdk').glob('MacOSX*.sdk'))[-1]
        for arch in ('x86_64','arm64'):
            build=ROOT/'build'/('macos-'+arch)
            command=base+['-B',build,'-DCMAKE_SYSTEM_NAME=Darwin','-DCMAKE_SYSTEM_PROCESSOR='+arch,'-DCMAKE_OSX_ARCHITECTURES='+arch,'-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0','-DCMAKE_OSX_SYSROOT='+sdk.as_posix(),'-DCMAKE_C_COMPILER='+(llvm/'clang.exe').as_posix(),'-DCMAKE_C_COMPILER_TARGET='+arch+'-apple-macos13.0','-DCMAKE_OBJC_COMPILER='+(llvm/'clang.exe').as_posix(),'-DCMAKE_OBJC_COMPILER_TARGET='+arch+'-apple-macos13.0','-DCMAKE_AR='+(llvm/'llvm-ar.exe').as_posix(),'-DCMAKE_RANLIB='+(llvm/'llvm-ranlib.exe').as_posix(),'-DCMAKE_INSTALL_NAME_TOOL='+(llvm/'llvm-install-name-tool.exe').as_posix(),'-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld']
            run(command,logs/(arch+'-configure.log'));run([cmake,'--build',build,'--parallel','8'],logs/(arch+'-build.log'))
    if args.target in ('all','linux'):
        sysroot=tools/'linux/sysroot';metadata=json.loads((tools/'linux/target.json').read_text())
        build=ROOT/'build/linux-x64';build.mkdir(parents=True,exist_ok=True)
        # Use our own ELF driver copy: never mutate the shared tool store.
        import shutil
        driver=build/'linker';driver.mkdir(exist_ok=True);shutil.copyfile(llvm/'lld.exe',driver/'ld.lld.exe');ENV['PATH']=str(driver)+os.pathsep+ENV['PATH']
        toolchain=build/'target.cmake'
        toolchain.write_text('\n'.join(['set(CMAKE_SYSTEM_NAME Linux)','set(CMAKE_SYSTEM_PROCESSOR x86_64)','set(CMAKE_C_COMPILER '+quote(llvm/'clang.exe')+')','set(CMAKE_C_COMPILER_TARGET x86_64-linux-gnu)','set(CMAKE_SYSROOT '+quote(sysroot)+')','set(CMAKE_C_FLAGS_INIT [=[--gcc-toolchain='+quote(sysroot/'usr')+']=])','set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES '+quote(sysroot/'usr/include/x86_64-linux-gnu')+')','set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")','set(CMAKE_AR '+quote(llvm/'llvm-ar.exe')+')','set(CMAKE_RANLIB '+quote(llvm/'llvm-ranlib.exe')+')','set(CMAKE_FIND_ROOT_PATH '+quote(sysroot)+')','set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)','set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)','set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)','set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)'])+'\n',encoding='utf-8')
        gtk=build/'gtk.cmake';includes=[quote(sysroot/flag[2:].lstrip('/')) for flag in shlex.split(metadata['gtk_cflags']) if flag.startswith('-I')];libs=[flag[2:] for flag in shlex.split(metadata['gtk_libs']) if flag.startswith('-l')]
        gtk.write_text('set(GTK3_INCLUDE_DIRS '+' '.join(includes)+')\nset(GTK3_LIBRARIES '+' '.join(libs)+')\nset(GTK3_LIBRARY_DIRS '+quote(sysroot/'usr/lib/x86_64-linux-gnu')+')\n',encoding='utf-8')
        run(base+['-B',build,'-DCMAKE_TOOLCHAIN_FILE='+toolchain.as_posix(),'-DLOOKLAN_TARGET_GTK='+gtk.as_posix()],logs/'linux-configure.log');run([cmake,'--build',build,'--parallel','8'],logs/'linux-build.log')
    sources=hashlib.sha256()
    sources.update((ROOT/'CMakeLists.txt').read_bytes());sources.update((ROOT/'data/oui.bin').read_bytes())
    for folder in (ROOT/'src',ROOT/'tools'):
        for path in sorted(folder.rglob('*')):
            if path.is_file() and path.suffix in ('.c','.h','.m','.py','.in'):sources.update(path.relative_to(ROOT).as_posix().encode()+b'\0'+path.read_bytes())
    result={'build_host':'Windows','source_sha256':sources.hexdigest(),'targets':args.target,'llvm':llvm.parent.name,'native_runtime_tests':'not_run','linux_baseline':'Ubuntu 22.04 x64 / GTK 3','macos_minimum':'13.0'}
    (ROOT/'dist').mkdir(exist_ok=True);(ROOT/'dist/ports-cross-build.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS: local Windows cross-compilation; native runtime validation remains pending.')
if __name__=='__main__':main()
