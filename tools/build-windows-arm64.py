"""Local Windows ARM64 fallback when MSVC ARM64 target tools are absent."""
import argparse,json,os,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(args,env):
    result=subprocess.run([str(x) for x in args],env=env)
    if result.returncode:raise RuntimeError('ARM64 build failed')
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tools-root',type=Path,default=ROOT.parent/'KiTTY-NG/_tools')
    args=parser.parse_args();tools=args.tools_root.resolve()
    compiler=tools/'llvm-mingw/20261006-local/toolchain/bin/aarch64-w64-mingw32-clang.exe'
    cmake=next((tools/'macos/cmake').rglob('cmake.exe'));ninja=next((tools/'macos/ninja').rglob('ninja.exe'))
    build=ROOT/'build/windows-arm64-llvm';build.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env['PATH']
    target=build/'target.cmake'
    def quote(path):return '"'+str(path).replace('\\','/')+'"'
    target.write_text('\n'.join(['set(CMAKE_SYSTEM_NAME Windows)','set(CMAKE_SYSTEM_PROCESSOR ARM64)','set(CMAKE_C_COMPILER '+quote(compiler)+')','set(CMAKE_RC_COMPILER '+quote(compiler.parent/'aarch64-w64-mingw32-windres.exe')+')','set(CMAKE_C_FLAGS_INIT "-D_CRT_SECURE_NO_WARNINGS -DLOOKLAN_MINGW_DNS_COMPAT -O2")'])+'\n',encoding='utf-8')
    run([cmake,'-S',ROOT,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_MAKE_PROGRAM='+ninja.as_posix(),'-DCMAKE_TOOLCHAIN_FILE='+target.as_posix(),'-DCMAKE_C_FLAGS=-D_CRT_SECURE_NO_WARNINGS -DLOOKLAN_MINGW_DNS_COMPAT -O2'],env)
    run([cmake,'--build',build,'--target','LookAtLANng','--parallel','8'],env)
    output=ROOT/'build/windows-arm64/Release';output.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(build/'Look@LAN-ng.exe',output/'Look@LAN-ng.exe')
    (output/'cross-build.json').write_text(json.dumps({'build_host':'Windows','architecture':'arm64','compiler':'LLVM-MinGW (static support libraries, OS UCRT)','tests':'not_run'},indent=2)+'\n')
    print('Built local ARM64 candidate; no runtime tests executed.')
if __name__=='__main__':main()
