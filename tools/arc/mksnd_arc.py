#!/usr/bin/env python3
"""mksnd_arc.py -- FujiBarrage's sounds for the Arcadia 2001's one voice
(the 2637 UVI): the effects, designed for the chip, and the tunes (music/*.txt
through tools/mus.py, as every port has them), for p_snd.inc's page and the
vblank's player (fixa.inc VBN*).

    mksnd_arc.py SNDEQU.inc SNDTAB.inc SNDDAT.inc SNDTIT.inc [--list]

The chip: one voice, a tone, a noise or both (b3 tone, b4 noise of $18FE;
b0-2 the level, 7 loudest); its divider p (1-127, $18FD b0-6) toggles the
square every p lines: f = 15734 / (2 p) Hz (MAME's model, refined against
the hardware by its sound's author; FujiBlast's table said p + 1). The noise
is a 9-bit LFSR clocked by the same divider: a small p hisses, a large one
rumbles. The divider is coarse up high (p 10 is 787 Hz, p 9 874 Hz), so the
tunes go two octaves down (every note within 36 cents).

A sound is a list of notes, 3 bytes each: its jiffies (1-127; 1/60 s, the
cart's clock, so PAL plays them in time), the pitch, the VOLUME byte (0: a
rest). A 0 ends it; $FF, then an address (big-endian), goes on there (the
title's loop). The vblank plays them (fixa.inc): a jiffy or more a frame.

SNDEQU.inc: the ids (SND_* 0-15 as src/proto.h; SND_BOOM0-3 a boom's
sizes; UI_*; MUS_*). SNDTAB.inc (fixed B's): each id's first note (SNDTH,
SNDTL: addresses in the sound page) and its priority (SNDPRT: a sound only cuts one of its priority
or lower; 0 the talk's letters, 1 the ticks, 2 the tunes, 3 the rest).
SNDDAT.inc: the notes, from SNDBASE in the sound page (PG_SND); SNDTIT.inc
the title's, from $0800 in a page of its own (PG_SNDT: it is most of 2K). An
address's b7 says which (the 2650's indirect addresses are 15 bits: b7 of
the high byte is the player's alone).
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import mus  # noqa: E402

LINE = 15734.0                  # NTSC lines a second (PAL's 15625: 12 cents flat)
TONE, NOISE, BOTH = 0x08, 0x10, 0x18
SNDBASE = 0x0880                # the sound page's notes from here (its code below)
TITBASE = 0x0800                # the title's page: notes only
TITLEPG = 0x8000                # (an address's b15: that page)
PAGEEND = 0x0FFF                # (the page's id byte)
MAXJ = 127                      # a note's jiffies (the player's count is signed)
SHIFT = -24                     # the tunes' melody, two octaves down


def pitch(hz):
    return max(1, min(127, round(LINE / (2 * hz))))


# ---- the effects: (jiffies, p, mode, level) -------------------------------------------------
def t(p, j, lv, mode=TONE):
    return (j, p, mode, lv)


def nz(p, j, lv):
    return (j, p, NOISE, lv)


def rest(j):
    return (j, 0, 0, 0)


def decay(p, j, l0, l1, mode=NOISE, p1=None):
    """the level from l0 down to l1 over j jiffies (p drifting to p1)"""
    out, n = [], l0 - l1 + 1
    for i in range(n):
        lv = l0 - i
        q = p if p1 is None else round(p + (p1 - p) * i / max(1, n - 1))
        jj = j * (i + 1) // n - j * i // n
        if jj:
            out.append((jj, q, mode, lv))
    return out


def sweep(p0, p1, n, l0, l1, j=1, mode=TONE):
    """n steps of j jiffies, the divider p0 to p1 (geometric: even in pitch)"""
    out = []
    for i in range(n):
        f = i / max(1, n - 1)
        out.append((j, round(p0 * (p1 / p0) ** f), mode, round(l0 + (l1 - l0) * f)))
    return out


def boom(p0, p1, hit, j, l0=7):
    return [nz(p0, hit, l0)] + decay(p1, j, l0 - 1, 1)


EFFECTS = {
    # SND_* (src/proto.h's order)
    "fire": [t(110, 2, 7), (2, 60, BOTH, 7)] + decay(40, 14, 6, 1),
    "boom": boom(60, 80, 4, 30),                # (OP_SOUND's size picks a BOOMn)
    "nuke": [nz(127, 8, 7), (6, 100, BOTH, 7), nz(127, 20, 7)]
            + decay(127, 100, 7, 1, NOISE, 120),
    "napalm": [nz(8, j, lv) for j, lv in ((2, 6), (2, 2), (1, 7), (3, 3), (2, 6), (2, 1),
                                           (1, 5), (3, 2), (2, 5), (2, 1), (2, 4), (3, 2),
                                           (2, 3), (2, 1))],
    "laser": sweep(3, 27, 16, 7, 4),
    "plasma": [t(p, 2, lv) for p, lv in ((13, 7), (16, 7), (13, 6), (16, 6), (12, 5),
                                         (16, 5), (12, 4), (17, 3))],
    "dig": [(2, p, BOTH, lv) for p, lv in ((56, 6), (60, 3), (56, 6), (60, 3),
                                           (56, 5), (60, 2), (56, 4), (60, 2))],
    "roll": [t(p, 3, lv) for p, lv in ((52, 5), (57, 5), (52, 4), (58, 4), (53, 3), (59, 2))],
    "dirt": [nz(20, 4, 5)] + decay(30, 12, 4, 1),
    "shield": [t(9, 2, 6), t(11, 3, 6), t(14, 3, 5), t(18, 3, 4), t(18, 3, 2)],
    "deflect": sweep(11, 5, 8, 6, 5),
    "chute": [nz(6, 6, 2), nz(6, 8, 3), nz(6, 4, 2), nz(6, 4, 1)],
    "death": [t(p, 3, lv) for p, lv in ((17, 7), (21, 7), (25, 6), (29, 6), (35, 6),
                                        (41, 5), (49, 5), (58, 4))]
             + [nz(60, 4, 6)] + decay(70, 12, 5, 1),
    "bounce": [t(28, 2, 6), t(21, 2, 5), t(25, 2, 4), t(25, 2, 2)],
    "thunder": [nz(2, 3, 7), nz(4, 2, 4), nz(2, 3, 7), nz(90, 4, 6)]
               + decay(110, 66, 6, 1, NOISE, 127),
    "tick": [t(10, 1, 4)],
    # a boom's sizes 0-3
    "boom0": boom(30, 50, 3, 15),
    "boom1": boom(60, 80, 4, 30),
    "boom2": boom(90, 110, 5, 45),
    "boom3": boom(120, 127, 6, 70),
    # the cues
    "ui_tick": [t(15, 2, 4)],
    "ui_select": [t(13, 3, 5), t(13, 2, 3)],
    "ui_go": [t(13, 4, 6), t(10, 4, 6), t(8, 6, 6), t(8, 2, 4), t(8, 2, 2)],
    "ui_error": [(6, 56, BOTH, 6), rest(2), (8, 60, BOTH, 5), (4, 60, BOTH, 3)],
    "ui_turn": [t(pitch(262), 5, 5), t(pitch(330), 5, 6), t(pitch(392), 5, 6),   # a fanfare,
                t(pitch(523), 8, 7)] + decay(pitch(523), 13, 6, 2, TONE),        #  loudest last
    "ui_talk": [t(11, 1, 2), t(14, 1, 2)],
}
# the ids: SND_* 0-15, then the dense ones fixa.inc and the pages name
IDS = ["fire", "boom", "nuke", "napalm", "laser", "plasma", "dig", "roll", "dirt", "shield",
       "deflect", "chute", "death", "bounce", "thunder", "tick",
       "boom0", "boom1", "boom2", "boom3",
       "ui_tick", "ui_select", "ui_go", "ui_error", "ui_turn", "ui_talk"]
TUNES = ["title", "win", "over", "shop"]
PRI = {"ui_talk": 0, "ui_tick": 1}
TUNEPRI, FXPRI = 2, 3


# ---- the tunes: one voice -------------------------------------------------------------------
def vol(k, steps):
    for until, v in steps:
        if k < until:
            return v
    return steps[-1][1]


def level(v):
    """the Atari's volume (0-15, the masters' instruments) as the 2637's 0-7"""
    return 0 if v <= 0 else max(1, min(7, round(v * 0.7)))


ACLOCK = 63921.0


def melody(ev):
    """(Hz, mode, level) for ch2's event: two octaves down"""
    kind, note, k = ev
    if kind == "blip":
        return (mus.hz(note + SHIFT), TONE, level(vol(k, ((2, 6), (4, 3), (999, 0)))))
    if kind == "tone":
        return (mus.hz(note + SHIFT), TONE, level(vol(k, ((2, 6), (999, 4)))))
    if kind in ("arp", "arpmaj", "warble"):
        return (mus.hz(note + SHIFT), TONE, level(5))
    k &= ~1                             # slides: a step every other jiffy (the Atari's)
    pure = round(ACLOCK / (2 * mus.hz(note))) - 1
    if kind == "sweep":
        a = min(255, round((pure + 1) * 1.08 ** k) - 1)
        v = max(1, 6 - k // 4)
    else:
        a = max(8, round((pure + 1) * 0.96 ** k) - 1)
        v = vol(k, ((24, 5), (999, 6)))
    return (ACLOCK / (2 * (a + 1)) * 2 ** (SHIFT / 12), TONE, level(v))


def jiffy(e1, e2):
    """one jiffy of the tune: (p, VOLUME byte). The kick and the snare's hit
    cut through the melody for their jiffies; the hats only when it rests;
    the bass when it rests, an octave or two up (the divider's lowest is 62 Hz)"""
    d = e1[0] if e1 else None
    k = e1[2] if e1 else 0
    if d == "K":
        return ((pitch(150), TONE | 7), (pitch(70), TONE | 6))[min(k, 1)]
    if d == "S" and k == 0:
        return (3, NOISE | 6)
    if e2 is not None:
        hz, mode, lv = melody(e2)
        return (pitch(hz), mode | lv) if lv else (0, 0)
    if d == "S":
        return (3, NOISE | 3)
    if d == "H":
        return (1, NOISE | 3) if k == 0 else (0, 0)
    if d == "bass":
        hz = mus.hz(e1[1] + 12)
        while hz < 64:
            hz *= 2
        return (pitch(hz), TONE | level(vol(k, ((2, 10), (999, 6)))))
    return (0, 0)


def tune_notes(src):
    """the tune's notes and the note its loop goes back to (None: it ends)"""
    song = mus.parse(src)
    notes, loopat = [], None
    for bi, pat in enumerate(song.order):
        e1, e2 = mus.frames(song, pat, 1), mus.frames(song, pat, 2)
        if len(e1) != len(e2):
            raise SystemExit("%s: pattern %s's channels differ in length" % (src, pat))
        if song.loop is not None and bi == song.loop:
            loopat = len(notes)
        fresh = True                    # (a note never runs into the loop's bar)
        for a, b in zip(e1, e2):
            p, v = jiffy(a, b)
            if v == 0:
                p = 0
            if not fresh and notes[-1][1] == p and notes[-1][2] == v and notes[-1][0] < MAXJ:
                notes[-1][0] += 1
            else:
                notes.append([1, p, v])
            fresh = False
    return [tuple(n) for n in notes], loopat, song


def main():
    equ, tab, dat, tit = sys.argv[1:5]
    lines, at, addr = [], {}, SNDBASE
    seen = {}
    for name in IDS:
        notes = tuple((j, p if lv else 0, (m | lv) if lv else 0) for j, p, m, lv in EFFECTS[name])
        if notes in seen:               # (boom is boom1)
            at[name] = seen[notes]
            continue
        seen[notes] = at[name] = addr
        lines.append("SE_%s:  ; $%04X" % (name, addr))
        for j, p, v in notes:
            assert 1 <= j <= MAXJ and 0 <= p <= 127 and 0 <= v <= 0x1F, (name, j, p, v)
            lines.append("        DB      %d,%d,$%02X" % (j, p, v))
        lines.append("        DB      0")
        addr += 3 * len(notes) + 1
    fxbytes = addr - SNDBASE
    info, tlines = [], []
    for name in TUNES:
        if name == "title":             # (its own page)
            base, lines, addr, save = TITLEPG | TITBASE, tlines, TITLEPG | TITBASE, (lines, addr)
        else:
            save = None
        src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "music",
                           name + ".txt")
        notes, loopat, song = tune_notes(src)
        at["mus_" + name] = addr
        lines.append("ST_%s:  ; $%04X" % (name, addr))
        for j, p, v in notes:
            lines.append("        DB      %d,%d,$%02X" % (j, p, v))
        if loopat is None:
            lines.append("        DB      0")
            addr += 3 * len(notes) + 1
        else:
            to = at["mus_" + name] + 3 * loopat      # (b15 kept: the same page)
            lines.append("        DB      $FF,$%02X,$%02X" % (to >> 8, to & 0xFF))
            addr += 3 * len(notes) + 3
        jiff = sum(n[0] for n in notes)
        info.append("%s: %d notes, %.1f s%s" % (name, len(notes), jiff / 60,
                                                 "" if loopat is None else " (loops)"))
        if save:
            if (addr & 0x7FFF) > PAGEEND:
                raise SystemExit("mksnd_arc: the title runs to $%04X, past its page" % addr)
            tend = addr & 0x7FFF
            lines, addr = save
    if addr > PAGEEND:
        raise SystemExit("mksnd_arc: the notes run to $%04X, past the page" % addr)
    allids = IDS + ["mus_" + n for n in TUNES]
    eq = ["; GENERATED by tools/arc/mksnd_arc.py (the effects; the tunes from music/*.txt):",
          "; edit those, not this. fixa.inc plays them; p_snd.inc holds the notes.",
          "SNDBASE EQU     $%04X" % SNDBASE]
    names = {"boom0": "SND_BOOM0", "boom1": "SND_BOOM1", "boom2": "SND_BOOM2",
             "boom3": "SND_BOOM3"}
    for i, n in enumerate(allids):
        if i < 16:
            continue                    # (SND_*: proto.inc's)
        eq.append("%-9s EQU    %d" % (names.get(n, n.upper()), i))
    eq.append("NSNDID    EQU    %d" % len(allids))
    tb = ["; GENERATED by tools/arc/mksnd_arc.py -- do not edit."]
    tb.append("SNDTH:  DB      " + ",".join("$%02X" % (at[n] >> 8) for n in allids))
    tb.append("SNDTL:  DB      " + ",".join("$%02X" % (at[n] & 0xFF) for n in allids))
    tb.append("SNDPRT: DB      " + ",".join(
        str(PRI.get(n, TUNEPRI if n.startswith("mus_") else FXPRI)) for n in allids))
    open(equ, "w").write("\n".join(eq) + "\n")
    open(tab, "w").write("\n".join(tb) + "\n")
    open(dat, "w").write("; GENERATED by tools/arc/mksnd_arc.py -- do not edit.\n"
                         + "\n".join(lines) + "\n")
    open(tit, "w").write("; GENERATED by tools/arc/mksnd_arc.py -- do not edit.\n"
                         + "\n".join(tlines) + "\n")
    if "--list" in sys.argv:
        print("mksnd_arc: effects %d bytes; %s; notes $%04X-$%04X (%d free), the title's "
              "$%04X-$%04X (%d free)" % (fxbytes, "; ".join(info), SNDBASE, addr - 1,
                                         PAGEEND - addr, TITBASE, tend - 1, PAGEEND - tend))


if __name__ == "__main__":
    main()
