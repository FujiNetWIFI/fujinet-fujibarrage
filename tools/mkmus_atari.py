#!/usr/bin/env python3
"""mkmus_atari.py -- the tunes (music/*.txt, tools/mus.py) for POKEY's voices
1 and 2 (src/atari/snd.s): ch1 on voice 1, ch2 on voice 2. (From
FujiBlast's: several tunes to a file, and `once` tunes that end.)

Steps of AUDF, AUDC, jiffies as snd.s plays them; each channel of a pattern
is a subroutine (the same data once), each channel's order a list of calls:

    AUDF AUDC n         n jiffies (1-252)
    lo   hi   $FE       call (one level)
    0    0    $FD       return
    lo   hi   $FF       jump

AUDCTL is netstream's (channels 3+4 at 1.79 MHz, joined): voices 1 and 2 are
on the 64 kHz clock, never anything else.
  pure tone ($A0): f = 63921 / (2 (N + 1))
  bass, distortion C ($C0, the 4-bit poly): f = 63921 / (15 (N + 1)), in
  tune only while N + 1 shares no factor with 15
  noise ($80): the drums

usage: mkmus_atari.py OUT.s NAME=music/x.txt ...   (labels _NAME1, _NAME2)"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mus  # noqa: E402

CLOCK = 63921.0
CALL, RET, JUMP = 0xFE, 0xFD, 0xFF
MAXN = 252


def cents(f, want):
    return 1200 * math.log2(f / want)


def pure(note):
    """AUDF of a pure tone"""
    d = round(CLOCK / (2 * mus.hz(note)))
    if not 2 <= d <= 256:
        raise SystemExit("note %d is out of the pure tone's range" % note)
    return d - 1


def bass(note):
    """AUDF of a distortion C bass note: the nearest divisor coprime with 15"""
    want = mus.hz(note)
    best = min((d for d in range(2, 257) if math.gcd(d, 15) == 1),
               key=lambda d: abs(cents(CLOCK / (15 * d), want)))
    return best - 1


def vol(k, steps):
    """the volume k jiffies in: steps is ((until, volume), ...)"""
    for until, v in steps:
        if k < until:
            return v
    return steps[-1][1]


def voice(ev):
    """(AUDF, AUDC) for one jiffy's event"""
    if ev is None:
        return (0, 0)
    kind, note, k = ev
    if kind == "K":                     # a falling buzz: thump
        return ((0x1C, 0xCF), (0x3A, 0xCC))[k]
    if kind == "S":
        return (0x10, 0x80 | (0xC, 0x6, 0x6)[k])
    if kind == "H":
        return (0x01, 0x86)
    if kind == "bass":
        return (bass(note), 0xC0 | vol(k, ((2, 10), (999, 6))))
    if kind == "blip":
        v = vol(k, ((2, 6), (4, 3), (999, 0)))
        return (pure(note), 0xA0 | v) if v else (0, 0)
    if kind == "tone":
        return (pure(note), 0xA0 | vol(k, ((2, 6), (999, 4))))
    if kind in ("arp", "arpmaj", "warble"):
        return (pure(note), 0xA5)
    k &= ~1                             # slides: a step every other jiffy
    if kind == "sweep":                 # down, fading
        return (min(255, round((pure(note) + 1) * 1.08 ** k) - 1),
                0xA0 | max(1, 6 - k // 4))
    if kind == "rise":
        return (max(8, round((pure(note) + 1) * 0.96 ** k) - 1),
                0xA0 | vol(k, ((24, 5), (999, 6))))
    raise SystemExit("no Atari voice for %r" % kind)


def steps(evs):
    """jiffy by jiffy -> steps (AUDF, AUDC, n)"""
    out = []
    for fc in map(voice, evs):
        if out and tuple(out[-1][:2]) == fc and out[-1][2] < MAXN:
            out[-1][2] += 1
        else:
            out.append([fc[0], fc[1], 1])
    return out


def convert(src, name):
    """the asm lines and byte count of the tune in src, labelled _NAME1/_NAME2"""
    song = mus.parse(src)
    subs = [{}, {}]                     # per channel: steps -> label
    order = [[], []]
    for pat in song.order:
        lens = set()
        for ch in (0, 1):
            evs = mus.frames(song, pat, ch + 1)
            lens.add(len(evs))
            key = tuple(map(tuple, steps(evs)))
            if key not in subs[ch]:
                subs[ch][key] = "%s%d_%d" % (name, ch + 1, len(subs[ch]))
            order[ch].append(subs[ch][key])
        if len(lens) != 1:
            raise SystemExit("%s: pattern %s's channels differ in length" % (src, pat))
    lines, size = ["; " + src], 0
    for ch in (0, 1):
        top = "_%s%d" % (name, ch + 1)
        lines.append(top + ":")
        for lab in order[ch]:
            lines.append("        .byte   <%s, >%s, $%02X" % (lab, lab, CALL))
        if song.loop is None:           # once: the end
            lines.append("        .byte   0, 0, 0")
        else:
            tgt = "%s + %d" % (top, 3 * song.loop)
            lines.append("        .byte   <(%s), >(%s), $%02X" % (tgt, tgt, JUMP))
        size += 3 * (len(order[ch]) + 1)
        for key, lab in subs[ch].items():
            lines.append(lab + ":")
            for f, c, n in key:
                lines.append("        .byte   $%02X, $%02X, %d" % (f, c, n))
            lines.append("        .byte   0, 0, $%02X" % RET)
            size += 3 * (len(key) + 1)
    jiffies = sum(len(song.patterns[n]) for n in song.order) * song.tempo
    print("%s: %d bars, %.1f s at 60 Hz, %d bytes" % (src, len(song.order), jiffies / 60, size))
    return lines, [n for n in ("_%s1" % name, "_%s2" % name)]


def main():
    """mkmus_atari.py OUT.s NAME=music/x.txt ...: the tunes, one file"""
    out = sys.argv[1]
    lines = ["; GENERATED by tools/mkmus_atari.py: edit music/*.txt, not this.",
             "; Steps AUDF, AUDC, jiffies; 0 ends; $FE call, $FD return, $FF jump (src/atari/snd.s).",
             "        .rodata"]
    for arg in sys.argv[2:]:
        name, src = arg.split("=", 1)
        body, labels = convert(src, name)
        lines.insert(2, "        .export " + ", ".join(labels))
        lines += body
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
