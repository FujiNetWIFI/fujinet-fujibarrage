#!/usr/bin/env python3
"""preview_intv.py -- mock Intellivision screens of FujiBarrage, for the art
and layout's approval: the field (a real one from refview, through the cell
model the ROM uses: sky, solid, or a pattern from a GRAM pool of N_POOL
cards), the tanks as MOBs, a fireball, the HUD; the title; the waiting
room, the settings, the shop, a round's end, the game's end; the sprite
sheet; every sky with the dirt colours. A screen is 20x12 cards in
foreground/background mode with the border round it, each pixel two
scanlines tall, as jzIntv shows it.

usage: preview_intv.py OUTDIR FIELD.px|FIXTURE.bas [...]   (refview -w 160 -h 80 output)"""
import os
import sys

from PIL import Image

import intvart as A

JZ = os.environ.get("JZINTV_DIR", os.path.expanduser("~/Workspace/jzintv-20200712-src"))
FW, FH = 160, 80
TOP = 2         # the field's first card row


class Screen:
    def __init__(self, border=A.BLACK, bg=A.BLUE):
        self.grom = open(os.path.join(JZ, "rom", "grom.bin"), "rb").read()
        self.cards = [[(["........"] * 8, A.WHITE, bg) for _ in range(20)] for _ in range(12)]
        self.mobs = []
        self.border = border

    def card(self, col, row, rows, fg, bg):
        self.cards[row][col] = (rows, fg, bg)

    def glyph(self, ch):
        n = ord(ch) - 32
        return ["".join("#" if b >> (7 - x) & 1 else "." for x in range(8)) for b in self.grom[n * 8:n * 8 + 8]]

    def text(self, col, row, s, fg=A.WHITE, bg=None):
        for i, ch in enumerate(s):
            if col + i >= 20:
                break
            b = self.cards[row][col + i][2] if bg is None else bg
            self.card(col + i, row, self.glyph(ch), fg, b)

    def bar(self, row, bg):
        for c in range(20):
            rows, fg, _ = self.cards[row][c]
            self.cards[row][c] = (rows, fg, bg)

    def mob(self, x, y, rows, colour, flip=False, ysz=2, behind=True):
        """x, y in field-screen pixels (0-159, 0-95); a row ysz scanlines"""
        self.mobs.append((x, y, rows, colour, flip, ysz, behind))

    def render(self, path, scale=3):
        img = Image.new("RGB", (176, 208), A.PAL[self.border])
        px = img.load()
        fgmask = set()
        for r in range(12):
            for c in range(20):
                rows, fg, bg = self.cards[r][c]
                for y in range(8):
                    for x in range(8):
                        on = rows[y][x] == "#"
                        for s in (0, 1):
                            px[8 + c * 8 + x, 8 + (r * 8 + y) * 2 + s] = A.PAL[fg if on else bg]
                        if on:
                            fgmask.add((c * 8 + x, r * 8 + y))
        for x0, y0, rows, colour, flip, ysz, behind in reversed(self.mobs):
            for ry, row in enumerate(rows):
                for rx in range(8):
                    if row[rx] != "#":
                        continue
                    xx = x0 + (7 - rx if flip else rx)
                    for dy in range(ysz):
                        sy = y0 * 2 + ry * ysz + dy
                        if 0 <= xx < 160 and 0 <= sy < 192:
                            if behind and (xx, sy // 2) in fgmask:
                                continue
                            px[8 + xx, 8 + sy] = A.PAL[colour]
        img.resize((176 * scale, 208 * scale // 2), Image.NEAREST).save(path)
        return img


# ---- the field ----------------------------------------------------------------------
def field_from(path):
    """a 160x80 field of 0 sky / 1 dirt from refview's shot px or a fixture's terrain"""
    if path.endswith(".px"):
        b = open(path, "rb").read()
        return [[1 if b[y * FW + x] == 1 else 0 for x in range(FW)] for y in range(FH)], None
    text = open(path).read()
    words, tanks, cur = {}, {}, None
    for ln in text.splitlines():
        ln = ln.strip()
        if ln.endswith(":"):
            cur = ln[:-1]
            words[cur] = []
        elif ln.startswith("DATA") and cur:
            words[cur] += [int(v.replace("$", "0x"), 0) for v in ln[4:].split(",")]
    n = int(text.split("FIX_NTERR = ")[1].split()[0])
    terr = []
    for w in words["fix_terrain"]:
        terr += [w & 255, w >> 8]
    terr = terr[:n]
    f = [[0] * FW for _ in range(FH)]
    i = 0
    for x in range(FW):
        v = terr[i]
        if v < 0xF0:
            for y in range(v, FH):
                f[y][x] = 1
            i += 1
        else:
            k = v - 0xF0
            for s in range(k):
                for y in range(terr[i + 1 + 2 * s], min(terr[i + 2 + 2 * s], FH)):
                    f[y][x] = 1
            i += 1 + 2 * k
    tanks = list(zip(words["fix_tx"], words["fix_ty"], words["fix_ta"]))
    return f, tanks


def cells_of(f):
    """each cell's 8 rows as '#'/'.' strings"""
    out = {}
    for cy in range(FH // 8):
        for cx in range(FW // 8):
            out[(cx, cy)] = ["".join("#" if f[cy * 8 + y][cx * 8 + x] else "." for x in range(8)) for y in range(8)]
    return out


def pool_stats(f):
    cl = cells_of(f)
    mixed = [r for r in cl.values() if any("#" in x for x in r) and not all(x == "#" * 8 for x in r)]
    uniq = []
    for r in mixed:
        if r not in uniq:
            uniq.append(r)
    return len(mixed), len(uniq)


def draw_field(scr, f, sky, pal, tanks=None, stars=False, fx=None, fire=A.YELLOW):
    """the cells as the ROM draws them. No fire in a cell: the dirt's pixels
    in the dirt colour on the row's sky. Fire in it: the fire's pixels in the
    fire colour, on whichever of sky or dirt covers more of the rest. A
    pattern neither empty nor full is a card from the pool (the same
    pattern, the same card); with the pool full, the nearest level card."""
    uniq = []
    art = A.read_art()
    for cy in range(FH // 8):
        for cx in range(FW // 8):
            bg = A.SKIES[sky][cy]
            fg = A.DIRT[pal]
            dirt = [[f[cy * 8 + y][cx * 8 + x] for x in range(8)] for y in range(8)]
            fire = [[fx[cy * 8 + y][cx * 8 + x] if fx else 0 for x in range(8)] for y in range(8)]
            nf = sum(map(sum, fire))
            if nf:
                nd = sum(dirt[y][x] and not fire[y][x] for y in range(8) for x in range(8))
                ns = 64 - nf - nd
                pat = fire
                bg = A.DIRT[pal] if nd > ns else bg
                fg = fire_fg = fire
                fg = A.YELLOW
                if A.DIRT[pal] == A.YELLOW and bg == A.DIRT[pal]:
                    fg = A.WHITE
            else:
                pat = dirt
            rows = ["".join("#" if pat[y][x] else "." for x in range(8)) for y in range(8)]
            if not any("#" in r for r in rows):
                if stars and cy < A.STAR_ROWS and (cx * 37 + cy * 101 + cx * cy * 13) % 7 < 2:
                    scr.card(cx, TOP + cy, art["star1" if (cx + cy) % 2 else "star2"], A.WHITE, bg)
                else:
                    scr.card(cx, TOP + cy, ["." * 8] * 8, fg, bg)
                continue
            if all(r == "#" * 8 for r in rows):
                scr.card(cx, TOP + cy, rows, fg, bg)
                continue
            if rows not in uniq:
                uniq.append(rows)
            if uniq.index(rows) >= A.N_POOL:      # the pool's full: the nearest level
                n = sum(r.count("#") for r in rows)
                lv = min(range(3), key=lambda k: abs((k + 1) * 16 - n))
                rows = A.levels()[lv]
            scr.card(cx, TOP + cy, rows, fg, bg)
    if tanks:
        for s, (x, y, ang) in enumerate(tanks):
            draw_tank(scr, art, s, x, y, ang)
    return len(uniq)


def draw_tank(scr, art, seat, x, y, ang, chute=False):
    a = ang if ang <= 90 else 180 - ang
    k = min(range(5), key=lambda i: abs(A.TANK_ANGLES[i] - a))
    rows = art["tank%d" % A.TANK_ANGLES[k]]
    flip = ang > 90
    mx = x - 3 - (1 if flip else 0)
    if chute:
        scr.mob(mx, TOP * 8 + y - 16, art["chute"] + art["chutetank"], A.SEATS[seat], flip)
    else:
        scr.mob(mx, TOP * 8 + y - 8, rows, A.SEATS[seat], flip)


def fireball(scr, cx, cy, r, frame=0):
    """a fireball over the field: cells inside filled (background: fire),
    the ones on its edge one of the 8 edge cards; cx, cy, r in pixels"""
    edges = A.fire_edges()
    for row in range(10):
        for col in range(20):
            mx, my = col * 8 + 4, row * 8 + 4
            d = ((mx - cx) ** 2 + (my - cy) ** 2) ** 0.5
            if d <= r - 4:
                scr.card(col, TOP + row, ["." * 8] * 8, A.WHITE, A.FIRE_BG[frame % 6])
            elif d <= r + 4:
                dx, dy = mx - cx, my - cy
                import math
                ang = math.atan2(dx, -dy)
                k = int(((ang + math.pi * 2) % (math.pi * 2) + math.pi / 8) / (math.pi / 4)) % 8
                _, _, bg = scr.cards[TOP + row][col]
                scr.card(col, TOP + row, edges[k], A.FIRE_FG[frame % 6], bg)


def hud(scr, name, seat, ang, pw, en, weapon, n, wind, timer, msg=None):
    scr.bar(0, A.SEATS[seat])
    scr.bar(1, A.BLACK)
    scr.text(0, 0, "%-8s%3d %4d %3d" % (name, ang, pw, en), A.BLACK if A.SEATS[seat] in A.LIGHT else A.WHITE)
    if msg:
        scr.text(0, 1, msg[:20].ljust(20), A.YELLOW)
    else:
        wa = "<" if wind < 0 else ">"
        scr.text(0, 1, "%-10s%2d %s%2d %2d" % (weapon[:10], n, wa, abs(wind), timer), A.WHITE)


# ---- the screens ----------------------------------------------------------------------
NAMES = ["ANN", "BOLT", "COBALT", "GASKET", "SPARKY", "WIDGET", "PISTON", "KABOOM", "ROVER", "ZAPPER"]
KINDS = ["HOST", "ROOKIE", "GUNNER", "SNIPER", "DEADEYE", "LOBBER", "TACTICIAN", "RICOCHET", "WILDCARD", "ROOKIE"]


def title(d, art):
    scr = Screen(A.BLACK, A.BLACK)
    cells, uniq = A.logo()
    for col, row, rows, line in cells:
        fg = A.WHITE if line == 0 else [A.YELLOW, A.YELLOW, A.RED][row - 4] if 4 <= row <= 6 else A.RED
        scr.card(col, row, rows, fg, A.BLACK)
    scr.text(2, 8, "ARTILLERY FOR TEN", A.TAN)
    scr.text(5, 10, "PRESS FIRE", A.WHITE)
    for c in range(20):
        scr.card(c, 11, ["." * 8] * 6 + ["#" * 8] * 2, A.DKGREEN, A.BLACK)
    draw_tank(scr, art, 0, 30, 86 - 16, 45)
    draw_tank(scr, art, 3, 130, 86 - 16, 135)
    scr.render(os.path.join(d, "title.png"))
    return len(uniq)


def waiting(d):
    scr = Screen(A.BLACK, A.BLACK)
    scr.bar(0, A.BLUE)
    scr.text(0, 0, "ROOM1 OPEN RANGE", A.YELLOW)
    for s in range(10):
        scr.card(0, 1 + s, ["#" * 8] * 7 + ["." * 8], A.SEATS[s] if A.SEATS[s] < 8 else A.WHITE, A.BLUE)
        scr.cards[1 + s][0] = (["." * 8] * 8, A.WHITE, A.SEATS[s])
        scr.text(2, 1 + s, NAMES[s], A.WHITE)
        scr.text(11, 1 + s, ("HOST" if s == 0 else KINDS[s])[:9], A.TAN if s else A.YELLOW)
    scr.bar(11, A.BLUE)
    scr.text(0, 11, "FIRE:START R:SETUP", A.WHITE)
    scr.render(os.path.join(d, "waiting.png"))


def settings(d):
    scr = Screen(A.BLACK, A.BLACK)
    scr.bar(0, A.BLUE)
    scr.text(0, 0, "SETUP 1/3 FIELD", A.YELLOW)
    rows = [("SKY", "RANDOM"), ("BUMPINESS", "20"), ("SLOPES", "20"), ("FLATTEN PEA", "OFF"),
            ("RANDOM LAND", "OFF"), ("MAX WIND", "200"), ("CHANGING WI", "OFF"), ("GRAVITY", "0.20"),
            ("AIR THICKNE", "0"), ("DIRT STAYS ", "0")]
    for i, (l, v) in enumerate(rows):
        sel = i == 5
        bg = A.BLUE if sel else A.BLACK
        scr.bar(1 + i, bg)
        scr.text(0, 1 + i, ">" if sel else " ", A.YELLOW)
        scr.text(1, 1 + i, l, A.WHITE)
        scr.text(19 - len(v) + 1, 1 + i, v, A.YELLOW if sel else A.TAN)
    scr.bar(11, A.BLUE)
    scr.text(0, 11, "MAX WIND  <> CHANGE", A.WHITE)
    scr.render(os.path.join(d, "settings.png"))


def shop(d):
    scr = Screen(A.BLACK, A.BLACK)
    scr.bar(0, A.BLUE)
    scr.text(0, 0, "SHOP 2   $187500", A.YELLOW)
    items = [("MINI MISSILE", "99"), ("MISSILE", "5"), ("MINI NUKE", ""), ("NUKE", "1"), ("HOPPER", ""),
             ("CLUSTER BOMB", ""), ("MIRV", "2"), ("MEGA MIRV", ""), ("NAPALM", "")]
    for i, (n, own) in enumerate(items):
        sel = i == 3
        bg = A.BLUE if sel else A.BLACK
        scr.bar(1 + i, bg)
        scr.text(0, 1 + i, ">" if sel else " ", A.YELLOW)
        scr.text(1, 1 + i, n, A.WHITE)
        scr.text(18, 1 + i, own.rjust(2), A.YELLOW)
    scr.bar(10, A.BLACK)
    scr.text(0, 10, "$12000 X1  OWN 1", A.TAN)
    scr.bar(11, A.BLUE)
    scr.text(0, 11, "FIRE BUY  L SELL 30", A.WHITE)
    scr.render(os.path.join(d, "shop.png"))


def results(d, final):
    scr = Screen(A.BLACK, A.BLACK)
    scr.bar(0, A.BLUE)
    scr.text(0, 0, "GAME OVER" if final else "ROUND 3 OF 10", A.YELLOW)
    scr.bar(1, A.SEATS[4])
    scr.text(0, 1, "SPARKY " + ("WINS THE GAME!" if final else "WINS"), A.BLACK if A.SEATS[4] in A.LIGHT else A.WHITE)
    wins = [1, 0, 2, 0, 3, 1, 0, 0, 2, 0]
    for s in range(10):
        scr.cards[2 + s][0] = (["." * 8] * 8, A.WHITE, A.SEATS[s])
        scr.text(2, 2 + s, NAMES[s], A.WHITE)
        scr.text(11, 2 + s, "%d" % (12500 * (s + 1) % 97000), A.TAN)
        scr.text(18, 2 + s, "%d" % wins[s], A.YELLOW)
    scr.render(os.path.join(d, "gameover.png" if final else "roundend.png"))


def sprites(d, art):
    scr = Screen(A.BLACK, A.BLACK)
    scr.text(0, 0, "TANKS 0-180", A.WHITE)
    for k, ang in enumerate([0, 22, 45, 68, 90, 112, 135, 158, 180]):
        draw_tank(scr, art, k, 8 + k * 16, 24 - 16, ang)
    draw_tank(scr, art, 9, 8 + 9 * 16 + 4, 24 - 16, 90, chute=True)
    scr.text(0, 3, "LEVELS", A.WHITE)
    for k, lv in enumerate(A.levels()):
        scr.card(8 + k, 3, lv, A.TAN, A.CYAN)
    scr.text(0, 6, "ARROW STARS SOLID", A.WHITE)
    scr.card(1, 7, art["arrow"], A.YELLOW, A.BLUE)
    scr.card(7, 7, art["star1"], A.WHITE, A.BLACK)
    scr.card(8, 7, art["star2"], A.WHITE, A.BLACK)
    scr.card(13, 7, ["#" * 8] * 8, A.TAN, A.BLACK)
    scr.text(0, 9, "SEATS", A.WHITE)
    for s in range(10):
        scr.cards[10][s * 2] = (["." * 8] * 8, A.WHITE, A.SEATS[s])
    scr.render(os.path.join(d, "sprites.png"))


def skies(d, f, tanks):
    """every sky (rows) with four dirt colours (columns), small"""
    tiles = []
    for sky in range(7):
        for pal in range(4):
            scr = Screen(A.BLACK)
            draw_field(scr, f, sky, pal * 2 if pal < 3 else 3, tanks, stars=(sky == 2))
            hud(scr, "ANN", 0, 45, 400, 100, "MISSILE", 5, 12, 30)
            tiles.append(scr.render(os.path.join(d, "_t.png"), scale=1))
    sheet = Image.new("RGB", (176 * 4, 208 * 7))
    for i, im in enumerate(tiles):
        sheet.paste(im, ((i % 4) * 176, (i // 4) * 208))
    sheet = sheet.resize((176 * 4 * 2, 208 * 7), Image.NEAREST)
    sheet.save(os.path.join(d, "skies.png"))
    os.remove(os.path.join(d, "_t.png"))


def dirts(d, f, tanks):
    tiles = []
    for pal in range(8):
        scr = Screen(A.BLACK)
        draw_field(scr, f, 1, pal, tanks)
        hud(scr, "ANN", 0, 45, 400, 100, "MISSILE", 5, 12, 30)
        tiles.append(scr.render(os.path.join(d, "_t.png"), scale=1))
    sheet = Image.new("RGB", (176 * 4, 208 * 2))
    for i, im in enumerate(tiles):
        sheet.paste(im, ((i % 4) * 176, (i // 4) * 208))
    sheet.resize((176 * 4 * 2, 208 * 2), Image.NEAREST).save(os.path.join(d, "dirts.png"))
    os.remove(os.path.join(d, "_t.png"))


def main():
    d = sys.argv[1]
    os.makedirs(d, exist_ok=True)
    art = A.read_art()
    fields = [field_from(p) for p in sys.argv[2:]]
    f, tanks = fields[0]
    for i, (fi, _) in enumerate(fields):
        m, u = pool_stats(fi)
        print("field %s: %d mixed cells, %d unique patterns (pool %d)" % (sys.argv[2 + i], m, u, A.N_POOL))
    # the play screen: a field, ten tanks, a fireball, the HUD
    scr = Screen(A.BLACK)
    used = draw_field(scr, f, 1, 0, tanks)
    hud(scr, "ANN", 0, 45, 400, 100, "MISSILE", 5, -12, 30)
    scr.render(os.path.join(d, "play.png"))
    for name, sky, pal, r in (("boom", 1, 0, 19), ("boom2", 4, 1, 19), ("boom3", 1, 0, 5), ("boom4", 0, 0, 12)):
        scr = Screen(A.BLACK)
        cx0, cy0 = tanks[3][0], tanks[3][1] - 2
        fx = [[1 if (x - cx0) ** 2 + (y - cy0) ** 2 <= r * r else 0 for x in range(FW)] for y in range(FH)]
        n = draw_field(scr, f, sky, pal, tanks, fx=fx)
        hud(scr, "BOLT", 1, 68, 812, 74, "NUKE" if r > 8 else "MISSILE", 1, 7, 12, msg="BOLT: TAKE THAT!")
        scr.render(os.path.join(d, name + ".png"))
        print("%s: blast r %d, %d pool cards" % (name, r, n))
    print("title: %d logo cards" % title(d, art))
    waiting(d)
    settings(d)
    shop(d)
    results(d, False)
    results(d, True)
    sprites(d, art)
    skies(d, f, tanks)
    dirts(d, f, tanks)


if __name__ == "__main__":
    main()
