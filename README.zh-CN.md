# Razer Blade 15 Studio 2020 黑苹果 — OLED 唤醒 V61-P1

[English](README.md)

这个版本是**已经实机验证的 OLED 睡眠唤醒版本**，不再是旧的“修复前基线”。在完全匹配的测试机器上，真实 Normal Sleep/Wake 后已恢复可见的 10-bit 桌面，并通过 V61-P1 运行时检查。

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
| 显示模式 | 1680×945 HiDPI、3360×1890 backing、60 Hz、30-bit framebuffer |
| 唤醒结果 | Normal Sleep/Wake 通过；自动受保护重画后桌面正常可见 |

这是精确硬件和系统版本上的结果，不是通用 OLED 修复。`RazerOLEDWakeFix.kext` 被故意限制在 Darwin `25.2.0`–`25.2.99`。没有重新核对源码偏移和面板时序前，不要解除限制，也不要在其他系统版本或屏幕上使用。

## 这个版本包含什么

- OpenCore 1.0.7 启动文件和已经脱敏的 V61-P1 `config.plist`。
- `RazerOLEDWakeFix.kext` 及可审计源码。
- HID V12 的 VoodooI2C/VoodooI2CHID 二进制和可复现源码补丁。
- 用户态、事件驱动的 OLED 自动唤醒服务：只在真实唤醒 8 秒后，把完全匹配的 Samsung 内屏从 1680×945 HiDPI 短暂切到 1600×900 HiDPI，再准确恢复原模式。其余时间休眠；屏幕或分辨率不匹配时会拒绝执行。
- 中英文安装、根补丁、验证、温度和回滚说明。

当前成功方案需要两层共同工作：V61 内核补丁恢复 OLED 的 eDP 1.4 链路训练路径；用户态显示事务让 CoreGraphics 重画已经恢复的 framebuffer。安装自动服务不需要管理员密码。

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
5. 运行 `Tools/verify-v61p1.sh`，再做一次受控睡眠唤醒测试。
6. 多次冷启动、重启和唤醒都通过前，保留 U 盘和原始私有 EFI。

完整原理和最终证据见[OLED 唤醒原理与验证](docs/OLED-WAKE.zh-CN.md)，恢复方法见[回滚](docs/ROLLBACK.zh-CN.md)。

## 当前实际状态

- OLED Normal Sleep/Wake：在精确测试目标上，V61 加自动重画服务后**可用**。
- 触控板和触屏：HID V12 下可用。
- 10-bit 输出：保留；唤醒后的测试报告为 `Framebuffer Depth: 30-Bit Color (ARGB2101010)`。
- Broadcom BCM4360 Wi-Fi：依赖仓库未包含的 Apple 派生 kext 和完全匹配系统版本的根补丁。
- 内置音频：依赖测试安装中完全匹配版本的根补丁。
- NVIDIA Quadro RTX 5000 Max-Q：在 macOS 下禁用。
- 后续 macOS 更新：在重新验证所有二进制补丁和 V61 偏移前不支持。

## 发热和风扇

这台机器在 macOS 下不会天然更冷。Razer 的 EC/风扇策略主要为 Windows 设计，而 4K、10-bit、HiDPI 桌面会让 `WindowServer` 长期合成较大的 framebuffer。改 CPU 或风扇参数前先读[温度与功耗](docs/THERMALS.zh-CN.md)。OLED 自动服务只在唤醒事件后工作，平时处于空闲状态，不是持续发热源。

## 源码和授权

仓库原创文档、配置和自定义代码采用 [MIT](LICENSE)。第三方组件继续遵守各自上游许可证，见 [THIRD_PARTY.md](THIRD_PARTY.md)。上面列出的 Apple 派生二进制均未包含。

## 免责声明

黑苹果配置取决于具体硬件、固件和系统版本。错误的 framebuffer 或内核补丁可能导致黑屏或无法启动。务必保留 Windows/固件直接启动方式和已验证 U 盘。此项目与 Apple、Razer、Intel、NVIDIA 及 OpenCore 上游项目无隶属关系。
