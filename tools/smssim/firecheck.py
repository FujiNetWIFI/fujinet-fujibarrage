#!/usr/bin/env python3
"""firecheck.py -- does the SMS show its fire while a shot plays? Every frame
from the script's start, the store's fire (the cart's RAM, half 0: the F
plane) against the fire on the screen (the name table's field cells, rows
0-20, pixels in palette 0's fire colours 11-13), pixel for pixel.

usage: [FC_ROM=IMAGE FC_MAP=MAP] tools/smssim/firecheck.py [-flat] [-seed N] [-j N] [-keep] [-ideal DIR] OUTDIR [WEAPON...]

For each weapon (default 0 1 2 3 7: the missiles, the nukes, MEGA MIRV)
refview makes the shot (tools/adamsim/fixture.py), smsrun plays it in the
TEST=1 build (make sms TEST=1) and dumps the store and VRAM each frame.
Prints per run: the frames with fire in the store, the share of the
store's fire pixels that were on the screen the same frame (shown), and
the frames on which at least half of it was. -keep keeps the dumps.
-ideal DIR: also the screen's fire against the store's in an earlier -keep
run (DIR/W/cap: the blasts drawn every frame, as they were before the SMS
waited for its screen), frame for frame.
"""
import os
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(TOP, "tools", "adamsim"))
import fixture  # noqa: E402

SMSRUN = os.path.join(HERE, "smsrun")
ROM = os.environ.get("FC_ROM") or os.path.join(TOP, "r2r", "sms", "barrage.sms")
MAP = os.environ.get("FC_MAP") or os.path.join(TOP, "build", "sms", "barrage.map")
NT = 0x3800
FIRE = (11, 12, 13)


def store_fire(half0):
    """the F plane as a set of (x, y)"""
    out = set()
    for r in range(21):
        for c in range(32):
            for yy in range(8):
                b = half0[r * 512 + 256 + c * 8 + yy]
                if b:
                    for xx in range(8):
                        if b & (0x80 >> xx):
                            out.add((c * 8 + xx, r * 8 + yy))
    return out


def screen_fire(vram):
    """the field cells' pixels in a fire colour (palette 0) as a set of (x, y)"""
    out = set()
    cache = {}
    for r in range(21):
        for c in range(32):
            w = vram[NT + (r * 32 + c) * 2] | vram[NT + (r * 32 + c) * 2 + 1] << 8
            if w & 0x0800:              # palette 1: the text's, not fire
                continue
            t = w & 0x1FF
            hf, vf = w & 0x200, w & 0x400
            if t not in cache:
                px = []
                for y in range(8):
                    p = vram[t * 32 + y * 4: t * 32 + y * 4 + 4]
                    px.append([sum(((p[b] >> (7 - x)) & 1) << b for b in range(4)) for x in range(8)])
                cache[t] = px
            px = cache[t]
            for yy in range(8):
                row = px[7 - yy if vf else yy]
                for xx in range(8):
                    if row[7 - xx if hf else xx] in FIRE:
                        out.add((c * 8 + xx, r * 8 + yy))
    return out


def one(d, w, seed, extra, keep, ideal):
    os.makedirs(d, exist_ok=True)
    frames = fixture.make(d, ["-seed", seed, "-weapon", str(w)] + extra)
    n = frames + 240
    cap = os.path.join(d, "cap")
    os.makedirs(cap, exist_ok=True)
    lines = ["until _test_hook+1 0xC3 900", "load _fix_ram %s" % os.path.join(d, "fix.bin"),
             "serve _test_hook %s" % d, "poke _test_hook+2 1 2", "poke _test_hook 0x5A",
             "until _in_script 1 2000"]
    for i in range(n):
        lines += ["cartram cap/s%04d 0" % i, "vram cap/v%04d" % i, "wait 1"]
    lines += ["until _test_hook 0xDD 30000", "wait 2", "peekw _test_t1"]
    scen = os.path.join(d, "fscene")
    open(scen, "w").write("\n".join(lines) + "\n")
    r = subprocess.run([SMSRUN, ROM, MAP, scen, d], capture_output=True, text=True)
    if r.returncode:
        return "weapon %2d: the run failed: %s" % (w, (r.stderr or r.stdout).strip()[-300:])
    played = None
    for line in r.stdout.splitlines():
        if line.startswith("_test_t1:"):
            played = int(line.split()[1])
    fire_frames = shown_px = store_px = half = 0
    i_px = i_shown = i_frames = i_half = 0
    first = last = None
    strip = []
    for i in range(n):
        s = open(os.path.join(cap, "s%04d" % i), "rb").read()
        sf = store_fire(s)
        vf = None
        if ideal:
            ip = os.path.join(ideal, str(w), "cap", "s%04d" % i)
            isf = store_fire(open(ip, "rb").read()) if os.path.exists(ip) else set()
            if len(isf) > 3:            # (a blast, not a shell's head)
                v = open(os.path.join(cap, "v%04d" % i), "rb").read()
                vf = screen_fire(v)
                i_frames += 1
                i_px += len(isf)
                i_shown += len(vf)
                i_half += len(vf) * 2 >= len(isf)
        if not sf:
            continue
        if vf is None:
            v = open(os.path.join(cap, "v%04d" % i), "rb").read()
            vf = screen_fire(v)
        both = len(sf & vf)
        fire_frames += 1
        store_px += len(sf)
        shown_px += both
        half += both * 2 >= len(sf)
        first = i if first is None else first
        last = i
        strip.append("%d/%d" % (both, len(sf)))
    if not keep:
        shutil.rmtree(cap)
    open(os.path.join(d, "strip.txt"), "w").write(" ".join(strip) + "\n")
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
    for line in res:
        print(line)


if __name__ == "__main__":
    main()
