#!/usr/bin/env python3
"""mkmus_intv.py -- FujiBarrage's tunes (music/*.txt, tools/mus.py) for the
Intellivision's PSG (src/intv/sound.bas): ch1 on voice A, ch2 on voice C.
FujiBlast's, with several tunes to a file, `once` tunes that end, and a
word a step. The Atari's converter (tools/mkmus_atari.py) is the model:
each channel of a pattern is a subroutine (the same data once), each
channel's order a list of calls.

A step is one word: bits 0-6 its code (an index into the period tables, 0
silent; or 112 + n: the voice a noise drum of period n, its tone off),
bits 7-10 the PSG volume, bits 11-15 the jiffies (1-31: a longer note is
several steps). A word with jiffies 0 is a control: code 0 returns, 1
calls and 2 jumps (the word after it says where), 3 ends a tune played
once.

The PSG's tone is f = clock / (16 P): 111861 / P at an NTSC console's
1.79 MHz, 125000 / P at PAL's 2 MHz. Both tables are written, a word a
period (mus_nper, mus_pper).

usage: mkmus_intv.py OUT.bas NAME=music/x.txt ...
  (each tune's channels start at CONST NAME1 and NAME2 in mus_d)"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mus  # noqa: E402

NTSC, PAL = 111861.0, 125000.0
ACLOCK = 63921.0            # the Atari's, whose steps the sweeps and slides copy
RET, CALL, JUMP, END = 0, 1, 2, 3      # a control word's code (its jiffies 0)
MAXN = 31                               # a step's jiffies: 5 bits (a longer one: several)
NOISE0 = 112                            # a step's code from here: noise, period code - NOISE0

# noise periods of the drums (the PSG's, 1-31; lower is brighter)
NOISE = {"S": 10, "H": 3}


def cents(f, want):
    return 1200 * math.log2(f / want)


def pure(note):
    """the Atari's AUDF of a pure tone (the slides are shaped in them)"""
    return round(ACLOCK / (2 * mus.hz(note))) - 1


def vol(k, steps):
    """the volume k jiffies in: steps is ((until, volume), ...)"""
    for until, v in steps:
        if k < until:
            return v
    return steps[-1][1]


def psgvol(v):
    """the Atari's volume (0-15) as the PSG's, whose steps are 3 dB"""
    return 0 if v == 0 else min(15, v + 4)


def audf_hz(a):
    return ACLOCK / (2 * (a + 1))


def voice(ev, ch):
    """(Hz or None, volume, noise period) for one jiffy's event"""
    if ev is None:
        return (None, 0, 0)
    kind, note, k = ev
    if kind == "K":                     # a falling thump
        return ((150.0, 15, 0), (70.0, 13, 0))[k]
    if kind == "S":
        return (None, psgvol((0xC, 0x6, 0x6)[k]), NOISE["S"])
    if kind == "H":
        return (None, psgvol(6), NOISE["H"])
    if kind == "bass":
        return (mus.hz(note), psgvol(vol(k, ((2, 10), (999, 6)))), 0)
    if kind == "blip":
        v = vol(k, ((2, 6), (4, 3), (999, 0)))
        return (mus.hz(note), psgvol(v), 0)
    if kind == "tone":
        return (mus.hz(note), psgvol(vol(k, ((2, 6), (999, 4)))), 0)
    if kind in ("arp", "arpmaj", "warble"):
        return (mus.hz(note), psgvol(5), 0)
    k &= ~1                             # slides: a step every other jiffy
    if kind == "sweep":                 # down, fading
        a = min(255, round((pure(note) + 1) * 1.08 ** k) - 1)
        return (audf_hz(a), psgvol(max(1, 6 - k // 4)), 0)
    if kind == "rise":
        a = max(8, round((pure(note) + 1) * 0.96 ** k) - 1)
        return (audf_hz(a), psgvol(vol(k, ((24, 5), (999, 6)))), 0)
    raise SystemExit("no PSG voice for %r" % kind)


def main():
    out = sys.argv[1]
    tunes = [a.split("=", 1) for a in sys.argv[2:]]
    pitches = {None: 0}                 # Hz -> table index, all the tunes'
    data = []
    consts = []

    def steps(evs, ch):
        """one word a step: the code (a pitch's index, or NOISE0 + the noise's
        period), the volume << 7, the jiffies << 11"""
        out = []
        for hz, v, nz in (voice(e, ch) for e in evs):
            if hz not in pitches:
                pitches[hz] = len(pitches)
            code = NOISE0 + nz if nz else pitches[hz]
            if out and out[-1][0] == code and out[-1][1] == v and out[-1][2] < MAXN:
                out[-1][2] += 1
            else:
                out.append([code, v, 1])
        return [c | v << 7 | n << 11 for c, v, n in out]

    for name, src in tunes:
        song = mus.parse(src)
        subs = [{}, {}]                 # per channel: steps -> label
        order = [[], []]
        for pat in song.order:
            lens = set()
            for ch in (0, 1):
                evs = mus.frames(song, pat, ch + 1)
                lens.add(len(evs))
                key = tuple(steps(evs, ch))
                if key not in subs[ch]:
                    subs[ch][key] = "m%d_%d" % (ch + 1, len(subs[ch]))
                order[ch].append(subs[ch][key])
            if len(lens) != 1:
                raise SystemExit("%s: pattern %s's channels differ in length" % (src, pat))
        # lay out from where the tunes before it ended: loop 1's list, loop
        # 2's, then the subroutines: a word a step, a call or a jump two
        at, pos, tops = {}, len(data), []
        for ch in (0, 1):
            tops.append(pos)
            pos += 2 * len(order[ch]) + 2
        for ch in (0, 1):
            for key, lab in subs[ch].items():
                at[(ch, lab)] = pos
                pos += len(key) + 1
        for ch in (0, 1):
            for lab in order[ch]:
                data += [CALL, at[(ch, lab)]]
            if song.loop is None:
                data += [END, 0]
            else:
                data += [JUMP, tops[ch] + 2 * song.loop]
        for ch in (0, 1):
            for key in subs[ch]:
                data += list(key) + [RET]
        assert len(data) == pos
        consts += [(name.upper() + "1", tops[0]), (name.upper() + "2", tops[1])]
        jiffies = sum(len(song.patterns[n]) for n in song.order) * song.tempo
        print("%s: %d bars, %.1f s at 60 Hz, %s" % (src, len(song.order), jiffies / 60,
                                                    "once" if song.loop is None else "loops"))
    assert max(data) < 65536
    if len(pitches) > NOISE0:
        raise SystemExit("%d pitches: past the noise codes" % len(pitches))
    inv = sorted(pitches, key=lambda h: pitches[h])
    per = {"n": [], "p": []}
    for hz in inv:
        for r, clk in (("n", NTSC), ("p", PAL)):
            per[r].append(1 if hz is None else max(1, min(4095, round(clk / hz))))

    def dump(f, name, vals, per=16):
        f.write("%s:\n" % name)
        for i in range(0, len(vals), per):
            f.write("    DATA %s\n" % ", ".join(map(str, vals[i:i + per])))

    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "w") as f:
        f.write("' GENERATED by tools/mkmus_intv.py from %s: edit those, not this.\n"
                % ", ".join(src for _, src in tunes))
        f.write("' A word a step: the code (a period's index, or %d + a noise period), the\n"
                "' volume << 7, the jiffies << 11. Jiffies 0: a control, its code 0 return,\n"
                "' 1 call and 2 jump (the next word: where), 3 the end. src/intv/sound.bas\n"
                "' plays them.\n" % NOISE0)
        for n, v in consts:
            f.write("    CONST %s = %d\n" % (n, v))
        dump(f, "mus_nper", per["n"])
        dump(f, "mus_pper", per["p"])
        dump(f, "mus_d", data, 16)
    print("%s: %d words, %d pitches" % (out, len(data) + 2 * len(inv), len(inv)))


if __name__ == "__main__":
    main()
