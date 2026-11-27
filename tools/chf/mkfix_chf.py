#!/usr/bin/env python3
"""mkfix_chf.py -- refview's fixture (refview -fixture-asm) as the bytes the
Channel F's test scene 3 reads from FIXBUF (tools/chfsim `load`).

    mkfix_chf.py FIXTURE.inc OUT.bin

  0 sky, 1 segments, 2 the shooter, 3-12 the tanks' x, 13-22 y, 23-32 angle,
  33-34 the terrain's bytes (LE), the terrain's column entries, then each
  segment: its length (LE), its bytes.
"""
import re
import sys


def main():
    text = open(sys.argv[1]).read()
    eq = {k: int(v) for k, v in re.findall(r"^(\w+)\s+EQU\s+(\d+)", text, re.M)}
    lab = {}
    for name, body in re.findall(r"^(\w+):\n((?:\s+D[BW]\s+[^\n]*\n)+)", text, re.M):
        lab[name] = [int(x) for line in body.strip().splitlines() for x in line.split(None, 1)[1].split(",")]
    tx, ty, ta = lab["FIXTX"], lab["FIXTY"], lab["FIXTA"]
    terr, lens, segs = lab["FIXTERR"], lab["FIXSLEN"], lab["FIXSEG"]
    out = bytearray([eq["FIXSKY"], eq["FIXNSEG"], eq["FIXSHOOT"]])
    out += bytes(tx) + bytes(ty) + bytes(ta)
    out += bytes([len(terr) & 255, len(terr) >> 8]) + bytes(terr)
    at = 0
    for n in lens:
        out += bytes([n & 255, n >> 8]) + bytes(segs[at:at + n])
        at += n
    if len(out) > 0x1000:
        sys.exit("mkfix_chf: %d bytes: FIXBUF holds 4096" % len(out))
    open(sys.argv[2], "wb").write(out)


if __name__ == "__main__":
    main()
