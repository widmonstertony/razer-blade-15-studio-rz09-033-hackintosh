#!/bin/zsh
set -euo pipefail

rescue_label="com.tonytan.razer-oled-wake-rescue"
rescue_plist="$HOME/Library/LaunchAgents/$rescue_label.plist"
rescue_support_dir="$HOME/Library/Application Support/RazerOLEDWakeRescue"

/bin/launchctl bootout "gui/$UID" "$rescue_plist" >/dev/null 2>&1 || true
/bin/rm -f "$rescue_plist"
/bin/rm -f "$rescue_support_dir/DisplayModeNudgeV2"
/bin/rm -f "$rescue_support_dir/OLEDWakeRescueAgent"
/bin/rmdir "$rescue_support_dir" >/dev/null 2>&1 || true

print "Removed $rescue_label. Existing log files were kept in $HOME/Library/Logs."
