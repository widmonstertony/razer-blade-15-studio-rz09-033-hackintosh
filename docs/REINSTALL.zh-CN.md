# 从仓库重装：依赖、根补丁与验收

<!-- CURRENT-MAIN-NOTICE-20261011 -->

当前 main 为 26.7.1 / 25G241，下面保留的是早期 26.2 或移植前记录，不是当前执行指引。请以[新版完整配置/重建指南](MAIN-26.7.1.zh-CN.md)为准。新版 Wi-Fi/音频/触控板已确认，OLED 加载/路由通过，但新版睡眠及全新重装未验收。26.2 使用备份分支。

## 历史记录（版本和状态以原记录为准）

[English](REINSTALL.md)

26.7.1 候选另见[隔离升级与升级后清单](UPGRADE-26.7.1.zh-CN.md)。只读根补丁配方/材料检查和私有研究 EFI 生成已实现，尚不代表新版或全新重装全流程已验证。

目标是：相同硬件的其他机主，不依赖 Tony 的旧 EFI 或私有系统，也能重建配置。**当前状态：已实现依赖准备和只读检查；尚未完成一次从空白系统开始的全流程重装验收。**现有 26.2 安装已由机主验证，不能把这个结果当作重装流程也已验证。

## 支持范围

仅支持 RZ09-0330Q、BIOS 1.06、i7-10875H、物理 Intel 8086:9bc4、PCH 8086:068d、Samsung OLED 4c83:a029（SDCA029）、BCM4360 14e4:43a0、ALC298，以及 **macOS 26.2 / 25C56**。伪装的 MacBookPro16,1 不代表物理硬件相同，必须核对真正的机器、BIOS、屏幕和设备。

另一台完全相同的机器必须使用自己的 SMBIOS 和个人授权。不同屏幕、BIOS、显卡、网卡、声卡或电脑，需要单独适配配置；不能宣称通用。**26.7.1 / 25G241 暂时明确拒绝使用旧方案**，直到 OLED、私有对象布局、根补丁和 R4 全部完成独立验证。

## 1. 备份与安装

先把个人数据和完整私有 ESP 备份到独立存储设备，准备并验证可启动的恢复 U 盘与固件启动选择。通过 Apple 合法取得并安装精确支持的系统 build。准备工具不会格式化磁盘或写入任何 ESP。[安装指南](INSTALL.zh-CN.md)始终要求先用 USB 测试。

## 2. 没有旧 EFI，也能准备缺失依赖

先阅读[第三方来源与授权](../THIRD_PARTY.md)、对应上游项目和许可。没有 Python 3 时先自行安装。双击 `Tools/Prepare-Reinstall.command`（或用 zsh 运行）：它会先询问是否同意获取依赖，校验公开仓库和每个下载文件的固定 SHA256，在 `.reinstall-private/` 下建立新的**私人暂存目录**。

也可以在仓库根目录运行：

```sh
python3 -B Tools/Reinstall/reinstall.py prepare --build 25C56 --accept-asset-licenses
```

暂存 EFI 会补齐 IOSkywalkFamily 1.2.0、带 AirPortBrcmNIC 的 IO80211FamilyLegacy 1.0.0、HfsPlus 和 apfs_aligned。来源固定到提交，不跟随 latest/master；其中 Wi-Fi 可执行文件已与当前可用备份逐个比较一致。Apple 派生文件不上传本仓库；工具不写内置/USB ESP、不生成个人身份、不运行根补丁、不改 TCC、不重启。

需要同时缓存对应 fork 的 PatcherSupportPkg 1.9.6 镜像（948,923,392 字节）时：

```sh
python3 -B Tools/Reinstall/reinstall.py prepare --build 25C56 --accept-asset-licenses --with-root-support
```

依赖齐全后加 `--offline`，可不联网生成另一个暂存目录。缺文件或校验失败会停止，不自动覆盖坏缓存。这个目录以后即使填入个人身份也必须保持私有。SHA 相符只能证明字节一致，不能替代对上游来源及许可的判断。

暂存 config **仍然是公开 SMBIOS 占位值**。用 GenSMBIOS 生成自己的身份；没有 Ubuntu 时移除可选启动覆盖；用 ocvalidate 1.0.7 检查，再复制到 USB 测试。`assetsSha256Verified` 不代表 `bootQualified`。

## 3. 精确根补丁配方

固定定义来源：[非官方 Tahoe fork 提交 225a713](https://github.com/lzhoang2801/OpenCore-Legacy-Patcher/tree/225a713f0da5bedadaf8bc32b9b883d1692f9d64)。Patcher 3.0.0；该 fork 的 PatcherSupportPkg **1.9.6**；Apple 的 KDK **26.2 / 25C56**。不同项目即使版本号相同，也不能替换支持镜像；按 manifest 校验。

按固定源码的 `SOURCE.md`，用独立 Python 3.11 虚拟环境和经审阅的依赖构建/运行。不要让 patcher 的 OpenCore 构建功能覆盖这套自定义 EFI。任何一个同名 OpenCore-Patcher.app 都不能直接视为匹配版本；核对提交及 payload，不能绕过完整性/签名失败。此前本地归档的 app 有 Info.plist 签名错误，**本流程不会发布或自动运行它**。

选择 Post-Install Root Patch 前必须确认：

- `sw_vers` 是 26.2 / 25C56；KDK 的 SystemVersion.plist build 也完全相同。
- 启动安全策略与配对快照相同：SecureBootModel Disabled、SIP data `03080000`（0x803）、`amfi=0x80 -lilubetaall`、AMFIPass 禁用。这是降低安全性的兼容折中，不能独立切换 AMFI/TCC 设置。
- 检测计划**只有 Modern Wireless 和 Modern Audio**，没有不相关的显卡或其他 Mac 的补丁。

所需文件操作：

| 补丁 | 源目录选择 | 操作与目标 |
| --- | --- | --- |
| Modern Wireless | 13.7.2-25 | 覆盖 `/usr/libexec/wifip2pd` |
| Modern Wireless | 13.7.2-25 | 合并 IO80211.framework、WiFiPeerToPeer.framework 到 `/System/Library/PrivateFrameworks` |
| Modern Audio | 26.0 Beta 1 | 覆盖 `/System/Library/Extensions/AppleHDA.kext` |

让经过审阅的 patcher 处理 KDK 合并、所有权/签名、内核集合和 APFS 快照；仅复制文件不等于完成安装。保存完整日志。遇到 build 不符、额外补丁、内核缓存/快照失败或缺少依赖，应停止，不能反复盲装。

**目前尚未发布自动写根补丁的命令。**必须先在隔离环境演练新安装的硬件检测、补丁执行和恢复，才能声称全流程无人值守可复现。准备工具刻意不承担根补丁写入。

## 4. 冷启动、R4、声音和权限

保留配对的 MIC22X AppleALC、VerbStub 和 layout 22。根补丁完成后，完整关机并通过 USB 冷启动；此前只热重启曾导致输出无声。按[当前快照](CURRENT-SNAPSHOT.zh-CN.md)实际测试扬声器和麦克风。

用 `zsh Tools/OLEDWakeRescue/install.sh` 安装不变的 R4；测试唤醒前配置精确匹配的 HiDPI 模式。BetterDisplay 10-bit 是独立用户设置，仓库不分发该软件，也不会自动强制开启。

每个应用由该用户自己明确授权麦克风；不分发 TCC 数据库、个人应用授权或录音。如果应用不弹请求，单独诊断应用，不改 EFI。在系统设置中要求睡眠/锁屏后立即输入密码，并在关闭 Watch 解锁或锁定 Watch 后验收。

## 5. 验收不是只看文件存在

```sh
python3 -B Tools/Reinstall/reinstall.py audit
zsh Tools/verify-v61p5.sh
```

第一条只读核对四个根补丁可执行文件和补丁元数据，不代表整个 bundle 完整、网络/录音真的正常。随后手动测试 Wi-Fi 连接及上网、扬声器、麦克风电平与录音、耳机、触控板/触屏、冷启动、热重启、菜单睡眠、真实合盖睡眠、唤醒和密码锁屏。稳定版预期仍有约 8–11 秒绿色，随后 R4 恢复；不会自动让用户睡眠或重启。

USB 全部通过后，再由机主明确把准备好的 EFI 迁移到内置 ESP，并保留恢复 U 盘。失败时见[回滚](ROLLBACK.zh-CN.md)。系统更新会清除根补丁；必须使用经过明确验证的新 build 配置，不能仅删除旧版版本限制。
