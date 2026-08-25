# V61-P1 component inventory

Versions below describe the sanitized public V61-P1 tree. “Enabled” refers to the top-level OpenCore entry; plug-ins have their own ordered entries.

| Component | Version | V61-P1 state |
| --- | ---: | --- |
| OpenCore | 1.0.7 | Enabled |
| RazerOLEDWakeFix.kext | 4.5.0 / V61 | Enabled only on Darwin 25.2.x |
| AMFIPass.kext | 1.4.1 | Disabled |
| AppleALC.kext | 1.9.7 | Enabled |
| CPUFriend.kext | 1.3.0 | Enabled |
| CPUFriendDataProvider.kext | 1.0.1 | Enabled |
| CpuTscSync.kext | 1.1.2 | Present, not referenced |
| HibernationFixup.kext | 1.5.4 | Present, not referenced |
| Lilu.kext | 1.7.2 | Enabled |
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
| VoodooI2C.kext | 2.9.1 + HID V12 | Enabled |
| VoodooI2CHID.kext | 1.0 + HID V12 | Enabled |
| VoodooPS2Controller.kext | 2.3.7 | Controller and keyboard enabled |
| WhateverGreen.kext | 1.7.0 | Enabled |

## Apple-derived components omitted from Git

| Referenced component | Config state | Public repository |
| --- | --- | --- |
| IOSkywalkFamily.kext | Enabled for Darwin 23+ | Omitted |
| IO80211FamilyLegacy.kext | Enabled for Darwin 23+ | Omitted |
| AirPortBrcmNIC plug-in | Enabled for Darwin 23+ | Omitted with parent kext |
| HfsPlus.efi | Enabled | Omitted |
| apfs_aligned.efi | Enabled | Omitted |

## User-space wake components

| Component | Behavior |
| --- | --- |
| OLEDWakeRescueAgent | Aqua LaunchAgent; event-driven IOKit wake listener; eight-second delay |
| DisplayModeNudgeV2 | Exact Samsung/mode gate; app-only 1600×900 HiDPI transaction and exact restore |

Both tested executables are x86_64 and ad-hoc signed. Source and build scripts are under `Tools/OLEDWakeRescue`.
