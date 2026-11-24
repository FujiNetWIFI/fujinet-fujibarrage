#!/usr/bin/env python3
"""fieldcheck.py OUTDIR -- the Arcadia field as the client drew it, against
the model (tools/arc/mock_arc.py's Mock.build, look B, night): for every
snapshot of a tools/arcsim/scenes/field.txt run (OUTDIR/out.txt: its PEEKs
of NTV, MYSEAT, MYTURN+AIMA, ST and the UVI $1800-$1AFF; OUTDIR/tap.log:
the server's frames, tools/rigs/tapproxy.py) the field's terrain rebuilt
from the TERRAIN frames at the client's version, the tanks from its STATE,
then:
  - the field's 384 cells (rows 0-23), cell for cell;
  - the sprites: their registers, pictures (UDC $38-$3B) and colours;
  - the mode registers (multicolour, block graphics, colours, VPOS);
  - the picture: arcmodel.py's rendering of the peeked UVI against MAME's
    snapshot, pixel for pixel (the model is what MAME draws);
and prints the HUD's rows as text. Exit 1 on a difference.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import arcmodel as A  # noqa: E402
import mock_arc as M  # noqa: E402

FW = FH = 48
OFF_TURN, OFF_SEATS, SEAT_SIZE = 4, 72, 25
P_NAME, P_KIND, P_ANGLE, P_X, P_Y, P_FLAGS = 0, 9, 14, 17, 19, 21
GL = " /\\#^|_|::::<><>0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.,+$@@@@<>@@"


# a SCRIPT op's length (docs/PROTOCOL.md), for the settles
FIXLEN = {0x01: 1, 0x10: 18, 0x12: 1, 0x20: 8, 0x21: 7, 0x23: 10, 0x31: 2, 0x32: 3, 0x33: 3, 0x34: 6,
          0x35: 2, 0x36: 5, 0x37: 2, 0x38: 1, 0x39: 2, 0x3A: 5}


def entry(b, i):
    """the column entry at b[i]: its spans [(top, end)], its length"""
    e = b[i]
    if e < 0xF0:
        return [(e, FH)], 1
    if e == 0xF0:
        return [], 1
    n = e & 15
    return [(b[i + 1 + 2 * k], b[i + 2 + 2 * k]) for k in range(n)], 1 + 2 * n


def settles(seg, cols):
    """the segment's OP_SETTLEs into cols (the field after a shot is its
    terrain with them: the server's version bump after it has no columns)"""
    i = 0
    while i < len(seg):
        o = seg[i]
        i += 1
        if o == 0:
            return
        if o in FIXLEN:
            i += FIXLEN[o]
        elif o == 0x11:
            i += 7 + seg[i + 6]
        elif o == 0x24:
            i += 4 + 3 * seg[i + 3]
        elif o == 0x30:
            i += 2 + seg[i + 1]
        elif o == 0x25:
            x0, n = seg[i] | seg[i + 1] << 8, seg[i + 2]
            i += 4
            for k in range(n):
                col, ln = entry(seg, i)
                i += ln
                if x0 + k < FW:
                    cols[x0 + k] = col
        else:
            return


def versions(tap):
    """every whole terrain version in the log, in order: (tv, columns); a
    version bump with no columns after a shot is the last version with the
    shot's settles"""
    out, cur, base = [], None, None
    have = {}
    work, segs = None, {}
    for line in open(tap):
        w = line.split()
        if w and re.match(r"^[0-9.]+$", w[0]):
            w = w[1:]                   # (TAP_BOTH: the time first)
        if len(w) == 2 and w[0] == "S" and work is not None:
            p = bytes.fromhex(w[1])
            sid, sg, fl, off = p[0], p[1], p[2], p[3] | p[4] << 8
            buf = segs.setdefault((sid, sg), bytearray())
            if off == len(buf):
                buf += p[5:]
            if fl & 1:
                settles(bytes(buf), work)
            continue
        if len(w) != 2 or w[0] != "G":
            continue                    # (and "> X": the client's)
        h = w[1]
        p = bytes.fromhex(h)
        b, tv, fl, x = p[0], p[1], p[2], p[3] | p[4] << 8
        if len(p) == 5 and b != 0xFF and fl & 1 and work is not None:
            have[tv] = [list(c) for c in work]          # (the bump: the settles)
            out.append((tv, [list(c) for c in work]))
            cur = None
            continue
        if cur is None or cur[0] != tv:
            if b == 0xFF:
                cols = [[] for _ in range(FW)]
            elif b in have:
                cols = [list(c) for c in have[b]]
            else:
                cols = None
            cur = (tv, cols)
        cols = cur[1]
        i = 5
        while i < len(p) and cols is not None:
            e = p[i]
            i += 1
            if e < 0xF0:
                col = [(e, FH)]
            elif e == 0xF0:
                col = []
            else:
                col = []
                for _ in range(e & 15):
                    col.append((p[i], p[i + 1]))
                    i += 2
            if x < FW:
                cols[x] = col
            x += 1
        if fl & 1 and cols is not None:
            have[tv] = cols
            out.append((tv, [list(c) for c in cols]))
            work, segs = [list(c) for c in cols], {}
            cur = None
    return out


def kinds(cols):
    px = bytearray(FW * FH)
    for x, col in enumerate(cols):
        for top, end in col:
            for y in range(max(top, 0), min(end, FH)):
                px[y * FW + x] = 1
    return bytes(px)


def snaps(out):
    """the snapshots and what was peeked after each"""
    res, cur = [], None
    for line in open(out):
        m = re.match(r"SNAP (\S+) (\d+)", line)
        if m:
            cur = {"name": m.group(1), "frame": int(m.group(2))}
            res.append(cur)
            continue
        m = re.match(r"PEEK ([0-9A-F]{4}): (.*)", line)
        if m and cur is not None:
            cur.setdefault("peek", []).append((int(m.group(1), 16), bytes.fromhex(m.group(2).replace(" ", ""))))
    return res


LAST = {}


def check(s, vers, syms, outdir):
    pk = dict(LAST)                     # (what a snapshot didn't peek: the last's)
    pk.update({a: d for a, d in s.get("peek", [])})
    LAST.update(pk)
    uvi = pk.get(0x1800)
    st = pk.get(syms["ST"])
    if uvi is None or st is None or len(uvi) < 768:
        return None
    if uvi[0x1F8] != 0xED:              # (a text screen: not the field)
        return True, ["not the field (a text screen)"]
    ntv = pk[syms["NTV"]][0]
    my = pk[syms["MYSEAT"]][0]
    myturn, aima = pk[syms["MYTURN"]][0], pk[syms["MYTURN"]][1]
    tanks = []
    for i in range(10):
        r = st[OFF_SEATS + i * SEAT_SIZE:OFF_SEATS + (i + 1) * SEAT_SIZE]
        on = r[P_KIND] != 0 and r[P_FLAGS] & 1 and r[P_X + 1] == 0 and r[P_X] < FW
        a = aima if (myturn and i == my) else r[P_ANGLE]
        tanks.append((bool(on), r[P_X], r[P_Y], a))
    M.OWN = my
    shooter = st[OFF_TURN]
    if shooter >= 10 and my < 10:       # (everyone's turn: the client counts from
        shooter = my                    #  ours, which is mock_arc's shooter = OWN)
    lines = []
    best = None
    for k, (tv, cols) in enumerate(vers):
        if tv != ntv:
            continue
        f = {"px": kinds(cols), "tanks": tanks, "shooter": shooter}
        u, conf, wrong, cells = M.Mock("B", "night").build(f, 0)
        got = uvi[:13 * 16] + uvi[0x200:0x200 + 11 * 16]
        diff = [j for j in range(384) if got[j] != cells[j]]
        if best is None or len(diff) < len(best[1]):
            best = (k, diff, u, conf)
    if best is None:
        lines.append("no whole terrain version %02X in the log" % ntv)
        return False, lines
    k, diff, u, conf = best
    ok = not diff
    lines.append("terrain tv %02X (version %d in the log), turn %s, us %d%s: field cells %s, 3-colour cells %d"
                 % (ntv, k, shooter, my, " aiming %d" % aima if myturn else "",
                    "384/384 equal" if ok else "%d differ: %s" % (len(diff), diff[:12]), conf))
    # the sprites, the registers
    m = u.m
    used = [n for n in range(4) if m[0xF0 + 2 * n]]
    sp = [(n, uvi[0xF0 + 2 * n], uvi[0xF1 + 2 * n], m[0xF0 + 2 * n], m[0xF1 + 2 * n]) for n in range(4)]
    bad = [x for x in sp if (x[1], x[2]) != (x[3], x[4])]
    pics = [n for n in used if uvi[0x180 + 8 * n:0x188 + 8 * n] != m[0x180 + 8 * n:0x188 + 8 * n]]
    regs = {"SPRC23": (uvi[0x1FA], m[0x1FA]), "SPRC01": (uvi[0x1FB], m[0x1FB]),
            "GFXMOD": (uvi[0x1F8], m[0x1F8]), "BGCOL": (uvi[0x1F9] & ~0x40, m[0x1F9]),
            "VPOS": (uvi[0xFC], m[0xFC]), "MULTI": (uvi[0xFD] & 0x80, m[0xFD] & 0x80)}
    # (M4: while we aim, our tank's sprite blinks yellow, 16 frames of 32:
    # a sprite colour field may be yellow then instead of the model's)

    def colok(k2, v):
        if k2 not in ("SPRC01", "SPRC23") or not myturn:
            return v[0] == v[1]
        return (v[0] & 0xC0) == (v[1] & 0xC0) and all(
            ((v[0] >> sh) & 7) in (((v[1] >> sh) & 7), 1) for sh in (0, 3))
    rbad = {k2: v for k2, v in regs.items() if not colok(k2, v)}
    lines.append("sprites %d used: positions %s, pictures %s, registers %s"
                 % (len(used), "equal" if not bad else "differ %s" % bad,
                    "equal" if not pics else "differ %s" % pics, "equal" if not rbad else "differ %s" % rbad))
    ok = ok and not bad and not pics and not rbad
    # the picture: the model's rendering of what the client wrote vs MAME's
    png = os.path.join(outdir, s["name"] + ".png")
    if os.path.exists(png):
        from PIL import Image
        mu = A.UVI()
        mu.poke(0x1800, uvi)
        fr = mu.frame()
        im = Image.open(png).convert("RGB")
        w, h = im.size
        px = im.load()
        nd = sum(1 for y in range(min(h, A.H)) for x in range(min(w, A.W))
                 if px[x, y] != A.COLORS[fr[y][x]])
        lines.append("picture: %s (%dx%d, arcmodel of the peeked UVI vs MAME's snapshot)"
                     % ("0 px differ" if nd == 0 else "%d px differ" % nd, w, h))
        ok = ok and nd == 0
    for r in (24, 25):
        row = uvi[0x200 + (r - 13) * 16:0x200 + (r - 12) * 16]
        lines.append("HUD %d |%s|" % (r, "".join(GL[c & 0x3F] for c in row[1:])))
    return ok, lines


def symbols(lst):
    s = {}
    for line in open(lst, errors="replace"):
        for m in re.finditer(r"\b([A-Z_][A-Z0-9_]*)\s*:\s*([0-9A-F]+) [C-]", line):
            s.setdefault(m.group(1), int(m.group(2), 16))
    return s


def main(outdir, lstdir=None):
    lstdir = lstdir or os.path.join(os.path.dirname(__file__), "..", "..", "build", "arcadia-rig", "obj")
    syms = symbols(os.path.join(lstdir, "bb_p0.lst"))
    vers = versions(os.path.join(outdir, "tap.log"))
    allok = True
    for s in snaps(os.path.join(outdir, "out.txt")):
        r = check(s, vers, syms, outdir)
        if r is None:
            continue
        ok, lines = r
        allok = allok and ok
        print("%s (frame %d): %s" % (s["name"], s["frame"], "OK" if ok else "DIFFERS"))
        for ln in lines:
            print("  " + ln)
    sys.exit(0 if allok else 1)


if __name__ == "__main__":
    main(*sys.argv[1:])
