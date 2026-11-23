#!/usr/bin/env python3
"""mksnd_astro.py -- FujiBarrage's sounds for the Astrocade's custom I/O chip
(src/astrocade/src/p_snd.inc): the effects, designed for the chip, and the
tunes (music/*.txt through tools/mus.py, as every port has them).

    mksnd_astro.py OUT.inc

The chip: tones A, B and C are square waves dividing one master
oscillator, f = 1789773 / (2 (MO+1) (T+1)); the noise is mixed in at its
own volume, and it can modulate the master (every tone then roars). MO is
SNDMO (26: about 130 Hz to 16 kHz, a tune's notes within 30 cents). A
noise step may set the master for as long as it lasts (a deep thump on C)
and turn the modulation on: only effects do, on the field, where no tune
plays (the whistle on A is retuned with it, briefly).

The field's channels: A the falling shell's whistle (the engine's own:
WHIST), B a tone effect or a cue, C a low tone (a thump, a roll), N the
noise. A text screen's tune: its melody on A, its bass (and the kick) on
C, the snare and hat on N; cues go on B, so a tick in the shop never cuts
a tune.

An effect's channel list: steps of 4 bytes, T (the noise's: 0, or the
master for the step: MO / 2, + 80H modulated), volume, frames (0 ends),
decay (the volume drops 1 every that many frames, to 1; 0 holds it).

A tune: bars (a pattern each: its jiffies, the lists of A, B, C, N), the
title loops from its `loop` bar, the jingles end. A tune's list: steps of
3 bytes, the note (MIDI; 0 a rest; a raw step's: T), the instrument (I_*
below: the engine voices it each jiffy, as tools/mkmus_intv.py does), the
frames. The bass sits an octave or two up, as the chip's lowest tone is
about 130 Hz at this master.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import mus  # noqa: E402

CLOCK = 1789773
MO = 26
CH = {"A": 0, "B": 1, "C": 2, "N": 3}

# the instruments a tune step names (p_snd.inc's MVOICE has the same order)
I_RAW, I_BASS, I_BLIP, I_TONE, I_ARP, I_ARPMAJ, I_WARBLE, I_KICK, I_SNARE, I_HAT = range(10)
INSTR = {"bass": I_BASS, "blip": I_BLIP, "tone": I_TONE, "arp": I_ARP, "arpmaj": I_ARPMAJ,
         "warble": I_WARBLE, "K": I_KICK, "S": I_SNARE, "H": I_HAT}


def per(hz, mo=MO):
    """the tone register for hz at master mo (clamped to the chip's)"""
    return max(1, min(255, round(CLOCK / (2 * (mo + 1) * hz)) - 1))


# ---- the effects -------------------------------------------------------------------------
def tone(hz, vol, frames, decay=0, mo=MO):
    return (per(hz, mo), vol, frames, decay)


def sweep(f0, f1, n, v0, v1, frames=1, mo=MO):
    """n steps from f0 to f1 (geometric), the volume v0 to v1"""
    out = []
    for i in range(n):
        t = i / max(1, n - 1)
        out.append(tone(f0 * (f1 / f0) ** t, round(v0 + (v1 - v0) * t), frames, 0, mo))
    return out


def noise(vol, frames, decay=0, mo=None, mod=False):
    t = 0 if mo is None else (mo // 2) | (0x80 if mod else 0)
    return (t, vol, frames, decay)


DEEP = 120          # a thump's master: C's tones down to about 30 Hz

EFFECTS = {
    # SND_* (src/proto.h), then the boom's sizes, then the cues
    "fire": {        # the launch: a deep thump under a noise burst
        "N": [noise(15, 2, 0, DEEP), noise(12, 3, 1, DEEP), noise(8, 6, 2, DEEP)],
        "C": sweep(90, 40, 8, 15, 6, 1, DEEP)},
    "boom0": {       # a small blast
        "N": [noise(14, 3), noise(11, 12, 2)],
        "C": sweep(140, 90, 6, 12, 4, 2)},
    "boom1": {
        "N": [noise(15, 3, 0, DEEP), noise(13, 22, 2, DEEP)],
        "C": sweep(80, 35, 10, 15, 4, 2, DEEP)},
    "boom2": {
        "N": [noise(15, 4, 0, DEEP), noise(14, 36, 3, DEEP, True)],
        "C": sweep(70, 30, 14, 15, 5, 3, DEEP)},
    "boom3": {
        "N": [noise(15, 5, 0, DEEP), noise(15, 60, 4, DEEP, True)],
        "C": sweep(60, 28, 20, 15, 5, 3, DEEP)},
    "nuke": {        # the roar: the master noise-modulated, two low tones under it
        "N": [noise(15, 6, 0, 160), noise(15, 120, 8, 160, True)],
        "C": [tone(40, 15, 120, 9, 160)],
        "B": [tone(55, 13, 110, 9, 160)]},
    "thunder": {     # a crack, then the rumble
        "N": [noise(15, 3), noise(10, 2), noise(15, 3), noise(13, 70, 5, 180, True)],
        "C": [(0, 0, 8, 0), tone(36, 14, 70, 5, 180)]},
    "napalm": {      # crackling
        "N": [noise(v, f) for v, f in ((13, 2), (4, 2), (14, 1), (6, 3), (12, 2), (3, 2),
                                       (11, 1), (5, 3), (10, 2), (2, 2), (8, 2), (3, 3),
                                       (6, 2), (2, 2))]},
    "laser": {"B": sweep(2400, 300, 16, 13, 6)},
    "plasma": {"B": [tone(f, v, 2) for f, v in ((620, 12), (520, 12), (620, 11), (520, 11),
                                                   (640, 10), (500, 9), (660, 8), (480, 6))]},
    "dig": {         # grinding: noise pulses over a low buzz
        "N": [noise(v, 2) for v in (10, 3, 11, 4, 10, 3, 9, 2)],
        "C": [tone(f, 9, 2) for f in (140, 133, 140, 133, 140, 133, 140, 133)]},
    "roll": {"C": [tone(f, v, 3) for f, v in ((150, 10), (138, 9), (150, 9), (135, 8),
                                               (148, 7), (132, 6))]},
    "dirt": {"N": [noise(10, 4), noise(8, 16, 3)]},
    "shield": {"B": [tone(880, 13, 2), tone(700, 12, 3), tone(560, 10, 3), tone(440, 8, 6, 2)]},
    "deflect": {"B": sweep(700, 1600, 8, 12, 9)},
    "chute": {"N": [noise(5, 6), noise(7, 8), noise(5, 8, 3)]},
    "death": {       # down and down, a burst at the end
        "B": [tone(f, v, 3) for f, v in ((450, 13), (380, 13), (320, 12), (270, 12),
                                          (225, 11), (190, 10), (160, 9), (135, 8))],
        "N": [(0, 0, 18, 0), noise(12, 4), noise(8, 14, 3)]},
    "bounce": {"B": [tone(280, 12, 2), tone(370, 11, 2), tone(320, 9, 4, 2)]},
    "tick": {"B": [tone(750, 9, 1)]},
    "ui_tick": {"B": [tone(1100, 8, 1)]},
    "ui_select": {"B": [tone(620, 11, 3, 1)]},
    "ui_go": {"B": [tone(620, 11, 4), tone(800, 11, 4), tone(1020, 11, 8, 2)]},
    "ui_error": {"B": [tone(140, 13, 6), tone(130, 11, 8, 2)]},
    "ui_turn": {"B": [tone(523, 10, 5), tone(659, 11, 5), tone(784, 12, 5),   # a fanfare,
                      tone(1047, 15, 21, 4)]},                                  #  loudest last
    "ui_talk": {"B": [tone(700, 7, 1), tone(560, 6, 1)]},
}
# SND_* 0-15 (src/proto.h's order), then the dense ids p_snd.inc keeps
IDS = ["fire", "boom0", "nuke", "napalm", "laser", "plasma", "dig", "roll", "dirt", "shield",
       "deflect", "chute", "death", "bounce", "thunder", "tick",
       "boom0", "boom1", "boom2", "boom3",
       "ui_tick", "ui_select", "ui_go", "ui_error", "ui_turn", "ui_talk"]
TUNES = ["title", "win", "over", "shop"]        # MUS_TITLE, MUS_WIN, MUS_OVER, MUS_SHOP


# ---- the tunes ----------------------------------------------------------------------------
def tune_lists(song, src):
    """each bar's lists (A, C, N) as 3-byte steps, and the bar's jiffies"""
    bars = []
    for pat in song.order:
        ev2 = mus.frames(song, pat, 2)
        ev1 = mus.frames(song, pat, 1)
        if len(ev1) != len(ev2):
            raise SystemExit("%s: pattern %s's channels differ in length" % (src, pat))
        a, c, n = [], [], []

        def add(lst, note, ins, k0):
            """a step begins at jiffy k0 (a rest, or a note with its instrument)"""
            lst.append([note, ins, k0])

        # A: ch2's events; a sweep's or a rise's every other jiffy a raw step
        for j, ev in enumerate(ev2):
            if ev is None:
                if not a or a[-1][0] != 0:
                    add(a, 0, I_RAW, j)
            elif ev[0] in ("sweep", "rise"):
                if ev[2] % 2 == 0:
                    t, v = raw_slide(ev)
                    add(a, t, I_RAW | v << 4, j)
            elif ev[2] == 0:
                add(a, ev[1], INSTR[ev[0]], j)
        # C: ch1's kick and bass; N: ch1's snare and hat
        for j, ev in enumerate(ev1):
            if ev is None:
                if not c or c[-1][0] != 0:
                    add(c, 0, I_RAW, j)
                if not n or n[-1][0] != 0:
                    add(n, 0, I_RAW, j)
            elif ev[0] in ("S", "H"):
                if ev[2] == 0:
                    add(n, 1, INSTR[ev[0]], j)
                    if not c or c[-1][0] != 0:
                        add(c, 0, I_RAW, j)
            elif ev[2] == 0:
                note = 0
                if ev[0] == "bass":
                    note = ev[1]
                    while mus.hz(note) < 131:
                        note += 12
                add(c, note, INSTR[ev[0]] if ev[0] != "bass" else I_BASS, j)
                if ev[0] == "K" and (not n or n[-1][0] != 0):
                    add(n, 0, I_RAW, j)
        total = len(ev2)
        out = []
        for lst in (a, c, n):
            steps = []
            for i, (note, ins, k0) in enumerate(lst):
                k1 = lst[i + 1][2] if i + 1 < len(lst) else total
                while k1 - k0 > 255:
                    steps.append((note, ins, 255))
                    k0 += 255
                steps.append((note, ins, k1 - k0))
            out.append(steps)
        bars.append((total, out))
    return bars


def raw_slide(ev):
    """a slide's step: (T, volume) as tools/mkmus_intv.py shapes them"""
    kind, note, k = ev
    aclock = 63921.0
    pure = round(aclock / (2 * mus.hz(note))) - 1
    if kind == "sweep":
        a = min(255, round((pure + 1) * 1.08 ** k) - 1)
        v = max(1, 6 - k // 4)
    else:
        a = max(8, round((pure + 1) * 0.96 ** k) - 1)
        v = 5 if k < 24 else 6
    return per(aclock / (2 * (a + 1))), v


def main():
    out = sys.argv[1]
    lines = ["; GENERATED by tools/astro/mksnd_astro.py (the effects; the tunes from",
             "; music/*.txt): edit those, not this. p_snd.inc plays them.",
             "SNDMO   EQU     %d" % MO, "NSNDID  EQU     %d" % (len(IDS) + len(TUNES)),
             "MUSID0  EQU     %d               ; the tunes' ids from here" % len(IDS)]
    for i, (name, ins) in enumerate(sorted(INSTR.items(), key=lambda x: x[1])):
        pass
    # the ids' table: an effect's entry, or a tune's record
    lines.append("SNDTAB:")
    for name in IDS:
        lines.append("        DW      SE_%s" % name)
    for name in TUNES:
        lines.append("        DW      ST_%s" % name)
    nbytes = 0
    # the effects: an entry (channel, list, ... 0FFH), the lists
    for name, chans in EFFECTS.items():
        ent = []
        for ch, steps in chans.items():
            ent.append("%d,SL_%s_%s" % (CH[ch], name, ch))
        lines.append("SE_%s:" % name)
        for e in ent:
            c, l = e.split(",")
            lines.append("        DB      %s" % c)
            lines.append("        DW      %s" % l)
        lines.append("        DB      0FFH")
        nbytes += 3 * len(ent) + 1
        for ch, steps in chans.items():
            lines.append("SL_%s_%s:" % (name, ch))
            for t, v, f, d in steps:
                assert 0 <= t <= 255 and 0 <= v <= 15 and 1 <= f <= 255 and 0 <= d <= 255, (name, t, v, f, d)
                lines.append("        DB      %d,%d,%d,%d" % (t, v, f, d))
            lines.append("        DB      0,0,0,0")
            nbytes += 4 * len(steps) + 4
    # the tunes: records, bars, then the music lists (MUSBEG-MUSEND)
    notes = set()
    musl, recs = [], []
    seen = {}
    for name in TUNES:
        src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "music", name + ".txt")
        song = mus.parse(src)
        bars = tune_lists(song, src)
        loop = 0xFF if song.loop is None else song.loop
        recs.append("ST_%s:  DB      %d,%d" % (name, len(bars), loop))
        recs.append("        DW      SB_%s" % name)
        recs.append("SB_%s:" % name)
        for bi, (jif, lists) in enumerate(bars):
            labs = []
            for chi, steps in zip("ACN", lists):
                key = tuple(steps)
                if key not in seen:
                    lab = "SM_%s_%d%s" % (name, bi, chi)
                    seen[key] = lab
                    musl.append("%s:" % lab)
                    for note, ins, f in steps:
                        if ins & 15 not in (I_RAW, I_SNARE, I_HAT, I_KICK) and note:
                            notes.add(note)
                            if ins & 15 in (I_ARP, I_ARPMAJ):
                                notes.update((note + 3, note + 4, note + 7))
                            if ins & 15 == I_WARBLE:
                                notes.add(note + 1)
                        musl.append("        DB      %d,%d,%d" % (note, ins, f))
                    musl.append("        DB      0,0,0")
                    nbytes += 3 * len(steps) + 3
                labs.append(seen[key])
            assert jif <= 255
            recs.append("        DB      %d" % jif)
            recs.append("        DW      %s,0,%s,%s" % (labs[0], labs[1], labs[2]))
            nbytes += 9
        nbytes += 4
    lo, hi = min(notes), max(notes)
    lines.append("; a note's tone register (MIDI %d-%d)" % (lo, hi))
    lines.append("PITCHLO EQU     %d" % lo)
    lines.append("PITCHT:")
    tab = [per(mus.hz(n)) for n in range(lo, hi + 1)]
    for i in range(0, len(tab), 16):
        lines.append("        DB      " + ",".join(str(x) for x in tab[i:i + 16]))
    lines.append("KICKT1  EQU     %d               ; the kick's two jiffies" % per(150))
    lines.append("KICKT2  EQU     %d" % per(130))
    lines += recs
    lines.append("MUSBEG:")
    lines += musl
    lines.append("MUSEND:")
    nbytes += len(tab)
    worst = max(abs(1200 * __import__("math").log2(CLOCK / (2 * (MO + 1) * (per(mus.hz(n)) + 1)) / mus.hz(n)))
                for n in range(lo, hi + 1))
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")
    print("%s: %d bytes of sound data, notes %d-%d (worst %.0f cents)" % (out, nbytes, lo, hi, worst))


if __name__ == "__main__":
    main()
