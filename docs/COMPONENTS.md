# Component inventory

Versions are read from the captured EFI. “Enabled” refers to the top-level OpenCore kernel entry; plug-in entries may have their own state.

| Component | Version | Baseline state |
| --- | ---: | --- |
| AMFIPass.kext | 1.4.1 | Disabled |
| AppleALC.kext | 1.9.5 | Enabled |
| CPUFriend.kext | 1.3.0 | Enabled |
| CPUFriendDataProvider.kext | 1.0.1 | Enabled |
| CpuTscSync.kext | 1.1.2 | Present, not referenced |
| HibernationFixup.kext | 1.5.4 | Present, not referenced |
| Lilu.kext | 1.7.1 | Enabled |
| NoTouchID.kext | 1.0.3 | Enabled |
| RealtekCardReader.kext | 0.9.7 | Enabled |
| RealtekCardReaderFriend.kext | 1.0.4 | Enabled |
| RestrictEvents.kext | 1.1.6 | Enabled |
| SMCBatteryManager.kext | 1.3.7 | Enabled |
| SMCLightSensor.kext | 1.3.7 | Enabled |
| SMCProcessor.kext | 1.3.7 | Enabled |
| USBPorts.kext | 1.0 | Enabled |
| VerbStub.kext | 1.0.3 | Enabled |
| VirtualSMC.kext | 1.3.7 | Enabled |
| VoodooI2C.kext | 2.9.1 | Enabled |
| VoodooI2CHID.kext | 1.0 | Enabled |
| VoodooPS2Controller.kext | 2.3.6 | Enabled |
| WhateverGreen.kext | 1.7.0 | Enabled |

## Apple-derived components omitted from Git

| Referenced component | Baseline state | Public repository |
| --- | --- | --- |
| IOSkywalkFamily.kext | Enabled for Darwin 23+ | Omitted |
| IO80211FamilyLegacy.kext | Enabled for Darwin 23+ | Omitted |
| AirPortBrcmNIC plug-in | Enabled for Darwin 23+ | Omitted with parent kext |
| HfsPlus.efi | Enabled | Omitted |
| apfs_aligned.efi | Enabled | Omitted |

## OpenCore note

The captured `OpenCore.efi` is dated 2025-11-18 and the configuration validates with `ocvalidate` 1.0.6. Its binary hash is not byte-identical to the official 1.0.6 release binary, so it may be a nightly or custom build. This repository preserves that fact instead of claiming an exact official release match.
