# Razer Blade 15 Studio 2020 (RZ09-033) Hackintosh EFI

> Public, sanitized snapshot of the **pre-fix** OpenCore configuration from a Razer Blade 15 Studio Edition (Early 2020). This baseline was captured on 2026-08-07 before changing OLED brightness or sleep/wake behavior.

这是 Razer Blade 15 Studio Edition 2020（RZ09-033）的 OpenCore EFI 配置备份。仓库首先保存修改前基线；OLED 亮度与睡眠唤醒修复会在本机测试通过后另行提交，方便比较和回滚。

## Important warning / 重要警告

This repository is intentionally **not drop-in bootable**:

- SMBIOS identifiers were replaced with OpenCore sample placeholders.
- Apple-derived Wi-Fi and filesystem binaries are not redistributed.
- The baseline keeps references to omitted files so it accurately documents the running configuration.

Do not copy this repository directly to an EFI System Partition. Generate unique SMBIOS values and restore the omitted files from your own legally obtained macOS/OpenCore installation first.

不要直接把仓库内容覆盖到 EFI 分区。使用前必须生成自己的 SMBIOS，并从个人安装环境恢复未公开的 Apple 二进制。

## Hardware

| Component | Model |
| --- | --- |
| Laptop | Razer Blade 15 Studio Edition (Early 2020), RZ09-033 |
| BIOS | Razer 1.06 |
| CPU | Intel Core i7-10875H, 8 cores / 16 threads |
| iGPU | Intel UHD Graphics 630, PCI `8086:9BC4` |
| dGPU | NVIDIA Quadro RTX 5000 Max-Q, PCI `10DE:1EB5` — disabled in macOS |
| Internal panel | Samsung `SDCA029`, 3840×2160 OLED touch display |
| Audio | Realtek ALC298 (`10EC:0298`) |
| Wi-Fi | Broadcom BCM4360-class adapter, PCI `14E4:43A0` |
| Storage | Crucial P3 4 TB NVMe (`CT4000P3SSD8`) |
| Memory | 64 GB DDR4 |
| SMBIOS profile | `MacBookPro16,1` |

See [docs/HARDWARE.md](docs/HARDWARE.md) for device paths and configuration notes.

## Repository state

The `main` branch begins with the configuration exactly as it was found before the OLED/wake work, except for public-safety redactions and redistribution exclusions.

The `agent/pre-next-fix-snapshot` branch records the sanitized configuration captured on 2026-08-11 immediately before the next repair attempt. It preserves the currently deployed `RazerOLEDWakeFix.kext` v2.8.0 binary and the `41.0-oled-s3-dpcd-state-diagnostic` configuration marker. See [docs/PRE-NEXT-FIX-SNAPSHOT.md](docs/PRE-NEXT-FIX-SNAPSHOT.md).

Current known issues:

1. Native OLED brightness adjustment does not affect the panel as expected.
2. After display/system sleep, the internal OLED can remain black after wake.
3. `SSDT-SLPWAK.aml` and its `_WAK` → `ZWAK` rename are present but disabled in the baseline.
4. The baseline uses the legacy `enable-backlight-registers-fix`, which is obsolete on macOS 13.4 and newer.

The planned test patch is documented in [docs/KNOWN-ISSUES.md](docs/KNOWN-ISSUES.md). It is deliberately not part of this baseline commit.

## Configured subsystems

- Intel UHD 630 framebuffer and 4K eDP link patches via WhateverGreen
- NVIDIA dGPU power-off through `SSDT-DDGPU.aml`
- ALC298 audio via AppleALC plus VerbStub
- Battery, ambient-light sensor and SMC monitoring
- I2C HID trackpad/touch support and PS/2 keyboard support
- Custom USB map for `MacBookPro16,1`
- Realtek card reader support
- Broadcom Wi-Fi compatibility path for Darwin 23+
- Thunderbolt hot-plug ACPI configuration

“Configured” does not mean every item has been re-tested on every macOS release.

## Layout

```text
EFI/
  BOOT/
  OC/
    ACPI/
    Drivers/
    Kexts/
    Resources/
    Tools/
    config.plist
ACPI-Sources/       # iasl-decompiled audit copies
docs/
```

The files under `ACPI-Sources/` are reconstructed by Intel iasl from the AML binaries. They are provided for review and are not guaranteed to match the original author’s formatting or comments.

## Public redactions

The following values are replaced with the placeholders used by OpenCore `Sample.plist`:

- `PlatformInfo -> Generic -> MLB`
- `PlatformInfo -> Generic -> ROM`
- `PlatformInfo -> Generic -> SystemSerialNumber`
- `PlatformInfo -> Generic -> SystemUUID`
- OpenCore password hash and salt are forced empty

Never publish real MLB, ROM, serial or UUID values. Generate a new identity with [GenSMBIOS](https://github.com/corpnewt/GenSMBIOS) before use.

## Files not redistributed

The running EFI contains several Apple-derived files that are intentionally absent here:

- `EFI/OC/Kexts/IOSkywalkFamily.kext`
- `EFI/OC/Kexts/IO80211FamilyLegacy.kext`
- `EFI/OC/Drivers/HfsPlus.efi`
- `EFI/OC/Drivers/apfs_aligned.efi`

Obtain required Apple components from your own macOS installation or the documented workflow of [OpenCore Legacy Patcher](https://github.com/dortania/OpenCore-Legacy-Patcher). OpenCore can use the open-source `OpenHfsPlus.efi` where appropriate.

## Validation

- Public config parses as an XML property list.
- Original SMBIOS identifiers were scanned and are absent from the public tree.
- The baseline config validates with OpenCore `ocvalidate` 1.0.6.
- All included AML files were successfully disassembled with Intel iasl 20260408.

## Restore and use

See [docs/RESTORE.md](docs/RESTORE.md). Keep a bootable USB EFI and a copy of the original private `config.plist` before testing changes.

## Third-party projects

This is an aggregation/configuration repository. Included third-party components remain under their upstream licenses. See [THIRD_PARTY.md](THIRD_PARTY.md).

## Disclaimer

Hackintosh configurations are hardware- and firmware-specific. Test from removable media first. No warranty is provided, and this repository is not affiliated with Apple, Razer, Intel, NVIDIA or the OpenCore developers.
