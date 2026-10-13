# Multi-version OLED candidate — NOT a release

[简体中文](README.zh-CN.md)

Version 4.7.2 is included in this sanitized 26.7.1 NVIDIA-off backup. I observed one successful menu Sleep/Wake with the original DDGPU table enabled and unchanged R4; repeating with NVIDIA enabled failed. Read [my exact results and limits](../../docs/STATE-2026-10-12.md). This adds a bounded sleep-phase gate in `SleepPhaseGate.hpp`; 48 ASan/UBSan fixture cases preserve the legacy path and reject the observed premature restore. Repeated/lid wake and NVIDIA coexistence remain unqualified. Static tests alone do not prove wake. The 26.2 branch remains unchanged.

DriverProfiles.hpp uses a bounded Mach-O LC_UUID reader before hardware setup/patching. 25C56's UUID is 0A970005-966B-39C9-9D30-1E8F9BF6F86A. Its original V61 logic and 10 exact patterns/call offsets remain separate and unchanged. All 10 offline pattern/call/shape checks pass.

25G241's UUID is 7E294F0F-852E-3252-B54D-61D9A7BBE6B2. Its independent ten-pattern/seven-call implementation now passes KDK and shipping-code static checks; all seven explicit object fields have a native-code evidence map. It remains IsolatedValidationPending and rejects by default. The explicit research boot flag permits an isolated rehearsal, not production use. See [the complete port evidence and gates](PORT-25G241.md).

Unknown/malformed images, wrong Darwin, missing/non-unique patterns, unexpected call targets and out-of-range rel32 replacements refuse. UUID alone is not qualification. Refusal does not guarantee native OLED wake. The new candidate's loading/routing behavior, including its new safety preflight on 25C56, is not owner-validated just because the original V61 binary was.

## Build and test

Use Lilu 1.7.1 resources, MacKernelSDK commit 05094e5e88cec7caedbfb35e8449ed0db94bf95b and an x86_64-capable Apple SDK. Same environment overrides as [the original build](../RazerOLEDWakeFix/README.md): LILU_RESOURCES, MAC_KERNEL_SDK, MACOS_SDK. Run this directory's build-clt.sh with zsh. SDKs are not vendored.

From this directory:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined test_profiles.cpp -o /private/tmp/razer-oled-profile-tests
/private/tmp/razer-oled-profile-tests /path/to/legal/25C56/CFL /path/to/legal/25G241/CFL
python3 -B offline_audit.py RazerOLEDWakeFix.cpp /path/to/legal/25C56/CFL
```

Host tests cover legacy acceptance; pending port, wrong kernel, unknown UUID, truncation, missing/duplicate UUID and malformed load-command rejection. A compiler update need not produce identical bytes. These are offline checks, not loading/routing/sleep proof.

Next: deliberate new-build sleep/lock tests and repeated acceptance. Call targets and rel32 reachability are preflighted before byte writes; nine wrappers pass through until fully armed, but routing is not atomic. Never load alongside another bundle with the same identifier. Main is bounded to 25.6.x with its research flag; the old backup branch retains 25.2.x bounds. Custom code is MIT; upstream bootstrap/SDK licenses remain applicable.
