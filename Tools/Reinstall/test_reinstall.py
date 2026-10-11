import importlib.util
import io
import json
from pathlib import Path
import stat
import tempfile
import unittest
import zipfile
import plistlib
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('reinstall', Path(__file__).with_name('reinstall.py'))
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)

class ReinstallTests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads(tool.MANIFEST.read_text())

    def test_profile_allowlist(self):
        self.assertEqual(tool.require_profile(self.manifest, '25C56')['macOS'], '26.2')
        self.assertEqual(tool.require_profile(self.manifest, '25G241')['macOS'], '26.7.1')
        for build in ['25G82', '25C57', '', '27.2']:
            with self.assertRaises(ValueError): tool.require_profile(self.manifest, build)

    def test_frozen_urls_and_hashes(self):
        for asset in self.manifest['assets']:
            self.assertTrue(asset['url'].startswith('https://'))
            self.assertNotIn('/master/', asset['url'])
            self.assertEqual(len(asset['sha256']), 64)

    def test_other_branch_cannot_stage_wrong_efi(self):
        from types import SimpleNamespace
        with self.assertRaisesRegex(ValueError, 'matching backup branch'):
            tool.prepare(SimpleNamespace(build='25C56'), self.manifest)

    def test_main_efi_profile_and_nvidia_policy(self):
        config=plistlib.loads((tool.ROOT/'EFI/OC/config.plist').read_bytes())
        self.assertEqual(self.manifest['efiBuild'],'25G241')
        ranges={(x['MinKernel'],x['MaxKernel']) for x in config['Kernel']['Add']
                if x['BundlePath'] in ['RazerOLEDWakeFix.kext','IOSkywalkFamily.kext','IO80211FamilyLegacy.kext','IO80211FamilyLegacy.kext/Contents/PlugIns/AirPortBrcmNIC.kext']}
        self.assertEqual(ranges,{('25.6.0','25.6.99')})
        self.assertTrue(next(x for x in config['ACPI']['Add'] if x['Path']=='SSDT-DDGPU.aml')['Enabled'])
        self.assertFalse(next(x for x in config['Kernel']['Add'] if x['BundlePath']=='AMFIPass.kext')['Enabled'])

    def test_plan_is_not_deployment_permission(self):
        for build in ['25C56','25G241']:
            plan=tool.root_plan(self.manifest,build)
            self.assertFalse(plan['deploymentAllowed'])
            self.assertFalse(plan['kdkFallbackAllowed'])
            self.assertFalse(plan['executesPatcher'])
            self.assertEqual(len(plan['operations']),4)
            self.assertEqual(plan['rootSets'],['Modern Wireless','Modern Audio'])
        with self.assertRaises(ValueError): tool.root_plan(self.manifest,'25G242')

    def test_exact_kdk_input_gate(self):
        with tempfile.TemporaryDirectory() as temp:
            kdk=Path(temp)/'kdk'; support=Path(temp)/'support'
            version=kdk/'System/Library/CoreServices/SystemVersion.plist'
            version.parent.mkdir(parents=True)
            for build,product in [('25G82','26.7.1'),('25G241','26.6.2')]:
                version.write_bytes(plistlib.dumps({'ProductBuildVersion':build,'ProductVersion':product}))
                with self.assertRaisesRegex(ValueError,'Exact KDK'):
                    tool.verify_root_inputs(self.manifest,'25G241',support,kdk)
            version.write_bytes(plistlib.dumps({'ProductBuildVersion':'25G241','ProductVersion':'26.7.1'}))
            with self.assertRaisesRegex(ValueError,'SHA256'):
                tool.verify_root_inputs(self.manifest,'25G241',support,kdk)

    def archive(self, extra=None):
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, 'w') as z:
            z.writestr('Safe.kext/Contents/Info.plist', 'test')
            if extra: z.writestr(*extra)
        stream.seek(0)
        return zipfile.ZipFile(stream)

    def test_safe_archive(self):
        with self.archive() as z:
            self.assertEqual(len(tool.safe_zip_members(z, 'Safe.kext')), 1)

    def test_traversal_and_unexpected_roots(self):
        for name in ['../escape', '/tmp/escape', 'Safe.kext/../../escape',
                     'Other.kext/file', 'Safe.kext\\escape']:
            with self.archive((name, 'payload')) as z:
                with self.assertRaises(ValueError): tool.safe_zip_members(z, 'Safe.kext')

    def test_symlink_refusal(self):
        info = zipfile.ZipInfo('Safe.kext/link')
        info.create_system = 3
        info.external_attr = (stat.S_IFLNK | 0o777) << 16
        with self.archive((info, '../../escape')) as z:
            with self.assertRaises(ValueError): tool.safe_zip_members(z, 'Safe.kext')

    def test_cache_mismatch_and_missing_offline(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            asset = {'name':'asset.bin', 'sha256':'0'*64}
            with self.assertRaises(ValueError): tool.fetch(asset, path, True)
            (path/'asset.bin').write_bytes(b'wrong')
            with self.assertRaises(ValueError): tool.fetch(asset, path, True)
            self.assertEqual((path/'asset.bin').read_bytes(), b'wrong')

if __name__ == '__main__': unittest.main()
