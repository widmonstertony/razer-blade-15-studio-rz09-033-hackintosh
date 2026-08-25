# Thermals and power

[简体中文](THERMALS.zh-CN.md)

macOS is not automatically cooler or quieter on a Razer laptop. Apple tunes fan curves, CPU power limits and display power for Apple hardware. This Blade uses a Razer embedded controller (EC), a disabled NVIDIA dGPU, an Intel iGPU driving a 4K 10-bit OLED, and non-native ACPI/SMC reporting.

## Evidence from the validated machine

A read-only sample on 2026-08-24 found:

- only Intel UHD 630 was exposed as an active display GPU; NVIDIA was absent from `system_profiler SPDisplaysDataType`;
- the screen was using the required 10-bit path and a large HiDPI backing surface;
- `WindowServer` briefly used about 32% CPU;
- active Codex renderer/service work and Continuity Capture also contributed load;
- a later two-sample reading was about 89% CPU idle, so the CPU was not stuck at full load;
- BetterDisplay had active graphics clients;
- AC `displaysleep` and `sleep` were both 0, so automatic screen/system sleep was disabled;
- the OLED rescue agent was event-driven and idle; it was not a continuous CPU consumer.

This points first to display composition and current applications, not proof of failed CPU power management. Temperature still needs a 10-minute idle comparison because a one-second process snapshot cannot prove the long-term cause.

## Why 4K 10-bit HiDPI is warm

At “looks like 1680×945,” macOS renders a 3360×1890 backing surface and composites it for the 3840×2160 panel. The 30-bit framebuffer moves more data than an 8-bit path. Window transparency, video, browser animation and BetterDisplay can keep the iGPU/display engine busy. The OLED panel also consumes more power on bright content.

V61-P1 keeps 10-bit output by design. Reducing heat does not require changing to 8-bit.

## Safe optimization order

1. Let the machine sit untouched for ten minutes, then check Activity Monitor's CPU and Energy tabs.
2. Temporarily quit BetterDisplay and compare `WindowServer` usage and temperature for five minutes. Keep the 1680×945 HiDPI mode during the comparison.
3. Close heavy browser/video/Codex workloads and disable Continuity Camera when it is not being used.
4. Use Dark Mode and lower rendered brightness; dark OLED content reduces panel power without changing 10-bit framebuffer depth.
5. Re-enable sensible AC idle timers if desired:

   ```sh
   sudo pmset -c displaysleep 10 sleep 30 disksleep 10 powernap 0 womp 0
   ```

   This changes only AC-power policy. Verify one automatic sleep/wake after changing it. Use `pmset -g custom` to record the previous values first.
6. If lower performance is acceptable, test Low Power Mode from System Settings before changing EFI CPU data. Revert it if animation becomes less smooth.
7. In Windows, update Razer firmware and verify the stock fan/EC behavior. Do not flash unofficial EC firmware or write arbitrary fan registers from macOS.

Do not install random CPUFriend profiles, disable Turbo Boost permanently, or modify PL1/PL2 merely to make the chassis feel cooler. First record idle temperature, package power and the top processes; otherwise the change can hide a broken dGPU or display-power path.

## Verify the dGPU remains off

```sh
system_profiler SPDisplaysDataType
ioreg -l -w0 -r -c IOPCIDevice | grep -iE 'nvidia|geforce|quadro|10de'
```

The NVIDIA GPU must not appear as an active macOS display device. If it reappears or battery discharge remains high at idle, stop and audit `SSDT-DDGPU.aml`, its config entry and BIOS state before applying CPU limits.

## About a reported “60 W” charger

The Blade's high-power barrel adapter is managed by the Razer EC, not by Apple's native USB-C charger stack. On this installation macOS reported only `Connected: Yes` and no reliable wattage. A displayed 60 W value can therefore be an ACPI/SMC compatibility placeholder and does not prove that a 330 W supply is electrically limited to 60 W.

The practical checks are whether the genuine adapter is recognized by the Razer firmware, whether the battery discharges during a sustained combined CPU/GPU load in Windows, and whether the connector/cable overheats. If those fail, diagnose the adapter separately; do not “fix” it by editing macOS wattage labels.
