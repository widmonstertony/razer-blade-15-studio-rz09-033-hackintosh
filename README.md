# Razer Blade 15 Studio 2020 Hackintosh — OLED Wake V61-P1

[简体中文](README.zh-CN.md)

This release is the **validated OLED sleep/wake build**, not the older pre-fix snapshot. On the exact test machine, a real Normal Sleep/Wake cycle returned to a visible 10-bit desktop and passed the V61-P1 runtime checks.

## Validated target

| Item | Exact tested value |
| --- | --- |
| Laptop | Razer Blade 15 Studio Edition 2020, RZ09-0330Q, BIOS 1.06 |
| CPU / iGPU | Intel Core i7-10875H / UHD 630 `8086:9BC4` |
| Internal panel | Samsung OLED `4C83:A029`, EDID name `SDCA029` |
| macOS | Tahoe 26.2, build `25C56`, Darwin `25.2.0` |
| OpenCore | 1.0.7 |
| OLED patch | V61, kext version 4.5.0, marker `61.0-oled-s3-edp14-rate-select` |
| Input patch | HID V12 |
| Display mode | 1680×945 HiDPI, 3360×1890 backing, 60 Hz, 30-bit framebuffer |
| Wake result | Normal Sleep/Wake passed; desktop visible after automatic protected redraw |

This is an exact-target result, not a universal OLED fix. `RazerOLEDWakeFix.kext` is deliberately restricted to Darwin `25.2.0`–`25.2.99`. Do not remove that guard or use it on another build/panel without source-level revalidation.

## What this release contains

- OpenCore 1.0.7 boot files and a sanitized V61-P1 `config.plist`.
- `RazerOLEDWakeFix.kext` and its auditable source.
- HID V12 VoodooI2C/VoodooI2CHID binaries and reproducible source patches.
- A user-space, event-driven wake rescue service. Eight seconds after a real wake, it briefly changes the exact Samsung panel from 1680×945 HiDPI to 1600×900 HiDPI and restores the original mode. It stays idle at all other times and refuses to act on another panel or mode.
- English and Chinese setup, root-patch, verification, thermal and rollback instructions.

The working result uses both layers: the V61 kernel patch restores the panel's eDP 1.4 link-training path, and the user-space display transaction forces CoreGraphics to redraw the recovered framebuffer. The service does not need administrator privileges.

## Important: not drop-in bootable

The public tree is intentionally sanitized and incomplete:

- MLB, ROM, serial number and UUID are OpenCore sample placeholders.
- OpenCore password hash and salt are empty.
- Apple-derived `IOSkywalkFamily.kext`, `IO80211FamilyLegacy.kext`, `HfsPlus.efi` and `apfs_aligned.efi` are not redistributed.
- Wi-Fi and internal-audio root patches must be produced from your own legally obtained installation.

Never overwrite the internal EFI first. Prepare a FAT32 USB test EFI, restore the omitted files from your own machine, insert a unique SMBIOS, run `ocvalidate`, and prove boot/input/wake before touching the internal ESP.

## Start here

1. Read [Installation](docs/INSTALL.md) end to end.
2. Read [Root patches and Wi-Fi](docs/ROOT-PATCHES.md) before changing macOS.
3. Prepare and boot the USB EFI.
4. Install the wake rescue with `Tools/OLEDWakeRescue/install.sh`.
5. Run `Tools/verify-v61p1.sh`, then perform one controlled sleep/wake test.
6. Keep the USB and original private EFI until several cold boots, restarts and wakes pass.

The full mechanism and the final validation evidence are in [OLED wake design and validation](docs/OLED-WAKE.md). Recovery is in [Rollback](docs/ROLLBACK.md).

## Current practical status

- OLED Normal Sleep/Wake: **working on the exact tested target** with V61 plus the automatic redraw service.
- Trackpad and touchscreen: working with HID V12.
- 10-bit output: retained; the post-wake test reported `Framebuffer Depth: 30-Bit Color (ARGB2101010)`.
- Broadcom BCM4360 Wi-Fi: working only with the machine's omitted Apple-derived kexts and exact-build root patches.
- Internal audio: depends on the exact-build root patch used by the tested installation.
- NVIDIA Quadro RTX 5000 Max-Q: disabled in macOS.
- Future macOS updates: unsupported until all binary patches and V61 offsets are revalidated for the new build.

## Heat and fan behavior

macOS is not automatically cooler on this laptop. The Razer EC/fan policy is designed for Windows, while a 4K 10-bit HiDPI desktop makes `WindowServer` continuously composite a large framebuffer. See [Thermals and power](docs/THERMALS.md) before changing CPU or fan controls. The OLED rescue agent is event-driven and idle between wake events; it is not a continuous thermal load.

## Source and licensing

Original repository documentation, configuration and custom code are under [MIT](LICENSE). Embedded third-party projects retain their upstream licenses; see [THIRD_PARTY.md](THIRD_PARTY.md). No Apple-derived binary named above is included.

## Disclaimer

Hackintosh configuration is hardware-, firmware- and build-specific. A wrong framebuffer or kernel patch can cause a black screen or an unbootable system. Keep Windows/direct firmware boot and a known-good USB recovery path. This project is not affiliated with Apple, Razer, Intel, NVIDIA or the upstream OpenCore projects.
