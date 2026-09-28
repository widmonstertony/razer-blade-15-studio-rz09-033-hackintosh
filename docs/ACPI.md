# ACPI inventory

[简体中文](ACPI.zh-CN.md)

| Table | V61-P5 state | Purpose inferred from table/source |
| --- | --- | --- |
| SSDT-BATT.aml | Enabled | Battery field/EC compatibility |
| SSDT-AWAC.aml | Enabled | AWAC/RTC compatibility |
| SSDT-EC-USBX.aml | Enabled | Embedded controller and USB power properties |
| SSDT-ALC298.aml | Disabled | ALC298 audio wake/power helper |
| SSDT-ALS0.aml | Enabled | Ambient-light sensor device |
| SSDT-DDGPU.aml | Enabled | Calls NVIDIA `PEGP._OFF()` under Darwin |
| SSDT-GPRW.aml | Enabled | GPRW instant-wake handling |
| SSDT-PLUG.aml | Enabled | CPU power-management plug device |
| SSDT-PNLF.aml | Enabled | Apple backlight interface on `GFX0` |
| SSDT-SBUS-MCHC.aml | Enabled | SMBus/MCHC device exposure |
| SSDT-OC-XOSI.aml | Enabled | Darwin-aware `_OSI` behavior |
| SSDT-I2C.aml | Enabled | I2C input-device configuration |
| SSDT-TPDX.aml | Disabled | Optional touchpad helper |
| SSDT-SLPWAK.aml | **Enabled** | Bounded Darwin wake/lid-state correction for the active EC lid |
| SSDT-TB3HP.aml | Enabled | Thunderbolt hot-plug tree for `RP13` |

## P5 native-lid pair

P5 requires all three items together:

1. `SSDT-SLPWAK.aml` enabled in `ACPI > Add`;
2. firmware `_WAK` renamed to `ZWAK`;
3. only `\_SB.PCI0.LPCB.EC0.LID0._LID` renamed to `XLID`, scoped by OpenCore `Base`, `DSDT` signature and `Count=1`.

P4 armed a one-shot “return open” flag before calling firmware `ZWAK`, but cleared it only when a future `_LID` read occurred. If firmware did not read `_LID` synchronously, the flag survived wake and could consume the next real close event. P5 keeps the override valid only while `ZWAK` is on the stack, clears it unconditionally afterward, synchronizes the EC lid field to open and sends one status notification. Every later physical close/open goes directly through the original `XLID()` method.

The physical 2026-09-27 test recorded `Clamshell Sleep`, then a `Normal Sleep` wake with `AppleClamshellCausesSleep=Yes`, `SleepDisabled=No` and no second sleep.

Do not enable only one or two members of the pair, do not make `_LID → XLID` global, and do not reuse this table on a different DSDT without resolving the exact active lid path.
