"""Locally build Windows or Linux using freshly verified stable tools and IEEE data."""
import argparse, os, subprocess, shutil, sys
from pathlib import Path
from build_preflight import prepare,ROOT
parser=argparse.ArgumentParser();parser.add_argument('--target',choices=['windows','linux'],default='windows');parser.add_argument('--arch',choices=['x86','x64','arm64'],default='x64');parser.add_argument('--tools-root',type=Path,default=Path(os.environ.get('LOOKLAN_TOOLS_ROOT',ROOT.parent/'KiTTY-NG/_tools')));args=parser.parse_args()
compiler=prepare(args.target) if args.target=='windows' else None
tools=args.tools_root;cmake=Path(shutil.which('cmake')) if shutil.which('cmake') else next((tools/'macos/cmake').rglob('cmake.exe'));ninja=Path(shutil.which('ninja')) if shutil.which('ninja') else next((tools/'macos/ninja').rglob('ninja.exe'))
if args.target=='linux':
    subprocess.run([sys.executable,str(ROOT/'tools/build-ports-windows.py'),'--target','linux','--tools-root',str(tools)],check=True)
else:
    prefix={'x86':'i686','x64':'x86_64','arm64':'aarch64'}[args.arch]+'-w64-mingw32'
    build=ROOT/('build/windows-'+args.arch+'-stable');build.mkdir(parents=True,exist_ok=True)
    def q(p):return '"'+p.as_posix()+'"'
    toolchain=build/'target.cmake';toolchain.write_text('\n'.join(['set(CMAKE_SYSTEM_NAME Windows)','set(CMAKE_SYSTEM_PROCESSOR '+args.arch+')','set(CMAKE_C_COMPILER '+q(compiler.parent/(prefix+'-clang.exe'))+')','set(CMAKE_RC_COMPILER '+q(compiler.parent/(prefix+'-windres.exe'))+')','set(CMAKE_C_FLAGS_INIT "-D_CRT_SECURE_NO_WARNINGS -DLOOKLAN_MINGW_DNS_COMPAT -O2")'])+'\n',encoding='utf-8')
    env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env['PATH']
    subprocess.run([str(cmake),'-S',str(ROOT),'-B',str(build),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_MAKE_PROGRAM='+ninja.as_posix(),'-DCMAKE_TOOLCHAIN_FILE='+toolchain.as_posix()],env=env,check=True)
    subprocess.run([str(cmake),'--build',str(build),'--parallel','8'],env=env,check=True)
    output=ROOT/('build/windows-'+args.arch+'/Release');output.mkdir(parents=True,exist_ok=True);shutil.copyfile(build/'Look@LAN-ng.exe',output/'Look@LAN-ng.exe');shutil.copyfile(ROOT/'build/validation/build-preflight-windows.json',output/'preflight.json')
    if args.arch!='arm64':subprocess.run([str(cmake.parent/'ctest.exe'),'--test-dir',str(build),'--output-on-failure'],env=env,check=True)
    else:print('ARM64 compiled locally; native runtime validation required before publication.')
