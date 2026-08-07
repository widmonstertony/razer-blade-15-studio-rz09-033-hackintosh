# Restore and recovery

## Before changing the machine

1. Keep the private, unredacted EFI backup offline.
2. Put a known-working EFI on a FAT32 USB drive.
3. Confirm the firmware boot menu can start Windows directly.
4. Change only `EFI/OC/config.plist` for the first OLED/wake test.

## Restore the baseline config

The public `config.plist` is sanitized and cannot restore Apple services by itself. To roll back the real machine, use the private timestamped backup created beside the live config:

```text
EFI/OC/config.plist.pre-oled-wake-fix-YYYYMMDD-HHMMSS.bak
```

Rename the current `config.plist`, then copy the private backup back to `config.plist`.

## Rebuild from this repository

1. Restore the omitted Apple-derived components from your own installation.
2. Generate new `MacBookPro16,1` SMBIOS values with GenSMBIOS.
3. Replace MLB, ROM, SystemSerialNumber and SystemUUID in `EFI/OC/config.plist`.
4. Validate with the `ocvalidate` binary matching your OpenCore build.
5. Test from USB before installing to the internal EFI System Partition.
