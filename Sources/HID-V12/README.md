# HID V12 source patches

The included VoodooI2C and VoodooI2CHID binaries contain machine-specific typing lockout, contact-generation quarantine and palm-rejection work used by the validated Razer build. The modifications are published as GPLv3 patches against exact upstream commits.

## Exact bases

```text
VoodooI2C:    e3ac3ed207170c39db88d6a67a76cc9fd3a1895a
VoodooI2CHID: a478fd605043989269584a73e23a15cc78994f5b
```

## Reconstruct the source

```sh
git clone --recursive https://github.com/VoodooI2C/VoodooI2C.git
cd VoodooI2C
git checkout e3ac3ed207170c39db88d6a67a76cc9fd3a1895a
git submodule update --init --recursive
git apply /path/to/Sources/HID-V12/VoodooI2C-core.patch
git -C "VoodooI2C Satellites/VoodooI2CHID" apply \
  /path/to/Sources/HID-V12/VoodooI2CHID-satellite.patch
```

Build the `VoodooI2C.xcworkspace` Release configuration with a compatible full Xcode installation and initialized submodules. Replace VoodooI2C and VoodooI2CHID as a matched pair; their cross-kext event interface changed in V12, so mixing an old object or old satellite can disable typing protection or fail to link.

The exact tested binaries are already in `EFI/OC/Kexts`. The patches and resulting modified binaries are distributed under GPLv3; see `LICENSES/GPL-3.0.txt` and the upstream copyright notices.
