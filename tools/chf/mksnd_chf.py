#!/usr/bin/env python3
"""mksnd_chf.py -- FujiBarrage's sounds for the Channel F's three tones: the
effects, drawn by hand for them, and the tunes (music/*.txt through
tools/mus.py, as every port has them).

    mksnd_chf.py OUT.inc [--list]

The console: port 5 bits 7-6, $40 1 kHz, $80 500 Hz (an octave down), $C0
a ~120 Hz buzz; no volume, no noise. A tone is struck only when the bits
change, then decays (MAME: half-life 9 ms, so a jiffy later it is at 28%,
two at 8%): a held tone is struck again every jiffy (60 pips a second, the
loudest), every other one (softer), or once (a pip). A sound is a list of
steps (sound.inc): tone, jiffies, strike (0 once, 1 every jiffy, 2 every
other); a new step strikes; tone 0 is silence (cut at once). Jiffies 0
ends it; tone $FF, then an address (big-endian), goes on there (the
title's loop). The cart's clock times them: PAL plays them in time.

The tunes, a jiffy at a time: the kick on the buzz (both its jiffies); the
melody (ch2) struck at each note, held ones struck every other jiffy, high
or mid by its pitch against the tune's middle; the snare a high pip where
the melody is quiet; the bass (ch1's notes) a buzz pip on its own. Then
runs of the same tone and strike pattern become steps.

OUT.inc: the ids (SND_* 0-15 are src/proto.h's, so not here; SND_BOOM0-3
a boom's sizes; UI_*; MUS_*), SNDTAB (each id's steps, hi, lo), SNDPRI
(a sound cuts only one of its priority or lower: 0 the talk's letters, 1
the ticks, 2 the tunes, 3 the rest), the steps.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(TOP, "tools"))
import mus  # noqa: E402

HI, MID, BUZZ, OFF = 0x40, 0x80, 0xC0, 0x00
ONCE, EVERY, OTHER = 0, 1, 2


def s(tone, j, strike=ONCE):
    return (tone, j, strike)


def rest(j):
    return (OFF, j, ONCE)


def pips(tone, gaps):
    """single strikes of tone, each followed by its silence (a crackle, a
    thinning rumble)"""
    out = []
    for g in gaps:
        out += [s(tone, 1), rest(g)]
    return out


def trill(a, b, n, j=1):
    """a and b struck in turn, j jiffies each, n times both"""
    out = []
    for _ in range(n):
        out += [s(a, j), s(b, j)]
    return out


def boom(core, tail, gaps):
    """a boom: the buzz every jiffy, every other, then pips thinning out"""
    return [s(BUZZ, core, EVERY), s(BUZZ, tail, OTHER)] + pips(BUZZ, gaps)


EFFECTS = {
    # SND_* (src/proto.h's order)
    "fire": [s(BUZZ, 2, EVERY), s(MID, 2, EVERY), s(HI, 3, EVERY), s(HI, 4, OTHER)],
    "boom": boom(10, 10, (2, 3, 4)),
    "nuke": [s(HI, 2, EVERY), s(MID, 2, EVERY), s(BUZZ, 40, EVERY), s(BUZZ, 40, OTHER)]
            + pips(BUZZ, (2, 2, 3, 3, 4, 5, 6, 8, 10)),
    "napalm": [s(HI, 1), rest(2), s(BUZZ, 1), rest(1), s(HI, 1), rest(3), s(MID, 1),
               rest(1), s(BUZZ, 1), rest(2), s(HI, 1), rest(1), s(HI, 1), rest(4),
               s(BUZZ, 1), rest(2), s(MID, 1), rest(3), s(HI, 1), rest(5), s(BUZZ, 1)],
    "laser": trill(HI, MID, 6) + trill(MID, BUZZ, 3),
    "plasma": [s(HI, 4, EVERY), s(MID, 4, EVERY), s(HI, 4, EVERY), s(MID, 4, EVERY),
               s(HI, 4, OTHER)],
    "dig": [s(BUZZ, 2), s(MID, 2)] * 4,
    "roll": [s(MID, 3), rest(1)] * 6,
    "dirt": [s(BUZZ, 4, EVERY), s(BUZZ, 8, OTHER)],
    "shield": [s(HI, 2, EVERY), s(MID, 2, EVERY), s(HI, 6, OTHER)],
    "deflect": [s(HI, 1), s(MID, 1), s(HI, 1), rest(1), s(HI, 4, EVERY)],
    "chute": [s(MID, 6, OTHER), rest(2), s(MID, 6, OTHER)],
    "death": [s(HI, 6, EVERY), s(MID, 8, EVERY), s(BUZZ, 14, EVERY), s(BUZZ, 10, OTHER)]
             + pips(BUZZ, (2, 3, 5)),
    "bounce": [s(MID, 2, EVERY), s(HI, 2, EVERY), rest(1), s(HI, 1)],
    "thunder": [s(HI, 1), s(BUZZ, 3, EVERY), rest(2), s(BUZZ, 20, EVERY), s(BUZZ, 20, OTHER)]
               + pips(BUZZ, (3, 4, 6, 8)),
    "tick": [s(HI, 1)],
    # a boom's sizes 0-3 (OP_SOUND SND_BOOM's size)
    "boom0": boom(4, 4, (2,)),
    "boom1": boom(8, 8, (2, 3)),
    "boom2": boom(14, 12, (2, 3, 4, 5)),
    "boom3": boom(22, 20, (2, 3, 4, 5, 6, 8)),
    # the cues
    "ui_tick": [s(HI, 1)],
    "ui_select": [s(MID, 2), s(HI, 2)],
    "ui_go": [s(MID, 4, EVERY), s(HI, 8, EVERY)],
    "ui_error": [s(BUZZ, 6, EVERY), rest(3), s(BUZZ, 8, EVERY)],
    "ui_turn": [s(MID, 4, EVERY), rest(1), s(MID, 4, EVERY), rest(1), s(HI, 10, EVERY),
                s(HI, 6, OTHER)],
    "ui_talk": [s(HI, 1)],
}
IDS = ["fire", "boom", "nuke", "napalm", "laser", "plasma", "dig", "roll", "dirt", "shield",
       "deflect", "chute", "death", "bounce", "thunder", "tick",
       "boom0", "boom1", "boom2", "boom3",
       "ui_tick", "ui_select", "ui_go", "ui_error", "ui_turn", "ui_talk"]
TUNES = ["title", "win", "over", "shop"]
EXTRA = ["SND_BOOM0", "SND_BOOM1", "SND_BOOM2", "SND_BOOM3",
         "UI_TICK", "UI_SELECT", "UI_GO", "UI_ERROR", "UI_TURN", "UI_TALK",
         "MUS_TITLE", "MUS_WIN", "MUS_OVER", "MUS_SHOP"]
PRI = {"ui_talk": 0, "ui_tick": 1, "tick": 1}
TUNEPRI, FXPRI = 2, 3


# ---- the tunes ---------------------------------------------------------------
def voice(e1, e2, mid):
    """one jiffy: (tone, struck)"""
    d = e1[0] if e1 else None
    k = e1[2] if e1 else 0
    if d == "K" and k < 2:
        return (BUZZ, True)
    if e2 is not None:
        kind, note, j = e2
        tone = HI if note >= mid else MID
        if j == 0:
            return (tone, True)
        if kind == "blip":
            return (tone, False) if j < 3 else (OFF, False)
        if kind in ("arp", "arpmaj", "warble"):
            return (tone, j % 2 == 0)       # (a note every other jiffy)
        if kind in ("sweep", "rise"):
            return (tone, j % 2 == 0)
        return (tone, j % 2 == 0)           # held: every other jiffy
    if d == "S" and k == 0:
        return (HI, True)
    if d == "bass" and k == 0:
        return (BUZZ, True)
    return (None, False)                    # nothing new: what rings decays


def tune_jiffies(path):
    song = mus.parse(path)
    j1, j2, starts = [], [], []
    for name in song.order:
        starts.append(len(j1))
        j1 += mus.frames(song, name, 1)
        j2 += mus.frames(song, name, 2)
    notes = sorted(e[1] for e in j2 if e is not None)
    mid = notes[len(notes) // 2] if notes else 72
    v = [voice(a, b, mid) for a, b in zip(j1, j2)]
    loop = starts[song.loop] if song.loop is not None else None
    return v, loop


def steps_of(jiffies):
    """per-jiffy (tone, struck) -> steps; a jiffy not struck holds what rings
    (a step of OFF if nothing rang). Returns the steps and, for each jiffy
    that starts a step, its index (the loop's target)."""
    out, at = [], {}
    i, n = 0, len(jiffies)
    while i < n:
        tone, st = jiffies[i]
        at[i] = len(out)
        if not st:                          # silence (or a tail): up to a strike
            j = i
            while j < n and not jiffies[j][1]:
                j += 1
            out.append(rest(j - i))
            i = j
            continue
        if i + 1 < n and jiffies[i + 1] == (tone, True):         # every jiffy
            j = i
            while j < n and jiffies[j] == (tone, True):
                j += 1
            out.append(s(tone, j - i, EVERY))
            i = j
            continue
        j = i + 1                           # every other: struck, held, struck...
        while (j + 1 < n and not jiffies[j][1] and jiffies[j][0] in (tone, None)
               and jiffies[j + 1] == (tone, True)):
            j += 2
        if j > i + 1:
            out.append(s(tone, j - i, OTHER))
            i = j
            continue
        j = i + 1                           # once, then what rings decays
        while j < n and not jiffies[j][1] and jiffies[j][0] in (tone, None):
            j += 1
        out.append(s(tone, j - i))
        i = j
    big = []
    idx = {}
    for k, (t, j, st) in enumerate(out):    # (a byte's jiffies at most)
        idx[k] = len(big)
        while j > 255:
            big.append((t, 255, st))
            j -= 255
        big.append((t, j, st))
    return big, {i: idx[k] for i, k in at.items()}


def main():
    out = sys.argv[1]
    sounds, loops = {}, {}
    for k in IDS:
        sounds[k] = EFFECTS[k]
    for t in TUNES:
        jf, loop = tune_jiffies(os.path.join(TOP, "music", t + ".txt"))
        st, at = steps_of(jf)
        sounds["mus_" + t] = st
        if loop is not None:
            while loop not in at:
                loop += 1
            loops["mus_" + t] = at[loop]
    order = IDS + ["mus_" + t for t in TUNES]
    L = ["; GENERATED by tools/chf/mksnd_chf.py from its effects and music/*.txt -- do not edit."]
    for i, n in enumerate(EXTRA):
        L.append("%-10s EQU %d" % (n, 16 + i))
    L.append("NSND       EQU %d" % len(order))
    L.append("SNDTAB:")
    for k in order:
        L.append("        DB (S_%s)>>8, (S_%s)&0FFH" % (k.upper(), k.upper()))
    L.append("SNDPRI:")
    pri = [PRI.get(k, TUNEPRI if k.startswith("mus_") else FXPRI) for k in order]
    L.append("        DB " + ", ".join(str(p) for p in pri))
    total = 0
    for k in order:
        L.append("S_%s:" % k.upper())
        for i, (t, j, st) in enumerate(sounds[k]):
            if k in loops and i == loops[k]:
                L.append("S_%s_L:" % k.upper())
            L.append("        DB 0%02XH, %d, %d" % (t, j, st))
        if k in loops:
            L.append("        DB 0FFH, (S_%s_L)>>8, (S_%s_L)&0FFH" % (k.upper(), k.upper()))
        else:
            L.append("        DB 0, 0, 0")
        total += 3 * len(sounds[k]) + 3
    open(out, "w").write("\n".join(L) + "\n")
    if "--list" in sys.argv:
        for k in order:
            secs = sum(j for _, j, _ in sounds[k]) / 60.0
            print("%-12s %4d steps %6.2f s%s" % (k, len(sounds[k]), secs,
                                                   "  (loops)" if k in loops else ""))
        print("bytes: %d" % (total + 3 * len(order)))


if __name__ == "__main__":
    main()
