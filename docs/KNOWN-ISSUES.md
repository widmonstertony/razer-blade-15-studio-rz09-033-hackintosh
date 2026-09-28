# Known issues and boundaries

[简体中文](KNOWN-ISSUES.zh-CN.md)

1. **Exact OS build only.** V61 targets Tahoe 26.2 / Darwin 25.2.x. A newer macOS build is unsupported until the framebuffer binary, kernel patch and root patches are revalidated.
2. **Exact panel and mode only.** The automatic redraw supports Samsung `4C83:A029` at 1680×945 HiDPI / 3360×1890 / 60 Hz with a 1600×900 HiDPI rescue mode. It fails closed on any mismatch.
3. **User session required.** The LaunchAgent runs after login in the Aqua session; it cannot redraw a pre-login/FileVault screen.
4. **Brightness is not conventional backlight control.** OLED physical luminance behavior differs from LCD PWM/backlight control. Software dimming is a fallback and does not change the wake fix.
5. **Broadcom Wi-Fi and internal audio require exact-build root patches.** Apple-derived files are absent from this repository and system updates remove/rebuild the root-patched volume.
6. **Green wake interval remains.** The stable R4 build normally shows a solid green framebuffer for about 8–11 seconds, then redraws the locked desktop. R5–R9 masking experiments are excluded because they could leave the physical OLED permanently green despite successful API results.
7. **10-bit is a separate BetterDisplay state.** P5/R4 wake works at 24-bit. BetterDisplay 4.3.5 currently produces `30-Bit Color (ARGB2101010)`, but EFI/V61 does not force it and persistence across the next wake has not yet been re-proved. Verify the framebuffer after every wake or mode change.
8. **Menu and physical-lid Normal Sleep/Wake are validated; generalized hibernation is not.** Deep standby, hibernate images and every dock/external-display combination are outside the proven test.
9. **P5 is a paired ACPI set.** Enabling only the table or only one rename can break firmware method resolution. The active-lid rename must remain path-scoped to `\_SB.PCI0.LPCB.EC0.LID0`.
10. **The public EFI is sanitized.** It is not bootable until a unique SMBIOS and legally obtained omitted files are supplied.
11. **macOS may run warmer than Windows.** Razer EC fan policy is non-native, and scaled 4K HiDPI composition can make WindowServer busy. See `THERMALS.md`.

Do not “solve” a boundary by deleting version guards. Port and validate the affected component instead.
