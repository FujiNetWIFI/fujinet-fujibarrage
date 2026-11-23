#!/usr/bin/env python3
"""adamfield.py -- the Adam's field store (src/tms9918/fld.asm) from a RAM
dump: two planes, D (dirt) and F (fire), in the VDP's pattern layout from
A700h, character row r's D at page A7h + 2r and its F the page after.

    field(ram) -> [y][x] values: 1 sky, 2 dirt, 3 fire
    picture(path, f)   a PNG (sky blue, dirt brown, fire yellow; 4: red)
"""
SHADOW = 0xA700
W, H = 256, 168


def field(ram):
    f = []
    for y in range(H):
        row = []
        base = SHADOW + (y >> 3) * 512 + (y & 7)
        for x in range(W):
            a = base + (x & 0xF8)
            bit = 0x80 >> (x & 7)
            if ram[a + 256] & bit:
                row.append(3)
            elif ram[a] & bit:
                row.append(2)
            else:
                row.append(1)
        f.append(row)
    return f


def picture(path, f, scale=2):
    from PIL import Image
    pal = {0: (0, 0, 0), 1: (80, 120, 220), 2: (150, 100, 40), 3: (255, 230, 60), 4: (255, 0, 0)}
    im = Image.new("RGB", (len(f[0]), len(f)))
    for y, row in enumerate(f):
        for x, v in enumerate(row):
            im.putpixel((x, y), pal.get(v, (255, 0, 255)))
    im.resize((im.width * scale, im.height * scale), Image.NEAREST).save(path)
