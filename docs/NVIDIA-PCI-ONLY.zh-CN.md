# NVIDIA 研究：先做 PCI-only，Intel 仍作主显示

[English](NVIDIA-PCI-ONLY.md)

公开 main 是实验前的可用 26.7.1 基线，仍关闭 Quadro。机主另外同意在 **GitHub 备份/发布后**执行本地 PCI-only EFI 事务：现场备份精确实体 ESP，只改 `ACPI > Add > SSDT-DDGPU.aml > Enabled: true → false`，保留 AML 文件。Windows/Ubuntu 及其他 EFI 文件必须不变，Intel 属性、OLED、HID、MIC22X、无线、启动参数、SIP、AMFIPass、BAR 设置均保留。本阶段不安装驱动、不打根补丁、不改活动内核集合、不睡眠、不自动重启。

此前独显消失是 SSDT-DDGPU 调用固件断电方法。PCI-only 只是让自定义断电表不加载；仍需**完整关机再开机，并在 IORegistry 看到物理 `10de:1eb5`**。可能增加发热/耗电或影响启动/睡眠。PCI 可见不等于 Metal 加速。保留外部备份及恢复实验前 **26.7.1** EFI 的路径；原始 ESP 镜像不是 macOS 备份。

研究来源：[nullmoth v1.13.0](https://github.com/nullmoth/nvidia-macos-driver/releases/tag/v1.13.0)，提交 `0960b2ef5dbc4a573056d4c8e042a5bd2e15d651`。下载包的 Tahoe NVAccel 仍是旧构建，本地重建对齐了 NVRMFB 的分配器 ABI 和设计中的 headless 路径。四个候选 kext（NVRM、NVRMFB、NVRMAGDC、NVAccel）通过精确版本的**临时**内核集合链接；活动 Boot/System/Aux KC 哈希不变，驱动未安装。私有 ABI 静态检查和 CPU 测试不能证明真实 GPU 运算。

确认 PCI 可见后，另行审查的驱动安装还必须保留恢复能力，并验证真实 NVIDIA Metal 计算/渲染/回读、Intel 内屏、网络/音频/输入、多次冷启动，以及后续睡眠。Intel 运算、私人进程加载 bundle、kmutil 返回成功都不算 NVIDIA 成功。不承诺 CUDA 或所有 App 自动调用独显。不要盲跑上游全量安装器，其安全/BAR/系统改动不在 PCI-only 同意范围。本仓库不分发 NVIDIA 二进制或绑定机主机器的特权写入器。
