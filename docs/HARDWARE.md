# Hardware and firmware notes

## System

- Manufacturer: Razer
- Model: Blade 15 Studio Edition (Early 2020), RZ09-033
- BIOS: 1.06
- CPU: Intel Core i7-10875H
- Memory: 64 GB
- Internal storage: Crucial P3 4 TB NVMe

## Graphics

### Intel UHD Graphics 630

- PCI ID: `8086:9BC4`
- ACPI path reported by Windows: `\_SB.PCI0.GFX0`
- OpenCore path: `PciRoot(0x0)/Pci(0x2,0x0)`
- Configured platform ID: `00009B3E`
- Panel link: 4K eDP with DPCD maximum link rate `0x14`

V61-P5 WhateverGreen/framebuffer properties include:

- `enable-backlight-registers-alternative-fix`
- `enable-dpcd-max-link-rate-fix`
- `enable-max-pixel-clock-override`
- `complete-modeset` and an exact framebuffer mask
- custom framebuffer connector data

The V61 marker and exact-target eDP behavior are provided by `RazerOLEDWakeFix.kext`; see `OLED-WAKE.md`.

### NVIDIA Quadro RTX 5000 Max-Q

- PCI ID: `10DE:1EB5`
- ACPI path: `\_SB.PCI0.PEG0.PEGP`
- Unsupported by modern macOS
- `SSDT-DDGPU.aml` invokes `PEGP._OFF()` during Darwin initialization

## Display

- EDID vendor/product: Samsung `SDCA029`
- Native mode: 3840×2160 at 60 Hz
- Technology: OLED touch display

OLED panels do not have a conventional LCD backlight. Whether macOS can control physical luminance depends on the panel, Intel framebuffer path and firmware implementation. Software dimming remains a fallback but is not equivalent to lowering physical panel drive.

## Audio

- Codec: Realtek ALC298
- Hardware ID: `10EC:0298`
- Layout ID in V61-P5: `47`
- AppleALC and VerbStub are enabled
- `SSDT-ALC298.aml` exists but is disabled

## Networking

- Wi-Fi PCI ID: `14E4:43A0`
- Broadcom BCM4360-class adapter
- V61-P5 uses Apple legacy networking components on Darwin 23 and newer; those Apple-derived binaries are not redistributed in this public repository.

## USB and input

- Custom `USBPorts.kext` personality: `MacBookPro16,1-XHC`
- Internal USB ports are marked connector type `255`
- HID V12 VoodooI2C/VoodooI2CHID and VoodooPS2Controller are present

## Thunderbolt

`SSDT-TB3HP.aml` targets root port `RP13` and is enabled in V61-P5. The final protected menu and physical-lid sleep/wake tests passed with this state.
