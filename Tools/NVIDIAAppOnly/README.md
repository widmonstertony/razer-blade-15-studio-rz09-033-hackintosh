# Optional exact-target NVIDIA app-only startup overlay

[English guide](../../docs/NVIDIA-APP-ONLY.md) · [中文指南](../../docs/NVIDIA-APP-ONLY.zh-CN.md)

This directory contains **our service**, not the upstream NVIDIA driver package. It requires the already installed, initialized and small-workload-tested 25G241 / Quadro `10de:1eb5` target. It opens the Metal process list except WindowServer and schedules one post-desktop firmware initialization per boot, retaining `-nvrmnogo` to avoid upstream's display-arm routine. It does not write EFI, driver files, KCs, root patches, TCC or security settings. Successful installation is not next-boot or sleep qualification.

此目录只包含**本项目的后台服务**，不是 NVIDIA 驱动包。要求匹配目标已安装、初始化并通过小负载测试；开放兼容 Metal 进程但禁止 WindowServer，每次开机在 Intel 桌面就绪后最多初始化一次，保留 `-nvrmnogo`。不写 EFI、驱动、KC、根补丁、TCC 或安全参数。安装成功不等于下一轮启动或睡眠通过。

From the repository root / 在仓库根目录：

```sh
python3 -B Tools/NVIDIAAppOnly/install.py self-test
python3 -B Tools/NVIDIAAppOnly/test_install.py
sudo python3 -B Tools/NVIDIAAppOnly/install.py install
```

Read the complete guide before installing. Save the exact `BACKUP` path for removal; removal restores the original list's inode, preserving compatibility with the earlier private stage-1 rollback. No automatic hardware retry, reboot or sleep.

安装前读完整指南。保存精确 `BACKUP` 路径；撤回会恢复原列表的 inode，兼容旧的私人第一阶段回退。不自动重复硬件调用、重启或睡眠。

Rebuild native helper / 重建原生助手（Xcode Command Line Tools）：

```sh
xcrun clang -arch x86_64 -mmacosx-version-min=26.0 -O2 -Wall -Wextra -Werror \
  Tools/NVIDIAAppOnly/nvidia-startup.c \
  -framework IOKit -framework CoreFoundation -framework ApplicationServices \
  -o Tools/NVIDIAAppOnly/nvidia-startup
```

Compare hashes; different toolchains can change a binary. Do not replace installer pins without reviewing the changed source/build. Original code is covered by the repository MIT license. No upstream NVIDIA or Apple-derived binary is included in this directory.

比较哈希；不同工具链可能改变二进制。未审阅源码/构建差异前，不改安装器固定哈希。原创代码采用仓库 MIT 授权；本目录不含上游 NVIDIA 或 Apple 派生二进制。
