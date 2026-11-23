#!/usr/bin/env python3
"""prof.py MAPFILE TRACE -- count a MAME trace's instructions (cocosim's
trace step) by function: the nearest symbol at or below each address."""
import bisect
import collections
import glob
import os
import re
import sys

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# the map lists the sources' equates too, as if they were addresses: not those
EQU = set()
for f in glob.glob(os.path.join(TOP, "src", "**", "*.asm"), recursive=True) + \
        glob.glob(os.path.join(TOP, "src", "**", "*.inc"), recursive=True):
    EQU.update(re.findall(r"^(\w+)\s+equ\b", open(f, errors="replace").read(), re.M | re.I))
syms = []
for name, addr in re.findall(r"Symbol: (\S+) \(.*\) = ([0-9A-F]{4})", open(sys.argv[1]).read()):
    if name.startswith(("L0", "\\", ".", "l_", "s_", "@")) or "funcend" in name or "funcsize" in name \
            or name in EQU:
        continue
    syms.append((int(addr, 16), name))
syms.sort()
keys = [a for a, _ in syms]
count = collections.Counter()
total = 0
for line in open(sys.argv[2], errors="replace"):
    m = re.match(r"([0-9A-F]{4}):", line)
    if not m:
        continue
    a = int(m.group(1), 16)
    i = bisect.bisect_right(keys, a) - 1
    count[syms[i][1] if i >= 0 else "?"] += 1
    total += 1
print("%d instructions" % total)
for name, n in count.most_common(30):
    print("%5.1f%%  %s" % (100.0 * n / total, name))
