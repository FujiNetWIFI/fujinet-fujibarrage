#!/usr/bin/env python3
"""fieldcheck.py -- the Channel F's field against what the server sent it.

    fieldcheck.py OUTDIR [NAME ...]

OUTDIR holds tap.log (tools/rigs/tapproxy.py: the server's frames) and, for
each NAME (default: every fld-*.bin), NAME.bin, FLD dumped (column-major, 64
bytes a column, port-1 codes: chfsim `save FLD 6528 NAME.bin`) and
NAME.ntv (the client's NTV: `save NTV 1 NAME.ntv`). The field of terrain
version NTV is rebuilt from the log's G frames (and a shot's SETTLE ops,
for a version bump with no columns: tools/arc/fieldcheck.py's rule) and
compared pixel for pixel, sky against dirt; the client's tank and fire
pixels (blue, red) are left out and counted.
"""
import glob
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "arc"))
import fieldcheck as arcfc                 # noqa: E402  (versions(), settles())

FW, FH = 102, 46
arcfc.FW, arcfc.FH = FW, FH
SKY, TANK, FX, DIRT = 0xC0, 0x80, 0x40, 0x00


def main():
    out = sys.argv[1]
    names = sys.argv[2:] or sorted(os.path.basename(f)[:-4] for f in glob.glob(os.path.join(out, "fld-*.bin")))
    vers = arcfc.versions(os.path.join(out, "tap.log"))
    have = {}
    for tv, cols in vers:
        have[tv] = cols
    bad = 0
    for n in names:
        fld = open(os.path.join(out, n + ".bin"), "rb").read()
        ntv = open(os.path.join(out, n + ".ntv"), "rb").read()[0]
        if ntv not in have:
            print("%s: no whole terrain version %02X in the log" % (n, ntv))
            bad += 1
            continue
        cols = have[ntv]
        diff, other, first = 0, 0, None
        for x in range(FW):
            for y in range(FH):
                v = fld[64 * x + y]
                want = DIRT if any(t <= y < e for t, e in cols[x]) else SKY
                if v in (TANK, FX):
                    other += 1
                    continue
                if v != want:
                    diff += 1
                    if first is None:
                        first = (x, y, v, want)
        print("%s: tv %02X, %d px differ%s, %d tank/fire px left out"
              % (n, ntv, diff, (" (first %d,%d: %02X, want %02X)" % first) if first else "", other))
        bad += diff > 0
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
