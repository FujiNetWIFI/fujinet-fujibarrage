#!/usr/bin/env python3
"""vramcheck.py -- the field's cells in the VRAM against what render.asm
should have made of the store and the line styles (st_line, style.inc).

    vramcheck.py RAM.bin VRAM.bin [MAP]

RAM.bin and VRAM.bin from one moment of an adamrun run (`dump`, `vram`);
MAP the game's (build/adam/game.map). Cells still dirty, under a balloon
or a bitmap tank (flags 1, 2, 4) are skipped. Exit 1 on a difference.
"""
import re
import sys

L_SKYC, L_SKYP, L_DIRTC, L_DIRTP, L_MIXC, L_SIZE = range(6)
SH_HI = 0xA7


def syms(path):
    out = {}
    for l in open(path):
        m = re.match(r'(\S+)\s+=\s+\$([0-9A-F]+) ; addr', l)
        if m:
            out[m.group(1)] = int(m.group(2), 16)
    return out


def main():
    ram = open(sys.argv[1], "rb").read()
    vram = open(sys.argv[2], "rb").read()
    s = syms(sys.argv[3] if len(sys.argv) > 3 else "build/adam/game.map")
    line, fire, starb = s["_st_line"], s["_st_fire"], s["_st_starb"]
    stars, ph, dirty = ram[s["_st_stars"]], ram[s["_fire_ph"]], s["_fld_dirty"]
    bad = skip = 0
    for r in range(21):
        for c in range(32):
            if ram[dirty + r * 32 + c] & 7:
                skip += 1
                continue
            c37 = (c * 37) & 255
            for i in range(8):
                y = r * 8 + i
                st = ram[line + y * L_SIZE:line + y * L_SIZE + L_SIZE]
                d = ram[(SH_HI + 2 * r) * 256 + c * 8 + i]
                f = ram[(SH_HI + 2 * r + 1) * 256 + c * 8 + i]
                if f:
                    dirt = bin(d & ~f & 255).count("1")
                    sky = bin(~(d | f) & 255).count("1")
                    base = st[L_MIXC] & 15 if sky >= dirt else st[L_MIXC] >> 4
                    pat, col = f, ram[fire + (((y >> 3) + ph) & 7)] | base
                elif d == 0:
                    pat = ram[starb + ((st[L_SKYP] + c37) & 255)] if stars else st[L_SKYP]
                    col = st[L_SKYC]
                elif d == 255:
                    pat, col = st[L_DIRTP], st[L_DIRTC]
                else:
                    pat, col = d, st[L_MIXC]
                at = r * 256 + c * 8 + i
                if vram[at] != pat or vram[0x2000 + at] != col:
                    if bad < 8:
                        print("cell %d,%d line %d: %02X/%02X, want %02X/%02X"
                              % (r, c, i, vram[at], vram[0x2000 + at], pat, col))
                    bad += 1
    print("vram: %d lines differ (%d cells skipped)" % (bad, skip))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
