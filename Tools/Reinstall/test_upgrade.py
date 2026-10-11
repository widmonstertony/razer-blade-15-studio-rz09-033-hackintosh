import copy
import importlib.util
from pathlib import Path
import plistlib
import unittest

spec=importlib.util.spec_from_file_location('upgrade',Path(__file__).with_name('prepare_upgrade.py'))
tool=importlib.util.module_from_spec(spec); spec.loader.exec_module(tool)

class UpgradeTests(unittest.TestCase):
    def setUp(self):
        self.baseline=plistlib.loads((tool.ROOT/'EFI/OC/config.plist').read_bytes())
        # This legacy migration helper requires the old 26.2 baseline, not main.
        args=self.baseline['NVRAM']['Add'][tool.GUID]['boot-args'].split()
        self.baseline['NVRAM']['Add'][tool.GUID]['boot-args']=' '.join(x for x in args if x!=tool.FLAG)
        item=next(x for x in self.baseline['Kernel']['Add'] if x['BundlePath']==tool.OLED)
        item['MinKernel'],item['MaxKernel']='25.2.0','25.2.99'

    def test_current_main_refused_as_old_upgrade_baseline(self):
        current=plistlib.loads((tool.ROOT/'EFI/OC/config.plist').read_bytes())
        with self.assertRaises(ValueError): tool.configure(current,'installer-native')

    def test_no_baseline_mutation_and_exact_rollback(self):
        original=copy.deepcopy(self.baseline)
        for phase in ['rollback','installer-native','oled-isolated-test','wifi-isolated-test']:
            changed=tool.configure(self.baseline,phase)
            self.assertEqual(self.baseline,original)
            if phase=='rollback': self.assertEqual(changed,original)

    def test_add_block_are_paired(self):
        for phase in ['installer-native','oled-isolated-test','wifi-isolated-test']:
            config=tool.configure(self.baseline,phase)
            entries=[x for x in config['Kernel']['Add'] if x['BundlePath'] in tool.WIFI]
            entries += [x for x in config['Kernel']['Block'] if x['Identifier']=='com.apple.iokit.IOSkywalkFamily']
            self.assertEqual(len(entries),4)
            ranges={(x['MinKernel'],x['MaxKernel']) for x in entries}
            self.assertEqual(ranges,{('25.6.0','25.6.99')} if phase=='wifi-isolated-test' else {('23.0.0','25.2.99')})
            args=config['NVRAM']['Add'][tool.GUID]['boot-args'].split()
            self.assertEqual(tool.FLAG in args,phase!='installer-native')
            self.assertIn('amfi=0x80',args)

    def test_missing_duplicate_and_policy_refuse(self):
        duplicate=copy.deepcopy(self.baseline)
        item=next(x for x in duplicate['Kernel']['Add'] if x['BundlePath'] in tool.WIFI)
        duplicate['Kernel']['Add'].append(copy.deepcopy(item))
        with self.assertRaises(ValueError): tool.configure(duplicate,'installer-native')
        missing=copy.deepcopy(self.baseline); missing['Kernel']['Block']=[]
        with self.assertRaises(ValueError): tool.configure(missing,'oled-isolated-test')
        bad=copy.deepcopy(self.baseline); bad['NVRAM']['Add'][tool.GUID]['boot-args']='-lilubetaall'
        with self.assertRaises(ValueError): tool.configure(bad,'wifi-isolated-test')
        with self.assertRaises(ValueError): tool.configure(self.baseline,'production')

    def test_no_broadened_oled_or_security_baseline(self):
        changed=copy.deepcopy(self.baseline)
        next(x for x in changed['Kernel']['Add'] if x['BundlePath']==tool.OLED)['MaxKernel']=''
        with self.assertRaises(ValueError): tool.configure(changed,'installer-native')
        changed=copy.deepcopy(self.baseline)
        changed['Misc']['Security']['SecureBootModel']='Default'
        with self.assertRaises(ValueError): tool.configure(changed,'installer-native')
        changed=copy.deepcopy(self.baseline)
        changed['NVRAM']['Add'][tool.GUID]['csr-active-config']=b'\x00'*4
        with self.assertRaises(ValueError): tool.configure(changed,'wifi-isolated-test')

if __name__=='__main__': unittest.main()
