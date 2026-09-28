#!/bin/zsh
set -euo pipefail

razer_source_dir="${0:A:h}"
razer_repo_dir="${razer_source_dir:h:h}"
razer_lilu_sdk="${LILU_RESOURCES:-$razer_repo_dir/vendor/Lilu.kext/Contents/Resources}"
razer_kernel_sdk="${MAC_KERNEL_SDK:-$razer_repo_dir/vendor/MacKernelSDK}"
razer_macos_sdk="${MACOS_SDK:-$(/usr/bin/xcrun --sdk macosx --show-sdk-path)}"
razer_build_dir="$razer_source_dir/build"
razer_kext="$razer_build_dir/RazerOLEDWakeFix.kext"
razer_objects="$razer_build_dir/objects"

for razer_required in \
	"$razer_lilu_sdk/Headers/kern_api.hpp" \
	"$razer_source_dir/RazerOLEDWakeFix_plugin_start.cpp" \
	"$razer_kernel_sdk/Headers/mach/mach_types.h" \
	"$razer_kernel_sdk/Library/x86_64/libkmod.a"
do
	[[ -e "$razer_required" ]] || { echo "Missing build dependency: $razer_required"; exit 1; }
done

/bin/mkdir -p "$razer_objects" "$razer_kext/Contents/MacOS"
/bin/rm -f \
	"$razer_objects/RazerOLEDWakeFix.o" \
	"$razer_objects/plugin_start.o" \
	"$razer_objects/RazerOLEDWakeFix_info.o" \
	"$razer_kext/Contents/MacOS/RazerOLEDWakeFix" \
	"$razer_kext/Contents/Info.plist"

razer_common=(
	-target x86_64-apple-macos10.6
	-nostdinc
	-fno-builtin
	-fno-common
	-mkernel
	-Os
	-g
	-DKERNEL
	-DKERNEL_PRIVATE
	-DDRIVER_PRIVATE
	-DAPPLE
	-DNeXT
	-DMODULE_VERSION=4.5.0
	-DPRODUCT_NAME=RazerOLEDWakeFix
	-isysroot "$razer_macos_sdk"
	-I "$razer_kernel_sdk/Headers"
)

/usr/bin/clang++ -x c++ -std=c++17 -fno-exceptions -fno-rtti -fapple-kext \
	-fno-asynchronous-unwind-tables -I "$razer_lilu_sdk" "${razer_common[@]}" \
	-c "$razer_source_dir/RazerOLEDWakeFix.cpp" -o "$razer_objects/RazerOLEDWakeFix.o"

/usr/bin/clang++ -x c++ -std=c++17 -fno-exceptions -fno-rtti -fapple-kext \
	-fno-asynchronous-unwind-tables -I "$razer_lilu_sdk" "${razer_common[@]}" \
	-c "$razer_source_dir/RazerOLEDWakeFix_plugin_start.cpp" -o "$razer_objects/plugin_start.o"

/usr/bin/clang -x c -std=gnu11 "${razer_common[@]}" \
	-c "$razer_source_dir/RazerOLEDWakeFix_info.c" -o "$razer_objects/RazerOLEDWakeFix_info.o"

/usr/bin/clang++ -target x86_64-apple-macos10.6 -isysroot "$razer_macos_sdk" -Os \
	-L "$razer_kernel_sdk/Library/x86_64" -nostdlib -static -Xlinker -kext \
	-lkmodc++ -lkmod -lcc_kext \
	"$razer_objects/RazerOLEDWakeFix.o" \
	"$razer_objects/plugin_start.o" \
	"$razer_objects/RazerOLEDWakeFix_info.o" \
	-o "$razer_kext/Contents/MacOS/RazerOLEDWakeFix"

/usr/bin/ditto "$razer_source_dir/Info.plist" "$razer_kext/Contents/Info.plist"
/bin/chmod 0755 "$razer_kext/Contents/MacOS/RazerOLEDWakeFix"
/usr/bin/plutil -lint "$razer_kext/Contents/Info.plist"

print "$razer_kext"
