#!/usr/bin/env python3
"""refcheck.py -- the Atari plays a weapon's shot as the reference does.

usage: tools/atarisim/refcheck.py [-flat] [-seed N] [-aim ANGLE POWER] OUTDIR WEAPON...

For each weapon (proto I_* number): refview makes a shot with it (Atari
shape, 1536-byte buffer) and writes it as src/test/fixture.h; a TEST=1
build plays it in asim (scene 1, stopped after the shot); then the Atari's
field ($A010, 2 bits a pixel) is compared with refview's after the shot,
dirt against dirt. Prints the pixels that differ (0 is the goal).
Leaves src/test/fixture.h as `make fixture` makes it, and r2r/atari/barrage.atr
the playable build again.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
FIELD, W, H = 0xA010, 160, 168


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


def atari_px(mem):
    """the field's pixels: 0 tank, 1 sky, 2 dirt, 3 fire/text"""
    out = []
    for y in range(H):
        for x in range(W):
            b = mem[FIELD + y * 40 + x // 4]
            out.append((b >> (6 - 2 * (x % 4))) & 3)
    return out


def main():
    args = sys.argv[1:]
    flat = "-flat" in args
    args = [a for a in args if a != "-flat"]
    seed, aim = "5", []
    while args[0] in ("-seed", "-aim"):
        if args[0] == "-seed":
            seed, args = args[1], args[2:]
        else:
            aim, args = ["-angle", args[1], "-power", args[2]], args[3:]
    out, weapons = os.path.abspath(args[0]), [int(a) for a in args[1:]]
    os.makedirs(out, exist_ok=True)
    fails = 0
    # make doesn't see TEST=1 change the flags: a clean build of it
    subprocess.run(["rm", "-rf", os.path.join(TOP, "build/barrage")], check=True)
    for w in weapons:
        ref = os.path.join(out, f"ref{w}")
        cmd = ["go", "run", "./cmd/refview", "-shots", "1", "-seed", seed, "-weapon", str(w),
               "-buf", "1536", "-fixture", "../src/test/fixture.h", "-out", ref]
        if flat:
            cmd.insert(3, "-flat")
        cmd[3:3] = aim
        log = run(cmd, cwd=os.path.join(TOP, "server")).stdout.strip()
        b = subprocess.run(["defoogi", "make", "atari", "TEST=1"], cwd=TOP, capture_output=True, text=True)
        if b.returncode:
            fails += 1
            err = [l for l in (b.stdout + b.stderr).splitlines() if "rror" in l or "overflow" in l]
            print(f"weapon {w:2d}: the TEST=1 build failed: {err[:2]}   ({log})")
            continue
        scen = os.path.join(out, f"scen{w}.txt")
        with open(scen, "w") as f:
            f.write("test 1 2\nwait 2600\nsnap final\n")
        sim = os.path.join(out, f"sim{w}")
        run(["python3", os.path.join(HERE, "atarisim.py"), os.path.join(TOP, "r2r/atari/barrage.atr"),
             os.path.join(TOP, "build/barrage-atari.map"), scen, sim])
        mem = open(os.path.join(sim, "final.mem"), "rb").read()
        px = open(os.path.join(ref, "shot1.px"), "rb").read()
        mine = atari_px(mem)
        # tanks and text are the Atari's own drawing: only air and dirt count
        diff = [i for i in range(W * H) if mine[i] in (1, 2) and (mine[i] == 2) != (px[i] == 1)]
        fails += len(diff) > 0
        where = " ".join(f"({i % W},{i // W})" for i in diff[:4])
        print(f"weapon {w:2d}: {len(diff):5d} pixels differ {where}  ({log})")
    run(["make", "fixture"], cwd=TOP)
    # the TEST=1 objects share the build directory: rebuild the playable disk
    subprocess.run(["rm", "-rf", os.path.join(TOP, "build/barrage")], check=True)
    run(["defoogi", "make", "atari"], cwd=TOP)
    sys.exit(1 if fails else 0)


main()
