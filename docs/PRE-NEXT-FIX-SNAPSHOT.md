# Pre-next-fix snapshot — 2026-08-11

This branch is a public-safe backup of the OpenCore files that were deployed immediately before the next OLED sleep/wake repair attempt. The private source snapshot was copied read-only from the Windows-mounted EFI System Partition. No EFI file was changed during capture.

## Active display configuration

- Machine: Razer Blade 15 Studio Edition (Early 2020), RZ09-033
- iGPU: Intel UHD Graphics 630, `8086:9BC4`
- Internal display: Samsung `SDCA029`, EDID product `4C83:A029`, 3840×2160 OLED
- SMBIOS: `MacBookPro16,1`
- OpenCore marker: `RazerOLEDWakeFixVersion=41.0-oled-s3-dpcd-state-diagnostic`
- Custom kext: `RazerOLEDWakeFix.kext` v2.8.0, enabled for Darwin 25.2.x
- Diagnostic boot arguments: `amfi=0x80 -lilubetaall igdebug=0x4000180a01248a73`
- WhateverGreen display properties include complete modeset, 4K eDP connector data, maximum-link-rate handling and the alternative backlight-register fix.

`SSDT-OLEDWAK.aml` and `SSDT-RAZER-WAKE.aml` are present on the ESP but are not enabled in the active `ACPI -> Add` list. Their presence must not be mistaken for executed ACPI behavior.

## Observed failure

The panel works after a cold macOS boot. After system sleep, the computer wakes but the internal OLED remains black. AUX/DPCD remains readable and the panel is reported in D0, while eDP link training does not recover.

Observed receiver state supplied from the macOS diagnostic path:

| State | DPCD `0x100` | DPCD `0x200` |
| --- | --- | --- |
| Cold working | `14 84 00 0e 0e 0e 0e` | `02 00 77 77 01 01 66 66` |
| Failed wake | `14 84 00 06 06 06 06` | `02 00 00 00 00 00 22 22` |

The failed wake remains at the `0x06` lane-set phase and never reaches the expected `0x1111` lane status before the later `0x0e`/`0x7777` state.

## Windows comparison

A single Windows test entered traditional S3 and successfully restored the same OLED after a power-button wake. This rules out the idea that Windows only succeeds by avoiding full S3 through Modern Standby. The Windows ETW collection did not expose raw AUX/DPCD/MMIO address-value transactions, so it does not provide a register recipe for macOS.

The large Windows trace archive is intentionally not committed to this repository.

## Public-safety changes

- MLB, ROM, SystemSerialNumber and SystemUUID are replaced with the repository's OpenCore sample placeholders.
- OpenCore password hash and salt are empty.
- Apple-derived Wi-Fi and filesystem binaries remain excluded.
- AppleDouble, Finder metadata, old config backups and old kext backup directories are excluded.

The included custom OLED kext is the deployed binary recovered from the ESP. Its original source tree was not present on the Windows EFI partition and is therefore not represented here as reproducible source code.

## Status

This snapshot is a rollback and audit point, not a confirmed fix. Do not deploy it without restoring private SMBIOS values and the legally obtained omitted binaries described in [RESTORE.md](RESTORE.md).
