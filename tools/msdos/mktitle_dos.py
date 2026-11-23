#!/usr/bin/env python3
"""mktitle_dos.py OUT.c [PREVIEW_DIR] -- the PC's title pictures, drawn here
(tools/mktitle_coco.py's sunset: two tanks, one's shell arcing over to burst
by the other, FUJIBARRAGE over it all) and packed with its lz_pack
(src/msdos/lzd.asm unpacks them onto the screen) for the menu programs:

    title_cga[]       CGA's 320x200 in 4 colours, the screen's own layout
                      (B800h: even lines from 0, odd from 2000h): palette 0
                      bright (light green, light red, yellow) on blue
    title_vga[]       VGA's 320x200 a byte a pixel (A000h)
    title_vga_dac[]   its colours 0-191 (6-bit RGB; 192-255 stay the
                      text's: src/msdos/vga/vga.h)

PREVIEW_DIR: title_cga.png and title_vga.png at 4:3 (Pillow; the build
needs none).
"""
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import mktitle_coco as mt     # noqa: E402

W, H, PAR = 320, 200, 55
ROWS = ["....SSSS....", "...SSHHSS...", ".SSSSSSSSSS.", "SSSSSSSSSSSS", "KSKSKSKSKSKS", ".KKKKKKKKKK."]
HINT = "ESC: LOBBY"


def lerp(a, b, t):
    return tuple(int(round(a[k] + (b[k] - a[k]) * t)) for k in range(3))


def ramp(stops, n):
    """n colours along the stops (t, colour)"""
    out = []
    for i in range(n):
        t = i / (n - 1) if n > 1 else 0
        for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
            if t0 <= t <= t1:
                out.append(lerp(c0, c1, (t - t0) / (t1 - t0) if t1 > t0 else 0))
                break
    return out


# ---- VGA: a palette of 192, laid out by what uses it ------------------------------

class Pal:
    def __init__(self):
        self.c = [(0, 0, 0)]

    def add(self, cols):
        at = len(self.c)
        self.c.extend(cols)
        assert len(self.c) <= 192, len(self.c)
        return at


def title_vga():
    p = Pal()
    horizon = 150
    sky = p.add(ramp([(0, (6, 8, 36)), (0.45, (60, 24, 96)), (0.75, (190, 60, 90)), (1, (255, 140, 50))], 80))
    stars = p.add([(255, 255, 255), (200, 210, 255), (150, 150, 190)])
    sun = p.add(ramp([(0, (255, 252, 190)), (1, (255, 150, 40))], 12))
    far = p.add(ramp([(0, (60, 26, 70)), (1, (30, 12, 40))], 12))
    near = p.add(ramp([(0, (120, 112, 34)), (0.3, (92, 70, 24)), (1, (34, 24, 10))], 28))
    rim = p.add([(170, 160, 60), (140, 130, 50)])
    red = p.add([(230, 40, 30), (150, 20, 16), (255, 200, 190)])
    blue = p.add([(60, 120, 255), (30, 60, 160), (210, 230, 255)])
    logo = p.add(ramp([(0, (255, 255, 200)), (0.35, (255, 220, 60)), (0.7, (255, 110, 20)), (1, (170, 20, 10))], 16))
    burst = p.add([(255, 255, 255), (255, 250, 170), (255, 200, 60), (255, 120, 20), (200, 40, 10)])
    white, yellow, grey, shadow = p.add([(255, 255, 255), (255, 230, 80), (170, 170, 190), (40, 10, 30)]), 0, 0, 0
    yellow, grey, shadow = white + 1, white + 2, white + 3

    cv = mt.Canvas(W, H, 0)
    f = mt.font3()
    b3 = lambda ch: mt.glyph_bits3(f, ch)
    for y in range(H):                      # the sky: a colour a line
        c = sky + min(79, y * 80 // horizon)
        for x in range(W):
            cv.set(x, y, c)
    rnd = random.Random(7)
    for _ in range(70):                     # stars up in the night
        x, y = rnd.randrange(W), rnd.randrange(60)
        cv.set(x, y, stars + rnd.randrange(3))
    sx, sy, sr = 214, 150, 34               # the sun on the horizon, banded
    for y in range(sy - sr, sy):
        if (sy - y) % 7 == 1 and y > sy - 22:
            continue
        for x in range(sx - 50, sx + 51):
            if ((x - sx) * PAR / 64) ** 2 + (y - sy) ** 2 <= sr * sr:
                cv.set(x, y, sun + min(11, (y - (sy - sr)) * 12 // sr))
    farh = mt.hills(W, 140, 14, 0.031, 0.077, 1.3)
    for x in range(W):
        for y in range(farh[x], H):
            cv.set(x, y, far + min(11, (y - farh[x]) * 12 // 40))
    nearh = mt.hills(W, 160, 12, 0.02, 0.05, 0.4)
    for x in range(W):
        for y in range(nearh[x], H):
            d = y - nearh[x]
            if d < 2:
                c = rim + d
            else:
                lv = min(27, (y - 150) * 28 // 50)
                if (y // 6) % 3 == 0 and d > 6:
                    lv = min(27, lv + 3)
                c = near + max(0, lv)
            cv.set(x, y, c)
    lx, rx = 64, 262
    mt.tank(cv, lx, nearh[lx] + 1, ROWS, {"S": red, "K": 0, "H": red + 2}, 2, 2, 52, PAR, 11, 2, red)
    mt.tank(cv, rx, nearh[rx] + 1, ROWS, {"S": blue, "K": 0, "H": blue + 2}, 2, 2, 150, PAR, 11, 2, blue)
    pts = mt.arc(lx + 12, nearh[lx] - 20, rx - 34, nearh[rx - 34] - 2, 96, 60)
    for k, (x, y) in enumerate(pts[:-3]):
        if k % 2 == 0:
            cv.set(int(x), int(y), white)
            cv.set(int(x) + 1, int(y), burst + 1)
    bx, by = int(pts[-1][0]), int(pts[-1][1])
    mt.burst(cv, bx, by, 13, PAR, [(0.3, burst), (0.5, burst + 1), (0.7, burst + 2), (0.85, burst + 3), (1.01, burst + 4)])
    s = "FUJIBARRAGE"
    sxl, syl = 3, 3
    wl = len(s) * 8 * sxl
    mt.logo(cv, b3, s, (W - wl) // 2 + 2, 12, sxl, syl, 8,
            lambda x, y: logo + min(15, y * 16 // (8 * syl)), 0, shadow)
    tag = "AIM, FIRE, TALK TRASH."
    tw = len(tag) * 8
    mt.text(cv, b3, tag, (W - tw) // 2 + 1, 47, 0)
    mt.text(cv, b3, tag, (W - tw) // 2, 46, white)
    pf = "PRESS FIRE"
    pw = len(pf) * 8
    cv.rect((W - pw) // 2 - 4, 176, (W + pw) // 2 + 4, 188, 0)
    mt.text(cv, b3, pf, (W - pw) // 2, 178, yellow)
    cv.rect(0, 190, W, 200, 0)
    mt.text(cv, b3, HINT, (W - len(HINT) * 8) // 2, 191, grey)
    return cv, p.c


# ---- CGA: palette 0 bright on blue ------------------------------------------------

def title_cga():
    BG, GRN, RED, YEL = 0, 1, 2, 3
    cv = mt.Canvas(W, H, BG)
    f = mt.font3()
    b3 = lambda ch: mt.glyph_bits3(f, ch)
    rnd = random.Random(7)
    for _ in range(50):
        cv.set(rnd.randrange(W), rnd.randrange(60), YEL)
    sx, sy, sr = 214, 150, 34
    for y in range(sy - sr, sy):
        if (sy - y) % 7 == 1 and y > sy - 22:
            continue
        for x in range(sx - 50, sx + 51):
            if ((x - sx) * PAR / 64) ** 2 + (y - sy) ** 2 <= sr * sr:
                lv = (y - (sy - sr)) * 17 // sr
                cv.set(x, y, RED if lv > mt.BAYER[y & 3][x & 3] else YEL)
    farh = mt.hills(W, 140, 14, 0.031, 0.077, 1.3)
    for x in range(W):
        for y in range(farh[x], H):
            cv.set(x, y, GRN if (x + y) % 2 else BG)       # a haze of green
    nearh = mt.hills(W, 160, 12, 0.02, 0.05, 0.4)
    for x in range(W):
        for y in range(nearh[x], H):
            d = y - nearh[x]
            c = YEL if d < 1 else GRN
            if d > 6 and (y // 6) % 3 == 0 and (x + y) % 3 == 0:
                c = BG
            cv.set(x, y, c)
    lx, rx = 64, 262
    mt.tank(cv, lx, nearh[lx] + 1, ROWS, {"S": RED, "K": BG, "H": YEL}, 2, 2, 52, PAR, 11, 2, RED)
    mt.tank(cv, rx, nearh[rx] + 1, ROWS, {"S": YEL, "K": BG, "H": RED}, 2, 2, 150, PAR, 11, 2, YEL)
    pts = mt.arc(lx + 12, nearh[lx] - 20, rx - 34, nearh[rx - 34] - 2, 96, 60)
    for k, (x, y) in enumerate(pts[:-3]):
        if k % 2 == 0:
            cv.set(int(x), int(y), YEL)
    bx, by = int(pts[-1][0]), int(pts[-1][1])
    mt.burst(cv, bx, by, 13, PAR, [(0.45, YEL), (0.8, RED), (1.01, YEL)])
    s = "FUJIBARRAGE"
    sxl, syl = 3, 3
    wl = len(s) * 8 * sxl
    mt.logo(cv, b3, s, (W - wl) // 2 + 2, 12, sxl, syl, 8,
            lambda x, y: YEL if y < 4 * syl or (y < 6 * syl and (x + y) % 2) else RED, BG, RED)
    tag = "AIM, FIRE, TALK TRASH."
    mt.text(cv, b3, tag, (W - len(tag) * 8) // 2, 46, YEL)
    pf = "PRESS FIRE"
    pw = len(pf) * 8
    cv.rect((W - pw) // 2 - 4, 176, (W + pw) // 2 + 4, 188, RED)
    mt.text(cv, b3, pf, (W - pw) // 2, 178, YEL)
    cv.rect(0, 190, W, 200, BG)
    mt.text(cv, b3, HINT, (W - len(HINT) * 8) // 2, 191, GRN)
    return cv


CGA_RGB = [(85, 85, 255), (85, 255, 85), (255, 85, 85), (255, 255, 85)]    # blue; palette 0 bright


def cga_bytes(cv):
    data = bytearray(16384)
    for y in range(H):
        off = (y & 1) * 0x2000 + (y >> 1) * 80
        row = cv.px[y]
        for i in range(80):
            data[off + i] = row[4 * i] << 6 | row[4 * i + 1] << 4 | row[4 * i + 2] << 2 | row[4 * i + 3]
    return bytes(data)


def carray(name, data):
    out = ["const uint8_t %s[%d] = {" % (name, len(data))]
    for i in range(0, len(data), 20):
        out.append("    " + ",".join("%d" % b for b in data[i:i + 20]) + ",")
    out.append("};")
    return "\n".join(out)


def preview(path, cv, pal):
    from PIL import Image
    im = Image.new("RGB", (W, H))
    im.putdata([pal[c] for row in cv.px for c in row])
    im.resize((W * 2, int(H * 2 * 1.2)), Image.NEAREST).save(path)


def main():
    out = sys.argv[1]
    vcv, vpal = title_vga()
    ccv = title_cga()
    vdata = bytes(c for row in vcv.px for c in row)
    cdata = cga_bytes(ccv)
    vpk = mt.lz_pack(vdata)
    cpk = mt.lz_pack(cdata)
    assert mt.lz_unpack(vpk) == vdata and mt.lz_unpack(cpk) == cdata
    dac = bytearray()
    for r, g, b in vpal + [(0, 0, 0)] * (192 - len(vpal)):
        dac += bytes((r >> 2, g >> 2, b >> 2))
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "w") as o:
        o.write("/* GENERATED by tools/msdos/mktitle_dos.py */\n#include <stdint.h>\n\n")
        o.write("#ifdef DOS_VGA\n" + carray("title_vga", vpk) + "\n" + carray("title_vga_dac", dac) + "\n")
        o.write("#else\n" + carray("title_cga", cpk) + "\n#endif\n")
    print("title: VGA %d bytes packed (%d colours), CGA %d" % (len(vpk), len(vpal), len(cpk)))
    if len(sys.argv) > 2:
        d = sys.argv[2]
        os.makedirs(d, exist_ok=True)
        preview(os.path.join(d, "title_vga.png"), vcv, vpal)
        preview(os.path.join(d, "title_cga.png"), ccv, CGA_RGB)


if __name__ == "__main__":
    main()
