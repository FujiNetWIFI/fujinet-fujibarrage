#!/usr/bin/env python3
"""fixture.py -- a test scene's fixture for the PC (src/msdos/test/fixext.h):
refview plays a shot at the PC's shape; its -fixture-all header becomes the
files a TEST=1 build reads from its disk (src/msdos/fixfile.c).

    fixture.py OUTDIR [refview's arguments...]

OUTDIR/fix.h (refview's header), FIX.BIN (the blob), SEG00.BIN .. (the
shot's segments), ref/ (refview's -out: shot1.px is the field after it).
Prints the script's frames. The blob is the Adam's (tools/adamsim/fixture.py):
sky, palette, segments; the ten tanks' x (16-bit), y, angle; then the
TERRAIN entries, a column each.
"""
import os
import sys

import importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("adamfix", os.path.join(HERE, "..", "adamsim", "fixture.py"))
adamfix = importlib.util.module_from_spec(_spec)        # (the Adam's: same blob, other shape)
_spec.loader.exec_module(adamfix)

SHAPE = ["-w", "320", "-h", "176", "-par", "55", "-tankw", "12", "-tankh", "6", "-buf", "4096"]
FIX_RAM = 4096


def make(out, args):
    import re
    import subprocess
    os.makedirs(out, exist_ok=True)
    r = subprocess.run([adamfix.refview()] + ["-shots", "1"] + SHAPE + args +
                       ["-fixture-all", os.path.join(out, "fix.h"), "-out", os.path.join(out, "ref")],
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit("refview: " + r.stderr)
    d = adamfix.parse(open(os.path.join(out, "fix.h")).read())
    adamfix.FIX_RAM = FIX_RAM
    open(os.path.join(out, "FIX.BIN"), "wb").write(adamfix.blob(d))
    at = 0
    for k, n in enumerate(d["fix_seglen"]):
        open(os.path.join(out, "SEG%02d.BIN" % k), "wb").write(bytes(d["fix_seg"][at:at + n]))
        at += n
    m = re.search(r"(\d+) frames", r.stdout)
    return int(m.group(1)) if m else 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    print(make(sys.argv[1], sys.argv[2:]))
