#!/usr/bin/env python3
"""dosfield.py -- the PC's field from a dump of its screen (tools/dossim's
`dump`), as values a line: 0 tank, 1 sky, 2 dirt, 3 fire (field.h).

    cga(data)   B800h's 16K: mode 4, even lines at 0, odd at 2000h, 80 bytes
                a line; a pixel's colour c is the value c ^ 1
                (src/msdos/cga/fld.asm's fillt/invt)
    vga(data)   A000h's 64000: a byte a pixel; the value its colour's class
                (src/msdos/vga/vga.h)
    picture(path, rows)  a PNG of values (4 a diff: red)
"""
W, H = 320, 176


def cga(data):
    rows = []
    for y in range(H):
        off = (y & 1) * 0x2000 + (y >> 1) * 80
        row = []
        for x in range(W):
            c = (data[off + (x >> 2)] >> (6 - 2 * (x & 3))) & 3
            row.append(c ^ 1)
        rows.append(row)
    return rows


def vga_class(c):
    if 1 <= c <= 63:
        return 1
    if 64 <= c <= 127:
        return 2
    if 128 <= c <= 143:
        return 3
    return 0


def vga(data):
    return [[vga_class(data[y * W + x]) for x in range(W)] for y in range(H)]


COLOURS = [(0, 160, 0), (60, 80, 255), (170, 85, 0), (255, 255, 255), (255, 0, 0)]


def picture(path, rows):
    from PIL import Image
    im = Image.new("RGB", (len(rows[0]), len(rows)))
    im.putdata([COLOURS[v] for row in rows for v in row])
    im.save(path)
