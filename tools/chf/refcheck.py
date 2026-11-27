#!/usr/bin/env python3
"""refcheck.py -- the Channel F plays a weapon's shot as the reference does.

usage: tools/chf/refcheck.py [-flat] [-seed N] [-shield I] [-gate] OUTDIR [WEAPON...]

For each weapon (proto I_* number; default all 33): refview makes a shot with
it at the Channel F's caps exactly as the server projects them from its
HELLO (102x46 at 60 Hz, PAR 48, tank 5x3, a 1536-byte buffer, PLAT_CHANNELF)
and writes it as an asm fixture; tools/chf/mkfix_chf.py packs it; chfsim
loads it into the running image's FIXBUF (test scene 3: the field, the
tanks, each segment in turn through the player, as in a room) -- one build
serves every weapon. The field after the shot (FLD, saved) is compared with
refview's (shot1.px), pixel for pixel, sky against dirt; our tanks' pixels
are left out (the reference's field has no tanks), fire left over counts.
Also: the frames the shot ran behind its script (TV marks 98, 99 timed in
clocks), VCHECK (the screen as the game thinks it is) and the settle guard.
-gate: all 33 on hilly ground, flat ground and with a shield (item 42) each:
99 shots. Exit 1 if any differ.

Uses r2r/channelf/barrage.bin and build/channelf/barrage.lst (make channelf).
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
CAPS = ["-w", "102", "-h", "46", "-par", "48", "-buf", "1536", "-tankw", "5", "-tankh", "3", "-plat", "15"]
W, H = 102, 46
CLOCKS = 29568                  # a frame's (NTSC)
VAL = {0xC0: 0, 0x00: 1, 0x40: 2, 0x80: 3}     # FLD's codes as refview's values


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


def one(rv, out, w, seed, extra, tag):
    d = os.path.join(out, "%s-%d" % (tag, w))
    os.makedirs(d, exist_ok=True)
    fix = os.path.join(d, "fixture.inc")
    cmd = [rv] + CAPS + ["-shots", "1", "-seed", seed, "-weapon", str(w), "-fixture-asm", fix, "-out", d] + extra
    log = run(cmd).stdout.strip().splitlines()[-1]
    run(["python3", os.path.join(HERE, "mkfix_chf.py"), fix, os.path.join(d, "fix.bin")])
    scn = os.path.join(d, "scene.txt")
    with open(scn, "w") as f:
        f.write("test 3\nmarks TV\nload %s FIXBUF\npoke FIXRDY 1\nwaitfor TV 99 20000\nwait 2\n"
                "save FLD 6528 fld.bin\nvcheck\n" % os.path.join(d, "fix.bin"))
    env = dict(os.environ, CHFSIM_SECONDS="900")
    r = subprocess.run(["python3", os.path.join(TOP, "tools/chfsim/chfsim.py"), scn, d],
                       capture_output=True, text=True, env=env, cwd=TOP).stdout
    open(os.path.join(d, "chfsim.txt"), "w").write(r)
    marks = {int(a): int(b) for a, b in re.findall(r"MARK (\d+) (\d+)", r)}
    sf = re.search(r"(\d+) frames", log)
    lag = None
    if 98 in marks and 99 in marks and sf:
        lag = (marks[99] - marks[98]) / CLOCKS - int(sf.group(1))
    vc = re.search(r"VCHECK (\d+)", r)
    sg = re.search(r"SETTLE (\d+) early", r)
    if not os.path.exists(os.path.join(d, "fld.bin")) or 99 not in marks:
        return None, lag, log, vc, sg
    fld = open(os.path.join(d, "fld.bin"), "rb").read()
    px = open(os.path.join(d, "shot1.px"), "rb").read()
    diff = []
    for y in range(H):
        for x in range(W):
            v = VAL.get(fld[64 * x + y], 9)
            if v == 3:
                continue
            if v != px[y * W + x]:
                diff.append((x, y, v, px[y * W + x]))
    with open(os.path.join(d, "diff.txt"), "w") as fo:
        fo.write(log + "\n")
        for x, y, a, b in diff:
            fo.write("x %3d y %2d: ours %d ref %d\n" % (x, y, a, b))
    return diff, lag, log, vc, sg


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
    out = os.path.abspath(args[0])
    weapons = [int(a) for a in args[1:]] or list(range(33))
    os.makedirs(out, exist_ok=True)
    rv = os.path.join(out, "refview")
    run(["go", "build", "-o", rv, "./cmd/refview"], cwd=os.path.join(TOP, "server"))
    runs = []
    if gate:
        runs = [("hilly", []), ("flat", ["-flat"]), ("shield", ["-shield", "42"])]
    else:
        runs = [("flat" if flat else "hilly", (["-flat"] if flat else []) + extra)]
    bad, lags = 0, []
    for tag, ex in runs:
        for w in weapons:
            diff, lag, log, vc, sg = one(rv, out, w, seed, ex, tag)
            v = vc.group(1) if vc else "?"
            g = sg.group(1) if sg else "?"
            if diff is None:
                print("%-6s weapon %2d: the shot never ended  (%s)" % (tag, w, log))
                bad += 1
                continue
            lags.append((lag or 0, tag, w))
            ok = not diff and v == "0" and g == "0"
            print("%-6s weapon %2d: %3d px differ, %s frames behind, vcheck %s, settle %s  (%s)"
                  % (tag, w, len(diff), "?" if lag is None else "%5.1f" % lag, v, g, log))
            bad += not ok
    if lags:
        lags.sort()
        print("lag: worst %.1f frames (%s %d), mean %.1f" % (lags[-1][0], lags[-1][1], lags[-1][2],
                                                             sum(x[0] for x in lags) / len(lags)))
    print("refcheck: %d of %d not right" % (bad, len(runs) * len(weapons)))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
