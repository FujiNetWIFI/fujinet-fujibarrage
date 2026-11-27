#!/usr/bin/env python3
"""sndwav.py -- every sound of the Channel F port as a WAV, recorded from MAME
(the image's own player: p_test.inc scene 6, the sound board, through
chfsim CHFSIM_WAV).

    sndwav.py OUTDIR [IMAGE LISTING]

The board plays each sound to its end (the title's loop: 8 s), then half a
second of silence, writing its id to TV as it starts; chfsim's marks give
each start's machine time, so the recording is cut into OUTDIR/NN-name.wav (from
its start to the next one's). Each file's length to its last sound is
printed.
"""
import os
import re
import subprocess
import sys
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import mksnd_chf  # noqa: E402

NAMES = (["fire", "boom", "nuke", "napalm", "laser", "plasma", "dig", "roll", "dirt",
          "shield", "deflect", "chute", "death", "bounce", "thunder", "tick"]
         + mksnd_chf.IDS[16:] + ["mus_" + t for t in mksnd_chf.TUNES])


def main():
    out = os.path.abspath(sys.argv[1])
    os.makedirs(out, exist_ok=True)
    rec = os.path.join(out, "board")
    os.makedirs(rec, exist_ok=True)
    scn = os.path.join(rec, "board.txt")
    open(scn, "w").write("test 6\nmarks TV\nwaitfor TV 255 20000\nwait 10\n")
    wav = os.path.join(rec, "board.wav")
    env = dict(os.environ, CHFSIM_WAV=wav, CHFSIM_SECONDS="400")
    cmd = [sys.executable, os.path.join(TOP, "tools", "chfsim", "chfsim.py"), scn, rec] + sys.argv[2:4]
    r = subprocess.run(cmd, env=env, capture_output=True, text=True, cwd=TOP).stdout
    marks = [(int(v), float(t)) for v, t in re.findall(r"MARK (\d+) \d+ frame \d+ at ([\d.]+)", r)]
    starts = [(v, t) for v, t in marks if v < len(NAMES) or v == 255]
    w = wave.open(wav)
    rate, ch, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
    data = w.readframes(w.getnframes())
    w.close()
    step = ch * width               # (the recording starts with the machine)
    for k, (v, t) in enumerate(starts):
        if v == 255:
            break
        a, b = round(t * rate), round(starts[k + 1][1] * rate)
        seg = data[a * step:b * step]
        last = 0
        for i in range(len(seg) // step):
            if abs(int.from_bytes(seg[i * step:i * step + width], "little", signed=True)) > 500:
                last = i
        name = os.path.join(out, "%02d-%s.wav" % (v, NAMES[v]))
        o = wave.open(name, "wb")
        o.setnchannels(ch)
        o.setsampwidth(width)
        o.setframerate(rate)
        o.writeframes(seg[:(last + rate // 10) * step])
        o.close()
        print("%-28s %5.2f s" % (os.path.basename(name), last / rate))


if __name__ == "__main__":
    main()
