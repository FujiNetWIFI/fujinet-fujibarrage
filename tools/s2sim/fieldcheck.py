#!/usr/bin/env python3
"""fieldcheck.py -- the Studio II's field model against what the server sent.

    fieldcheck.py OUTDIR [NAME ...]

OUTDIR holds tap.log (tools/rigs/tapproxy.py: the server's frames) and, for
each NAME (default: every fld-*), NAME.tops (TOPS, 64 bytes: s2sim `ssave
TOPS 64 NAME.tops`), NAME.spn (SPN, 2048: `ssave SPN 2048 NAME.spn`) and
NAME.ntv (the client's NTV: `save NTV 1 NAME.ntv`). The field of terrain
version NTV is rebuilt from the log's G frames and a shot's SETTLE ops
(tools/arc/fieldcheck.py's versions()) and compared with the model's
columns, pixel for pixel.
"""
import glob
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "arc"))
import fieldcheck as arcfc                 # noqa: E402

FW, FH = 64, 108
arcfc.FW, arcfc.FH = FW, FH


def model(tops, spn):
    """the model's columns: (top, end) spans, as fld.inc reads them"""
    cols = []
    for x in range(FW):
        t = tops[x]
        if t < 0xF0:
            cols.append([(t, FH)] if t < FH else [])
        elif t == 0xF0:
            cols.append([])
        else:
            slot = (x >> 3) * 256 + (x & 7) * 32
            n = t - 0xF0
            cols.append([(spn[slot + 2 * k], min(spn[slot + 2 * k + 1], FH)) for k in range(n)])
    return cols


def main():
    out = sys.argv[1]
    names = sys.argv[2:] or sorted(os.path.basename(f)[:-5] for f in glob.glob(os.path.join(out, "fld-*.tops")))
    have = dict(arcfc.versions(os.path.join(out, "tap.log")))
    bad = 0
    for n in names:
        tops = open(os.path.join(out, n + ".tops"), "rb").read()
        spn = open(os.path.join(out, n + ".spn"), "rb").read()
        ntv = open(os.path.join(out, n + ".ntv"), "rb").read()[0]
        if ntv not in have:
            print("%s: no whole terrain version %02X in the log" % (n, ntv))
            bad += 1
            continue
        want, ours = have[ntv], model(tops, spn)
        diff, first = 0, None
        for x in range(FW):
            for y in range(FH):
                a = any(t <= y < e for t, e in want[x])
                b = any(t <= y < e for t, e in ours[x])
                if a != b:
                    diff += 1
                    if first is None:
                        first = (x, y, b, a)
        print("%s: tv %02X, %d px differ%s" % (n, ntv, diff, (" (first %d,%d: ours %d, want %d)" % first) if first else ""))
        bad += diff > 0
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
