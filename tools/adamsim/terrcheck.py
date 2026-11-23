#!/usr/bin/env python3
"""terrcheck.py -- the Adam's field store against the terrain it was given
(tools/cocosim/terrcheck.py's --fixture, on adamrun: no rebuild).

    terrcheck.py [-seed N] [-shots N] OUTDIR
    terrcheck.py --live TAPLOG RAM.bin [OUT.png]

refview plays N shots (4) on the field of seed N (5) at the Adam's shape;
the field after them, with tunnels and islands carved (TERRAIN entries
with spans), becomes the fixture's terrain. The TEST=1 build (make adam
TEST=1, as it is) plays scene 5, and every pixel of the store must be the
field's: sky and dirt compared, fire (the turn's arrow) counted. OUTDIR:
adam.png, ref.png, diff.png (red: a difference), the run's files.

--live: the TERRAIN (G) frames of tools/rigs/tapproxy.py's TAPLOG replayed
(the CoCo's tap_terrain) against a RAM dump of the live rig (rig.sh's
OUTDIR/ram.bin), taken while the field was the last terrain's (no shot
since). Sky and dirt compared, fire counted.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(TOP, "tools", "cocosim"))
import adamfield  # noqa: E402
import fixture  # noqa: E402
from terrcheck import carve, encode, tap_terrain  # noqa: E402  (the CoCo's)

W, H = 256, 168
ADAMRUN = os.path.join(HERE, "adamrun")
DDP = os.path.join(TOP, "r2r", "adam", "barrage.ddp")
MAP = os.path.join(TOP, "build", "adam", "game.map")


def live(log, ram, png):
    t = tap_terrain(log, W, H)
    if t is None:
        sys.exit("no whole terrain in " + log)
    f = adamfield.field(open(ram, "rb").read())
    nd = nfire = 0
    diff = []
    for y in range(H):
        row = []
        for x in range(W):
            v = f[y][x]
            if v == 3:
                nfire += 1
            elif v != t[y][x]:
                nd += 1
                v = 4
            row.append(v)
        diff.append(row)
    print("adam live: %d sky/dirt pixels differ (%d fire not compared)" % (nd, nfire))
    if png:
        adamfield.picture(png, diff)
    sys.exit(1 if nd else 0)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--live" and len(args) >= 3:
        live(args[1], args[2], args[3] if len(args) > 3 else None)
    seed, shots = "5", "4"
    while args and args[0].startswith("-"):
        if args[0] == "-seed":
            seed = args[1]
        elif args[0] == "-shots":
            shots = args[1]
        args = args[2:]
    if not args:
        sys.exit(__doc__)
    out = os.path.abspath(args[0])
    os.makedirs(out, exist_ok=True)
    ref = os.path.join(out, "ref4")
    subprocess.run([fixture.refview(), "-shots", shots, "-seed", seed] + fixture.SHAPE + ["-out", ref],
                   check=True, capture_output=True)
    px = open(os.path.join(ref, "shot%s.px" % shots), "rb").read()
    px = bytes(1 if v == 1 else 0 for v in px)     # (the view's: 0 sky, 1 dirt, 2 fire)
    px = carve(px, W, H)
    entries = encode(px, W, H)
    fixture.make(out, ["-seed", seed])
    d = fixture.parse(open(os.path.join(out, "fix.h")).read())
    d["fix_terrain"] = entries
    d["FIX_NSEG"] = 0
    open(os.path.join(out, "fix.bin"), "wb").write(fixture.blob(d))
    scen = os.path.join(out, "scene")
    open(scen, "w").write("until _test_hook+1 0xC3 600\nload _fix_ram %s\npoke _test_hook+2 5 0\n"
                          "poke _test_hook 0x5A\nwait 600\ndump ram.bin\nshot screen.ppm\n"
                          % os.path.join(out, "fix.bin"))
    subprocess.run([ADAMRUN, DDP, MAP, scen, out], check=True)
    f = adamfield.field(open(os.path.join(out, "ram.bin"), "rb").read())
    nd = nfire = 0
    diff = []
    for y in range(H):
        row = []
        for x in range(W):
            v, want = f[y][x], 2 if px[y * W + x] else 1
            if v == 3:
                nfire += 1
            elif v != want:
                nd += 1
                v = 4
            row.append(v)
        diff.append(row)
    adamfield.picture(os.path.join(out, "adam.png"), f)
    adamfield.picture(os.path.join(out, "ref.png"), [[2 if px[y * W + x] else 1 for x in range(W)] for y in range(H)])
    adamfield.picture(os.path.join(out, "diff.png"), diff)
    spans = sum(1 for e in entries if e > 0xF0)
    print("adam: %d sky/dirt pixels differ (%d fire not compared; %d of %d columns with spans)"
          % (nd, nfire, spans, W))
    sys.exit(1 if nd else 0)


if __name__ == "__main__":
    main()
