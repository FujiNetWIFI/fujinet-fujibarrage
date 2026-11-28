#!/usr/bin/env python3
"""sndwav_s2.py -- every sound of the Studio II port as a WAV, recorded from
MAME (the image's own player: p_test.inc scene 6, through s2sim's
S2SIM_WAV), for Checkpoint 2.

    sndwav_s2.py OUTDIR

Builds the TEST image (build/studio2-snd). One run plays the effects and
cues one every 180 jiffies from the first; the first beep marks the start,
so the recording is cut every 3 seconds into OUTDIR/NN-name.wav. Each tune
is a run of its own (OUTDIR/NN-mus_name.wav from its first beep).
"""
import os
import struct
import subprocess
import sys
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import mksnd_s2  # noqa: E402

NAMES = mksnd_s2.IDS
TUNES = mksnd_s2.TUNES
SLOT = 3.0


def record(img, lst, out, arg, seconds, wav):
    scn = os.path.join(out, "snd%d.txt" % arg)
    with open(scn, "w") as f:
        f.write("test 6 %d\nwait %d\n" % (arg, int(seconds * 60) + 60))
    env = dict(os.environ, S2SIM_WAV=wav, S2SIM_LST=lst, S2SIM_SECONDS=str(int(seconds) + 60))
    subprocess.run(["python3", os.path.join(TOP, "tools/s2sim/s2sim.py"), scn,
                    os.path.join(out, "run%d" % arg), img], env=env, cwd=TOP,
                   capture_output=True, check=True)


def samples(wav):
    w = wave.open(wav)
    rate, ch, n = w.getframerate(), w.getnchannels(), w.getnframes()
    raw = w.readframes(n)
    v = struct.unpack("<%dh" % (n * ch), raw)
    return rate, ch, v


def first(v, ch):
    for i in range(0, len(v), ch):
        if abs(v[i]) > 1000:
            return i // ch
    return None


def cut(rate, ch, v, a, b, path):
    w = wave.open(path, "wb")
    w.setnchannels(ch)
    w.setsampwidth(2)
    w.setframerate(rate)
    seg = v[a * ch:b * ch]
    w.writeframes(struct.pack("<%dh" % len(seg), *seg))
    w.close()


def main():
    out = os.path.abspath(sys.argv[1])
    os.makedirs(out, exist_ok=True)
    bdir = os.path.join(TOP, "build", "studio2-snd")
    subprocess.run(["bash", os.path.join(TOP, "src/studio2/build.sh")], cwd=TOP, check=True,
                   capture_output=True, env=dict(os.environ, TEST="1", OUT=bdir, R2R=bdir))
    img, lst = os.path.join(bdir, "barrage.st2"), os.path.join(bdir, "barrage.lst")
    wav = os.path.join(out, "fx.wav")
    record(img, lst, out, 0, SLOT * len(NAMES), wav)
    rate, ch, v = samples(wav)
    t0 = first(v, ch)
    for k, name in enumerate(NAMES):
        a = t0 + int(k * SLOT * rate) - int(0.02 * rate)
        cut(rate, ch, v, a, a + int(SLOT * rate), os.path.join(out, "%02d-%s.wav" % (k, name)))
    for j, t in enumerate(TUNES):
        k = len(NAMES) + j
        tw = os.path.join(out, "tune%d.wav" % k)
        record(img, lst, out, k, 34 if t == "title" else 6, tw)
        rate, ch, v = samples(tw)
        t1 = first(v, ch)
        cut(rate, ch, v, t1 - int(0.02 * rate), len(v) // ch, os.path.join(out, "%02d-mus_%s.wav" % (k, t)))
    print("%d sounds in %s" % (len(NAMES) + len(TUNES), out))


if __name__ == "__main__":
    main()
