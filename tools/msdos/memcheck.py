#!/usr/bin/env python3
"""memcheck.py BUILDDIR -- each DOS program's memory (from wlink's maps):
code, DGROUP (data, bss, stack) and what it needs to load; fails a program
whose DGROUP is past 64K."""
import os
import re
import sys

def sizes(path):
    """the map's segment sizes by class, and the memory needed"""
    text = open(path, errors="replace").read()
    code = data = 0
    for m in re.finditer(r"^(\S+)\s+(\S+)\s+(\S+)\s+([0-9a-f]{4}):([0-9a-f]{4})\s+([0-9a-f]+)\s*$", text, re.M):
        seg, cls, grp, _, _, size = m.groups()
        n = int(size, 16)
        if cls.upper() == "CODE":
            code += n
        else:
            data += n
    need = re.search(r"Memory size:\s+([0-9a-f]+)", text)
    return code, data, int(need.group(1), 16) if need else 0

bad = 0
d = sys.argv[1]
for name in ("barrage", "bmenu", "barrvga", "bmenuvga"):
    m = os.path.join(d, name + ".map")
    if not os.path.exists(m):
        continue
    code, data, need = sizes(m)
    flag = ""
    if data > 0x10000:
        flag = "  DGROUP OVER 64K"
        bad = 1
    print("%-9s code %6d  data %6d  load %6d%s" % (name.upper(), code, data, need, flag))
sys.exit(bad)
