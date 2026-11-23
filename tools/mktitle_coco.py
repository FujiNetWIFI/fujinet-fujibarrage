#!/usr/bin/env python3
"""mktitle_coco.py -- the CoCo's title pictures, drawn here and packed
(lz_pack: src/coco/lzd.asm unpacks them) into the menu program's art.

    mktitle_coco.py title2 OUT.asm [PREVIEW.png]   the CoCo 1/2's: PMODE 4,
        128 x 192 colour pixels in the four artifact colours (0 black, 1
        blue, 2 orange, 3 white: the bit pairs; src/coco2/video.c swaps 1
        and 2 for the other phase), the screen at $1000 as one stream
    mktitle_coco.py title3 OUT.asm [PREVIEW.png]   the CoCo 3's: 320 x 200
        in 16 colours of its own (RGB and composite codes), the GIME's
        lines 256 bytes apart: one stream an 8K block (32 lines)

A sunset over the hills: two tanks, one's shell arcing over to burst by the
other, FUJIBARRAGE over it all, the tagline, PRESS FIRE.

The packing: a control byte c < $80 is c + 1 bytes as they are; $81-$FF
copies (c & $7F) + 2 bytes from D + 1 back (D: the next two bytes, high
first) -- never from before the stream's start; $80 ends it.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
sys.path.insert(0, HERE)

BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


# ---- packing -----------------------------------------------------------------

def lz_pack(data, window=8192):
    out, lit = bytearray(), bytearray()
    heads = {}
    i, n = 0, len(data)

    def flush():
        k = 0
        while k < len(lit):
            part = lit[k:k + 128]
            out.append(len(part) - 1)
            out.extend(part)
            k += 128
        lit.clear()

    while i < n:
        best, bd = 0, 0
        if i + 3 <= n:
            key = bytes(data[i:i + 3])
            for p in reversed(heads.get(key, [])[-48:]):
                if i - p > window:
                    break
                ln = 0
                while ln < 129 and i + ln < n and data[p + ln] == data[i + ln]:
                    ln += 1
                if ln > best:
                    best, bd = ln, i - p
                    if ln == 129:
                        break
        if best >= 4:
            flush()
            out.append(0x80 | (best - 2))
            out.append((bd - 1) >> 8)
            out.append((bd - 1) & 255)
            step = best
        else:
            lit.append(data[i])
            step = 1
        for k in range(i, min(i + step, n - 2)):
            heads.setdefault(bytes(data[k:k + 3]), []).append(k)
        i += step
    flush()
    out.append(0x80)
    return bytes(out)


def lz_unpack(s):
    out, i = bytearray(), 0
    while True:
        c = s[i]
        i += 1
        if c < 0x80:
            out.extend(s[i:i + c + 1])
            i += c + 1
        elif c == 0x80:
            return bytes(out)
        else:
            d = (s[i] << 8 | s[i + 1]) + 1
            i += 2
            for _ in range((c & 0x7F) + 2):
                out.append(out[-d])


# ---- drawing -------------------------------------------------------------------

class Canvas:
    def __init__(self, w, h, c=0):
        self.w, self.h = w, h
        self.px = [[c] * w for _ in range(h)]

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x] = c

    def get(self, x, y):
        return self.px[y][x] if 0 <= x < self.w and 0 <= y < self.h else None

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1):
            for x in range(x0, x1):
                self.set(x, y, c)

    def dither(self, x0, y0, x1, y1, a, b, level):
        """level 0-16: how much of b"""
        for y in range(y0, y1):
            for x in range(x0, x1):
                self.set(x, y, b if level > BAYER[y & 3][x & 3] else a)


def font3():
    return open(os.path.join(ROOT, "gfx", "coco", "namco.bin"), "rb").read()


def font2():
    from PIL import Image
    im = Image.open(os.path.join(ROOT, "gfx", "coco", "font3x7.png")).convert("L")
    px = im.load()
    g = []
    for k in range(64):
        rows = []
        for y in range(8):
            rows.append(sum(8 >> x for x in range(4) if px[k * 4 + x, y] > 128))
        g.append(rows)
    return g


def glyph_bits3(f, ch):
    c = ord(ch)
    if c < 32 or c > 127:
        c = 32
    return [[(f[(c - 32) * 8 + y] >> (7 - x)) & 1 for x in range(8)] for y in range(8)]


def glyph_bits2(f, ch):
    c = ord(ch.upper())
    if c < 32 or c > 95:
        c = 32
    return [[(f[c - 32][y] >> (3 - x)) & 1 for x in range(4)] for y in range(8)]


def text(cv, bits_of, s, x, y, c, sx=1, sy=1, cw=8):
    for k, ch in enumerate(s):
        g = bits_of(ch)
        for gy, row in enumerate(g):
            for gx, b in enumerate(row):
                if b:
                    for yy in range(sy):
                        for xx in range(sx):
                            cv.set(x + k * cw * sx + gx * sx + xx, y + gy * sy + yy, c)


def mask_text(w, h, bits_of, s, sx, sy, cw):
    m = Canvas(w, h, 0)
    text(m, bits_of, s, 0, 0, 1, sx, sy, cw)
    return m


def logo(cv, bits_of, s, x0, y0, sx, sy, cw, fill, outline, shadow=None):
    """big letters: an outline round them, a shadow under, fill(row) inside"""
    w, h = len(s) * cw * sx + 4, 8 * sy + 4
    m = mask_text(w, h, bits_of, s, sx, sy, cw)
    on = lambda x, y: 0 <= x < w and 0 <= y < h and m.px[y][x]
    if shadow is not None:
        for y in range(h):
            for x in range(w):
                if on(x, y):
                    cv.set(x0 + x + 2, y0 + y + 2, shadow)
    for y in range(-1, h + 1):
        for x in range(-1, w + 1):
            if not on(x, y) and any(on(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                cv.set(x0 + x, y0 + y, outline)
    for y in range(h):
        for x in range(w):
            if on(x, y):
                cv.set(x0 + x, y0 + y, fill(x, y))


def hills(w, base, amp, f1, f2, ph):
    return [int(base + amp * (0.6 * math.sin(x * f1 + ph) + 0.4 * math.sin(x * f2 + 2 * ph))) for x in range(w)]


def tank(cv, cx, foot, rows, colours, sx, sy, barrel_deg, par, length, thick, bcol, outline=None):
    """a shape (rows of characters -> colours) sx by sy a pixel, its foot's
    centre at (cx, foot); a barrel from its turret's top; an outline round
    it all (a colour) if asked"""
    h, w = len(rows), len(rows[0])
    x0, y0 = cx - w * sx // 2, foot - h * sy
    if outline is not None:
        sub = Canvas(cv.w, cv.h, None)
        tank(sub, cx, foot, rows, colours, sx, sy, barrel_deg, par, length, thick, bcol)
        for y in range(cv.h):
            for x in range(cv.w):
                if sub.px[y][x] is None and any(sub.get(x + dx, y + dy) not in (None,)
                                                for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                    cv.set(x, y, outline)
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            c = colours.get(ch)
            if c is not None:
                cv.rect(x0 + i * sx, y0 + j * sy, x0 + (i + 1) * sx, y0 + (j + 1) * sy, c)
    r = math.radians(barrel_deg)
    bx, by = cx, y0 + sy
    for t in range(0, length * 4):
        x = bx + t / 4 * math.cos(r) * 64 / par
        y = by - t / 4 * math.sin(r)
        for k in range(thick):
            cv.set(int(round(x)), int(round(y)) + k, bcol)


def arc(x0, y0, x1, y1, h, n):
    """n points of a shell's flight from (x0, y0) to (x1, y1), h over"""
    return [(x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n - 4 * h * (k / n) * (1 - k / n)) for k in range(n + 1)]


def burst(cv, bx, by, size, par, colours, spikes=7):
    for y in range(by - 2 * size, by + 2 * size):
        for x in range(bx - 3 * size, bx + 3 * size):
            dx, dy = (x - bx) * par / 64, y - by
            r = math.hypot(dx, dy)
            ang = math.atan2(dy, dx)
            sp = size * (1 + 0.35 * math.sin(ang * spikes) + 0.2 * math.sin(ang * 13))
            if r < sp:
                f = r / sp
                for lim, c in colours:
                    if f < lim:
                        cv.set(x, y, c)
                        break


# ---- the CoCo 3's ------------------------------------------------------------------

PAL3 = [
    (0, 0, 0),          # 0 black
    (0, 0, 85),         # 1 night
    (85, 0, 85),        # 2 dusk purple
    (170, 0, 85),       # 3 magenta-red
    (255, 85, 0),       # 4 orange-red
    (255, 170, 0),      # 5 orange
    (255, 255, 85),     # 6 pale yellow (the sun, the logo's top)
    (255, 255, 255),    # 7 white
    (85, 0, 0),         # 8 far mountains
    (85, 85, 0),        # 9 olive (near hills' light)
    (170, 85, 0),       # 10 brown
    (0, 85, 0),         # 11 dark green
    (255, 85, 85),      # 12 the red tank
    (85, 170, 255),     # 13 the blue tank
    (170, 0, 0),        # 14 deep red (the logo's bottom)
    (255, 170, 85),     # 15 peach (the burst)
]


def title3():
    W, H, PAR = 320, 200, 55
    cv = Canvas(W, H, 1)
    f = font3()
    b3 = lambda ch: glyph_bits3(f, ch)
    # the sky: night to dusk to fire, dithered a pair of colours at a time
    bands = [(1, 2, 0, 50), (2, 3, 50, 95), (3, 4, 95, 128), (4, 5, 128, 150)]
    for a, b, y0, y1 in bands:
        for y in range(y0, y1):
            lv = (y - y0) * 17 // (y1 - y0)
            cv.dither(0, y, W, y + 1, a, b, min(16, lv))
    # stars up in the night
    import random
    rnd = random.Random(7)
    for _ in range(40):
        x, y = rnd.randrange(W), rnd.randrange(46)
        cv.set(x, y, 7 if rnd.random() < 0.3 else 6)
    # the sun on the horizon, banded
    sx, sy, sr = 214, 150, 34
    for y in range(sy - sr, sy):
        if (sy - y) % 7 in (1,) and y > sy - 22:
            continue
        for x in range(sx - 50, sx + 51):
            if ((x - sx) * PAR / 64) ** 2 + (y - sy) ** 2 <= sr * sr:
                cv.set(x, y, 6 if y < sy - 14 else 5)
    # far mountains
    far = hills(W, 140, 14, 0.031, 0.077, 1.3)
    for x in range(W):
        for y in range(far[x], H):
            cv.set(x, y, 8)
    # the near hills: dark green to brown, dithered strata
    near = hills(W, 160, 12, 0.02, 0.05, 0.4)
    for x in range(W):
        for y in range(near[x], H):
            d = y - near[x]
            if d < 2:
                c = 9
            else:
                lv = min(16, (y - 150) * 16 // 50)
                c = 10 if lv > BAYER[y & 3][x & 3] else 9
                if (y // 6) % 3 == 0 and d > 6:
                    c = 10 if (x + y) % 4 else 8
            cv.set(x, y, c)
    # the tanks: red on the left firing, blue on the right
    rows = ["....SSSS....", "...SSHHSS...", ".SSSSSSSSSS.", "SSSSSSSSSSSS", "KSKSKSKSKSKS", ".KKKKKKKKKK."]
    lx, rx = 64, 262
    tank(cv, lx, near[lx] + 1, rows, {"S": 12, "K": 0, "H": 7}, 2, 2, 52, PAR, 11, 2, 12)
    tank(cv, rx, near[rx] + 1, rows, {"S": 13, "K": 0, "H": 7}, 2, 2, 150, PAR, 11, 2, 13)
    # the shell's arc, dotted, bursting on the ground short of the blue tank
    pts = arc(lx + 12, near[lx] - 20, rx - 34, near[rx - 34] - 2, 96, 60)
    for k, (x, y) in enumerate(pts[:-3]):
        if k % 2 == 0:
            cv.set(int(x), int(y), 7)
            cv.set(int(x) + 1, int(y), 6)
    bx, by = int(pts[-1][0]), int(pts[-1][1])
    burst(cv, bx, by, 13, PAR, [(0.3, 7), (0.55, 6), (0.75, 15), (1.01, 4)])
    # FUJIBARRAGE: big, a gradient, outlined, a shadow
    s = "FUJIBARRAGE"
    sxl, syl = 3, 3
    wl = len(s) * 8 * sxl
    grad = [6, 6, 6, 5, 5, 5, 5, 4, 4, 4, 14, 14]
    logo(cv, b3, s, (W - wl) // 2 + 2, 12, sxl, syl, 8,
         lambda x, y: 7 if y == 3 else grad[min(len(grad) - 1, y * len(grad) // (8 * syl))], 0, 2)
    tag = "AIM, FIRE, TALK TRASH."
    tw = len(tag) * 8
    for dx, dy in ((1, 1),):
        text(cv, b3, tag, (W - tw) // 2 + dx, 46 + dy, 0)
    text(cv, b3, tag, (W - tw) // 2, 46, 7)
    pf = "PRESS FIRE"
    pw = len(pf) * 8
    cv.rect((W - pw) // 2 - 4, 182, (W + pw) // 2 + 4, 194, 0)
    text(cv, b3, pf, (W - pw) // 2, 184, 6)
    return cv


def pack3(cv):
    streams = []
    for blk in range(7):
        data = bytearray()
        for y in range(blk * 32, min(cv.h, blk * 32 + 32)):
            row = cv.px[y]
            data.extend(bytes(row[2 * i] << 4 | row[2 * i + 1] for i in range(cv.w // 2)))
            data.extend(bytes(96))
        if data:
            streams.append(lz_pack(bytes(data)))
    return streams


# ---- the CoCo 1/2's ------------------------------------------------------------------

def title2():
    W, H, PAR = 128, 192, 109
    BLK, BLUE, ORANGE, WHITE = 0, 1, 2, 3
    cv = Canvas(W, H, BLUE)
    f = font2()
    b2 = lambda ch: glyph_bits2(f, ch)
    # a few stars of white high up, a dither of the sun's light low down
    import random
    rnd = random.Random(3)
    for _ in range(14):
        cv.set(rnd.randrange(W), rnd.randrange(8, 60), WHITE)
    sx, sy, sr = 88, 128, 22
    for y in range(sy - sr, sy):
        if (sy - y) % 5 == 1 and y > sy - 14:
            continue
        for x in range(W):
            if ((x - sx) * PAR / 64) ** 2 + (y - sy) ** 2 <= sr * sr:
                cv.set(x, y, ORANGE)
    far = hills(W, 124, 8, 0.07, 0.16, 1.3)
    for x in range(W):
        for y in range(far[x], H):
            cv.set(x, y, BLK)
    near = hills(W, 140, 8, 0.05, 0.11, 0.4)
    for x in range(W):
        for y in range(near[x], H):
            d = y - near[x]
            c = ORANGE
            if d >= 3 and (y // 5) % 3 == 0 and (x + (y >> 1)) % 3 == 0:
                c = BLK
            cv.set(x, y, c)
        cv.set(x, near[x], WHITE if x % 2 else ORANGE)
    body = ["..S..", ".SSS.", "SSSSS", "SSSSS", "S.S.S"]
    lx, rx = 24, 104
    tank(cv, lx, near[lx] + 1, body, {"S": BLK}, 2, 2, 45, PAR, 5, 2, BLK, WHITE)
    tank(cv, rx, near[rx] + 1, body, {"S": BLK}, 2, 2, 135, PAR, 5, 2, BLK, WHITE)
    pts = arc(lx + 4, near[lx] - 14, rx - 16, near[rx - 16] - 1, 86, 40)
    for k, (x, y) in enumerate(pts[:-2]):
        if k % 2 == 0:
            cv.set(int(x), int(y), WHITE)
    bx, by = int(pts[-1][0]), int(pts[-1][1])
    burst(cv, bx, by, 10, PAR, [(0.45, WHITE), (0.8, ORANGE), (1.01, BLK)])
    # FUJI / BARRAGE: three times the text's font, white, a black outline
    for s, y in (("FUJI", 10), ("BARRAGE", 38)):
        w = len(s) * 4 * 3
        logo(cv, b2, s, (W - w) // 2 + 1, y, 3, 3, 4, lambda x, yy: WHITE, BLK)
    tag = "AIM, FIRE, TALK TRASH."
    text(cv, b2, tag, (W - len(tag) * 4) // 2, 70, WHITE, cw=4)
    pf = "PRESS FIRE"
    pw = len(pf) * 4
    cv.rect((W - pw) // 2 - 2, 178, (W + pw) // 2 + 1, 188, BLK)
    text(cv, b2, pf, (W - pw) // 2, 179, WHITE, cw=4)
    return cv


def pack2(cv):
    data = bytearray()
    for y in range(cv.h):
        row = cv.px[y]
        for i in range(0, cv.w, 4):
            data.append(row[i] << 6 | row[i + 1] << 4 | row[i + 2] << 2 | row[i + 3])
    return [lz_pack(bytes(data))]


# ---- out ----------------------------------------------------------------------

def emit(path, label, streams, extra=()):
    with open(path, "w") as o:
        o.write("* GENERATED by tools/mktitle_coco.py. Do not edit.\n        section art\n")
        for lab, data in extra:
            o.write("        export  %s\n%s\n" % (lab, lab))
            for i in range(0, len(data), 16):
                o.write("        fcb     %s\n" % ",".join("$%02X" % b for b in data[i:i + 16]))
        o.write("        export  %s\n%s\n" % (label, label))
        for k, s in enumerate(streams):
            o.write("* stream %d\n" % k)
            for i in range(0, len(s), 16):
                o.write("        fcb     %s\n" % ",".join("$%02X" % b for b in s[i:i + 16]))


def preview(path, cv, pal, sx, sy):
    from PIL import Image
    im = Image.new("RGB", (cv.w, cv.h))
    im.putdata([pal[c] for row in cv.px for c in row])
    im.resize((cv.w * sx, cv.h * sy), Image.NEAREST).save(path)


def main():
    what, out = sys.argv[1], sys.argv[2]
    prev = sys.argv[3] if len(sys.argv) > 3 else None
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    if what == "title3":
        import cocopal
        cv = title3()
        streams = pack3(cv)
        for k, s in enumerate(streams):
            want = bytearray()
            for y in range(k * 32, min(cv.h, k * 32 + 32)):
                want.extend(bytes(cv.px[y][2 * i] << 4 | cv.px[y][2 * i + 1] for i in range(160)) + bytes(96))
            assert lz_unpack(s) == bytes(want)
        rgb = [cocopal.rgb_code(c) for c in PAL3]
        cmp_ = [cocopal.cmp_code(c) for c in PAL3]
        emit(out, "_title3", streams, [("_title3_pal", rgb + cmp_)])
        print("title3: %d bytes packed (%d streams)" % (sum(len(s) for s in streams), len(streams)))
        if prev:
            preview(prev, cv, [cocopal.show(c) for c in rgb], 3, 3)
    elif what == "title2":
        cv = title2()
        streams = pack2(cv)
        emit(out, "_title2", streams)
        print("title2: %d bytes packed" % len(streams[0]))
        if prev:
            pal = [(0, 0, 0), (40, 110, 255), (230, 120, 20), (255, 255, 255)]
            preview(prev, cv, pal, 6, 3)
    else:
        sys.exit("what? " + what)


if __name__ == "__main__":
    main()
