#!/usr/bin/env python3
"""Exact-target overlay for an ALREADY installed and small-workload-tested driver.
No EFI, kext, collection, security, root patch, NVRAM or display-mode write.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import shutil
import stat
import subprocess
import tempfile
import uuid

PACKAGE = Path(__file__).resolve().parent
APP = Path('/Library/Application Support/RazerNVIDIA')
BACKUPS = Path('/Library/Application Support/RazerNVIDIABackups')
DAEMON = Path('/Library/LaunchDaemons/com.tonytan.nvidia-app-only.plist')
ALLOW = Path('/Library/GPUBundles/nvmtl-allow.txt')
LABEL = 'system/com.tonytan.nvidia-app-only'
PINS = {
 'nvidia-startup': '3fa9e31a78c54d6de2e87a029e28a4cef8f27cf8816893189104682130f8152f',
 'nvidia-startup.c': 'e92489af0a4ffa1cbb3de0c5aa263f5a52267e5fa8dd34b40f8381db91a99813',
 'nvmtl-allow.txt': '395e0ba4db1bd07ed34a5337bdaaf88b8467a9ae5eb7092443da8ae9557078d8',
 'com.tonytan.nvidia-app-only.plist': 'd1bc822900aee3a877fb7fe594aaa3de496414882faaea81f06036e6bb078d43',
}
COLLECTIONS = {
 '/System/Library/KernelCollections/BootKernelExtensions.kc': 'f94046e9be8f83e014ab1965d3ee5a8f11048c17c60d27850d7dee3672e06cdc',
 '/System/Library/KernelCollections/SystemKernelExtensions.kc': '9a25de6e34c28f8cd9802aa61bbcf6395f5884de70e39558523ba82e90b3c4ae',
 '/Library/KernelCollections/AuxiliaryKernelExtensions.kc': '3b323074df4dea0c4e61de275e96e0ca5713eb20d9da5057394e09f9b8562b14',
}
ARGS = '-v keepsyms=1 debug=0x100 amfi=0x80 -lilubetaall -razer-oled25g241-test nvaccel=1 nvfb=0 -nvrmnogo -nvrmnobootscreen -nvrmnoflip'
VRAM = 17179869184

def require(value, message):
    if not value:
        raise RuntimeError(message)

def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1024 * 1024), b''):
            h.update(b)
    return h.hexdigest()

def command(args, check=True):
    p = subprocess.run([str(a) for a in args], capture_output=True, text=True, timeout=60)
    if check:
        require(p.returncode == 0, str(args[0]) + ': ' + (p.stderr or p.stdout))
    return p.stdout.strip()

def trusted_dir(p, create=False, mode=0o755):
    if create and not p.exists():
        p.mkdir(mode=mode)
        os.chown(p, 0, 0)
        os.chmod(p, mode)
    s = p.lstat()
    require(stat.S_ISDIR(s.st_mode) and p.resolve() == p and s.st_uid == 0 and not s.st_mode & 0o022,
            'Untrusted directory: ' + str(p))

def regular(p, root=False):
    s = p.lstat()
    require(stat.S_ISREG(s.st_mode) and not s.st_mode & 0o022 and s.st_nlink == 1,
            'Unsafe file: ' + str(p))
    if root:
        require(s.st_uid == 0, 'File not root-owned: ' + str(p))
    return s

def publish(data, dest, mode=0o644):
    fd, tmp = tempfile.mkstemp(prefix='.nvidia-stage-', dir=dest.parent)
    try:
        with os.fdopen(fd, 'wb') as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        os.chown(tmp, 0, 0)
        os.chmod(tmp, mode)
        os.replace(tmp, dest)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)  # This invocation's exact temporary file only.

def save(tx, state):
    publish((json.dumps(state, indent=2) + '\n').encode(), tx / 'state.json', 0o600)

def package_check():
    for name, expected in PINS.items():
        regular(PACKAGE / name)
        require(sha(PACKAGE / name) == expected, 'Package hash changed: ' + name)
    parsed = plistlib.loads((PACKAGE / DAEMON.name).read_bytes())
    require(parsed['RunAtLoad'] is True and parsed['KeepAlive'] is False, 'Unexpected launch policy')
    require(parsed['ProgramArguments'] == [str(APP / 'nvidia-startup'), '--run'], 'Unexpected command')
    entries = [s for s in (PACKAGE / 'nvmtl-allow.txt').read_text().splitlines() if s and not s.startswith('#')]
    require(entries == ['-WindowServer', '*'], 'WindowServer deny must precede wildcard')
    require('GPU_CALLS=0' in command([PACKAGE / 'nvidia-startup', '--self-test']), 'CPU self-test failed')

def baseline():
    require(command(['/usr/bin/sw_vers', '-buildVersion']) == '25G241', 'Exact 25G241 build required')
    require(command(['/usr/sbin/sysctl', '-n', 'kern.bootargs']) == ARGS, 'Boot arguments changed')
    for p, h in COLLECTIONS.items():
        regular(Path(p), root=True)
        require(sha(p) == h, 'Active collection changed: ' + p)
    security = {
      'sip': command(['/usr/bin/csrutil', 'status']),
      'authenticatedRoot': command(['/usr/bin/csrutil', 'authenticated-root', 'status']),
      'bootArgs': command(['/usr/sbin/sysctl', '-n', 'kern.bootargs']),
    }
    target = json.loads(command([PACKAGE / 'nvidia-startup', '--inspect']))
    require(target['eligible'] and target['intelPanelReady'] and target['vramBytes'] == VRAM,
            'Already-initialized Intel-primary target required; no automatic first-time experiment')
    return security, target

def rollback(tx, state):
    # Never stop a helper while its journal shows a pending hardware call.
    if (APP / 'journals').exists():
        for f in (APP / 'journals').glob('boot-*.jsonl'):
            records = [json.loads(s) for s in f.read_text().splitlines()]
            require(not records or records[-1].get('phase') != 'PENDING_BEFORE_OPEN',
                    'Firmware call pending; manual shutdown/recovery required before removal')
    command(['/bin/launchctl', 'bootout', 'system', DAEMON], check=False)
    q = tx / 'quarantine'
    trusted_dir(q, create=True, mode=0o700)
    if DAEMON.exists():
        regular(DAEMON, root=True)
        require(sha(DAEMON) == PINS[DAEMON.name], 'Unknown daemon; refuse removal')
        os.rename(DAEMON, q / DAEMON.name)
    if APP.exists():
        trusted_dir(APP)
        identity = state.get('createdAppIdentity')
        require(identity == [APP.stat().st_dev, APP.stat().st_ino], 'Application directory identity changed')
        if (APP / 'nvidia-startup').exists():
            regular(APP / 'nvidia-startup', root=True)
            require(sha(APP / 'nvidia-startup') == PINS['nvidia-startup'], 'Unknown helper; refuse removal')
        os.rename(APP, q / 'RazerNVIDIA')
    original = tx / 'allow-original.txt'
    if original.exists():
        require(sha(original) == state['beforeAllowSha256'], 'Original allow-list changed')
        if ALLOW.exists():
            regular(ALLOW, root=True)
            require(sha(ALLOW) == PINS['nvmtl-allow.txt'], 'Unknown live allow-list; refuse overwrite')
            os.rename(ALLOW, q / 'allow-apps.txt')
        os.rename(original, ALLOW)  # Restores original inode for stage-1 rollback.
        require(ALLOW.stat().st_ino == state['beforeAllowInode'], 'Original inode not restored')
    state['phase'] = 'rolled-back'
    save(tx, state)
    print('RESULT=NVIDIA-APP-ONLY-SERVICE-REMOVED; DRIVER_STILL_INSTALLED; SHUTDOWN_MANUALLY')

def install():
    require(os.getuid() == 0 and int(os.environ.get('SUDO_UID', '0')) >= 501, 'Owner sudo required')
    package_check()
    require(not APP.exists() and not APP.is_symlink() and not DAEMON.exists() and not DAEMON.is_symlink(),
            'Service paths already exist; refuse overwrite')
    trusted_dir(APP.parent)
    trusted_dir(DAEMON.parent)
    trusted_dir(ALLOW.parent)
    old = regular(ALLOW, root=True)
    entries = [s.strip() for s in ALLOW.read_text().splitlines() if s.strip() and not s.lstrip().startswith('#')]
    require(entries == ['-WindowServer', 'metal-workload-probe'], 'Unexpected prior application policy')
    security, target = baseline()
    trusted_dir(BACKUPS, create=True, mode=0o700)
    tx = BACKUPS / ('txn-' + uuid.uuid4().hex)
    trusted_dir(tx, create=True, mode=0o700)
    state = {'phase': 'backed-up', 'beforeAllowSha256': sha(ALLOW), 'beforeAllowInode': old.st_ino,
             'securityBefore': security, 'registryBefore': target, 'collectionHashes': COLLECTIONS,
             'efiWritten': False, 'driverOrCollectionWritten': False, 'securityWritten': False,
             'sourcePins': PINS, 'automaticRebootOrSleep': False, 'startupColdBootQualified': False,
             'oledSleepWakeQualified': False, 'cudaQualified': False, 'fullyCompatibleWithAllApps': False}
    save(tx, state)
    # Retain the original file by rename rather than copy; old rollback checks
    # file identity and must still work after this overlay is removed.
    os.rename(ALLOW, tx / 'allow-original.txt')
    try:
        trusted_dir(APP, create=True)
        state['createdAppIdentity'] = [APP.stat().st_dev, APP.stat().st_ino]
        save(tx, state)
        trusted_dir(APP / 'journals', create=True, mode=0o700)
        for name in ['nvidia-startup', 'nvidia-startup.c']:
            publish((PACKAGE / name).read_bytes(), APP / name, 0o755 if name == 'nvidia-startup' else 0o644)
            require(sha(APP / name) == PINS[name], 'Installed helper/source hash mismatch')
        publish((PACKAGE / 'nvmtl-allow.txt').read_bytes(), ALLOW)
        publish((PACKAGE / DAEMON.name).read_bytes(), DAEMON)
        # Already initialized this boot: root helper verifies pins then SKIPS,
        # so installation never makes another firmware call.
        result = command([APP / 'nvidia-startup', '--run'])
        require('ALREADY_INITIALIZED_THIS_BOOT; GPU_CALLS=0' in result, 'Did not safely skip current boot')
        command(['/bin/launchctl', 'bootstrap', 'system', DAEMON])
        post_security, post_target = baseline()
        require(post_security == security and post_target == target, 'Post-install baseline changed')
        state['phase'] = 'installed-awaiting-manual-cold-boot'
        state['currentBootHelperSkippedWithZeroGpuCalls'] = True
        state['installedAppAccessPolicy'] = ['-WindowServer', '*']
        state['backgroundServiceRegistered'] = True
        save(tx, state)
        publish((json.dumps({'backup': str(tx), 'helperSha256': PINS['nvidia-startup'],
                 'serviceConfigured': True, 'appPolicyOpenExceptWindowServer': True,
                 'efiWritten': False, 'securityWritten': False, 'startupColdBootQualified': False,
                 'oledSleepWakeQualified': False}, indent=2) + '\n').encode(), APP / 'installation.json')
        print('BACKUP=' + str(tx))
        print('CURRENT_BOOT_GPU_CALLS=0; EFI_WRITTEN=NO; SECURITY_CHANGED=NO; CACHES_CHANGED=NO')
        print('RESULT=NVIDIA-APP-ACCESS-AND-AUTO-INIT-SERVICE-INSTALLED; COLD_BOOT_AND_SLEEP_NOT_YET_QUALIFIED')
    except Exception:
        rollback(tx, state)
        raise

def self_test():
    package_check()
    # Exercise the exact list order semantics from the pinned upstream parser.
    def allowed(name, lines):
        for line in lines:
            if line.startswith('-'):
                if line[1:] == name:
                    return False
                continue
            if line == '*' or line == name:
                return True
        return False
    lines = ['-WindowServer', '*']
    require(not allowed('WindowServer', lines), 'Desktop deny failed')
    for name in ['SomeMetalApp', 'metal-workload-probe', 'UnknownApp']:
        require(allowed(name, lines), 'App exposure failed')
    print('SELF_TEST=PASS; PACKAGE_AND_POLICY=PASS; EFI_WRITTEN=NO; GPU_CALLS=0')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['self-test', 'install', 'remove'])
    parser.add_argument('--transaction', type=Path)
    args = parser.parse_args()
    if args.mode == 'self-test':
        self_test()
    elif args.mode == 'install':
        install()
    else:
        require(os.getuid() == 0 and int(os.environ.get('SUDO_UID', '0')) >= 501, 'Owner sudo required')
        require(args.transaction is not None, 'Exact backup transaction required')
        tx = args.transaction.resolve()
        require(tx.parent == BACKUPS and tx.name.startswith('txn-'), 'Unknown rollback target')
        trusted_dir(BACKUPS)
        trusted_dir(tx)
        state = json.loads((tx / 'state.json').read_text())
        require(state['sourcePins'] == PINS, 'Unknown transaction format')
        require(state['phase'] != 'rolled-back', 'Already removed; no repeat')
        rollback(tx, state)

if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print('STOPPED=' + str(e) + '; no retry/reboot/sleep')
        raise SystemExit(1)
