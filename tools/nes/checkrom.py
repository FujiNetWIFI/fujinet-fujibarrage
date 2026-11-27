#!/usr/bin/env python3
"""checkrom.py CFG MAP IMAGE -- check the NES client's image (FujiBlast's).

The rules are the FujiNet NES cartridge's (fujinet-firmware pico/nes/tools/
checkrom.py; fujinet-lib-experimental makefiles/nes-romstamp.py):
  - the file is as long as its header says;
  - "FUJI" at $FFF0 (the end of the last bank, less 16): it keeps the
    cartridge's mailbox alive after the image boots;
  - the reset vector is in the fixed banks ($C000-$FFFF);
  - no read-modify-write instruction aims at the mailbox's write-only
    pages ($5500-$57FF): the 6502 writes the old value back first, and the
    cartridge takes that as a store of its own. Each code segment the map
    lists is scanned, in the bank CFG (src/nes/barrage-nes.cfg) loads it
    to: B<n> is bank n (B0 at $A000, the rest at $8000), FIXED the last two.
A failing image is deleted (an image left behind can't look up to date).
"""
import os
import re
import sys

WR_LO, WR_HI = 0x5500, 0x57FF
RMW = {0x0E, 0x1E, 0x2E, 0x3E, 0x4E, 0x5E, 0x6E, 0x7E, 0xCE, 0xDE, 0xEE, 0xFE}

# 6502 instruction lengths (unofficial opcodes count as 1).
LEN = [1] * 256
for op in (0x69, 0x29, 0xC9, 0xE0, 0xC0, 0x49, 0xA9, 0xA2, 0xA0, 0x09, 0xE9,
           0xA5, 0xA6, 0xA4, 0x85, 0x86, 0x84, 0x65, 0x25, 0x06, 0x24, 0xC5,
           0xC6, 0x45, 0xE6, 0x46, 0x26, 0x66, 0xE5, 0x05, 0x75, 0x35, 0x16,
           0xD5, 0xD6, 0x55, 0xF6, 0x56, 0x36, 0x76, 0xF5, 0x15, 0xB5, 0xB4,
           0x95, 0x94, 0xB6, 0x96, 0x61, 0x21, 0xC1, 0x41, 0xA1, 0x01, 0xE1,
           0x81, 0x71, 0x31, 0xD1, 0x51, 0xB1, 0x11, 0xF1, 0x91,
           0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0):
    LEN[op] = 2
for op in (0x6D, 0x2D, 0x0E, 0x2C, 0xCD, 0xEC, 0xCC, 0xCE, 0x4D, 0xEE, 0x4C,
           0x20, 0xAD, 0xAE, 0xAC, 0x4E, 0x0D, 0x2E, 0x6E, 0xED, 0x8D, 0x8E,
           0x8C, 0x7D, 0x3D, 0x1E, 0xDD, 0xDE, 0x5D, 0xFD, 0xFE, 0x5E, 0xBD,
           0xBC, 0x3E, 0x7E, 0x1D, 0x9D, 0x79, 0x39, 0xD9, 0x59, 0xB9, 0xBE,
           0x19, 0xF9, 0x99, 0x6C):
    LEN[op] = 3

BANK = 0x2000
CODE_SEGS = re.compile(r"CODE|STARTUP|ONCE")


def seg_banks(cfg, nbanks):
    """code segment -> (bank, the address it is seen at)"""
    text = open(cfg).read()
    segs = {}
    for name, area in re.findall(r"^\s*(\w+):\s*load\s*=\s*(\w+)", text.split("SEGMENTS")[1], re.M):
        m = re.match(r"B(\d+)$", area)
        if m:
            b = int(m.group(1))
            segs[name] = (b, 0xA000 if b == 0 else 0x8000)
        elif area == "FIXED":
            segs[name] = (nbanks - 2, 0xC000)
    return segs


def main():
    if len(sys.argv) != 4:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    cfg, mapfile, path = sys.argv[1:]
    img = open(path, "rb").read()
    problems = []
    if img[:4] != b"NES\x1a":
        problems.append("no iNES header")
    else:
        prg = ((img[9] & 0x0F) << 8 | img[4]) * 16384
        chr_ = ((img[9] >> 4) << 8 | img[5]) * 8192
        if len(img) != 16 + prg + chr_:
            problems.append("%d bytes, the header says %d" % (len(img), 16 + prg + chr_))
        p = img[16:16 + prg]
        if p[-16:-12] != b"FUJI":
            problems.append("no FUJI claim at $FFF0")
        vec = p[-4] | p[-3] << 8
        if vec < 0xC000:
            problems.append("the reset vector $%04X is not in the fixed banks" % vec)
        banks = seg_banks(cfg, prg // BANK)
        segs = re.findall(r"^(\w+)\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+[0-9A-F]{6}",
                          open(mapfile).read(), re.M)
        for name, start, end in segs:
            if name not in banks or not CODE_SEGS.search(name):
                continue
            bank, base = banks[name]
            start, end = int(start, 16), int(end, 16)
            off = bank * BANK - base
            pc = start
            while pc <= end - 2:
                op = p[off + pc]
                if op in RMW:
                    tgt = p[off + pc + 1] | p[off + pc + 2] << 8
                    if WR_LO <= tgt <= WR_HI:
                        problems.append("%s: a read-modify-write ($%02X) of $%04X at $%04X"
                                        % (name, op, tgt, pc))
                pc += LEN[op]
    if problems:
        for m in problems:
            print("%s: %s" % (path, m), file=sys.stderr)
        os.remove(path)
        return 1
    print("%s: ok" % path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
