"""cocopal.py -- the CoCo 3's colours: an RGB colour as the GIME's codes.

rgb_code(rgb): the nearest RGB-monitor code (2 bits a channel, levels 0 85
170 255; the bits R1 G1 B1 R0 G0 B0).
cmp_code(rgb): the nearest composite-monitor code, from MAME's measured
composite palette (src/mame/trs/gime.cpp get_composite_color, the normal
phase), by a perceptual distance.
show(code, cmp): what a monitor shows for a code (for previews).
"""

LEVEL = (0, 85, 170, 255)

# MAME's gime.cpp composite_palette (phase normal): code -> 0xRRGGBB
COMPOSITE = [
    0x000000, 0x004c00, 0x004300, 0x0a3100, 0x2f1b00, 0x550100, 0x6c0000, 0x770006,
    0x71004b, 0x5c008b, 0x3b00b8, 0x1100ca, 0x001499, 0x002c62, 0x004011, 0x004b00,
    0x2d2d2d, 0x069800, 0x288f00, 0x537d00, 0x786700, 0xa04c00, 0xb63402, 0xc3224c,
    0xbd1693, 0xa814d5, 0x881cfe, 0x5e2cff, 0x105ee9, 0x0076b2, 0x008b60, 0x009618,
    0x747474, 0x41d714, 0x62cf00, 0x8ebd00, 0xb4a700, 0xdd8c01, 0xf5733a, 0xfe6085,
    0xfd53ce, 0xe950ff, 0xc958ff, 0x9e67ff, 0x4e9aff, 0x36b3f7, 0x26c9a3, 0x2bd558,
    0xfdfdfe, 0x88e85a, 0xa1e03f, 0xbed238, 0xd8c342, 0xf1b161, 0xfea08d, 0xfe95bf,
    0xfd8ef1, 0xef8eff, 0xd895ff, 0xb9a1ff, 0x86c4ff, 0x78d4f2, 0x71e2b6, 0xffffff,
]


def _rgb(v):
    return ((v >> 16) & 255, (v >> 8) & 255, v & 255)


def rgb_of(code):
    """an RGB-monitor code as RGB"""
    r = ((code >> 4) & 2) | ((code >> 2) & 1)
    g = ((code >> 3) & 2) | ((code >> 1) & 1)
    b = ((code >> 2) & 2) | (code & 1)
    return (LEVEL[r], LEVEL[g], LEVEL[b])


def _dist(a, b):
    # redmean: a cheap perceptual distance
    rm = (a[0] + b[0]) / 2
    dr, dg, db = a[0] - b[0], a[1] - b[1], a[2] - b[2]
    return (2 + rm / 256) * dr * dr + 4 * dg * dg + (2 + (255 - rm) / 256) * db * db


def rgb_code(rgb):
    best = min(range(64), key=lambda c: _dist(rgb_of(c), rgb))
    return best


def cmp_code(rgb):
    return min(range(64), key=lambda c: _dist(_rgb(COMPOSITE[c]), rgb))


def show(code, cmp=False):
    return _rgb(COMPOSITE[code]) if cmp else rgb_of(code)
