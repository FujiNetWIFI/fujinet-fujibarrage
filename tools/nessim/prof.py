#!/usr/bin/env python3
"""prof.py -- nesrun's `prof` (Mesen's profiler: cycles by function, keyed
by PRG-ROM offset) by name, from ld65's debug file (build/nes/barrage.dbg:
each segment's place in the image, each label's address).

    prof.py PROF.txt [DBG] [-n N]

Prints the functions by exclusive cycles (their own code) and by inclusive
(with what they call), the N first (20) of each, and the frames' total.
"""
import bisect
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def labels(dbg):
    segs, syms = {}, []
    for line in open(dbg):
        kind, _, rest = line.rstrip("\n").partition("\t")
        f = dict(re.findall(r'(\w+)=("[^"]*"|[^,]*)', rest))
        if kind == "seg" and "ooffs" in f:
            segs[f["id"]] = (int(f["start"], 16), int(f["ooffs"]))
        elif kind == "sym" and f.get("type") == "lab" and "seg" in f:
            syms.append((f["seg"], int(f["val"], 16), f["name"].strip('"')))
    out = []
    for seg, val, name in syms:
        if seg in segs:
            start, ooffs = segs[seg]
            out.append((ooffs - 16 + val - start, name))
    out.sort()
    return out


def main():
    args = sys.argv[1:]
    n = 20
    if "-n" in args:
        i = args.index("-n")
        n = int(args[i + 1])
        del args[i:i + 2]
    if not args:
        sys.exit(__doc__)
    dbg = args[1] if len(args) > 1 else os.path.join(HERE, "..", "..", "build", "nes", "barrage.dbg")
    labs = labels(dbg)
    offs = [o for o, _ in labs]

    def name(off):
        i = bisect.bisect_right(offs, off) - 1
        if i < 0:
            return "$%05X" % off
        o, nm = labs[i]
        return nm if o == off else "%s+%d" % (nm, off - o)

    rows = []
    for line in open(args[0]):
        t, a, ex, inc, calls = line.split()
        a, ex, inc, calls = int(a), int(ex), int(inc), int(calls)
        rows.append((name(a) if a >= 0 else "(%s %d)" % (t, a), ex, inc, calls))
    total = sum(r[1] for r in rows)
    print("total %d cycles (%.1f frames of 29781)" % (total, total / 29781.0))
    for key, title in ((1, "exclusive"), (2, "inclusive")):
        print("-- by %s" % title)
        for r in sorted(rows, key=lambda r: -r[key])[:n]:
            print("%8d %5.1f%%  %8d calls  %s" % (r[key], 100.0 * r[key] / max(total, 1), r[3], r[0]))


if __name__ == "__main__":
    main()
