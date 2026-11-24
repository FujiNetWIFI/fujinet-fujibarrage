#!/usr/bin/env python3
"""terrcheck.py TAPLOG RUNLOG [SYMBOL] -- the H89's field store against the
terrain it was given: the TERRAIN (G) frames of tools/rigs/tapproxy.py's
TAPLOG replayed (tools/cocosim/terrcheck.py's tap_terrain) against the
`peek fld_store 1760` line of h89sim's RUNLOG, taken while the field was the
last terrain's (no shot since). Sky and dirt compared; fire (the turn's
arrow) left out. Two bits a block, four a byte, the leftmost in the low bits.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(TOP, "tools", "cocosim"))
from terrcheck import tap_terrain  # noqa: E402

W, H = 160, 44


def store(runlog, sym):
    last = None
    for line in open(runlog, errors="replace"):
        p = line.split()
        if len(p) > 2 and p[0] == "PEEK" and p[1] == sym:
            last = bytes(int(x, 16) for x in p[2:])
    return last


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    sym = sys.argv[3] if len(sys.argv) > 3 else "fld_store"
    t = tap_terrain(sys.argv[1], W, H)
    if t is None:
        sys.exit("no whole terrain in " + sys.argv[1])
    s = store(sys.argv[2], sym)
    if s is None or len(s) != W * H // 4:
        sys.exit("no `peek %s %d` in %s" % (sym, W * H // 4, sys.argv[2]))
    nd = nfire = 0
    for y in range(H):
        for x in range(W):
            v = (s[y * (W // 4) + x // 4] >> ((x & 3) * 2)) & 3
            if v == 3:
                nfire += 1
            elif v != t[y][x]:
                nd += 1
    print("h89: %d sky/dirt blocks differ (%d fire not compared)" % (nd, nfire))
    return 1 if nd else 0


if __name__ == "__main__":
    sys.exit(main())
