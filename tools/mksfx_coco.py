#!/usr/bin/env python3
"""mksfx_coco.py -- the CoCo's sound data: the effects (from the
Intellivision's src/intv/sfx.asm, heard and approved, as the Apple's
tools/mksfx_a2.py) and the tunes (music/*.txt through tools/mus.py and the
Intellivision's voicing, tools/mkmus_intv.py, as tools/mkmus_a2.py), for
src/coco/sound.c and each machine's engine.

    mksfx_coco.py coco2|coco3 sfx OUT.asm [-s SECTION]
    mksfx_coco.py coco2|coco3 tunes OUT.asm [-s SECTION] NAME=music/x.txt ...
(SECTION: where it goes, art by default: src/coco/barrage{,3}.link)

A pitch is in the machine's own units ("units"):
  coco3  a half wave in the GIME timer's counts (3579545 Hz): the timer's
         IRQ flips the voice (src/coco3/snd3a.asm)
  coco2  a phase step a sample of the mixer (src/coco2/snd2a.asm: R samples
         a second, a square wave of f = step x R / 65536 Hz)
Noise: the voice's level is a random bit, a new one at each of its edges
(the CoCo 1/2: each time its phase's high byte moves on).

An effect: steps of jiffies (0 ends it), then the volume (bits 0-3, 3 dB a
step: the PSG's) with bit 4 noise and bit 5 the envelope (the volume 15,
falling), the pitch (units, high byte first); with the envelope, its fall
in volume steps a jiffy (8.8). The cue tables: _sx_cues (16 SND_*: the
body, the low end), _sx_booms (4 sizes: body, low end), _sx_ui (6 UI_*).

A tune: a word a step, as the Apple's: bits 0-6 the code (an index into
_mus_per, 0 silent; or 112 + n: noise, _mus_nz[n]), 7-10 the volume, 11-15
the jiffies; jiffies 0 a control: 0 returns, 1 calls and 2 jumps (the next
word an address), 3 ends. Each tune's channels start at _t_NAME1, _t_NAME2.

Also (sfx): _vol_lin (the 16 volumes as the DAC's levels, a voice's share)
and _bl_units (a whistle's or blip's pitch p: the Apple's AY period 2 (p+1)
at 1020484 Hz, 128 of them, in units).
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

ICLOCK = 1789773.0              # the Intellivision's PSG
TCLK = 3579545.0                # the GIME timer's count
CYC2 = 129                      # the CoCo 1/2's mixer: a sample every 129 cycles (snd2a.asm),
R2 = 894886.0 / CYC2 * 0.983    # less the field sync's IRQ (measured: tools/cocosim, scene 9)
VMAX = {"coco3": 26, "coco2": 21}       # a voice's loudest (CoCo 3: the sum clipped at 63; CoCo 1/2: 3 x 21)
MIN3 = 1000                     # the CoCo 3's shortest half wave (the IRQ's rate: CPU; higher: an octave down)
NOISE3 = 1800                   # and a noise voice's (its edges)
SX_N, SX_E = 0x1000, 0x2000


def units(m, hz, noise=False):
    if m == "coco3":
        u = round(TCLK / (2 * hz))
        if noise:
            return max(NOISE3, min(32767, u))
        while u < MIN3:                 # (too high for the IRQ's rate: an octave down)
            u *= 2
        return min(32767, u)
    u = round(hz * (256 if noise else 65536) / R2)
    return max(1, min(32767, u))


def tone_hz(period):
    """the PSG's tone period at the Intellivision's clock"""
    return ICLOCK / 16 / max(1, period)


def noise_hz(n):
    """its noise period: the rate of new random bits"""
    return ICLOCK / 16 / max(1, n)


def value(tok):
    tok = tok.strip().replace("SX_N", str(SX_N)).replace("SX_E", str(SX_E))
    tok = re.sub(r"\$([0-9A-Fa-f]+)", lambda m: str(int(m.group(1), 16)), tok)
    return eval(tok, {"__builtins__": {}}, {})


def tables(src):
    out, cur = {}, None
    for line in open(src):
        line = line.split(";")[0].rstrip()
        m = re.match(r"^(\w+):\s*DECLE\s+(.*)$", line)
        if m:
            cur = m.group(1)
            out[cur] = [t for t in m.group(2).split(",") if t.strip()]
            continue
        m = re.match(r"^\s+DECLE\s+(.*)$", line)
        if m and cur:
            out[cur] += [t for t in m.group(1).split(",") if t.strip()]
            continue
        if re.match(r"^\S", line):
            cur = None
    return out


def effect(m, words):
    vals = [value(t) for t in words]
    out, i = [], 0
    while True:
        what, how = vals[i], vals[i + 1]
        i += 2
        if how == 0:
            out.append(0)
            return out
        jif, vol = (how >> 4) & 255, how & 15
        flags = 0
        if what & SX_N:
            flags |= 0x10
            u = units(m, noise_hz(what & 31), True)
        else:
            u = units(m, tone_hz(what & 0x0FFF))
        out += [jif, vol | flags | (0x20 if what & SX_E else 0), u >> 8, u & 255]
        if what & SX_E:
            ep = vals[i]
            i += 1
            dv = min(15 * 256, round(16 * 16.667 / (ep * 0.143) * 256))
            out += [dv >> 8, dv & 255]
            out[-6] = jif                       # (the step: as long as it says)
            out[-5] = 15 | flags | 0x20


SECTION = "art"


def fcb(lines, label, data):
    lines.append("%s" % label)
    for k in range(0, len(data), 16):
        lines.append("        fcb     %s" % ",".join("$%02X" % b for b in data[k:k + 16]))


def sfx(m, src, dst):
    t = tables(src)
    fx = [n for n in t if n.startswith("sx_") and n not in ("sx_cues", "sx_booms", "sx_ui")]
    lines = ["* GENERATED by tools/mksfx_coco.py (%s) from %s: edit that, not this." % (m, os.path.relpath(src)),
             "        section %s" % SECTION,
             "        export  _sx_cues,_sx_booms,_sx_ui,_vol_lin,_bl_units"]
    n = 0
    for name in fx:
        b = effect(m, t[name])
        n += len(b)
        fcb(lines, name, b)
    for tab in ("sx_cues", "sx_booms", "sx_ui"):
        words = [w.strip() for w in t[tab]]
        n += 2 * len(words)
        lines.append("_%s" % tab)
        for k in range(0, len(words), 8):
            lines.append("        fdb     %s" % ",".join(words[k:k + 8]))
    # (the PSG's 3 dB a step squeezed to 1.5: a voice has 21-26 levels of a 6-bit DAC)
    vl = [0] + [max(1, round(VMAX[m] * 10 ** (-(15 - v) * 1.5 / 20))) for v in range(1, 16)]
    fcb(lines, "_vol_lin", vl)
    bl = [units(m, 1020484.0 / 16 / (2 * (p + 1))) for p in range(128)]
    lines.append("_bl_units")
    for k in range(0, 128, 8):
        lines.append("        fdb     %s" % ",".join(str(u) for u in bl[k:k + 8]))
    n += 16 + 256
    open(dst, "w").write("\n".join(lines) + "\n")
    print("%s: %d effects, %d bytes" % (dst, len(fx), n))


def tunes(m, dst, specs):
    import mus
    import mkmus_intv as mi
    pitches = {None: 0}
    noises = {}
    lines, exports, total = [], [], 0

    def steps(evs, ch):
        res = []
        for hz, v, nz in (mi.voice(e, ch) for e in evs):
            if nz:
                noises.setdefault(nz, 0)
                code = mi.NOISE0 + nz
            else:
                if hz not in pitches:
                    pitches[hz] = len(pitches)
                code = pitches[hz]
            if res and res[-1][0] == code and res[-1][1] == v and res[-1][2] < mi.MAXN:
                res[-1][2] += 1
            else:
                res.append([code, v, 1])
        return [c | v << 7 | n << 11 for c, v, n in res]

    for name, src in specs:
        song = mus.parse(src)
        subs = [{}, {}]
        order = [[], []]
        for pat in song.order:
            for ch in (0, 1):
                key = tuple(steps(mus.frames(song, pat, ch + 1), ch))
                if key not in subs[ch]:
                    subs[ch][key] = "%s_%d_%d" % (name, ch + 1, len(subs[ch]))
                order[ch].append(subs[ch][key])
        for ch in (0, 1):
            top = "_t_%s%d" % (name, ch + 1)
            exports.append(top)
            lines.append(top)
            for k, lab in enumerate(order[ch]):
                lines.append("%s_o%d_%d fdb %d,%s" % (name, ch + 1, k, mi.CALL, lab))
                total += 4
            if song.loop is None:
                lines.append("        fdb     %d,0" % mi.END)
            else:
                lines.append("        fdb     %d,%s_o%d_%d" % (mi.JUMP, name, ch + 1, song.loop))
            total += 4
        for ch in (0, 1):
            for key, lab in subs[ch].items():
                words = list(key) + [mi.RET]
                total += 2 * len(words)
                lines.append(lab)
                for i in range(0, len(words), 12):
                    lines.append("        fdb     %s" % ",".join(str(w) for w in words[i:i + 12]))
    inv = sorted(pitches, key=lambda h: pitches[h])
    per = [0 if hz is None else units(m, hz) for hz in inv]
    nz = [units(m, noise_hz(n), True) if n in noises else 0 for n in range(16)]
    with open(dst, "w") as f:
        f.write("* GENERATED by tools/mksfx_coco.py (%s) from %s: edit those, not this.\n"
                % (m, ", ".join(s for _, s in specs)))
        f.write("        section %s\n" % SECTION)
        f.write("        export  _mus_per,_mus_nz,%s\n" % ",".join(exports))
        f.write("_mus_per\n")
        for i in range(0, len(per), 12):
            f.write("        fdb     %s\n" % ",".join(str(p) for p in per[i:i + 12]))
        f.write("_mus_nz\n        fdb     %s\n" % ",".join(str(p) for p in nz))
        f.write("\n".join(lines) + "\n")
    print("%s: %d bytes, %d pitches" % (dst, total + 2 * len(per) + 32, len(per)))


def main():
    global SECTION
    m, what, out = sys.argv[1:4]
    rest = sys.argv[4:]
    if rest[:1] == ["-s"]:
        SECTION, rest = rest[1], rest[2:]
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    if what == "sfx":
        sfx(m, os.path.join(HERE, "..", "src", "intv", "sfx.asm"), out)
    elif what == "tunes":
        tunes(m, out, [a.split("=", 1) for a in rest])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
