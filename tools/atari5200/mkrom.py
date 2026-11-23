#!/usr/bin/env python3
"""mkrom.py -- the 5200 FujiBarrage image: a 128K Super Cart, checked.

    tools/atari5200/mkrom.py MENU.map MENU.bin GAME.map GAME.bin OUT.a52

Bank 0 is the menu (src/atari5200/menu-a52.cfg: 32K). Banks 1-3 are the
game (barrage-a52.cfg: 96K): ld65 links the core (__REPL__ to $AFFF, and
$B800 to the header's end) into bank 3, the one the console starts in;
here banks 1 and 2 get the same bytes, so the core runs whichever bank is
in. (M.U.L.E.'s mkrom.py, with FujiBlast's checks.)

Over every code segment, in its own bank:
  - no store and no read-modify-write into the cart window ($4000-$BFFF):
    the edge has no R/W;
  - no read of the mailbox's hotspots ($B500-$B7FF) but the lib's
    (fuji_atari5200_go.o), and none indexed from a base that isn't
    page-aligned;
  - no read of the bank switch ($BFC0-$BFFF) but bank.o's and the crt0's;
  - no JMP ($xxFF);
and in every bank the mailbox and the switch's reads $FF, nothing in the 4
bytes below $BFC0 (an RTS's dummy read), "FUJI" version 1 kind 4 at $BFE0,
$02 at $BFE7, $FF at $BFFD, the entry in the code. A bad image isn't written.
"""
import re
import sys

WIN = 0x4000
BANK = 0x8000
HOT_LO, HOT_HI = 0xB500, 0xB7FF
SW_LO = 0xBFC0

LEN = [1] * 256
for op in (0x69, 0x29, 0xC9, 0xE0, 0xC0, 0x49, 0xA9, 0xA2, 0xA0, 0x09, 0xE9,
           0xA5, 0xA6, 0xA4, 0x85, 0x86, 0x84, 0x65, 0x25, 0x06, 0x24, 0xC5,
           0xC6, 0x45, 0xE6, 0x46, 0x26, 0x66, 0xE5, 0x05, 0x75, 0x35, 0x16,
           0xD5, 0xD6, 0x55, 0xF6, 0x56, 0x36, 0x76, 0xF5, 0x15, 0xB5, 0xB4,
           0x95, 0x94, 0xB6, 0x96, 0x61, 0x21, 0xC1, 0x41, 0xA1, 0x01, 0xE1,
           0x81, 0x71, 0x31, 0xD1, 0x51, 0xB1, 0x11, 0xF1, 0x91, 0xC4, 0xE4,
           0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0):
    LEN[op] = 2
for op in (0x6D, 0x2D, 0x0E, 0x2C, 0xCD, 0xEC, 0xCC, 0xCE, 0x4D, 0xEE, 0x4C,
           0x20, 0xAD, 0xAE, 0xAC, 0x4E, 0x0D, 0x2E, 0x6E, 0xED, 0x8D, 0x8E,
           0x8C, 0x7D, 0x3D, 0x1E, 0xDD, 0xDE, 0x5D, 0xFD, 0xFE, 0x5E, 0xBD,
           0xBC, 0x3E, 0x7E, 0x1D, 0x9D, 0x79, 0x39, 0xD9, 0x59, 0xB9, 0xBE,
           0x19, 0xF9, 0x99, 0x6C):
    LEN[op] = 3

STORES = {0x8D, 0x8E, 0x8C, 0x9D, 0x99}
RMW = {0x0E, 0x1E, 0x2E, 0x3E, 0x4E, 0x5E, 0x6E, 0x7E, 0xCE, 0xDE, 0xEE, 0xFE}
IDX_READS = {0x7D, 0x3D, 0xDD, 0x5D, 0xFD, 0xBD, 0xBC, 0x1D,
             0x79, 0x39, 0xD9, 0x59, 0xB9, 0xBE, 0x19, 0xF9}
JMPS = {0x4C, 0x20, 0x6C}
CORE_SEGS = ("STARTUP", "LOWCODE", "ONCE", "CODE", "CASCODE", "HICODE")
HOT_MODS = ("fuji_atari5200_go.o",)
SW_MODS = ("bank.s.o", "crt0.s.o")


def section(text, title):
    return re.search(title + r":\s*\n-+\s*\n(.*?)\n\n", text, re.S).group(1)


def read_map(path):
    text = open(path).read()
    segs = {}
    for line in section(text, "Segment list").splitlines():
        p = line.split()
        if len(p) >= 4 and re.fullmatch(r"[0-9A-Fa-f]{6}", p[1]):
            segs[p[0]] = (int(p[1], 16), int(p[2], 16))
    pieces = []
    cur = None
    for line in section(text, "Modules list").splitlines():
        if not line.startswith(" "):
            cur = line.strip().rstrip(":")
            continue
        m = re.match(r"\s+(\w+)\s+Offs=([0-9A-Fa-f]+)\s+Size=([0-9A-Fa-f]+)", line)
        if m and m.group(1) in segs:
            s = segs[m.group(1)][0] + int(m.group(2), 16)
            pieces.append((m.group(1), s, s + int(m.group(3), 16), cur))
    return segs, pieces


def modname(mod):           # "dir/x.o" or "dir/lib.lib(x.o)" -> "x.o"
    return re.sub(r".*[/(]", "", mod).rstrip(")")


def check(img, bank, segname, s, e, pieces, bad):
    def module_at(a):
        for seg, ps, pe, mod in pieces:
            if seg == segname and ps <= a < pe:
                return mod
        return "?"
    base = bank * BANK + s - WIN
    code = img[base:base + e + 1 - s]
    pc = 0
    while pc < len(code):
        op = code[pc]
        n = LEN[op]
        if pc + n > len(code):
            break
        if n == 3 and op not in JMPS:
            at = s + pc
            tgt = code[pc + 1] | (code[pc + 2] << 8)
            here = "bank %d %s $%04X (%s)" % (bank, segname, at, module_at(at))
            if (op in STORES or op in RMW) and WIN <= tgt <= 0xBFFF:
                bad.append("%s: a store to $%04X: the cart window" % (here, tgt))
            elif HOT_LO <= tgt <= HOT_HI or (op in IDX_READS and HOT_LO <= tgt + 255 and tgt <= HOT_HI):
                if modname(module_at(at)) not in HOT_MODS:
                    bad.append("%s: a read of the mailbox's hotspots ($%04X)" % (here, tgt))
                elif op in IDX_READS and tgt & 0xFF:
                    bad.append("%s: an indexed read of $%04X crosses a hotspot page" % (here, tgt))
            elif SW_LO <= tgt <= 0xBFFF or (op in IDX_READS and tgt + 255 >= SW_LO and tgt <= 0xBFFF):
                if modname(module_at(at)) not in SW_MODS:
                    bad.append("%s: a read of $%04X switches banks" % (here, tgt))
        if op == 0x6C and code[pc + 1] == 0xFF:
            bad.append("bank %d %s $%04X: JMP ($%02X%02X) hits the page-wrap bug"
                       % (bank, segname, s + pc, code[pc + 2], code[pc + 1]))
        pc += n


def main():
    mmap, mbin, gmap, gbin, out = sys.argv[1:6]
    menu = open(mbin, "rb").read()
    game = open(gbin, "rb").read()
    bad = []
    if len(menu) != BANK:
        sys.exit("mkrom: %s is %d bytes, not %d" % (mbin, len(menu), BANK))
    if len(game) != 3 * BANK:
        sys.exit("mkrom: %s is %d bytes, not %d" % (gbin, len(game), 3 * BANK))
    img = bytearray(menu + game)
    gsegs, gpieces = read_map(gmap)
    msegs, mpieces = read_map(mmap)

    # the core into banks 1 and 2: __REPL__ to $AFFF, $B800 to the end
    lo = gsegs["STARTUP"][0]
    for b in (1, 2):
        for a, e in ((lo, 0xB000), (0xB800, 0xC000)):
            img[b * BANK + a - WIN:b * BANK + e - WIN] = img[3 * BANK + a - WIN:3 * BANK + e - WIN]

    for name, (s, e) in msegs.items():
        if name in CORE_SEGS or name == "BOOTCODE":
            check(img, 0, name, s, e, mpieces, bad)
    for name, (s, e) in gsegs.items():
        if name in CORE_SEGS:
            for b in (1, 2, 3):
                check(img, b, name, s, e, gpieces, bad)
        elif name == "FIELDCODE":
            check(img, 3, name, s, e, gpieces, bad)
        elif name == "ROOMCODE":
            check(img, 2, name, s, e, gpieces, bad)
        elif name == "BOOTCODE":
            check(img, 1, name, s, e, gpieces, bad)

    for b in range(4):
        bank = img[b * BANK:(b + 1) * BANK]
        mem = lambda a: bank[a - WIN]
        if any(mem(a) != 0xFF for a in range(0xB000, 0xB800)):
            bad.append("bank %d: the mailbox's $B000-$B7FF is not $FF" % b)
        if any(mem(a) != 0xFF for a in range(0xBFBC, 0xBFE0)):
            bad.append("bank %d: $BFBC-$BFDF is not $FF (the bank switch, and the 4 bytes below it)" % b)
        if bytes(mem(0xBFE0 + i) for i in range(4)) != b"FUJI":
            bad.append("bank %d: no \"FUJI\" claim at $BFE0" % b)
        if (mem(0xBFE4), mem(0xBFE5)) != (1, 4):
            bad.append("bank %d: the claim is not version 1, kind 4 (a Super Cart)" % b)
        if mem(0xBFE7) != 0x02:
            bad.append("bank %d: $BFE7 is not $02 (the 2-port BIOS's PAL check)" % b)
        if mem(0xBFFD) != 0xFF:
            bad.append("bank %d: $BFFD is not $FF (the BIOS's title)" % b)
        vec = mem(0xBFFE) | (mem(0xBFFF) << 8)
        if not WIN <= vec < 0xB000:
            bad.append("bank %d: entry $%04X is not in the program" % (b, vec))
    if msegs["STARTUP"][0] != 0x4000:
        bad.append("the menu's entry (STARTUP) is at $%04X, not $4000 (bank.s jumps there)" % msegs["STARTUP"][0])
    for b in (1, 2):
        if img[b * BANK + lo - WIN:b * BANK + 0xB000 - WIN] != img[3 * BANK + lo - WIN:3 * BANK + 0xB000 - WIN]:
            bad.append("bank %d: the core differs from bank 3's" % b)
    for x in bad:
        print("mkrom: %s" % x, file=sys.stderr)
    if bad:
        sys.exit(1)
    rl = sum(e + 1 - s for s, e in gsegs.values() if lo <= s < 0xB000)
    rh = sum(e + 1 - s for s, e in gsegs.values() if 0xB800 <= s < 0xBFBC)
    print("mkrom: core     $%04X-$AFFF %5d of %5d, $B800-$BFBB %4d of %4d"
          % (lo, rl, 0xB000 - lo, rh, 0xBFBC - 0xB800))
    for b in (1, 2, 3):
        own = img[b * BANK:b * BANK + lo - WIN]
        used = len(own.rstrip(b"\xff"))
        print("mkrom: bank %d   own $4000-$%04X %5d of %5d" % (b, WIN + used - 1, used, lo - WIN))
    m = img[0:BANK]
    used = len(m[:0xB000 - WIN].rstrip(b"\xff"))
    used2 = len(m[0xB800 - WIN:0xBFBC - WIN].rstrip(b"\xff"))
    print("mkrom: bank 0   menu $4000-$AFFF %5d of %5d, $B800-$BFBB %4d of %4d"
          % (used, 0xB000 - WIN, used2, 0xBFBC - 0xB800))
    open(out, "wb").write(bytes(img))


if __name__ == "__main__":
    main()
