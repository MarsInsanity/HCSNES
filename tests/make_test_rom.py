#!/usr/bin/env python3
"""Create a tiny original LoROM that fills the backdrop and increments WRAM."""
from pathlib import Path
import struct
def make_rom():
    rom=bytearray([0xea])*32768
    # Native mode, 8-bit accumulator, blank screen, set CGRAM color, unblank.
    code=bytes.fromhex(
        '78 18 fb e2 20 a9 80 8d 00 21 a9 00 8d 21 21 '
        'a9 1f 8d 22 21 a9 00 8d 22 21 a9 0f 8d 00 21 '
        'ee 00 00 4c 1e 80')
    rom[:len(code)]=code
    rom[0x7fc0:0x7fd5]=b'HCSNES ORIGINAL TEST'.ljust(21)
    rom[0x7fd5:0x7fdc]=bytes([0x20,0,5,0,1,0x33,0])
    rom[0x7fdc:0x7fe0]=b'\xff\xff\x00\x00'
    for address in [0x7fe4,0x7fe6,0x7fe8,0x7fea,0x7fee,0x7ff4,0x7ff8,0x7ffa,0x7ffc,0x7ffe]:
        struct.pack_into('<H',rom,address,0x8000)
    checksum=sum(rom)&65535
    struct.pack_into('<HH',rom,0x7fdc,checksum^65535,checksum)
    return bytes(rom)
if __name__=='__main__':
    import sys
    destination=Path(sys.argv[1] if len(sys.argv)>1 else 'hcsnes-test.sfc')
    destination.write_bytes(make_rom())
    print(destination)
