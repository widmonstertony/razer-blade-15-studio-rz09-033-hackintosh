# Quadro 应用专用 Metal：真实证据、可选自动启动与准确限制

[English](NVIDIA-APP-ONLY.md)

## 2026-10-11 的实际结果

精确机主目标：RZ09-0330Q / BIOS 1.06 / i7-10875H / Intel UHD 630 `8086:9bc4` / Quadro RTX 5000 Max-Q TU104 `10de:1eb5`，子系统 `1a58:200a` / Samsung OLED `4c83:a029` / macOS 26.7.1 `25G241`。

机主本地驱动已经安装。一次获同意的 GSP/RM 初始化成功，真实查询到 16 GiB 显存。随后一轮 Metal 测试明确按 PCI `10de:1eb5` 选择 Quadro，而不是 Intel：三轮各 65,536 个整数计算结果，以及离屏渲染全部 4,096 个 RGBA 像素均正确。独立复查确认 Intel 主屏 10-bit、安装文件/签名、内核缓存和安全参数保持原状，未发现新的内核或 WindowServer 崩溃报告文件。这是小负载正确性，不是完整稳定性或性能验收。

全应用访问/自动启动服务现已在机主本机安装并独立核对：root 所有的助手/列表/plist 哈希及权限匹配，launchd 注册后运行一次、退出码 0，本次已经初始化的开机被跳过，没有额外固件调用。Intel 10-bit、驱动/原组件文件和签名、KC、安全参数保持原状。下一轮自动冷启动验收仍待完成。root 私有回退备份由机主安装器创建，本次非 root 独立核验没有打开它。精确后续状态见 [STATUS-26.7.1.json](STATUS-26.7.1.json)。默认公开 `EFI/` 仍是 Intel 主显示、NVIDIA 关闭的基线。

## “所有应用”具体是什么意思

此配置向一般进程开放 NVMTL Metal 设备，但在通配符之前明确禁止 WindowServer；已安装插件的 headless 检查还在环境覆盖前禁止 WindowServer。Intel 继续驱动内屏；不安装 NVIDIA framebuffer、AGDC 或 Intel override kext。

这只是让兼容的 Metal 程序能发现设备，不会强制软件选它，也不会实现驱动尚缺的 API/功能。软件在 Windows/Linux 支持 NVIDIA 或 CUDA，不代表它兼容此 macOS 移植。CUDA、真实日常应用、NVENC/NVDEC、持续负载、跨 GPU 呈现、睡眠和全部显存容量/性能均未在本机验收。不设置全局 `NVMTL_ENABLE` 环境变量。

## 已安装驱动是前提——不是全新驱动安装器

源输入是 [nullmoth v1.13.0](https://github.com/nullmoth/nvidia-macos-driver/releases/tag/v1.13.0)，固定源码提交 `0960b2ef5dbc4a573056d4c8e042a5bd2e15d651`。发行包 SHA256：`994970b1bba53e3ae8d7f500ef27b25cae8bc6818abc112d35230a58e6882fdf`。

只安装 NVRM 和 NVAccel。发行包的 Tahoe NVAccel 分配器 ABI 陈旧，因此机主候选从固定源码以 `NM_TAHOE=1`、Apple clang 21、NVIDIA 610.57.04 头文件重建。用户态资源/嵌套签名在私人副本修复，Mach-O 代码/数据不变。AuxKC 保留 AppleMobileDevice、SoftRAID、HighPointRR、HighPointIOP；没有运行通用厂商安装器或改成 AMFIPass。这些驱动/固件二进制及机主私人事务**不在仓库分发**。

安装此叠加服务前，须先另行审阅安装方案，复现匹配组件、确认精确版本 KC 联合链接、备份真实 EFI/KC/原组件、准备 NVIDIA-off 回退，并完成受控初始化和小负载 Metal 正确性验证。公开叠加工具不会安装这些前提，也不是完整 NVIDIA 从零重装配方；全新重装尚未演练，不能宣传为所有机器一键显卡支持。

主要运行时固定值：

| 文件 | SHA256 |
| --- | --- |
| NVRM 可执行文件 | `4c74eb5a5be05e8819f2d8bd6ec41e09978b6ef9dccbd06de54f4ac927c32d96` |
| Tahoe NVAccel 可执行文件 | `f687a54d88eda9e065bda5275877d71f0e6eb2345de63813ebe97b69de498af5` |
| NVMTLDriver 可执行文件 | `a9877ab9bb61722b7a1b2f5a0458dc5cb83890370e16fa46c1030377b864640b` |
| 实际六组件 AuxKC | `3b323074df4dea0c4e61de275e96e0ca5713eb20d9da5057394e09f9b8562b14` |

KC 包含本地组件元数据，另一台机器/缓存可能正常地不同。工具会拒绝这种情况；不能为了安装通过就删掉检查，需另行审阅适配。

## 自动初始化，但不接管显示

保留 `-nvrmnogo`。直接去掉它会进入上游的启动等待/显示 arm 流程并写 framebuffer/AGDC 控制。此方案改用 root 后台服务 `com.tonytan.nvidia-app-only`，等待登录后的 Samsung Intel 主屏处于活动状态、驱动进入 headless，再每次开机调用**最多一次** NVRM control selector 0 / scalar 2，不走厂商显示接管流程。等待桌面最多 30 分钟；不设 KeepAlive，不在调用失败后重试，不自动重启、不挂睡眠唤醒钩子、不跑负载。

未知 build/启动参数、其他硬件、文件/KC/安全参数变化、发现显示接管组件或桌面不就绪都会拒绝。每次开机的 root 私有排他日志在打开控制客户端前强制落盘；已初始化则直接跳过，不调用第二次。这不能消除实验性 DMA/ABI/启动/发热风险，内核调用失败仍可能卡死，也不会在睡眠后自动重新初始化显卡。

助手是本仓库 MIT 授权的原生程序，无需单独 GUI app 或 Node 运行时。审计者可用 Xcode Command Line Tools 重建并比较包哈希；硬编码精确目标必须经审阅后适配其他版本。

## 前提验收后才安装叠加服务

在仓库根目录运行，需 Python 3：

```sh
python3 -B Tools/NVIDIAAppOnly/install.py self-test
python3 -B Tools/NVIDIAAppOnly/test_install.py
sudo python3 -B Tools/NVIDIAAppOnly/install.py install
```

密码只输入终端，不发聊天。安装器要求本次开机已经初始化且 Intel 主屏正常。不写 EFI、启动参数、内核缓存、根补丁、Wi-Fi/音频/HID 或安全设置。只新增 `/Library/Application Support/RazerNVIDIA`、一个 LaunchDaemon 和 `nvmtl-allow.txt`；原列表以 rename 保存在 root 私有 `/Library/Application Support/RazerNVIDIABackups` 事务中，保留 inode 以兼容此前的第一阶段回退。

保存输出的 `BACKUP` 和 RESULT。安装成功只是**已配置**，不是下一次启动/睡眠通过。手动完整关机再开机，首次不接外置屏，确认 Intel 10-bit、Wi-Fi、真实声音及 HID。查看 `/Library/Application Support/RazerNVIDIA/startup.log`，对助手运行 `--inspect`，核对一次成功日志和真实 16 GiB 查询。初始化失败不要反复 kickstart/重试。然后再分别验收真实应用、功耗和睡眠。

## 撤回服务 / 恢复

必须替换成你自己输出的**精确 BACKUP 路径**：

```sh
sudo python3 -B Tools/NVIDIAAppOnly/install.py remove --transaction '/Library/Application Support/RazerNVIDIABackups/txn-YOUR-EXACT-ID'
```

有未返回硬件调用的日志时拒绝撤回；否则停用后台服务，文件/日志保留到该事务的 `quarantine`，恢复原诊断调用列表及 inode。它不会卸载 NVIDIA 内核代码，也不能撤回本次开机的硬件状态；之后需手动完整关机。先撤回这个服务，才运行旧第一阶段驱动回退。

无法启动时用此前已安装的 OpenCore **空格 → NVIDIA OFF - Recovery → Y → macOS**。不 Reset NVRAM，不清禁用标志；恢复启动参数不同，服务会拒绝初始化。这里没有厂商自动卸载服务；进系统后先撤回此服务，再用私人第一阶段回退。内置盘备份和 GitHub 不能保证无需独立启动介质恢复，也不含完整系统/个人数据备份。

## 警告及 OLED 状态

小测试的复制引擎队列创建失败，改用图形通道；`SET_LG_SECTOR_PROMOTION` 返回警告。BAR1 仅 256 MiB，映射 local 分配默认回退到系统 RAM；小负载正确和识别 16 GiB 不等于全部显存访问/性能通过。PCIe 拓扑断言仍在。可选 PV/保留映射检测硬编码旧 `24G830` 地址，在 `25G241` 拒绝，不能据此说已确认无内存损坏，也没有绕过该拒绝。

OLED **26.2** 菜单/合盖唤醒已由机主验证，但保留绿色间隔再由 R4 重画。**26.7.1** 的 OLED 4.7.1 启动补丁路由及 R4 已就绪，真实菜单/合盖唤醒以及独显启用后的组合状态仍未验收。保存工作、由机主观察真实唤醒，确认可见锁屏、10-bit、Wi-Fi/声音/HID、不再次睡回去后，才能说新版 OLED 修复成功。此工具不自动睡眠或重启测试。

脱敏证据：[NVIDIA-EVIDENCE.json](NVIDIA-EVIDENCE.json)。不上传私有序列号、原始 ESP、Windows BCD、TCC 数据库或私人日志。
