# OLED 唤醒原理与验证

[English](OLED-WAKE.md)

## 用大白话解释故障

睡眠后电脑本身其实已经醒了：触控板能动，鼠标也会移动。OLED 链路也不是完全死亡，但桌面 framebuffer 只显示黑色或整块绿色。强制重启前的一瞬间甚至可能看到睡眠前画面。这说明问题不是“整台电脑没醒”，而是屏幕链路恢复和桌面重画没有完整完成。

裸机 Linux 能唤醒这块完全相同的屏幕，证明固件、屏幕供电和 Intel 显示引擎在物理上都能恢复。把 Linux、Windows 的过程与 Tahoe 对比后发现，这块屏幕在 S3 后需要 eDP 1.4 rate-select 事务和特定的 Intel 源端训练状态；Tahoe 的 Coffee Lake 原生 framebuffer 路径没有在这台 Razer 上完整重现。

## 两层解决方案

### 第一层：`RazerOLEDWakeFix.kext` V61

这个 Lilu 插件只针对 Darwin 25.2 的 `AppleIntelCFLGraphicsFramebuffer`。它会先核对运行中二进制的精确指令模式，匹配后才修改。在原生 LinkTraining 写 DPCD `0x100` 的位置，补丁改用这块屏幕已经证明可行的 eDP 1.4 序列：把 DPCD `0x115` 设为 rate selector 2，然后向 `0x101` 写 lane count。它同时保留 V60 已验证的面板供电、双重唤醒通知和 Intel 动态 I_boost 处理。

任何关键资格、指令模式或 AUX 操作失败时都会退回 Apple 原生路径。OpenCore 把它限制在 `25.2.0`–`25.2.99`；这是安全边界，不是需要去掉的麻烦。

### 第二层：受保护的 CoreGraphics 重画

V61 能恢复活动显示链路，但桌面仍可能停在纯绿 framebuffer。下面这个 CoreGraphics 显示模式事务可以稳定触发重画：

```text
1680×945 HiDPI / 3360×1890 / 60 Hz
          ↓ 0.75 秒
1600×900 HiDPI / 3200×1800 / 60 Hz
          ↓ 精确恢复
1680×945 HiDPI / 3360×1890 / 60 Hz
```

`OLEDWakeRescueAgent` 注册真实 IOKit 系统电源通知。收到 `kIOMessageSystemHasPoweredOn` 后等待 8 秒，再启动同目录 helper。helper 只允许一块内置 Samsung vendor/product `19587/41001` 屏幕，而且当前模式和唯一救援模式必须完全匹配，否则拒绝执行。它只做 app-only 事务，并验证最终准确恢复。agent 每次执行前还会检查文件所有者、权限、普通文件类型和代码签名。

服务没有轮询循环。平时正常使用时不会持续操作显示器。

## 最终实机结果

2026-08-24 的验证使用真实 Normal Sleep/Wake，而不是仅关闭显示器：

- 电源日志确认 Normal Sleep/Wake；
- 通过实体电源键唤醒；
- V61 报告 eDP rate selector 已拦截，并在 DPCD `0x115` 验证 selector 2；
- 6 次 I_boost 设置全部验证，拒绝为 0；
- initial、phase-1、phase-2 训练路径都运行；
- lane clock-recovery/channel-equalization 和 BMU 检查通过；
- 自动服务在第 8 秒运行并恢复精确原模式；
- 用户确认桌面正常可见；
- 唤醒后仍为 30-bit framebuffer（`ARGB2101010`）。

公开版本只记录脱敏后的结论和预期计数，不上传包含机器身份或启动会话数据的原始日志。

## 运行时检查

```sh
ioreg -r -n RazerOLEDWakeFix -l | egrep \
  'VersionInfo|PatchStatus|V61PatchArmStatus|EDP14|IBoost'

launchctl print "gui/$UID/com.tonytan.razer-oled-wake-rescue"
tail -n 80 "$HOME/Library/Logs/RazerOLEDWakeRescue.log"
system_profiler SPDisplaysDataType | grep 'Framebuffer Depth'
```

预期身份属性包括：

```text
VersionInfo = REL-450-2026-08-24
PatchStatus = applied-verified-s3-edp14-rate-select-v61
V61PatchArmStatus = all-v60-routes-plus-edp14-rate-selector-call-site-verified
```

运行 `Tools/verify-v61p1.sh` 可以完成静态和运行时检查。

## 已知边界

- 补丁内含 Tahoe 26.2 的精确 framebuffer 指令模式；其他 build 必须视为不兼容。
- 救援分辨率被故意固定为已经测试的 HiDPI 模式。用户如果改了日常分辨率，helper 会安全拒绝；必须修改、审查并重新编译后才能支持新模式。
- LaunchAgent 只在已登录的 Aqua 用户会话中运行，不能修复登录前显示问题。
- 接入外屏不会把这个内屏专用修复变成通用方案。
- hibernate/deep standby 尚未扩展验证；目前结论只覆盖已测试的 Normal Sleep/Wake。
- 补丁不能让 OLED 的物理亮度控制变成传统 LCD 背光控制。

## 源码目录

- `Sources/RazerOLEDWakeFix/`：V61 kext 源码和可移植 CLT 构建脚本。
- `Sources/HID-V12/`：针对已记录 VoodooI2C/VoodooI2CHID 上游提交的补丁。
- `Tools/OLEDWakeRescue/src/`：用户态 helper/agent 源码。
- `Tools/OLEDWakeRescue/bin/`：实测的 x86_64 ad-hoc 签名二进制。
