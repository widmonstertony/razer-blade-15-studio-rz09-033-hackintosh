# Rollback and recovery

[简体中文](ROLLBACK.zh-CN.md)

Rollback has three independent parts: the V61 kernel patch, the P5 native-lid ACPI set, and the R4 user-space redraw service. Removing only one part does not recreate the complete pre-fix state.

## Fastest safe recovery

1. Hold the firmware boot-menu key during power-on.
2. Boot the known-good USB OpenCore or Windows directly.
3. If the USB works, leave the internal ESP untouched until important data is backed up.
4. Mount the internal ESP and restore the complete dated private EFI backup as one directory.
5. Validate its config with the matching OpenCore `ocvalidate`, then restart.

Never reconstruct a private EFI from the public config alone: the public identity is deliberately unusable and Apple-derived components are absent.

## Remove only the automatic redraw service

From this repository checkout:

```sh
./Tools/OLEDWakeRescue/uninstall.sh
```

This unloads the LaunchAgent and removes its two installed executables. Logs are kept in `~/Library/Logs`. No administrator password is required.

Without the service, V61 may wake to a live green framebuffer rather than a usable desktop. Test only when a recovery boot path is available.

## Disable only the P5 native-lid set

Work on a USB copy, never the internal ESP first. Disable all three P5 members together:

1. set `ACPI > Add > SSDT-SLPWAK.aml` to `Enabled=false`;
2. disable the `_WAK → ZWAK` patch;
3. disable the path-scoped active-lid `_LID → XLID` patch.

Run OpenCore 1.0.7 `ocvalidate`, boot the USB, and retest menu sleep. A partial rollback can leave firmware methods renamed without their replacement and must not be used. Removing P5 may restore the earlier intermittent ignored-lid or second-sleep behavior; it does not remove V61 or R4.

## Disable only the V61 kernel patch

On a USB copy of the private EFI, use ProperTree or another plist-aware editor:

1. Set the `Kernel > Add` entry for `RazerOLEDWakeFix.kext` to `Enabled=false`.
2. Leave the V61 kext file in place for the first rollback test; changing one variable is easier to audit.
3. Run OpenCore 1.0.7 `ocvalidate`.
4. Boot that USB and confirm macOS starts.

Disabling V61 restores Tahoe's native framebuffer route, which on this machine reintroduces the OLED black-wake failure. It is a diagnostic or emergency rollback, not a working wake solution.

## Restore the pre-fix repository baseline

Git history retains the sanitized pre-fix baseline immediately before the V61-P1 commit and the earlier V61-P1 release branch. They are useful for comparison, not ready-to-use private recovery images. Use the real dated private EFI backup for the machine.

## Root-patch rollback

EFI rollback does not undo changes to the sealed macOS System volume. If Wi-Fi/audio root patches cause a boot failure:

1. boot Recovery or a known-good installer;
2. restore the matching APFS snapshot when available, or reinstall the exact macOS build without erasing user data;
3. boot a recovery EFI with the incompatible injected kext entries disabled if necessary;
4. apply only a reviewed root patch set built for that exact OS build.

Keep this separate from OLED debugging: first regain a stable boot, then restore Wi-Fi/audio, then retest OLED wake.
