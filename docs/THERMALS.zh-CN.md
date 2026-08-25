# 温度与功耗

[English](THERMALS.md)

Razer 笔记本在 macOS 下不会自动变得更冷、更安静。Apple 的风扇曲线、CPU 功耗限制和显示功耗都是为自家硬件调的；这台 Blade 使用 Razer EC、被禁用的 NVIDIA 独显、驱动 4K 10-bit OLED 的 Intel 核显，以及非原生 ACPI/SMC 上报。

## 已验证机器上的证据

2026-08-24 的只读采样发现：

- 活动显示 GPU 只有 Intel UHD 630；`system_profiler SPDisplaysDataType` 没有 NVIDIA；
- 屏幕使用要求保留的 10-bit 路径和较大的 HiDPI backing surface；
- `WindowServer` 短时约占 32% CPU；
- 正在工作的 Codex renderer/service 和 Continuity Capture 也贡献了负载；
- 后一次双样本中 CPU 约 89% 空闲，所以不是 CPU 一直满载；
- BetterDisplay 有活动图形客户端；
- 交流电下 `displaysleep` 和 `sleep` 都为 0，即自动息屏和自动睡眠被关闭；
- OLED 自动服务是事件驱动且处于空闲，不是持续 CPU 消耗者。

这些证据首先指向显示合成和当前应用负载，还不能证明 CPU 电源管理坏了。要判断长期原因，仍需静置 10 分钟对比温度；一秒钟的进程快照不能说明持续状态。

## 为什么 4K、10-bit、HiDPI 会热

系统显示“看起来像 1680×945”时，macOS 实际先渲染 3360×1890 backing surface，再合成到 3840×2160 屏幕。30-bit framebuffer 比 8-bit 搬运更多数据。窗口透明、视频、浏览器动画和 BetterDisplay 都可能让核显/显示引擎保持忙碌。OLED 在显示大面积亮色时本身也更耗电。

V61-P1 故意保留 10-bit。降温不等于必须改成 8-bit。

## 安全优化顺序

1. 电脑完全不操作静置 10 分钟，再看“活动监视器”的 CPU 和能源页。
2. 暂时退出 BetterDisplay，保持 1680×945 HiDPI 不变，比较 5 分钟内 `WindowServer` 占用和温度。
3. 关闭较重的浏览器、视频和 Codex 工作；不用时关闭 Continuity Camera。
4. 使用深色模式并降低画面亮度。OLED 显示暗色时能降低面板功耗，同时不改变 10-bit framebuffer。
5. 如果需要，恢复合理的交流电空闲计时：

   ```sh
   sudo pmset -c displaysleep 10 sleep 30 disksleep 10 powernap 0 womp 0
   ```

   这只修改交流电策略。修改前先用 `pmset -g custom` 记录旧值，修改后验证一次自动睡眠唤醒。
6. 如果能接受性能降低，先在“系统设置”里测试低电量模式，不要先改 EFI CPU 数据。动画变差就恢复。
7. 在 Windows 中更新 Razer 固件并验证原厂风扇/EC 行为。不要刷非官方 EC 固件，也不要在 macOS 下随意写风扇寄存器。

不要为了摸起来凉就随机安装 CPUFriend 配置、永久关闭 Turbo Boost 或直接改 PL1/PL2。应先记录静置温度、CPU package power 和最高占用进程，否则这些限制会掩盖独显或显示电源路径的问题。

## 确认独显仍被关闭

```sh
system_profiler SPDisplaysDataType
ioreg -l -w0 -r -c IOPCIDevice | grep -iE 'nvidia|geforce|quadro|10de'
```

NVIDIA 不应作为活动 macOS 显示设备出现。如果它重新出现，或空闲时电池仍明显掉电，应先审查 `SSDT-DDGPU.aml`、对应 config 项和 BIOS 状态，再考虑限制 CPU。

## 关于显示“60 W”电源

Blade 的大功率圆口电源由 Razer EC 管理，不走 Apple 原生 USB-C 充电器栈。本次检查中 macOS 只报告 `Connected: Yes`，没有可靠瓦数。因此界面里的 60 W 可能只是 ACPI/SMC 兼容占位值，不能证明 330 W 电源在电气上只输出 60 W。

真正要检查的是：原装电源是否被 Razer 固件识别、Windows 下持续 CPU+GPU 负载时电池是否仍放电，以及插头/线缆是否异常发热。如果这些检查失败，应单独诊断电源；不要通过修改 macOS 显示的瓦数标签来“修复”。
