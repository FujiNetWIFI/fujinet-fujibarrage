#!/usr/bin/env python3
"""terrcheck.py --live cga|vga TAPLOG SCREEN.bin [OUT.png] -- the PC's field
against the terrain it was given: the TERRAIN (G) frames of
tools/rigs/tapproxy.py's TAPLOG replayed (tools/cocosim/terrcheck.py's
tap_terrain) against a dump of the screen (dossim's `dump`: B800h 16384 or
A000h 64000), taken while the field was the last terrain's (no shot
since). Sky and dirt compared; tanks and fire (the turn's arrow) left out.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(TOP, "tools", "cocosim"))
import dosfield  # noqa: E402
from terrcheck import tap_terrain  # noqa: E402  (the CoCo's)


def live(m, log, scr, png):
    W, H = dosfield.W, dosfield.H
    t = tap_terrain(log, W, H)
    if t is None:
        sys.exit("no whole terrain in " + log)
    data = open(scr, "rb").read()
    f = dosfield.cga(data) if m == "cga" else dosfield.vga(data)
    nd = nother = 0
    diff = []
    for y in range(H):
        row = []
        for x in range(W):
            v = f[y][x]
            if v in (0, 3):
                nother += 1
            elif v != t[y][x]:
                nd += 1
                v = 4
            row.append(v)
        diff.append(row)
    print("%s live: %d sky/dirt pixels differ (%d tank/fire not compared)" % (m, nd, nother))
    if png:
        dosfield.picture(png, diff)
    return nd


if __name__ == "__main__":
    a = sys.argv[1:]
    if len(a) < 4 or a[0] != "--live" or a[1] not in ("cga", "vga"):
        sys.exit(__doc__)
    sys.exit(1 if live(a[1], a[2], a[3], a[4] if len(a) > 4 else None) else 0)
