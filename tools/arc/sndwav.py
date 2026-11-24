#!/usr/bin/env python3
"""sndwav.py -- every sound of the Arcadia port as a WAV, recorded from MAME
(the image's own player: p_snd.inc's test scenes through arcsim ARCSIM_WAV).

    sndwav.py OUTDIR [IMAGE]

Scene 11 plays every effect and cue 180 frames apart in one recording; it
is cut into OUTDIR/fx-NAME.wav (from each one's start, 3 s). Scene 12 plays
one tune a run: OUTDIR/tune-NAME.wav (the title 40 s: round its loop once).
Each file's length to the last sound and its loudest frequency are printed.
"""
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import mksnd_arc  # noqa: E402

TUNES = {"title": (26, 2400), "win": (27, 300), "over": (28, 400), "shop": (29, 200)}


def record(scene, arg, frames, wav, image):
    with tempfile.TemporaryDirectory() as tmp:
        sc = os.path.join(tmp, "s.txt")
        open(sc, "w").write("test %d %d\nwait %d\n" % (scene, arg, frames))
        env = dict(os.environ, ARCSIM_WAV=wav)
        subprocess.run([sys.executable, os.path.join(ROOT, "tools", "arcsim", "arcsim.py"),
                        sc, os.path.join(tmp, "out"), image], env=env, check=True,
                       stdout=subprocess.DEVNULL)


def load(path):
    w = wave.open(path)
    rate, ch, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
    x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i%d" % width)
    return rate, ch, width, x.reshape(-1, ch)


def save(path, rate, ch, width, x):
    w = wave.open(path, "wb")
    w.setnchannels(ch)
    w.setsampwidth(width)
    w.setframerate(rate)
    w.writeframes(x.astype("<i%d" % width).tobytes())
    w.close()


def loud(x):
    return np.abs(x.astype(np.int64)).max(axis=1) > 300


def describe(rate, x):
    on = np.nonzero(loud(x))[0]
    if not len(on):
        return "silent"
    a, b = on[0], on[-1] + 1
    m = x[a:b].mean(axis=1)
    f = np.abs(np.fft.rfft(m * np.hanning(len(m))))
    f[:5] = 0
    return "%.2f s, peak %.0f Hz" % ((b - a) / rate, np.argmax(f) * rate / len(m))


def main(out, image):
    os.makedirs(out, exist_ok=True)
    tmp = os.path.join(out, "fx-all.wav")
    ids = mksnd_arc.IDS
    record(11, 0, 180 * len(ids) + 60, tmp, image)
    rate, ch, width, x = load(tmp)
    on = np.nonzero(loud(x))[0]
    t0 = on[0]                          # the first sound's start: then 3 s each
    per = rate * 3
    for i, name in enumerate(ids):
        seg = x[t0 + i * per - rate // 20: t0 + (i + 1) * per - rate // 20]
        p = os.path.join(out, "fx-%02d-%s.wav" % (i, name))
        save(p, rate, ch, width, seg)
        print("%-28s %s" % (os.path.basename(p), describe(rate, seg)))
    for name, (sid, frames) in TUNES.items():
        p = os.path.join(out, "tune-%s.wav" % name)
        record(12, sid, frames, p, image)
        rate, ch, width, x = load(p)
        on = np.nonzero(loud(x))[0]
        if len(on):
            x = x[max(0, on[0] - rate // 20):]
            save(p, rate, ch, width, x)
        print("%-28s %s" % (os.path.basename(p), describe(rate, x)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "r2r", "arcadia",
                                                                          "barrage.bin"))
