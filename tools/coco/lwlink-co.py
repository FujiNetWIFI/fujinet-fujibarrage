#!/usr/bin/env python3
"""lwlink-co.py -- lwlink for CMOC (--lwlink=), with FujiBarrage's CoCo memory
maps (after FujiBlast's tools/coco/lwlink-fb.py).

CMOC links with its own script; this swaps in src/coco/barrage.link (the
CoCo 1/2) or barrage3.link (CO_LINK=coco3), runs the real lwlink, checks
every region's limit, keeps the map (CO_MAP), and splits the DECB output
into what the CoCo loads:

  <out>.bin     low core (< $8000): LOADM'd by BASIC (or chain.asm), has
                the exec address
  <out>h.bin    the high core ($8000 up) and, on the CoCo 1/2, the art at
                $E000: loaded by the program itself (src/coco/bank.c)
  <out>a.bin    (CoCo 3) the art: one MMU block, seen at $E000 through
                slot 7 (bank.c)

All are DECB .bin files (segments of 00 len addr data, then FF 0000 exec).
"""
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
COCO3 = os.environ.get("CO_LINK") == "coco3"
SCRIPT = os.path.join(HERE, "..", "..", "src", "coco", "barrage3.link" if COCO3 else "barrage.link")

# CoCo 1/2: $0E00 the direct page, $0F00 the link's ring, $1000-$27FF the
# screen; the low core from $2800. CoCo 3: its screen is in other blocks.
LOW_START = 0x1F00 if COCO3 else 0x2800
LOW_END = 0x7D00        # the stack is the 768 bytes below $8000 (--initial-s; a live game
                        # takes about 320 of them: src/coco/bank.c marks it)
HIGH_END = 0xC000 if COCO3 else 0xBFF0   # (the SAM's vectors at $BFF0 on a CoCo 1/2)
ART_END = 0xFE00 if COCO3 else 0xFF00    # (the CoCo 3's constant page; the I/O page)
REGIONS = [
    ("fixed", 0x0EF0, 0x0F00),      # src/coco/fixed.asm: the clock, the test hook
    ("low", LOW_START, LOW_END),
    ("high", 0x8000, HIGH_END),
    ("art", 0xE000, ART_END),
] + ([("mid", 0x1000, 0x1F00)] if COCO3 else [])   # (the CoCo 3: bss only, past AUTOEXEC.BAS's time)


def region_of(addr):
    for name, a, b in REGIONS:
        if a <= addr < b:
            return name, a, b
    return None, 0, 0


def read_decb(path):
    data = open(path, "rb").read()
    blocks, i, exe = [], 0, None
    while i < len(data):
        kind = data[i]
        n = (data[i + 1] << 8) | data[i + 2]
        a = (data[i + 3] << 8) | data[i + 4]
        if kind == 0xFF:
            exe = a
            break
        blocks.append((a, data[i + 5:i + 5 + n]))
        i += 5 + n
    return blocks, exe


def write_decb(path, blocks, exe):
    with open(path, "wb") as f:
        for a, b in blocks:
            for k in range(0, len(b), 0xFF00):     # length is 16 bits
                part = b[k:k + 0xFF00]
                f.write(bytes([0, len(part) >> 8, len(part) & 255,
                               (a + k) >> 8, (a + k) & 255]))
                f.write(part)
        f.write(bytes([0xFF, 0, 0, exe >> 8, exe & 255]))


def main():
    args = sys.argv[1:]
    out = mapf = None
    new = []
    for a in args:
        if a.startswith("--script="):
            a = "--script=" + SCRIPT
        elif a.startswith("--output="):
            out = a[len("--output="):]
        elif a.startswith("--map="):
            mapf = a[len("--map="):]
        new.append(a)
    if mapf is None:
        mapf = (out or "a.out") + ".map"
        new.append("--map=" + mapf)
    r = subprocess.call(["lwlink"] + new)
    if r != 0 or out is None:
        return r
    if os.environ.get("CO_MAP"):            # CMOC deletes its map: the rigs read it
        shutil.copy(mapf, os.environ["CO_MAP"])

    secs = {}
    for line in open(mapf):
        m = re.match(r"Section: (\S+) \((.*)\) load at ([0-9A-F]+), length ([0-9A-F]+)", line)
        if not m:
            continue
        name, a, n = m.group(1), int(m.group(3), 16), int(m.group(4), 16)
        s = secs.setdefault(name, [a, a])
        s[0] = min(s[0], a)
        s[1] = max(s[1], a + n)
    bad = 0
    for name, (a, e) in sorted(secs.items(), key=lambda kv: kv[1][0]):
        if e == a:
            continue
        reg, lo, hi = region_of(a)
        if reg is None or e > hi:
            print("lwlink-co: section %s $%04X-$%04X is outside its region (%s, ends $%04X)"
                  % (name, a, e, reg, hi), file=sys.stderr)
            bad = 1
    if bad:
        return 1

    blocks, exe = read_decb(out)
    base = out[:-4] if out.endswith(".bin") else out
    low = [(a, b) for a, b in blocks if LOW_START <= a < 0x8000]
    # (the CoCo 3's mid region, $1000-$1EFF, goes with the high core: bank.c
    # writes it once BASIC, whose AUTOEXEC.BAS sits there, is gone)
    high = [(a, b) for a, b in blocks if 0x8000 <= a < (0xE000 if COCO3 else 0x10000) or 0x1000 <= a < LOW_START]
    write_decb(out, low, exe)
    write_decb(base + "h.bin", high, exe)
    if COCO3:
        write_decb(base + "a.bin", [(a, b) for a, b in blocks if a >= 0xE000], exe)

    used = {}
    for name, (a, e) in secs.items():
        reg, lo, hi = region_of(a)
        if reg and e > a:
            used[reg] = max(used.get(reg, lo), e)
    print("%s: " % os.path.basename(out) + "; ".join(
        "%s to $%04X of $%04X (%d free)" % (reg, used.get(reg, lo), hi, hi - used.get(reg, lo))
        for reg, lo, hi in REGIONS if reg != "fixed"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
