# 已知问题与边界

[English](KNOWN-ISSUES.md)

1. **只支持精确系统 build。** V61 只针对 Tahoe 26.2 / Darwin 25.2.x。新的 macOS build 必须先重新验证 framebuffer 二进制、内核补丁和根补丁。
2. **只支持精确屏幕与模式。** 自动重画只支持 Samsung `4C83:A029`、1680×945 HiDPI / 3360×1890 / 60 Hz，并使用 1600×900 HiDPI 救援模式；任何不匹配都会安全拒绝。
3. **需要用户会话。** LaunchAgent 只在登录后的 Aqua 会话运行，不能重画登录前/FileVault 画面。
4. **OLED 亮度不是传统背光控制。** 软件调暗只是后备手段，不等于修复物理唤醒链路。
5. **Broadcom Wi-Fi 与内置音频依赖精确 build 的根补丁。** 仓库不含 Apple 派生文件，系统更新也会重建并清除根补丁系统卷。
6. **唤醒绿色过渡仍存在。** 稳定 R4 通常先显示约 8–11 秒整块绿色，再重画锁屏桌面。R5–R9 遮罩实验可能在 API 报告成功时让物理 OLED 永久绿色，因此不包含在发布版。
7. **10-bit 是单独的 BetterDisplay 状态。** P5/R4 在 24-bit 下也能唤醒。BetterDisplay 4.3.5 当前可得到 `30-Bit Color (ARGB2101010)`，但 EFI/V61 不会强制开启，而且尚未再证明下一次唤醒后仍保持。每次唤醒或切换模式后都要检查 framebuffer。
8. **已验证菜单与真实合盖 Normal Sleep/Wake，但未泛化验证休眠。** 深度 standby、hibernate image 以及所有扩展坞/外屏组合都不在已证明范围内。
9. **P5 是成套 ACPI。** 只启用表或只启用一条改名都可能破坏固件方法解析；活动 lid 改名必须限定到 `\_SB.PCI0.LPCB.EC0.LID0`。
10. **公开 EFI 已脱敏。** 生成唯一 SMBIOS 并补回合法取得的省略文件前不能直接启动。
11. **macOS 可能比 Windows 更热。** Razer EC 风扇策略并非原生适配，缩放 4K HiDPI 合成也会增加 WindowServer 负载。参见 `THERMALS.zh-CN.md`。

不要通过删除版本限制来“解决”边界；必须移植并重新验证对应组件。
