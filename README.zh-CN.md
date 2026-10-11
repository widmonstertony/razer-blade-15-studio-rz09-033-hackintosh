# Razer Blade 15 Studio 2020 — Tahoe 26.7.1 / 25G241

[English](README.md)

## 当前 main — 2026-10-11

`main` 保存脱敏的 **26.7.1 / 25G241 / Darwin 25.6.x** Intel 主显示基线：OpenCore 1.0.7、HID V12、MIC22X 和 OLED **4.7.1**。机主已在完整关机再开机后确认 Wi-Fi、声音、触控板正常。OLED 已加载、补丁路由验证就绪；**26.7.1 真实睡眠唤醒尚未验收**，尤其是独显启用后的组合状态。默认公开 EFI 仍禁用 NVIDIA。

**新增 NVIDIA 实测证据：**机主本地的 NVRM + 重建 Tahoe NVAccel 已对 Quadro RTX 5000 完成一次固件初始化，识别 16 GiB 显存，并通过真正由 NVIDIA 执行的 Metal 整数计算和离屏绘图；Intel 主屏 10-bit 保持正常。见[应用专用 NVIDIA 配置、限制和回退](docs/NVIDIA-APP-ONLY.zh-CN.md)。`Tools/NVIDIAAppOnly` 提供可选后台启动服务和全应用 Metal 开放策略；安装密码授权和下一轮冷启动测试仍待完成。这不等于 CUDA、所有软件兼容或睡眠通过，复制默认 EFI 也不会安装这些系统组件。

从当前 [26.7.1 配置与验收指南](docs/MAIN-26.7.1.zh-CN.md)、[重装指南](docs/REINSTALL.zh-CN.md)、[精确状态](docs/STATUS-26.7.1.json)开始。个人 SMBIOS、Apple 衍生依赖未上传。完整全新重装尚未演练；这不是完整系统备份，也不是通用即插即用 EFI。

原来已验证 OLED 唤醒的 **26.2 / 25C56 版本**已按原提交 `843bab0` 保存到 [backup/macos-26.2-v61p5-mic22x-20261011](https://github.com/widmonstertony/razer-blade-15-studio-rz09-033-hackintosh/tree/backup/macos-26.2-v61p5-mic22x-20261011)。26.2 请用该分支。更换 EFI 不能降级系统，也不能恢复根补丁。

安全设置仍配对保留：`amfi=0x80`、SIP `03080000`、SecureBootModel Disabled、AMFIPass 禁用。NVIDIA PCI-only 是[已经完成的早期实验阶段](docs/NVIDIA-PCI-ONLY.zh-CN.md)；当前有界 Metal 实测及可选自动服务见上方独立指南。

## 以下为历史 26.2 发布记录，不是当前 main 安装指引

以下内容保留旧版本记录，其中睡眠成功、二进制版本及系统限制不代表本目录的 26.7.1 EFI。请先使用上方新版指南。

**2026-10-09 可用状态快照：**从新一轮只读实体 ESP 备份刷新，配对加入 MIC22X 音频。机主确认完整关机再开机后扬声器、麦克风可用；另行修复应用麦克风授权后，Codex 听写也可用。公开 EFI 已脱敏，不是完整私有 ESP 镜像。请阅读[当前快照与音频配置](docs/CURRENT-SNAPSHOT.zh-CN.md)和[升级 / NVIDIA 评估](docs/UPGRADE-NVIDIA.zh-CN.md)。此次没有升级 macOS，也没有加入 NVIDIA 驱动。

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
| 音频 | ALC298、自定义 AppleALC 1.9.7 MIC22X、`layout-id=22`、VerbStub，以及精确版本 Modern Audio 根补丁 |
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

全新安装、没有旧私有 EFI 时，先读[重装依赖准备与固定根补丁配方](docs/REINSTALL.zh-CN.md)。准备工具从固定来源下载并校验缺失依赖，只生成私人暂存 EFI，不写 ESP/系统根目录/TCC；完整重装验收仍待完成，26.7.1 暂时拒绝使用旧方案。

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
- 内置音频：MIC22X 在完整关机再开机后，输入输出经机主确认可用；仍依赖完全匹配版本的根补丁。配置方法和热重启验收限制见[音频说明](docs/CURRENT-SNAPSHOT.zh-CN.md)。
- NVIDIA Quadro RTX 5000 Max-Q：在 macOS 下禁用。
- 后续 macOS 更新：在重新验证所有二进制补丁和 V61 偏移前不支持。

## 可复现范围

这份指南以“另一位相同 RZ09-0330Q 配置的使用者也能从零复现”为目标，但它不是通用的一键 EFI。必须先按中英文[安装说明](docs/INSTALL.zh-CN.md)从 U 盘测试；你需要生成自己的 SMBIOS，补回自己合法取得且仓库省略的 Apple 派生文件，并为完全相同的系统 build 制作根补丁。绝不能复制本机的私有 EFI、序列号、ROM、Windows BCD 或 APFS 数据。

## 发热和风扇

这台机器在 macOS 下不会天然更冷。Razer 的 EC/风扇策略主要为 Windows 设计，而缩放后的 4K HiDPI 桌面会让 `WindowServer` 长期合成较大的 framebuffer。改 CPU 或风扇参数前先读[温度与功耗](docs/THERMALS.zh-CN.md)。OLED 自动服务只在唤醒事件后工作，平时处于空闲状态，不是持续发热源。

## 源码和授权

另有[已做离线检查的多版本 OLED 源码候选](Sources/RazerOLEDWakeFix-MultiVersion/README.zh-CN.md)，保留旧版匹配并加入独立的 26.7.1 移植；没有装入此 EFI，也尚未通过新版真实唤醒测试。见[隔离升级与升级后清单](docs/UPGRADE-26.7.1.zh-CN.md)；研究 EFI 不是正式升级许可。

仓库原创文档、配置和自定义代码采用 [MIT](LICENSE)。第三方组件继续遵守各自上游许可证，见 [THIRD_PARTY.md](THIRD_PARTY.md)。上面列出的 Apple 派生二进制均未包含。

## 免责声明

黑苹果配置取决于具体硬件、固件和系统版本。错误的 framebuffer 或内核补丁可能导致黑屏或无法启动。务必保留 Windows/固件直接启动方式和已验证 U 盘。此项目与 Apple、Razer、Intel、NVIDIA 及 OpenCore 上游项目无隶属关系。
