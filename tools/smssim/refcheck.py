#!/usr/bin/env python3
"""refcheck.py -- the SMS's player against the server's reference view (the
Adam's tools/adamsim/refcheck.py on smsrun: one TEST=1 build serves every
run; the SMS's field is the Adam's shape, so its fixtures are the Adam's).

usage: tools/smssim/refcheck.py [-gate] [-flat] [-seed N] [-shield I]
           [-aim ANGLE POWER] [-j N] OUTDIR [WEAPON...]

For each weapon (proto I_* number): refview makes a shot with it
(tools/adamsim/fixture.py), smsrun loads the fixture into the TEST=1 build
(make sms TEST=1, as it is), plays scene 1 with arg 2 (stop after the shot)
serving the shot's segments, and the field store (the cart's RAM, half 0:
src/tms9918/fld.asm's layout from 8000h) is compared with refview's field
after the shot, pixel for pixel: dirt and fire. The lag: the frames the
shot took to play less the script's own. OUTDIR/<run>/W/{sms,ref,diff}.png
and screen.ppm (the picture: vramcheck.py checks it).
-gate: every weapon (0-32), hilly, flat and with shields (I_SHIELD on the
even seats): 99 runs. Exits non-zero if any pixel differs.
"""
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(TOP, "tools", "adamsim"))
import adamfield  # noqa: E402
import fixture  # noqa: E402

W, H = 256, 168
I_SHIELD = 42
SMSRUN = os.path.join(HERE, "smsrun")
ROM = os.path.join(TOP, "r2r", "sms", "barrage.sms")
MAP = os.path.join(TOP, "build", "sms", "barrage.map")


def field(half0):
    """the store (two planes from the half's start, row r's D at page 2r, F
    the page after) as [y][x] values: 1 sky, 2 dirt, 3 fire"""
    f = []
    for y in range(H):
        row = []
        base = (y >> 3) * 512 + (y & 7)
        for x in range(W):
            a = base + (x & 0xF8)
            bit = 0x80 >> (x & 7)
            row.append(3 if half0[a + 256] & bit else 2 if half0[a] & bit else 1)
        f.append(row)
    return f


def one(d, w, seed, extra):
    os.makedirs(d, exist_ok=True)
    frames = fixture.make(d, ["-seed", seed, "-weapon", str(w)] + extra)
    scen = os.path.join(d, "scene")
    open(scen, "w").write(
        "until _test_hook+1 0xC3 900\nload _fix_ram %s\nserve _test_hook %s\npoke _test_hook+2 1 2\n"
        "poke _test_hook 0x5A\nuntil _test_hook 0xDD 30000\nwait 2\npeekw _test_t1\nwait 120\ncartram store.bin 0\n"
        "vram vram.bin\ncartram tc.bin 1\ndump ram.bin 0xC000 8192\nshot screen.ppm\n" % (os.path.join(d, "fix.bin"), d))
    r = subprocess.run([SMSRUN, ROM, MAP, scen, d], capture_output=True, text=True)
    if r.returncode:
        return False, None, "weapon %2d: the run failed: %s" % (w, (r.stderr or r.stdout).strip()[-300:])
    played = None
    for line in r.stdout.splitlines():
        if line.startswith("_test_t1:"):
            played = int(line.split()[1])
    f = field(open(os.path.join(d, "store.bin"), "rb").read())
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
    adamfield.picture(os.path.join(d, "sms.png"), f)
    adamfield.picture(os.path.join(d, "ref.png"), theirs)
    adamfield.picture(os.path.join(d, "diff.png"), diff)
    lag = None if played is None else played - frames
    vc = subprocess.run([sys.executable, os.path.join(HERE, "vramcheck.py"), d, MAP], capture_output=True, text=True)
    vline = vc.stdout.strip().splitlines()[-1] if vc.stdout.strip() else vc.stderr.strip()
    return nd == 0 and nf == 0 and vc.returncode == 0, lag, \
        "weapon %2d: %4d dirt, %4d fire pixels differ, %s frames behind (%d); screen: %s" % (
            w, nd, nf, "?" if lag is None else "%3d" % lag, frames, vline)


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
