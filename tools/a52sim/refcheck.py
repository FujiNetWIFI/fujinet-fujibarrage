#!/usr/bin/env python3
"""refcheck.py -- the 5200 plays a weapon's shot as the reference does
(tools/a2sim/refcheck.py's, for the 5200).

usage: tools/a52sim/refcheck.py [-gate] [-flat] [-seed N] [-shield I] [-aim ANGLE POWER] [-j N] OUTDIR [WEAPON...]

For each weapon (proto I_* number): refview makes a shot with it (the
Atari's shape, 160x168 at 60 Hz, with the 5200's 1024-byte buffer) and
writes it as src/atari5200/test/fixture.h; a TEST=1 build plays it in MAME
(a52sim.py: scene 1, stopped after the shot); the field ($1010, 2 bits a
pixel, 40 bytes a line) is compared with refview's after the shot: dirt
against dirt, fire against fire (tank pixels left out: the reference has
none). Prints the pixels that differ (0 is the goal) and how many frames
the shot ran behind its script (test.c's test_t0, test_t1), and writes
OUTDIR/<run>/W/{a52,ref,diff}.png. -gate: every weapon (0-32), hilly, flat
and with shields (I_SHIELD on the even seats): 99 runs. The images are
built one after another (each its own fixture); MAME runs -j at once (4).
Leaves the fixture as it was and r2r/atari5200/barrage.a52 the playable
build again.
"""
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
FIX = os.path.join(TOP, "src/atari5200/test/fixture.h")
FIELD, W, H = 0x1010, 160, 168
I_SHIELD = 42


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


def build(test):
    cmd = ["make", "atari5200"] + (["TEST=1"] if test else [])
    return subprocess.run(cmd, cwd=TOP, capture_output=True, text=True)


def picture(path, rows, scale=3):
    """1 sky, 2 dirt, 3 fire, 4 a difference, 0 a tank"""
    from PIL import Image
    pal = {0: (16, 16, 16), 1: (20, 140, 255), 2: (230, 110, 0), 3: (255, 255, 255), 4: (255, 0, 255)}
    im = Image.new("RGB", (W, H))
    im.putdata([pal[v] for r in rows for v in r])
    im.resize((W * 2 * scale, H * scale), Image.NEAREST).save(path)


def prepare(d, w, seed, extra):
    """refview's shot, and the TEST=1 image that plays it (copied to d)"""
    os.makedirs(d, exist_ok=True)
    cmd = ["go", "run", "./cmd/refview", "-buf", "1024", "-shots", "1", "-seed", seed,
           "-weapon", str(w), "-fixture-all", FIX, "-out", d] + extra
    log = run(cmd, cwd=os.path.join(TOP, "server")).stdout.strip()
    last = log.splitlines()[-1] if log else ""
    b = build(True)
    if b.returncode:
        err = [l for l in (b.stdout + b.stderr).splitlines() if "rror" in l or "overflow" in l]
        return None, "the TEST=1 build failed: %s" % err[:2], last, log
    shutil.copy(os.path.join(TOP, "r2r/atari5200/barrage.a52"), os.path.join(d, "barrage.a52"))
    shutil.copy(os.path.join(TOP, "build/a52/game.map"), os.path.join(d, "game.map"))
    return d, None, last, log


def play(d):
    scn = os.path.join(d, "scene.txt")
    with open(scn, "w") as f:
        f.write("test 1 2\nwait 3000\npeek test_t0 4\ndump %X %X field.bin\n" % (FIELD, FIELD + 40 * H - 1))
    sim = os.path.join(d, "sim")
    return run(["python3", os.path.join(HERE, "a52sim.py"), os.path.join(d, "barrage.a52"),
                os.path.join(d, "game.map"), scn, sim]).stdout


def judge(d, w, r, last, log):
    sim = os.path.join(d, "sim")
    m = re.search(r"PEEK test_t0 (\w\w) (\w\w) (\w\w) (\w\w)", r)
    if not m or not os.path.exists(os.path.join(sim, "field.bin")):
        print("weapon %2d: no result from a52sim  (%s)" % (w, last))
        return False, None
    b0, b1, b2, b3 = (int(x, 16) for x in m.groups())
    t0, t1 = b0 | b1 << 8, b2 | b3 << 8
    if t1 == 0 and t0:
        print("weapon %2d: the shot never ended  (%s)" % (w, last))
        return False, None
    sf = re.search(r"(\d+) frames", log)
    lag = ((t1 - t0) & 0xFFFF) - int(sf.group(1)) if sf else None
    mem = open(os.path.join(sim, "field.bin"), "rb").read()
    ref = open(os.path.join(d, "shot1.px"), "rb").read()
    nd = nf = 0
    mine, theirs, diff = [], [], []
    for y in range(H):
        mr, tr, dr = [], [], []
        for x in range(W):
            b = mem[y * 40 + x // 4]
            v = (b >> (6 - 2 * (x & 3))) & 3         # 0 tank, 1 sky, 2 dirt, 3 fire
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
    picture(os.path.join(d, "a52.png"), mine)
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
    seed, extra, jobs = "5", [], 4
    while args[0] in ("-seed", "-aim", "-shield", "-j"):
        if args[0] == "-seed":
            seed, args = args[1], args[2:]
        elif args[0] == "-shield":
            extra, args = extra + ["-shield", args[1]], args[2:]
        elif args[0] == "-j":
            jobs, args = int(args[1]), args[2:]
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
        for name, more in runs:
            if gate:
                print("== " + name, flush=True)
            todo = []
            for w in weapons:
                d, err, last, log = prepare(os.path.join(out, name, str(w)), w, seed, extra + more)
                if err:
                    print("weapon %2d: %s  (%s)" % (w, err, last))
                    fails += 1
                else:
                    todo.append((d, w, last, log))
            with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
                results = list(ex.map(lambda t: play(t[0]), todo))
            for (d, w, last, log), r in zip(todo, results):
                ok, lag = judge(d, w, r, last, log)
                fails += not ok
                if lag is not None:
                    lags.append(lag)
    finally:
        shutil.move(keep, FIX)
        os.utime(FIX, None)
        build(False)
    if lags:
        worst = max(lags)
        print("%d runs, %d failed; lag: worst %d, mean %.1f frames" % (
            len(runs) * len(weapons), fails, worst, sum(lags) / len(lags)))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
