# Current 26.7.1 configuration, rebuild and acceptance

[简体中文](MAIN-26.7.1.zh-CN.md)

This is the 2026-10-11 Intel-primary baseline, **not NVIDIA installation**. The previous 26.2 configuration is on [the backup branch](https://github.com/widmonstertony/razer-blade-15-studio-rz09-033-hackintosh/tree/backup/macos-26.2-v61p5-mic22x-20261011). Choose a branch matching your OS. EFI/root-patch backups are not full macOS or personal-data backups; the old branch cannot downgrade an upgraded system.

## Exact target and limits

Only RZ09-0330Q / BIOS 1.06 / i7-10875H / Intel `8086:9bc4` / PCH `8086:068d` / Samsung OLED `4c83:a029` SDCA029 / BCM4360 `14e4:43a0` / ALC298. System: **26.7.1, 25G241, Darwin 25.6.x**, OpenCore 1.0.7. No assurance for other panels, BIOSes, devices or OS builds. Check actual hardware, not just the spoofed MacBookPro16,1 model.

Owner-confirmed after cold start: Wi-Fi, audio, trackpad. OLED 4.7.1 boot reports verified routes and IOSleep stub resolution; R4 is running. New-build **menu/lid sleep/wake, lock, repeated cycles, headphones, Bluetooth and all-app microphone use still need their own acceptance**. The prior 26.2 green-then-redraw wake success is historical evidence only. A clean reinstall has not been rehearsed.

Public kext copies have compiler-path/debug metadata removed with Apple `strip -S`. All non-debug sections and external symbol/address lists were compared and remain identical to the privately boot-qualified binaries. Public hashes consequently differ; the stripped public copies still require your USB boot acceptance. See [binary sanitation evidence](PUBLIC-BINARY-SANITIZATION.json). The physical EFI was not altered by publication.

## 1. Prepare a private USB EFI

1. Back up personal data and the actual complete private ESP to independent storage. Test a recovery USB and direct Windows firmware boot. Public GitHub omits your identity and Windows/Ubuntu boot state.
2. Obtain the exact macOS build legally. Verify the downloaded checkout with `shasum -a 256 -c SHA256SUMS.txt` **before** inserting private identity/dependencies.
3. Review third-party licenses. Double-click `Tools/Prepare-Reinstall.command`, or run:

   ```sh
   python3 -B Tools/Reinstall/reinstall.py prepare --build 25G241 --accept-asset-licenses --with-root-support
   ```

   This only creates a private staging EFI under `.reinstall-private/`, downloading hash-pinned IOSkywalkFamily, IO80211FamilyLegacy/AirPortBrcmNIC, HfsPlus, apfs_aligned and optional root support. It does not write an ESP, install root patches or set identity. Missing/corrupted input refuses. Do not commit this directory.
4. In the staging EFI, replace MLB, ROM, SystemSerialNumber and SystemUUID with your own unique MacBookPro16,1 identity. Existing owners should preserve their own identity. Remove the optional Ubuntu BlessOverride if absent. Do not copy another person's BCD, serial or raw EFI image.
5. Run OpenCore **1.0.7** `ocvalidate` and `plutil -lint` on the staged config. Place it on an independently prepared FAT32 USB and boot from firmware. Never erase/write a disk without verifying the exact target. These preparation tools do not do it for you.

The public main keeps SSDT-DDGPU enabled, Intel properties unchanged, Wi-Fi Add/Block entries paired at 25.6.0–25.6.99 and OLED bounded to 25.6.0–25.6.99. The explicit `-razer-oled25g241-test` flag remains because wake qualification is pending. Unknown CFL fingerprints refuse patches; refusal does not guarantee native wake.

## 2. Restore Wi-Fi and audio roots

Use USB tethering/Ethernet on the first native installation. Exact **KDK 26.7.1 / 25G241** is mandatory; no closest-build substitution. KDK image SHA256: `9045a521c8ae362d4cb2cc54701b03462686e0069518a0b66cfe2028f87dfe9b`.

Pinned **unofficial** patcher source: [225a713f0da5bedadaf8bc32b9b883d1692f9d64](https://github.com/lzhoang2801/OpenCore-Legacy-Patcher/tree/225a713f0da5bedadaf8bc32b9b883d1692f9d64), version 3.0.0, fork support package 1.9.6. Review its `SOURCE.md` and dependencies, use an isolated Python 3.11 environment, and build/run the pinned source. No arbitrary similarly named app or another fork's payload should be substituted. Do not run its OpenCore builder over this custom EFI. Public preparation is **not** an automatic root writer; source building, privilege authorization and actual patch execution remain manual/reviewed steps.

Require a detected plan containing **exactly Modern Wireless and Modern Audio**:

| Set / source | Root operation |
| --- | --- |
| Modern Wireless / `13.7.2-25` | Overwrite `/usr/libexec/wifip2pd`; merge `IO80211.framework` and `WiFiPeerToPeer.framework` under `/System/Library/PrivateFrameworks` |
| Modern Audio / `26.0 Beta 1` | Overwrite `/System/Library/Extensions/AppleHDA.kext` |

Let the reviewed engine perform the exact KDK merge, permissions, KC rebuild and APFS system snapshot. Copying four files is insufficient. Abort on an unexpected patch set, fallback KDK, failed integrity/KC/snapshot or target mismatch. Save logs and patch metadata; it must name 25G241, the exact KDK and commit. The owner's existing upgrade completed these steps successfully; a different clean installation still requires acceptance.

Keep the paired security state: SecureBootModel Disabled, SIP bytes `03080000` (0x803), `amfi=0x80 -lilubetaall`, AMFIPass **disabled**. This reduces security. Do not try the earlier TCC1/AMFIPass experiment, independently remove AMFI arguments, or distribute a TCC database. If running the source from Python requires privilege, authorize reviewed operations normally; do not bypass/replace a privileged helper's signing checks.

Fully shut down and cold-start through USB. Keep MIC22X AppleALC, layout 22 and VerbStub together. Verify real audio recording/output, not just listed devices or a moving meter. Warm restarts previously sometimes left speakers silent; do not call a restart-only result qualified. Give each app its own microphone consent in System Settings; it is not an EFI setting.

## 3. R4 and deliberate OLED tests

Configure the Samsung panel at 1680×945 HiDPI / backing 3360×1890 / 60 Hz, with 1600×900 HiDPI also available. The unchanged R4 agent probes these exact modes. BetterDisplay 10-bit is an optional user-space setting, not an EFI capability switch; verify 30-bit `ARGB2101010` again after mode changes/wake.

Read-only checks:

```sh
python3 -B Tools/Reinstall/reinstall.py audit
zsh Tools/verify-v61p5.sh
ioreg -r -c RazerOLEDWakeFix -l -w0
```

Require version `REL-471-2026-10-11`, `applied-verified-s3-edp14-rate-select-v61`, `all-v60-routes-plus-edp14-rate-selector-call-site-verified` and `verified-single-kc-branch-stub-to-canonical-IOSleep`. A merely listed kext is not success.

After deliberate acceptance of the unqualified new-build wake state:

```sh
zsh Tools/OLEDWakeRescue/install.sh --accept-unvalidated-wake
```

The installer enforces exact OS/runtime gates, verifies binary signatures and probes the display; it does not sleep/reboot and needs no administrator rights. The R4 agent is idle between real wake events and redraws about eight seconds after wake. Do not bring back R5–R9 experiments or auto-restart guards.

Save work, set password immediately after sleep, lock/disable Watch unlock, and deliberately test one Apple-menu Normal Sleep/Wake and a separate physical-lid cycle. Allow the prior green/redraw interval but require an eventual usable desktop, no immediate re-sleep, functioning HID/Wi-Fi/audio and real lock screen. New-build duration/reliability is not promised. Never loop forced sleep/reboot to chase success. Only migrate to the internal ESP after your USB acceptance passes.

## 4. Recovery / future updates

If PCI-only experiments fail, restore the **pre-PCI 26.7.1** config/EFI, not an old 26.2 config onto 26.7.1. Preserve Windows/Ubuntu files. Returning to 26.2 additionally requires its matching OS/root patches or a real system backup/reinstall. Internal-drive-only backups do not cover disk loss or guarantee access after a boot failure.

System updates replace root-patched files. Rebuild/qualify a separate exact OS/KDK/CFL profile; do not just widen Darwin bounds. OLED source and failure-closed checks are in [MultiVersion](../Sources/RazerOLEDWakeFix-MultiVersion/README.md). NVIDIA experiments and their stricter success criteria are [separate](NVIDIA-PCI-ONLY.md).
