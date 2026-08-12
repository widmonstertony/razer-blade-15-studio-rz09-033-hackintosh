# Changelog

## Pre-next-fix snapshot — 2026-08-11

- Added the sanitized current EFI on branch `agent/pre-next-fix-snapshot` without changing the original `main` baseline.
- Preserved the deployed `RazerOLEDWakeFix.kext` v2.8.0 binary and its active OpenCore entry.
- Recorded configuration marker `41.0-oled-s3-dpcd-state-diagnostic` and the active diagnostic boot arguments.
- Added `SSDT-OLEDWAK.aml` and `SSDT-RAZER-WAKE.aml` as files present on the ESP; neither is enabled by the current `ACPI -> Add` list.
- Updated VoodooI2C/VoodooI2CHID to match the currently deployed copies.
- Re-applied public SMBIOS redactions and verified that the real identifiers are absent from the public tree.
- Continued to omit Apple-derived Wi-Fi and filesystem binaries.

## Baseline — 2026-08-07

- Captured the pre-fix EFI configuration.
- Replaced unique SMBIOS identifiers with OpenCore sample placeholders.
- Removed Apple-derived Wi-Fi and filesystem binaries from the public tree.
- Removed macOS AppleDouble and Finder metadata.
- Added iasl-decompiled ACPI audit sources.
- Documented hardware, component versions, known OLED/wake issues and recovery steps.
