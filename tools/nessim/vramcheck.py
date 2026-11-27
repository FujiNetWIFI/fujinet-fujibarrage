#!/usr/bin/env python3
"""vramcheck.py -- the NES's field on the screen against its store: every
field cell not marked changed (src/nes/fld.s's fld_dirty) nor a balloon's
(render.s's bln_cells) must show the store's pixels -- its own tile in its
band's CHR bank, or a constant one (src/nes/render.s's T_*) -- with the
crust (a dirt pixel with sky or fire one or two lines over it: colour 0).

    vramcheck.py WRAM.bin VRAM.bin [MAP]

WRAM.bin: nesrun's `dump F 0x6000 8192`; VRAM.bin: its `vram` (CHR-RAM 32K,
the name tables 2K, palettes, OAM). Exit 1 on a difference.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import nesfield  # noqa: E402

NT_FIELD = 4
T_SKY, T_DIRT, T_CRUST, T_FIRE, T_STAR = 96, 97, 98, 99, 100


def planes(f, r, c):
    """cell (r, c)'s tile from the store's values f[y][x]: plane 0, plane 1"""
    lo, hi = [], []
    for yy in range(8):
        y = r * 8 + yy
        b0 = b1 = 0
        for xx in range(8):
            x = c * 8 + xx
            v = f[y][x]
            if v == 2:                          # dirt: crust unless dirt two lines over
                a1 = f[y - 1][x] if y >= 1 else 2
                a2 = f[y - 2][x] if y >= 2 else 2
                col = 2 if a1 == 2 and a2 == 2 else 0
            else:
                col = {1: 1, 3: 3}.get(v, 0)
            b0 |= (col & 1) << (7 - xx)
            b1 |= (col >> 1) << (7 - xx)
        lo.append(b0)
        hi.append(b1)
    return lo + hi


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    wram = open(sys.argv[1], "rb").read()
    vram = open(sys.argv[2], "rb").read()
    mp = sys.argv[3] if len(sys.argv) > 3 else os.path.join(HERE, "..", "..", "build", "nes", "barrage.map")
    syms = nesfield.symbols(mp)
    f = nesfield.field(wram, syms)
    # (rows over 0: dirt, so row 0 has no crust from outside)
    chr_, nt = vram[:32768], vram[32768:34816]
    dirty = syms["_fld_dirty"] - 0x6000
    bln = syms["_bln_cells"] - 0x6000
    bad = skipped = 0
    for r in range(21):
        b, k = r // 3, r % 3
        for c in range(32):
            bit = 0x80 >> (c & 7)
            if wram[dirty + r * 4 + c // 8] & bit or wram[bln + r * 4 + c // 8] & bit:
                skipped += 1
                continue
            want = planes(f, r, c)
            name = nt[(r + NT_FIELD) * 32 + c]
            own = (b & 1) * 128 + k * 32 + c
            if name != own and (name >> 7) != (b & 1):
                got = None
            else:
                t = name & 127
                got = list(chr_[b * 2048 + t * 16: b * 2048 + t * 16 + 16])
                if T_STAR <= t < T_STAR + 8 and all(v == 1 for row in f[r * 8:r * 8 + 8]
                                                    for v in row[c * 8:c * 8 + 8]):
                    got = want                  # (a star in a sky cell)
            if got != want:
                if bad < 8:
                    print("cell %d,%d: name %02X, tile %s, want %s" % (
                        r, c, name, "?" if got is None else bytes(got).hex(), bytes(want).hex()))
                bad += 1
    print("vram: %d cells differ (%d skipped)" % (bad, skipped))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
