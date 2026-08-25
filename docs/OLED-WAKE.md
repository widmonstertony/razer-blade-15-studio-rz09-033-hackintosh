# OLED wake design and validation

[简体中文](OLED-WAKE.zh-CN.md)

## The failure in plain language

The laptop itself woke from S3: input worked and the mouse cursor could move. The OLED link was also partly alive, but the desktop framebuffer was either black or a solid green surface. Immediately before a forced restart, the pre-sleep image could briefly appear. That evidence separated a dead computer from a display-link/redraw failure.

Linux could wake this exact panel, which proved that the firmware, panel rails and Intel display engine are physically capable of recovering. Comparing Linux and Windows sequences with Tahoe showed that this panel expects an eDP 1.4 rate-select transaction and specific source-side training state after S3. Tahoe's native Coffee Lake framebuffer path did not reproduce the complete sequence on this Razer.

## Two-layer solution

### Layer 1: `RazerOLEDWakeFix.kext` V61

The Lilu plug-in targets only `AppleIntelCFLGraphicsFramebuffer` on Darwin 25.2. It validates exact live instruction patterns before patching. On the native LinkTraining DPCD `0x100` write, it substitutes the panel-proven eDP 1.4 sequence: DPCD `0x115` rate selector 2 followed by the lane-count write at `0x101`. It also preserves the previously verified V60 panel-power, notification and dynamic Intel I_boost handling.

Every important eligibility, pattern or AUX failure falls back to Apple's original path. The OpenCore entry limits loading to `25.2.0`–`25.2.99`; this is a safety boundary, not an inconvenience.

### Layer 2: protected CoreGraphics redraw

V61 recovered a live display path but the desktop could remain a solid green framebuffer. A CoreGraphics display-mode transaction reliably forced redraw:

```text
1680×945 HiDPI / 3360×1890 / 60 Hz
          ↓ 0.75 seconds
1600×900 HiDPI / 3200×1800 / 60 Hz
          ↓ exact restore
1680×945 HiDPI / 3360×1890 / 60 Hz
```

`OLEDWakeRescueAgent` registers for real IOKit system-power notifications. Eight seconds after `kIOMessageSystemHasPoweredOn`, it launches the sibling helper. The helper refuses to run unless exactly one built-in Samsung vendor/product `19587/41001` display is online in the validated original mode and exactly one compatible rescue mode exists. It performs an app-only transaction and verifies exact restoration. The agent checks ownership, mode, regular-file status and code signature before every launch.

The service has no polling loop and does no display work while the machine is awake normally.

## Final validated result

Validation on 2026-08-24 used a real Normal Sleep/Wake, not display-only sleep:

- sleep/wake was recorded as Normal Sleep/Wake;
- wake source was the physical power button;
- V61 reported its eDP-rate selector intercept and verified selector 2 at DPCD `0x115`;
- six I_boost programming attempts were verified with zero refusals;
- initial, phase-1 and phase-2 training routes executed;
- lane clock-recovery/channel-equalization and BMU checkpoints passed;
- the automatic service ran after eight seconds and restored the exact original mode;
- the user confirmed a normal visible desktop;
- the post-wake framebuffer remained 30-bit (`ARGB2101010`).

The sanitized release records conclusions and expected counters, not raw logs containing machine identifiers or boot-session data.

## Runtime checks

```sh
ioreg -r -n RazerOLEDWakeFix -l | egrep \
  'VersionInfo|PatchStatus|V61PatchArmStatus|EDP14|IBoost'

launchctl print "gui/$UID/com.tonytan.razer-oled-wake-rescue"
tail -n 80 "$HOME/Library/Logs/RazerOLEDWakeRescue.log"
system_profiler SPDisplaysDataType | grep 'Framebuffer Depth'
```

Expected identifying properties include:

```text
VersionInfo = REL-450-2026-08-24
PatchStatus = applied-verified-s3-edp14-rate-select-v61
V61PatchArmStatus = all-v60-routes-plus-edp14-rate-selector-call-site-verified
```

Run `Tools/verify-v61p1.sh` for the complete static/runtime check.

## Known boundaries

- Exact Tahoe 26.2 framebuffer patterns are embedded. Another build must be treated as incompatible.
- The rescue mode is intentionally fixed to the tested HiDPI modes. A user selecting another normal resolution must update, review and rebuild the helper; it will otherwise fail closed.
- The LaunchAgent runs only in the logged-in Aqua session. It cannot repair a pre-login display problem.
- An external display does not make the internal-panel target generic.
- Hibernate/deep-standby behavior has not been generalized beyond the validated Normal Sleep/Wake test.
- The patch does not make physical OLED brightness control equivalent to a conventional LCD backlight.

## Source layout

- `Sources/RazerOLEDWakeFix/`: V61 kext source and a portable CLT build script.
- `Sources/HID-V12/`: patches against the documented VoodooI2C/VoodooI2CHID upstream commits.
- `Tools/OLEDWakeRescue/src/`: user-space helper/agent source.
- `Tools/OLEDWakeRescue/bin/`: exact tested x86_64 ad-hoc-signed binaries.
