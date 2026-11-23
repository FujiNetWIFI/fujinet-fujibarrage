#!/usr/bin/env python3
"""fixture.py -- a test scene's fixture for the Adam (src/adam/test/fixext.h):
refview plays a shot at the Adam's shape; its -fixture-all header becomes
the blob adamrun loads at fix_ram and the segments it serves.

    fixture.py OUTDIR [refview's arguments...]

OUTDIR/fix.h (refview's header), fix.bin (the blob), seg0.bin .. (the
shot's segments), ref/ (refview's -out: shot1.px is the field after it).
Prints the script's frames.

The blob: sky, palette, segments; the ten tanks' x (16-bit), y, angle;
then the TERRAIN entries, a column each.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.abspath(os.path.join(HERE, "..", ".."))
REFVIEW = os.path.join(TOP, "server", "refview")
SHAPE = ["-w", "256", "-h", "168", "-par", "73", "-tankw", "7", "-tankh", "5", "-buf", "1536"]
FIX_RAM = 2048


def refview():
    """the refview binary, built if it's missing or older than the server's sources"""
    newest = 0
    for d, _, fs in os.walk(os.path.join(TOP, "server")):
        for f in fs:
            if f.endswith(".go"):
                newest = max(newest, os.path.getmtime(os.path.join(d, f)))
    if not os.path.exists(REFVIEW) or os.path.getmtime(REFVIEW) < newest:
        subprocess.run(["go", "build", "-o", "refview", "./cmd/refview"], cwd=os.path.join(TOP, "server"),
                       check=True)
    return REFVIEW


def parse(text):
    d = {}
    for m in re.finditer(r"#define (FIX_\w+) (\d+)", text):
        d[m.group(1)] = int(m.group(2))
    for m in re.finditer(r"static const \w+ (\w+)\[\d+\] = \{(.*?)\};", text, re.S):
        d[m.group(1)] = [int(v) for v in m.group(2).replace("\n", " ").split(",") if v.strip()]
    return d


def blob(d):
    out = bytearray([d["FIX_SKY"], d["FIX_PAL"], d["FIX_NSEG"]])
    for x in d["fix_tx"]:
        out += bytes([x & 255, x >> 8])
    out += bytes(d["fix_ty"]) + bytes(d["fix_ta"]) + bytes(d["fix_terrain"])
    if len(out) > FIX_RAM:
        sys.exit("fixture.py: the fixture is %d bytes: over fix_ram's %d" % (len(out), FIX_RAM))
    return bytes(out)


def make(out, args):
    os.makedirs(out, exist_ok=True)
    r = subprocess.run([refview()] + ["-shots", "1"] + SHAPE + args +
                       ["-fixture-all", os.path.join(out, "fix.h"), "-out", os.path.join(out, "ref")],
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit("refview: " + r.stderr)
    d = parse(open(os.path.join(out, "fix.h")).read())
    open(os.path.join(out, "fix.bin"), "wb").write(blob(d))
    at = 0
    for k, n in enumerate(d["fix_seglen"]):
        open(os.path.join(out, "seg%d.bin" % k), "wb").write(bytes(d["fix_seg"][at:at + n]))
        at += n
    m = re.search(r"(\d+) frames", r.stdout)
    return int(m.group(1)) if m else 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    print(make(sys.argv[1], sys.argv[2:]))
