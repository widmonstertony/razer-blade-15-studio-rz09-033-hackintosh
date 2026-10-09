# Upgrade and NVIDIA assessment — 2026-10-09

[简体中文](UPGRADE-NVIDIA.zh-CN.md)

This is a research/acceptance plan, **not permission to update a working installation**. The published EFI still targets 26.2 / 25C56. No macOS installer or NVIDIA payload is shipped.

## Latest relevant macOS

Apple lists macOS **27.0.1** as the latest release, but macOS 27 requires Apple silicon. This Intel i7-10875H cannot use that native OS by changing SMBIOS. The relevant candidate is **Tahoe 26.7.1**, not 27.2. Sources: [Apple security releases](https://support.apple.com/en-us/100100), [macOS 27 compatibility](https://support.apple.com/en-us/127455).

The machine's `softwareupdate --list-full-installers` offered 26.7.1 **25G241**, approximately 17.1 GiB. A [matching KDK](https://github.com/dortania/KdkSupportPkg/releases/tag/25G241) is published. Its DMG's published SHA-256 is `9045a521c8ae362d4cb2cc54701b03462686e0069518a0b66cfe2028f87dfe9b`; verify an actual download before use. Availability is not proof that this Hackintosh works on it. The existing older local installer and a 25G82 OLED candidate are not substitutes.

### What still blocks a direct upgrade

- V61 is a build-specific Intel framebuffer binary patch, restricted to Darwin 25.2.x. Revalidate original target bytes, unique matches, offsets/call targets and runtime training. Merely broadening MinKernel/MaxKernel is unsafe.
- The freshly exported Broadcom entries have MinKernel 23.0.0 and no MaxKernel, unlike the earlier restrictive candidate; this does not prove target compatibility. Their binaries and Modern Wireless root patches must be verified against the target. An OS update rebuilds the System volume; currently working BCM4360 networking can disappear.
- AppleHDA/Modern Audio root patches and the MIC22X pairing need target-build boot and functional audio tests.
- The current unofficial patchset/signature policy must be evaluated as a complete pair. Do not repeat the failed AMFI-only/TCC1 migration.
- [Official OCLP 3.0.0-rc.3](https://github.com/dortania/OpenCore-Legacy-Patcher/releases/tag/3.0.0-rc.3) is a prerelease, not the latest stable release. [CustoMac v3.0.3](https://github.com/kgp-macPro/OCLP-CustoMac/releases/tag/v3.0.3) is a third-party fork whose stated physical qualification reaches 26.6.2, not this laptop on 26.7.1. Do not let either replace this machine's custom OpenCore configuration automatically.

### Recommended order

1. Preserve the exact private ESP image and a restorable System/Data backup; prove firmware/recovery USB boot. An EFI image alone cannot roll back a macOS upgrade.
2. Stage the exact 25G241 installer, KDK and reviewed wireless/audio payloads offline. Prepare USB Ethernet or supported phone networking; do not promise upgraded Wi-Fi before testing.
3. Port OLED against the exact target framebuffer and make a separately validated USB EFI. Leave the current internal EFI intact.
4. With the owner's separate approval, install into an independent APFS test volume or external SSD, keeping working 26.2 untouched. Partitioning/volume creation is not performed by this guide.
5. Test cold/warm boots, Wi-Fi reconnection, speakers/microphone and app permissions, HID, 10-bit output, password lock, menu sleep and real lid sleep/wake, and Windows/Ubuntu recovery boots. Record actual pass/fail; driver enumeration alone is insufficient.
6. Only after acceptance, authorize migration of the main system. Keep NVIDIA testing separate from the OS update.

No success percentage or zero-risk guarantee is established. Security improvements in later releases are a reason to prepare migration, not a reason to bypass recovery and build validation.

## NVIDIA: Intel main display plus optional compute

The retained Linux capture identifies the laptop GPU as TU104GLM Quadro RTX 5000 Mobile/Max-Q **10de:1eb5**, subsystem **1a58:200a**. The [nullmoth project](https://github.com/nullmoth/nvidia-macos-driver) device table includes 1EB5. A PCI-ID match does not prove initialization, laptop routing, compute or sleep compatibility.

Reviewed source: `b9a5a2b35c6475b1df0e7438916df25ad5c6d1d8`. [Application release 1.1.1](https://github.com/nullmoth/nvidia-macos-driver/releases) says the bundled driver remains 1.1.0. The author's substantial physical validation is RTX 5060 on macOS 15.8.1; [Turing qualification is pending](https://github.com/nullmoth/nvidia-macos-driver/blob/main/docs/CARD-SUPPORT.md), as are Tahoe and this mixed-GPU laptop configuration. The project does not implement an Optimus display mux. Its Metal/OpenCL/Vulkan paths are not a CUDA implementation.

Therefore, **Intel-primary plus callable NVIDIA is an experimental goal, not an available guaranteed setup**. Keep NVIDIA disabled in this working EFI. A justified experiment would use a separately approved Sequoia 15.8.1 test system and USB EFI, first validate GPU power/PCI/BAR/GSP, then Metal enumeration and a small compute task while preserving Intel OLED and an immediate recovery route. Enabling the dGPU may also affect power, fan speed, sleep and external display routing; these require measurement rather than assumptions.

Do not load experimental NVIDIA kexts into the working main system just because the card appears in a supported-device JSON file. No NVIDIA installer, firmware or driver binary is added to this release.
