#!/usr/bin/env python3
"""mkfix_s2.py -- refview's fixture (refview -fixture-asm) into a copy of a
TEST build's image, at FIXBUF (the cart's RAM: the scenes read it there).

    mkfix_s2.py FIXTURE.inc IMAGE.st2 LISTING OUT.st2

FIXBUF is bank BK_FIX's window; it holds (the Channel F's mkfix_chf.py layout):
  0 sky, 1 segments, 2 the shooter, 3-12 the tanks' x, 13-22 y, 23-32 angle,
  33-34 the terrain's bytes (LE), the terrain's column entries, then each
  segment: its length (LE), its bytes.
One TEST build serves every fixture: only the image's FIXBUF pages change.
"""
import os
import re
import sys


FIXMAX = 3072


def pack(path):
    text = open(path).read()
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
    return bytes(out)


def symbol(lst, name):
    for line in open(lst, errors="replace"):
        m = re.search(r"\b%s\s*:\s*([0-9A-F]+) [C-]" % name, line)
        if m:
            return int(m.group(1), 16)
    sys.exit("mkfix_s2: no %s in %s" % (name, lst))


def main():
    fix, img, lst, out = sys.argv[1:5]
    patch(img, lst, out, pack(fix))


def patch(img, lst, out, data):
    """data into a copy of the image at FIXBUF (bank BK_FIX)"""
    if len(data) > FIXMAX:
        sys.exit("mkfix_s2: %d bytes: FIXBUF holds %d" % (len(data), FIXMAX))
    at = symbol(lst, "FIXBUF")
    bank = symbol(lst, "BK_FIX")
    st2 = bytearray(open(img, "rb").read())
    if st2[:4] != b"RCA2":
        sys.exit("mkfix_s2: %s is not an ST2" % img)
    blocks = st2[4] - 1
    where, b, opened = {}, 0, False      # (the window's pages: bank BK_FIX's,
    for i in range(blocks):              #  each $C4 after the first opens the next)
        pg = st2[64 + i]
        if pg == 0xC4:
            b += opened
            opened = True
        if 0xC4 <= pg <= 0xCF and b != bank:
            continue
        where[pg] = 256 * (i + 1)
    for k, b in enumerate(data.ljust(FIXMAX, b"\0")):
        a = at + k
        pg = a >> 8
        if pg not in where:
            sys.exit("mkfix_s2: page %02X is not in the image" % pg)
        st2[where[pg] + (a & 0xFF)] = b
    open(out, "wb").write(st2)
    print("mkfix_s2: %d bytes at %04X" % (len(data), at))


if __name__ == "__main__":
    main()
