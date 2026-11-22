"""intvart.py -- FujiBarrage's Intellivision art and colours, in one place
for tools/mkgfx_intv.py (the ROM's tables) and tools/preview_intv.py (mock
screens): the palette, the skies, the dirt, the seats, the GRAM map, the
cards (gfx/intv/art.txt, and the ones made here: the fire's edges, the
levels, the logo)."""
import os

# the STIC's 16 colours as jzIntv shows them (tools/intvsim/intvsim.py)
PAL = [(0x00, 0x00, 0x00), (0x00, 0x2D, 0xFF), (0xFF, 0x3D, 0x10), (0xC9, 0xCF, 0xAB),
       (0x38, 0x6B, 0x3F), (0x00, 0xA7, 0x56), (0xFA, 0xEA, 0x50), (0xFF, 0xFC, 0xFF),
       (0xBD, 0xAC, 0xC8), (0x24, 0xB8, 0xFF), (0xFF, 0xB4, 0x1F), (0x54, 0x6E, 0x00),
       (0xFF, 0x4E, 0x57), (0xA4, 0x96, 0xFF), (0x75, 0xCC, 0x80), (0xB5, 0x1A, 0x58)]
(BLACK, BLUE, RED, TAN, DKGREEN, GREEN, YELLOW, WHITE, GREY, CYAN, ORANGE, BROWN, PINK, LTBLUE,
 YGREEN, PURPLE) = range(16)
NAMES = ["black", "blue", "red", "tan", "dark green", "green", "yellow", "white", "grey", "cyan",
         "orange", "brown", "pink", "light blue", "yellow-green", "purple"]

ROWS = 10       # the field's card rows (screen rows 2-11)

# each sky (SKY_*: plain, gradient, stars, storm, sunset, cavern, black) a
# background colour a card row, top first
SKIES = [
    [LTBLUE] * ROWS,                                                        # plain
    [BLUE, BLUE, BLUE, LTBLUE, LTBLUE, LTBLUE, LTBLUE, CYAN, CYAN, CYAN],   # gradient: night to day
    [BLACK] * ROWS,                                                         # stars (star cards in it)
    [BLACK, BLACK, BLACK, GREY, GREY, GREY, GREY, GREY, GREY, GREY],        # storm
    [BLUE, PURPLE, PURPLE, PINK, PINK, RED, ORANGE, ORANGE, YELLOW, YELLOW],  # sunset
    [BLACK] * ROWS,                                                         # cavern
    [BLACK] * ROWS,                                                         # black
]
SKY_NAMES = ["plain", "gradient", "stars", "storm", "sunset", "cavern", "black"]
STAR_ROWS = 5   # stars in the top rows of the stars sky

# the dirt (ST palette 0-7): a foreground colour, so one of the 8 primaries
DIRT = [TAN, DKGREEN, GREEN, RED, YELLOW, WHITE, BLACK, BLUE]

# the seats' tanks (MOBs: any of the 16): none of the skies' blues, so a
# tank stands out on the sky it mostly sits against
SEATS = [RED, YELLOW, WHITE, GREEN, PURPLE, ORANGE, PINK, YGREEN, BROWN, DKGREEN]
LIGHT = {YELLOW, WHITE, TAN, YGREEN, CYAN, ORANGE, PINK, LTBLUE, GREY}   # black text on them

# the fire's colours, cycled (backgrounds of a fireball's inside cells: any
# of the 16; its edges' foregrounds: the primaries)
FIRE_BG = [YELLOW, ORANGE, RED, ORANGE, YELLOW, WHITE]
FIRE_FG = [YELLOW, RED, RED, YELLOW, WHITE, WHITE]

# ---- the GRAM map (FG/BG mode: cards 0-63) ----------------------------------
G_SOLID = 0
G_LEVEL = 1        # 1-3: the bottom 2, 4, 6 rows (a cell the pool had no card for)
G_TANK = 4         # 4-8: barrel at 0, 22, 45, 68, 90 degrees
G_ARROW = 9
G_CHUTE = 10       # 10-11: canopy over a tank (a 16-row MOB: an even card first)
G_STAR = 12        # 12-13
G_HEAD = 14        # 14-19: track heads (rebuilt each frame)
N_HEAD = 6
G_POOL = 20        # 20-63: the field's cells (dirt, fire)
N_POOL = 64 - G_POOL
G_LOGO = 20        # the title's logo: over the pool (only on the title: the tanks keep theirs)

TANK_ANGLES = [0, 22, 45, 68, 90]


def read_art(path=None):
    if path is None:
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gfx", "intv", "art.txt")
    cards, name, rows = {}, None, []
    for ln in open(path):
        ln = ln.rstrip("\n")
        if ln.startswith("# ") or ln == "#" or not ln.strip():   # (a row may start with '#')
            continue
        if ln.startswith("card "):
            name, rows = ln[5:].strip(), []
            cards[name] = rows
            continue
        if name is not None and len(rows) < 8:
            assert len(ln) == 8 and set(ln) <= {"#", "."}, ln
            rows.append(ln)
    for n, r in cards.items():
        assert len(r) == 8, n
    return cards


def bits(rows):
    """a card's 8 bytes, bit 7 the left pixel"""
    return [int(r.replace("#", "1").replace(".", "0"), 2) for r in rows]


def fire_edges():
    """the 8 edges of a fireball, N NE E SE S SW W NW: the side towards the
    fire's middle filled (a quarter disc in the corners)"""
    out = []
    for d in range(8):
        dx = [0, 1, 1, 1, 0, -1, -1, -1][d]    # where the cell is from the middle
        dy = [-1, -1, 0, 1, 1, 1, 0, -1][d]
        rows = []
        for y in range(8):
            r = ""
            for x in range(8):
                px, py = x + 0.5, y + 0.5
                if dx and dy:   # a corner: all but the outer corner, cut round
                    ox = 8 if dx > 0 else 0
                    oy = 8 if dy > 0 else 0
                    on = (px - ox) ** 2 + (py - oy) ** 2 >= 6.0 ** 2 and \
                        (px - (8 - ox)) ** 2 + (py - (8 - oy)) ** 2 <= 9.5 ** 2
                elif dx:        # a side: the inner half, its edge rounded a little
                    cx = 0 if dx > 0 else 8
                    reach = 4.5 + 0.8 * (1 - abs(py - 4) / 4)
                    on = abs(px - cx) <= reach
                else:
                    cy = 0 if dy > 0 else 8
                    reach = 4.5 + 0.8 * (1 - abs(px - 4) / 4)
                    on = abs(py - cy) <= reach
                r += "#" if on else "."
            rows.append(r)
        out.append(rows)
    return out


def levels():
    return [["." * 8] * (8 - n) + ["#" * 8] * n for n in (2, 4, 6)]


# ---- the logo -------------------------------------------------------------------
FONT = {   # 5x7
    "F": ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    "U": ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "J": ["..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."],
    "I": [".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."],
    "B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    "G": [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###."],
    "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
}


def letter(ch, sx, sy, thick):
    """a 5x7 letter, each dot sx by sy pixels, strokes thickened `thick`
    pixels to the right: rows of '#'/'.'"""
    w, h = 5 * sx + thick, 7 * sy
    pix = [["."] * w for _ in range(h)]
    for r, line in enumerate(FONT[ch]):
        for c, d in enumerate(line):
            if d == "#":
                for yy in range(sy):
                    for xx in range(sx + thick):
                        pix[r * sy + yy][c * sx + xx] = "#"
    return ["".join(p) for p in pix]


# the title's lines: word, dot size, thickening, pitch (whole cards: a
# letter repeated is the same cards), the top card row
LOGO_LINES = [("FUJI", 1, 2, 1, 8, 1), ("BARRAGE", 3, 3, 0, 16, 4)]


def logo():
    """the title's two lines, each letter on a whole-card pitch and the
    lines centred: [(col, row, rows8, line)] for the cells with ink, and the
    unique cards"""
    cells = []
    for line, (word, sx, sy, thick, pitch, top) in enumerate(LOGO_LINES):
        cw = len(word) * pitch // 8
        ch = (7 * sy + 7) // 8
        col0 = (20 - cw) // 2
        for i, c in enumerate(word):
            lt = letter(c, sx, sy, thick)
            lw, lh = len(lt[0]), len(lt)
            ox = (pitch - lw) // 2
            oy = (ch * 8 - lh) // 2
            full = [["."] * pitch for _ in range(ch * 8)]
            for y in range(lh):
                for x in range(lw):
                    full[oy + y][ox + x] = lt[y][x]
            for cy in range(ch):
                for cx in range(pitch // 8):
                    rows = ["".join(full[cy * 8 + y][cx * 8:cx * 8 + 8]) for y in range(8)]
                    if any("#" in r for r in rows):
                        cells.append((col0 + i * pitch // 8 + cx, top + cy, rows, line))
    uniq = []
    for _, _, rows, _ in cells:
        if rows not in uniq:
            uniq.append(rows)
    return cells, uniq
