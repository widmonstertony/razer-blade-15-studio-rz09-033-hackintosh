# Current working snapshot and MIC22X audio — 2026-10-09

[简体中文](CURRENT-SNAPSHOT.zh-CN.md)

This snapshot was exported from a new read-only physical ESP image. It preserves the working V61-P5/R4/HID V12 configuration and adds the exact MIC22X audio binary from that image. The complete image stays private. Files already in the redistributable EFI allowlist were compared against it; only AppleALC changed. Unused ACPI files, `.before-*`, `FSCK*`, Windows/Ubuntu boot data and private identity are deliberately not published. No physical EFI or installed system was changed during publication.

## Functional evidence and limits

| Item | Evidence |
| --- | --- |
| Speakers / microphone | Owner confirmed both after a complete shutdown and cold startup; CoreAudio shows Internal Speakers and Built-in Microphone |
| Codex dictation | Owner confirmed usable after a separate, explicitly approved application-permission workaround; not reproduced by copying EFI |
| UHD 630 / color | Online, Metal 3, 1680×945 HiDPI / 3360×1890 backing / 60 Hz, 30-bit ARGB2101010 |
| BCM4360 Wi-Fi | Connected on 5 GHz 802.11ac with existing exact-build root patches |
| Bluetooth | Controller on; fresh pairing/AirDrop/audio tests not performed |
| HID / OLED | Matched drivers loaded; P5 lid state, V61 service and exact R4 agent/helper passed checks; earlier owner-confirmed real wakes remain the sleep evidence |

This audit did not force a new sleep, restart, or application recording. Fresh warm restarts, camera, ports, external display, media keys, palm rejection, Watch unlock and Windows/Ubuntu boots are not newly qualified. The known 8–11-second green wake interval remains. A short CPU sample and no reported thermal throttling do not prove quiet fans or low temperatures.

## Reproduce the audio configuration

Follow [INSTALL.md](INSTALL.md) and [ROOT-PATCHES.md](ROOT-PATCHES.md) first, on a USB EFI and the exact 26.2 / 25C56 target. Do not overwrite a working internal ESP.

1. Use the included **matched MIC22X AppleALC**, not an arbitrary stock AppleALC with layout 22. Its Mach-O SHA-256 is `6f566fa8b58948520adb58472a3bee89a65f4f497a891922a346e6ddf2a0f3b1`.
2. Keep the published enabled AppleALC and VerbStub entries and HDEF `DeviceProperties > Add > PciRoot(0x0)/Pci(0x1f,0x3) > layout-id` data `16000000` (decimal 22). Do not mix the MIC47/B/X experiments with this pair or introduce a conflicting `alcid` boot argument.
3. Apply only the reviewed **exact-build Modern Audio / AppleHDA root patch** as described in the root-patch guide. EFI alone cannot supply Apple's missing audio driver. AppleHDA is not redistributed here.
4. Keep the working security policy. This snapshot preserves `-v keepsyms=1 debug=0x100 amfi=0x80 -lilubetaall`. Do not independently remove `amfi=0x80` or enable AMFIPass: that earlier TCC1 combination failed to boot with the existing root patches. This reduced-security state is a compatibility compromise, not a security recommendation.
5. After saving work, shut down fully, wait for complete power-off, then power on through the USB test EFI. A warm restart previously left output silent, whereas cold startup restored both input and output. This snapshot does **not** claim that every warm restart is fixed.
6. In Sound settings select Internal Speakers for output and Built-in Microphone for input. Check mute/volume, play a short sound, and speak while watching the input meter. Test headphones separately; device enumeration alone is not proof of sound.
7. For an application, trigger recording and approve its own microphone request under Privacy & Security. Hardware audio and application authorization are separate. The owner needed a nonstandard, consented, narrowly scoped permission workaround; this release does not ship an automatic authorization script or a TCC database. If an app never requests permission, diagnose that specific app/prompt policy instead of changing EFI or copying someone else's privacy database.
8. Test a separate warm restart, sleep/wake and lock-screen cycle before treating the USB candidate as fully accepted. Keep your own previous working audio configuration for rollback.

MIC22X keeps layout 22's independent microphone/line-input groups and uses layout 47's speaker/headphone paths: `[9,34,18]`, `[8,35,24]`, `[20,12,2]`, `[33,13,3]`, linked with PathMapID 212. See [matched resources and reproduction](../Sources/AppleALC-MIC22X/README.md).

## Keyboard lighting is separate

On the owner's existing `osx-razer-led` tool, brightness read **0** on 2026-10-09. Setting `brightness 160` and `wave left` changed the readback to 160/255, despite an auxiliary `Unable to create plugin (0xe00002be)` message. Readback is not a photograph or owner confirmation of the physical LEDs.

If you already have this same tool, run it from its own actual path with those arguments; it is not installed or distributed here. Do not change `fnkey` merely to restore lighting. This release does not install an automatic login/wake lighting service.

## Backup boundary

Keep a full private ESP backup **and** a restorable System/Data backup. GitHub contains a sanitized reproduction tree, not a byte-for-byte private recovery image. Supply your own SMBIOS and omitted Apple-derived files. User-space R4, root patches, BetterDisplay preferences and app permissions live outside the EFI and must be configured separately.
