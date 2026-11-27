#!/usr/bin/env python3
"""nesfield.py -- the NES's field store (src/nes/fld.s) from a dump of the
cartridge's WRAM ($6000-$7FFF) and the map's symbols: [y][x] values, 1 sky,
2 dirt, 3 fire (0: a code that can't be: a bad store)."""
import re

W, H = 256, 168
K_DSKY, K_FSKY, K_FDIRT, K_3D = 0, 1, 2, 3


def symbols(mapfile):
    text = open(mapfile).read()
    part = text.split("Exports list by name:")[1].split("Exports list by value:")[0]
    return {n: int(a, 16) for n, a in re.findall(r"(\S+)\s+([0-9A-F]{6})\s+\S+", part)}


def field(wram, syms):
    fmap = syms["_fmap"] - 0x6000
    pool = syms["_fpool"] - 0x6000
    kind = syms["_fkind"] - 0x6000
    f = [[0] * W for _ in range(H)]
    for r in range(H // 8):
        for c in range(W // 8):
            code = wram[fmap + r * 32 + c]
            for yy in range(8):
                y = r * 8 + yy
                if 1 <= code <= 3:
                    for xx in range(8):
                        f[y][c * 8 + xx] = code
                    continue
                if code < 4:
                    continue
                k = wram[kind + code - 4]
                p = wram[pool + (code - 4) * 8 + yy]
                q = wram[pool + (code - 3) * 8 + yy] if k == K_3D else 0
                for xx in range(8):
                    bit = (p >> (7 - xx)) & 1
                    if k == K_DSKY:
                        v = 2 if bit else 1
                    elif k == K_FSKY:
                        v = 3 if bit else 1
                    elif k == K_FDIRT:
                        v = 3 if bit else 2
                    elif k == K_3D:
                        v = 2 if bit else 3 if (q >> (7 - xx)) & 1 else 1
                    else:
                        v = 0
                    f[y][c * 8 + xx] = v
    return f
