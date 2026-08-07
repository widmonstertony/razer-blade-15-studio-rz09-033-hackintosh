# ACPI inventory

| Table | Baseline | Purpose inferred from table/source |
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
| SSDT-SLPWAK.aml | Disabled | Wraps firmware `_WAK` and notifies lid devices after S3 wake |
| SSDT-TB3HP.aml | Enabled | Thunderbolt hot-plug tree for `RP13` |

The matching baseline ACPI rename for `SSDT-SLPWAK.aml` (`_WAK` → `ZWAK`) is also disabled. Both pieces must be enabled together for the wrapper to call the original firmware method.
