#!/usr/bin/env python3
"""refcheck.py -- the Adam's player against the server's reference view
(tools/cocosim/refcheck.py, on adamrun: one TEST=1 build serves every run).

usage: tools/adamsim/refcheck.py [-gate] [-flat] [-seed N] [-shield I]
           [-aim ANGLE POWER] [-j N] OUTDIR [WEAPON...]

For each weapon (proto I_* number): refview makes a shot with it at the
Adam's shape (tools/adamsim/fixture.py), adamrun loads the fixture into
the TEST=1 build (make adam TEST=1, as it is), plays scene 1 with arg 2
(stop after the shot) serving the shot's segments, and the field store
(tools/adamsim/adamfield.py) is compared with refview's field after the
shot, pixel for pixel: dirt and fire. The lag: the frames the shot took
to play less the script's own. OUTDIR/<run>/W/{adam,ref,diff}.png.
-gate: every weapon (0-32), hilly, flat and with shields (I_SHIELD on the
even seats): 99 runs. Exits non-zero if any pixel differs.
"""
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import adamfield  # noqa: E402
import fixture  # noqa: E402

W, H = 256, 168
I_SHIELD = 42
ADAMRUN = os.path.join(HERE, "adamrun")
DDP = os.path.join(TOP, "r2r", "adam", "barrage.ddp")
MAP = os.path.join(TOP, "build", "adam", "game.map")


def one(d, w, seed, extra):
    os.makedirs(d, exist_ok=True)
    frames = fixture.make(d, ["-seed", seed, "-weapon", str(w)] + extra)
    scen = os.path.join(d, "scene")
    open(scen, "w").write(
        "until _test_hook+1 0xC3 600\nload _fix_ram %s\nserve _test_hook %s\npoke _test_hook+2 1 2\n"
        "poke _test_hook 0x5A\nuntil _test_hook 0xDD 30000\nwait 2\npeekw _test_t1\ndump ram.bin\n"
        "shot screen.ppm\n" % (os.path.join(d, "fix.bin"), d))
    r = subprocess.run([ADAMRUN, DDP, MAP, scen, d], capture_output=True, text=True)
    if r.returncode:
        return False, None, "weapon %2d: the run failed: %s" % (w, (r.stderr or r.stdout).strip())
    played = None
    for line in r.stdout.splitlines():
        if line.startswith("_test_t1:"):
            played = int(line.split()[1])
    f = adamfield.field(open(os.path.join(d, "ram.bin"), "rb").read())
    ref = open(os.path.join(d, "ref", "shot1.px"), "rb").read()
    nd = nf = 0
    theirs, diff = [], []
    for y in range(H):
        tr, dr = [], []
        for x in range(W):
            v = f[y][x]
            rd = {0: 1, 1: 2, 2: 3}.get(ref[y * W + x], 1)     # refview: 0 sky, 1 dirt, 2 fire
            tr.append(rd)
            if (v == 2) != (rd == 2):
                nd += 1
            elif (v == 3) != (rd == 3):
                nf += 1
            dr.append(4 if v != rd else v)
        theirs.append(tr)
        diff.append(dr)
    adamfield.picture(os.path.join(d, "adam.png"), f)
    adamfield.picture(os.path.join(d, "ref.png"), theirs)
    adamfield.picture(os.path.join(d, "diff.png"), diff)
    lag = None if played is None else played - frames
    return nd == 0 and nf == 0, lag, "weapon %2d: %4d dirt, %4d fire pixels differ, %s frames behind (%d)" % (
        w, nd, nf, "?" if lag is None else "%3d" % lag, frames)


def main():
    args = sys.argv[1:]
    gate = "-gate" in args
    flat = "-flat" in args
    args = [a for a in args if a not in ("-gate", "-flat")]
    seed, extra, jobs = "5", [], 8
    while args and args[0] in ("-seed", "-aim", "-shield", "-j"):
        if args[0] == "-seed":
            seed, args = args[1], args[2:]
        elif args[0] == "-aim":
            extra, args = extra + ["-angle", args[1], "-power", args[2]], args[3:]
        elif args[0] == "-shield":
            extra, args = extra + ["-shield", args[1]], args[2:]
        else:
            jobs, args = int(args[1]), args[2:]
    if not args:
        sys.exit(__doc__)
    out = os.path.abspath(args[0])
    weapons = [int(a) for a in args[1:]] or list(range(33))
    runs = [("hilly", []), ("flat", ["-flat"]), ("shield", ["-shield", str(I_SHIELD)])] if gate else \
        [("flat" if flat else "run", ["-flat"] if flat else [])]
    fixture.refview()
    jobsl = [(name, more, w) for name, more in runs for w in weapons]
    with ThreadPoolExecutor(jobs) as ex:
        res = list(ex.map(lambda j: (j, one(os.path.join(out, j[0], str(j[2])), j[2], seed, extra + j[1])), jobsl))
    bad, lags = 0, []
    cur = None
    for (name, _, w), (ok, lag, msg) in res:
        if name != cur:
            print("--", name)
            cur = name
        print(msg + ("" if ok else "  <--"))
        bad += not ok
        if lag is not None:
            lags.append((lag, name, w))
    if lags:
        worst = max(lags)
        print("lag: worst %d frames (%s, weapon %d), mean %.1f" % (worst[0], worst[1], worst[2],
                                                                 sum(l for l, _, _ in lags) / len(lags)))
    print("%d of %d runs differ" % (bad, len(res)))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
