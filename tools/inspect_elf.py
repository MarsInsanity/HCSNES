#!/usr/bin/env python3
"""Inspect the installable core ABI without depending on readelf."""
from pathlib import Path
import json, re, struct, sys
def inspect(path):
    b=Path(path).read_bytes()
    assert b[:6]==b'\x7fELF\x01\x01','Expected 32-bit little-endian ELF'
    e=struct.unpack_from('<HHIIIIIHHHHHH',b,16)
    typ,machine,version,entry,phoff,shoff,flags,ehsize,phentsize,phnum,shentsize,shnum,shstr=e
    assert machine==8,'Expected MIPS'
    sections=[struct.unpack_from('<IIIIIIIIII',b,shoff+i*shentsize) for i in range(shnum)]
    names=b[sections[shstr][4]:sections[shstr][4]+sections[shstr][5]]
    def name(offset):return names[offset:].split(b'\0',1)[0].decode()
    sec={name(s[0]):s for s in sections}
    abi=sec['.MIPS.abiflags']; fields=struct.unpack_from('<HBBBBBBIIII',b,abi[4])
    assert fields[1:5]==(32,2,1,1),fields
    assert fields[6]==1,'Expected hard float double precision ABI'
    assert flags&0x1000,'Expected O32 ABI'
    assert flags&0xf0000000==0x70000000,'Expected MIPS32r2 ISA'
    strings=sec.get('.dynstr'); dependencies=[]
    if strings and '.dynamic' in sec:
        st=b[strings[4]:strings[4]+strings[5]]; ds=sec['.dynamic']
        for i in range(ds[4],ds[4]+ds[5],8):
            tag,value=struct.unpack_from('<II',b,i)
            if tag==1:dependencies.append(st[value:].split(b'\0',1)[0].decode())
    return dict(file=Path(path).name,bytes=len(b),type=typ,machine='MIPS',
       endian='little',abi='O32',isa='MIPS32r2',fp='hard float, 32-bit FPRs',
       needed=dependencies,glibc_versions=sorted(set(x.decode() for x in re.findall(rb'GLIBC_[0-9.]+',b))))
if __name__=='__main__':
    for p in sys.argv[1:]:print(json.dumps(inspect(p),indent=2))
