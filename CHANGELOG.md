# Changelog

## OLED Wake V61-P5 Native Lid + R4 — 2026-09-27

- Enabled the P5 `SSDT-SLPWAK` path and published its auditable ASL/AML.
- Added a DSDT-only, path-scoped rename of the active `\_SB.PCI0.LPCB.EC0.LID0._LID` method to `XLID`; unrelated firmware lid objects are untouched.
- Bounded the temporary Darwin “lid open” override to the synchronous firmware `ZWAK` call and cleared it unconditionally before `_WAK` returns. This prevents a stale wake flag from swallowing the next real physical lid close.
- Replaced the original app-only redraw helper with the validated R4 helper: two session-scoped CoreGraphics transactions, restoration to the usable 1680×945 mode, and no global ColorSync reset.
- Verified a real physical close as `Clamshell Sleep`, followed by `Normal Sleep` wake, preserved lock state, successful R4 redraw and no second sleep or WindowServer diagnostic.
- Documented the stable limitation: the panel still shows a solid green framebuffer for roughly 8–11 seconds before R4 redraws it. R5–R9 masking experiments are intentionally not shipped because they could report success while the physical OLED remained green.
- Renamed and expanded the verifier to `Tools/verify-v61p5.sh`, including the paired ACPI configuration, lid runtime state and exact R4 binary checks.
- Reworked the English and Simplified Chinese guides for reproducible USB-first setup, physical-lid testing and independent P5/R4 rollback.

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
