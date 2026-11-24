#!/usr/bin/env python3
"""stepcheck.py -- the Arcadia's field against the reference's, step by step
(refcheck.py's shot, played in lockstep: test scene 3 with argument 1, a
step a pass, the pass's end held a frame and counted in STEPC).

usage: tools/arc/stepcheck.py [-flat] [-seed N] [-shield I] [-every K] [-show STEP] OUTDIR WEAPON

arcdump (refview's set-up, every frame of the shot at the Arcadia's caps)
gives the reference's field and tanks after each step; the Arcadia's cells
after the same step are peeked and compared with mock_arc.py's Mock('B',
'night') of it. Prints each sampled step's differing cells (the first
ones in full). Transients are expected where the Arcadia draws a growing
blast as rings (view.go redraws it whole each frame): the end must agree.
"""
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import mock_arc as M  # noqa: E402

FIX = os.path.join(TOP, "src/arcadia/test/fixture.inc")
CAPS = ["-w", "48", "-h", "48", "-par", "78", "-buf", "160", "-tankw", "2", "-tankh", "1", "-plat", "11"]


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


def main():
    args = sys.argv[1:]
    flat = "-flat" in args
    args = [a for a in args if a != "-flat"]
    seed, extra, every, show = "5", [], 1, -1
    while args[0] in ("-seed", "-shield", "-every", "-show"):
        if args[0] == "-seed":
            seed = args[1]
        elif args[0] == "-show":
            show = int(args[1])
        elif args[0] == "-shield":
            extra += ["-shield", args[1]]
        else:
            every = int(args[1])
        args = args[2:]
    out, w = os.path.abspath(args[0]), int(args[1])
    os.makedirs(out, exist_ok=True)
    srv = os.path.join(TOP, "server")
    rv, ad = os.path.join(out, "refview"), os.path.join(out, "arcdump")
    run(["go", "build", "-o", rv, "./cmd/refview"], cwd=srv)
    run(["go", "build", "-o", ad, "./cmd/arcdump"], cwd=srv)
    fl = ["-flat"] if flat else []
    keep = FIX + ".keep"
    shutil.copy(FIX, keep)
    try:
        log = run([rv] + CAPS + ["-shots", "1", "-seed", seed, "-weapon", str(w), "-fixture-asm", FIX, "-out", out,
                                 "-font", os.path.join(TOP, "gfx/font3x5.txt")] + extra + fl).stdout
        run([ad] + CAPS[:-2] + ["-plat", "11", "-seed", seed, "-weapon", str(w), "-out", out] + extra + fl)
        frames = M.frames(os.path.join(out, "shot1.dmp"))
        nsteps = len(frames) - 2
        run(["bash", "src/arcadia/build.sh"], cwd=TOP)
        steps = list(range(every, nsteps + 1, every))
        if nsteps not in steps:
            steps.append(nsteps)
        scn = os.path.join(out, "scene.txt")
        with open(scn, "w") as f:
            f.write("test 3 1\nuntil TMARK 98 3000\n")
            for k in steps:
                f.write("until STEPC %d 20000\npeek SCR 208\npeek SCR2 176\n" % (k & 255))
        r = run(["python3", os.path.join(TOP, "tools/arcsim/arcsim.py"), scn, out], cwd=TOP).stdout
    finally:
        shutil.move(keep, FIX)
        os.utime(FIX, None)
    pk = re.findall(r"PEEK ([0-9A-F]{4}): ((?:[0-9A-F]{2} ?)+)", r)
    mk = M.Mock("B", "night")
    M.OWN = 0
    shown = 0
    bad = 0
    for i, k in enumerate(steps):
        if 2 * i + 1 >= len(pk):
            print("step %d: no sample (%s)" % (k, "timeout" if "timeout" in r else "?"))
            break
        mine = bytes.fromhex(pk[2 * i][1].replace(" ", "")) + bytes.fromhex(pk[2 * i + 1][1].replace(" ", ""))
        u, c, wr, theirs = mk.build(frames[k], k)
        d = [(j // 16, j % 16, mine[j], theirs[j]) for j in range(384) if mine[j] != theirs[j]]
        if d:
            bad += 1
            if shown < 6 or k == show:
                shown += 1
                print("step %3d: %d cells: %s" % (k, len(d), " ".join("r%dc%d %02X/%02X" % x for x in d[:12])))
            else:
                print("step %3d: %d cells" % (k, len(d)))
    print("%s; %d of %d sampled steps differ" % (log.strip().splitlines()[-1], bad, len(steps)))


if __name__ == "__main__":
    main()
