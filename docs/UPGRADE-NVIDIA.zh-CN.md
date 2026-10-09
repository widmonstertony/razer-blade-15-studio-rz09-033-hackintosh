# 升级与 NVIDIA 评估 — 2026-10-09

[English](UPGRADE-NVIDIA.md)

这是研究与验收方案，**不是直接升级可用系统的指令**。仓库 EFI 仍限定 26.2 / 25C56，不分发 macOS 安装器或 NVIDIA 驱动。

## 最新相关 macOS

Apple 当前最新正式版为 **27.0.1**，但 macOS 27 要求 Apple 芯片。这台 Intel i7-10875H 不能靠修改 SMBIOS 使用该原生系统。应评估的目标是 **Tahoe 26.7.1**，不是 27.2。来源：[Apple 版本列表](https://support.apple.com/en-us/100100)、[macOS 27 兼容要求](https://support.apple.com/en-us/127455)。

本机 `softwareupdate --list-full-installers` 提供 26.7.1 **25G241**，约 17.1 GiB。[对应 KDK](https://github.com/dortania/KdkSupportPkg/releases/tag/25G241) 已发布；其 DMG 公布 SHA-256 为 `9045a521c8ae362d4cb2cc54701b03462686e0069518a0b66cfe2028f87dfe9b`，实际下载仍须核验。“材料存在”不等于本机已经兼容。已有旧安装器或针对 25G82 的 OLED 候选不能替代新目标。

### 为什么还不能直接升

- V61 修改特定 Intel framebuffer 二进制，限定 Darwin 25.2.x。须重新验证目标原始字节、唯一匹配、偏移 / 调用目标和运行时训练；仅放宽 MinKernel/MaxKernel 不安全。
- 新导出的当前 Broadcom 条目为 MinKernel 23.0.0、无 MaxKernel，和之前受限候选不同；这不证明目标兼容。二进制和 Modern Wireless 根补丁仍必须针对目标验证。更新重建 System 卷，现有 BCM4360 Wi-Fi 可能失效。
- AppleHDA / Modern Audio 根补丁和 MIC22X 组合要重新启动和实际输入输出验收。
- 当前非官方根补丁 / 签名策略要整体评估，不能重试已失败的单独 AMFI / TCC1 迁移。
- [官方 OCLP 3.0.0-rc.3](https://github.com/dortania/OpenCore-Legacy-Patcher/releases/tag/3.0.0-rc.3) 是预发布，不是最新正式稳定版。[CustoMac v3.0.3](https://github.com/kgp-macPro/OCLP-CustoMac/releases/tag/v3.0.3) 是第三方分支，作者实机声明验证到 26.6.2，不是本机 26.7.1。不能让这些工具自动覆盖本机定制 OpenCore。

### 推荐顺序

1. 保留精确私有 ESP 镜像，另做可恢复的 System/Data 备份，验证固件 / 恢复 U 盘可启动。仅 EFI 不能回滚系统升级。
2. 离线备好精确 25G241 安装器、KDK、已审查 Wi-Fi / 音频材料，并准备 USB 网卡或支持的手机网络；测试前不能承诺升级后 Wi-Fi 可用。
3. 针对目标 framebuffer 移植 OLED，配对制作验证过的 U 盘 EFI，不动当前内置 EFI。
4. 另行取得机主批准，在独立 APFS 测试卷或外置 SSD 安装，保留当前可用 26.2。本指南没有执行分区 / 新建卷。
5. 验收冷 / 热启动、Wi-Fi 重连、扬声器 / 麦克风与应用权限、HID、10-bit、密码锁屏、菜单与真实合盖睡眠，以及 Windows/Ubuntu 恢复启动。记录实际结果，不能只看驱动已加载。
6. 所有项目通过后再批准主系统迁移。不要把 NVIDIA 实验和系统更新放在同一轮。

目前没有已证明的成功率或零风险保证。新版本安全修复值得准备迁移，但不能跳过恢复和精确 build 验证。

## NVIDIA：Intel 主显示 + 可选计算

已保存 Linux 数据确认本机 TU104GLM Quadro RTX 5000 Mobile/Max-Q 为 **10de:1eb5**，子系统 **1a58:200a**。[nullmoth 项目](https://github.com/nullmoth/nvidia-macos-driver)设备表含 1EB5，但 PCI-ID 匹配不证明 GPU 初始化、笔记本路由、计算或睡眠兼容。

已研究源码提交 `b9a5a2b35c6475b1df0e7438916df25ad5c6d1d8`。[应用 1.1.1 发布](https://github.com/nullmoth/nvidia-macos-driver/releases)说明捆绑驱动仍是 1.1.0。作者主要实机验证 RTX 5060 + macOS 15.8.1；[Turing 验证仍待完成](https://github.com/nullmoth/nvidia-macos-driver/blob/main/docs/CARD-SUPPORT.md)，Tahoe 和本机混合双显卡也没有完整验证。项目没有实现 Optimus 显示 mux；Metal/OpenCL/Vulkan 路径不等于 CUDA。

所以，**“Intel 负责显示，NVIDIA 可以调用”是实验目标，不是已保证可用的方案**。当前可用 EFI 继续禁用 NVIDIA。合理实验应在另行批准的独立 Sequoia 15.8.1 系统 + U 盘 EFI，先检查 GPU 电源 / PCI / BAR / GSP，再验证 Metal 枚举与小计算任务，保留 Intel OLED 和随时恢复的启动路径。启用独显也可能改变功耗、风扇、睡眠和外接显示路由，需要测量，不能假设。

不能因为设备表包含显卡，就把实验 NVIDIA kext 加载到可用主系统。此次没有加入 NVIDIA 安装器、固件或驱动二进制。
