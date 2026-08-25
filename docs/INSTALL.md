# Installation — V61-P1 OLED wake build

[简体中文](INSTALL.zh-CN.md)

Read this entire page before writing an EFI partition. This procedure is intentionally conservative because the public configuration is sanitized and omits files that cannot be redistributed.

## 1. Confirm the exact target

Run in macOS:

```sh
sw_vers
uname -r
system_profiler SPDisplaysDataType
ioreg -lw0 | grep -E 'DisplayVendorID|DisplayProductID'
```

Proceed only when the machine is the RZ09-0330Q with BIOS 1.06, macOS 26.2 build 25C56 / Darwin 25.2.x, UHD 630 `8086:9BC4`, and Samsung panel `4C83:A029`. The install script also enforces the OS build and the display helper enforces the panel and exact display modes.

## 2. Make two recoverable backups

1. Mount the internal EFI System Partition (ESP).
2. Copy the entire private `EFI` directory to two different storage devices.
3. Keep one known-working FAT32 USB EFI connected during the first tests.
4. Confirm the firmware boot menu can start Windows directly.

Do not publish either private backup. It contains the machine identity and may contain Windows BCD data.

## 3. Prepare a USB test ESP

Use an empty USB drive. In Disk Utility choose **View > Show All Devices**, select the physical USB device, and erase it as **MS-DOS (FAT)** with **GUID Partition Map**. Verify the selected disk twice; erasing destroys its contents.

Copy this repository's `EFI` directory to the root of the USB volume. Do not write the internal ESP yet.

## 4. Restore the deliberately omitted files

Copy these files from your own legally obtained, currently working private EFI into the same paths on the USB:

```text
EFI/OC/Kexts/IOSkywalkFamily.kext
EFI/OC/Kexts/IO80211FamilyLegacy.kext
EFI/OC/Drivers/HfsPlus.efi
EFI/OC/Drivers/apfs_aligned.efi
```

The public config keeps references to them so the tested ordering is visible. If you intentionally use a different legal replacement, update `UEFI > Drivers` and `Kernel > Add` accordingly; never leave an enabled entry whose file is absent.

## 5. Generate a unique SMBIOS

Generate a unique `MacBookPro16,1` identity with [GenSMBIOS](https://github.com/corpnewt/GenSMBIOS). Replace all four public placeholders in the USB config:

```text
PlatformInfo > Generic > MLB
PlatformInfo > Generic > ROM
PlatformInfo > Generic > SystemSerialNumber
PlatformInfo > Generic > SystemUUID
```

Never reuse values shown online or values from another Mac. If updating an existing working installation, transplant the four values from your private config into the USB config locally and never commit the result.

## 6. Handle the optional Ubuntu entry

`Misc > BlessOverride` contains `\EFI\ubuntu\shimx64.efi` because the validated machine dual-boots Ubuntu. Keep it only if that file exists on your system. Otherwise remove that one array entry; it is not required for macOS or OLED wake.

## 7. Validate before booting

Use the `ocvalidate` binary from OpenCore 1.0.7:

```sh
/path/to/OpenCore-1.0.7/Utilities/ocvalidate/ocvalidate /Volumes/EFI/EFI/OC/config.plist
plutil -lint /Volumes/EFI/EFI/OC/config.plist
```

Both commands must pass. Also confirm that there are no `.before-*`, `FSCK*`, `._*` or `.DS_Store` files in the USB EFI.

To verify the checkout itself before adding private files:

```sh
shasum -a 256 -c SHA256SUMS.txt
```

Run this from the repository root. The command will naturally stop matching after you add your private SMBIOS and omitted files; keep those changes outside Git.

## 8. Boot the USB and perform static checks

Select the USB OpenCore entry from firmware. Do not set it as permanent default yet. After macOS login, confirm:

- keyboard, trackpad and touchscreen work;
- UHD 630 acceleration and the 1680×945 HiDPI mode work;
- Wi-Fi and internal audio work if their exact-build root patches are present;
- the NVIDIA GPU is not listed as an active macOS display device.

From the repository checkout, run:

```sh
chmod +x Tools/verify-v61p1.sh Tools/OLEDWakeRescue/*.sh
./Tools/OLEDWakeRescue/install.sh
./Tools/verify-v61p1.sh
```

`install.sh` requires no administrator password. It installs two user-owned binaries under `~/Library/Application Support/RazerOLEDWakeRescue` and one LaunchAgent under `~/Library/LaunchAgents`.

## 9. Controlled sleep/wake test

1. Save all work and disconnect nonessential USB/Thunderbolt devices.
2. Keep AC power connected and leave the recovery USB available.
3. Choose **Apple menu > Sleep**; do not use display-only sleep for this proof.
4. Wait at least 30 seconds.
5. Wake once with the physical power button.
6. Allow at least 12 seconds after wake. A black or green framebuffer may be visible before the automatic redraw runs at eight seconds.
7. Confirm the desktop is visible and responsive, then inspect:

```sh
tail -n 80 "$HOME/Library/Logs/RazerOLEDWakeRescue.log"
system_profiler SPDisplaysDataType | grep -E 'Framebuffer Depth|Resolution|Looks like'
pmset -g log | tail -n 80
```

Expected rescue log: `POWER_EVENT`, one or more `WAKE_RESCUE_ATTEMPT` lines, and `WAKE_RESCUE_PASS`. The tested 10-bit result reports `Framebuffer Depth: 30-Bit Color (ARGB2101010)`.

If the screen does not recover, do not install this EFI internally. Boot the known-good USB and follow [Rollback](ROLLBACK.md).

## 10. Move to the internal ESP only after proof

After successful USB boot, input, Wi-Fi/audio as applicable, restart, cold boot and sleep/wake tests:

1. Mount the internal ESP.
2. Rename its current private `EFI` directory to a dated backup on another disk.
3. Copy the fully prepared and tested USB `EFI` directory to the internal ESP.
4. Re-run `ocvalidate` against the internal copy.
5. Restart once and verify OpenCore, macOS, Windows and Ubuntu entries as applicable.

Keep the recovery USB unchanged. Do not delete the private backup merely because the first boot succeeded.

## System updates

Do not install a different Tahoe build while V61-P1 is enabled. A macOS update replaces patched system files, invalidates exact binary offsets and can remove Wi-Fi/audio root patches. See [Root patches and Wi-Fi](ROOT-PATCHES.md).
