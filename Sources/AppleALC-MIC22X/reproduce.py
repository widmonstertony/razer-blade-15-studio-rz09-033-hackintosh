#!/usr/bin/env python3
"""Reproduce the exact MIC22X binary from official AppleALC 1.9.7.

AppleALC resources/binary retain upstream BSD-3-Clause licensing. This helper
is original repository code under MIT. It never writes an EFI partition.
"""
import argparse
import hashlib
import pathlib
import plistlib
import shutil
import struct
import zlib

OFFICIAL_HASH = '5b67211797985272949b352eff0bb797504903a2ea4598e2d75d0ceca0ed5aa4'
PATCHED_HASH = '6f566fa8b58948520adb58472a3bee89a65f4f497a891922a346e6ddf2a0f3b1'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('official_kext', type=pathlib.Path)
    parser.add_argument('new_output_kext', type=pathlib.Path)
    args = parser.parse_args()
    if args.new_output_kext.exists():
        parser.error('Output already exists; refusing to overwrite')
    original = (args.official_kext / 'Contents/MacOS/AppleALC').read_bytes()
    if hashlib.sha256(original).hexdigest() != OFFICIAL_HASH:
        parser.error('Not the exact official AppleALC 1.9.7 binary')
    changed = bytearray(original)
    root = pathlib.Path(__file__).resolve().parent
    ranges = [
        ('Platforms22',329,0x137590,0x30B362,0x1AB608,0x222138),
        ('layout22',3520,0x13D1D0,0x310F0C,0x1AB800,0x2222DC),
    ]
    for label, size, x64, i386, len64, len32 in ranges:
        stream = (root / (label+'-MIC22X.xml.zlib')).read_bytes()
        raw = (root / (label+'-MIC22X.xml')).read_bytes().replace(b'\n',b'').replace(b'\t',b'')
        if zlib.decompress(stream) != raw or len(stream) > size:
            raise ValueError('Resource integrity/slot-size mismatch: '+label)
        obj = plistlib.loads(b'<plist version="1.0">'+raw+b'</plist>')
        if label == 'Platforms22':
            assert obj['PathMaps'][0]['PathMapID'] == 212
        else:
            assert obj['LayoutID'] == 22 and obj['PathMapRef'][0]['PathMapID'] == 212
        assert struct.unpack_from('<Q',original,len64)[0] == size
        assert struct.unpack_from('<I',original,len32)[0] == size
        assert original[x64:x64+size] == original[i386:i386+size]
        zlib.decompress(original[x64:x64+size])
        padded = stream + bytes(size-len(stream))
        changed[x64:x64+size] = padded
        changed[i386:i386+size] = padded
        struct.pack_into('<Q',changed,len64,len(stream))
        struct.pack_into('<I',changed,len32,len(stream))
    if hashlib.sha256(changed).hexdigest() != PATCHED_HASH:
        raise ValueError('Result differs from the tested MIC22X binary')
    shutil.copytree(args.official_kext,args.new_output_kext)
    output = args.new_output_kext / 'Contents/MacOS/AppleALC'
    output.write_bytes(changed)
    assert hashlib.sha256(output.read_bytes()).hexdigest() == PATCHED_HASH
    print('MIC22X reproduced and verified: '+PATCHED_HASH)
    print('No physical EFI or installed system changed.')

if __name__ == '__main__':
    main()
