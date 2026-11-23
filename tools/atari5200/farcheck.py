#!/usr/bin/env python3
"""farcheck.py -- no call across the 5200 game's overlays but through a
far wrapper (tools/atari5200/far.txt, mkfar.py).

    farcheck.py FIELD_OBJS -- ROOM_OBJS -- BOOT_OBJS -- CORE_OBJS

An overlay's code is only in its own bank: a symbol it exports (code or
read-only data) may be imported only by its own objects and by far.o
(whose wrappers pick the bank). od65 lists each object's imports and
exports. Its variables are in RAM, so anyone's: RAM_OK names them.
"""
import re
import subprocess
import sys


def dump(obj, what):
    out = subprocess.run(["od65", "--dump-" + what, obj], capture_output=True, text=True, check=True).stdout
    return set(re.findall(r'Name:\s+"([^"]+)"', out))


def main():
    args = sys.argv[1:]
    i = args.index("--")
    j = args.index("--", i + 1)
    k = args.index("--", j + 1)
    groups = {"field": args[:i], "room": args[i + 1:j], "boot": args[j + 1:k], "core": args[k + 1:]}
    owner = {}
    for g, objs in groups.items():
        if g == "core":
            continue
        for o in objs:
            for s in dump(o, "exports"):
                owner[s] = (g, o)
    bad = []
    for g, objs in groups.items():
        for o in objs:
            if o.endswith("far.s.o"):
                continue
            for s in dump(o, "imports"):
                if s in owner and owner[s][0] != g and s not in RAM_OK:
                    bad.append("%s (%s) uses %s from the %s overlay (%s): add it to far.txt"
                               % (o, g, s, owner[s][0], owner[s][1]))
    for b in sorted(bad):
        print("farcheck: " + b, file=sys.stderr)
    if bad:
        sys.exit(1)


# overlay variables (in RAM, so anyone's), and the core's functions an
# overlay's source holds (a #pragma code-name "CODE": src/play.c's entry_len)
RAM_OK = {"_cat_ok", "_cat_stale", "_tk", "_my_seat", "_entry_len"}

if __name__ == "__main__":
    main()
