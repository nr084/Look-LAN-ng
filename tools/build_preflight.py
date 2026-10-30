"""Fail-closed official stable toolchain and IEEE MA-L refresh for every build."""
import csv, datetime, hashlib, io, json, os, re, subprocess, tarfile, tempfile, urllib.request, zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
TOOLS=ROOT/'build/toolchains'
def fetch(url):
    if os.name=='nt':
        (ROOT/'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='build-fetch-',dir=ROOT/'build') as folder:
            path=Path(folder)/'download';download(url,path);return path.read_bytes()
    request=urllib.request.Request(url,headers={'User-Agent':'LookLAN-ng-build-preflight','Cache-Control':'no-cache'})
    with urllib.request.urlopen(request,timeout=120) as response:return response.read()
def download(url,path):
    if os.name=='nt':
        # Windows curl uses the OS Schannel certificate store, including local
        # trust roots. Keep full certificate verification and bound retries.
        curl=Path(os.environ.get('SystemRoot','C:/Windows'))/'System32/curl.exe'
        subprocess.run([str(curl),'--fail','--location','--silent','--show-error','--connect-timeout','20','--max-time','90','--retry','2','--retry-delay','1','--user-agent','LookLAN-build-preflight','--header','Cache-Control: no-cache','--output',str(path),url],check=True,timeout=300)
    else:
        import shutil
        with urllib.request.urlopen(url,timeout=180) as response,path.open('wb') as out:shutil.copyfileobj(response,out)
def latest(repo):
    r=json.loads(fetch('https://api.github.com/repos/'+repo+'/releases/latest'))
    if r['prerelease'] or r['draft']:raise RuntimeError('Stable compiler release required')
    return r
def install(release,pattern,folder):
    asset=next(a for a in release['assets'] if re.fullmatch(pattern,a['name']))
    target=TOOLS/folder/release['tag_name'];stamp=target/'verified.json'
    if stamp.exists():
        receipt=json.loads(stamp.read_text())
        if receipt['digest']!=asset['digest']:raise RuntimeError('Cached toolchain provenance mismatch')
        compiler=target/receipt['compiler']
        if hashlib.sha256(compiler.read_bytes()).hexdigest()!=receipt['compiler_sha256']:raise RuntimeError('Cached compiler modified')
        return compiler
    target.mkdir(parents=True,exist_ok=True)
    archive=target/asset['name'];print('Downloading stable',asset['name'],flush=True)
    download(asset['browser_download_url'],archive)
    if 'sha256:'+hashlib.sha256(archive.read_bytes()).hexdigest()!=asset['digest']:raise RuntimeError('Official archive SHA-256 mismatch')
    if archive.suffix=='.zip':
        with zipfile.ZipFile(archive) as z:z.extractall(target)
    else:subprocess.run(['tar','-xf',str(archive),'-C',str(target)],check=True)
    compiler=next(p for p in target.rglob('clang.exe') if p.parent.name=='bin')
    stamp.write_text(json.dumps({'digest':asset['digest'],'compiler':compiler.relative_to(target).as_posix(),'compiler_sha256':hashlib.sha256(compiler.read_bytes()).hexdigest()},indent=2)+'\n')
    archive.unlink();return compiler
def refresh_vendor():
    data=fetch('https://standards-oui.ieee.org/oui/oui.csv')
    vendors={int(r['Assignment'],16):r['Organization Name'].strip() for r in csv.DictReader(io.StringIO(data.decode('utf-8-sig'))) if r['Registry']=='MA-L' and re.fullmatch('[0-9A-Fa-f]{6}',r['Assignment'])}
    if len(vendors)<30000:raise RuntimeError('Incomplete official IEEE database')
    import struct
    strings=bytearray();records=bytearray();offsets={}
    for prefix,name in sorted(vendors.items()):
        if not name or '\0' in name or len(name.encode())>1024:raise RuntimeError('Invalid IEEE name')
        if name not in offsets:offsets[name]=len(strings);strings+=name.encode()+b'\0'
        records+=struct.pack('<II',prefix,offsets[name])
    binary=b'OUI1'+struct.pack('<II',len(vendors),len(strings))+records+strings
    output=ROOT/'data/oui.bin';temporary=output.with_suffix('.tmp');temporary.write_bytes(binary);os.replace(temporary,output)
    metadata={'source':'https://standards-oui.ieee.org/oui/oui.csv','retrieved_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_sha256':hashlib.sha256(data).hexdigest(),'binary_sha256':hashlib.sha256(binary).hexdigest(),'prefixes':len(vendors),'binary_bytes':len(binary)}
    (ROOT/'data/oui-source.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
    return metadata
def prepare(target):
    if os.name!='nt':raise RuntimeError('Windows build entry point only; Mac builds run separately on Mac')
    print('Checking official latest stable LLVM/LLVM-MinGW and IEEE database...',flush=True)
    r=latest('llvm/llvm-project');llvm_version=r['tag_name'].removeprefix('llvmorg-')
    if not re.fullmatch(r'\d+\.\d+\.\d+',llvm_version):raise RuntimeError('Non-stable LLVM version rejected')
    mingw=latest('mstorsjo/llvm-mingw')
    compiler=install(mingw,r'llvm-mingw-.*-ucrt-x86_64.zip','llvm-mingw')
    version=subprocess.check_output([str(compiler),'--version'],text=True,encoding='utf-8',errors='replace')
    if not re.search(r'clang version '+re.escape(llvm_version)+r'\b',version):raise RuntimeError('Latest stable LLVM required: '+llvm_version+'; installed '+version)
    vendor=refresh_vendor();report={'checked_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'target':target,'llvm_stable':llvm_version,'compiler':str(compiler),'compiler_version':version,'vendor':vendor}
    path=ROOT/'build/validation'/('build-preflight-'+target+'.json');path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: latest stable compiler '+llvm_version+'; fresh IEEE database',flush=True)
    return compiler
def prepare_mac():
    import sys,shutil
    if sys.platform!='darwin':raise RuntimeError('Native macOS preflight runs only on Mac')
    release=latest('llvm/llvm-project');version=release['tag_name'].removeprefix('llvmorg-')
    if not re.fullmatch(r'\d+\.\d+\.\d+',version):raise RuntimeError('Stable LLVM required')
    if not shutil.which('brew'):raise RuntimeError('Homebrew LLVM is required for latest-stable native builds; install Homebrew on the Mac build host')
    subprocess.run(['brew','update'],check=True)
    formula=json.loads(subprocess.check_output(['brew','info','--json=v2','llvm'],text=True))['formulae'][0]
    if formula['versions']['stable']!=version:raise RuntimeError('Homebrew has not published the latest official stable LLVM yet; build stopped')
    if not formula['installed']:subprocess.run(['brew','install','llvm'],check=True)
    elif all(item['version']!=version for item in formula['installed']):subprocess.run(['brew','upgrade','llvm'],check=True)
    compiler=Path(subprocess.check_output(['brew','--prefix','llvm'],text=True).strip())/'bin/clang'
    actual=subprocess.check_output([str(compiler),'--version'],text=True)
    if not re.search(r'clang version '+re.escape(version)+r'\b',actual):raise RuntimeError('Native compiler does not match latest LLVM stable')
    vendor=refresh_vendor();report={'target':'macos','compiler':str(compiler),'llvm_stable':version,'compiler_version':actual,'vendor':vendor}
    path=ROOT/'build/validation/build-preflight-macos.json';path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: native latest stable compiler and fresh IEEE database');return compiler
if __name__=='__main__':
    import sys
    print(prepare_mac() if sys.argv[1]=='macos' else prepare(sys.argv[1]))
