#!/usr/bin/env python3
"""Make private RESEARCH EFIs only. Does not authorize or perform an upgrade.

Input must be a complete owner-validated baseline EFI, not the bare public EFI.
No ESP mount/write, root patch, NVRAM write, sleep, reboot or GitHub upload.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
GUID = '7C436110-AB2A-4BBB-A880-FE41995C9F82'
WIFI = {'IOSkywalkFamily.kext', 'IO80211FamilyLegacy.kext',
        'IO80211FamilyLegacy.kext/Contents/PlugIns/AirPortBrcmNIC.kext'}
OLED = 'RazerOLEDWakeFix.kext'
FLAG = '-razer-oled25g241-test'

def configure(baseline, phase):
    config = copy.deepcopy(baseline)
    adds = [x for x in config['Kernel']['Add'] if x['BundlePath'] in WIFI]
    blocks = [x for x in config['Kernel']['Block'] if x['Identifier'] == 'com.apple.iokit.IOSkywalkFamily']
    oled = [x for x in config['Kernel']['Add'] if x['BundlePath'] == OLED]
    if (len(adds) != 3 or len({x['BundlePath'] for x in adds}) != 3 or len(blocks) != 1 or
        len(oled) != 1 or any(not x['Enabled'] for x in adds+blocks+oled) or
        blocks[0]['Strategy'] != 'Exclude'):
        raise ValueError('Expected exactly three enabled Wi-Fi Add entries, one Exclude Block and one OLED')
    args = config['NVRAM']['Add'][GUID]['boot-args'].split()
    if 'amfi=0x80' not in args or '-lilubetaall' not in args:
        raise ValueError('Paired baseline boot policy missing; refusing to alter AMFI/TCC')
    if FLAG in args:
        raise ValueError('Baseline already contains experimental OLED flag; use a stable baseline')
    if (oled[0]['MinKernel'],oled[0]['MaxKernel']) != ('25.2.0','25.2.99'):
        raise ValueError('Stable OLED baseline must retain exact 25.2 limits')
    if (config['Misc']['Security']['SecureBootModel'] != 'Disabled' or
        config['NVRAM']['Add'][GUID]['csr-active-config'] != bytes.fromhex('03080000') or
        any(x['Enabled'] for x in config['Kernel']['Add'] if x['BundlePath']=='AMFIPass.kext')):
        raise ValueError('Paired baseline security policy differs; requires separate review')
    if phase == 'rollback':
        return config
    if phase not in {'installer-native', 'oled-isolated-test', 'wifi-isolated-test'}:
        raise ValueError('Unknown phase')
    for entry in adds+blocks:
        entry['MinKernel'] = '25.6.0' if phase == 'wifi-isolated-test' else '23.0.0'
        entry['MaxKernel'] = '25.6.99' if phase == 'wifi-isolated-test' else '25.2.99'
    if phase != 'installer-native':
        oled[0]['MinKernel'], oled[0]['MaxKernel'] = '25.6.0', '25.6.99'
        config['NVRAM']['Add'][GUID]['boot-args'] = ' '.join(args+[FLAG])
    return config

def inventory(root):
    result = {}
    for path in sorted(root.rglob('*')):
        if path.is_symlink():
            raise ValueError('Symlink in EFI input/stage requires separate review: '+str(path))
        if path.is_file():
            result[path.relative_to(root).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result

def prepare(baseline, candidate, validator):
    baseline, candidate, validator = baseline.resolve(), candidate.resolve(), validator.resolve()
    original = plistlib.loads((baseline/'OC/config.plist').read_bytes())
    original_inventory = inventory(baseline)
    inventory(candidate)
    info = plistlib.loads((candidate/'Contents/Info.plist').read_bytes())
    if info.get('CFBundleIdentifier') != 'com.tonytan.RazerOLEDWakeFix' or info.get('CFBundleVersion') != '4.7.1':
        raise ValueError('Expected separately built 4.7.1 multi-version candidate')
    for entry in original['Kernel']['Add']:
        if entry['Enabled']:
            bundle = baseline/'OC/Kexts'/entry['BundlePath']
            if not (bundle/entry['PlistPath']).is_file() or (entry['ExecutablePath'] and
                not (bundle/entry['ExecutablePath']).is_file()):
                raise ValueError('Incomplete baseline kext: '+entry['BundlePath'])
    for entry in original['ACPI']['Add']:
        if entry['Enabled'] and not (baseline/'OC/ACPI'/entry['Path']).is_file():
            raise ValueError('Incomplete baseline ACPI: '+entry['Path'])
    for entry in original['UEFI']['Drivers']:
        if entry['Enabled'] and not (baseline/'OC/Drivers'/entry['Path']).is_file():
            raise ValueError('Incomplete baseline driver: '+entry['Path'])
    # Validate all four configs before creating an artifact.
    phases = ['rollback','installer-native','oled-isolated-test','wifi-isolated-test']
    configs = {phase:configure(original,phase) for phase in phases}
    private = ROOT/'.reinstall-private'
    if private.is_symlink(): raise ValueError('Private workspace symlink refused')
    private.mkdir(mode=0o700,exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='upgrade-25G241-RESEARCH-',dir=private))
    manifest = {'target':'26.7.1 / 25G241', 'bootQualified':False, 'deploymentAllowed':False,
                'identityFromBaseline':True, 'baseline':original_inventory,
                'candidate':inventory(candidate), 'efiWritten':False, 'systemPatched':False,
                'warnings':['NOT a production upgrade kit', 'No isolated boot/KC/wake qualification',
                            'Baseline identity must be owned and configured; no SMBIOS generated',
                            'Installer/native and OLED-only test deliberately have NO BCM4360 Wi-Fi',
                            'Do not boot your working system with an experimental EFI'], 'phases':{}}
    for phase in phases:
        tree = stage/phase/'EFI'
        shutil.copytree(baseline,tree)
        if phase in {'oled-isolated-test','wifi-isolated-test'}:
            shutil.copytree(candidate,tree/'OC/Kexts'/OLED,dirs_exist_ok=True)
        if phase != 'rollback':
            (tree/'OC/config.plist').write_bytes(plistlib.dumps(configs[phase],sort_keys=False))
        result = subprocess.run([str(validator),str(tree/'OC/config.plist')],capture_output=True,text=True)
        if result.returncode:
            raise ValueError('ocvalidate failed; retained private stage for diagnosis: '+str(stage)+'\n'+result.stdout+result.stderr)
        files = inventory(tree)
        changed = sorted(p for p in files.keys()|original_inventory.keys() if files.get(p)!=original_inventory.get(p))
        allowed = {'OC/config.plist'}
        if phase in {'oled-isolated-test','wifi-isolated-test'}:
            allowed |= {'OC/Kexts/'+OLED+'/'+p for p in manifest['candidate']}
        if any(p not in allowed for p in changed):
            raise ValueError('Unexpected stage mutation: '+str(changed))
        manifest['phases'][phase] = {'ocvalidatePassed':True, 'changedFiles':changed, 'sha256':files}
    # A separate target-only R4 rehearsal. The public stable service is unmodified.
    rescue_source = ROOT/'Tools/OLEDWakeRescue'
    rescue_stage = stage/'R4-isolated-test'
    shutil.copytree(rescue_source,rescue_stage)
    installer = rescue_source/'install.sh'
    original_script = installer.read_text()
    expected_guard = '--accept-unvalidated-wake'
    if expected_guard not in original_script:
        raise ValueError('R4 installer source changed; re-audit before generating research wrapper')
    research_script = original_script
    gate = '''\nif [[ "${1:-}" != --isolated-test ]]; then
    print -u2 "RESEARCH ONLY: requires --isolated-test, exact 25G241 and experimental boot flag."
    exit 65
fi
razer_test_boot_args="$(/usr/sbin/sysctl -n kern.bootargs)"
if [[ " $razer_test_boot_args " != *" -razer-oled25g241-test "* ]]; then
    print -u2 "Refusing R4 research install: experimental boot flag absent."
    exit 65
fi
if [[ "$(/usr/bin/sw_vers -buildVersion)" != 25G241 ]]; then
    print -u2 "Research R4 may only run on 25G241."
    exit 65
fi
set -- --accept-unvalidated-wake
'''
    research_script = research_script.replace('set -euo pipefail\n','set -euo pipefail\n'+gate,1)
    (rescue_stage/'install.sh').write_text(research_script)
    manifest['r4Research'] = {'sourceInstallerSha256':hashlib.sha256(installer.read_bytes()).hexdigest(),
                            'targetBuild':'25G241', 'installed':False, 'sleepWakeQualified':False}
    (stage/'RESEARCH-NOT-READY.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return stage

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline-efi',type=Path,required=True)
    p.add_argument('--candidate-kext',type=Path,required=True)
    p.add_argument('--ocvalidate',type=Path,required=True)
    p.add_argument('--research-only',action='store_true',required=True)
    a=p.parse_args()
    try:
        print('RESEARCH_STAGE='+str(prepare(a.baseline_efi,a.candidate_kext,a.ocvalidate)))
        print('DEPLOYMENT_ALLOWED=NO; EFI_WRITTEN=NO; SYSTEM_PATCHED=NO')
    except (ValueError,OSError,KeyError,plistlib.InvalidFileException) as exc:
        p.exit(1,'REFUSED: '+str(exc)+'\n')
