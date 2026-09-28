# OLED wake design and validation

[简体中文](OLED-WAKE.zh-CN.md)

## The failure in plain language

The laptop itself woke from S3: input worked and the mouse cursor could move. The OLED link was also partly alive, but the desktop framebuffer was either black or a solid green surface. Immediately before a forced restart, the pre-sleep image could briefly appear. That evidence separated a dead computer from a display-link/redraw failure.

Linux could wake this exact panel, which proved that the firmware, panel rails and Intel display engine are physically capable of recovering. Comparing Linux and Windows sequences with Tahoe showed that this panel expects an eDP 1.4 rate-select transaction and specific source-side training state after S3. Tahoe's native Coffee Lake framebuffer path did not reproduce the complete sequence on this Razer.

## Three-part solution

### Layer 1: `RazerOLEDWakeFix.kext` V61

The Lilu plug-in targets only `AppleIntelCFLGraphicsFramebuffer` on Darwin 25.2. It validates exact live instruction patterns before patching. On the native LinkTraining DPCD `0x100` write, it substitutes the panel-proven eDP 1.4 sequence: DPCD `0x115` rate selector 2 followed by the lane-count write at `0x101`. It also preserves the previously verified V60 panel-power, notification and dynamic Intel I_boost handling.

Every important eligibility, pattern or AUX failure falls back to Apple's original path. The OpenCore entry limits loading to `25.2.0`–`25.2.99`; this is a safety boundary, not an inconvenience.

### Part 2: P5 native-lid correction

The firmware exposes the active lid as `\_SB.PCI0.LPCB.EC0.LID0`. P5 enables `SSDT-SLPWAK.aml` with two paired, reviewed OpenCore renames: firmware `_WAK` becomes `ZWAK`, and only the active lid's `_LID` becomes `XLID`.

During a Darwin S3 wake, P5 temporarily reports the lid as open only while the original `ZWAK` method is executing. It then clears the temporary flag unconditionally, synchronizes the EC lid field to open and notifies the active lid once. Normal physical close/open events after wake call the original `XLID()` directly. This fixes the P4 failure in which a stale one-shot flag could survive wake and incorrectly consume the next real lid-close event.

### Part 3: R4 protected CoreGraphics redraw

V61 recovered a live display path but the desktop could remain a solid green framebuffer. A CoreGraphics display-mode transaction reliably forced redraw:

```text
1680×945 HiDPI / 3360×1890 / 60 Hz
          ↓ 0.75 seconds
1600×900 HiDPI / 3200×1800 / 60 Hz
          ↓ exact restore
1680×945 HiDPI / 3360×1890 / 60 Hz
```

`OLEDWakeRescueAgent` registers for real IOKit system-power notifications. Eight seconds after `kIOMessageSystemHasPoweredOn`, it launches the sibling helper. The helper refuses to run unless exactly one built-in Samsung vendor/product `19587/41001` display is online in the validated original mode and exactly one compatible rescue mode exists. R4 performs two `kCGConfigureForSession` transactions, restores the equivalent `usable=1` 1680×945 mode, and does not call the global ColorSync reset used by the older helper. The agent checks ownership, mode, regular-file status and code signature before every launch.

The service has no polling loop and does no display work while the machine is awake normally.

## Final validated result

V61/R4 validation on 2026-08-24 used a real Normal Sleep/Wake, not display-only sleep:

- sleep/wake was recorded as Normal Sleep/Wake;
- wake source was the physical power button;
- V61 reported its eDP-rate selector intercept and verified selector 2 at DPCD `0x115`;
- six I_boost programming attempts were verified with zero refusals;
- initial, phase-1 and phase-2 training routes executed;
- lane clock-recovery/channel-equalization and BMU checkpoints passed;
- the automatic service ran after eight seconds and restored the exact original mode;
- the user confirmed a normal visible desktop;
- the post-wake framebuffer remained 30-bit (`ARGB2101010`).

P5 physical-lid validation on 2026-09-27 added a separate real close/open proof:

- the display turned off and powerd recorded `Entering Sleep state due to 'Clamshell Sleep'`;
- the machine woke from `Normal Sleep` with the lock state preserved;
- R4 started at the configured eight-second boundary and returned `WAKE_RESCUE_PASS` after exact mode restoration;
- there was no second sleep, WindowServer/GPU diagnostic or rescue error;
- post-wake state was `AppleClamshellState=No`, `AppleClamshellCausesSleep=Yes`, `SleepDisabled=No`.
- the display audit immediately after that lid-wake reported `Framebuffer Depth: 24-Bit Color (ARGB8888)`;
- after the user enabled 10-bit in BetterDisplay 4.3.5, a later runtime audit reported `Framebuffer Depth: 30-Bit Color (ARGB2101010)`. CoreGraphics still exposed its legacy 8:8:8 mode-description string, so this release uses the active framebuffer report as the color-depth evidence. The override has not yet been re-proved across another sleep/wake cycle.

The user still saw a solid green framebuffer before R4 completed. In the stable build this transition lasts roughly 8–11 seconds. Experiments R5–R9 tried earlier triggering, brightness masking and final-output fading, but could report software success while the physical OLED remained permanently green. They are intentionally excluded from this release.

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

Run `Tools/verify-v61p5.sh` for the complete static/runtime check. It does not put the machine to sleep; the menu-sleep and physical-lid tests remain manual safety gates.

## Known boundaries

- Exact Tahoe 26.2 framebuffer patterns are embedded. Another build must be treated as incompatible.
- The rescue mode is intentionally fixed to the tested HiDPI modes. A user selecting another normal resolution must update, review and rebuild the helper; it will otherwise fail closed.
- The LaunchAgent runs only in the logged-in Aqua session. It cannot repair a pre-login display problem.
- A solid green framebuffer for about 8–11 seconds is expected before R4 redraw. This release favors repeatable recovery over hiding that interval.
- P5/R4 wake is usable at 24-bit. The currently observed 30-bit state requires a separate BetterDisplay 4.3.5 user-space override; it is not provided by EFI/V61 and must be rechecked after wake or a mode change.
- An external display does not make the internal-panel target generic.
- Hibernate/deep-standby behavior has not been generalized beyond the validated Normal Sleep/Wake test.
- The patch does not make physical OLED brightness control equivalent to a conventional LCD backlight.

## Source layout

- `Sources/RazerOLEDWakeFix/`: V61 kext source and a portable CLT build script.
- `Sources/HID-V12/`: patches against the documented VoodooI2C/VoodooI2CHID upstream commits.
- `ACPI-Sources/SSDT-SLPWAK.dsl`: P5 bounded native-lid source; the paired AML is in `EFI/OC/ACPI/`.
- `Tools/OLEDWakeRescue/src/`: user-space helper/agent source.
- `Tools/OLEDWakeRescue/bin/`: exact tested x86_64 ad-hoc-signed binaries.
