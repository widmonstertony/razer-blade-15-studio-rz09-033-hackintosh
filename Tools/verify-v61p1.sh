#!/bin/zsh
set -euo pipefail

verify_root="${0:A:h:h}"
verify_config="$verify_root/EFI/OC/config.plist"
verify_failures=0

verify_expect() {
	local verify_label="$1"
	shift
	if "$@" >/dev/null 2>&1; then
		print "PASS: $verify_label"
	else
		print -u2 "FAIL: $verify_label"
		verify_failures=$((verify_failures + 1))
	fi
}

verify_expect "macOS 26.2" test "$(/usr/bin/sw_vers -productVersion)" = "26.2"
verify_expect "build 25C56" test "$(/usr/bin/sw_vers -buildVersion)" = "25C56"
verify_expect "Darwin 25.2.x" test "${$(/usr/bin/uname -r):r}" = "25.2"
verify_expect "public config parses" /usr/bin/plutil -lint "$verify_config"
verify_expect "OLED kext is present in the repository" test -f "$verify_root/EFI/OC/Kexts/RazerOLEDWakeFix.kext/Contents/MacOS/RazerOLEDWakeFix"
verify_expect "OLED runtime service exists" /usr/sbin/ioreg -r -n RazerOLEDWakeFix
verify_expect "wake rescue LaunchAgent is loaded" /bin/launchctl print "gui/$UID/com.tonytan.razer-oled-wake-rescue"

if [[ -x "$HOME/Library/Application Support/RazerOLEDWakeRescue/OLEDWakeRescueAgent" ]]; then
	verify_expect "protected display probe" "$HOME/Library/Application Support/RazerOLEDWakeRescue/OLEDWakeRescueAgent" --probe
else
	print -u2 "FAIL: OLED wake rescue is not installed"
	verify_failures=$((verify_failures + 1))
fi

/usr/sbin/ioreg -r -n RazerOLEDWakeFix -l | /usr/bin/egrep \
	'VersionInfo|PatchStatus|V61PatchArmStatus|EDP14|IBoost' || true
/usr/sbin/system_profiler SPDisplaysDataType | /usr/bin/egrep \
	'Intel UHD Graphics 630|Framebuffer Depth|Resolution|Looks like' || true

if (( verify_failures != 0 )); then
	print -u2 "V61-P1 verification failed: $verify_failures check(s)."
	exit 1
fi

print "V61-P1 static/runtime checks passed. A real sleep/wake cycle is still required."
