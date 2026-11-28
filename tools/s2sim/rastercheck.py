#!/usr/bin/env python3
"""rastercheck.py -- the Studio II's picture against its model: the cart's
background raster (s2sim `rsave NAME.raster`: lines 0-127, 8 bytes a line)
over the field's lines 0-107 against the model (`ssave TOPS 64 NAME.tops`,
`ssave SPN 2048 NAME.spn`): lit where the model has dirt, dark elsewhere.
Taken at a quiet point (no shot playing) it must be 0, but for a smoke
trail's dots (lit over sky: reported apart).

    rastercheck.py OUTDIR [NAME ...]     (default: every *.raster)
"""
import glob
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from refcheck import model, W, H  # noqa: E402


def main():
    out = sys.argv[1]
    names = sys.argv[2:] or [os.path.basename(p)[:-7] for p in sorted(glob.glob(os.path.join(out, "*.raster")))]
    bad = 0
    for n in names:
        f = lambda e: open(os.path.join(out, n + e), "rb").read()
        m = model(f(".tops"), f(".spn"))
        ras = f(".raster")
        miss = extra = 0
        for y in range(H):
            for x in range(W):
                lit = (ras[y * 8 + x // 8] >> (7 - x % 8)) & 1
                if m[x][y] and not lit:
                    miss += 1
                elif lit and not m[x][y]:
                    extra += 1
        print("%s: %d dirt dots dark, %d sky dots lit" % (n, miss, extra))
        bad += miss > 0 or extra > 0
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
