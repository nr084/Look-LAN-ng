"""Prepare local architecture candidates without running validation or applications.

Explicit development mode: never publish these files as validated stable releases.
"""
import argparse,importlib.util,json,re,shutil,subprocess,sys,tempfile
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('ports_packager',ROOT/'tools/package-ports-windows.py')
packager=importlib.util.module_from_spec(spec);spec.loader.exec_module(packager)
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--ports-only',action='store_true');args=parser.parse_args()
    version=re.search(r'^#define APP_VERSION "([^"]+)"',(ROOT/'build/windows-x64/generated/build_info.h').read_text(),re.M).group(1)
    if '-rc.' not in version:raise RuntimeError('Development packaging requires an RC version')
    tool,upx_version=packager.upx_tool()
    summary={'version':version,'development':True,'build_host':'Windows','checks':'not_run at user request','native_tests':'not_run','upx':upx_version,'assets':{},'windows':{}}
    if args.ports_only:
        previous=json.loads((ROOT/'dist/development-artifacts.json').read_text())
        if previous['version']!=version:raise RuntimeError('No matching Windows candidate packages')
        summary['windows']=previous['windows']
        for item in summary['windows'].values():summary['assets'][item['file']]=previous['assets'][item['file']]
    with tempfile.TemporaryDirectory(prefix='development-package-',dir=ROOT/'build') as temp:
        for arch in () if args.ports_only else ('x86','x64','arm64'):
            source=ROOT/f'build/windows-{arch}/Release/Look@LAN-ng.exe'
            raw_dir=ROOT/'build/unpacked'/version/('windows-'+arch);raw_dir.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(source,raw_dir/'Look@LAN-ng.exe')
            packed=Path(temp)/(arch+'.exe');result=packager.trial_pack(tool,source,packed,verify=False)
            binary=packed if result['applied'] else source
            destination=ROOT/'dist'/f'look@lan-ng-{arch}-{version}.zip'
            packager.write_zip(destination,[('Look@LAN-ng.exe',binary,0o100755)])
            summary['assets'][destination.name]=packager.digest(destination)
            summary['windows'][arch]={'file':destination.name,'compiler':'LLVM-MinGW' if arch=='arm64' else 'MSVC v143','upx':result,'runtime_tests':'not_run','raw':str(raw_dir.relative_to(ROOT))}
    subprocess.run([sys.executable,str(ROOT/'tools/package-ports-windows.py'),'--development'],check=True)
    ports=json.loads((ROOT/'dist/ports-package.json').read_text())
    for platform in ('linux','macos'):
        item=ports[platform]
        if item['version']!=version:raise RuntimeError('Platform versions differ')
        summary[platform]=item;summary['assets'][item['file']]=item['sha256']
    (ROOT/'dist/development-artifacts.json').write_text(json.dumps(summary,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    print('Local candidate packages prepared. No validation, smoke tests or application launches performed.')
    for name in summary['assets']:print(ROOT/'dist'/name)
if __name__=='__main__':main()
