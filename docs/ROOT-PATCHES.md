# Root patches, Broadcom Wi-Fi and audio

[简体中文](ROOT-PATCHES.zh-CN.md)

The validated machine uses macOS Tahoe 26.2 build 25C56 plus exact-build root patches for Broadcom BCM4360 Wi-Fi and internal audio. These are separate from the EFI and are the most fragile part of the installation.

## What is and is not in this repository

The config contains enabled references to:

```text
IOSkywalkFamily.kext
IO80211FamilyLegacy.kext
IO80211FamilyLegacy.kext/Contents/PlugIns/AirPortBrcmNIC.kext
```

The binaries are Apple-derived and are not included. The patched System volume is also not included. Obtain required files only through your own macOS installation and a patcher workflow whose licensing you have reviewed.

The tested system used a temporary, unofficial OpenCore-Patcher 3.0.0 build from the [tahoe-patchset project](https://github.com/lzhoang2801/tahoe-patchset). It is not an official guarantee from the [OpenCore Legacy Patcher project](https://github.com/dortania/OpenCore-Legacy-Patcher), and it must not be assumed compatible with another Tahoe build.

## Exact-build workflow

1. Confirm `sw_vers -buildVersion` returns `25C56`.
2. Back up the complete private EFI and macOS data.
3. Install the Kernel Debug Kit matching **exactly** 26.2 / 25C56 from Apple's developer downloads if the patcher requires it.
4. Boot the supplied OpenCore security state: `SecureBootModel=Disabled`, `csr-active-config=0x0803`, and boot arguments `amfi=0x80 -lilubetaall`.
5. Use the reviewed patcher build to apply only the required Modern Wireless and Modern Audio root patch sets.
6. Restart through OpenCore.
7. Verify Wi-Fi association, Internet access, internal speakers, microphone/headphone behavior and one clean restart.
8. Re-run `Tools/verify-v61p5.sh`, then repeat both a menu sleep/wake and a physical-lid close/open test.

Do not run Post-Install Root Patch merely because the button is available. First confirm the OS build, KDK and patchset definitions all match. Keep the patcher's generated log with the private backup.

## Why an update can remove Wi-Fi or audio

macOS system updates rebuild the sealed System volume. Root-patched frameworks and drivers are replaced with Apple's new-build versions, while binary signatures and offsets change. The old patch can therefore disappear or become unsafe even when OpenCore still boots. The EFI's Broadcom entries alone do not make BCM4360 work on Tahoe.

After any supported update, the correct order is:

1. update OpenCore/kext compatibility in a USB test EFI;
2. obtain the exact new macOS installer and matching KDK;
3. port and validate V61 against the new framebuffer binary before enabling it;
4. update macOS;
5. apply root patches built for that exact build;
6. restart and verify Wi-Fi/audio;
7. repeat protected OLED sleep/wake validation.

There is currently no evidence in this repository that V61-P5/R4 is safe on Tahoe 26.6.x. Its Darwin guard is intentional.

## Recovery when Wi-Fi is absent

- Boot with Ethernet, USB tethering or a known-supported USB Wi-Fi device.
- Confirm the two omitted kext bundles exist in the paths referenced by `Kernel > Add`.
- Confirm the exact-build root patch actually completed and that a restart occurred.
- Do not repeatedly install `broadcom-sta-dkms` in Ubuntu for this card as a way to repair macOS; Linux packages do not modify macOS drivers.
- If root patching fails, restore the matching sealed System snapshot or reinstall the same macOS build, then use the reviewed exact-build patcher.
