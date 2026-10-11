#!/usr/bin/env python3
"""Readonly pattern inventory of the SHIPPING CFL code inside an x86_64 KC.

This is not a loader, patcher, or live sleep/wake test. No code is changed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import uuid

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('collection', type=Path, help='Legally extracted 25G241 SystemKernelExtensions.kc')
parser.add_argument('kdk', type=Path, help='Exact 25G241 KDK CFL executable')
parser.add_argument('--patterns', type=Path, default=Path(__file__).with_name('Patterns25G241.hpp'))
parser.add_argument('--output', type=Path)
args = parser.parse_args()
collection_path, kdk_path, source_path = args.collection, args.kdk, args.patterns
identifier = 'com.apple.driver.AppleIntelCFLGraphicsFramebuffer'
EXPECTED_UUID = '7E294F0F-852E-3252-B54D-61D9A7BBE6B2'
EXPECTED_KC = 'c29d75fc8d22cfa880675fa181917d2159aefda336df665169977a259bea56c8'
EXPECTED_KDK = '8ddf3c6d526ed73ac827d627f8ec2708a101784bdd56c5dbbd3e20caaf6bf61a'


def commands(data, start=0):
    assert data[start:start+4] == b'\xcf\xfa\xed\xfe'
    count, size = struct.unpack_from('<II', data, start+16)
    pos, end = start+32, start+32+size
    for _ in range(count):
        cmd, length = struct.unpack_from('<II', data, pos)
        assert length >= 8 and pos+length <= end <= len(data)
        yield cmd, pos
        pos += length
    assert pos == end


def image(data, start):
    section = ident = None
    relocations = []
    for cmd, pos in commands(data, start):
        if cmd == 0x1b:
            ident = str(uuid.UUID(bytes=data[pos+8:pos+24])).upper()
        elif cmd == 0x19:
            count = struct.unpack_from('<I', data, pos+64)[0]
            for i in range(count):
                sect = pos+72+i*80
                name = data[sect:sect+16].rstrip(b'\0')
                segment = data[sect+16:sect+32].rstrip(b'\0')
                if (segment, name) == (b'__TEXT', b'__text'):
                    vm, length = struct.unpack_from('<QQ', data, sect+32)
                    off = struct.unpack_from('<I', data, sect+48)[0]
                    assert off+length <= len(data)
                    section = {'vm': vm, 'offset': off, 'bytes': length}
        elif cmd == 0xb:
            off, count = struct.unpack_from('<II', data, pos+64)
            for i in range(count):
                address, bits = struct.unpack_from('<iI', data, off+i*8)
                relocations.append((address, 1 << ((bits >> 25) & 3)))
    assert ident and section
    return ident, section, relocations


collection = collection_path.read_bytes()
if hashlib.sha256(collection).hexdigest() != EXPECTED_KC:
    raise SystemExit('REFUSED: non-exact shipping KC SHA256')
matches = []
for cmd, pos in commands(collection):
    if cmd == 0x80000035:
        off = struct.unpack_from('<Q', collection, pos+16)[0]
        name_off = struct.unpack_from('<I', collection, pos+24)[0]
        name = collection[pos+name_off:collection.index(b'\0', pos+name_off)].decode()
        if name == identifier:
            matches.append(off)
assert len(matches) == 1
shipping_uuid, section, _ = image(collection, matches[0])
text = collection[section['offset']:section['offset']+section['bytes']]
kdk = kdk_path.read_bytes()
if hashlib.sha256(kdk).hexdigest() != EXPECTED_KDK:
    raise SystemExit('REFUSED: non-exact KDK CFL SHA256')
kdk_uuid, kdk_section, relocations = image(kdk, 0)
kdk_text = kdk[kdk_section['offset']:kdk_section['offset']+kdk_section['bytes']]
ignored = set()
for address, length in relocations:
    off = address - kdk_section['vm']
    if 0 <= off and off+length <= len(kdk_text):
        ignored.update(range(off, off+length))
source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source_path.read_text(), flags=re.S)
patterns = []
for name, body in re.findall(r'static const uint8_t\s+(find\w+)\[\]\s*\{(.*?)\};', source, re.S):
    pattern = bytes(int(v,16) for v in re.findall(r'0x([0-9A-Fa-f]+)', body))
    offsets = [m.start() for m in re.finditer(re.escape(pattern), text)]
    patterns.append({'name': name, 'matchesInShippingCflText': len(offsets),
                     'textOffsets': offsets})
assert len(patterns) == 10
report = {
    'collection': str(collection_path), 'collectionSha256': hashlib.sha256(collection).hexdigest(),
    'cflFilesetOffset': matches[0], 'shippingCflUuid': shipping_uuid, 'kdkCflUuid': kdk_uuid,
    'sameCompilationUuid': shipping_uuid == kdk_uuid, 'shippingTextBytes': len(text),
    'shippingTextSha256': hashlib.sha256(text).hexdigest(), 'kdkTextBytes': len(kdk_text),
    'sameTextIgnoringKdkExternalRelocations': len(text) == len(kdk_text) and all(
        a == b or i in ignored for i, (a, b) in enumerate(zip(text, kdk_text))),
    'source': str(source_path), 'sourceSha256': hashlib.sha256(source_path.read_bytes()).hexdigest(), 'patterns': patterns,
    'candidateVerdict': 'STATIC_SHIPPING_TEXT_CHECKS_ONLY',
    'scope': 'Shipping code pattern inventory only. Does not apply patches or prove live wake behavior.',
    'systemInstalled': False, 'efiWritten': False, 'sleepWakeTested': False,
}
passed = (shipping_uuid == kdk_uuid == EXPECTED_UUID and
          report['sameTextIgnoringKdkExternalRelocations'] and
          all(p['matchesInShippingCflText'] == 1 for p in patterns))
report['staticShippingTextChecksPassed'] = passed
if args.output:
    args.output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))

raise SystemExit(0 if passed else 1)
