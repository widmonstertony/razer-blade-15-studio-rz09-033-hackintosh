#!/bin/zsh
set -euo pipefail
reinstall_root="${0:A:h:h}"
cd "$reinstall_root"
print "准备重装依赖 / Prepare reinstall dependencies"
print "只下载并校验固定版本依赖，生成私人工作目录；不写 EFI、不安装根补丁、不重启。"
print "Only creates a private staging directory. No ESP/root/TCC writes or restart."
print "阅读 docs/REINSTALL.zh-CN.md 或 docs/REINSTALL.md 和各依赖授权后，输入 YES 继续。"
read -r "reinstall_answer?Type YES after reviewing the guide and asset licenses: "
if [[ "$reinstall_answer" != YES ]]; then
    print "Cancelled / 已取消"
    exit 0
fi
if ! command -v python3 >/dev/null 2>&1; then
    print -u2 "Python 3 is required. This tool does not install runtimes automatically."
    exit 69
fi
python3 -B Tools/Reinstall/reinstall.py prepare --build 25G241 --accept-asset-licenses
print "准备完成不等于可启动。仍需个人 SMBIOS、精确 KDK、根补丁与 USB 验收。"
print "Preparation is not boot qualification. Complete identity, KDK, root patches and USB tests."
read -r "reinstall_done?Press Return to close / 按回车关闭: "
