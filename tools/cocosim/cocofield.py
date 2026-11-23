#!/usr/bin/env python3
"""cocofield.py -- the CoCo's field as values, from cocosim's dumps.

    field2(data, swapped)   the CoCo 1/2's: the PMODE 4 screen at $1000
                            (`dump 1000 27FF screen.bin`), 128 x 168; a
                            colour pixel's bit pair is its value, 01 and 10
                            swapped when the artifact phase is (fswap, $0E38)
    field3(blocks)          the CoCo 3's: blocks $30-$35 (`dumpblk 30 b0.bin`
                            ...), 320 x 176, two pixels a byte, lines 256
                            bytes apart; colour -> value as fld3.asm's dec
    picture(path, f, scale) a PNG of values: 0 tank black, 1 sky blue,
                            2 dirt brown, 3 fire white, 4 a difference red

Values: 0 tank, 1 sky, 2 dirt, 3 fire (src/platform-specific/field.h PX_*).
"""

W2, H2 = 128, 168
W3, H3 = 320, 176
DEC3 = [0, 1, 1, 2, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
COLOURS = [(0, 0, 0), (80, 140, 230), (150, 90, 30), (255, 255, 255), (255, 0, 0)]


def field2(data, swapped=False):
    f = []
    for y in range(H2):
        row = []
        for x in range(W2):
            b = data[y * 32 + x // 4]
            v = (b >> (6 - 2 * (x & 3))) & 3
            if swapped and v in (1, 2):
                v ^= 3
            row.append(v)
        f.append(row)
    return f


def field3(blocks):
    """blocks: the bytes of blocks $30, $31, ... (8K each)"""
    f = []
    for y in range(H3):
        blk = blocks[y >> 5]
        base = (y & 31) * 256
        row = []
        for x in range(W3):
            b = blk[base + (x >> 1)]
            c = b >> 4 if x % 2 == 0 else b & 15
            row.append(DEC3[c])
        f.append(row)
    return f


def picture(path, f, scale=3):
    from PIL import Image
    h, w = len(f), len(f[0])
    im = Image.new("RGB", (w, h))
    im.putdata([COLOURS[v] for row in f for v in row])
    im.resize((w * scale, h * scale), Image.NEAREST).save(path)
