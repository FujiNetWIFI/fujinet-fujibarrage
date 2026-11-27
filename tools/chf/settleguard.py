#!/usr/bin/env python3
"""settleguard.py -- the settle rule over the FujiNet Go Channel F core's
port log (chf_headless -iolog FILE: "W cycles port data" a write, "F n
cycles" a frame).

After a VRAM strobe's $50 to port 0, nothing may go to port 0, 1, 4 or 5
for SETTLE clocks (the console is still storing the pixel; MAME and the Go
core store at once, so only this check sees it). Prints the writes that
came early and the smallest gap; exits 1 if any did.

usage: settleguard.py IOLOG [SETTLE]       (SETTLE default 94)
"""
import sys


def main():
    path = sys.argv[1]
    settle = int(sys.argv[2]) if len(sys.argv) > 2 else 94
    last, early, mingap, writes, frame, shown = None, 0, None, 0, 0, 0
    for line in open(path):
        w = line.split()
        if not w:
            continue
        if w[0] == "F":
            frame = int(w[1])
            continue
        if w[0] != "W":
            continue
        cyc, port, data = int(w[1]), int(w[2]), int(w[3])
        if port not in (0, 1, 4, 5):
            continue
        writes += 1
        if last is not None:
            gap = cyc - last
            if mingap is None or gap < mingap:
                mingap = gap
            if gap < settle:
                early += 1
                if shown < 10:
                    print("early: port %d <- %02X %d clocks after a strobe (frame %d, cycle %d)"
                          % (port, data, gap, frame, cyc))
                    shown += 1
        if port == 0 and data == 0x50:
            last = cyc
    print("SETTLE %d early (min %s clocks, guard %d, %d writes, %d frames)"
          % (early, mingap, settle, writes, frame))
    sys.exit(1 if early else 0)


if __name__ == "__main__":
    main()
