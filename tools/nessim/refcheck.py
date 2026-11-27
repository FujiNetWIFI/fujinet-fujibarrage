#!/usr/bin/env python3
"""refcheck.py -- the NES's player and store against the server's reference
view (tools/smssim/refcheck.py's, on nesrun: one TEST=1 build serves every
run).

usage: tools/nessim/refcheck.py [-gate] [-flat] [-seed N] [-shield I]
           [-aim ANGLE POWER] [-j N] OUTDIR [WEAPON...]

For each weapon (proto I_* number): refview makes a shot with it at the
NES's shape (tools/adamsim/fixture.py, the script buffer 1024), nesrun
loads the fixture into the TEST=1 build (make nes TEST=1), plays scene 1
with arg 2 (stop after the shot) serving the shot's segments, and the store
(tools/nessim/nesfield.py, from the WRAM) is compared with refview's field
after the shot, pixel for pixel: dirt and fire. The lag: the frames the
shot took to play less the script's own; fld_lossy (cells left whole for
want of a block) must be 0. OUTDIR/<run>/W/{nes,ref,diff}.png. -gate:
every weapon (0-32), hilly, flat and with shields (I_SHIELD on the even
seats): 99 runs. Exits non-zero if any pixel differs.
"""
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(TOP, "tools", "adamsim"))
sys.path.insert(0, HERE)
import adamfield  # noqa: E402
import fixture  # noqa: E402
import nesfield  # noqa: E402

W, H = 256, 168
FIX_RAM = 512                   # (src/nes/test/fixext.h)
I_SHIELD = 42
NESRUN = os.path.join(HERE, "nesrun")
ROM = os.path.join(TOP, "r2r", "nes", "barrage.nes")
MAP = os.path.join(TOP, "build", "nes", "barrage.map")


def one(d, w, seed, extra):
    os.makedirs(d, exist_ok=True)
    frames = fixture.make(d, ["-seed", seed, "-weapon", str(w), "-buf", "1024"] + extra)
    if os.path.getsize(os.path.join(d, "fix.bin")) > FIX_RAM:
        return False, None, "weapon %2d: the fixture is over the NES's fix_ram (%d)" % (w, FIX_RAM)
    scen = os.path.join(d, "scene")
    open(scen, "w").write(
        "until _test_hook+1 0xC3 900\nload _fix_ram %s\nserve _test_hook %s\npoke _test_hook+2 1 2\n"
        "poke _test_hook 0x5A\nuntil _test_hook 0xDD 30000\nwait 2\npeekw _test_t1\npeekw _fld_lossy\nwait 120\n"
        "dump wram.bin 0x6000 8192\nvram vram.bin\nshot screen.ppm\n" % (os.path.join(d, "fix.bin"), d))
    r = subprocess.run([NESRUN, ROM, MAP, scen, d], capture_output=True, text=True)
    if r.returncode:
        return False, None, "weapon %2d: the run failed: %s" % (w, (r.stderr or r.stdout).strip()[-300:])
    played = lossy = None
    for line in r.stdout.splitlines():
        if line.startswith("_test_t1:"):
            played = int(line.split()[1])
        if line.startswith("_fld_lossy:"):
            lossy = int(line.split()[1])
    f = nesfield.field(open(os.path.join(d, "wram.bin"), "rb").read(), SYMS)
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
    adamfield.picture(os.path.join(d, "nes.png"), f)
    adamfield.picture(os.path.join(d, "ref.png"), theirs)
    adamfield.picture(os.path.join(d, "diff.png"), diff)
    lag = None if played is None else played - frames
    return nd == 0 and nf == 0 and lossy == 0, lag, \
        "weapon %2d: %4d dirt, %4d fire pixels differ, %s frames behind (%d); lossy %s" % (
            w, nd, nf, "?" if lag is None else "%3d" % lag, frames, lossy)


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
    global SYMS
    SYMS = nesfield.symbols(MAP)
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
