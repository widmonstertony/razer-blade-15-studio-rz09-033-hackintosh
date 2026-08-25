# RazerOLEDWakeFix V61 source

This is the source corresponding to the 4.5.0 binary in `EFI/OC/Kexts/RazerOLEDWakeFix.kext`. It is an exact-target Lilu plug-in for the RZ09-0330Q Samsung OLED and Tahoe 26.2 / Darwin 25.2.x.

## Build dependencies

- Apple Command Line Tools with an SDK capable of compiling x86_64 kext code;
- [Lilu](https://github.com/acidanthera/Lilu) resources/headers;
- [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK), including x86_64 kext libraries.

The script defaults to these repository-local paths, which are ignored by Git:

```text
vendor/Lilu.kext/Contents/Resources
vendor/MacKernelSDK
```

They can be overridden without editing the script:

```sh
LILU_RESOURCES=/path/to/Lilu.kext/Contents/Resources \
MAC_KERNEL_SDK=/path/to/MacKernelSDK \
MACOS_SDK=/path/to/MacOSX.sdk \
./Sources/RazerOLEDWakeFix/build-clt.sh
```

The result is `Sources/RazerOLEDWakeFix/build/RazerOLEDWakeFix.kext`. Compare its Info.plist, architecture and runtime behavior with the tested binary before using it. A compiler update need not produce a byte-identical executable.

## Safety model

The source validates exact Tahoe 25.2 instruction patterns and publishes detailed IORegistry status. Do not weaken `MinKernel`/`MaxKernel`, bypass live-pattern verification or copy the fixed display registers to another machine. Porting to another build requires a fresh framebuffer analysis and a protected USB boot test.

The Lilu bootstrap file retains its upstream copyright notice. Original V61 implementation in this directory is covered by the repository MIT license; Lilu headers and MacKernelSDK remain under their own licenses and are not vendored here.
