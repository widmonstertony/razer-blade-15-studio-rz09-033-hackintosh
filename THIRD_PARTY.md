# Third-party components

This repository aggregates configuration and redistributable files from multiple projects. Each component remains subject to its upstream license and copyright.

- [OpenCorePkg](https://github.com/acidanthera/OpenCorePkg)
- [Lilu](https://github.com/acidanthera/Lilu)
- [WhateverGreen](https://github.com/acidanthera/WhateverGreen)
- [VirtualSMC](https://github.com/acidanthera/VirtualSMC)
- [AppleALC](https://github.com/acidanthera/AppleALC)
- [RestrictEvents](https://github.com/acidanthera/RestrictEvents)
- [CPUFriend](https://github.com/acidanthera/CPUFriend)
- [VoodooPS2](https://github.com/acidanthera/VoodooPS2)
- [VoodooI2C](https://github.com/VoodooI2C/VoodooI2C)
- [RealtekCardReader](https://github.com/0xFireWolf/RealtekCardReader)
- [RealtekCardReaderFriend](https://github.com/0xFireWolf/RealtekCardReaderFriend)
- [NoTouchID](https://github.com/al3xtjames/NoTouchID)
- [AMFIPass](https://github.com/dortania/OpenCore-Legacy-Patcher)
- [HibernationFixup](https://github.com/acidanthera/HibernationFixup)
- [CpuTscSync](https://github.com/acidanthera/CpuTscSync)

## V61-P1 custom components

- `RazerOLEDWakeFix` is original exact-target code in this repository and uses the Lilu plug-in API. Its Lilu bootstrap file retains the upstream Acidanthera copyright notice. The implementation was informed by public Linux i915 behavior and hardware traces; no Apple framebuffer binary is included.
- HID V12 modifies VoodooI2C and VoodooI2CHID. The exact upstream commits and reconstructable patches are under `Sources/HID-V12`; those patches and modified binaries are GPLv3. A copy of GPLv3 is in `LICENSES/GPL-3.0.txt`.
- `DisplayModeNudgeV2` and `OLEDWakeRescueAgent` are original user-space utilities under the repository MIT license and use public CoreGraphics, IOKit and Security framework APIs.

Apple-derived binaries listed in the main README are intentionally not redistributed.
