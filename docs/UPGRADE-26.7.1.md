# 26.7.1 / 25G241 — rehearsal and post-upgrade checklist

<!-- CURRENT-MAIN-NOTICE-20261011 -->

Current main is 26.7.1 / 25G241. The material below preserves earlier 26.2 or pre-port history, not current execution instructions. Follow the [current complete setup/rebuild guide](MAIN-26.7.1.md). New-build Wi-Fi/audio/trackpad and OLED loading/routes passed; new-build sleep and clean reinstall remain pending. Use the backup branch for 26.2.

## Historical record (original versions/status retained)

[简体中文](UPGRADE-26.7.1.zh-CN.md)

**NOT RELEASED / DO NOT UPGRADE THE WORKING SYSTEM YET.** Static OLED checks pass, but no isolated boot/root-KC/wake qualification exists. This document is the execution plan, not a record of successful installation. See [OLED port evidence](../Sources/RazerOLEDWakeFix-MultiVersion/PORT-25G241.md) and [clean reinstall workflow](REINSTALL.md).

## Preconditions

Same audited RZ09-0330Q/BIOS/panel/GPU/PCH/Wi-Fi/audio hardware only. Maintain your unique SMBIOS. Back up System/Data and the actual ESP to independent storage and prove the recovery USB can boot 26.2. An EFI image alone does not back up macOS or restore its root patches.

Use an independent external SSD for the first target installation. If unavailable, a separately approved test volume on the internal APFS container is a less-isolated alternative: it changes the container, shares failure exposure and requires a full independent backup. No tool here creates/erases volumes. An external monitor is useful for rescue but is not a substitute for a test disk.

Freeze legal Apple installer 26.7.1/25G241 and exact matching KDK. Reject nearest-build KDK fallback. Keep all root-patch inputs cached before testing; Internet/Wi-Fi cannot be assumed on the first target boot. No NVIDIA installation, TCC database changes or new security-policy experiment belongs in this test.

## Read-only root input plan

From the repository root:

```sh
python3 -B Tools/Reinstall/reinstall.py plan --build 25G241
python3 -B Tools/Reinstall/reinstall.py verify-inputs --build 25G241 --support-root /path/Universal-Binaries --kdk-root /path/extracted-KDK
```

The pinned fork uses Modern Wireless from 13.7.2-25 and Modern Audio/AppleHDA from 26.0 Beta 1 for XNU 25; that definition has no 25G241-specific runtime qualification. The input auditor verifies exact KDK version/build, seven support executables/shims and the pinned CFL executable. It does not run a patcher or validate every bundle file.

An offline kextlibs repository check found duplicate `_ceil` and `_pow` exports in Libm and DspFuncLib (exit 2). This is a static ambiguity, not proof that a real KC will fail; actual target kmutil and boot remain mandatory. Never fix it by arbitrarily deleting native libraries or plugins.

## Private EFI rehearsal kit

`Tools/Reinstall/prepare_upgrade.py` accepts a complete known-good baseline EFI, a separately built 4.7.0 candidate and ocvalidate 1.0.7. Bare public EFI lacks omitted dependencies and is refused. The tool copies inputs only into a unique private directory, verifies all configs with ocvalidate and audits allowed file differences. It never writes an ESP or changes boot selection.

```sh
python3 -B Tools/Reinstall/prepare_upgrade.py --baseline-efi /path/owner-validated/EFI --candidate-kext /path/build/RazerOLEDWakeFix.kext --ocvalidate /path/ocvalidate --research-only
```

The output is labeled `RESEARCH-NOT-READY.json`; it is not an upgrade authorization.

| EFI | Purpose | Wi-Fi/OLED policy |
| --- | --- | --- |
| rollback | Return to the existing 26.2 system | Byte-identical baseline EFI |
| installer-native | Installer and first target boot | Three Wi-Fi Add entries and matching Skywalk Block capped together at 25.2.99; native target Skywalk; old OLED remains capped |
| oled-isolated-test | After target root patching, approved OLED test | Candidate OLED on 25.6 only with explicit research flag; legacy Wi-Fi still omitted |
| wifi-isolated-test | After OLED/audio pass, controlled Wi-Fi boot | Same OLED; all four Wi-Fi Add/Block entries moved together to 25.6.0–25.6.99 |

Target-only configurations are **not** for booting your old working system. Keep firmware USB/rollback selection available. Preserve Windows/Ubuntu EFI directories and BCD; no automatic internal-ESP merge is provided.

## What to do after the target upgrade/installation

1. First boot the **test target**, not the working 26.2 volume, through installer-native. Confirm `sw_vers` is 26.7.1/25G241. Do not test sleep/close the lid yet. BCM4360 Wi-Fi and AppleHDA audio can be absent at this stage; that is not the final expected result.
2. On the target only, install Apple's exact KDK 26.7.1/25G241. Review and run the pinned source-built root patcher as described in REINSTALL. Require only Modern Wireless and Modern Audio; no OpenCore builder. Abort on missing input, closest-KDK substitution, unexpected patches, failed KC or failed APFS snapshot. Keep logs and require metadata to name the exact target KDK/build/commit.
3. Cold-start the test target with the native/Wi-Fi-capped EFI first. Validate real speaker output, microphone recording, mute/volume and headphones with MIC22X/layout 22/VerbStub. Do not confuse a moving input meter with successful app permission or recorded audio.
4. After native boot/audio pass, owner-authorized OLED rehearsal may boot oled-isolated-test. Require a fully armed candidate, not just a listed kext. The kit includes a separate `R4-isolated-test` copy: its installer requires `--isolated-test`, actual 25G241/Darwin 25.6 and the research boot flag. This wrapper has **not** been installed/tested. Public stable R4's 25C56 guard stays unchanged.
5. Configure the same panel/HiDPI/60 Hz modes, install the research R4 copy **on the isolated target only**, then validate menu sleep, physical-lid sleep, wake, lock screen and repeated cycles. Expect possible initial green frames; require eventual usable desktop without another immediate sleep, forced restart or lost 10-bit mode. Record actual Normal Sleep/Wake, kext diagnostics and R4 completion.
6. Boot wifi-isolated-test only after the above passes. Validate BCM4360 association, DHCP/DNS/Internet, 2.4/5 GHz, transfer stability, Bluetooth coexistence and sleep/reconnect. File hashes cannot guarantee networking/AWDL.
7. Recheck HID, shutdown, warm restart, cold-start audio and microphone access. Give apps their own explicit permission. Do not distribute a TCC database or silently grant other users access. Confirm immediate password lock with Watch locked.
8. Only after functional acceptance: remove the research-only designation through a reviewed qualified release, prepare the production EFI with your private identity, publish sanitized source/bilingual instructions to GitHub, and separately approve internal EFI/system migration. This is not automated by the rehearsal generator.

If a gate fails, return via the rollback USB to 26.2 and retain the target logs. Do not replace a failing root patch with a security setting change, forced sleep loop or automatic reboot.
