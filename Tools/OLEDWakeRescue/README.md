# OLED Wake Rescue

`OLEDWakeRescueAgent` waits for a real system-wake notification. Eight seconds later, the validated R4 `DisplayModeNudgeV2` performs two session-scoped CoreGraphics mode transactions and restores the exact usable mode on the validated Samsung panel.

The stable result deliberately allows the panel's solid-green wake framebuffer to remain visible for roughly 8–11 seconds before redraw. Do not replace R4 with the abandoned R5–R9 brightness/fade masking experiments; on this hardware they could report API success while the physical OLED remained permanently green.

## Install the exact tested binaries

```sh
./Tools/OLEDWakeRescue/install.sh
```

The installer verifies macOS 26.2 build 25C56, validates both code signatures, probes the exact display without changing it, and loads a per-user LaunchAgent. It requires no administrator password. It does not modify EFI; install the paired V61-P5 EFI separately through the USB-first procedure.

## Build locally

```sh
xcode-select --install
./Tools/OLEDWakeRescue/build.sh
./Tools/OLEDWakeRescue/install.sh
```

The build script uses the active macOS SDK and produces x86_64, ad-hoc-signed binaries in `bin/`. Source and binaries should be reviewed before installation.

## Test and logs

```sh
"$HOME/Library/Application Support/RazerOLEDWakeRescue/OLEDWakeRescueAgent" --probe
tail -n 80 "$HOME/Library/Logs/RazerOLEDWakeRescue.log"
launchctl print "gui/$UID/com.tonytan.razer-oled-wake-rescue"
```

`--probe` changes no display state. `--test-once` performs the protected mode transaction immediately and should be used only after a successful probe. A normal result contains `NUDGE_V3_PASS` even though the public helper filename remains `DisplayModeNudgeV2` for compatibility with the installed LaunchAgent.

## Uninstall

```sh
./Tools/OLEDWakeRescue/uninstall.sh
```

Uninstall keeps existing logs for diagnostics.
