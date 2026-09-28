# Razer Blade 15 Studio 2020 Hackintosh — OLED Wake V61-P5 + R4

[简体中文](README.zh-CN.md)

This release is the **currently usable, machine-validated OLED sleep/wake build**, not the older pre-fix snapshot. On the exact test machine, both Apple-menu sleep and physical-lid sleep wake to a visible, locked desktop. The OLED initially shows a solid green framebuffer for about 8–11 seconds; the R4 service then redraws it automatically. V61-P5 also fixes the earlier intermittent failure in which a real lid close was ignored.

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
| Display mode | 1680×945 HiDPI, 3360×1890 backing, 60 Hz; BetterDisplay 4.3.5 override currently reports 30-bit `ARGB2101010` |
| Wake result | Menu and physical-lid Normal Sleep/Wake passed; lock preserved; desktop visible after R4 redraw |
| Native-lid patch | V61-P5 bounded `_WAK`/active `EC0.LID0._LID` correction |

This is an exact-target result, not a universal OLED fix. `RazerOLEDWakeFix.kext` is deliberately restricted to Darwin `25.2.0`–`25.2.99`. Do not remove that guard or use it on another build/panel without source-level revalidation.

## What this release contains

- OpenCore 1.0.7 boot files and a sanitized V61-P5 `config.plist`.
- The compiled P5 `SSDT-SLPWAK.aml`, auditable ASL source, enabled firmware `_WAK → ZWAK` rename and path-scoped active-lid `_LID → XLID` rename.
- `RazerOLEDWakeFix.kext` and its auditable source.
- HID V12 VoodooI2C/VoodooI2CHID binaries and reproducible source patches.
- The stable R4 user-space, event-driven wake rescue service. Eight seconds after a real wake, it uses two session-scoped CoreGraphics transactions to change the exact Samsung panel from 1680×945 HiDPI to 1600×900 HiDPI and restore the original usable mode. It stays idle at all other times and refuses to act on another panel or mode.
- English and Chinese setup, root-patch, verification, thermal and rollback instructions.

The working result uses three cooperating pieces: V61 restores the panel's eDP 1.4 link-training path, P5 keeps native lid state correct across wake, and R4 forces CoreGraphics to redraw the recovered framebuffer. The service does not need administrator privileges.

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
5. Run `Tools/verify-v61p5.sh`, then test Apple-menu sleep and a separate real physical-lid close/open cycle.
6. Expect up to about 12 seconds of green during wake; judge success only after R4 has had time to redraw.
7. Keep the USB and original private EFI until several cold boots, restarts, menu wakes and lid wakes pass.

The full mechanism and the final validation evidence are in [OLED wake design and validation](docs/OLED-WAKE.md). Recovery is in [Rollback](docs/ROLLBACK.md).

## Current practical status

- OLED Normal Sleep/Wake: **working on the exact tested target** with V61 plus R4 automatic redraw.
- Physical lid close/open: **working with P5**; the verified run entered `Clamshell Sleep`, woke from `Normal Sleep`, preserved the lock screen and did not fall back asleep.
- Wake appearance: a solid green framebuffer remains visible for roughly 8–11 seconds before R4 restores the desktop. This is expected in the stable build, not a promise of seamless Apple-like wake.
- Trackpad and touchscreen: working with HID V12.
- Color depth: without a BetterDisplay override, the P5/R4 lid-wake audit reported `24-Bit Color (ARGB8888)`. After enabling 10-bit in BetterDisplay 4.3.5, a new runtime audit reports `30-Bit Color (ARGB2101010)`. This is a user-space display setting, not an EFI/V61 feature; verify it again after every wake or display-mode change.
- Broadcom BCM4360 Wi-Fi: working only with the machine's omitted Apple-derived kexts and exact-build root patches.
- Internal audio: depends on the exact-build root patch used by the tested installation.
- NVIDIA Quadro RTX 5000 Max-Q: disabled in macOS.
- Future macOS updates: unsupported until all binary patches and V61 offsets are revalidated for the new build.

## Reproduction boundary

The guide is written so another owner of the same RZ09-0330Q configuration can reproduce the result, but it is not a one-click universal EFI. Follow the bilingual [installation guide](docs/INSTALL.md) from a USB first. You must supply your own SMBIOS, legally obtained omitted Apple-derived files and exact-build root patches. Do not copy a private EFI, serial number, ROM, Windows BCD or APFS data from this machine.

## Heat and fan behavior

macOS is not automatically cooler on this laptop. The Razer EC/fan policy is designed for Windows, while the scaled 4K HiDPI desktop makes `WindowServer` continuously composite a large framebuffer. See [Thermals and power](docs/THERMALS.md) before changing CPU or fan controls. The OLED rescue agent is event-driven and idle between wake events; it is not a continuous thermal load.

## Source and licensing

Original repository documentation, configuration and custom code are under [MIT](LICENSE). Embedded third-party projects retain their upstream licenses; see [THIRD_PARTY.md](THIRD_PARTY.md). No Apple-derived binary named above is included.

## Disclaimer

Hackintosh configuration is hardware-, firmware- and build-specific. A wrong framebuffer or kernel patch can cause a black screen or an unbootable system. Keep Windows/direct firmware boot and a known-good USB recovery path. This project is not affiliated with Apple, Razer, Intel, NVIDIA or the upstream OpenCore projects.
