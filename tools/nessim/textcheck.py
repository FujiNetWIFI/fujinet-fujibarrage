#!/usr/bin/env python3
"""textcheck.py -- the NES's text screen from a VRAM dump (nesrun's `vram`),
as text, and (--shop) the shop's list held against the catalog.

    textcheck.py VRAM.bin                 the 24 rows, printed (a cell's
                                          form after it where it isn't 0:
                                          1 yellow ink, 2 grey, 3 inverse)
    textcheck.py VRAM.bin --shop CAT.bin  the list's rows (src/room.c's
                                          SHOP_TOP 2, 19 rows) must be the
                                          shop's items in order, each row
                                          one form, one row the selection

A cell is tile = glyph + 64 * form (src/nes/scr.c), glyph ' ' to '_', in
name rows 3-26. CAT.bin is barraged -catalog's (build/nes/catalog.bin).
Exit 1 when the shop's list is wrong.
"""
import sys

NT = 32768                      # (the dump: CHR-RAM 32K, then the name tables)
ROW0 = 3                        # the text's first name row
SHOP_TOP, SHOP_ROWS = 2, 19
SHOP_NAME_N = 13                # (src/layout.h's, 32 columns)
N_ITEMS, I_MINI_MISSILE = 48, 0  # (src/proto.h)


def screen(vram):
    rows = []
    for r in range(24):
        cells = vram[NT + (r + ROW0) * 32: NT + (r + ROW0) * 32 + 32]
        rows.append(("".join(chr(32 + (t & 63)) for t in cells), [t >> 6 for t in cells]))
    return rows


def items(cat):
    """the catalog's item names (src/cat.c's walk: pages, then items)"""
    at = 4
    for _ in range(min(cat[3], 4)):
        at += 1 + cat[at]
    names = []
    for _ in range(N_ITEMS):
        at += 6
        names.append(cat[at + 1: at + 1 + cat[at]].decode("latin-1").upper())
        at += 1 + cat[at]
    return names


def shop(rows, cat):
    names = [n[:SHOP_NAME_N] for i, n in enumerate(items(cat)) if i != I_MINI_MISSILE]
    bad = []
    lst = rows[SHOP_TOP: SHOP_TOP + SHOP_ROWS]
    first = lst[0][0][1:1 + SHOP_NAME_N].rstrip()
    if first not in names:
        return ["row %d: %r is no item" % (SHOP_TOP, first)]
    k = names.index(first)
    sel = 0
    for i, (text, forms) in enumerate(lst):
        r = SHOP_TOP + i
        want = names[k + i] if k + i < len(names) else ""
        if text[1:1 + SHOP_NAME_N].rstrip() != want:
            bad.append("row %d: %r, want %r" % (r, text[1:1 + SHOP_NAME_N].rstrip(), want))
        if text[0] != " " or any(c not in " 0123456789X" for c in text[1 + SHOP_NAME_N:]):
            bad.append("row %d: stray cells: %r" % (r, text))
        if len(set(forms)) != 1:
            bad.append("row %d: forms %s" % (r, "".join(str(f) for f in forms)))
        sel += forms[0] == 1
    if sel != 1:
        bad.append("%d rows selected" % sel)
    return bad


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    rows = screen(open(sys.argv[1], "rb").read())
    if "--shop" in sys.argv:
        cat = open(sys.argv[sys.argv.index("--shop") + 1], "rb").read()
        bad = shop(rows, cat)
        for b in bad[:12]:
            print(b)
        print("shop: %s" % ("%d faults" % len(bad) if bad else "right"))
        sys.exit(1 if bad else 0)
    for text, forms in rows:
        print(text.rstrip() + ("  " + "".join(str(f) for f in forms) if any(forms) else ""))


if __name__ == "__main__":
    main()
