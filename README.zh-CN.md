# Razer Blade 15 Studio 2020 黑苹果 — OLED 唤醒 V61-P5 + R4

[English](README.md)

这个版本是**目前可日常使用、并经过本机验证的 OLED 睡眠唤醒版本**，不再是旧的“修复前基线”。在完全匹配的测试机器上，苹果菜单睡眠和真实物理合盖睡眠都能唤醒到可见、仍保持锁定的桌面。OLED 刚亮时仍会显示约 8–11 秒整块绿色，随后由 R4 服务自动重画。V61-P5 还修复了旧版偶发吞掉真实合盖事件的问题。

## 已验证目标

| 项目 | 实机验证值 |
| --- | --- |
| 电脑 | Razer Blade 15 Studio Edition 2020，RZ09-0330Q，BIOS 1.06 |
| CPU / 核显 | Intel Core i7-10875H / UHD 630 `8086:9BC4` |
| 内屏 | Samsung OLED `4C83:A029`，EDID 名称 `SDCA029` |
| macOS | Tahoe 26.2，版本 `25C56`，Darwin `25.2.0` |
| OpenCore | 1.0.7 |
| OLED 补丁 | V61，kext 4.5.0，标记 `61.0-oled-s3-edp14-rate-select` |
| 输入补丁 | HID V12 |
| 显示模式 | 1680×945 HiDPI、3360×1890 backing、60 Hz；BetterDisplay 4.3.5 覆盖后当前报告 30-bit `ARGB2101010` |
| 唤醒结果 | 菜单与真实合盖 Normal Sleep/Wake 均通过；锁屏保留；R4 重画后桌面正常 |
| 原生合盖补丁 | V61-P5 有界 `_WAK` / 活动 `EC0.LID0._LID` 校正 |

这是精确硬件和系统版本上的结果，不是通用 OLED 修复。`RazerOLEDWakeFix.kext` 被故意限制在 Darwin `25.2.0`–`25.2.99`。没有重新核对源码偏移和面板时序前，不要解除限制，也不要在其他系统版本或屏幕上使用。

## 这个版本包含什么

- OpenCore 1.0.7 启动文件和已经脱敏的 V61-P5 `config.plist`。
- P5 的 `SSDT-SLPWAK.aml`、可审计 ASL 源码、已启用的固件 `_WAK → ZWAK` 改名，以及只限定活动 lid 路径的 `_LID → XLID` 改名。
- `RazerOLEDWakeFix.kext` 及可审计源码。
- HID V12 的 VoodooI2C/VoodooI2CHID 二进制和可复现源码补丁。
- 稳定的 R4 用户态事件驱动唤醒服务：只在真实唤醒 8 秒后，用两次 session 级 CoreGraphics 事务把完全匹配的 Samsung 内屏从 1680×945 HiDPI 短暂切到 1600×900 HiDPI，再恢复原来的可用模式。其余时间休眠；屏幕或分辨率不匹配时会拒绝执行。
- 中英文安装、根补丁、验证、温度和回滚说明。

当前成功方案需要三部分共同工作：V61 恢复 OLED 的 eDP 1.4 链路训练路径，P5 保证唤醒前后原生 lid 状态正确，R4 让 CoreGraphics 重画已经恢复的 framebuffer。安装自动服务不需要管理员密码。

## 重要：不能直接复制启动

公开仓库为了安全和合法分发，故意做了脱敏并省略若干文件：

- MLB、ROM、序列号和 UUID 都是 OpenCore 示例占位值。
- OpenCore 密码哈希和 salt 已清空。
- 不分发 Apple 派生的 `IOSkywalkFamily.kext`、`IO80211FamilyLegacy.kext`、`HfsPlus.efi` 和 `apfs_aligned.efi`。
- Wi-Fi 和内置音频的根补丁必须由你自己的合法安装环境生成。

绝对不要先覆盖内置 EFI。先制作 FAT32 U 盘测试 EFI，从自己的机器恢复省略文件，生成唯一 SMBIOS，运行 `ocvalidate`，并确认启动、输入和睡眠唤醒都正常，再考虑写入内置 ESP。

## 从这里开始

1. 从头读完[安装说明](docs/INSTALL.zh-CN.md)。
2. 改动 macOS 前先读[根补丁与 Wi-Fi](docs/ROOT-PATCHES.zh-CN.md)。
3. 制作并启动 U 盘测试 EFI。
4. 运行 `Tools/OLEDWakeRescue/install.sh` 安装自动唤醒服务。
5. 运行 `Tools/verify-v61p5.sh`，然后分别测试苹果菜单睡眠，以及一次真实物理合盖/开盖。
6. 唤醒时预期最多约 12 秒绿色；必须等 R4 有时间完成重画后再判断成败。
7. 多次冷启动、重启、菜单唤醒和合盖唤醒都通过前，保留 U 盘和原始私有 EFI。

完整原理和最终证据见[OLED 唤醒原理与验证](docs/OLED-WAKE.zh-CN.md)，恢复方法见[回滚](docs/ROLLBACK.zh-CN.md)。

## 当前实际状态

- OLED Normal Sleep/Wake：在精确测试目标上，V61 加 R4 自动重画后**可用**。
- 真实合盖/开盖：P5 下**可用**；实测进入 `Clamshell Sleep`，从 `Normal Sleep` 唤醒，锁屏保留，也没有再次自动睡回去。
- 唤醒观感：R4 恢复桌面前仍会看到约 8–11 秒纯绿色 framebuffer。这是稳定版的已知预期，不代表已经实现苹果原生机器一样的无缝唤醒。
- 触控板和触屏：HID V12 下可用。
- 色深：未启用 BetterDisplay 覆盖时，P5/R4 合盖唤醒审计为 `24-Bit Color (ARGB8888)`；在 BetterDisplay 4.3.5 中启用 10-bit 后，新一轮运行时审计为 `30-Bit Color (ARGB2101010)`。这是用户态显示设置，不是 EFI/V61 自动提供的功能；每次唤醒或切换显示模式后都应重新核对。
- Broadcom BCM4360 Wi-Fi：依赖仓库未包含的 Apple 派生 kext 和完全匹配系统版本的根补丁。
- 内置音频：依赖测试安装中完全匹配版本的根补丁。
- NVIDIA Quadro RTX 5000 Max-Q：在 macOS 下禁用。
- 后续 macOS 更新：在重新验证所有二进制补丁和 V61 偏移前不支持。

## 可复现范围

这份指南以“另一位相同 RZ09-0330Q 配置的使用者也能从零复现”为目标，但它不是通用的一键 EFI。必须先按中英文[安装说明](docs/INSTALL.zh-CN.md)从 U 盘测试；你需要生成自己的 SMBIOS，补回自己合法取得且仓库省略的 Apple 派生文件，并为完全相同的系统 build 制作根补丁。绝不能复制本机的私有 EFI、序列号、ROM、Windows BCD 或 APFS 数据。

## 发热和风扇

这台机器在 macOS 下不会天然更冷。Razer 的 EC/风扇策略主要为 Windows 设计，而缩放后的 4K HiDPI 桌面会让 `WindowServer` 长期合成较大的 framebuffer。改 CPU 或风扇参数前先读[温度与功耗](docs/THERMALS.zh-CN.md)。OLED 自动服务只在唤醒事件后工作，平时处于空闲状态，不是持续发热源。

## 源码和授权

仓库原创文档、配置和自定义代码采用 [MIT](LICENSE)。第三方组件继续遵守各自上游许可证，见 [THIRD_PARTY.md](THIRD_PARTY.md)。上面列出的 Apple 派生二进制均未包含。

## 免责声明

黑苹果配置取决于具体硬件、固件和系统版本。错误的 framebuffer 或内核补丁可能导致黑屏或无法启动。务必保留 Windows/固件直接启动方式和已验证 U 盘。此项目与 Apple、Razer、Intel、NVIDIA 及 OpenCore 上游项目无隶属关系。
