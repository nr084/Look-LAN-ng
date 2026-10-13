"""Package locally cross-built ports; verify contents, signatures and executable modes."""
import argparse,re,hashlib,json,os,plistlib,shutil,struct,subprocess,tempfile,urllib.request,zipfile,sys
sys.dont_write_bytecode=True
from pathlib import Path
from verify_macos_port import verify_bundle as verify_code_seals,verify_binary
ROOT=Path(__file__).resolve().parents[1]
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def call(args):
    result=subprocess.run([str(x) for x in args],capture_output=True,text=True,encoding='utf-8',errors='replace')
    if result.returncode:raise RuntimeError(result.stdout[-3000:]+result.stderr[-3000:])
    return result.stdout
def verify_bundle(codesign,bundle):
    # rcodesign verify mis-parses the empty CMS blob in ad-hoc signatures.
    # Verify all Universal code pages and special/resource seals independently.
    del codesign
    verify_code_seals(bundle)
def upx_tool():
    request=urllib.request.Request('https://api.github.com/repos/upx/upx/releases/latest',headers={'User-Agent':'LookLAN-ng-port-builder'})
    release=json.load(urllib.request.urlopen(request,timeout=30))
    assert not release['draft'] and not release['prerelease']
    version=release['tag_name'].removeprefix('v');asset=next(x for x in release['assets'] if x['name']==f'upx-{version}-win64.zip')
    directory=ROOT/'build/cross-tools/upx'/version;directory.mkdir(parents=True,exist_ok=True)
    tool=directory/f'upx-{version}-win64/upx.exe'
    stamp=directory/'verified-tool.json'
    if not tool.exists() or not stamp.exists():
        with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='upx-download-') as temp:
            archive=Path(temp)/'upx.zip';urllib.request.urlretrieve(asset['browser_download_url'],archive)
            assert asset['digest']=='sha256:'+digest(archive),'Official UPX archive digest mismatch'
            with zipfile.ZipFile(archive) as z:z.extractall(directory)
            stamp.write_text(json.dumps({'archive_digest':asset['digest'],'exe_sha256':digest(tool)})+'\n')
    verified=json.loads(stamp.read_text());assert verified['archive_digest']==asset['digest'] and verified['exe_sha256']==digest(tool),'Cached UPX digest mismatch'
    return tool,version
def trial_pack(upx,source,target,verify=True):
    result=subprocess.run([str(upx),'--ultra-brute','--lzma','-o',str(target),str(source)],capture_output=True,text=True,encoding='utf-8',errors='replace')
    if result.returncode:
        message=(result.stdout+result.stderr)[-1200:]
        if 'CantPackException:' in message:message=message.split('CantPackException:',1)[1].strip()
        return {'applied':False,'reason':message}
    if verify:call([upx,'-t',target])
    return {'applied':True,'integrity':'passed' if verify else 'not_run','options':'--ultra-brute --lzma'}
def write_zip(output,files):
    with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name,source,mode in files:
            entry=zipfile.ZipInfo(name);entry.create_system=3;entry.external_attr=mode<<16;entry.compress_type=zipfile.ZIP_DEFLATED
            z.writestr(entry,source.read_bytes(),compresslevel=9)
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--development',action='store_true',help='Local candidate only; skip checks at explicit user request');parser.add_argument('--target',choices=('all','macos','linux'),default='all');parser.add_argument('--tools-root',type=Path,default=ROOT.parent/'KiTTY-NG/_tools');args=parser.parse_args()
    tools=args.tools_root.resolve();upx,upx_version=upx_tool()
    summary_path=ROOT/'dist/ports-package.json'
    summary=json.loads(summary_path.read_text(encoding='utf-8')) if summary_path.exists() else {}
    summary.update(build_host='Windows',upx=upx_version)
    readme=ROOT/('docs/DEVELOPMENT_README.txt' if args.development else 'docs/PORT_TESTING.txt')
    if args.target in ('all','macos'):
        llvm=sorted((tools/'macos/llvm').glob('*/bin/llvm-lipo.exe'),key=lambda p:tuple(map(int,p.parents[1].name.split('.'))))[-1].parent
        dmg=sorted((tools/'macos/dmg').glob('v*/apfs.exe'),key=lambda p:tuple(map(int,p.parent.name[1:].split('.'))))[-1]
        codesign=next((tools/'macos/rcodesign').rglob('rcodesign.exe'))
        source_bundle=ROOT/'build/macos-arm64/Look@LAN-ng.app';plist=plistlib.loads((source_bundle/'Contents/Info.plist').read_bytes());version=plist['CFBundleGetInfoString']
        output=ROOT/'dist'/version;output.mkdir(parents=True,exist_ok=True)
        summary['version']=version
        summary['runtime_validation']='Linux status is recorded per platform; macOS '+version+' native execution remains pending'
    if args.target in ('all','linux'):
        linux_version=re.search(r'^#define APP_VERSION \"([^\"]+)\"', (ROOT/'build/linux-x64/generated/desktop/build_info.h').read_text(),re.M).group(1)
        proof_path=ROOT/'dist/linux-release-validation.json'
        if proof_path.exists():
            proof=json.loads(proof_path.read_text(encoding='utf-8'))
            if proof.get('version')==linux_version and proof.get('github_release'):
                raise RuntimeError('Published Linux version is immutable; bump its version or package --target macos')
    with tempfile.TemporaryDirectory(prefix='port-package-',dir=ROOT/'build') as folder:
        work=Path(folder)
        if args.target in ('all','macos'):
            payload=work/'payload';payload.mkdir();bundle=payload/'Look@LAN-ng.app';shutil.copytree(source_bundle,bundle)
            packing={};inputs=[]
            for arch in ('x86_64','arm64'):
                thin=ROOT/f'build/macos-{arch}/Look@LAN-ng.app/Contents/MacOS/Look@LAN-ng';raw_dir=ROOT/'build/unpacked/macos'/arch;raw_dir.mkdir(parents=True,exist_ok=True);shutil.copyfile(thin,raw_dir/'Look@LAN-ng')
                trial=work/('packed-'+arch);packing[arch]=trial_pack(upx,thin,trial,verify=not args.development);inputs.append(trial if packing[arch]['applied'] else thin)
            binary=bundle/'Contents/MacOS/Look@LAN-ng';call([llvm/'llvm-lipo.exe','-create',*inputs,'-output',binary])
            if not args.development:call([llvm/'llvm-lipo.exe',binary,'-verify_arch','x86_64','arm64'])
            resources=bundle/'Contents/Resources';resources.mkdir(exist_ok=True)
            png=(ROOT/'src/resources/assets/app.png').read_bytes();chunk=b'ic08'+struct.pack('>I',8+len(png))+png;(resources/'LookLAN.icns').write_bytes(b'icns'+struct.pack('>I',8+len(chunk))+chunk)
            for lang,message in [('en','Discover devices on the network interface you select.'),('ru','Поиск устройств в сети на выбранном вами сетевом интерфейсе.')]:
                localization=resources/(lang+'.lproj');localization.mkdir();(localization/'InfoPlist.strings').write_text('"NSLocalNetworkUsageDescription" = "'+message+'";\n',encoding='utf-8')
            call([codesign,'sign','--code-signature-flags','runtime',bundle])
            if not args.development:verify_bundle(codesign,bundle)
            shutil.copyfile(readme,payload/'README.txt')
            smoke=payload/'Smoke-test.command';smoke.write_text('#!/bin/sh\nlooklan_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"\n"$looklan_dir/Look@LAN-ng.app/Contents/MacOS/Look@LAN-ng" --smoke-test\nlooklan_result=$?\nprintf "Smoke result: %s (0=passed, 77=no active IPv4)\\n" "$looklan_result"\nexit "$looklan_result"\n',encoding='utf-8',newline='\n')
            native_binaries=[];native_script=None
            if '-rc.' in version and not args.development:
                validation=payload/'Native-validation';validation.mkdir()
                for name in ('core_tests','feature_tests','monitor_tests','profile_report_tests','details_nbns_tests','posix_runtime_tests'):
                    native=validation/name
                    call([llvm/'llvm-lipo.exe','-create',ROOT/'build/macos-x86_64'/name,ROOT/'build/macos-arm64'/name,'-output',native])
                    call([codesign,'sign',native]);verify_binary(native);native_binaries.append(native)
                native_script=payload/'Native-tests.command';shutil.copyfile(ROOT/'tools/macos-native-tests.command',native_script)
            carrier=work/'metadata';carrier.mkdir();records=[]
            for path in [payload]+sorted(payload.rglob('*')):
                relative='.' if path==payload else path.relative_to(payload).as_posix();directory=path.is_dir();mode=0o40755 if directory else 0o100755 if path in (binary,smoke,native_script,*native_binaries) else 0o100644
                record={'original':relative,'materialized':relative,'kind':'directory' if directory else 'file','materializedKind':'directory' if directory else 'file','darwin':{'mode':mode,'uid':0,'gid':0}}
                if not directory:record['payload']={'sha256':digest(path),'size':path.stat().st_size}
                records.append(record)
            (carrier/'manifest.json').write_text(json.dumps({'version':1,'generation':1,'records':records})+'\n')
            destination=ROOT/'dist'/f'look@lan-ng-macos-universal-{version}.dmg'
            call([dmg,'pack',payload,destination,'--fs','hfs+','--compression','lzma','--volname','Look@LAN-ng '+version,'--metadata-root',carrier,'--strict','--output','json'])
            if not args.development:
                extracted=work/'extracted';verified=work/'verified';call([dmg,'extract',destination,'-C',extracted,'--preserve-meta','--metadata-root',verified,'--verify','--output','json'])
                lookup={r['original']:r for r in json.loads((verified/'manifest.json').read_text())['records']}
                for record in records:
                    if record['kind']=='file':assert digest(extracted/record['original'])==record['payload']['sha256']
                    if record['darwin']['mode']&0o111:assert lookup[record['original']]['darwin']['mode']&0o111
                for native in native_binaries:verify_binary(extracted/native.relative_to(payload))
                final=extracted/'Look@LAN-ng.app';verify_bundle(codesign,final);call([llvm/'llvm-lipo.exe',final/'Contents/MacOS/Look@LAN-ng','-verify_arch','x86_64','arm64']);assert destination.read_bytes()[-512:-508]==b'koly'
            local_bundle=output/'Look@LAN-ng.app';shutil.copytree(bundle,local_bundle,dirs_exist_ok=True)
            summary['macos']={'version':version,'file':destination.name,'bytes':destination.stat().st_size,'sha256':digest(destination),'architectures':['x86_64','arm64'],'filesystem':'HFS+ / LZMA (smallest supported codec)','upx':packing,'signature':'ad-hoc hardened runtime applied; verification not run' if args.development else 'ad-hoc hardened runtime; code page / Info.plist / resource hashes verified before and after DMG extraction','notarized':False,'mount_test':'not_run','roundtrip':'not_run' if args.development else 'all payload hashes and execute bits passed'}
        if args.target in ('all','linux'):
            linux_raw=ROOT/'build/linux-x64/Look@LAN-ng';raw_dir=ROOT/'build/unpacked/linux-x64';raw_dir.mkdir(parents=True,exist_ok=True);shutil.copyfile(linux_raw,raw_dir/'Look@LAN-ng')
            linux_exe=work/'Look@LAN-ng';linux_packing=trial_pack(upx,linux_raw,linux_exe,verify=not args.development)
            if not linux_packing['applied']:shutil.copyfile(linux_raw,linux_exe)
            launch=work/'LookLAN.sh';launch.write_text('#!/bin/sh\nlooklan_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"\nexec "$looklan_dir/Look@LAN-ng" "$@"\n',encoding='utf-8',newline='\n')
            linux_version=re.search(r'^#define APP_VERSION \"([^\"]+)\"', (ROOT/'build/linux-x64/generated/desktop/build_info.h').read_text(),re.M).group(1)
            linux_readme=ROOT/'docs/LINUX_README.txt' if '-' not in linux_version else readme
            zip_path=ROOT/'dist'/f'look@lan-ng-linux-x64-{linux_version}.zip';write_zip(zip_path,[('Look@LAN-ng',linux_exe,0o100755),('LookLAN.sh',launch,0o100755),('LookLAN.png',ROOT/'src/resources/assets/app.png',0o100644),('README.txt',linux_readme,0o100644)])
            if not args.development:
                with zipfile.ZipFile(zip_path) as z:
                    assert hashlib.sha256(z.read('Look@LAN-ng')).hexdigest()==digest(linux_exe);assert z.getinfo('Look@LAN-ng').external_attr>>16&0o111
            summary['linux']={'version':linux_version,'file':zip_path.name,'bytes':zip_path.stat().st_size,'sha256':digest(zip_path),'upx':linux_packing,'baseline':'Ubuntu 22.04+ x64, GTK 3 system libraries','runtime_test':'not_run'}
    summary['development']=args.development
    (ROOT/'dist/ports-package.json').write_text(json.dumps(summary,indent=2,ensure_ascii=False)+'\n',encoding='utf-8');print(json.dumps(summary,indent=2,ensure_ascii=True))
if __name__=='__main__':main()
