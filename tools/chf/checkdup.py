#!/usr/bin/env python3
"""checkdup.py -- one name, one meaning: AS's symbols are global across the
fixed block and every page, and its labels are case-insensitive, so two
files' local labels of one name collide ("double defined", and often a
branch then reaching the other one). Fails the build on any name defined
twice (labels, EQUs, ram.inc VARs and RGNs); macro-local m_ labels aside.

usage: checkdup.py FILE...
"""
import collections
import re
import sys


def main():
    defs = collections.defaultdict(list)
    for f in sys.argv[1:]:
        for n, line in enumerate(open(f, encoding="latin-1"), 1):
            for pat in (r"^([A-Za-z_]\w*):", r"^([A-Za-z_]\w*)\s+EQU\b", r"^\s+(?:VAR|RGN)\s+(\w+)"):
                m = re.match(pat, line, re.I)
                if m:
                    defs[m.group(1).upper()].append("%s:%d" % (f.split("/")[-1], n))
    bad = [(k, v) for k, v in sorted(defs.items()) if len(v) > 1 and not k.startswith("M_")
           and k not in ("NAME", "SIZE")]
    for k, v in bad:
        print("checkdup: %s defined at %s" % (k, ", ".join(v)))
    if bad:
        sys.exit("checkdup: %d name(s) defined twice" % len(bad))
    print("checkdup: ok")


if __name__ == "__main__":
    main()
