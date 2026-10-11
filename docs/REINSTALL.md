# Reinstall from this repository — dependencies, root patches and acceptance

<!-- CURRENT-MAIN-NOTICE-20261011 -->

Current main is 26.7.1 / 25G241. The material below preserves earlier 26.2 or pre-port history, not current execution instructions. Follow the [current complete setup/rebuild guide](MAIN-26.7.1.md). New-build Wi-Fi/audio/trackpad and OLED loading/routes passed; new-build sleep and clean reinstall remain pending. Use the backup branch for 26.2.

## Historical record (original versions/status retained)

[简体中文](REINSTALL.zh-CN.md)

Goal: an owner of the same hardware can rebuild the configuration without Tony's old EFI or private system. **Status: dependency preparation and read-only audits implemented; a complete clean reinstall has not been tested.** The existing 26.2 installation is owner-validated. These are different claims.

## Supported target

Only RZ09-0330Q / BIOS 1.06 / i7-10875H / physical Intel 8086:9bc4 / PCH 8086:068d / Samsung OLED 4c83:a029 (SDCA029) / BCM4360 14e4:43a0 / ALC298, on **macOS 26.2 build 25C56**. Spoofed MacBookPro16,1 is not proof of the physical hardware. Check the actual laptop, BIOS, panel and devices before using its ACPI/kernel patches.

Another identical laptop must use its own SMBIOS and user permissions. A different panel, BIOS, GPU, Wi-Fi card, audio codec or laptop needs a separate audited hardware profile. This is not a universal laptop installer. **26.7.1 / 25G241 is explicitly blocked** until OLED, private object layouts, root patches and R4 are independently qualified.

## 1. Recoverability and installation

Back up your data and private ESP to independent storage. Prepare a known-good USB boot path and test firmware boot selection. Obtain macOS legally from Apple and install the exact supported build. Do not erase disks or write an ESP using the preparation tool: it never does either. Follow [INSTALL](INSTALL.md) for USB-first boot.

## 2. Prepare omitted dependencies without an old EFI

Review [third-party attribution](../THIRD_PARTY.md), the source projects and licenses. Install Python 3 if it is absent. Double-click `Tools/Prepare-Reinstall.command` (or invoke it with zsh). It asks before fetching inputs, checks every download against the pinned SHA256, verifies the public checkout, and creates a unique **private staging directory** under `.reinstall-private/`.

Command-line equivalent, from the repository root:

```sh
python3 -B Tools/Reinstall/reinstall.py prepare --build 25C56 --accept-asset-licenses
```

It fills the staged EFI's IOSkywalkFamily 1.2.0, IO80211FamilyLegacy 1.0.0 with AirPortBrcmNIC, HfsPlus and apfs_aligned. Sources are fixed commits, not `latest`/`master`; Wi-Fi Mach-O binaries were compared with the working private backup. No Apple-derived inputs are committed here. The tool does not write the internal/USB ESP, configure personal identity, run a patcher, change TCC, or reboot.

To also cache the matching fork's 948,923,392-byte PatcherSupportPkg 1.9.6 image:

```sh
python3 -B Tools/Reinstall/reinstall.py prepare --build 25C56 --accept-asset-licenses --with-root-support
```

Once all assets are cached, add `--offline` to prepare another staging directory without networking. A missing/corrupted input fails closed; it is not silently replaced. Keep `.reinstall-private` private even if you later fill in identity. A SHA match confirms bytes, not the trustworthiness/licensing of the upstream source.

The staged config **still has public SMBIOS placeholders**. Generate your own identity using GenSMBIOS, remove the optional Ubuntu override if absent, run ocvalidate 1.0.7, and boot from USB. Do not treat `assetsSha256Verified` as `bootQualified`.

## 3. Exact root-patch recipe

Pinned definition source: [unofficial Tahoe fork commit 225a713](https://github.com/lzhoang2801/OpenCore-Legacy-Patcher/tree/225a713f0da5bedadaf8bc32b9b883d1692f9d64). Patcher version 3.0.0; fork PatcherSupportPkg **1.9.6**; KDK **26.2 / 25C56** from Apple. Do not replace the fork support image with an identically numbered image from another project: compare the manifest hash.

Build/run the pinned source following its `SOURCE.md`, with a separate Python 3.11 virtual environment and reviewed dependencies. Do not overwrite your custom EFI with the patcher's OpenCore builder. An arbitrary download called `OpenCore-Patcher.app` is not sufficient. Check commit and payloads; do not bypass a failed integrity/signature check. The previously archived local app has an invalid Info.plist signature and is **not** shipped or automatically executed by this workflow.

Before using Post-Install Root Patch, verify all of the following:

- `sw_vers -productVersion` is `26.2`, build is `25C56`; the installed KDK's SystemVersion.plist has the same build.
- Boot policy matches the published paired snapshot: SecureBootModel Disabled; SIP data `03080000` (0x803); `amfi=0x80 -lilubetaall`; AMFIPass disabled. This reduced-security policy is a documented compatibility compromise. Do not independently flip AMFI/TCC settings.
- The detected plan contains **only Modern Wireless and Modern Audio**, not unrelated GPU patches or another Mac's patches.

Required file operations:

| Patch | Source selector | Operation / target |
| --- | --- | --- |
| Modern Wireless | 13.7.2-25 | Overwrite `/usr/libexec/wifip2pd` |
| Modern Wireless | 13.7.2-25 | Merge `IO80211.framework` and `WiFiPeerToPeer.framework` into `/System/Library/PrivateFrameworks` |
| Modern Audio | 26.0 Beta 1 | Overwrite `/System/Library/Extensions/AppleHDA.kext` |

Let the reviewed patcher handle KDK merge, ownership/signatures, kernel collections and APFS snapshots. Simply copying those files is not an installation. Save the entire patch log. Stop on any build mismatch, unexpected patch set, failed cache/snapshot step or missing input; do not keep retrying blindly.

**No automatic root-write command is released yet.** A fresh install's detection, patcher execution and recovery still require an isolated rehearsal before the project can claim end-to-end unattended reproduction. The preparation tool is intentionally not a root patcher.

## 4. Reboot, R4, audio and permissions

Keep matched MIC22X AppleALC and VerbStub with layout 22. After root patching, fully shut down and cold-start through the USB EFI; a warm restart alone previously left output silent. Follow [CURRENT-SNAPSHOT](CURRENT-SNAPSHOT.md) for real speaker/microphone tests.

Install the unchanged R4 service with `zsh Tools/OLEDWakeRescue/install.sh`. Configure the exact HiDPI modes before wake tests. BetterDisplay 10-bit is a separate optional user setting; the repository does not distribute it or force it automatically.

For each application, explicitly grant microphone permission to that user/app. No TCC database, personal app entitlement or private recording is distributed. If the app never prompts, diagnose it separately; do not change EFI. Require a password immediately after sleep/lock via System Settings, and verify with Watch unlock disabled/Watch locked.

## 5. Acceptance is functional, not just files

```sh
python3 -B Tools/Reinstall/reinstall.py audit
zsh Tools/verify-v61p5.sh
```

The first command compares four root executables and patch metadata without changing anything; it does not validate complete bundle integrity or actual recording/networking. Then manually test Wi-Fi association and Internet, speakers, microphone input/recording, headphones, trackpad/touchscreen, cold boot, warm restart, menu sleep, physical-lid sleep, wake and password lock. Expect the stable 8–11-second green interval followed by R4 recovery. Do not automatically sleep/restart a user's computer.

Only after USB tests pass may the owner deliberately migrate the prepared EFI to the internal ESP. Keep the recovery USB. [ROLLBACK](ROLLBACK.md) covers failure handling. System updates can remove root patches; use an explicitly qualified new-build profile, never the old plan with only a version check removed.

For the pending 26.7.1 target, use the separate [isolated upgrade/post-upgrade checklist](UPGRADE-26.7.1.md). Read-only root plans/input audits and private research-EFI generation are implemented, but no end-to-end new-build or clean-install success is claimed.
