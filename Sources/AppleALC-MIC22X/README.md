# AppleALC 1.9.7 MIC22X / ALC298

This is a custom resource combination, not an upstream AppleALC release. The matched binary is included in the public EFI. English and Chinese setup/limits are in [audio setup](../../docs/CURRENT-SNAPSHOT.md) / [音频配置](../../docs/CURRENT-SNAPSHOT.zh-CN.md).

Resources derive from [AppleALC 1.9.7](https://github.com/acidanthera/AppleALC/tree/1.9.7/Resources/ALC298): layout 22 independent Mic/LineIn inputs plus layout 47 IntSpeaker/Headphone outputs. Both layout/platform resources use PathMapID 212; codec layout remains 22. Upstream BSD terms are in [AppleALC license](../../LICENSES/AppleALC-BSD-3-Clause.txt). No AppleHDA or other Apple system binary is included.

## Exact reproduction / 精确复现

Obtain the official AppleALC 1.9.7 release yourself. From this repository root:

```sh
python3 Sources/AppleALC-MIC22X/reproduce.py \
  /path/to/official/AppleALC.kext /path/to/new/AppleALC-MIC22X.kext
```

The output path must not exist. The script accepts only the exact official Mach-O hash, patches the two compressed resources and their length metadata in both x86_64/i386 slots, verifies the exact final hash and copies to a new directory. It does not install anything. The committed compressed streams avoid a dependence on a particular compressor's byte output.

输出目录必须不存在。脚本只接受精确官方二进制，在 x86_64/i386 两个资源槽替换压缩资源及长度元数据，验证最终哈希后输出到新目录，不安装任何内容。已提交压缩流，避免不同压缩工具输出不一致。

| Mach-O | SHA-256 |
| --- | --- |
| Official 1.9.7 | `5b67211797985272949b352eff0bb797504903a2ea4598e2d75d0ceca0ed5aa4` |
| Tested MIC22X | `6f566fa8b58948520adb58472a3bee89a65f4f497a891922a346e6ddf2a0f3b1` |

Only resource bytes/lengths differ. Original kext Info.plist remains unchanged. This binary still requires layout-id 22 and the matching system audio root patch; it does not repair microphone privacy policy. 本二进制仍需要 layout-id 22 和配对系统音频根补丁，不修复麦克风隐私授权策略。
