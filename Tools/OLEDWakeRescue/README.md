# OLED Wake Rescue

`OLEDWakeRescueAgent` waits for a real system-wake notification. Eight seconds later, `DisplayModeNudgeV2` performs a protected CoreGraphics mode switch and exact restore on the validated Samsung panel.

## Install the exact tested binaries

```sh
./Tools/OLEDWakeRescue/install.sh
```

The installer verifies macOS 26.2 build 25C56, validates both code signatures, probes the exact display without changing it, and loads a per-user LaunchAgent. It requires no administrator password.

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

`--probe` changes no display state. `--test-once` performs the protected mode transaction immediately and should be used only after a successful probe.

## Uninstall

```sh
./Tools/OLEDWakeRescue/uninstall.sh
```

Uninstall keeps existing logs for diagnostics.
