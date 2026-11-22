#!/usr/bin/env python3
"""sndwav.py -- the sound's WAVs from an a2sim recording (A2WAV=raw.wav):
MAME's channel 1 is the Mockingboard's first AY (0 the speaker, 3 the
drive's noise), mono, its DC level taken off.

usage: sndwav.py fx RAW.wav OUTDIR        the effects of test scene 7: each
                                           cut at its onset (they come 3 s
                                           apart, after the AY's reset click)
       sndwav.py tune RAW.wav OUT.wav S   a tune: its longest stretch of
                                           sound, S seconds from its onset
"""
import array
import sys
import wave

NAMES = ["fire", "boom-small", "nuke", "napalm", "laser", "plasma", "dig", "roll", "dirt", "shield",
         "deflect", "chute", "death", "bounce", "thunder", "tick", "boom-1", "boom-2", "boom-3",
         "ui-tick", "ui-select", "ui-go", "ui-error", "ui-turn"]
CH = 1


def load(path):
    w = wave.open(path)
    rate, ch, nf = w.getframerate(), w.getnchannels(), w.getnframes()
    return rate, array.array("h", w.readframes(nf))[CH::ch]


def activity(x, rate):
    win = rate // 50
    return win, [max(x[i:i + win]) - min(x[i:i + win]) > 300 for i in range(0, len(x) - win, win)]


def onsets(act, quiet_for=20):
    out, quiet = [], 99
    for i, a in enumerate(act):
        if a and quiet >= quiet_for:
            out.append(i)
        quiet = 0 if a else quiet + 1
    return out


def save(path, rate, x, a, n):
    m = sum(x[max(0, a - rate // 10):a]) // max(1, min(a, rate // 10)) if a else x[0]
    clip = x[a:a + n]
    w = wave.open(path, "w")
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(rate)
    w.writeframes(array.array("h", [max(-32768, min(32767, v - m)) for v in clip]).tobytes())


def main():
    mode, raw = sys.argv[1:3]
    rate, x = load(raw)
    win, act = activity(x, rate)
    if mode == "fx":
        on = onsets(act)[-len(NAMES):]
        if len(on) != len(NAMES):
            sys.exit("%d effects found, not %d" % (len(on), len(NAMES)))
        for k, (name, o) in enumerate(zip(NAMES, on)):
            save("%s/a2-fx-%02d-%s.wav" % (sys.argv[3], k, name), rate, x, max(0, o * win - rate // 50),
                 int(2.9 * rate))
        print("%d effects, %s" % (len(on), " ".join("%.2f" % ((b - a) / 50) for a, b in zip(on, on[1:]))))
    else:
        out, secs = sys.argv[3], float(sys.argv[4])
        best, start, run, i0 = 0, 0, 0, 0       # the longest stretch (gaps under 0.4 s kept)
        quiet = 99
        for i, a in enumerate(act):
            if a:
                if quiet >= 20:
                    i0 = i
                quiet = 0
                if i - i0 > best:
                    best, start = i - i0, i0
            else:
                quiet += 1
        save(out, rate, x, max(0, start * win - rate // 50), int(secs * rate))
        print("%s: from %.2f s, %.2f s of sound" % (out, start / 50, best / 50))


if __name__ == "__main__":
    main()
