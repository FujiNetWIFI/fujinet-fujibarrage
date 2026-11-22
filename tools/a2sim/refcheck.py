#!/usr/bin/env python3
"""refcheck.py -- the Apple II plays a weapon's shot as the reference does
(tools/astrosim/refcheck.py's, for the Apple II).

usage: tools/a2sim/refcheck.py [-gate] [-flat] [-seed N] [-shield I] [-aim ANGLE POWER] OUTDIR [WEAPON...]

For each weapon (proto I_* number): refview makes a shot with it (the
Apple's shape: 140x160 at 60 Hz, PAR 117, a 1536-byte buffer) and writes
it as src/apple2/test/fixture.h; a TEST=1 build plays it in a2sim (scene
1, stopped after the shot); HGR page 1 is decoded (tools/a2sim/hgrfield.py)
and compared with refview's field after the shot: dirt against dirt, fire
against fire (tank pixels left out: the reference has none). Prints the
pixels that differ (0 is the goal) and how many frames the shot ran behind
the script (the player's lag: test.c's test_t0, test_t1), and writes
OUTDIR/<run>/W/{a2,ref,diff}.png. -gate: every weapon (0-32), hilly,
flat and with shields (I_SHIELD on the even seats): 99 runs. Leaves the
fixture as it was and r2r/apple2/barrage.po the playable build again.
"""
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import hgrfield  # noqa: E402

W, H = hgrfield.W, hgrfield.H
FIX = os.path.join(TOP, "src/apple2/test/fixture.h")
I_SHIELD = 42


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


def picture(path, rows, scale=3):
    """1 sky, 2 dirt, 3 fire, 4 a difference, 0 a tank"""
    from PIL import Image
    pal = {0: (16, 16, 16), 1: (20, 140, 255), 2: (230, 110, 0), 3: (255, 255, 255), 4: (255, 0, 255)}
    im = Image.new("RGB", (W, H))
    im.putdata([pal[v] for r in rows for v in r])
    im.resize((W * 2 * scale // 2 * 2, round(H * 2.1875 * scale)), Image.NEAREST).save(path)


def build(test):
    subprocess.run(["rm", "-rf", os.path.join(TOP, "build/barrage/apple2")], check=True)
    cmd = ["defoogi", "make", "apple2"] + (["TEST=1"] if test else [])
    return subprocess.run(cmd, cwd=TOP, capture_output=True, text=True)


def one(out, w, seed, extra):
    d = os.path.join(out, str(w))
    os.makedirs(d, exist_ok=True)
    cmd = ["go", "run", "./cmd/refview", "-w", str(W), "-h", str(H), "-par", "117", "-buf", "1536",
           "-shots", "1", "-seed", seed, "-weapon", str(w), "-fixture", FIX, "-out", d] + extra
    log = run(cmd, cwd=os.path.join(TOP, "server")).stdout.strip()
    last = log.splitlines()[-1] if log else ""
    # only test.c changes: an incremental TEST=1 build
    b = subprocess.run(["defoogi", "make", "apple2", "TEST=1"], cwd=TOP, capture_output=True, text=True)
    if b.returncode:
        err = [l for l in (b.stdout + b.stderr).splitlines() if "rror" in l or "overflow" in l]
        print("weapon %2d: the TEST=1 build failed: %s  (%s)" % (w, err[:2], last))
        return False, None
    scn = os.path.join(d, "scene.txt")
    with open(scn, "w") as f:
        f.write("test 1 2\nwait 6000\npeek test_t0 4\ndump hgr.bin $2000 8192\n")
    sim = os.path.join(d, "sim")
    r = run(["python3", os.path.join(HERE, "a2sim.py"), os.path.join(TOP, "r2r/apple2/barrage.po"),
             os.path.join(TOP, "build/barrage-apple2.map"), scn, sim]).stdout
    m = re.search(r"PEEK test_t0 (\w\w) (\w\w) (\w\w) (\w\w)", r)
    if not m or not os.path.exists(os.path.join(sim, "hgr.bin")):
        print("weapon %2d: no result from a2sim  (%s)" % (w, last))
        return False, None
    b0, b1, b2, b3 = (int(x, 16) for x in m.groups())
    t0, t1 = b0 | b1 << 8, b2 | b3 << 8
    sf = re.search(r"(\d+) frames", log)
    if t1 == 0 and t0:
        print("weapon %2d: the shot never ended  (%s)" % (w, last))
        return False, None
    lag = ((t1 - t0) & 0xFFFF) - int(sf.group(1)) if sf else None
    data = open(os.path.join(sim, "hgr.bin"), "rb").read()
    a = hgrfield.field(data)
    ref = open(os.path.join(d, "shot1.px"), "rb").read()
    nd = nf = 0
    mine, theirs, diff = [], [], []
    for y in range(H):
        mr, tr, dr = [], [], []
        for x in range(W):
            v = a[y][x]
            rv = ref[y * W + x]
            rd = 2 if rv == 1 else 3 if rv == 2 else 1      # refview: 0 sky, 1 dirt, 2 fire
            mr.append(v)
            tr.append(rd)
            if v == 0:
                dr.append(0)
                continue
            if (v == 2) != (rd == 2):
                nd += 1
            elif (v == 3) != (rd == 3):
                nf += 1
            dr.append(4 if v != rd else v)
        mine.append(mr)
        theirs.append(tr)
        diff.append(dr)
    picture(os.path.join(d, "a2.png"), mine)
    picture(os.path.join(d, "ref.png"), theirs)
    picture(os.path.join(d, "diff.png"), diff)
    print("weapon %2d: %4d dirt, %4d fire pixels differ, %s frames behind  (%s)"
          % (w, nd, nf, "?" if lag is None else "%3d" % lag, last), flush=True)
    return nd == 0 and nf == 0, lag


def main():
    args = sys.argv[1:]
    gate = "-gate" in args
    flat = "-flat" in args
    args = [a for a in args if a not in ("-gate", "-flat")]
    seed, extra = "5", []
    while args[0] in ("-seed", "-aim", "-shield"):
        if args[0] == "-seed":
            seed, args = args[1], args[2:]
        elif args[0] == "-shield":
            extra, args = extra + ["-shield", args[1]], args[2:]
        else:
            extra, args = extra + ["-angle", args[1], "-power", args[2]], args[3:]
    out = os.path.abspath(args[0])
    weapons = [int(a) for a in args[1:]] or list(range(33))
    runs = [("hilly", []), ("flat", ["-flat"]), ("shield", ["-shield", str(I_SHIELD)])] if gate else \
        [("flat" if flat else "run", ["-flat"] if flat else [])]
    keep = FIX + ".keep"
    shutil.copy(FIX, keep)
    fails, lags = 0, []
    try:
        b = build(True)                 # (make doesn't see TEST=1 change the flags)
        if b.returncode:
            print(b.stdout[-2000:], b.stderr[-2000:])
            raise SystemExit("the TEST=1 build failed")
        for name, more in runs:
            if gate:
                print("== " + name, flush=True)
            for w in weapons:
                ok, lag = one(os.path.join(out, name), w, seed, extra + more)
                fails += not ok
                if lag is not None:
                    lags.append(lag)
    finally:
        shutil.move(keep, FIX)
        os.utime(FIX, None)
        build(False)
    if lags:
        print("%d runs, %d failed; lag: worst %d, mean %.1f frames" % (
            len(runs) * len(weapons), fails, max(lags), sum(lags) / len(lags)))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
