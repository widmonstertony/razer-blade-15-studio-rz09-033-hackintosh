#!/bin/zsh
set -euo pipefail

rescue_root="${0:A:h}"
rescue_source="$rescue_root/bin"
rescue_support_dir="$HOME/Library/Application Support/RazerOLEDWakeRescue"
rescue_agent="$rescue_support_dir/OLEDWakeRescueAgent"
rescue_helper="$rescue_support_dir/DisplayModeNudgeV2"
rescue_label="com.tonytan.razer-oled-wake-rescue"
rescue_plist="$HOME/Library/LaunchAgents/$rescue_label.plist"
rescue_temp_plist=""

rescue_cleanup() {
	[[ -n "$rescue_temp_plist" ]] && /bin/rm -f "$rescue_temp_plist"
}
trap rescue_cleanup EXIT

if [[ "$(/usr/bin/sw_vers -productVersion)" != 26.2 || \
      "$(/usr/bin/sw_vers -buildVersion)" != 25C56 || \
      "$(/usr/bin/uname -r)" != 25.2.* ]]; then
	print -u2 "Refusing installation: V61-P5/R4 is validated only on macOS 26.2 (25C56), Darwin 25.2.x."
	exit 65
fi

for rescue_binary in DisplayModeNudgeV2 OLEDWakeRescueAgent
do
	[[ -f "$rescue_source/$rescue_binary" ]] || {
		print -u2 "Missing binary: $rescue_source/$rescue_binary"
		exit 66
	}
	/usr/bin/codesign --verify --strict "$rescue_source/$rescue_binary"
done

"$rescue_source/OLEDWakeRescueAgent" --probe

/usr/bin/install -d -m 0755 "$rescue_support_dir" "$HOME/Library/LaunchAgents" "$HOME/Library/Logs"
/usr/bin/install -m 0755 "$rescue_source/DisplayModeNudgeV2" "$rescue_helper"
/usr/bin/install -m 0755 "$rescue_source/OLEDWakeRescueAgent" "$rescue_agent"

/usr/bin/codesign --verify --strict "$rescue_helper"
/usr/bin/codesign --verify --strict "$rescue_agent"

rescue_temp_plist="$(/usr/bin/mktemp /private/tmp/razer-oled-wake-rescue.XXXXXX)"
/usr/bin/plutil -create xml1 "$rescue_temp_plist"
/usr/bin/plutil -insert Label -string "$rescue_label" "$rescue_temp_plist"
/usr/bin/plutil -insert ProgramArguments -xml "<array><string>$rescue_agent</string><string>--run</string></array>" "$rescue_temp_plist"
/usr/bin/plutil -insert RunAtLoad -bool true "$rescue_temp_plist"
/usr/bin/plutil -insert KeepAlive -bool true "$rescue_temp_plist"
/usr/bin/plutil -insert ProcessType -string Interactive "$rescue_temp_plist"
/usr/bin/plutil -insert LimitLoadToSessionType -string Aqua "$rescue_temp_plist"
/usr/bin/plutil -insert LowPriorityIO -bool true "$rescue_temp_plist"
/usr/bin/plutil -insert ThrottleInterval -integer 10 "$rescue_temp_plist"
/usr/bin/plutil -insert EnvironmentVariables -xml '<dict><key>PATH</key><string>/usr/bin:/bin:/usr/sbin:/sbin</string></dict>' "$rescue_temp_plist"
/usr/bin/plutil -insert StandardOutPath -string "$HOME/Library/Logs/RazerOLEDWakeRescue.log" "$rescue_temp_plist"
/usr/bin/plutil -insert StandardErrorPath -string "$HOME/Library/Logs/RazerOLEDWakeRescue.error.log" "$rescue_temp_plist"
/usr/bin/install -m 0644 "$rescue_temp_plist" "$rescue_plist"
/bin/rm -f "$rescue_temp_plist"
rescue_temp_plist=""

/bin/launchctl bootout "gui/$UID" "$rescue_plist" >/dev/null 2>&1 || true
/bin/launchctl bootstrap "gui/$UID" "$rescue_plist"
/bin/launchctl kickstart -k "gui/$UID/$rescue_label"
/bin/launchctl print "gui/$UID/$rescue_label" | /usr/bin/head -n 35

print "Installed $rescue_label. No administrator password was required."
