#!/usr/bin/env python3
"""mktitle_sms.py OUT.bin [PREVIEW.png] -- the Master System's title picture:
the PC's sunset (tools/msdos/mktitle_dos.py, after tools/mktitle_coco.py:
two tanks, one's shell arcing over to burst by the other, FUJIBARRAGE over
it all) drawn at 256x192 in the SMS's 64 colours, then made Mode 4 tiles:
each 8x8 tile takes one of two 16-colour palettes (fitted here, a tile's
colours moved to the nearest of its palette where they must), tiles alike
(or alike flipped) shared.

OUT.bin, what src/sms/scr.asm's scr_picture copies to the VRAM:
    0       the tile count N (2 bytes, little-endian)
    2       the 32 colours (CRAM: palette 0, palette 1)
    34      the names, 32 x 24 words (tile | flips << 9 | palette << 11)
    1570    the N tiles, 32 bytes each (4 planes a line)
"""
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import mktitle_coco as mt     # noqa: E402

W, H, PAR = 256, 192, 73
MAX_TILES = 448
ROWS = ["....SSSS....", "...SSHHSS...", ".SSSSSSSSSS.", "SSSSSSSSSSSS", "KSKSKSKSKSKS", ".KKKKKKKKKK."]
PRESS = "PRESS 1"
HINT = "2+PAUSE: LOBBY"


def c(r, g, b):
    """an SMS colour: 2 bits a channel (0, 85, 170, 255)"""
    return (r * 85, g * 85, b * 85)


def draw():
    cv = mt.Canvas(W, H, c(0, 0, 0))
    horizon = 140
    # the sky: night to a sunset's glow, in the SMS's own steps, a band each
    sky = [c(0, 0, 1), c(0, 0, 1), c(1, 0, 1), c(1, 0, 2), c(2, 0, 2), c(2, 0, 1), c(3, 0, 1),
           c(3, 1, 1), c(3, 1, 0), c(3, 2, 0), c(3, 2, 1)]
    for y in range(H):
        k = min(len(sky) - 1, y * len(sky) // horizon)
        for x in range(W):
            cv.set(x, y, sky[k])
    rnd = random.Random(7)
    for _ in range(46):                     # stars up in the night
        x, y = rnd.randrange(W), rnd.randrange(44)
        cv.set(x, y, rnd.choice([c(3, 3, 3), c(2, 2, 3)]))
    sx, sy, sr = 176, horizon, 28           # the sun on the horizon, banded
    sun = [c(3, 3, 2), c(3, 3, 1), c(3, 3, 0), c(3, 2, 0)]
    for y in range(sy - sr, sy):
        if (sy - y) % 6 == 1 and y > sy - 18:
            continue
        for x in range(sx - 40, sx + 41):
            if ((x - sx) * PAR / 64) ** 2 + (y - sy) ** 2 <= sr * sr:
                cv.set(x, y, sun[min(3, (y - (sy - sr)) * 4 // sr)])
    farh = mt.hills(W, 130, 12, 0.039, 0.096, 1.3)
    far = [c(1, 0, 1), c(1, 0, 1), c(0, 0, 1)]
    for x in range(W):
        for y in range(farh[x], H):
            cv.set(x, y, far[min(2, (y - farh[x]) * 3 // 30)])
    nearh = mt.hills(W, 152, 10, 0.025, 0.062, 0.4)
    near = [c(2, 2, 0), c(2, 1, 0), c(1, 1, 0), c(1, 0, 0), c(0, 0, 0)]
    for x in range(W):
        for y in range(nearh[x], H):
            d = y - nearh[x]
            if d < 2:
                col = c(3, 3, 1) if d == 0 else c(2, 2, 0)
            else:
                lv = min(4, (y - 146) * 5 // 46)
                if (y // 5) % 3 == 0 and d > 5:
                    lv = min(4, lv + 1)
                col = near[max(0, lv)]
            cv.set(x, y, col)
    lx, rx = 50, 212
    red = {"S": c(3, 0, 0), "K": c(0, 0, 0), "H": c(3, 2, 2)}
    blue = {"S": c(0, 1, 3), "K": c(0, 0, 0), "H": c(2, 3, 3)}
    mt.tank(cv, lx, nearh[lx] + 1, ROWS, red, 2, 2, 52, PAR, 9, 2, c(3, 0, 0))
    mt.tank(cv, rx, nearh[rx] + 1, ROWS, blue, 2, 2, 150, PAR, 9, 2, c(0, 1, 3))
    pts = mt.arc(lx + 10, nearh[lx] - 18, rx - 28, nearh[rx - 28] - 2, 80, 50)
    for k, (x, y) in enumerate(pts[:-3]):
        if k % 2 == 0:
            cv.set(int(x), int(y), c(3, 3, 3))
            cv.set(int(x) + 1, int(y), c(3, 3, 1))
    bx, by = int(pts[-1][0]), int(pts[-1][1])
    mt.burst(cv, bx, by, 11, PAR, [(0.3, c(3, 3, 3)), (0.5, c(3, 3, 1)), (0.7, c(3, 3, 0)),
                                   (0.85, c(3, 2, 0)), (1.01, c(3, 1, 0))])
    f = mt.font3()
    b3 = lambda ch: mt.glyph_bits3(f, ch)
    s = "FUJIBARRAGE"
    sxl, syl, cw = 3, 3, 7
    wl = len(s) * cw * sxl
    logo = [c(3, 3, 3), c(3, 3, 2), c(3, 3, 1), c(3, 3, 0), c(3, 2, 0), c(3, 1, 0), c(3, 0, 0), c(2, 0, 0)]
    mt.logo(cv, b3, s, (W - wl) // 2 + 1, 14, sxl, syl, cw,
            lambda x, y: logo[min(7, y * 8 // (8 * syl))], c(0, 0, 0), c(1, 0, 1))
    tag = "AIM, FIRE, TALK TRASH."
    tw = len(tag) * 8
    mt.text(cv, b3, tag, (W - tw) // 2 + 1, 49, c(0, 0, 0))
    mt.text(cv, b3, tag, (W - tw) // 2, 48, c(3, 3, 3))
    pw = len(PRESS) * 8
    cv.rect((W - pw) // 2 - 6, 168, (W + pw) // 2 + 6, 180, c(0, 0, 0))
    mt.text(cv, b3, PRESS, (W - pw) // 2, 170, c(3, 3, 0))
    cv.rect(0, 182, W, H, c(0, 0, 0))
    mt.text(cv, b3, HINT, (W - len(HINT) * 8) // 2, 183, c(2, 2, 2))
    return cv


def cram(col):
    r, g, b = (v // 85 for v in col)
    return r | g << 2 | b << 4


def dist(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b))


def tiles_of(cv):
    out = []
    for ty in range(24):
        for tx in range(32):
            out.append([cv.px[ty * 8 + y][tx * 8 + x] for y in range(8) for x in range(8)])
    return out


def pick16(hist):
    """the 16 colours (of those in hist: colour -> pixels) that leave the
    least error when every pixel takes its nearest: greedy"""
    cols = list(hist)
    if len(cols) <= 16:
        return cols
    chosen = []
    best = {q: 1 << 30 for q in cols}       # each colour's distance to the nearest chosen
    while len(chosen) < 16:
        def gain(k):
            return sum(hist[q] * max(0, best[q] - dist(q, k)) for q in cols)
        k = max((q for q in cols if q not in chosen), key=gain)
        chosen.append(k)
        for q in cols:
            best[q] = min(best[q], dist(q, k))
    return chosen


def fit(tiles):
    """two palettes of 16, each tile on the one that suits it best"""
    total = {}
    for t in tiles:
        for p in t:
            total[p] = total.get(p, 0) + 1
    if len(total) <= 16:
        return [list(total), []], [0] * len(tiles)
    # start: split the tiles by their commonest colour's brightness
    which = [0 if sum(max(set(t), key=t.count)) > 300 else 1 for t in tiles]
    for _ in range(12):
        pals = []
        for k in (0, 1):
            hist = {}
            for t, w in zip(tiles, which):
                if w == k:
                    for p in t:
                        hist[p] = hist.get(p, 0) + 1
            pals.append(pick16(hist) if hist else [])
        new = []
        for t in tiles:
            errs = [sum(min(dist(p, q) for q in pal) for p in t) if pal else 1 << 30 for pal in pals]
            new.append(0 if errs[0] <= errs[1] else 1)
        if new == which:
            break
        which = new
    return pals, which


def main():
    out = sys.argv[1]
    cv = draw()
    tiles = tiles_of(cv)
    pals, which = fit(tiles)
    for p in pals:
        while len(p) < 16:
            p.append((0, 0, 0))
    # black first in palette 1: it is the border's (register 7: colour 16)
    k = min(range(16), key=lambda q: dist(pals[1][q], (0, 0, 0)))
    if pals[1][k] == (0, 0, 0):
        pals[1][0], pals[1][k] = pals[1][k], pals[1][0]
    else:
        pals[1][min(range(16), key=lambda q: sum(w == 1 and pals[1][q] in t for t, w in zip(tiles, which)))] = (0, 0, 0)
        pals[1][0], pals[1][k] = pals[1][k], pals[1][0]
    idx = []
    err = 0
    for t, w in zip(tiles, which):
        pal = pals[w]
        row = []
        for p in t:
            j = min(range(16), key=lambda q: dist(p, pal[q]))
            err += pal[j] != p
            row.append(j)
        idx.append(row)
    # dedup with flips: a tile, its mirror, its upside-down, both
    def flips(t):
        rows = [t[y * 8:(y + 1) * 8] for y in range(8)]
        h = [r[::-1] for r in rows]
        return [(0, rows), (1, h), (2, rows[::-1]), (3, h[::-1])]
    seen, uniq, names = {}, [], []
    for t, w in zip(idx, which):
        name = None
        for fl, rows in flips(t):
            key = tuple(v for r in rows for v in r)
            if key in seen:
                name = seen[key] | fl << 9 | w << 11
                break
        if name is None:
            n = len(uniq)
            seen[tuple(t)] = n
            uniq.append(t)
            name = n | w << 11
        names.append(name)
    assert len(uniq) <= MAX_TILES, len(uniq)
    data = bytearray([len(uniq) & 255, len(uniq) >> 8])
    data += bytes(cram(col) for p in pals for col in p)
    for n in names:
        data += bytes([n & 255, n >> 8])
    for t in uniq:
        for y in range(8):
            for b in range(4):
                data.append(sum(((t[y * 8 + x] >> b) & 1) << (7 - x) for x in range(8)))
    open(out, "wb").write(data)
    print("%s: %d tiles, %d bytes, %d pixels moved to their palette's nearest" % (out, len(uniq), len(data), err))
    if len(sys.argv) > 2:
        from PIL import Image
        im = Image.new("RGB", (W, H))
        for i, (t, w) in enumerate(zip(idx, which)):
            tx, ty = i % 32, i // 32
            for k, v in enumerate(t):
                im.putpixel((tx * 8 + k % 8, ty * 8 + k // 8), pals[w][v])
        im.resize((W * 2 * 8 // 7, H * 2), Image.NEAREST).save(sys.argv[2])


if __name__ == "__main__":
    main()
