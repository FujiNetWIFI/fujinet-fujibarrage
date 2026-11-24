#!/usr/bin/env python3
"""mock_arc.py -- the Arcadia graphics study: real FujiBarrage shots (every
frame, from build/arcdump at 48x48) drawn through arcmodel.py, the 2637 as
MAME draws it, in each candidate look; with the numbers (cells needing three
colours, cells changed a frame) and a shop screen at 16 columns.

  mock_arc.py OUTDIR [SEEDS]     (runs build/arcdump for every weapon)

The screen (26 rows of 8x8 cells, block graphics, multicolour):
  rows 0-23   the field, 48x48 blocks (a cell 3x2 blocks: 3,3,2 px x 4 lines)
  rows 24-25  the HUD: character rows (each starts with $40, a blank cell)
Colours: $19F9 ink HOT, ground SKY (also the border); $19F8 ink DIRT, ground
DIRT. A cell's bit 6 picks the ink (1 HOT, 0 DIRT), bit 7 the ground (1 SKY,
0 DIRT): any two of sky, dirt and hot (fire, shells, block tanks) share a
cell; all three can't (the study counts them).

Looks (tanks are the question):
  A  tanks as HOT blocks; sprites: the shooter and our own tank (seat
     colours, barrel at the angle); 2 sprites spare (shell heads, markers)
  B  sprites for 4 tanks (ours, the shooter, the next two seats); the rest
     HOT blocks
  C  every live tank a sprite: ours and the shooter steady, the others share
     2 sprites in turn (flicker; animations only)
"""
import json
import math
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import arcmodel as A  # noqa: E402

from PIL import Image  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FW, FH = 48, 48
SEATS = 10
REC = 2 + 2 + 1 + 1 + SEATS * 4 + FW * FH
VPOS = 228                              # rows from line 27: the picture centred
YPOS = 255 - VPOS
FROWS, HUD = 24, 24

WHITE, YELLOW, CYAN, GREEN, MAGENTA, RED, BLUE, BLACK = range(8)
PALETTES = {                            # sky, dirt, hot, seat colours
    "day": (BLUE, GREEN, YELLOW, [WHITE, RED, CYAN, MAGENTA, BLACK]),
    "night": (BLACK, RED, YELLOW, [WHITE, CYAN, GREEN, MAGENTA, BLUE]),
}
NAMES = ["MINI MISSILE", "MISSILE", "MINI NUKE", "NUKE", "HOPPER", "CLUSTER BOMB", "MIRV",
         "MEGA MIRV", "NAPALM", "INFERNO", "TRACER", "SMOKE TRACER", "LIGHT ROLLER", "ROLLER",
         "HEAVY ROLLER", "SHOVEL CHARGE", "SHOVEL BLAST", "DIRT BUSTER", "HEAVY BUSTER",
         "LIGHT DRILL", "DRILL", "HEAVY DRILL", "LIGHT MOLE", "MOLE", "HEAVY MOLE", "DIRT CLUMP",
         "DIRT HEAP", "LANDSLIDE", "MUDSLIDE", "DIRT GEYSER", "QUAKE", "PLASMA BURST", "LASER"]
SEATNAMES = ["AST", "COBALT", "SPARKY", "RUST", "VOLT", "EMBER", "BOLT", "NOVA", "FLINT", "ONYX"]
OWN = 0                                 # our seat in the mock-ups
SKY, DIRT, HOT = 0, 1, 2


def frames(path):
    d = open(path, "rb").read()
    out = []
    for o in range(0, len(d), REC):
        r = d[o:o + REC]
        assert r[:2] == b"FR"
        fr, flash, shooter = struct.unpack_from("<HBB", r, 2)
        tanks = [tuple(r[6 + 4 * i:10 + 4 * i]) for i in range(SEATS)]
        px = r[6 + 4 * SEATS:]
        out.append({"frame": fr, "flash": flash, "shooter": shooter,
                    "tanks": [(on, x, y, a * 2) for on, x, y, a in tanks], "px": px})
    return out


def glyph_code(ch):
    if ch == " ":
        return 0
    if ch.isdigit():
        return 0x10 + int(ch)
    if "A" <= ch <= "Z":
        return 0x1A + ord(ch) - 65
    return {".": 0x34, ",": 0x35, "+": 0x36, "$": 0x37}.get(ch, 0)


put_px = A.put_px
tank_pic = A.tank_pic


class Mock:
    def __init__(self, look, palette):
        self.look = look
        self.sky, self.dirt, self.hot, self.seatc = PALETTES[palette]
        self.prev = None

    def sprites_for(self, f, k):
        live = [i for i, t in enumerate(f["tanks"]) if t[0]]
        sh = f["shooter"]
        pri = [s for s in (sh, OWN) if s in live]
        if sh == OWN:
            pri = [OWN]
        if self.look == "A":
            return pri
        rest = [s for s in live if s not in pri]
        if self.look == "B":
            ordered = sorted(rest, key=lambda s: (s - sh) % SEATS)
            return pri + ordered[:4 - len(pri)]
        free = 4 - len(pri)
        if not rest:
            return pri
        n = len(rest)
        return pri + [rest[(k * free + j) % n] for j in range(min(free, n))]

    def build(self, f, k, hud=("", "")):
        """the UVI for frame f (k: its number, for C's rotation); returns
        (uvi, conflicts, wrong blocks, cells)"""
        spr = self.sprites_for(f, k)
        kinds = [[SKY if p == 0 else DIRT if p == 1 else HOT for p in f["px"][y * FW:(y + 1) * FW]]
                 for y in range(FH)]
        for i, (on, x, y, a) in enumerate(f["tanks"]):
            if on and i not in spr and self.look != "C":
                for bx in (x - 1, x):
                    if 0 <= bx < FW and 0 < y <= FH:
                        kinds[y - 1][bx] = HOT
        u = A.UVI()
        u.poke(0x19F9, 0x80 | self.hot << 3 | self.sky)
        u.poke(0x19F8, 0x80 | 0x40 | self.dirt << 3 | self.dirt)
        u.poke(0x18FD, 0x80)
        u.poke(0x18FC, VPOS)
        conflicts = wrong = 0
        cells = []
        for r in range(FROWS):
            for c in range(16):
                ks = [kinds[2 * r + h][3 * c + s] for h in (0, 1) for s in (0, 1, 2)]
                bits = [(2, 1, 0), (5, 4, 3)]
                have = set(ks)
                pat = lambda want: sum(1 << bits[i // 3][i % 3] for i, kk in enumerate(ks) if kk in want)
                if have == {SKY}:
                    code = 0x80
                elif have == {DIRT}:
                    code = 0x00
                elif have == {SKY, DIRT}:
                    code = 0x80 | pat({DIRT})
                elif have == {HOT}:
                    code = 0xC0 | 0x3F
                elif have == {SKY, HOT}:
                    code = 0xC0 | pat({HOT})
                elif have == {DIRT, HOT}:
                    code = 0x40 | pat({HOT})
                else:                       # all three: HOT kept, the ground the majority
                    conflicts += 1
                    ns, nd = ks.count(SKY), ks.count(DIRT)
                    wrong += min(ns, nd)
                    code = (0xC0 if ns >= nd else 0x40) | pat({HOT})
                cells.append(code)
        # the HUD
        for r, text in ((HUD, hud[0]), (HUD + 1, hud[1])):
            row = [0x40] + [0x40 | glyph_code(ch) for ch in text[:15].ljust(15)]
            cells.extend(row)
        scr = bytes(cells)
        u.poke(0x1800, scr[:13 * 16])
        u.poke(0x1A00, scr[13 * 16:])
        # sprites
        cols = []
        for n, s in enumerate(spr[:4]):
            on, x, y, a = f["tanks"][s]
            u.poke(0x1980 + 8 * n, tank_pic(a))
            left = put_px(x) - 4
            top = YPOS + 4 * y - 8
            u.poke(0x18F0 + 2 * n, [256 - top, left + 43])
            cols.append(self.seatc[s % len(self.seatc)])
        cols += [0] * (4 - len(cols))
        u.poke(0x19FB, 0xC0 | cols[0] << 3 | cols[1])
        u.poke(0x19FA, 0xC0 | cols[2] << 3 | cols[3])
        return u, conflicts, wrong, scr[:FROWS * 16]


def hud_for(f, weapon):
    s = f["shooter"]
    on, x, y, a = f["tanks"][s]
    return ("%-6sA%3d P750" % (SEATNAMES[s][:6], a), "%-10s  W12" % NAMES[weapon][:10])


def run_dumps(out, seeds):
    exe = os.path.join(ROOT, "build", "arcdump")
    subprocess.run(["go", "build", "-o", exe, "./cmd/arcdump"], cwd=os.path.join(ROOT, "server"), check=True)
    segs = {}
    for seed in seeds:
        for w in range(33):
            d = os.path.join(out, "dump", "s%d_w%02d" % (seed, w))
            res = {}
            for buf in (160, 240):
                r = subprocess.run([exe, "-seed", str(seed), "-weapon", str(w), "-buf", str(buf), "-out", d],
                                   check=True, capture_output=True, text=True).stdout
                res[buf] = int(r.split(" segment")[0].split()[-1])
            segs["%d/%d" % (seed, w)] = res        # (the dump left is buffer 240's)
    return segs


def native(u):
    return A.to_image(u.frame())


def main(out, seeds="5,1,2"):
    seeds = [int(s) for s in seeds.split(",")]
    os.makedirs(out, exist_ok=True)
    segs = run_dumps(out, seeds)
    stats = {}
    for look in "ABC":
        for palette in PALETTES:
            for seed in seeds:
                for w in range(33):
                    fs = frames(os.path.join(out, "dump", "s%d_w%02d" % (seed, w), "shot1.dmp"))
                    m = Mock(look, palette)
                    prev = None
                    worst = sum_c = changed_max = changed_sum = wrong_max = 0
                    imgs = []
                    for k, f in enumerate(fs):
                        u, c, wr, scr = m.build(f, k, hud_for(f, w))
                        worst, wrong_max, sum_c = max(worst, c), max(wrong_max, wr), sum_c + c
                        if prev is not None:
                            ch = sum(a != b for a, b in zip(scr, prev))
                            changed_max, changed_sum = max(changed_max, ch), changed_sum + ch
                        prev = scr
                        if palette == "day" and seed == seeds[0]:
                            imgs.append(native(u))
                    key = "%s/%s/%d/%d" % (look, palette, seed, w)
                    stats[key] = {"frames": len(fs), "conflict_max": worst, "conflict_mean": sum_c / len(fs),
                                  "wrong_max": wrong_max, "changed_max": changed_max,
                                  "changed_mean": changed_sum / max(len(fs) - 1, 1),
                                  "segs240": segs["%d/%d" % (seed, w)][240], "segs160": segs["%d/%d" % (seed, w)][160]}
                    if imgs:
                        d = os.path.join(out, "anim")
                        os.makedirs(d, exist_ok=True)
                        imgs[0].save(os.path.join(d, "%s_w%02d.png" % (look, w)), save_all=True,
                                     append_images=imgs[1:] + [imgs[-1]] * 60,
                                     duration=[17 if i % 3 else 16 for i in range(len(imgs) + 60)],
                                     loop=0, optimize=False)
                    print(key, stats[key], flush=True)
    json.dump(stats, open(os.path.join(out, "stats.json"), "w"), indent=1)


if __name__ == "__main__":
    main(*sys.argv[1:])
