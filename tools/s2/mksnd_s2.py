#!/usr/bin/env python3
"""mksnd_s2.py -- FujiBarrage's sounds for the Studio II's one fixed tone
(the beeper: Q gates it; the ISR keeps Q on while RD.0 counts down a
jiffy at a time). Every sound is a rhythm: (jiffies on, jiffies off)
pairs, a 0 at the end (src/studio2/src/sound.inc plays them). FujiBlast's
tools/s2/mksnd_s2.py, for FujiBarrage's ids (the Arcadia's
tools/arc/mksnd_arc.py: the same ids, priorities and tunes).

    mksnd_s2.py OUT.inc [--list]

The effects are drawn here by hand to the other ports' shapes: a boom's
size is its length, a nuke rumbles long, napalm crackles, a death slows.
The tunes (music/*.txt through tools/mus.py) keep their melody's rhythm
(ch2; ch1's kicks where the melody rests): each note rings its length less
a 2-jiffy gap, so repeated notes stay apart (a gap under 2 jiffies isn't
heard: the beep runs on into the next).

OUT.inc: the ids' EQUs (SND_* 0-15 as src/proto.h, SND_BOOM0-3, UI_*,
MUS_*), SNDTAB (each id's rhythm, DW), SNDPRI (its priority: a sound cuts
only one of its priority or lower; 0 the talk, 1 the ticks, 2 the tunes,
3 the rest) and the rhythms.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import mus  # noqa: E402

EFFECTS = {
    # SND_* (src/proto.h's order)
    "fire":    [(3, 2), (2, 2), (1, 3), (1, 0)],
    "boom":    [(4, 2), (3, 2), (2, 0)],                 # (OP_SOUND's size picks a BOOMn)
    "nuke":    [(24, 3), (12, 3), (8, 4), (6, 4), (4, 5), (3, 6), (2, 8), (1, 0)],
    "napalm":  [(1, 2), (1, 3), (2, 2), (1, 4), (1, 2), (2, 3), (1, 5), (1, 0)],
    "laser":   [(2, 2), (2, 2), (2, 2), (2, 2), (8, 0)],
    "plasma":  [(1, 2), (2, 2), (1, 2), (2, 2), (1, 2), (3, 0)],
    "dig":     [(1, 3), (1, 3), (1, 3), (1, 0)],
    "roll":    [(2, 4), (2, 4), (2, 0)],
    "dirt":    [(3, 2), (1, 0)],
    "shield":  [(1, 2), (2, 2), (4, 0)],
    "deflect": [(1, 2), (1, 0)],
    "chute":   [(1, 6), (1, 0)],
    "death":   [(3, 2), (3, 2), (4, 2), (5, 3), (6, 3), (10, 0)],
    "bounce":  [(2, 3), (1, 0)],
    "thunder": [(1, 2), (1, 2), (14, 3), (6, 4), (3, 0)],
    "tick":    [(1, 0)],
    # a boom's sizes 0-3
    "boom0":   [(3, 2), (1, 0)],
    "boom1":   [(4, 2), (3, 2), (2, 0)],
    "boom2":   [(6, 2), (4, 2), (3, 3), (2, 0)],
    "boom3":   [(10, 2), (7, 3), (5, 3), (3, 4), (2, 0)],
    # the cues
    "ui_tick":   [(1, 0)],
    "ui_select": [(2, 0)],
    "ui_go":     [(3, 2), (3, 2), (8, 0)],
    "ui_error":  [(6, 3), (12, 0)],
    "ui_turn":   [(4, 2), (4, 2), (4, 2), (12, 0)],      # the fanfare's rhythm
    "ui_talk":   [(1, 0)],
}
IDS = ["fire", "boom", "nuke", "napalm", "laser", "plasma", "dig", "roll", "dirt", "shield",
       "deflect", "chute", "death", "bounce", "thunder", "tick",
       "boom0", "boom1", "boom2", "boom3",
       "ui_tick", "ui_select", "ui_go", "ui_error", "ui_turn", "ui_talk"]
TUNES = ["title", "win", "over", "shop"]
PRI = {"ui_talk": 0, "ui_tick": 1}
TUNEPRI, FXPRI = 2, 3
MAXJ = 255


def rhythm(name):
    """a tune's (on, off) pairs: ch2's notes ring their length less a gap
    (12 jiffies at most); ch1's kicks (2 jiffies) and snares (1) pip where
    the melody is quiet, 2 jiffies clear either side"""
    song = mus.parse(os.path.join(HERE, "..", "..", "music", name + ".txt"))
    e1s, e2s = [], []
    for pat in song.order:
        e1s += mus.frames(song, pat, 1)
        e2s += mus.frames(song, pat, 2)
    n = len(e2s)
    beep = [False] * (n + 8)
    starts = [i for i, b in enumerate(e2s) if b is not None and b[2] == 0]
    for i, st in enumerate(starts):
        nxt = starts[i + 1] if i + 1 < len(starts) else st + 8
        end = st + 1
        while end < nxt and e2s[end] is not None if end < n else False:
            end += 1                    # (the note's own length: to a rest)
        ln = end - st
        on = max(1, min(ln - 2, 12)) if ln >= 3 else 1
        for j in range(st, st + on):
            beep[j] = True
    for i, a in enumerate(e1s):
        if a is None or a[2] != 0 or a[0] not in ("K", "S"):
            continue
        ln = 2 if a[0] == "K" else 1
        lo, hi = max(0, i - 2), min(n + 8, i + ln + 2)
        if not any(beep[lo:hi]):
            for j in range(i, i + ln):
                beep[j] = True
    out, i = [], 0
    while i < len(beep) and not beep[i]:
        i += 1
    while i < len(beep):
        on = 0
        while i < len(beep) and beep[i] and on < MAXJ:
            on += 1
            i += 1
        off = 0
        while i < len(beep) and not beep[i]:
            off += 1
            i += 1
        while off > MAXJ:               # (a long rest: a 1-jiffy pip carries it)
            out.append((on, MAXJ))
            on, off = 1, off - MAXJ - 1
        out.append((on, off))
    out[-1] = (out[-1][0], 0)
    return out, n


def heard(r):
    """the pairs to store for the rhythm r to be heard: the beeper holds a
    beep a jiffy past its count (MAME: on n rings n + 1, its gap one
    less), so a count is one less and its gap one more (1 stays 1)"""
    return [(on - 1, off + 1) if on > 1 and off else (on - 1, 0) if on > 1 else (on, off)
            for on, off in r]


def main():
    dst = sys.argv[1]
    seqs = [(k, EFFECTS[k]) for k in IDS]
    info = []
    for t in TUNES:
        r, n = rhythm(t)
        seqs.append(("mus_" + t, r))
        info.append("%s: %d notes, %.1f s" % (t, len(r), n / 60))
    lines = ["; GENERATED by tools/s2/mksnd_s2.py: the sounds as rhythms (jiffies on, off)", ""]
    for i, (k, _) in enumerate(seqs):
        if i >= 16:                     # (SND_* 0-15: proto.inc's)
            nm = k.upper() if k.startswith(("ui_", "mus_")) else "SND_" + k.upper()
            lines.append("%-10s EQU     %d" % (nm, i))
    lines.append("N_SOUNDS   EQU     %d" % len(seqs))
    lines.append("")
    lines.append("SNDTAB:")
    for k, _ in seqs:
        lines.append("        DW      SR_%s" % k)
    lines.append("SNDPRI:")
    pri = [PRI.get(k, TUNEPRI if k.startswith("mus_") else FXPRI) for k, _ in seqs]
    for i in range(0, len(pri), 16):
        lines.append("        DB      " + ", ".join(str(p) for p in pri[i:i + 16]))
    seen, size = {}, 0
    for k, r in seqs:
        key = tuple(r)
        if key in seen:
            lines.append("SR_%s EQU SR_%s" % (k, seen[key]))
            continue
        seen[key] = k
        lines.append("SR_%s:" % k)
        flat = [v for p in heard(r) for v in p] + [0]
        for i in range(0, len(flat), 16):
            lines.append("        DB      " + ", ".join(str(v) for v in flat[i:i + 16]))
        size += len(flat)
    at = lines.index("SNDTAB:")              # (the data kept out of $x000-$x3FF)
    lines.insert(at, "        DFIT    %d" % (3 * len(seqs) + size))
    with open(dst, "w") as f:
        f.write("\n".join(lines) + "\n")
    if "--list" in sys.argv:
        print("%d bytes of rhythms; %s" % (size, "; ".join(info)))


if __name__ == "__main__":
    main()
