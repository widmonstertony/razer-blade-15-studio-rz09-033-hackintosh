# Known issues and boundaries

1. **Exact OS build only.** V61 targets Tahoe 26.2 / Darwin 25.2.x. A newer macOS build is unsupported until the framebuffer binary, kernel patch and root patches are revalidated.
2. **Exact panel and mode only.** The automatic redraw supports Samsung `4C83:A029` at 1680×945 HiDPI / 3360×1890 / 60 Hz with a 1600×900 HiDPI rescue mode. It fails closed on any mismatch.
3. **User session required.** The LaunchAgent runs after login in the Aqua session; it cannot redraw a pre-login/FileVault screen.
4. **Brightness is not conventional backlight control.** OLED physical luminance behavior differs from LCD PWM/backlight control. Software dimming is a fallback and does not change the wake fix.
5. **Broadcom Wi-Fi and internal audio require exact-build root patches.** Apple-derived files are absent from this repository and system updates remove/rebuild the root-patched volume.
6. **Normal Sleep/Wake is validated; generalized hibernation is not.** Deep standby, hibernate images and every dock/external-display combination are outside the proven test.
7. **The public EFI is sanitized.** It is not bootable until a unique SMBIOS and legally obtained omitted files are supplied.
8. **macOS may run warmer than Windows.** Razer EC fan policy is non-native, and 4K 10-bit HiDPI composition can make WindowServer busy. See `THERMALS.md`.

Do not “solve” a boundary by deleting version guards. Port and validate the affected component instead.
