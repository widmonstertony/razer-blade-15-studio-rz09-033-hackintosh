# Changelog

## OLED Wake V61-P1 Performance — 2026-08-24

- Updated the tested boot stack to OpenCore 1.0.7.
- Added `RazerOLEDWakeFix.kext` 4.5.0 (V61) with Darwin 25.2.x exact-build guards and published its source.
- Added the event-driven OLED wake rescue agent and protected CoreGraphics redraw helper, with build/install/uninstall scripts.
- Recorded a successful real Normal Sleep/Wake with automatic desktop redraw and retained 30-bit output.
- Updated Lilu to 1.7.2, AppleALC to 1.9.7 and VoodooPS2Controller to 2.3.7.
- Added the matched HID V12 VoodooI2C/VoodooI2CHID binaries and GPLv3 source patches.
- Removed the obsolete `agdpmod=vit9696` boot argument and retained the performance boot arguments `amfi=0x80 -lilubetaall`.
- Switched the OLED framebuffer property to `enable-backlight-registers-alternative-fix`, added the validated complete-modeset properties and retained 10-bit output.
- Enabled OpenCore launcher protection and the optional Ubuntu `shimx64.efi` BlessOverride used by the validated triple-boot machine.
- Re-sanitized SMBIOS identity and removed OpenCore password material from the public config.
- Added complete English and Simplified Chinese installation, root-patch, OLED design, thermal and rollback documentation.

## Baseline — 2026-08-07

- Captured the pre-fix EFI configuration.
- Replaced unique SMBIOS identifiers with OpenCore sample placeholders.
- Removed Apple-derived Wi-Fi and filesystem binaries from the public tree.
- Removed macOS AppleDouble and Finder metadata.
- Added iasl-decompiled ACPI audit sources.
- Documented hardware, component versions, known OLED/wake issues and recovery steps.
