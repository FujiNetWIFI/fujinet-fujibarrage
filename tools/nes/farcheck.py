#!/usr/bin/env python3
"""farcheck.py -- no call across the NES game's overlays but through a far
wrapper (tools/nes/far.txt, mkfar.py; the 5200's tools/atari5200/farcheck.py).

    farcheck.py NAME OBJS... -- NAME OBJS... -- ... -- core OBJS...

An overlay's code is only in its own bank: a symbol it exports (code or
read-only data) may be imported only by its own objects and by far.s.o
(whose wrappers pick the bank). od65 lists each object's imports and
exports. Its variables are in RAM, so anyone's: RAM_OK names them.
(src/appkey.c is two banks' -- its boot half and its saves -- and neither
calls the other.)
"""
import re
import subprocess
import sys


def dump(obj, what):
    out = subprocess.run(["od65", "--dump-" + what, obj], capture_output=True, text=True, check=True).stdout
    return set(re.findall(r'Name:\s+"([^"]+)"', out))


# overlay variables (in RAM, so anyone's), and the core's functions an
# overlay's source holds (a #pragma code-name "CODE": src/play.c's entry_len);
# checkrom.py fails an image where any of these lands in the overlay window
RAM_OK = {"_cat_ok", "_cat_stale", "_tk", "_my_seat", "_entry_len", "_test_hook",
          "_test_t0", "_test_t1", "_fix_ram", "_in_script", "_my_name", "_srv_host",
          "_srv_port", "_srv_table", "_band_pal", "_drawn", "_fire_on", "_flash_n",
          "_field_shown", "_rnd_stars", "_bln_cells", "_hud_sh", "_nm_col", "_nm_n",
          "_hp_row", "_hp_col", "_hp_inv"}


# the sound bank's (bank 6) used from elsewhere: the tunes' addresses,
# which C only hands to snd_tune (the player reads them, with bank 6 in),
# and the tables the player (src/nes/snd.s: the core's object) reads
SND_OK = {"_t_title1", "_t_title2", "_t_win1", "_t_win2", "_t_over1", "_t_over2",
          "_t_shop1", "_t_shop2", "_sx_cues", "_sx_booms", "_sx_ui", "_mus_per", "_mus_nz"}


def main():
    groups, cur = {}, None
    args = sys.argv[1:]
    while args:
        cur, args = args[0], args[1:]
        objs = []
        while args and args[0] != "--":
            objs.append(args[0])
            args = args[1:]
        args = args[1:]
        groups[cur] = objs
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
                if s in owner and owner[s][0] != g and s not in RAM_OK and s not in SND_OK:
                    bad.append("%s (%s) uses %s from the %s overlay (%s): add it to far.txt"
                               % (o, g, s, owner[s][0], owner[s][1]))
    for b in sorted(bad):
        print("farcheck: " + b, file=sys.stderr)
    if bad:
        sys.exit(1)


if __name__ == "__main__":
    main()
