#!/usr/bin/env python3
"""mkstate_s2.py -- a canned STATE and the catalog into a copy of a TEST
build's image (FIXBUF, bank BK_FIX), for test scene 5 (the screens).

    mkstate_s2.py IMAGE.st2 LISTING OUT.st2 PHASE [key=value ...]

PHASE: wait, shop, aim, round, over (tools/chf/mkstate_chf.py's state:
seats S2 (ours, the host) and bots; key=value bytes as there). The catalog
comes from barraged -catalog. FIXBUF: the state at +0, the catalog's
length at +510 (LE), the catalog at +512.
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import mkfix_s2  # noqa: E402


def main():
    img, lst, out, phase = sys.argv[1:5]
    with tempfile.TemporaryDirectory() as d:
        cat = os.path.join(d, "cat.bin")
        subprocess.run(["go", "run", "./cmd/barraged", "-catalog", cat], cwd=os.path.join(TOP, "server"),
                       check=True, capture_output=True)
        stf = os.path.join(d, "st.bin")
        subprocess.run(["python3", os.path.join(TOP, "tools/chf/mkstate_chf.py"), cat, phase, stf] + sys.argv[5:],
                       check=True)
        st = bytearray(open(stf, "rb").read())
        st[72:72 + 8] = b"S2\0\0\0\0\0\0"          # (seat 0's name: ours)
        catb = open(cat, "rb").read()
    data = bytes(st).ljust(510, b"\0") + bytes([len(catb) & 255, len(catb) >> 8]) + catb
    mkfix_s2.patch(img, lst, out, data)


if __name__ == "__main__":
    main()
