#!/bin/zsh
set -euo pipefail

verify_root="${0:A:h:h}"
verify_config="$verify_root/EFI/OC/config.plist"
verify_p5_aml="$verify_root/EFI/OC/ACPI/SSDT-SLPWAK.aml"
verify_rescue_dir="$HOME/Library/Application Support/RazerOLEDWakeRescue"
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

verify_value() {
	local verify_label="$1"
	local verify_expected="$2"
	shift 2
	local verify_actual
	verify_actual="$("$@" 2>/dev/null || true)"
	if [[ "$verify_actual" == "$verify_expected" ]]; then
		print "PASS: $verify_label"
	else
		print -u2 "FAIL: $verify_label (expected '$verify_expected', got '$verify_actual')"
		verify_failures=$((verify_failures + 1))
	fi
}

verify_sha256() {
	local verify_label="$1"
	local verify_file="$2"
	local verify_expected="$3"
	local verify_output verify_actual
	verify_output="$(/usr/bin/shasum -a 256 "$verify_file" 2>/dev/null || true)"
	verify_actual="${verify_output%% *}"
	if [[ "$verify_actual" == "$verify_expected" ]]; then
		print "PASS: $verify_label"
	else
		print -u2 "FAIL: $verify_label (expected '$verify_expected', got '$verify_actual')"
		verify_failures=$((verify_failures + 1))
	fi
}

verify_expect "macOS 26.2" test "$(/usr/bin/sw_vers -productVersion)" = "26.2"
verify_expect "build 25C56" test "$(/usr/bin/sw_vers -buildVersion)" = "25C56"
verify_expect "Darwin 25.2.x" test "${$(/usr/bin/uname -r):r}" = "25.2"
verify_expect "public config parses" /usr/bin/plutil -lint "$verify_config"
verify_expect "OLED kext is present in the repository" test -f "$verify_root/EFI/OC/Kexts/RazerOLEDWakeFix.kext/Contents/MacOS/RazerOLEDWakeFix"
verify_value "P5 SSDT is enabled" true /usr/libexec/PlistBuddy -c "Print :ACPI:Add:13:Enabled" "$verify_config"
verify_value "firmware _WAK rename is enabled" true /usr/libexec/PlistBuddy -c "Print :ACPI:Patch:6:Enabled" "$verify_config"
verify_value "active EC lid proxy is enabled" true /usr/libexec/PlistBuddy -c "Print :ACPI:Patch:8:Enabled" "$verify_config"
verify_value "active EC lid proxy is path-scoped" '\_SB.PCI0.LPCB.EC0.LID0' /usr/libexec/PlistBuddy -c "Print :ACPI:Patch:8:Base" "$verify_config"
verify_sha256 "P5 AML checksum" "$verify_p5_aml" e44cac48c80bf4637e93f22a118c4935ccda9e5cd17f62f6b953ee998fd42f62
verify_expect "OLED runtime service exists" /usr/sbin/ioreg -r -n RazerOLEDWakeFix
verify_expect "P5/P4 PCI9 ACPI device exists" /bin/zsh -c "/usr/sbin/ioreg -l -p IOACPIPlane | /usr/bin/grep -Fq 'PCI9@0'"
verify_expect "active lid is matched" /bin/zsh -c "/usr/sbin/ioreg -p IOACPIPlane -n LID0 -r | /usr/bin/grep -Fq 'registered, matched, active'"
verify_expect "clamshell sleep is enabled" /bin/zsh -c "/usr/sbin/ioreg -r -c IOPMrootDomain -l | /usr/bin/grep -Fq '\"AppleClamshellCausesSleep\" = Yes'"
verify_expect "system sleep is not globally disabled" /bin/zsh -c "/usr/sbin/ioreg -r -c IOPMrootDomain -l | /usr/bin/grep -Fq '\"SleepDisabled\" = No'"
verify_expect "wake rescue LaunchAgent is loaded" /bin/launchctl print "gui/$UID/com.tonytan.razer-oled-wake-rescue"

if [[ -x "$verify_rescue_dir/OLEDWakeRescueAgent" && -x "$verify_rescue_dir/DisplayModeNudgeV2" ]]; then
	verify_expect "installed R4 agent matches release" /usr/bin/cmp "$verify_root/Tools/OLEDWakeRescue/bin/OLEDWakeRescueAgent" "$verify_rescue_dir/OLEDWakeRescueAgent"
	verify_expect "installed R4 redraw helper matches release" /usr/bin/cmp "$verify_root/Tools/OLEDWakeRescue/bin/DisplayModeNudgeV2" "$verify_rescue_dir/DisplayModeNudgeV2"
	verify_expect "protected display probe" "$verify_rescue_dir/OLEDWakeRescueAgent" --probe
else
	print -u2 "FAIL: OLED wake rescue is not installed"
	verify_failures=$((verify_failures + 1))
fi

/usr/sbin/ioreg -r -n RazerOLEDWakeFix -l | /usr/bin/egrep \
	'VersionInfo|PatchStatus|V61PatchArmStatus|EDP14|IBoost' || true
/usr/sbin/system_profiler SPDisplaysDataType | /usr/bin/egrep \
	'Intel UHD Graphics 630|Framebuffer Depth|Resolution|Looks like' || true

if (( verify_failures != 0 )); then
	print -u2 "V61-P5/R4 verification failed: $verify_failures check(s)."
	exit 1
fi

print "V61-P5/R4 static/runtime checks passed. Menu sleep and a real physical-lid sleep/wake test are still required."
