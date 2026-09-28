# 根补丁、Broadcom Wi-Fi 与音频

[English](ROOT-PATCHES.md)

已验证机器运行 macOS Tahoe 26.2 build 25C56，并使用与该版本完全匹配的根补丁来支持 Broadcom BCM4360 Wi-Fi 和内置音频。这些补丁不属于 EFI，也是整个安装中最脆弱的部分。

## 仓库包含和不包含的内容

config 中保留了这些 Enabled 引用：

```text
IOSkywalkFamily.kext
IO80211FamilyLegacy.kext
IO80211FamilyLegacy.kext/Contents/PlugIns/AirPortBrcmNIC.kext
```

这些 Apple 派生二进制没有上传，打过补丁的 System 卷也没有上传。只能从你自己合法获得的 macOS 安装和已经审查许可证的补丁流程获取所需文件。

测试系统使用了 [tahoe-patchset 项目](https://github.com/lzhoang2801/tahoe-patchset)的临时非官方 OpenCore-Patcher 3.0.0 构建。它不代表 [OpenCore Legacy Patcher 官方项目](https://github.com/dortania/OpenCore-Legacy-Patcher)的保证，也不能假设它支持其他 Tahoe build。

## 精确版本流程

1. 确认 `sw_vers -buildVersion` 返回 `25C56`。
2. 备份完整私有 EFI 和 macOS 数据。
3. 如果补丁器要求 KDK，从 Apple Developer 下载并安装与 **26.2 / 25C56 完全匹配**的 Kernel Debug Kit。
4. 使用仓库当前 OpenCore 安全状态启动：`SecureBootModel=Disabled`、`csr-active-config=0x0803`，boot args 为 `amfi=0x80 -lilubetaall`。
5. 使用已经审查的补丁器版本，只应用所需的 Modern Wireless 和 Modern Audio 根补丁集。
6. 通过 OpenCore 重启。
7. 验证 Wi-Fi 连接和上网、内置扬声器、麦克风/耳机以及一次干净重启。
8. 重新运行 `Tools/verify-v61p5.sh`，然后分别重复菜单睡眠唤醒和真实物理合盖/开盖测试。

不要因为界面里有 Post-Install Root Patch 按钮就直接点击。必须先确认系统 build、KDK 和 patchset 定义全部匹配。把补丁器生成的日志与私有备份放在一起保存。

## 为什么升级后 Wi-Fi 或音频会消失

macOS 更新会重建封印的 System 卷。之前修改过的 framework 和驱动会被新版本 Apple 文件替换，同时二进制签名和偏移发生变化。因此旧补丁可能消失，也可能变得不安全；即使 OpenCore 还能启动，仅靠 EFI 里的 Broadcom 条目也不能让 BCM4360 在 Tahoe 下工作。

以后只有在新版本被明确支持时，才按这个顺序更新：

1. 先在 U 盘测试 EFI 中更新并验证 OpenCore/kext 兼容性；
2. 获取精确的新 macOS 安装器和匹配 KDK；
3. 针对新的 framebuffer 二进制移植并验证 V61，之后才能启用；
4. 更新 macOS；
5. 应用专门为该 build 制作的根补丁；
6. 重启并验证 Wi-Fi/音频；
7. 重新进行受保护的 OLED 睡眠唤醒验证。

本仓库目前没有任何证据证明 V61-P5/R4 可安全用于 Tahoe 26.6.x。Darwin 版本限制是故意保留的保护措施。

## Wi-Fi 消失时的恢复

- 临时使用网线、USB 网络共享或已知支持的 USB Wi-Fi。
- 确认 `Kernel > Add` 引用的两个省略 kext 确实位于正确路径。
- 确认精确版本根补丁真正完成，而且已经重启。
- 不要反复在 Ubuntu 安装 `broadcom-sta-dkms` 来修 macOS；Linux 软件包不会修改 macOS 驱动。
- 如果根补丁失败，恢复匹配的封印 System 快照或重装同一 macOS build，再使用已审查的精确版本补丁器。
