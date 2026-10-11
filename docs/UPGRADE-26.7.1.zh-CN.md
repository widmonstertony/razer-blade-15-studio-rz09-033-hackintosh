# 26.7.1 / 25G241 — 隔离验证与升级后清单

<!-- CURRENT-MAIN-NOTICE-20261011 -->

当前 main 为 26.7.1 / 25G241，下面保留的是早期 26.2 或移植前记录，不是当前执行指引。请以[新版完整配置/重建指南](MAIN-26.7.1.zh-CN.md)为准。新版 Wi-Fi/音频/触控板已确认，OLED 加载/路由通过，但新版睡眠及全新重装未验收。26.2 使用备份分支。

## 历史记录（版本和状态以原记录为准）

[English](UPGRADE-26.7.1.md)

**尚未发布：现在不要升级当前可用系统。** OLED 离线检查已通过，但还没有隔离启动、根补丁内核缓存和实机唤醒证据。本页是后续执行方案，不是“已经装成功”的记录。相关资料：[OLED 移植证据](../Sources/RazerOLEDWakeFix-MultiVersion/PORT-25G241.zh-CN.md)、[完整重装流程](REINSTALL.zh-CN.md)。

## 升级前条件

只适用已审计的 RZ09-0330Q、BIOS、屏幕、显卡/PCH、Wi-Fi 与音频硬件组合。每台机器使用自己的 SMBIOS。将系统/个人数据和实体 EFI 备份到独立存储，并确认恢复 U 盘确实能启动 26.2。只有 EFI 镜像不能恢复系统卷、个人数据或系统根补丁。

首次验证优先使用独立外置 SSD。没有外置 SSD 时，须另行同意才可建立内置盘的独立 APFS 测试卷：虽然不覆盖旧系统，仍会修改容器、共享磁盘故障风险，且需要独立完整备份。本工具不会建卷、抹盘。外接显示器能辅助救援，但不能代替测试磁盘。

安装包必须是 Apple 26.7.1 / 25G241，KDK 必须精确对应。提前缓存根补丁材料，第一次启动不能假设 Wi-Fi 有网。此次不混入 NVIDIA、TCC 数据库或新的安全设置实验。

## 根补丁材料的只读检查

在仓库根目录运行：

```sh
python3 -B Tools/Reinstall/reinstall.py plan --build 25G241
python3 -B Tools/Reinstall/reinstall.py verify-inputs --build 25G241 --support-root /路径/Universal-Binaries --kdk-root /路径/已解包KDK
```

固定 fork 对 XNU 25 使用 Modern Wireless 的 13.7.2-25 材料和 Modern Audio 的 26.0 Beta 1 AppleHDA；源码定义能生成这些补丁，不代表已经在 25G241 实测成功。检查工具核对精确 KDK 版本/构建号、七个补丁主程序/兼容库及 CFL 哈希；不执行补丁、不等于完整 bundle 验证。

离线 kextlibs 发现 Libm 与 DspFuncLib 同时导出 `_ceil` 和 `_pow`（退出码 2）。这表示静态查找有歧义，**不是已经证明实机缓存必然失败**；还需要真实 kmutil 重建与启动。不能擅自删除系统库或音频插件来“消除报错”。

## 候选 EFI 工具包

`Tools/Reinstall/prepare_upgrade.py` 接收完整、机主已确认可用的 EFI，独立编译的 4.7.0 OLED 候选以及 ocvalidate 1.0.7。缺少公开仓库省略依赖的裸 EFI 会被拒绝。

```sh
python3 -B Tools/Reinstall/prepare_upgrade.py --baseline-efi /路径/已验证/EFI --candidate-kext /路径/build/RazerOLEDWakeFix.kext --ocvalidate /路径/ocvalidate --research-only
```

仅在独立私有目录制作文件，通过配置校验并审计允许的改动；不写实体 EFI、不改启动顺序、不安装系统。输出明确标记 `RESEARCH-NOT-READY.json`，不是允许升级的凭证。

| 文件夹 | 用途 | Wi-Fi / OLED |
| --- | --- | --- |
| rollback | 回到 26.2 | EFI 文件与输入基线逐字节一致 |
| installer-native | 安装器和新版首次启动 | 三个 Wi-Fi Add 与一个 Skywalk Block 一起限制到 25.2.99；新版使用原生 Skywalk；旧 OLED 版本限制保留 |
| oled-isolated-test | 新版根补丁完成后，单独测试 OLED | OLED 候选只在 25.6 加实验开关；旧 Wi-Fi 暂不加载 |
| wifi-isolated-test | OLED/音频通过后，受控验证 Wi-Fi | OLED 同上；四个 Wi-Fi Add/Block 一起改为 25.6.0–25.6.99 |

实验 EFI 不用于启动现在的可用系统。保留固件中恢复 U 盘的选择路径；Windows/Ubuntu 的 EFI 和 BCD 必须原样保留，此工具不自动合并到实体分区。

## 升级/安装到测试系统之后，你需要做什么

1. 使用 installer-native 启动**独立测试系统**，不是旧 26.2；确认系统为 26.7.1 / 25G241。暂时不要睡眠、不要合盖。此时 BCM4360 Wi-Fi/AppleHDA 音频可能还没恢复，不是最终完成状态。
2. 只在测试系统安装 Apple 精确 KDK 26.7.1 / 25G241，按重装文档使用固定源码构建的根补丁工具。检测计划必须只有 Modern Wireless 和 Modern Audio，不使用其 OpenCore 构建器。输入缺失、选择最近 KDK、出现额外补丁、缓存重建或 APFS 快照失败就停止；保留完整日志，核对补丁记录中的 KDK/系统构建号/提交。
3. 先用仍限制 Wi-Fi 的原生 EFI 完全关机后冷启动测试系统，验证 MIC22X/layout 22/VerbStub 的扬声器、麦克风实际录音、音量/静音与耳机。输入电平会动不等于录音成功，也不等于应用权限已授权。
4. 原生启动与音频通过后，经机主批准才用 oled-isolated-test。必须确认候选“全部检查完成并启用”，不是仅看到 kext 名字。工具包附独立的 R4-isolated-test 副本：安装要求 `--isolated-test`、实际 25G241/Darwin 25.6 和实验启动参数。此安装副本尚未实测；公开稳定 R4 的 25C56 限制不变。
5. 配好相同屏幕、HiDPI、60 Hz 模式，**仅在测试系统**安装 R4 实验副本；测试系统菜单睡眠、物理合盖睡眠、唤醒、锁屏和多轮循环。允许初期出现绿屏，但必须恢复可用桌面，不立刻再次睡眠、不强制重启，10-bit 模式能保留。记录真实 Normal Sleep/Wake、OLED 状态与 R4 完成日志。
6. 上述通过后再用 wifi-isolated-test；验证 BCM4360 连网、DHCP/DNS/互联网、2.4/5 GHz、传输稳定性、蓝牙共存和睡眠后重连。文件哈希无法保证实际联网/AWDL。
7. 复查触控板/触屏、关机、热重启、冷启动音频和麦克风应用权限。每个用户/应用单独授权，不分发 TCC 数据库，不替别人静默授权。Apple Watch 锁定时验证睡眠后立即要求密码。
8. 全部实测通过后，才把候选按已验证版本重新审查、制作带你私有身份的正式升级 EFI，并更新 GitHub 的脱敏源码和中英文教程；再单独批准正式系统/实体 EFI 迁移。候选生成器不会自动执行这些操作。

任何环节失败，使用回退 U 盘返回 26.2，保留测试系统日志。不能靠改安全设置、反复强制睡眠或自动重启来掩盖失败。
