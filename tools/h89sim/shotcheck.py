#!/usr/bin/env python3
"""shotcheck.py PX.bin RUNLOG [N] -- the H89's field store after shots
against server/cmd/tapview's replay of the same frames: PX.bin is W*H
view values (0 sky, 1 dirt, 2 fire, 3 tank), RUNLOG h89sim's output with
`peek fld_store 1760` lines (N: which, from 1; default the last). Sky and
dirt are compared; a block that is fire or a tank in either is left out
(the turn's arrow, a shot in the air).
"""
import sys

W, H = 160, 44


def peeks(runlog):
    out = []
    for line in open(runlog, errors="replace"):
        p = line.split()
        if len(p) > 2 and p[0] == "PEEK" and p[1] == "fld_store":
            out.append(bytes(int(x, 16) for x in p[2:]))
    return out


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    px = open(sys.argv[1], "rb").read()
    ps = peeks(sys.argv[2])
    if not ps:
        sys.exit("no peek of fld_store in " + sys.argv[2])
    s = ps[int(sys.argv[3]) - 1] if len(sys.argv) > 3 else ps[-1]
    nd = nskip = 0
    bad = []
    for y in range(H):
        for x in range(W):
            e = px[y * W + x]
            v = (s[y * (W // 4) + x // 4] >> ((x & 3) * 2)) & 3     # 1 sky, 2 dirt, 3 fire
            if e >= 2 or v == 3:
                nskip += 1
                continue
            if v - 1 != e:
                nd += 1
                bad.append((x, y, e, v - 1))
    print("h89: %d sky/dirt blocks differ (%d fire/tank left out)" % (nd, nskip))
    for b in bad[:12]:
        print("  x %d y %d: reference %d, client %d" % b)
    return 1 if nd else 0


if __name__ == "__main__":
    sys.exit(main())
