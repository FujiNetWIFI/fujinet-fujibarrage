#!/usr/bin/env python3
"""refcheck.py -- the CoCo plays a weapon's shot as the reference does
(tools/a2sim/refcheck.py's, for the CoCo 1/2 and the CoCo 3).

usage: tools/cocosim/refcheck.py coco2|coco3 [-gate] [-flat] [-seed N]
           [-shield I] [-aim ANGLE POWER] [-j N] OUTDIR [WEAPON...]

For each weapon (proto I_* number): refview makes a shot with it at the
machine's shape (tools/cocosim/terrcheck.py SHAPES; every segment:
-fixture-all) into src/coco{2,3}/test/fixture.h; a TEST=1 build of the disk
is kept for the run. Then the runs play in cocosim, -j at a time (8): scene
1, stopped after the shot; the field is dumped, decoded
(tools/cocosim/cocofield.py) and compared with refview's field after the
shot: dirt against dirt, fire against fire (tank pixels left out: the
reference has none). Prints the pixels that differ (0 is the goal) and how
many frames the shot ran behind the script (test.c's test_t0, test_t1),
and writes OUTDIR/<run>/W/{coco,ref,diff}.png. -gate: every weapon (0-32),
hilly, flat and with shields (I_SHIELD on the even seats): 99 runs. Leaves
the fixture as it was and r2r/coco/barrage.dsk a TEST=1 build of it.
"""
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import cocofield  # noqa: E402
from terrcheck import SHAPES  # noqa: E402

I_SHIELD = 42


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def build():
    return run(["make", "coco", "TEST=1"], cwd=TOP)


def prepare(m, d, w, seed, extra):
    """refview's shot, the fixture, a build of it kept in d"""
    sh = SHAPES[m]
    os.makedirs(d, exist_ok=True)
    fix = os.path.join(TOP, sh["fix"])
    r = run(["go", "run", "./cmd/refview", "-w", str(sh["w"]), "-h", str(sh["h"]), "-par", str(sh["par"]),
             "-tankw", str(sh["tw"]), "-tankh", str(sh["th"]), "-buf", "1536", "-shots", "1", "-seed", seed,
             "-weapon", str(w), "-fixture-all", fix, "-out", d] + extra, cwd=os.path.join(TOP, "server"))
    log = (r.stdout + r.stderr).strip()
    last = log.splitlines()[-1] if log else ""
    if r.returncode:
        return None, last
    b = build()
    if b.returncode:
        err = [l for l in (b.stdout + b.stderr).splitlines() if "rror" in l and "free)" not in l]
        return None, "build failed: %s (%s)" % (err[:2], last)
    shutil.copy(os.path.join(TOP, "r2r/coco/barrage.dsk"), os.path.join(d, "barrage.dsk"))
    shutil.copy(os.path.join(TOP, sh["map"]), os.path.join(d, "game.map"))
    sf = re.search(r"(\d+) frames", log)
    return (int(sf.group(1)) if sf else None), last


def play(m, d, w, frames, last):
    sh = SHAPES[m]
    scn = os.path.join(d, "scene.txt")
    with open(scn, "w") as f:
        f.write("test 1 2\nwait 7200\npeek test_t0 2\npeek test_t1 2\n")
        if m == "coco2":
            f.write("peek $0E38 1\ndump 1000 27FF screen.bin\n")
        else:
            f.write("".join("dumpblk %X b%d.bin\n" % (0x30 + k, k) for k in range(6)))
    sim = os.path.join(d, "sim")
    r = run(["python3", os.path.join(HERE, "cocosim.py"), os.path.join(d, "barrage.dsk"),
             os.path.join(d, "game.map"), scn, sim], env=dict(os.environ, MACHINE=sh["mame"]))
    t = re.findall(r"PEEK test_t(\d) (\w\w) (\w\w)", r.stdout)
    if len(t) < 2:
        return False, None, "weapon %2d: no result from cocosim  (%s)" % (w, last)
    t0 = int(t[0][1] + t[0][2], 16)
    t1 = int(t[1][1] + t[1][2], 16)
    if t1 == 0 and t0:
        return False, None, "weapon %2d: the shot never ended  (%s)" % (w, last)
    lag = ((t1 - t0) & 0xFFFF) - frames if frames is not None else None
    if m == "coco2":
        p = os.path.join(sim, "screen.bin")
        if not os.path.exists(p):
            return False, None, "weapon %2d: no screen  (%s)" % (w, last)
        f = cocofield.field2(open(p, "rb").read(), "PEEK $0E38 01" in r.stdout)
    else:
        ps = [os.path.join(sim, "b%d.bin" % k) for k in range(6)]
        if not all(os.path.exists(p) for p in ps):
            return False, None, "weapon %2d: no screen  (%s)" % (w, last)
        f = cocofield.field3([open(p, "rb").read() for p in ps])
    ref = open(os.path.join(d, "shot1.px"), "rb").read()
    W, H = sh["w"], sh["h"]
    nd = nf = 0
    theirs, diff = [], []
    for y in range(H):
        tr, dr = [], []
        for x in range(W):
            v = f[y][x]
            rv = ref[y * W + x]
            rd = 2 if rv == 1 else 3 if rv == 2 else 1      # refview: 0 sky, 1 dirt, 2 fire
            tr.append(rd)
            if v == 0:
                dr.append(0)
                continue
            if (v == 2) != (rd == 2):
                nd += 1
            elif (v == 3) != (rd == 3):
                nf += 1
            dr.append(4 if v != rd else v)
        theirs.append(tr)
        diff.append(dr)
    cocofield.picture(os.path.join(d, "coco.png"), f)
    cocofield.picture(os.path.join(d, "ref.png"), theirs)
    cocofield.picture(os.path.join(d, "diff.png"), diff)
    return nd == 0 and nf == 0, lag, "weapon %2d: %4d dirt, %4d fire pixels differ, %s frames behind  (%s)" % (
        w, nd, nf, "?" if lag is None else "%3d" % lag, last)


def main():
    args = sys.argv[1:]
    if not args or args[0] not in SHAPES:
        sys.exit(__doc__)
    m, args = args[0], args[1:]
    gate = "-gate" in args
    flat = "-flat" in args
    args = [a for a in args if a not in ("-gate", "-flat")]
    seed, extra, jobs = "5", [], 8
    while args[0] in ("-seed", "-aim", "-shield", "-j"):
        if args[0] == "-seed":
            seed, args = args[1], args[2:]
        elif args[0] == "-j":
            jobs, args = int(args[1]), args[2:]
        elif args[0] == "-shield":
            extra, args = extra + ["-shield", args[1]], args[2:]
        else:
            extra, args = extra + ["-angle", args[1], "-power", args[2]], args[3:]
    out = os.path.abspath(args[0])
    weapons = [int(a) for a in args[1:]] or list(range(33))
    runs = [("hilly", []), ("flat", ["-flat"]), ("shield", ["-shield", str(I_SHIELD)])] if gate else \
        [("flat" if flat else "run", ["-flat"] if flat else [])]
    fix = os.path.join(TOP, SHAPES[m]["fix"])
    keep = fix + ".keep"
    shutil.copy(fix, keep)
    todo = []
    try:
        for name, more in runs:
            for w in weapons:
                d = os.path.join(out, name, str(w))
                frames, last = prepare(m, d, w, seed, extra + more)
                if frames is None and "build failed" in last:
                    print("%s weapon %2d: %s" % (name, w, last), flush=True)
                    continue
                todo.append((name, w, d, frames, last))
    finally:
        shutil.move(keep, fix)
        os.utime(fix, None)
        build()
    fails, lags = len(runs) * len(weapons) - len(todo), []
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        futs = [(name, ex.submit(play, m, d, w, frames, last)) for name, w, d, frames, last in todo]
        cur = None
        for name, fu in futs:
            ok, lag, line = fu.result()
            if gate and name != cur:
                print("== " + name, flush=True)
                cur = name
            print(line, flush=True)
            fails += not ok
            if lag is not None:
                lags.append(lag)
    if lags:
        print("%s: %d runs, %d failed; lag: worst %d, mean %.1f frames" % (
            m, len(runs) * len(weapons), fails, max(lags), sum(lags) / len(lags)))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
