#!/usr/bin/env python3
"""drawart_chf.py -- previews of the Channel F's looks (checkpoint 1): the
field in each sky's row palettes, the HUD's two rows, the title. Rendered as
MAME shows the console (a VRAM pixel 2 wide by 4 tall, its palette rule and
colours: src/mame/fairchild/channelf_v.cpp), so a preview is what the
emulator would show.

    drawart_chf.py FLD.bin OUT.png

FLD.bin is the field as the client holds it (chfsim `save FLD 6528`: a byte
a pixel, column-major, 64 a column, port-1 codes): terrain and tanks; a
fireball and a shell's trail are added for the fire's colour.
Needs Pillow (a preview tool, not the build).
"""
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from mkfont import GLYPHS                      # noqa: E402

RGB = {"black": (0x10, 0x10, 0x10), "white": (0xFD, 0xFD, 0xFD), "red": (0xFF, 0x31, 0x53),
       "green": (0x02, 0xCC, 0x5D), "blue": (0x4B, 0x3F, 0xF3), "ltgrey": (0xE0, 0xE0, 0xE0),
       "ltgreen": (0x91, 0xFF, 0xA6), "ltblue": (0xCE, 0xD0, 0xFF)}
BG = ["black", "ltblue", "ltgrey", "ltgreen"]
INK = ["blue", "red", "green"]
FW, FH = 102, 46
SKY, TANK, FX, DIRT = 0, 1, 2, 3                # VRAM values


def colour(v, pal):
    if v == 0:
        return RGB[BG[pal]]
    return RGB["white"] if pal == 0 else RGB[INK[v - 1]]


class Screen:
    """the visible 102 x 58 (VRAM (4,4)..(105,61)): values and row palettes"""

    def __init__(self):
        self.v = [[0] * 102 for _ in range(58)]
        self.pal = [0] * 58

    def text(self, row_y, col, s, ink):
        x = 3 + 4 * col
        for ch in s.upper():
            g = GLYPHS.get(ch, GLYPHS[" "]).split(";")
            for gy, line in enumerate(g):
                for gx, c in enumerate(line):
                    if c == "#" and 0 <= x + gx < 102:
                        self.v[row_y + gy][x + gx] = ink
            x += 4

    def big(self, row_y, x, s, ink):
        """2x letters: 6 x 10, 8 a letter"""
        for ch in s.upper():
            g = GLYPHS.get(ch, GLYPHS[" "]).split(";")
            for gy, line in enumerate(g):
                for gx, c in enumerate(line):
                    if c == "#":
                        for dy in range(2):
                            for dx in range(2):
                                if 0 <= x + 2 * gx + dx < 102:
                                    self.v[row_y + 2 * gy + dy][x + 2 * gx + dx] = ink
            x += 8

    def image(self):
        im = Image.new("RGB", (204, 232))
        px = im.load()
        for y in range(58):
            for x in range(102):
                c = colour(self.v[y][x], self.pal[y])
                for dy in range(4):
                    for dx in range(2):
                        px[2 * x + dx, 4 * y + dy] = c
        return im


def load_field(path):
    b = open(path, "rb").read()
    code = {0xC0: SKY, 0x80: TANK, 0x40: FX, 0x00: DIRT}
    f = [[code.get(b[64 * x + y], 0) for x in range(FW)] for y in range(FH)]
    # a fireball and a shell's trail, for the fire's colour
    cx, cy, rx, ry = 70, 30, 7, 5
    for y in range(FH):
        for x in range(FW):
            if (x - cx) ** 2 * ry * ry + (y - cy) ** 2 * rx * rx <= rx * rx * ry * ry and f[y][x] != TANK:
                f[y][x] = FX
    for t in range(0, 40, 3):
        x, y = 20 + t, int(20 - 0.04 * (t - 20) ** 2 + 4)
        if 0 <= y < FH and f[y][x] == SKY:
            f[y][x] = FX
    return f


def top_row(f):
    for y in range(FH):
        if any(v in (DIRT, TANK) for v in f[y]):
            return y
    return FH


# each sky: a function of the field's top row -> 46 row palettes
def bands(*spec):
    """(last row, palette) bands"""
    def make(top):
        out, y = [], 0
        for last, p in spec:
            while y <= last:
                out.append(p)
                y += 1
        return out
    return make


def horizon(p_below, margin=2):
    def make(top):
        h = max(0, top - margin)
        return [0 if y < h else p_below for y in range(FH)]
    return make


SKIES = [
    ("PLAIN A: light blue", bands((45, 1))),
    ("GRADIENT A: grey over blue", bands((13, 2), (45, 1))),
    ("GRADIENT B: blue, grey, green", bands((13, 1), (29, 2), (45, 3))),
    ("STARS A: night, all white", bands((45, 0))),
    ("STARS B: black above the hills, grey", horizon(2)),
    ("STORM A: grey", bands((45, 2))),
    ("STORM B: black clouds over grey", bands((7, 0), (45, 2))),
    ("SUNSET A: blue, grey, green", bands((13, 1), (29, 2), (45, 3))),
    ("SUNSET B: green over grey", bands((15, 3), (45, 2))),
    ("CAVERN A: grey", bands((45, 2))),
    ("CAVERN B: night, all white", bands((45, 0))),
    ("BLACK A: night, all white", bands((45, 0))),
    ("BLACK B: black above the hills, blue", horizon(1)),
]


def field_screen(f, sky, hud):
    s = Screen()
    for y in range(FH):
        for x in range(FW):
            s.v[y][x] = f[y][x]
    pals = sky(top_row(f))
    for y in range(FH):
        s.pal[y] = pals[y]
    if hud == "A":                             # white on black
        for y in range(46, 58):
            s.pal[y] = 0
        s.text(46, 0, "CHF      A 90 P 750 E100", 1)
        s.text(52, 0, "MINI NUKE  X 3 > 12   27", 1)
    else:                                      # light grey, blue text, names in their seat's ink
        for y in range(46, 58):
            s.pal[y] = 2
        s.text(46, 0, "CHF     ", 2)
        s.text(46, 8, " A 90 P 750 E100", 1)
        s.text(52, 0, "MINI NUKE  X 3 > 12   27", 1)
    return s


def title_screen():
    s = Screen()
    for y in range(58):
        s.pal[y] = 1 if y < 40 else 0
    # hills
    for x in range(102):
        h = int(28 + 5 * __import__("math").sin(x / 9.0) + 3 * __import__("math").sin(x / 4.0))
        for y in range(h, 40):
            s.v[y][x] = DIRT
    # two tanks
    for tx, ty in ((18, 0), (82, 0)):
        top = next(y for y in range(40) if s.v[y][tx] == DIRT)
        for dx, dy in [(-1, -3), (0, -3), (1, -3)] + [(d, r) for d in range(-2, 3) for r in (-2, -1)]:
            s.v[top + dy][tx + dx] = TANK
        for k in (1, 2, 3):
            s.v[top - 3 - k][tx + (k if tx < 50 else -k)] = TANK
    # the arc and the burst
    for t in range(0, 61, 2):
        x = 22 + t
        y = int(22 - 0.022 * (60 * t - t * t)) - 4
        if 0 <= y < 40 and s.v[y][x] == SKY:
            s.v[y][x] = FX
    cx, cy, rx, ry = 80, 22, 5, 4
    for y in range(40):
        for x in range(102):
            if (x - cx) ** 2 * ry * ry + (y - cy) ** 2 * rx * rx <= rx * rx * ry * ry and s.v[y][x] == SKY:
                s.v[y][x] = FX
    s.big(2, 7, "FUJIBARRAGE", FX)
    s.text(42, 6, "FOR FUJINET", 1)
    s.text(48, 7, "PRESS FIRE", 1)
    return s


def main():
    f = load_field(sys.argv[1])
    out = sys.argv[2]
    tiles = []
    for name, sky in SKIES:
        tiles.append((name, field_screen(f, sky, "A").image()))
    tiles.append(("HUD B: grey band, seat ink", field_screen(f, SKIES[0][1], "B").image()))
    tiles.append(("TITLE", title_screen().image()))
    cols = 5
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * 214, rows * 252), (60, 60, 60))
    d = ImageDraw.Draw(sheet)
    for i, (name, im) in enumerate(tiles):
        x, y = (i % cols) * 214 + 5, (i // cols) * 252 + 16
        sheet.paste(im, (x, y))
        d.text((x, y - 13), name, fill=(255, 255, 255))
    sheet.save(out)
    print(out)


if __name__ == "__main__":
    main()
