#!/usr/bin/env python3
"""refcheck.py -- the PC plays a weapon's shot as the reference does
(tools/cocosim/refcheck.py's, for the CGA and the VGA builds).

usage: tools/dossim/refcheck.py cga|vga [-gate] [-flat] [-seed N]
           [-shield I] [-aim ANGLE POWER] [-j N] [-nobuild] OUTDIR [WEAPON...]

One TEST=1 build serves every run: its fixture comes from files on the disk
(src/msdos/test/fixext.h). For each weapon (proto I_* number) refview makes
a shot at the PC's shape (tools/dossim/fixture.py: FIX.BIN, SEGnn.BIN);
the runs play in MAME (tools/dossim/dossim.py), -j at a time (8): scene 1,
stopped after the shot (test_hook[0] DDh); the screen is dumped, decoded
(tools/dossim/dosfield.py) and compared with refview's field after the
shot: dirt against dirt, fire against fire (tank pixels left out: the
reference has none). Prints the pixels that differ (0 is the goal) and how
many frames the shot ran behind the script (test_t1, the time played; MAME's
8088 is faster than a real one: 86Box says how a PC keeps up), and writes
OUTDIR/<run>/W/{pc,ref,diff}.png. -gate: every weapon (0-32), hilly, flat
and with shields (I_SHIELD on the even seats): 99 runs.
"""
import concurrent.futures
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import dosfield  # noqa: E402
import fixture  # noqa: E402

I_SHIELD = 42
B = os.path.join(TOP, "build", "msdos")


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def play(m, d, w, args):
    try:
        frames = fixture.make(d, args)
    except SystemExit as e:
        return False, None, "weapon %2d: refview: %s" % (w, e)
    segs = sorted(f for f in os.listdir(d) if f.startswith("SEG"))
    scn = os.path.join(d, "scene.txt")
    seg, size = ("B800", 16384) if m == "cga" else ("A000", 64000)
    with open(scn, "w") as f:
        f.write("test 1 2\nuntil test_hook 0 0xDD\nwait 30\npeek test_t1 2\n")
        f.write("dump screen.bin %s %d\n" % (seg, size))
    prog = "barrage" if m == "cga" else "barrvga"
    env = dict(os.environ, DOSSIM_NOFN="1", DOSSIM_SECONDS="400",
               DOSSIM_FILES=" ".join(os.path.join(d, x) for x in ["FIX.BIN"] + segs))
    if m == "vga":
        env.update(DOSSIM_PROG="BARRVGA", DOSSIM_MAMEARGS="-isa1 vga")
    sim = os.path.join(d, "sim")
    r = run(["python3", os.path.join(HERE, "dossim.py"), os.path.join(B, prog + ".exe"),
             os.path.join(B, prog + ".map"), scn, sim], env=env, timeout=900)
    t = re.findall(r"PEEK test_t1 (\w\w) (\w\w)", r.stdout)
    p = os.path.join(sim, "screen.bin")
    if not t or not os.path.exists(p):
        return False, None, "weapon %2d: no result from dossim" % w
    played = int(t[0][1] + t[0][0], 16)        # (little-endian)
    lag = played - frames
    data = open(p, "rb").read()
    f = dosfield.cga(data) if m == "cga" else dosfield.vga(data)
    ref = open(os.path.join(d, "ref", "shot1.px"), "rb").read()
    W, H = dosfield.W, dosfield.H
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
    dosfield.picture(os.path.join(d, "pc.png"), f)
    dosfield.picture(os.path.join(d, "ref.png"), theirs)
    dosfield.picture(os.path.join(d, "diff.png"), diff)
    return nd == 0 and nf == 0, lag, "weapon %2d: %4d dirt, %4d fire pixels differ, %3d frames behind (%d)" % (
        w, nd, nf, lag, frames)


def main():
    args = sys.argv[1:]
    if not args or args[0] not in ("cga", "vga"):
        sys.exit(__doc__)
    m, args = args[0], args[1:]
    gate = "-gate" in args
    flat = "-flat" in args
    nobuild = "-nobuild" in args
    args = [a for a in args if a not in ("-gate", "-flat", "-nobuild")]
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
    out = os.path.abspath(args[0])
    weapons = [int(a) for a in args[1:]] or list(range(33))
    runs = [("hilly", []), ("flat", ["-flat"]), ("shield", ["-shield", str(I_SHIELD)])] if gate else \
        [("flat" if flat else "run", ["-flat"] if flat else [])]
    if not nobuild:
        b = run(["make", "msdos", "TEST=1"], cwd=TOP)
        if b.returncode:
            sys.exit("build failed:\n" + (b.stdout + b.stderr)[-2000:])
    jobs_list = [(name, w, ["-seed", seed, "-weapon", str(w)] + rx + extra)
                 for name, rx in runs for w in weapons]
    ok = 0
    worst = 0
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        futs = {ex.submit(play, m, os.path.join(out, name, str(w)), w, a): (name, w) for name, w, a in jobs_list}
        res = {}
        for fu in concurrent.futures.as_completed(futs):
            res[futs[fu]] = fu.result()
    for name, w, _ in jobs_list:
        good, lag, msg = res[(name, w)]
        ok += good
        if lag is not None:
            worst = max(worst, lag)
        print("%-6s %s%s" % (name, msg, "" if good else "  <--"))
    print("%d/%d at 0 px; worst lag %d frames (MAME)" % (ok, len(jobs_list), worst))


if __name__ == "__main__":
    main()
