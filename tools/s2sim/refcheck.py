#!/usr/bin/env python3
"""refcheck.py -- the Studio II plays a weapon's shot as the reference does.

usage: tools/s2sim/refcheck.py [-flat] [-seed N] [-shield I] [-angle A] [-power P] [-gate] OUTDIR [WEAPON...]

For each weapon (proto I_* number; default all 33): refview makes a shot with
it at the Studio II's caps exactly as the server projects them from its
HELLO (64x108 at 60 Hz, PAR 210, tank 3x4, a 1536-byte buffer, six shell
tracks, PLAT_STUDIO2) and writes it as an asm fixture; tools/s2/mkfix_s2.py
patches it into a copy of the TEST image's FIXBUF (one build serves every
weapon); s2sim plays test scene 3 (the field, the tanks, each segment in
turn through the player, as in a room). Then:
  - the model (TOPS, SPN) after the shot against refview's field (shot1.px:
    0 sky, 1 dirt, 2 fire, 3 tank), dirt against the rest: must be 0;
  - the cart's background raster (what we drew; the tanks are sprites,
    not in it) against the same: lit where refview has dirt or fire
    (smoke trails), dark elsewhere (reported, not gated: a settle redraws
    its columns over a trail);
  - the frames the shot ran behind its script (jiffies taken - frames).
-gate: all 33 on hilly ground, flat ground and with a shield (item 42) each:
99 shots. Exit 1 if any model differs.

Builds the TEST image into build/studio2-ref (TEST=1).
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
CAPS = ["-w", "64", "-h", "108", "-par", "210", "-buf", "1536", "-tankw", "3", "-tankh", "4", "-plat", "17"]
W, H = 64, 108


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


def model(tops, spn):
    cols = []
    for x in range(W):
        t = tops[x]
        if t < 0xF0:
            cols.append([(t, H)] if t < H else [])
        elif t == 0xF0:
            cols.append([])
        else:
            slot = (x >> 3) * 256 + (x & 7) * 32
            cols.append([(spn[slot + 2 * k], min(spn[slot + 2 * k + 1], H)) for k in range(t - 0xF0)])
    return [[any(a <= y < b for a, b in cols[x]) for y in range(H)] for x in range(W)]


def one(rv, img, lst, out, w, seed, extra, tag):
    d = os.path.join(out, "%s-%d" % (tag, w))
    os.makedirs(d, exist_ok=True)
    fix = os.path.join(d, "fixture.inc")
    cmd = [rv] + CAPS + ["-shots", "1", "-seed", seed, "-weapon", str(w), "-fixture-asm", fix, "-out", d] + extra
    log = run(cmd).stdout.strip().splitlines()[-1]
    st2 = os.path.join(d, "fix.st2")
    run(["python3", os.path.join(TOP, "tools/s2/mkfix_s2.py"), fix, img, lst, st2])
    scn = os.path.join(d, "scene.txt")
    with open(scn, "w") as f:
        f.write("test 3\nuntil TMARK+15 165 30000\nwait 2\npeek TMARK 4\nssave TOPS 64 tops.bin\n"
                "ssave SPN 2048 spn.bin\nrsave raster.bin\nsnap end\nshort\n")
    env = dict(os.environ, S2SIM_SECONDS="600", S2SIM_LST=lst)
    r = subprocess.run(["python3", os.path.join(HERE, "s2sim.py"), scn, d, st2],
                       capture_output=True, text=True, env=env, cwd=TOP).stdout
    open(os.path.join(d, "s2sim.txt"), "w").write(r)
    m = re.search(r"PEEK [0-9A-F]+: (\w\w) (\w\w) (\w\w) (\w\w)", r)
    if "timeout" in r or not m or not os.path.exists(os.path.join(d, "tops.bin")):
        return None, None, None, log, r
    j = int(m.group(1), 16) | int(m.group(2), 16) << 8
    fr = int(m.group(3), 16) | int(m.group(4), 16) << 8
    lag = j - fr
    ours = model(open(os.path.join(d, "tops.bin"), "rb").read(), open(os.path.join(d, "spn.bin"), "rb").read())
    px = open(os.path.join(d, "shot1.px"), "rb").read()
    ras = open(os.path.join(d, "raster.bin"), "rb").read()
    mdiff, rdiff = [], 0
    for y in range(H):
        for x in range(W):
            ref = px[y * W + x]
            if ours[x][y] != (ref == 1):
                mdiff.append((x, y, ours[x][y], ref))
            lit = (ras[y * 8 + x // 8] >> (7 - x % 8)) & 1
            if lit != (ref in (1, 2)):
                rdiff += 1
    with open(os.path.join(d, "diff.txt"), "w") as fo:
        fo.write(log + "\n")
        for x, y, a, b in mdiff:
            fo.write("x %2d y %3d: ours %s ref %d\n" % (x, y, "dirt" if a else "air", b))
    return mdiff, rdiff, lag, log, r


def main():
    args = sys.argv[1:]
    seed, extra, gate, flat = "5", [], False, False
    while args and args[0].startswith("-"):
        a = args.pop(0)
        if a == "-seed":
            seed = args.pop(0)
        elif a == "-shield":
            extra += ["-shield", args.pop(0)]
        elif a == "-flat":
            flat = True
        elif a == "-gate":
            gate = True
        elif a in ("-angle", "-power"):
            extra += [a, args.pop(0)]
    out = os.path.abspath(args[0])
    weapons = [int(a) for a in args[1:]] or list(range(33))
    os.makedirs(out, exist_ok=True)
    rv = os.path.join(out, "refview")
    run(["go", "build", "-o", rv, "./cmd/refview"], cwd=os.path.join(TOP, "server"))
    bdir = os.path.join(TOP, "build", "studio2-ref")
    run(["bash", os.path.join(TOP, "src/studio2/build.sh")], cwd=TOP,
        env=dict(os.environ, TEST="1", OUT=bdir, R2R=bdir))
    img, lst = os.path.join(bdir, "barrage.st2"), os.path.join(bdir, "barrage.lst")
    if gate:
        runs = [("hilly", []), ("flat", ["-flat"]), ("shield", ["-shield", "42"])]
    else:
        runs = [("flat" if flat else "hilly", (["-flat"] if flat else []) + extra)]
    bad, lags = 0, []
    for tag, ex in runs:
        for w in weapons:
            mdiff, rdiff, lag, log, r = one(rv, img, lst, out, w, seed, ex, tag)
            sh = re.search(r"SHORT (\d+)", r)
            sh = sh.group(1) if sh else "?"
            if mdiff is None:
                print("%-6s weapon %2d: the shot never ended  (%s)" % (tag, w, log))
                bad += 1
                continue
            lags.append((lag, tag, w))
            print("%-6s weapon %2d: model %3d px differ, raster %4d, %5d frames behind, short %s  (%s)"
                  % (tag, w, len(mdiff), rdiff, lag, sh, log))
            bad += bool(mdiff) or sh != "0"
    if lags:
        lags.sort()
        print("lag: worst %d frames (%s %d), mean %.1f" % (lags[-1][0], lags[-1][1], lags[-1][2],
                                                          sum(l[0] for l in lags) / len(lags)))
    print("%d of %d shots differ" % (bad, sum(len(weapons) for _ in runs)))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
