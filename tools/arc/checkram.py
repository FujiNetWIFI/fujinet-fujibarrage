#!/usr/bin/env python3
"""checkram.py -- the Arcadia client's RAM ledger, from a page's listing.

    checkram.py build/arcadia/bb_p0.lst

src/arcadia/src/ram.inc hands out every byte by zone (VAR fails the
assembly when one runs over); this prints the ledger and checks what the
assembly can't see at once:
  - every zone within its bounds (X_END <= X_HI);
  - nothing persistent where a screen clears: zone A, the island, zone C,
    and cart RAM must miss $1800-$18CF and $1A00-$1ACF (the field's screen
    in 26-row mode; the Astrocade's bug of 2026-11-23 was ST under a text
    screen's lines);
  - TZ (a text screen's own) inside $1A00-$1ACF, which only 13-row mode
    leaves alone;
  - ST (STATE_SIZE) in RAM-L before the variables, SCRIPT at RAM-H's start;
  - the overlays (ram.inc OVERLAY base,n,phases: OVLk_LO, _N, _PH): no two
    that share a byte may share a phase (PB_* bits), so a name laid over
    another's bytes is live only when the other is dead.
Exit status 1 on any failure.
"""
import re
import sys

SYM = re.compile(r"\*?\s*([A-Za-z_][A-Za-z0-9_]*)\s*:\s*([0-9A-F]+)\s+[C-]\s")
SCREEN = [(0x1800, 0x18D0), (0x1A00, 0x1AD0)]


def symbols(path):
    text = open(path, errors="replace").read()
    tail = text[text.rfind("Symbol Table"):]
    return {m.group(1): int(m.group(2), 16) for m in SYM.finditer(tail)}


def main():
    s = symbols(sys.argv[1])
    bad = []
    zones = [("zone A", "ZA", True), ("island", "ZI", True), ("zone C", "ZC", True),
             ("TZ", "TZ", False), ("RAM-L", "CL", True), ("RAM-H", "CH", True)]
    print("checkram: the RAM ledger (src/arcadia/src/ram.inc)")
    used = {}
    for name, p, persist in zones:
        lo, hi, end = s[p + "_LO"], s[p + "_HI"], s[p + "_END"]
        used[p] = end - lo
        if end > hi:
            bad.append("%s runs over by %d bytes" % (name, end - hi))
        if persist:
            for a, b in SCREEN:
                if lo < b and a < hi:
                    bad.append("%s ($%04X-$%04X) lies where a screen clears ($%04X-$%04X)"
                               % (name, lo, hi - 1, a, b - 1))
        extra = ""
        if p == "CL":
            v = s["CLVARS"]
            lo2 = s["ST"]
            extra = " (ST %d at $%04X, variables %d, C3N + CELL3 %d)" % (
                s["STATE_SIZE"], lo2, v, s["CL_END"] - s["C3N"])
        if p == "CH":
            extra = " (SCRIPT %d, the shot's %d)" % (s["SHOTRES"] - s["SCRIPT"], s["CH_END"] - s["SHOTRES"])
        if p == "TZ":
            extra = " (dead on the field: a text screen's own)"
        print("  %-7s $%04X-$%04X  %4d of %4d used, %4d free%s" % (
            name, lo, hi - 1, end - lo, hi - lo, hi - end, extra))
    tz = (s["TZ_LO"], s["TZ_HI"])
    if not (0x1A00 <= tz[0] and tz[1] <= 0x1AD0):
        bad.append("TZ ($%04X-$%04X) isn't within $1A00-$1ACF" % (tz[0], tz[1] - 1))
    st, size = s["ST"], s["STATE_SIZE"]
    if st < 0x0600 or st + size > s["CL_LO"]:
        bad.append("ST ($%04X + %d) isn't in RAM-L before the variables ($%04X)" % (st, size, s["CL_LO"]))
    if s["SCRIPT"] != 0x2900:
        bad.append("SCRIPT ($%04X) isn't at RAM-H's start" % s["SCRIPT"])
    ovl = []
    k = 0
    while "OVL%d_LO" % k in s:
        ovl.append((s["OVL%d_LO" % k], s["OVL%d_N" % k], s["OVL%d_PH" % k], k))
        k += 1
    pb = sorted(((v, n) for n, v in s.items() if n.startswith("PB_")), key=lambda t: t[0])

    def phases(m):
        return "|".join(n for v, n in pb if m & v) or "-"
    shared = 0
    for i, (lo, n, ph, k1) in enumerate(ovl):
        for lo2, n2, ph2, k2 in ovl[i + 1:]:
            if lo < lo2 + n2 and lo2 < lo + n:
                shared += 1
                if ph & ph2:
                    bad.append("overlay %d ($%04X+%d, %s) and %d ($%04X+%d, %s) share a byte and %s"
                               % (k1, lo, n, phases(ph), k2, lo2, n2, phases(ph2), phases(ph & ph2)))
    print("  overlays: %d declared, %d pairs share bytes, all in phases of their own" % (len(ovl), shared)
          if not any(b.startswith("overlay") for b in bad) else "  overlays: %d declared" % len(ovl))
    con = used["ZA"] + used["ZI"] + used["ZC"]
    cart = (s["CL_END"] - 0x0600) + (s["CH_END"] - 0x2900)
    print("  console (26-row mode): %d of 84 used, %d free;  cart: %d of 1024 used "
          "(the shot's %d), %d free" % (con, 84 - con, cart,
                                        (s["CL_END"] - s["C3N"]) + (s["CH_END"] - s["SHOTRES"]),
                                        1024 - cart))
    for b in bad:
        print("checkram: " + b)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
