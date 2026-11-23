#!/usr/bin/env python3
"""edges.py -- do the colour changes hold still from frame to frame?

    edges.py X PICTURE...

For each picture, the rows where the colour at column X changes from the
row above. A display-list interrupt that lands on a different line in
different frames moves an edge: the edges that aren't in every picture
are printed (none: steady). For tools/a52sim's `burst` and the Go
desktop's A52_SCREENSHOT runs.
"""
import sys
from collections import Counter
from PIL import Image


def edges(path, x):
    im = Image.open(path).convert("RGB")
    col = [im.getpixel((x, y)) for y in range(im.height)]
    return {y for y in range(1, len(col)) if col[y] != col[y - 1]}


def main():
    x = int(sys.argv[1])
    pics = sys.argv[2:]
    sets = [edges(p, x) for p in pics]
    every = set.intersection(*sets)
    c = Counter(y for s in sets for y in s if y not in every)
    print("edges.py: %d pictures, %d edges in every one at x=%d, %d rows that come and go%s"
          % (len(pics), len(every), x, len(c), (": " + ", ".join("%d (%d)" % kv for kv in sorted(c.items()))) if c else ""))
    sys.exit(1 if c else 0)


if __name__ == "__main__":
    main()
