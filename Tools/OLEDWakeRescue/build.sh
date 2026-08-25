#!/bin/zsh
set -euo pipefail

rescue_root="${0:A:h}"
rescue_source="$rescue_root/src"
rescue_output="$rescue_root/bin"
rescue_clang="$(/usr/bin/xcrun --sdk macosx --find clang)"

/bin/mkdir -p "$rescue_output"

"$rescue_clang" -std=c11 -O2 -Wall -Wextra \
	"$rescue_source/DisplayModeNudgeV2.c" \
	-framework CoreFoundation -framework CoreGraphics -lm \
	-o "$rescue_output/DisplayModeNudgeV2"

"$rescue_clang" -std=c11 -O2 -Wall -Wextra \
	"$rescue_source/OLEDWakeRescueAgent.c" \
	-framework CoreFoundation -framework IOKit -framework Security -lpthread \
	-o "$rescue_output/OLEDWakeRescueAgent"

for rescue_binary in DisplayModeNudgeV2 OLEDWakeRescueAgent
do
	/bin/chmod 0755 "$rescue_output/$rescue_binary"
	/usr/bin/codesign --force --sign - "$rescue_output/$rescue_binary"
	/usr/bin/codesign --verify --strict "$rescue_output/$rescue_binary"
done

print "Built and ad-hoc signed binaries in $rescue_output"
