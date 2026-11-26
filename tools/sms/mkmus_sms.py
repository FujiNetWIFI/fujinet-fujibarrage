#!/usr/bin/env python3
"""mkmus_sms.py PSG.bin FM.bin -- the Master System's tunes (music/*.txt,
tools/mus.py), arranged for its two chips, as streams src/sms/mus.asm plays
a frame at a time from a ROM bank:

  the PSG (every console): the lead on tone 1, a harmony on tone 0 (a third
  under the lead in the tune's scale), the bass on tone 2 (an octave up
  under the chip's 109 Hz), the drums on the noise (the kick a thump on
  tone 2 as well); a part gives its channel up while an effect has it
  (mus.asm)
  the YM2413 (a Japanese SMS, a Mark III's FM unit): rhythm mode's drums
  (K the bass drum, S the snare, H the hi-hat), the lead and the harmony in
  instruments chosen by the master's (t a trumpet, b a vibraphone, a/A a
  harpsichord, w a flute, the slides a synthesizer), a synth bass, and a
  soft pad of strings: the bass note's third and fifth

The masters' notes and timing are the other ports' (tools/mkmus_intv.py's
voicing for the PSG's volumes and slides). PSG.bin and FM.bin, a ROM bank
each (at 8000h when mapped):

  0   for each tune (title, win, over, shop): its stream's address (2 bytes)
  8   the streams

A PSG stream: 10h+p lo hi (part p's period: 0 lead, 1 harmony, 2 bass),
20h+p att (its attenuation; p 3 the noise), 30h ctl (the noise's control),
40h+n-1 (wait n frames, 1-64), 01h lo hi (go to: the loop), 00h (the end).
An FM stream: reg val (a register under 40h, its value), 80h+n-1 (wait n
frames), FEh lo hi (go to), FFh (the end).
"""
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import mus            # noqa: E402
import mkmus_intv as mi   # noqa: E402

TOP = os.path.abspath(os.path.join(HERE, "..", ".."))
TUNES = ["title", "win", "over", "shop"]
CLOCK = 3579545.0
FS = CLOCK / 72
ATT = [15, 15, 15, 15, 14, 12, 11, 9, 8, 6, 5, 3, 2, 0, 0, 0]   # the AY's volume as the SN's attenuation
MAJOR, MINOR, HARM = [0, 2, 4, 5, 7, 9, 11], [0, 2, 3, 5, 7, 8, 10], [0, 2, 3, 5, 7, 8, 11]


def expand(song):
    """the song a jiffy at a time: (ch1 events, ch2 events, the loop's jiffy)"""
    c1, c2, starts = [], [], []
    for pat in song.order:
        starts.append(len(c1))
        c1 += mus.frames(song, pat, 1)
        c2 += mus.frames(song, pat, 2)
    loop = None if song.loop is None else starts[song.loop]
    return c1, c2, loop


def scale_of(c1, c2):
    """the key that holds the most of the tune's notes: (tonic, steps)"""
    hist = [0] * 12
    for ev in c1 + c2:
        if ev and ev[1] is not None and ev[2] == 0:
            hist[ev[1] % 12] += 1
    best = None
    for t in range(12):
        for st in (MAJOR, MINOR, HARM):
            s = sum(hist[(t + d) % 12] for d in st)
            if best is None or s > best[0]:
                best = (s, t, st)
    return best[1], best[2]


def third_under(n, key):
    t, st = key
    pcs = [(t + d) % 12 for d in st]
    if n % 12 not in pcs:
        return None
    i = pcs.index(n % 12)
    h = pcs[(i - 2) % 7]
    d = (n % 12 - h) % 12
    return n - d


def triad(root, key):
    """the third and the fifth over a bass note, in the key (or a power chord)"""
    t, st = key
    pcs = [(t + d) % 12 for d in st]
    third = 4 if (root + 4) % 12 in pcs else 3
    return root + third, root + 7


# ---- the PSG ----------------------------------------------------------------------

def sn_period(hz):
    return max(1, min(1023, round(CLOCK / (32 * hz))))


def psg(c1, c2, loop, key):
    """per jiffy: [lead (per, att), harmony, bass, noise (ctl, att)]"""
    frames = []
    harm_note = None
    for j, (a, b) in enumerate(zip(c1, c2)):
        lead = har = bass = (None, 15)
        noise = (None, 15)
        if b:
            hz, v, _ = mi.voice(b, 1)
            if hz:
                lead = (sn_period(hz), ATT[min(15, v + 1)])
            kind, note, k = b
            if kind in ("blip", "tone", "warble") and note is not None:
                if k == 0:
                    harm_note = third_under(note if kind != "warble" else note - (k >> 1 & 1), key)
                if harm_note is not None:
                    hz2, v2, _ = mi.voice((kind, harm_note, k), 1)
                    har = (sn_period(hz2), ATT[max(0, min(15, v2 - 1))])
            else:
                harm_note = None
        else:
            harm_note = None
        if a:
            kind, note, k = a
            if kind == "bass":
                hz, v, _ = mi.voice(a, 0)
                while hz < 110:
                    hz *= 2
                bass = (sn_period(hz), ATT[v])
            elif kind == "K":
                hz, v, _ = mi.voice(a, 0)
                bass = (sn_period(hz * 2), ATT[v])
                noise = (0x06, (2, 6)[k])               # white, the slowest rate
            elif kind == "S":
                noise = (0x05, (3, 6, 9)[min(k, 2)])    # white, the middle rate
            elif kind == "H":
                noise = (0x04, 9)                       # white, the fastest
        frames.append([lead, har, bass, noise])
    return frames


def psg_stream(frames, loop):
    out = bytearray()
    cur = [None] * 8
    wait = 0
    at = {}

    def flush():
        nonlocal wait
        while wait:
            n = min(64, wait)
            out.append(0x40 + n - 1)
            wait -= n

    for j, fr in enumerate(frames):
        ev = bytearray()
        for p in range(3):
            per, att = fr[p]
            if per is not None and cur[p] != per:
                ev += bytes([0x10 + p, per & 255, per >> 8])
                cur[p] = per
            if cur[4 + p] != att:
                ev += bytes([0x20 + p, att])
                cur[4 + p] = att
        ctl, att = fr[3]
        if ctl is not None and (cur[3] != ctl or (j and frames[j - 1][3][0] is None)):
            ev += bytes([0x30, ctl])                    # (a hit: the control again, the noise restarts)
            cur[3] = ctl
        elif ctl is None:
            cur[3] = None
        if cur[7] != att:
            ev += bytes([0x23, att])
            cur[7] = att
        if j == loop:
            flush()
            at["loop"] = len(out)
            cur = [None] * 8                            # (at the loop all of it again)
            ev = bytearray()
            for p in range(3):
                per, a2 = fr[p]
                if per is not None:
                    ev += bytes([0x10 + p, per & 255, per >> 8])
                    cur[p] = per
                ev += bytes([0x20 + p, a2])
                cur[4 + p] = a2
            if ctl is not None:
                ev += bytes([0x30, ctl])
                cur[3] = ctl
            ev += bytes([0x23, att])
            cur[7] = att
        if ev:
            flush()
            out += ev
        wait += 1
    flush()
    if loop is None:
        out += bytes([0x20, 15, 0x21, 15, 0x22, 15, 0x23, 15, 0x00])
        return out, None
    out += bytes([0x01, 0, 0])
    return out, at["loop"]


# ---- the YM2413 ---------------------------------------------------------------------

INST = {"tone": 7, "blip": 12, "arp": 11, "arpmaj": 11, "warble": 4, "sweep": 10, "rise": 10}
VIOLIN, ORGAN, SYNBASS = 1, 8, 13


def fnum(hz):
    for block in range(8):
        f = round(hz * (1 << (19 - block)) / FS)
        if f <= 511:
            return f, block
    return 511, 7


def fm(c1, c2, loop, key):
    """per jiffy: a dict of register writes (ch 0 lead, 1 harmony, 2 bass,
    3-4 the pad; 6-8 rhythm)"""
    regs = []
    on = {}                     # ch -> (note/hz, inst, vol) sounding
    harm_note = None
    for a, b in zip(c1, c2):
        w = []

        def note_on(ch, hz, inst, vol, retrig):
            f, blk = fnum(hz)
            if retrig and ch in on:
                w.append((0x20 + ch, 0))                # key off first: a new note
            w.append((0x30 + ch, inst << 4 | vol))
            w.append((0x10 + ch, f & 255))
            w.append((0x20 + ch, 0x10 | blk << 1 | f >> 8))
            on[ch] = (hz, inst, vol)

        def note_off(ch):
            if ch in on:
                hz, inst, vol = on.pop(ch)
                f, blk = fnum(hz)
                w.append((0x20 + ch, blk << 1 | f >> 8))

        # the lead and its harmony
        if b:
            kind, note, k = b
            hz = mus.hz(note) if kind not in ("sweep", "rise") else mi.voice(b, 1)[0]
            inst = INST[kind]
            vol = 1 if kind != "blip" else (1 if k < 3 else 4)
            if k == 0 or kind in ("arp", "arpmaj", "warble", "sweep", "rise"):
                note_on(0, hz, inst, vol, k == 0)
            if kind in ("blip", "tone", "warble"):
                if k == 0:
                    harm_note = third_under(note, key)
                if harm_note is not None and k == 0:
                    note_on(1, mus.hz(harm_note), VIOLIN if kind == "tone" else inst, 5, True)
            elif k == 0:
                note_off(1)
                harm_note = None
        else:
            note_off(0)
            note_off(1)
            harm_note = None
        # the bass, the pad, the drums
        rhythm = 0
        if a:
            kind, note, k = a
            if kind == "bass" and k == 0:
                while mus.hz(note) < 40:
                    note += 12
                note_on(2, mus.hz(note), SYNBASS, 2, True)
                th, fi = triad(note % 12 + 60, key)
                note_on(3, mus.hz(th), VIOLIN, 9, True)
                note_on(4, mus.hz(fi), VIOLIN, 9, True)
            elif kind in ("K", "S", "H") and k == 0:
                rhythm = {"K": 0x10, "S": 0x08, "H": 0x01}[kind]
                if kind == "K":
                    note_off(2)
        else:
            note_off(2)
            note_off(3)
            note_off(4)
        if rhythm:
            w.append((0x0E, 0x20))                      # (the drum off, then on: struck)
            w.append((0x0E, 0x20 | rhythm))
        regs.append(w)
    return regs


def fm_stream(regs, loop):
    out = bytearray()
    # rhythm mode: its frequencies and volumes (the YM2413's recommended)
    init = [(0x0E, 0x20), (0x16, 0x20), (0x17, 0x50), (0x18, 0xC0), (0x26, 0x05), (0x27, 0x05), (0x28, 0x01),
            (0x36, 0x02), (0x37, 0x33), (0x38, 0x44)]
    for r, v in init:
        out += bytes([r, v])
    at = {}
    wait = 0

    def flush():
        nonlocal wait
        while wait:
            n = min(64, wait)
            out.append(0x80 + n - 1)
            wait -= n

    for j, w in enumerate(regs):
        if j == loop:
            flush()
            at["loop"] = len(out)
            for ch in range(9):                         # (every channel off at the loop)
                out += bytes([0x20 + ch, 0])
        if w:
            flush()
            for r, v in w:
                out += bytes([r, v])
        wait += 1
    flush()
    if loop is None:
        for ch in range(6):
            out += bytes([0x20 + ch, 0])
        out += bytes([0x0E, 0x20, 0xFF])
        return out, None
    out += bytes([0xFE, 0, 0])
    return out, at["loop"]


def main():
    """OUT_PSG.bin OUT_FM.bin: a bank each"""
    outs = sys.argv[1:3]
    banks = [[], []]
    for name in TUNES:
        song = mus.parse(os.path.join(TOP, "music", name + ".txt"))
        c1, c2, loop = expand(song)
        key = scale_of(c1, c2)
        ps = psg_stream(psg(c1, c2, loop, key), loop)
        fs = fm_stream(fm(c1, c2, loop, key), loop)
        banks[0].append(ps)
        banks[1].append(fs)
        print("%-6s key %s %s: %d jiffies, PSG %d bytes, FM %d bytes" % (
            name, "C C# D D# E F F# G G# A A# B".split()[key[0]],
            {tuple(MAJOR): "major", tuple(MINOR): "minor", tuple(HARM): "harmonic minor"}[tuple(key[1])],
            len(c1), len(ps[0]), len(fs[0])))
    for out, streams in zip(outs, banks):
        pos, offs, blob = 8, [], bytearray()
        for s, lp in streams:
            if lp is not None:                          # the go-to's target: the bank's address
                a = 0x8000 + pos + lp
                s[-2], s[-1] = a & 255, a >> 8
            offs.append(0x8000 + pos)
            blob += s
            pos += len(s)
        data = struct.pack("<4H", *offs) + blob
        assert len(data) <= 0x3000, (out, len(data))
        open(out, "wb").write(data)
        print("%s: %d bytes" % (out, len(data)))


if __name__ == "__main__":
    main()
