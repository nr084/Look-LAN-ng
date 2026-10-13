"""Publish a verified, locally Mac-built release using the authenticated GitHub CLI."""
import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = 'nr084/Look-LAN-ng'

def run(*args):
    result = subprocess.run([str(x) for x in args], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stderr or result.stdout)
    return result.stdout.strip()

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', required=True)
    parser.add_argument('--source-repo', type=Path, required=True)
    parser.add_argument('--notes', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'darwin' or not re.fullmatch(r'\d+\.\d+\.\d+', args.version):
        raise SystemExit('Only stable macOS releases may be published here')
    proof = json.loads((ROOT / 'dist/macos-release-validation.json').read_text())
    if proof['version'] != args.version or proof['native_tests'] != 'passed' or proof['feature_complete'] is not True:
        raise SystemExit('Matching native validation is required')
    assets = [ROOT / 'dist' / name for name in proof['assets']]
    for file in assets:
        if not file.name.startswith('look@lan-ng-macos-') or digest(file) != proof['assets'][file.name]:
            raise SystemExit('Asset/proof mismatch')
    if run('git', '-C', args.source_repo, 'status', '--porcelain'):
        raise SystemExit('Commit reviewed release sources first')
    commit = run('git', '-C', args.source_repo, 'rev-parse', 'HEAD')
    remote = run('git', '-C', args.source_repo, 'remote', 'get-url', 'origin')
    if remote.rstrip('/').removesuffix('.git') != 'https://github.com/' + REPOSITORY:
        raise SystemExit('Unexpected source repository')
    tag = 'v' + args.version
    listing = json.loads(run('gh', 'api', f'repos/{REPOSITORY}/releases?per_page=100'))
    existing = next((r for r in listing if r['tag_name'] == tag), None)
    if existing and not existing['draft']:
        raise SystemExit('Published releases are immutable')
    if not existing:
        run('gh', 'release', 'create', tag, '--repo', REPOSITORY, '--target', commit,
            '--draft', '--title', 'Look@LAN-ng ' + args.version + ' — macOS', '--notes-file', args.notes)
    release = json.loads(run('gh', 'api', f'repos/{REPOSITORY}/releases/tags/{tag}'))
    for file in assets:
        found = next((a for a in release['assets'] if a['name'] == file.name), None)
        if found:
            if found.get('digest') != 'sha256:' + digest(file) or found['size'] != file.stat().st_size:
                raise SystemExit('Existing draft asset differs')
        else:
            run('gh', 'release', 'upload', tag, file, '--repo', REPOSITORY)
    release = json.loads(run('gh', 'api', f'repos/{REPOSITORY}/releases/tags/{tag}'))
    if {a['name'] for a in release['assets']} != {f.name for f in assets}:
        raise SystemExit('Unexpected draft assets')
    for file in assets:
        found = next(a for a in release['assets'] if a['name'] == file.name)
        if found.get('digest') != 'sha256:' + digest(file) or found['size'] != file.stat().st_size:
            raise SystemExit('GitHub upload integrity mismatch')
    run('gh', 'release', 'edit', tag, '--repo', REPOSITORY, '--draft=false', '--latest', '--notes-file', args.notes)
    final = json.loads(run('gh', 'api', f'repos/{REPOSITORY}/releases/tags/{tag}'))
    if final['draft'] or final['prerelease']:
        raise SystemExit('Release did not publish as stable')
    print(final['html_url'])

if __name__ == '__main__':
    main()
