# Quadro app-only Metal: real evidence, optional startup, exact limits

[简体中文](NVIDIA-APP-ONLY.zh-CN.md)

## Result as of 2026-10-11

Exact owner machine: RZ09-0330Q / BIOS 1.06 / i7-10875H / Intel UHD 630 `8086:9bc4` / Quadro RTX 5000 Max-Q TU104 `10de:1eb5`, subsystem `1a58:200a` / Samsung OLED `4c83:a029` / macOS 26.7.1 `25G241`.

The owner-local driver installation is complete. One manually authorized GSP/RM initialization succeeded and returned a real 16 GiB VRAM query. A subsequent one-shot Metal suite explicitly selected Quadro PCI `10de:1eb5`, not Intel: three runs of 65,536 integers and all 4,096 offscreen RGBA pixels were correct. Independent post-checks retained Intel primary 10-bit, installed bytes/signatures, active collections and security; no new kernel/WindowServer crash-report filenames were found. These are small correctness results, not full stability or performance qualification.

The optional all-app access/startup overlay is prepared and CPU/failure-tested. Owner installation authentication and its next cold-boot acceptance are pending. Check [STATUS-26.7.1.json](STATUS-26.7.1.json) for updates. Default public `EFI/` stays the Intel-primary NVIDIA-off baseline.

## What “all applications” means

The overlay opens the NVMTL Metal device to processes generally, while denying WindowServer **before** the wildcard. Its existing headless guard additionally denies WindowServer before any environment override. Intel still owns the internal panel; no NVIDIA framebuffer/AGDC/Intel-override kext is installed.

This makes a compatible Metal device available; it does not force applications to choose it or implement missing APIs/features. A program's Windows/Linux NVIDIA or CUDA support does not establish compatibility with this macOS port. CUDA, representative applications, NVENC/NVDEC, sustained load, cross-GPU presentation, sleep and full VRAM capacity/performance have not been qualified here. No global `NVMTL_ENABLE` environment variable is set.

## Installed driver prerequisite — not a fresh-driver installer

Driver input is [nullmoth v1.13.0](https://github.com/nullmoth/nvidia-macos-driver/releases/tag/v1.13.0), exact source commit `0960b2ef5dbc4a573056d4c8e042a5bd2e15d651`. Release archive SHA256: `994970b1bba53e3ae8d7f500ef27b25cae8bc6818abc112d35230a58e6882fdf`.

Only NVRM and NVAccel were installed. The supplied Tahoe NVAccel had a stale allocation ABI; the owner candidate was rebuilt from that exact source with `NM_TAHOE=1`, Apple clang 21 and NVIDIA 610.57.04 headers. Private userland resource/nested signatures were repaired without changing Mach-O code/data. The combined AuxKC retained AppleMobileDevice, SoftRAID, HighPointRR and HighPointIOP; no general vendor installer or AMFIPass migration was used. These driver/firmware binaries and the owner-private transaction are **not** redistributed here.

Before the overlay, a separately reviewed installation must reproduce matching payloads, prove exact-build KC linking, make real EFI/KC/native-component backups, provide an NVIDIA-off recovery tool, then pass controlled initialization and small Metal correctness. This public overlay cannot install that prerequisite, and it is not a complete end-to-end clean NVIDIA reinstall recipe. A clean reinstall has not been rehearsed. Do not present it as one-click universal GPU support.

Key runtime pins:

| Item | SHA256 |
| --- | --- |
| NVRM executable | `4c74eb5a5be05e8819f2d8bd6ec41e09978b6ef9dccbd06de54f4ac927c32d96` |
| Tahoe NVAccel executable | `f687a54d88eda9e065bda5275877d71f0e6eb2345de63813ebe97b69de498af5` |
| NVMTLDriver executable | `a9877ab9bb61722b7a1b2f5a0458dc5cb83890370e16fa46c1030377b864640b` |
| Installed six-component AuxKC | `3b323074df4dea0c4e61de275e96e0ca5713eb20d9da5057394e09f9b8562b14` |

KCs include local component metadata: a different machine/collection may legitimately hash differently. The helper intentionally refuses that case; do not remove pins merely to make installation succeed.

## Automatic initialization without display takeover

Keep `-nvrmnogo`. Simply removing it enters upstream's boot-hold/display-arm routine, including writes to framebuffer/AGDC controls. Instead this overlay installs a root background service, `com.tonytan.nvidia-app-only`, which waits for a logged-in, active Samsung Intel-primary desktop and the headless driver state. It then makes **one** NVRM control selector-0 scalar-2 call per boot, not the vendor display-arm flow. Readiness waiting is bounded at 30 minutes; there is no KeepAlive, retry after a call, auto-reboot, sleep/wake hook or workload.

Unknown build/boot arguments, different hardware, changed pinned files/KCs/security, takeover components or an unavailable desktop refuse initialization. A per-boot root-only exclusive journal is durably flushed before opening the control client. If already initialized, it skips without another call. This does not eliminate experimental DMA/ABI/boot/heat risks; a failed kernel call can still hang the machine. It also does not automatically reinitialize a card after sleep.

The helper is our MIT-licensed native code; it does not require a separate GUI app or Node runtime. Rebuild using Xcode Command Line Tools if auditing, then compare the supplied package hashes. Its hardcoded exact-target pins require reviewed adaptation for another build.

## Install this optional overlay only after prerequisite acceptance

From the repository root, with Python 3 available:

```sh
python3 -B Tools/NVIDIAAppOnly/install.py self-test
python3 -B Tools/NVIDIAAppOnly/test_install.py
sudo python3 -B Tools/NVIDIAAppOnly/install.py install
```

Enter the computer password in Terminal, not chat. The installer requires the currently initialized, Intel-primary target. It preserves EFI, boot arguments, kernel collections, root patches, Wi-Fi/audio/HID and security. It installs only `/Library/Application Support/RazerNVIDIA`, one LaunchDaemon and `nvmtl-allow.txt`; it retains the original allow-list by rename in a root-only transaction under `/Library/Application Support/RazerNVIDIABackups`, preserving its inode for the earlier stage-1 rollback.

Save the printed `BACKUP` and RESULT. Successful installation means **configured**, not next-boot/sleep acceptance. Manually fully shut down and start, initially without external displays; confirm Intel 10-bit, Wi-Fi, real audio and HID. Read `/Library/Application Support/RazerNVIDIA/startup.log`, inspect the helper with `--inspect`, and require one successful journal and actual 16 GiB query. Do not repeatedly kickstart/retry a failed init. Then deliberately qualify real applications, power and sleep separately.

## Remove overlay / recover

After substituting your **exact printed backup path**:

```sh
sudo python3 -B Tools/NVIDIAAppOnly/install.py remove --transaction '/Library/Application Support/RazerNVIDIABackups/txn-YOUR-EXACT-ID'
```

The removal refuses a pending hardware journal, stops the service and preserves its files/logs in that transaction's `quarantine`. It restores the original diagnostic allow-list and inode. It does not unload NVIDIA or undo this boot's hardware state; manually fully shut down afterwards. Earlier stage-1 driver rollback must be run only after removing this overlay.

If startup fails, use the previously installed OpenCore **Space → NVIDIA OFF - Recovery → Y → macOS**. Do not reset NVRAM or clear its disable flag. With the altered recovery boot arguments the service refuses initialization. The recovery tool does not automatically uninstall here; once accessible, remove this overlay, then use the private stage-1 rollback. Internal backups/GitHub cannot guarantee recovery without independent boot media and do not contain a full OS/personal-data backup.

## Warnings and OLED

Copy-engine queue creation failed in the small test and used a graphics-channel fallback; `SET_LG_SECTOR_PROMOTION` returned warnings. BAR1 is 256 MiB, and mapped local allocations defaulted to system RAM. Small correctness and a 16 GiB query are not validation of full-VRAM access or expected performance. PCIe topology assertions remain. The optional PV detector/retained-mapping predicate binds older `24G830` kernel addresses and refuses `25G241`; no memory-corruption detection claim or unsafe bypass is made.

OLED **26.2** menu/lid wake was owner-validated with a green interval followed by R4 redraw. On **26.7.1**, OLED 4.7.1 boot routes and R4 are active, but real menu/lid wake and the NVIDIA-active combination remain unqualified. Do not mark OLED “fixed on the new system” until a saved-work, owner-observed real wake passes with visible locked desktop, 10-bit, Wi-Fi/audio/HID and no immediate resleep. No automatic sleep/reboot test is included.

Sanitized evidence: [NVIDIA-EVIDENCE.json](NVIDIA-EVIDENCE.json). No private serials, raw ESP, Windows BCD, TCC database or personal logs are uploaded.
