#!/usr/bin/env python3
"""firecheck.py -- does the Adam show its fire while a shot plays? Every
frame from the script's start, the store's fire (the F plane at A800h, a
page after each row's D) against the fire in the VRAM (field lines drawn as
fire: their pattern is F, their colour neither of the line's sky/dirt
styles), pixel for pixel. The SMS's is tools/smssim/firecheck.py.

usage: [FC_DDP=IMAGE FC_MAP=MAP] tools/adamsim/firecheck.py [-flat] [-seed N] [-j N] [-keep] [-ideal DIR] OUTDIR [WEAPON...]

For each weapon (default 0 1 2 3 7: the missiles, the nukes, MEGA MIRV)
refview makes the shot (fixture.py), adamrun plays it in the TEST=1 build
(make adam TEST=1) and dumps the store and VRAM each frame. Prints per
run: the frames with fire in the store, the share of the store's fire
pixels on the screen the same frame (shown), and the frames on which at
least half of it was. -keep keeps the dumps. -ideal DIR: also the
screen's fire against the store's in an earlier -keep run (DIR/W/cap: the
blasts drawn every frame), frame for frame.
"""
import os
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import fixture  # noqa: E402

ADAMRUN = os.path.join(HERE, "adamrun")
DDP = os.environ.get("FC_DDP") or os.path.join(TOP, "r2r", "adam", "barrage.ddp")
MAP = os.environ.get("FC_MAP") or os.path.join(TOP, "build", "adam", "game.map")
SH, SH_LEN = 0xA700, 0x2A00
L_SKYC, L_DIRTC, L_MIXC, L_SIZE = 0, 2, 4, 5


def store_fire(sh):
    """the F plane as a set of (x, y)"""
    out = set()
    for r in range(21):
        for c in range(32):
            for yy in range(8):
                b = sh[r * 512 + 256 + c * 8 + yy]
                if b:
                    for xx in range(8):
                        if b & (0x80 >> xx):
                            out.add((c * 8 + xx, r * 8 + yy))
    return out


def screen_fire(vram, line):
    """the pattern bits of the field's lines drawn as fire, as a set of (x, y)"""
    out = set()
    for r in range(21):
        for c in range(32):
            for yy in range(8):
                y = r * 8 + yy
                at = r * 256 + c * 8 + yy
                p, col = vram[at], vram[0x2000 + at]
                st = line[y * L_SIZE:y * L_SIZE + L_SIZE]
                if not p or col in (st[L_SKYC], st[L_DIRTC], st[L_MIXC]):
                    continue
                for xx in range(8):
                    if p & (0x80 >> xx):
                        out.add((c * 8 + xx, y))
    return out


def one(d, w, seed, extra, keep, ideal):
    os.makedirs(d, exist_ok=True)
    frames = fixture.make(d, ["-seed", seed, "-weapon", str(w)] + extra)
    n = frames + 240
    cap = os.path.join(d, "cap")
    os.makedirs(cap, exist_ok=True)
    lines = ["until _test_hook+1 0xC3 900", "load _fix_ram %s" % os.path.join(d, "fix.bin"),
             "serve _test_hook %s" % d, "poke _test_hook+2 1 2", "poke _test_hook 0x5A",
             "until _in_script 1 2000", "dump cap/line _st_line %d" % (168 * L_SIZE)]
    for i in range(n):
        lines += ["dump cap/s%04d 0x%X 0x%X" % (i, SH, SH_LEN), "vram cap/v%04d" % i, "wait 1"]
    lines += ["until _test_hook 0xDD 30000", "wait 2", "peekw _test_t1"]
    scen = os.path.join(d, "fscene")
    open(scen, "w").write("\n".join(lines) + "\n")
    r = subprocess.run([ADAMRUN, DDP, MAP, scen, d], capture_output=True, text=True)
    if r.returncode:
        return "weapon %2d: the run failed: %s" % (w, (r.stderr or r.stdout).strip()[-300:])
    played = None
    for ln in r.stdout.splitlines():
        m = re.match(r"_test_t1:?\s+(\S+)", ln)
        if m:
            played = int(m.group(1), 0)
    line = open(os.path.join(cap, "line"), "rb").read()
    fire_frames = shown_px = store_px = half = 0
    i_px = i_shown = i_frames = i_half = 0
    first = last = None
    for i in range(n):
        sf = store_fire(open(os.path.join(cap, "s%04d" % i), "rb").read())
        vf = None
        if ideal:
            ip = os.path.join(ideal, str(w), "cap", "s%04d" % i)
            isf = store_fire(open(ip, "rb").read()) if os.path.exists(ip) else set()
            if len(isf) > 3:            # (a blast, not a shell's head)
                vf = screen_fire(open(os.path.join(cap, "v%04d" % i), "rb").read(), line)
                i_frames += 1
                i_px += len(isf)
                i_shown += len(vf & isf)
                i_half += len(vf & isf) * 2 >= len(isf)
        if not sf:
            continue
        if vf is None:
            vf = screen_fire(open(os.path.join(cap, "v%04d" % i), "rb").read(), line)
        both = len(sf & vf)
        fire_frames += 1
        store_px += len(sf)
        shown_px += both
        half += both * 2 >= len(sf)
        first = i if first is None else first
        last = i
    if not keep:
        shutil.rmtree(cap)
    tail = ""
    if i_frames:
        tail = "; vs the ideal: %5.1f%% of its fire on the screen, half or more on %3d of %3d frames" % (
            100.0 * i_shown / i_px, i_half, i_frames)
    if not fire_frames:
        return "weapon %2d: no fire in the store (%d frames looked at)" % (w, n)
    return "weapon %2d: fire in the store on %3d frames (%d-%d); shown %5.1f%% of its pixels; " \
           "half or more shown on %3d frames (%4.1f%%); %s frames behind" % (
               w, fire_frames, first, last, 100.0 * shown_px / store_px, half, 100.0 * half / fire_frames,
               "?" if played is None else played - frames) + tail


def main():
    args = sys.argv[1:]
    flat = "-flat" in args
    keep = "-keep" in args
    args = [a for a in args if a not in ("-flat", "-keep")]
    seed, jobs, ideal = "5", 6, None
    while args and args[0] in ("-seed", "-j", "-ideal"):
        if args[0] == "-seed":
            seed, args = args[1], args[2:]
        elif args[0] == "-ideal":
            ideal, args = os.path.abspath(args[1]), args[2:]
        else:
            jobs, args = int(args[1]), args[2:]
    if not args:
        sys.exit(__doc__)
    out = os.path.abspath(args[0])
    weapons = [int(a) for a in args[1:]] or [0, 1, 2, 3, 7]
    extra = ["-flat"] if flat else []
    fixture.refview()
    with ThreadPoolExecutor(jobs) as ex:
        res = list(ex.map(lambda w: one(os.path.join(out, str(w)), w, seed, extra, keep, ideal), weapons))
    for ln in res:
        print(ln)


if __name__ == "__main__":
    main()
