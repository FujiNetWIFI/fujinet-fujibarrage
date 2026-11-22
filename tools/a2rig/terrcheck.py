#!/usr/bin/env python3
"""terrcheck.py -- the Apple's field against the terrain the server sent it.

usage: terrcheck.py TAPLOG HGR.BIN [OUT.png]

TAPLOG is tools/rigs/tapproxy.py's log of what the server sent; its
TERRAIN (G) frames are replayed (a whole field, then diffs on it, as
src/net.c takes them) into the field they make. HGR.BIN is HGR page 1
(a2rig's `dump hgr.bin 0x2000 8192`), decoded by tools/a2sim/hgrfield.py.
Every pixel the Apple shows as sky or dirt must be the terrain's; tank (0)
and fire (3) pixels are counted, not compared. OUT: the differences in red
over the field.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "a2sim"))
import hgrfield  # noqa: E402

W, H = hgrfield.W, hgrfield.H
T_EMPTY = 240
G_LAST = 1


def column(e):
    """a column entry -> (its values top to bottom, its length)"""
    b = e[0]
    col = [1] * H
    if b < T_EMPTY:
        for y in range(b, H):
            col[y] = 2
        return col, 1
    if b == T_EMPTY:
        return col, 1
    n = b & 15
    for k in range(n):
        top, end = e[1 + 2 * k], e[2 + 2 * k]
        for y in range(top, min(end, H)):
            col[y] = 2
    return col, 1 + 2 * n


def terrain(log):
    field, building, tv = None, None, 0xFF
    for line in open(log):
        w = line.split()
        if len(w) < 2 or w[0] != "G":
            continue
        p = bytes.fromhex(w[1])
        if p[0] == 0xFF:
            building = [[1] * H for _ in range(W)] if building is None or tv != 0xFE else building
            tv = 0xFE
        elif p[0] != tv:
            continue                    # (not what the client had: it asks again)
        else:
            building = [c[:] for c in field]
        x, i = p[3] | p[4] << 8, 5
        while i < len(p) and x < W:
            building[x], n = column(p[i:])
            i += n
            x += 1
        if p[2] & G_LAST:
            field, tv = building, p[1]
    return field


def main():
    log, dump = sys.argv[1:3]
    t = terrain(log)
    if t is None:
        sys.exit("no whole terrain in " + log)
    data = open(dump, "rb").read()
    a = hgrfield.field(data)
    diff, tank, fire = [], 0, 0
    for y in range(H):
        for x in range(W):
            v = a[y][x]
            if v == 0:
                tank += 1
            elif v == 3:
                fire += 1
            elif v != t[x][y]:
                diff.append((x, y))
    print("%d pixels differ (sky/dirt); %d tank, %d fire pixels not compared" % (len(diff), tank, fire))
    if diff:
        print("first:", " ".join("%d,%d" % d for d in diff[:12]))
    if len(sys.argv) > 3:
        from PIL import Image
        im = Image.new("RGB", (W, H))
        pal = hgrfield.COL[1]
        px = [pal[a[y][x]] for y in range(H) for x in range(W)]
        for x, y in diff:
            px[y * W + x] = (255, 0, 0)
        im.putdata(px)
        im.resize((W * 4, round(H * 2.1875 * 2)), Image.NEAREST).save(sys.argv[3])


if __name__ == "__main__":
    main()
