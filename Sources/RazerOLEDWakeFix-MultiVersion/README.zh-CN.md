# 多版本 OLED 候选 — 尚未发布为可用版

[English](README.md)

这个脱敏的 26.7.1 NVIDIA-off 备份包含 4.7.2。我在启用原 DDGPU 断电表、保持 R4 不变时观察到一次菜单睡眠唤醒成功；恢复 NVIDIA 后测试失败。见[我的精确结果与限制](../../docs/STATE-2026-10-12.zh-CN.md)。本版新增 `SleepPhaseGate.hpp` 有界睡眠阶段门控；48 个 ASan/UBSan 测试保留旧版路径并拒绝已观察到的过早恢复。多轮/合盖与 NVIDIA 共存仍未验收，静态测试本身不能证明唤醒。旧 26.2 备份分支不变。

DriverProfiles.hpp 在硬件初始化/打补丁前，用有边界检查的 Mach-O LC_UUID 解析器选择配置。25C56 UUID 为 0A970005-966B-39C9-9D30-1E8F9BF6F86A；原 V61 逻辑和独立的 10 个精确匹配/调用偏移不变，离线匹配、调用目标和形状检查全部通过。

25G241 UUID 为 7E294F0F-852E-3252-B54D-61D9A7BBE6B2；独立的十处匹配/七个调用实现现已通过 KDK 与发布代码静态检查，七个明确使用的对象字段都有原生代码证据。仍标记 IsolatedValidationPending、默认拒绝；明确的实验启动参数只允许隔离验证，不代表正式可用。见[完整移植证据和验证门槛](PORT-25G241.zh-CN.md)。

未知/损坏镜像、Darwin 不符、匹配缺失/不唯一、原调用目标不符或 rel32 越界时拒绝。UUID 不等于整段代码或功能已验证；拒绝也不能保证原生唤醒。原版 V61 二进制由机主验证过，不代表加入新安全检查的候选在 25C56 已经过实机验证。

## 构建与测试

依赖 Lilu 1.7.1 resources、MacKernelSDK 提交 05094e5e88cec7caedbfb35e8449ed0db94bf95b 和能编译 x86_64 的 Apple SDK。按[原版构建说明](../RazerOLEDWakeFix/README.md)设置 LILU_RESOURCES、MAC_KERNEL_SDK、MACOS_SDK，用 zsh 运行本目录 build-clt.sh。SDK 不分发；测试命令见英文说明，编译器不同不保证字节一致。

带 AddressSanitizer/UndefinedBehaviorSanitizer 的宿主测试覆盖旧版接受，以及新版未完成、错误内核、未知 UUID、截断、缺少/重复 UUID、损坏 load command 的拒绝。这些不是实际内核加载证据。

下一步：明确的新系统睡眠/锁屏及重复验收。原调用目标与 rel32 距离在写指令前检查，九个钩子在全部完成前只转发；但钩子安装并非原子事务。不能同时加载两个相同 bundle ID；main 限定 25.6.x 并保留实验参数，旧备份分支仍限定 25.2.x。自定义代码使用 MIT，上游 bootstrap/SDK 保留各自授权。
