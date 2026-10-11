# 当前 26.7.1 配置、重建和验收

[English](MAIN-26.7.1.md)

这是 2026-10-11 的 Intel 主显示基线，**不是 NVIDIA 安装包**。旧 26.2 版本保留在[备份分支](https://github.com/widmonstertony/razer-blade-15-studio-rz09-033-hackintosh/tree/backup/macos-26.2-v61p5-mic22x-20261011)。必须选择匹配系统的分支。EFI/补丁备份不是完整 macOS 或个人数据备份，旧分支不能给已升级系统降级。

## 精确目标和限制

仅限 RZ09-0330Q / BIOS 1.06 / i7-10875H / Intel `8086:9bc4` / PCH `8086:068d` / Samsung OLED `4c83:a029` SDCA029 / BCM4360 `14e4:43a0` / ALC298。系统 **26.7.1、25G241、Darwin 25.6.x**，OpenCore 1.0.7。其他面板、BIOS、设备和系统版本不保证；要检查实际硬件，不能只看伪装的 MacBookPro16,1。

机主冷启动后确认：Wi-Fi、音频、触控板。OLED 4.7.1 的补丁路由及 IOSleep 跳板检查通过，R4 已运行。新版的**菜单/合盖唤醒、锁屏、重复循环、耳机、蓝牙及所有 App 麦克风仍需单独验收**。旧 26.2 的绿屏后重画成功只是历史证据。完整全新重装尚未演练。

公开 kext 副本使用 Apple `strip -S` 移除了本机编译路径/调试元数据；所有非调试区段及外部符号/地址列表均与私人实机已加载版本逐项一致，因此公开哈希会不同。公开去调试副本仍需使用者 USB 启动验收，见[二进制脱敏证据](PUBLIC-BINARY-SANITIZATION.json)。发布处理没有改实体 EFI。

## 1. 制作自己的 USB EFI

1. 将个人数据和完整私人 ESP 备份到独立存储，确认恢复 USB 能启动、Windows 能从固件直接启动。公开仓库没有个人身份和 Windows/Ubuntu 启动状态。
2. 合法取得精确系统版本。在添加私人身份/依赖**之前**运行 `shasum -a 256 -c SHA256SUMS.txt` 校验仓库。
3. 阅读第三方授权，双击 `Tools/Prepare-Reinstall.command`，或在仓库根目录运行：

   ```sh
   python3 -B Tools/Reinstall/reinstall.py prepare --build 25G241 --accept-asset-licenses --with-root-support
   ```

   只在 `.reinstall-private/` 生成私人暂存 EFI，下载固定哈希的 IOSkywalkFamily、IO80211FamilyLegacy/AirPortBrcmNIC、HfsPlus、apfs_aligned 及可选根补丁依赖。不写 ESP、不安装根补丁、不设置身份；缺失/损坏就拒绝。不要提交这个目录。
4. 暂存配置的 MLB、ROM、SystemSerialNumber、SystemUUID 换成你独有的 MacBookPro16,1 身份；已有系统保留自己的身份。无 Ubuntu 就移除可选 BlessOverride。不要复制他人的 BCD、序列号或完整 EFI 镜像。
5. 用 OpenCore **1.0.7** 的 `ocvalidate` 和 `plutil -lint` 校验暂存配置，放到另行准备的 FAT32 USB，再从固件启动。擦除/写盘前必须确认精确目标；这些准备工具不会替你擦盘或写盘。

公开 main 仍启用 SSDT-DDGPU 关闭独显，Intel 属性不变；无线 Add/Block 成套限定 25.6.0–25.6.99，OLED 也限定该范围。因睡眠未验收，保留 `-razer-oled25g241-test` 明确标记。未知 CFL 指纹拒绝打补丁；拒绝不代表原生唤醒一定成功。

## 2. 恢复 Wi-Fi 和音频根补丁

全新安装首轮使用 USB 手机共享/有线网络。必须使用 **KDK 26.7.1 / 25G241**，不允许相近版本替代。KDK 镜像 SHA256：`9045a521c8ae362d4cb2cc54701b03462686e0069518a0b66cfe2028f87dfe9b`。

固定的**非官方** patcher 源码：[225a713f0da5bedadaf8bc32b9b883d1692f9d64](https://github.com/lzhoang2801/OpenCore-Legacy-Patcher/tree/225a713f0da5bedadaf8bc32b9b883d1692f9d64)，版本 3.0.0，fork support 包 1.9.6。阅读其 `SOURCE.md` 和依赖，在独立 Python 3.11 环境构建/运行固定源码。不能换成任意同名 App 或其他 fork 的依赖；不要运行它的 OpenCore builder 覆盖定制 EFI。公开准备工具**不是自动根补丁写入器**；源码构建、权限授权和实际应用仍是手动、需审查步骤。

检测计划必须**恰好只有 Modern Wireless 和 Modern Audio**：

| 补丁 / 来源 | 系统操作 |
| --- | --- |
| Modern Wireless / `13.7.2-25` | 替换 `/usr/libexec/wifip2pd`；在 `/System/Library/PrivateFrameworks` 合并 `IO80211.framework` 和 `WiFiPeerToPeer.framework` |
| Modern Audio / `26.0 Beta 1` | 替换 `/System/Library/Extensions/AppleHDA.kext` |

让审查过的引擎执行精确 KDK 合并、权限处理、内核集合重建、APFS 系统快照；复制几个文件不够。出现额外补丁、KDK 回退、完整性/KC/快照失败或版本不符即停止。保存日志与元数据，必须注明 25G241、精确 KDK 和提交。机主现有升级系统已完成这些步骤，别人的全新安装仍须验收。

保留配套安全设置：SecureBootModel Disabled、SIP 字节 `03080000`（0x803）、`amfi=0x80 -lilubetaall`、AMFIPass **禁用**。这是降低安全性的兼容方案。不要重试 TCC1/AMFIPass 实验、单独删 AMFI 参数或分发 TCC 数据库。Python 源码需要权限时，正常授权审查过的操作，不要绕过/替换特权 helper 的签名检查。

完整关机，从 USB 冷启动。MIC22X AppleALC、layout 22 和 VerbStub 必须成套。验证实际录音和扬声器发声，不要只看设备存在/电平移动；此前热重启偶尔扬声器无声，不能只靠重启验收。每个 App 的麦克风权限由其用户在系统设置中单独授权，不是 EFI 设置。

## 3. R4 和明确的 OLED 测试

Samsung 内屏设置为 1680×945 HiDPI / 实际 3360×1890 / 60 Hz，同时提供 1600×900 HiDPI。原有 R4 只探测这些精确模式。BetterDisplay 10-bit 是可选用户设置，不是 EFI 开关；变更模式/唤醒后复查 30-bit `ARGB2101010`。

只读检查：

```sh
python3 -B Tools/Reinstall/reinstall.py audit
zsh Tools/verify-v61p5.sh
ioreg -r -c RazerOLEDWakeFix -l -w0
```

必须看到版本 `REL-471-2026-10-11`、`applied-verified-s3-edp14-rate-select-v61`、`all-v60-routes-plus-edp14-rate-selector-call-site-verified`、`verified-single-kc-branch-stub-to-canonical-IOSleep`。仅 kext 出现在列表中不算成功。

明确接受新版唤醒未验收后：

```sh
zsh Tools/OLEDWakeRescue/install.sh --accept-unvalidated-wake
```

安装器检查精确系统/运行状态、签名和显示模式，不会自动睡眠/重启，不要管理员权限。R4 只在真实唤醒约 8 秒后重画，其他时间空闲。不要恢复 R5–R9 或自动重启守护实验。

保存工作，设置睡眠后立即密码锁定，锁定 Watch/停用 Watch 解锁，分别明确测试一次菜单 Normal Sleep/Wake 和物理合盖循环。可观察旧版绿色重画过渡，但必须最终有可用桌面、不再次入睡、输入/网络/音频正常、真实锁屏；不承诺新版时长/稳定性。不要用强制睡眠/重启循环追求成功。USB 验收通过后才迁到内置 ESP。

## 4. 恢复与后续更新

PCI-only 实验失败时恢复 **PCI 实验前的 26.7.1** 配置/EFI，不要把旧 26.2 配置套到 26.7.1；保留 Windows/Ubuntu。回到 26.2 还需要匹配的系统/根补丁、真正系统备份或重装。仅放在内置盘的备份无法应对硬盘损坏，也不保证无法启动时能取出。

系统更新会替换根补丁文件。另行适配并验证精确 OS/KDK/CFL 配置，不能只放宽 Darwin 范围。OLED 源码及拒绝未知输入的检查见 [MultiVersion](../Sources/RazerOLEDWakeFix-MultiVersion/README.zh-CN.md)。NVIDIA 实验和更严格的成功标准见[单独说明](NVIDIA-PCI-ONLY.zh-CN.md)。
