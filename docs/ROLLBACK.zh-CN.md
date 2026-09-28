# 回滚与恢复

[English](ROLLBACK.md)

回滚分成三个互相独立的部分：V61 内核补丁、P5 原生合盖 ACPI 三件套和 R4 用户态重画服务。只删除其中一个，并不能完整恢复到修复前状态。

## 最快且安全的恢复

1. 开机时按固件启动菜单键。
2. 启动已验证 U 盘 OpenCore，或直接启动 Windows。
3. 如果 U 盘能用，先备份重要数据，不要急着继续改内置 ESP。
4. 挂载内置 ESP，把完整、带日期的私有 EFI 备份作为一个目录恢复。
5. 用匹配 OpenCore 版本的 `ocvalidate` 检查 config，再重启。

不要只靠公开 config 重建私有 EFI：公开身份被故意设置为不可用，而且缺少 Apple 派生组件。

## 只删除自动重画服务

在仓库目录运行：

```sh
./Tools/OLEDWakeRescue/uninstall.sh
```

脚本会卸载 LaunchAgent 并删除它安装的两个程序，日志保留在 `~/Library/Logs`。不需要管理员密码。

没有这个服务时，V61 可能唤醒到活动的纯绿 framebuffer，而不是可用桌面。只有准备好恢复启动方式时才测试。

## 只禁用 P5 原生合盖三件套

只能先在 U 盘副本上操作，不要先改内置 ESP。必须同时禁用三项：

1. 把 `ACPI > Add > SSDT-SLPWAK.aml` 设为 `Enabled=false`；
2. 禁用 `_WAK → ZWAK` Patch；
3. 禁用限定活动 lid 路径的 `_LID → XLID` Patch。

运行 OpenCore 1.0.7 `ocvalidate`，从 U 盘启动，再测试菜单睡眠。只回滚一部分可能留下已经改名却没有替代实现的固件方法，绝不能这样使用。移除 P5 可能重新出现旧版偶发忽略合盖或二次睡眠；它不会移除 V61 或 R4。

## 只禁用 V61 内核补丁

在私有 EFI 的 U 盘副本上，用 ProperTree 或其他 plist-aware 编辑器操作：

1. 把 `Kernel > Add` 中 `RazerOLEDWakeFix.kext` 的 `Enabled` 改为 `false`。
2. 第一次回滚测试先保留 kext 文件，只改变一个变量更容易审计。
3. 运行 OpenCore 1.0.7 的 `ocvalidate`。
4. 从这个 U 盘启动并确认 macOS 能进入。

禁用 V61 会恢复 Tahoe 原生 framebuffer 路径，而这台机器会重新出现 OLED 唤醒黑屏。它是诊断或紧急回滚，不是可用的唤醒方案。

## 恢复仓库中的修复前基线

Git 历史在 V61-P1 提交之前保留了脱敏的修复前基线，也保留旧 V61-P1 发布分支。它们适合比较，不是可以直接使用的私有恢复镜像；这台机器应使用真正的带日期私有 EFI 备份。

## 回滚根补丁

回滚 EFI 不会撤销封印 macOS System 卷的修改。如果 Wi-Fi/音频根补丁导致无法启动：

1. 启动 Recovery 或已验证安装器；
2. 如果有匹配 APFS 快照就恢复，否则在不抹掉用户数据的前提下重装同一个 macOS build；
3. 必要时使用恢复 EFI，并禁用不兼容的注入 kext 条目；
4. 只应用为该精确系统 build 制作且已经审查的根补丁。

不要和 OLED 调试混在一起：先恢复稳定启动，再恢复 Wi-Fi/音频，最后重新测试 OLED 唤醒。
