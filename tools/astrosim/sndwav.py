#!/usr/bin/env python3
"""sndwav.py -- each Astrocade sound as a WAV, for listening (the effects
and the tunes, as tools/astro/mksnd_astro.py makes them).

usage: tools/astrosim/sndwav.py OUTDIR [NAME ...]

For each sound (all by default): astrosim runs test scene 2 (src/astrocade
src/p_test.inc TSOUND: half a second of quiet, the sound, its time) with
MAME writing the audio (ASTROSIM_WAV); the WAV is cut from the sound's
start to the scene's end and written as OUTDIR/NAME.wav, its peak printed.
Uses the image as built (r2r/astrocade/barrage.bin).
"""
import os
import re
import struct
import subprocess
import sys
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
# name: the id TSOUND plays (0xFE: the falling shell's whistle, then a boom)
SOUNDS = [("fire", 0), ("boom0", 16), ("boom1", 17), ("boom2", 18), ("boom3", 19),
          ("nuke", 2), ("thunder", 14), ("napalm", 3), ("laser", 4), ("plasma", 5),
          ("dig", 6), ("roll", 7), ("dirt", 8), ("shield", 9), ("deflect", 10),
          ("chute", 11), ("death", 12), ("bounce", 13), ("tick", 15), ("whistle", 0xFE),
          ("ui_tick", 20), ("ui_select", 21), ("ui_go", 22), ("ui_error", 23),
          ("ui_turn", 24), ("ui_talk", 25),
          ("tune_title", 26), ("tune_win", 27), ("tune_over", 28), ("tune_shop", 29)]


def main():
    out = os.path.abspath(sys.argv[1])
    want = set(sys.argv[2:])
    os.makedirs(out, exist_ok=True)
    for name, sid in SOUNDS:
        if want and name not in want:
            continue
        d = os.path.join(out, "run_" + name)
        os.makedirs(d, exist_ok=True)
        scn = os.path.join(d, "scene.txt")
        with open(scn, "w") as f:
            f.write("test 2 %d\nuntil TESTHK+3 99 2400\n" % sid)
        raw = os.path.join(d, "all.wav")
        env = dict(os.environ, ASTROSIM_WAV=raw)
        r = subprocess.run(["python3", os.path.join(HERE, "astrosim.py"), scn, d], cwd=TOP,
                           env=env, capture_output=True, text=True).stdout
        t = re.search(r"^TEST (\d+)", r, re.M)
        u = re.search(r"^UNTIL [0-9A-F]+ [0-9A-F]+ (\d+)", r, re.M)
        if not t or not u:
            print("%s: the scene didn't finish" % name)
            continue
        f0, f1 = int(t.group(1)) + 30, int(u.group(1))
        w = wave.open(raw)
        rate, ch, sw = w.getframerate(), w.getnchannels(), w.getsampwidth()
        frames = w.readframes(w.getnframes())
        w.close()
        a, b = int(f0 / 60 * rate), int(f1 / 60 * rate)
        cut = frames[a * ch * sw:b * ch * sw]
        samples = struct.unpack("<%dh" % (len(cut) // 2), cut) if sw == 2 else []
        peak = max((abs(s) for s in samples), default=0)
        # the end's quiet trimmed (a tenth of a second kept)
        n = len(samples) // ch
        last = n
        while last > 0 and max(abs(samples[(last - 1) * ch + k]) for k in range(ch)) < 64:
            last -= 1
        last = min(n, last + rate // 10)
        o = wave.open(os.path.join(out, name + ".wav"), "wb")
        o.setnchannels(ch)
        o.setsampwidth(sw)
        o.setframerate(rate)
        o.writeframes(cut[:last * ch * sw])
        o.close()
        print("%-12s %5.2f s  peak %5.1f%%" % (name, last / rate, 100.0 * peak / 32768))


if __name__ == "__main__":
    main()
