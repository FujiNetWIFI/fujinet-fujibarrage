#!/usr/bin/env python3
"""checksize.py -- the image's budgets, from the AS listing's symbols
(FujiBlast's tools/chf/checksize.py, for FujiBarrage's pages).

  fixed    MB_FIX..MB_FIXEND   must end at or before the claim, $47FC
  pages    MB_Pn..MB_PnEND     8K each, in the window $4800-$67FF
  player   MB_P1..MB_P1END     8K, copied to RAM at PLYRAM
  data     MB_P4..MB_P4END     2K, copied to RAM at DATA
  RAM      the regions of ram.inc (RGN) in order, RAMEND <= RAMTOP

Each module's share of the fixed block is listed (the MB_ labels in order).
usage: checksize.py build/channelf/barrage.lst
"""
import re
import sys

text = open(sys.argv[1], encoding="latin-1").read()
syms = {}
for name, val in re.findall(r"([A-Z_][A-Z0-9_]*)\s*:\s*([0-9A-F]+)\s+\S\s*\|", text):
    syms.setdefault(name, int(val, 16))

errs = []
mods = sorted((v, k) for k, v in syms.items() if k.startswith("MB_") and not re.match(r"MB_P\d", k))
fix0, fix1 = syms["MB_FIX"], syms["MB_FIXEND"]
print("fixed:   $%04X-$%04X  %5d of %5d  (%d free)" % (fix0, fix1, fix1 - fix0, 0x47FC - fix0, 0x47FC - fix1))
if fix1 > 0x47FC:
    errs.append("the fixed block runs past $47FC")
fixmods = [(v, k) for v, k in mods if fix0 <= v <= fix1]
for (v, k), (v2, _) in zip(fixmods, fixmods[1:] + [(fix1, "")]):
    if k not in ("MB_FIX", "MB_FIXEND"):
        print("  %-12s %5d" % (k[3:].lower(), v2 - v))
names = {0: "boot", 1: "player (RAM)", 2: "hud", 3: "scr", 4: "data (RAM)", 5: "test"}
for n in range(6):
    a, b = syms.get("MB_P%d" % n), syms.get("MB_P%dEND" % n)
    if a is None:
        continue
    cap = 0x800 if n == 4 else 0x2000
    print("page %d %-13s %5d of %5d  (%d free)" % (n, names[n], b - a, cap, cap - (b - a)))
    if b - a > cap:
        errs.append("page %d is over %d" % (n, cap))
rgn = ["FLD", "PLYRAM", "DATA", "ST", "SCRIPT", "TBLS", "TXS", "PALS", "FBUF", "RAWBUF", "VARS"]
print("RAM:")
for k in rgn:
    if k in syms:
        print("  %-8s $%04X" % (k, syms[k]))
ramend, ramtop = syms["RAMEND"], syms["RAMTOP"]
print("  RAMEND   $%04X (top $%04X, %d free)" % (ramend, ramtop, ramtop - ramend))
if ramend > ramtop:
    errs.append("RAM past RAMTOP")
for e in errs:
    print("checksize: " + e, file=sys.stderr)
sys.exit(1 if errs else 0)
