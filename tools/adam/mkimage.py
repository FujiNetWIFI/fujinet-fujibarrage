#!/usr/bin/env python3
"""Make the Adam's boot media from the two programs z88dk linked:
r2r/adam/barrage.ddp (a tape, 256K) and barrage.dsk (a disk, 160K).

usage: mkimage.py BUILD R2R   (BUILD/game.ddp, menu.ddp, game.map, menu.map)

z88dk's .ddp has its bootstrap in block 0 and the program in blocks 1..N
(the bootstrap reads them to 0000h). The game's blocks come first, then
the menu's, then the art (gen/adam/art.bin: the VRAM's 3800h-3BFFh); each
program's chain table (src/adam/chain.asm, found by its "CHNT") says where
they are, so either can load the other, and each reads the art.

Checks the memory: a program must end below C800h (the bootstrap, where it
is loaded from); the game's variables must end below the field store
(SHADOW_ORG), the menu's below the stack's floor.
"""
import os
import re
import sys

BLOCK = 1024
DSK_BLOCKS = 160
BOOTSTRAP = 0xC800
STACK_LOW = 0xD100              # (src/adam/plat.c STACK_LOW; the top is D390h, EOS's)
SHADOW_ORG = 0xA700             # the field store (src/tms9918/fld.asm), 10.5K to D0FFh


def symbols(path):
    out = {}
    for line in open(path):
        m = re.match(r"(\S+)\s+=\s+\$([0-9A-Fa-f]+)", line)
        if m:
            out[m.group(1)] = int(m.group(2), 16)
    return out


def program(d, name, bss_limit, errors):
    ddp = open(os.path.join(d, name + ".ddp"), "rb").read()
    sym = symbols(os.path.join(d, name + ".map"))
    end = sym["__DATA_END_tail"]
    bss = sym["__BSS_END_tail"]
    if end > BOOTSTRAP:
        errors.append("the %s ends at %04Xh, over the bootstrap (C800h)" % (name, end))
    if bss > bss_limit:
        errors.append("the %s's variables end at %04Xh, over %04Xh" % (name, bss, bss_limit))
    blocks = end // BLOCK + 1       # (as the bootstrap counts them)
    return ddp, ddp[BLOCK:BLOCK + blocks * BLOCK], blocks, end, bss


def patch(img, table):
    at = img.find(b"CHNT")
    if at < 0:
        return False
    img[at + 4:at + 4 + len(table)] = bytes(table)
    return True


def main():
    d, r = sys.argv[1], sys.argv[2]
    errors = []
    gddp, game, gn, gend, gbss = program(d, "game", SHADOW_ORG, errors)
    _, menu, mn, mend, mbss = program(d, "menu", STACK_LOW, errors)
    game, menu = bytearray(game), bytearray(menu)
    ab = 1 + gn + mn                    # the art: gen/adam/art.bin (tools/mkgfx_tms.py art)
    art = open(os.path.join(os.path.dirname(__file__), "..", "..", "gen", "adam", "art.bin"), "rb").read()
    if len(art) != BLOCK or art[508:512] != b"ART!":
        errors.append("gen/adam/art.bin isn't the art block")
    table = [1, 0, gn, (1 + gn) & 255, (1 + gn) >> 8, mn, ab & 255, ab >> 8, 1]
    for img, name in ((game, "game"), (menu, "menu")):
        if not patch(img, table):
            errors.append("no CHNT table in the %s" % name)
    total = 1 + gn + mn + 1
    if total > DSK_BLOCKS:
        errors.append("%d blocks: over a disk's %d" % (total, DSK_BLOCKS))
    if errors:
        for e in errors:
            print("mkimage.py: " + e, file=sys.stderr)
        sys.exit(1)
    ddp = bytearray(256 * BLOCK)
    ddp[0:BLOCK] = gddp[0:BLOCK]
    ddp[BLOCK:BLOCK + len(game)] = game
    ddp[(1 + gn) * BLOCK:(1 + gn) * BLOCK + len(menu)] = menu
    ddp[ab * BLOCK:(ab + 1) * BLOCK] = art
    os.makedirs(r, exist_ok=True)
    open(os.path.join(r, "barrage.ddp"), "wb").write(ddp)
    open(os.path.join(r, "barrage.dsk"), "wb").write(ddp[:DSK_BLOCKS * BLOCK])
    print("adam: game 0000-%04X, variables to %04X (%d bytes under the store at %04X), blocks 1-%d"
          % (gend - 1, gbss - 1, SHADOW_ORG - gbss, SHADOW_ORG, gn))
    print("adam: menu 0000-%04X, variables to %04X (%d bytes under the stack), blocks %d-%d; art block %d"
          % (mend - 1, mbss - 1, STACK_LOW - mbss, 1 + gn, gn + mn, ab))


if __name__ == "__main__":
    main()
