# Known issues and test plan

## 1. OLED brightness

The baseline injects `enable-backlight-registers-fix`. WhateverGreen documents that the legacy BLR route no longer works on Kaby/Coffee Lake framebuffer drivers shipped with macOS 13.4 and newer. The intended first test replaces it with:

```text
enable-backlight-registers-alternative-fix = 01000000
```

This may restore the Intel framebuffer register path. It cannot guarantee physical OLED luminance control because the panel has no conventional LCD backlight.

If physical control remains unavailable, software dimming with MonitorControl or BetterDisplay is the fallback. Software dimming changes rendered luminance and is not the same as reducing panel drive current.

## 2. Black screen after wake

The staged test patch contains three changes:

1. Enable `SSDT-SLPWAK.aml`.
2. Enable its matching `_WAK` → `ZWAK` ACPI rename.
3. Add `igfxonln=1` to `boot-args` so WhateverGreen forces the internal framebuffer online after wake.

The existing wake SSDT was disassembled and audited. It preserves the firmware `ZWAK` return value and sends status-change notifications to common Razer lid-device paths after S3 wake.

## Test order

1. Boot from a removable test EFI where possible.
2. Test native brightness keys and the macOS display slider.
3. Run display-only sleep and wake.
4. Use Apple menu sleep for 30 seconds, then wake with the power button.
5. Test lid close/open last.
6. Record `pmset -g log` and the previous shutdown cause after any failure.

## Secondary diagnostics

If the first patch does not solve wake:

- test with `SSDT-TB3HP.aml` disabled and Thunderbolt disabled in firmware;
- verify all internal USB ports remain marked internal;
- inspect whether the NVIDIA dGPU powers back on after S3;
- test CpuTscSync only if the whole machine freezes rather than only the display;
- temporarily disable deep standby/autopoweroff to separate S3 wake from hibernation.
