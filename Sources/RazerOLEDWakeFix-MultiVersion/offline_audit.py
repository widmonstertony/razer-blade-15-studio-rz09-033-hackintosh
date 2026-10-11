#!/usr/bin/env python3
"""Read-only exact-pattern/call audit. Does not port or install a kext."""
import argparse, hashlib, json, pathlib, re, struct

def audit(source, executable, profile='25C56'):
    source_text = source.read_text()
    plan_text = (source.parent / 'PatchPlan.hpp').read_text()
    header = 'Patterns25G241.hpp' if profile == '25G241' else 'Legacy25C56Patterns.hpp'
    source_text += '\n' + (source.parent / header).read_text()
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', source_text, flags=re.S)
    raw = executable.read_bytes()
    start = 0
    if raw[:4] in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'):
        fat64 = raw[:4] == b'\xca\xfe\xba\xbf'
        n = struct.unpack_from('>I', raw, 4)[0]
        candidates = []
        for i in range(n):
            offset = 8 + i * (32 if fat64 else 20)
            cpu = struct.unpack_from('>I', raw, offset)[0]
            if cpu == 0x01000007:
                candidates.append(struct.unpack_from('>Q' if fat64 else '>I', raw, offset+8)[0])
        assert len(candidates) == 1
        start = candidates[0]
    data = raw[start:]
    assert data[:4] == b'\xcf\xfa\xed\xfe', 'Expected x86_64 little-endian Mach-O'
    ncmd = struct.unpack_from('<I', data, 16)[0]
    pos = 32
    segments, stubs = [], []
    symtab = indirect = external = None
    driver_uuid = None
    for _ in range(ncmd):
        cmd, size = struct.unpack_from('<II', data, pos)
        assert size >= 8 and pos + size <= len(data)
        if cmd == 0x1b:
            import uuid
            driver_uuid = str(uuid.UUID(bytes=data[pos+8:pos+24])).upper()
        elif cmd == 0x19:
            vm, vmsize, off, filesize = struct.unpack_from('<QQQQ', data, pos+24)
            segments.append((vm, off, filesize))
            nsects = struct.unpack_from('<I', data, pos+64)[0]
            for i in range(nsects):
                s = pos+72+i*80
                addr, length = struct.unpack_from('<QQ', data, s+32)
                flags, res1, res2 = struct.unpack_from('<III', data, s+64)
                if flags & 0xff == 8:
                    stubs.append((addr, length, res1, res2))
        elif cmd == 2:
            symtab = struct.unpack_from('<IIII', data, pos+8)
        elif cmd == 0xb:
            indirect = struct.unpack_from('<II', data, pos+56)
            external = struct.unpack_from('<II', data, pos+64)
        pos += size
    assert symtab is not None
    symoff, count, stroff, strsize = symtab
    names, addresses = [], {}
    for i in range(count):
        string, typ, sect, desc, value = struct.unpack_from('<IBBHQ', data, symoff+i*16)
        assert string < strsize
        name = data[stroff+string:data.index(b'\0', stroff+string)].decode('utf-8',errors='replace')
        names.append(name)
        if value and typ & 0xe:
            addresses.setdefault(value, []).append(name)
    if indirect:
        indoff, indcount = indirect
        for addr, length, first, width in stubs:
            assert width > 0
            for i in range(length//width):
                assert first+i < indcount
                idx = struct.unpack_from('<I',data,indoff+(first+i)*4)[0]
                if idx < len(names):
                    addresses.setdefault(addr+i*width, []).append(names[idx])
    relocations = {}
    if external:
        reloff, relcount = external
        for i in range(relcount):
            address, bits = struct.unpack_from('<iI',data,reloff+i*8)
            index = bits & 0xffffff
            if bits & (1<<27) and (bits>>28) == 2 and index < len(names):
                relocations[address] = names[index]
    def vm_for(filepos):
        for vm, off, size in segments:
            if off <= filepos < off+size:
                return vm+filepos-off
        raise ValueError('Call outside file-backed Mach-O segments')
    # Parse offsets and byte-shape guards from the same descriptors used in the kernel.
    which = 'new' if profile == '25G241' else 'legacy'
    block = re.search(r'static const Rule '+which+r'Rules\[\]\s*\{(.*?)\n\};',plan_text,re.S).group(1)
    descriptor = re.findall(r'\{(?:RazerOLED25G241::)?(find\w+),.*?sizeof\([^)]*\),\s*(\d+),\s*Target::(\w+),\s*(\w+)',block,re.S)
    assert len(descriptor) == 10
    offsets = {name:int(offset) for name,offset,target,suffix in descriptor}
    suffix_names = {name:suffix for name,offset,target,suffix in descriptor}
    specs = [
        ('findPanelSettleDelay', None, None),
        ('findPhase1StableRetryExit', None, None),
        ('findPhase1StableRetryFailure', None, None),
        ('findInitialDDIBufferEnableCallSite', offsets['findInitialDDIBufferEnableCallSite'], 'WriteRegister32'),
        ('findDDIBufferEnableCallSite', offsets['findDDIBufferEnableCallSite'], 'WriteRegister32'),
        ('findPhase2DDIBufferEnableCallSite', offsets['findPhase2DDIBufferEnableCallSite'], 'WriteRegister32'),
        ('findDPCDLaneSetWriteCallSite', offsets['findDPCDLaneSetWriteCallSite'], 'WriteAUX'),
        ('findPhase1ClockRecoveryDelayCallSite', offsets['findPhase1ClockRecoveryDelayCallSite'], '_IOSleep'),
        ('findNativeLinkConfigurationWriteCallSite', offsets['findNativeLinkConfigurationWriteCallSite'], 'WriteAUX'),
        ('findNativeEDPConfigurationWriteCallSite', offsets['findNativeEDPConfigurationWriteCallSite'], 'WriteAUX'),
    ]
    checks = []
    for name, delta, expected in specs:
        m = re.search(r'static const uint8_t\s+'+name+r'\[\]\s*\{(.*?)\};',text,re.S)
        assert m is not None
        pattern = bytes(int(v,16) for v in re.findall(r'0x([0-9A-Fa-f]+)',m.group(1)))
        positions = [m.start() for m in re.finditer(re.escape(pattern),data)]
        result = dict(pattern=name,matches=len(positions),unique=len(positions)==1)
        if len(positions) == 1:
            off = positions[0]
            result['fileOffset'] = hex(start+off)
            if delta is not None:
                call = off+delta
                result['callOpcodeE8'] = call+5 <= len(data) and data[call] == 0xe8
                if result['callOpcodeE8']:
                    target = vm_for(call)+5+struct.unpack_from('<i',data,call+1)[0]
                    result['targetVm'] = hex(target)
                    result['targetSymbols'] = addresses.get(target,[])
                    relocation = relocations.get(vm_for(call)+1)
                    if relocation:
                        result['externalBranchRelocation'] = relocation
                        result['targetSymbols'] = [relocation]
                    result['expectedTarget'] = expected
                    result['targetVerified'] = any(expected in x for x in result['targetSymbols'])
            # Mirror the production byte-shape guards for shortened prefixes.
            var = suffix_names[name]
            if var != 'nullptr':
                guard = re.search(r'static const ByteCheck '+var+r'\[\]\s*\{(.*?)\};',plan_text,re.S).group(1)
                shape = re.findall(r'\{(\d+),0x([a-fA-F0-9]+)\}',guard)
                assert shape
                result['productionShapeVerified'] = all(off+int(p)<len(data) and data[off+int(p)]==int(v,16) for p,v in shape)
        checks.append(result)
    expected_uuid = {'25C56':'0A970005-966B-39C9-9D30-1E8F9BF6F86A',
                     '25G241':'7E294F0F-852E-3252-B54D-61D9A7BBE6B2'}[profile]
    required_symbols = re.findall(r'static const char \*\w+Symbol\s*=\s*"([^"]+)"',source.read_text())
    symbol_checks = {s:s in names for s in required_symbols}
    passed = (driver_uuid == expected_uuid and all(symbol_checks.values()) and
              all(c['unique'] and c.get('targetVerified',True) and c.get('productionShapeVerified',True) for c in checks))
    return dict(executable=str(executable),bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest(),
                profile=profile,driverUuid=driver_uuid,exactUuidMatches=driver_uuid==expected_uuid,requiredSymbols=symbol_checks,
                sourceSha256=hashlib.sha256(source.read_bytes()).hexdigest(), expandedSourceSha256=hashlib.sha256(source_text.encode()).hexdigest(),
                patchPlanSha256=hashlib.sha256(plan_text.encode()).hexdigest(),checks=checks,
                staticPatternAndCallAuditPassed=passed,
                scope='Static comparison only; not live routing, object-layout or sleep/wake qualification')

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('source',type=pathlib.Path)
    p.add_argument('executable',type=pathlib.Path)
    p.add_argument('--profile', choices=['25C56','25G241'], default='25C56')
    p.add_argument('--output', type=pathlib.Path, help='Save this offline audit as JSON')
    a=p.parse_args()
    result=audit(a.source,a.executable,a.profile)
    if a.output:
        a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    raise SystemExit(0 if result['staticPatternAndCallAuditPassed'] else 1)
