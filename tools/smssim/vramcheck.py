#!/usr/bin/env python3
"""vramcheck.py -- the SMS's screen against its store: every field cell's
tile (rows 0-20 of the name table), pixel for pixel, is the class the store
says (src/tms9918/fld.asm) in the colour its line gives it (render.asm):
sky -> ln_sky[y] (or the star, 6), dirt -> ln_dirt[y] (or, with the crust,
7 where one of the two lines over it isn't dirt: over the field is dirt),
fire -> 11 + the fire's index by the line in the cell (fire_ix), all
palette 0, no flips.
Cells under a balloon (flag bit 1), dirty ones (bit 0: not drawn yet) and
those under a tank drawn in the tiles (bit 2: the sprites had no room) are
skipped and counted.

usage: vramcheck.py DIR [MAP]   (DIR: vram.bin, store.bin, ram.bin = RAM
                                 C000h-DFFFh, tc.bin the cart's RAM half 1;
                                 MAP z88dk's map)
Exits non-zero if a drawn cell differs.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.abspath(os.path.join(HERE, "..", ".."))
FIRE_IX = [11, 12, 13, 11, 12, 13, 11, 12]
STAR = 6


def symbols(path):
    out = {}
    for line in open(path):
        m = re.match(r"(\S+)\s+=\s+\$([0-9A-Fa-f]+)", line)
        if m:
            out[m.group(1)] = int(m.group(2), 16)
    return out


def tile_px(vram, t):
    rows = []
    for y in range(8):
        p = vram[t * 32 + y * 4: t * 32 + y * 4 + 4]
        rows.append([sum(((p[b] >> (7 - x)) & 1) << b for b in range(4)) for x in range(8)])
    return rows


def main():
    d = sys.argv[1]
    sym = symbols(sys.argv[2] if len(sys.argv) > 2 else os.path.join(TOP, "build", "sms", "barrage.map"))
    vram = open(os.path.join(d, "vram.bin"), "rb").read()
    store = open(os.path.join(d, "store.bin"), "rb").read()
    ram = open(os.path.join(d, "ram.bin"), "rb").read()
    at = lambda name: sym[name] - 0xC000
    tc = open(os.path.join(d, "tc.bin"), "rb").read()        # the cart's RAM, half 1 (the line tables)
    ln_sky = tc[sym["_ln_sky"] - 0x8000:sym["_ln_sky"] - 0x8000 + 168]
    ln_dirt = tc[sym["_ln_dirt"] - 0x8000:sym["_ln_dirt"] - 0x8000 + 168]
    flags = ram[at("_fld_dirty"):at("_fld_dirty") + 21 * 32]
    crust = ram[at("_sms_crust")]

    def dirt(x, y):
        if y < 0:
            return True
        return bool(store[(y >> 3) * 512 + (x & 0xF8) + (y & 7)] & (0x80 >> (x & 7)))
    bad = skipped = 0
    for r in range(21):
        for c in range(32):
            fl = flags[r * 32 + c]
            if fl & 0x77 or (crust and r and flags[(r - 1) * 32 + c] & 1):
                skipped += 1            # (the crust waits for the cell over it)
                continue
            w = vram[0x3800 + (r * 32 + c) * 2] | vram[0x3800 + (r * 32 + c) * 2 + 1] << 8
            if w & 0xFE00:
                bad += 1
                print("cell %d,%d: name %04X (flips or palette 1)" % (r, c, w))
                continue
            px = tile_px(vram, w & 0x1FF)
            nd = 0
            for yy in range(8):
                y = r * 8 + yy
                db = store[r * 512 + c * 8 + yy]
                fb = store[r * 512 + 256 + c * 8 + yy]
                for xx in range(8):
                    bit = 0x80 >> xx
                    v = px[yy][xx]
                    if fb & bit:
                        want = {FIRE_IX[yy]}
                    elif db & bit:
                        x = c * 8 + xx
                        want = {7} if crust and not (dirt(x, y - 1) and dirt(x, y - 2)) else {ln_dirt[y]}
                    else:
                        want = {ln_sky[y], STAR}
                    if v not in want:
                        nd += 1
            if nd:
                bad += 1
                print("cell %d,%d (tile %d): %d pixels differ" % (r, c, w & 0x1FF, nd))
    print("%d cells differ, %d skipped (balloon, tank or not drawn)" % (bad, skipped))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
