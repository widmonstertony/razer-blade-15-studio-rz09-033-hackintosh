# ACPI 清单

[English](ACPI.md)

| 表 | V61-P5 状态 | 从表和源码推断的用途 |
| --- | --- | --- |
| SSDT-BATT.aml | 启用 | 电池字段/EC 兼容 |
| SSDT-AWAC.aml | 启用 | AWAC/RTC 兼容 |
| SSDT-EC-USBX.aml | 启用 | EC 与 USB 供电属性 |
| SSDT-ALC298.aml | 禁用 | ALC298 音频唤醒/供电辅助 |
| SSDT-ALS0.aml | 启用 | 环境光传感器设备 |
| SSDT-DDGPU.aml | 启用 | Darwin 下调用 NVIDIA `PEGP._OFF()` |
| SSDT-GPRW.aml | 启用 | GPRW 瞬时唤醒处理 |
| SSDT-PLUG.aml | 启用 | CPU 电源管理 plug 设备 |
| SSDT-PNLF.aml | 启用 | `GFX0` 的 Apple 背光接口 |
| SSDT-SBUS-MCHC.aml | 启用 | 暴露 SMBus/MCHC 设备 |
| SSDT-OC-XOSI.aml | 启用 | Darwin 感知的 `_OSI` 行为 |
| SSDT-I2C.aml | 启用 | I2C 输入设备配置 |
| SSDT-TPDX.aml | 禁用 | 可选触控板辅助 |
| SSDT-SLPWAK.aml | **启用** | 只针对活动 EC lid 的有界 Darwin 唤醒/盖子状态校正 |
| SSDT-TB3HP.aml | 启用 | `RP13` 的雷电热插拔设备树 |

## P5 原生合盖三件套

P5 必须同时具备以下三项：

1. 在 `ACPI > Add` 中启用 `SSDT-SLPWAK.aml`；
2. 把固件 `_WAK` 改名为 `ZWAK`；
3. 只把 `\_SB.PCI0.LPCB.EC0.LID0._LID` 改名为 `XLID`，OpenCore Patch 必须同时限定 `Base`、`DSDT` signature 和 `Count=1`。

P4 在调用固件 `ZWAK` 前设置一次“返回开盖”标志，却只能等未来某次 `_LID` 读取时清除。如果固件没有在 `ZWAK` 内同步读取 `_LID`，标志会跨过唤醒继续残留，并把下次真实关盖误报为开盖。P5 只允许该覆盖在 `ZWAK` 同步执行期间生效，`ZWAK` 返回后无条件清零，再把 EC lid 字段同步为开盖并发送一次状态通知。之后的每次真实关盖/开盖都直接调用原始 `XLID()`。

2026-09-27 的真实物理合盖测试明确记录 `Clamshell Sleep`，随后从 `Normal Sleep` 唤醒；运行态为 `AppleClamshellCausesSleep=Yes`、`SleepDisabled=No`，也没有二次睡眠。

不要只启用三项中的一部分，不要把 `_LID → XLID` 做成全局改名，也不要在没有重新解析活动 lid 路径时把此表移植到另一份 DSDT。
