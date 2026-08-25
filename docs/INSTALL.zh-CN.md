# 安装说明 — V61-P1 OLED 唤醒版本

[English](INSTALL.md)

写入任何 EFI 分区前，请先完整读完本页。公开配置已经脱敏，而且故意省略了不能重新分发的文件，所以不能直接覆盖使用。

## 1. 确认目标完全匹配

在 macOS 中运行：

```sh
sw_vers
uname -r
system_profiler SPDisplaysDataType
ioreg -lw0 | grep -E 'DisplayVendorID|DisplayProductID'
```

只有在机器为 RZ09-0330Q、BIOS 1.06、macOS 26.2 build 25C56 / Darwin 25.2.x、UHD 630 `8086:9BC4`、Samsung 屏幕 `4C83:A029` 时才继续。安装脚本还会强制检查系统版本，显示工具会强制检查屏幕和精确分辨率。

## 2. 做两份可恢复备份

1. 挂载内置 EFI System Partition（ESP）。
2. 把完整的私有 `EFI` 文件夹复制到两个不同存储设备。
3. 第一次测试时保留一个已验证可启动的 FAT32 U 盘 EFI。
4. 确认固件启动菜单能够直接启动 Windows。

不要公开私有备份。里面包含机器身份，也可能含 Windows BCD 数据。

## 3. 准备 U 盘测试 ESP

使用空 U 盘。在“磁盘工具”中选择**显示 > 显示所有设备**，选中 U 盘物理设备，把它抹掉为 **MS-DOS (FAT)** 和 **GUID 分区图**。抹掉会删除全部内容，务必核对两次目标磁盘。

把本仓库的 `EFI` 文件夹复制到 U 盘卷的根目录。此时不要写入内置 ESP。

## 4. 恢复故意省略的文件

从你自己合法获得且目前可用的私有 EFI，把下面文件复制到 U 盘相同路径：

```text
EFI/OC/Kexts/IOSkywalkFamily.kext
EFI/OC/Kexts/IO80211FamilyLegacy.kext
EFI/OC/Drivers/HfsPlus.efi
EFI/OC/Drivers/apfs_aligned.efi
```

公开 config 保留了这些引用，以便准确展示已测试顺序。如果你合法使用其他替代文件，必须同步修改 `UEFI > Drivers` 和 `Kernel > Add`；不要让不存在的文件仍处于 Enabled。

## 5. 生成唯一 SMBIOS

用 [GenSMBIOS](https://github.com/corpnewt/GenSMBIOS) 生成唯一的 `MacBookPro16,1` 身份，然后替换 U 盘 config 中四个公开占位值：

```text
PlatformInfo > Generic > MLB
PlatformInfo > Generic > ROM
PlatformInfo > Generic > SystemSerialNumber
PlatformInfo > Generic > SystemUUID
```

不要使用网上展示的值，也不要使用另一台 Mac 的值。如果是在更新现有可用安装，只能在本地把私有 config 的四项值移植到 U 盘 config，绝不能提交到 Git。

## 6. 处理可选 Ubuntu 项

已验证机器是 macOS/Windows/Ubuntu 多系统，所以 `Misc > BlessOverride` 含 `\EFI\ubuntu\shimx64.efi`。只有系统里确实存在这个文件时才保留；否则删除这一条。它与 macOS 和 OLED 唤醒无关。

## 7. 启动前验证

使用 OpenCore 1.0.7 自带的 `ocvalidate`：

```sh
/path/to/OpenCore-1.0.7/Utilities/ocvalidate/ocvalidate /Volumes/EFI/EFI/OC/config.plist
plutil -lint /Volumes/EFI/EFI/OC/config.plist
```

两项都必须通过。还要确认 U 盘 EFI 里没有 `.before-*`、`FSCK*`、`._*` 或 `.DS_Store`。

加入私有文件前，可先在仓库根目录验证整个 checkout：

```sh
shasum -a 256 -c SHA256SUMS.txt
```

加入私有 SMBIOS 和省略文件后，校验值自然会改变；这些私有修改应始终留在 Git 之外。

## 8. 从 U 盘启动并做静态检查

在固件启动菜单里选择 U 盘 OpenCore；此时不要设为永久默认。登录 macOS 后确认：

- 键盘、触控板和触屏正常；
- UHD 630 加速和 1680×945 HiDPI 正常；
- 如果完全匹配版本的根补丁已经存在，Wi-Fi 和内置音频正常；
- NVIDIA 独显没有作为活动的 macOS 显示设备出现。

在仓库目录运行：

```sh
chmod +x Tools/verify-v61p1.sh Tools/OLEDWakeRescue/*.sh
./Tools/OLEDWakeRescue/install.sh
./Tools/verify-v61p1.sh
```

`install.sh` 不需要管理员密码。它会在 `~/Library/Application Support/RazerOLEDWakeRescue` 安装两个当前用户拥有的程序，并在 `~/Library/LaunchAgents` 安装一个启动项。

## 9. 受控睡眠唤醒测试

1. 保存全部工作，拔掉不必要的 USB/Thunderbolt 设备。
2. 保持交流电连接，并把恢复 U 盘放在手边。
3. 选择**苹果菜单 > 睡眠**；这次验证不要只测试显示器睡眠。
4. 至少等待 30 秒。
5. 只按一次实体电源键唤醒。
6. 唤醒后至少等待 12 秒。自动重画在第 8 秒运行前，可能短暂看到黑屏或绿屏。
7. 桌面恢复且可以操作后检查：

```sh
tail -n 80 "$HOME/Library/Logs/RazerOLEDWakeRescue.log"
system_profiler SPDisplaysDataType | grep -E 'Framebuffer Depth|Resolution|Looks like'
pmset -g log | tail -n 80
```

预期日志包含 `POWER_EVENT`、一个或多个 `WAKE_RESCUE_ATTEMPT`，最后是 `WAKE_RESCUE_PASS`。已测试的 10-bit 结果显示 `Framebuffer Depth: 30-Bit Color (ARGB2101010)`。

如果屏幕没有恢复，不要把这个 EFI 写入内置盘。用已验证 U 盘启动并按[回滚](ROLLBACK.zh-CN.md)操作。

## 10. 只有验证通过后才写入内置 ESP

确认 U 盘启动、输入、Wi-Fi/音频（如适用）、重启、冷启动和睡眠唤醒全部通过后：

1. 挂载内置 ESP。
2. 把当前私有 `EFI` 文件夹重命名并复制到其他磁盘作为带日期备份。
3. 把已经完整准备和测试过的 U 盘 `EFI` 复制到内置 ESP。
4. 对内置 config 再运行一次 `ocvalidate`。
5. 重启并核对 OpenCore、macOS、Windows 和 Ubuntu（如适用）的启动项。

恢复 U 盘保持不变。第一次启动成功不代表可以删除私有备份。

## 系统更新

启用 V61-P1 时不要安装其他 Tahoe build。系统更新会替换已打补丁的系统文件、让精确二进制偏移失效，并可能清除 Wi-Fi/音频根补丁。详见[根补丁与 Wi-Fi](ROOT-PATCHES.zh-CN.md)。
