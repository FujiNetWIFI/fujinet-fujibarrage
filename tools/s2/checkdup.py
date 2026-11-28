#!/usr/bin/env python3
"""checkdup.py -- a label defined twice across src/studio2 (AS's labels are
global to the program: one in two files clashes, and the error lands far
from either).

    tools/s2/checkdup.py src/studio2/src/*.inc src/studio2/rt/*.inc
"""
import re
import sys

LABEL = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*):")
EQU = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\s+(?:EQU|FUNCTION)\b", re.I)


def main():
    seen, bad = {}, 0
    for path in sys.argv[1:]:
        depth = 0
        for n, line in enumerate(open(path, errors="replace"), 1):
            code = line.split(";")[0]
            if re.match(r"^\s*\w*\s*MACRO\b", code, re.I) or re.search(r"\bMACRO\b", code, re.I):
                depth += 1
            if re.match(r"^\s*ENDM\b", code, re.I):
                depth -= 1
                continue
            if depth:
                continue
            m = LABEL.match(code) or EQU.match(code)
            if not m:
                continue
            name = m.group(1)
            if name in seen and seen[name][0] != (path, n):
                print("checkdup: %s:%d: %s also at %s:%d" % (path, n, name, *seen[name]), file=sys.stderr)
                bad = 1
            seen.setdefault(name, (path, n))
    if not bad:
        print("checkdup: %d labels, none twice" % len(seen))
    return bad


if __name__ == "__main__":
    sys.exit(main())
