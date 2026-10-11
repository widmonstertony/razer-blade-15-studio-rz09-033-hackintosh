#!/usr/bin/env python3
"""Prepare a private, non-boot-qualified reinstall workspace. Never writes ESP/root/TCC.

Uses Python 3 stdlib only. Apple-derived inputs are fetched by the user from
their pinned sources after license consent; they are never published here.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import shutil
import stat
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = Path(__file__).with_name('profiles.json')

def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()

def require_profile(manifest, build):
    profile = manifest['profiles'].get(build)
    if not profile or profile['qualification'].startswith('blocked-'):
        raise ValueError('Unsupported/unqualified build: ' + build)
    return profile

def root_plan(manifest, build):
    """Describe a candidate even if deployment is blocked. No hardware writes."""
    profile = manifest['profiles'].get(build)
    if profile is None:
        raise ValueError('Unknown build: ' + build)
    baseline = manifest['profiles']['25C56']
    return {
        'schema': 1, 'build': build, 'macOS': profile['macOS'],
        'qualification': profile['qualification'],
        'deploymentAllowed': False,  # clean-install rehearsal not passed for either profile
        'patcherCommit': baseline['patcherCommit'],
        'supportPackage': baseline['supportPackage'],
        'kdkVersion': profile['kdkVersion'], 'kdkBuild': build,
        'kdkFallbackAllowed': False,
        'rootSets': baseline['rootSets'], 'operations': manifest['rootRecipe'],
        'security': baseline['security'],
        'additionalEngineEffects': ['full matching KDK SLE merge', 'kernel collection rebuild',
                                    'APFS system snapshot creation'],
        'mandatoryGates': ['independent backup and recovery USB boot', 'exact physical hardware',
                           'exact OS/KDK build', 'only the two expected detected patch sets',
                           'valid reviewed patcher build/source and payload integrity',
                           'successful kmutil and APFS snapshot', 'USB cold-start functional acceptance'],
        'efiWritten': False, 'systemPatched': False, 'executesPatcher': False,
    }

def verify_root_inputs(manifest, build, support_root, kdk_root):
    """Inspect pre-extracted inputs; never mounts/installs/patches the host."""
    plan = root_plan(manifest, build)
    support_root, kdk_root = Path(support_root).resolve(), Path(kdk_root).resolve()
    version_file = kdk_root / 'System/Library/CoreServices/SystemVersion.plist'
    version = plistlib.loads(version_file.read_bytes())
    if (version.get('ProductBuildVersion') != build or
        version.get('ProductVersion') != plan['kdkVersion']):
        raise ValueError('Exact KDK version/build mismatch; no fallback permitted')
    # Executables plus all three compatibility-shim dylibs; not full bundle validation.
    files = {}
    for target, sha in manifest['profiles']['25C56']['rootFiles'].items():
        selector = '26.0 Beta 1' if target.startswith('/System/Library/Extensions') else '13.7.2-25'
        files[selector + target] = sha
    files.update({
        '13.7.2-25/System/Library/PrivateFrameworks/IO80211.framework/Versions/A/IO80211Old.dylib':
            '4e396ebbdad84a52104231d4f938a20c0595653d43a65a82c655c9dc38259215',
        '13.7.2-25/System/Library/PrivateFrameworks/WiFiPeerToPeer.framework/Versions/A/LibSystemShim.dylib':
            '4be59a19d25bd3154141f364b1a799fe22c7204367eb74ffc5fa0876e7e80170',
        '13.7.2-25/System/Library/PrivateFrameworks/WiFiPeerToPeer.framework/Versions/A/WiFiPeerToPeerOld.dylib':
            '97f4d13377a1bcd57d92c8d2493be5edf759817547b7d2187049d6ec9e5fb71f',
    })
    checks = {}
    for relative, sha in files.items():
        path = (support_root / relative).resolve()
        checks[relative] = support_root in path.parents and path.is_file() and digest(path) == sha
    cfl = kdk_root / 'System/Library/Extensions/AppleIntelCFLGraphicsFramebuffer.kext/Contents/MacOS/AppleIntelCFLGraphicsFramebuffer'
    expected = manifest['profiles'][build].get('framebufferSha256')
    cfl_ok = expected is None or cfl.is_file() and digest(cfl) == expected
    if not all(checks.values()) or not cfl_ok:
        raise ValueError('Root input executable/shim/CFL SHA256 mismatch')
    return {'build': build, 'exactKdkVersionBuild': True, 'rootInputExecutableHashes': checks,
            'framebufferMatchesIfPinned': cfl_ok, 'deploymentAllowed': False,
            'scope': 'Input executable evidence only, not complete bundle/ABI/KC/boot qualification',
            'efiWritten': False, 'systemPatched': False}

def safe_zip_members(archive, bundle):
    members = []
    for info in archive.infolist():
        path = PurePosixPath(info.filename)
        if info.filename.startswith('__MACOSX/'):
            continue
        if (path.is_absolute() or '..' in path.parts or '\\' in info.filename or
            not path.parts or path.parts[0] != bundle or '\x00' in info.filename):
            raise ValueError('Unsafe/unexpected archive member: ' + info.filename)
        mode = info.external_attr >> 16
        if stat.S_ISLNK(mode):
            raise ValueError('Symlink in kext ZIP requires separate review: ' + info.filename)
        members.append(info)
    if not any(m.filename == bundle + '/Contents/Info.plist' for m in members):
        raise ValueError('Missing bundle Info.plist')
    return members

def extract_bundle(asset, destination, bundle):
    with zipfile.ZipFile(asset) as archive:
        members = safe_zip_members(archive, bundle)  # validate the ENTIRE ZIP before writing
        for info in members:
            path = destination / info.filename
            if info.is_dir():
                path.mkdir(parents=True, exist_ok=True)
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(info) as src, path.open('xb') as dst:
                    shutil.copyfileobj(src, dst)
                mode = info.external_attr >> 16
                path.chmod(0o755 if mode & 0o111 else 0o644)

def fetch(asset, cache, offline):
    path = cache / asset['name']
    if path.exists():
        if path.is_symlink() or not path.is_file() or digest(path) != asset['sha256']:
            raise ValueError('Cached asset failed SHA256; refusing overwrite: ' + asset['name'])
        return path
    if offline:
        raise ValueError('Offline cache missing: ' + asset['name'])
    request = urllib.request.Request(asset['url'], headers={'User-Agent': 'Razer-Reinstall-Prepare/1'})
    # No archive execution, curl|shell, installers, privilege elevation or auto-mount.
    fd, temp_name = tempfile.mkstemp(prefix='asset-', dir=cache)
    temp = Path(temp_name)
    try:
        with os.fdopen(fd, 'wb') as dst, urllib.request.urlopen(request, timeout=60) as src:
            shutil.copyfileobj(src, dst)
        if digest(temp) != asset['sha256']:
            raise ValueError('Downloaded asset failed SHA256: ' + asset['name'])
        os.rename(temp, path)
    finally:
        if temp.exists():
            temp.unlink()  # only this invocation's exact mkstemp file
    return path

def check_public_tree():
    sums = ROOT / 'SHA256SUMS.txt'
    for line in sums.read_text().splitlines():
        expected, name = line.split(None, 1)
        name = name.lstrip('*')
        path = (ROOT / name).resolve()
        if ROOT not in path.parents or not path.is_file() or digest(path) != expected:
            raise ValueError('Public checkout mismatch: ' + name)

def prepare(args, manifest):
    require_profile(manifest, args.build)
    if args.build != manifest.get('efiBuild'):
        raise ValueError('This checkout EFI does not match requested build; use its matching backup branch')
    if not args.accept_asset_licenses:
        raise ValueError('Review dependency licenses and provide --accept-asset-licenses')
    check_public_tree()
    private = ROOT / '.reinstall-private'
    if private.is_symlink():
        raise ValueError('Private workspace must not be a symlink')
    private.mkdir(mode=0o700, exist_ok=True)
    cache = private / 'cache'
    if cache.is_symlink():
        raise ValueError('Cache must not be a symlink')
    cache.mkdir(exist_ok=True)
    assets = [a for a in manifest['assets'] if args.with_root_support or a['kind'] != 'root-support']
    verified = {a['name']: fetch(a, cache, args.offline) for a in assets}
    stage = Path(tempfile.mkdtemp(prefix=args.build+'-', dir=private))
    shutil.copytree(ROOT / 'EFI', stage / 'EFI')
    for asset in assets:
        source = verified[asset['name']]
        if asset['kind'] == 'kext-zip':
            extract_bundle(source, stage / 'EFI/OC/Kexts', asset['bundle'])
        elif asset['kind'] == 'driver':
            target = stage / 'EFI/OC/Drivers' / asset['name']
            if target.exists():
                raise ValueError('Refusing to overwrite driver: ' + target.name)
            shutil.copyfile(source, target)
    result = {
        'build': args.build, 'assetsSha256Verified': True,
        'stage': str(stage), 'efiWritten': False, 'systemPatched': False,
        'bootQualified': False, 'personalIdentityConfigured': False,
        'missingSteps': ['unique SMBIOS', 'exact-hardware verification', 'USB ocvalidate/boot',
                         'exact KDK', 'reviewed root-patch application', 'R4 install',
                         'audio/Wi-Fi/menu-and-lid-wake/lock tests'],
    }
    (stage / 'PREPARATION-RESULT.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))

def audit(args, manifest):
    build = subprocess.check_output(['/usr/bin/sw_vers', '-buildVersion'], text=True).strip()
    plan = root_plan(manifest, build)  # reading a research target is not deployment approval
    baseline = manifest['profiles']['25C56']
    paths = baseline['rootFiles']
    files = {name: Path(name).is_file() and digest(name) == sha for name, sha in paths.items()}
    metadata_path = Path('/System/Library/CoreServices/OpenCore-Legacy-Patcher.plist')
    metadata = plistlib.loads(metadata_path.read_bytes()) if metadata_path.exists() else {}
    expected_url = 'https://github.com/lzhoang2801/OpenCore-Legacy-Patcher/commit/' + plan['patcherCommit']
    expected_kdk = '/Library/Developer/KDKs/KDK_' + plan['kdkVersion'] + '_' + build + '.kdk'
    metadata_ok = (metadata.get('Commit URL') == expected_url and
                   metadata.get('OpenCore Legacy Patcher') == 'v3.0.0' and
                   metadata.get('PatcherSupportPkg') == 'v' + plan['supportPackage'] and
                   metadata.get('OS Version') == manifest['profiles'][build]['darwin'] + ' (' + build + ')' and
                   metadata.get('Kernel Debug Kit Used') == expected_kdk and
                   all(s in metadata for s in plan['rootSets']))
    print(json.dumps({
        'build': build, 'rootExecutableHashes': files, 'rootMetadataMatches': metadata_ok,
        'rootFileAuditPassed': all(files.values()) and metadata_ok,
        'deploymentAllowed': False,
        'scope': 'Read-only file evidence, not complete bundle integrity, hardware/functionality or clean-install qualification',
        'efiWritten': False, 'systemPatched': False,
    }, indent=2))
    return 0 if all(files.values()) and metadata_ok else 1

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    prep = sub.add_parser('prepare')
    prep.add_argument('--build', default='25G241')
    prep.add_argument('--accept-asset-licenses', action='store_true')
    prep.add_argument('--with-root-support', action='store_true')
    prep.add_argument('--offline', action='store_true')
    sub.add_parser('audit')
    plan = sub.add_parser('plan', help='Read-only plan, including blocked research builds')
    plan.add_argument('--build', required=True)
    inputs = sub.add_parser('verify-inputs', help='Read-only exact KDK and extracted support-file audit')
    inputs.add_argument('--build', required=True)
    inputs.add_argument('--support-root', type=Path, required=True)
    inputs.add_argument('--kdk-root', type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text())
    try:
        if args.action == 'prepare':
            prepare(args, manifest)
            return 0
        if args.action == 'plan':
            print(json.dumps(root_plan(manifest, args.build), indent=2))
            return 0
        if args.action == 'verify-inputs':
            print(json.dumps(verify_root_inputs(manifest, args.build, args.support_root, args.kdk_root), indent=2))
            return 0
        return audit(args, manifest)
    except (ValueError, OSError, subprocess.CalledProcessError) as exc:
        print('REFUSED: ' + str(exc), file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
