"""Package a locally built Mac candidate, check signatures and DMG roundtrip.

Runtime/GUI validation is recorded separately; this never publishes a release.
"""
import argparse
import hashlib
import json
import plistlib
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/macos-native-universal'

def run(*args):
    result = subprocess.run([str(x) for x in args], capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors='replace') or result.stdout.decode(errors='replace'))
    return result.stdout

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    if sys.platform != 'darwin':
        raise SystemExit('Run packaging on Mac')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', action='store_true', help='Prepare a stable local DMG; never publishes')
    args = parser.parse_args()
    source = BUILD / 'Look@LAN-ng.app'
    info = plistlib.loads((source / 'Contents/Info.plist').read_bytes())
    version = info['CFBundleGetInfoString']
    if '-rc.' not in version and not args.release:
        raise SystemExit('Stable packaging requires explicit --release')
    raw = ROOT / 'build/unpacked' / version / 'macos-native-universal'
    raw.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / 'Contents/MacOS/Look@LAN-ng', raw / 'Look@LAN-ng')
    output = ROOT / 'dist' / version / 'macos-native'
    output.mkdir(parents=True, exist_ok=True)
    bundle = output / 'Look@LAN-ng.app'
    if bundle.exists():
        shutil.rmtree(bundle)
    shutil.copytree(source, bundle)
    executable = bundle / 'Contents/MacOS/Look@LAN-ng'
    run('xcrun', 'strip', '-x', executable)
    run('codesign', '--force', '--sign', '-', '--options', 'runtime', bundle)
    run('codesign', '--verify', '--deep', '--strict', '--verbose=2', bundle)
    if set(run('lipo', '-archs', executable).decode().split()) != {'arm64', 'x86_64'}:
        raise RuntimeError('Universal architecture mismatch')
    asset = ROOT / 'dist' / f'look@lan-ng-macos-universal-{version}.dmg'
    with tempfile.TemporaryDirectory(prefix='looklan-mac-package-', dir=ROOT / 'build') as temp:
        stage = Path(temp) / 'payload'
        stage.mkdir()
        shutil.copytree(bundle, stage / bundle.name)
        (stage / 'Applications').symlink_to('/Applications')
        (stage / 'README.txt').write_text(
            f'Look@LAN-ng {version}\n'
            'Собрано на Mac. Перетащите приложение в Applications.\n'
            'Built on Mac. Drag the app into Applications.\n'
            'Ad-hoc signed; not notarized. Native validation/limitations: docs/MACOS_VALIDATION.md.\n',
            encoding='utf-8')
        image = Path(temp) / 'candidate.dmg'
        run('hdiutil', 'create', '-srcfolder', stage, '-volname', 'Look@LAN-ng',
            '-fs', 'HFS+', '-format', 'ULMO', image)
        run('hdiutil', 'verify', image)
        mount = Path(temp) / 'mount'
        mount.mkdir()
        attached = False
        try:
            run('hdiutil', 'attach', '-readonly', '-nobrowse', '-mountpoint', mount, image)
            attached = True
            mounted = mount / bundle.name
            for file in bundle.rglob('*'):
                if file.is_file():
                    target = mounted / file.relative_to(bundle)
                    if digest(file) != digest(target) or (file.stat().st_mode & 0o777) != (target.stat().st_mode & 0o777):
                        raise RuntimeError(f'DMG roundtrip mismatch: {file.relative_to(bundle)}')
            run('codesign', '--verify', '--deep', '--strict', mounted)
        finally:
            if attached:
                run('hdiutil', 'detach', mount)
        shutil.copy2(image, asset)
    metadata = {
        'version': version, 'build_host': 'macOS', 'architectures': ['arm64', 'x86_64'],
        'file': asset.name, 'sha256': digest(asset), 'bytes': asset.stat().st_size,
        'binary_sha256': digest(executable), 'signature': 'ad-hoc hardened runtime, verified',
        'notarization': 'not_done', 'dmg': 'ULMO/LZMA; integrity, mount, hashes/modes passed',
        'upx': 'not applied; macOS support disabled upstream, no force',
        'runtime_validation': 'see build/validation/macos-native and docs/MACOS_VALIDATION.md',
        'published': False,
    }
    (ROOT / 'dist/macos-native-package.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(asset)
    print(json.dumps(metadata, indent=2))

if __name__ == '__main__':
    main()
