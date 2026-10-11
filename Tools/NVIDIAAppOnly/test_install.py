#!/usr/bin/env python3
"""CPU/temp-files-only fault tests. No real root writes, launchctl or GPU API."""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import tempfile
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('overlay', Path(__file__).with_name('install.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
original_publish = m.publish
original_regular = m.regular

def fake_trusted(p, create=False, mode=0o755):
    if create and not p.exists():
        p.mkdir(mode=mode)
    m.require(p.is_dir() and p.resolve() == p, 'Unsafe temp test path')

def fake_command(args, check=True):
    if '--run' in args:
        return 'ALREADY_INITIALIZED_THIS_BOOT; GPU_CALLS=0'
    return ''

for fault in ['none', 'helper', 'source', 'list', 'daemon', 'bootstrap']:
    with tempfile.TemporaryDirectory(prefix='nvidia-overlay-test-', dir='/private/tmp') as tmp:
        root = Path(tmp)
        app = root / 'support' / 'RazerNVIDIA'
        daemon = root / 'daemons' / 'com.tonytan.nvidia-app-only.plist'
        allow = root / 'gpu' / 'nvmtl-allow.txt'
        for p in [app.parent, daemon.parent, allow.parent]:
            p.mkdir()
        allow.write_text('-WindowServer\nmetal-workload-probe\n')
        inode = allow.stat().st_ino
        original = allow.read_bytes()
        targets = {'helper': app / 'nvidia-startup', 'source': app / 'nvidia-startup.c',
                   'list': allow, 'daemon': daemon}
        def publish(data, dest, mode=0o644):
            if fault in targets and dest == targets[fault]:
                raise RuntimeError('Injected write fault')
            return original_publish(data, dest, mode)
        def command(args, check=True):
            if fault == 'bootstrap' and 'bootstrap' in args:
                raise RuntimeError('Injected launch fault')
            return fake_command(args, check)
        with patch.object(m, 'APP', app), patch.object(m, 'DAEMON', daemon), \
             patch.object(m, 'ALLOW', allow), patch.object(m, 'BACKUPS', app.parent / 'backups'), \
             patch.object(m, 'LABEL', 'test-only'), patch.object(m, 'trusted_dir', fake_trusted), \
             patch.object(m, 'regular', lambda p, root=False: original_regular(p, root=False)), \
             patch.object(m, 'baseline', lambda: ({'test': True}, {'vramBytes': m.VRAM})), \
             patch.object(m, 'package_check', lambda: None), patch.object(m, 'command', command), \
             patch.object(m, 'publish', publish), patch.object(os, 'getuid', lambda: 0), \
             patch.object(os, 'chown', lambda *a: None), patch.dict(os.environ, {'SUDO_UID': '501'}), \
             contextlib.redirect_stdout(io.StringIO()):
            failed = False
            try:
                m.install()
            except RuntimeError:
                failed = True
            if fault == 'none':
                assert not failed
                tx = next((app.parent / 'backups').glob('txn-*'))
                import json
                state = json.loads((tx / 'state.json').read_text())
                m.rollback(tx, state)
            else:
                assert failed
            assert allow.read_bytes() == original and allow.stat().st_ino == inode
            assert not app.exists() and not daemon.exists()
    print('FAULT_TEST=' + fault + ':PASS')
print('RESULT=OVERLAY-FAULT-AND-ROLLBACK-TESTS-PASS; GPU_CALLS=0; REAL_SYSTEM_WRITES=0')
