#!/usr/bin/env python3
"""hgrfield.py -- the field's values from a dump of HGR page 1 (a2sim's
`dump hgr.bin $2000 8192`): what src/apple2/fld.s wrote, pixel by pixel.

usage: hgrfield.py HGR.BIN [OUT.png [SCALE]]

A colour pixel x of line y is dots 2x (bit 0 of the value) and 2x + 1
(bit 1): 0 tank, 1 sky, 2 dirt, 3 fire. With OUT, a picture of them at
4:3 (a pixel 2 dots wide, a line 2.1875 dots tall), in the line's palette
(bit 7 of its first byte): blue/orange or violet/green. As a module:
field(data) -> rows of values, palette(data, y) -> 0 or 1.
"""
import sys

W, H = 140, 160
COL = {1: {0: (0, 0, 0), 1: (20, 140, 255), 2: (230, 110, 0), 3: (255, 255, 255)},
       0: {0: (0, 0, 0), 1: (220, 40, 255), 2: (20, 210, 0), 3: (255, 255, 255)}}


def line_addr(y):
    return (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28


def dot(data, y, d):
    return (data[line_addr(y) + d // 7] >> (d % 7)) & 1


def field(data):
    return [[dot(data, y, 2 * x) | dot(data, y, 2 * x + 1) << 1 for x in range(W)] for y in range(H)]


def palette(data, y):
    return data[line_addr(y)] >> 7


def main():
    data = open(sys.argv[1], "rb").read()
    f = field(data)
    if len(sys.argv) < 3:
        for row in f:
            print("".join(".~#*"[v] if v else "T" for v in row))
        return
    from PIL import Image
    s = int(sys.argv[3]) if len(sys.argv) > 3 else 4
    im = Image.new("RGB", (W, H))
    im.putdata([COL[palette(data, y)][v] for y, row in enumerate(f) for v in row])
    im.resize((W * 2 * s, round(H * 2.1875 * s)), Image.NEAREST).save(sys.argv[2])


if __name__ == "__main__":
    main()
