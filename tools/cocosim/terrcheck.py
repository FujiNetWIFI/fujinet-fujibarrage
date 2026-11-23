#!/usr/bin/env python3
"""terrcheck.py -- the CoCo's field against the terrain it was given.

usage: terrcheck.py --fixture [-seed N] [-shots N] OUTDIR [coco2] [coco3]
       terrcheck.py --live coco2|coco3 TAPLOG SIMDIR PREFIX [OUT.png]

--fixture: refview plays N shots (4) on the field of seed N (5) at each
machine's shape; the field after them (craters, overhangs: TERRAIN entries
with spans) becomes the test scenes' terrain (fix_terrain in
src/coco{2,3}/test/fixture.h, the rest of it kept). A TEST=1 build plays
scene 5 in cocosim, and every pixel the CoCo shows as sky or dirt must be
the field's; tank (0) and fire (3: the turn's arrow) pixels are counted,
not compared. OUTDIR/<machine>/: coco.png, ref.png, diff.png (red: a
difference). The fixtures are put back after, and r2r/coco/barrage.dsk is
the TEST=1 build of them again.

--live: the TERRAIN (G) frames of tools/rigs/tapproxy.py's TAPLOG replayed
(a whole field, then diffs on it, as src/net.c takes them) against a dump
in SIMDIR (cocosim's: PREFIXscreen.bin on the CoCo 1/2, PREFIXb0.bin ..
PREFIXb5.bin on the CoCo 3), taken while the field was the last terrain's
(no shot since). Sky and dirt compared, tank and fire counted.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.join(HERE, "..", "..")
sys.path.insert(0, HERE)
import cocofield  # noqa: E402

SHAPES = {
    "coco2": dict(w=128, h=168, par=109, tw=5, th=5, fix="src/coco2/test/fixture.h",
                  mame="coco2b", map="build/barrage-game2.map"),
    "coco3": dict(w=320, h=176, par=55, tw=12, th=6, fix="src/coco3/test/fixture.h",
                  mame="coco3", map="build/barrage-game3.map"),
}
T_EMPTY = 0xF0


def carve(px, w, h):
    """tunnels under the ground and islands over it: columns of spans (the
    entries' longest form), as tunnelling shots and falling dirt leave"""
    px = bytearray(px)
    for k in range(6):
        cx, cy = w * (2 * k + 1) // 12, h * 3 // 4 - (k % 3) * 6
        rx, ry = w // 20 + k, 4 + k % 3
        for y in range(cy - ry, cy + ry + 1):
            for x in range(cx - rx, cx + rx + 1):
                if 0 <= x < w and 0 <= y < h and ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1:
                    px[y * w + x] = 0
        for j in range(3):                      # islands: bands over the tunnels
            y0 = h // 6 + 8 * j + k
            for x in range(cx - rx + j, cx + rx - j):
                if 0 <= x < w:
                    px[y0 * w + x] = px[(y0 + 1) * w + x] = 1
    return bytes(px)


def encode(px, w, h):
    """a field (refview: 0 sky, 1 dirt, 2 fire) as TERRAIN entries"""
    out = []
    for x in range(w):
        runs, y = [], 0
        while y < h:
            if px[y * w + x] == 1:
                y0 = y
                while y < h and px[y * w + x] == 1:
                    y += 1
                runs.append((y0, y))
            else:
                y += 1
        if not runs:
            out.append(T_EMPTY)
        elif len(runs) == 1 and runs[0][1] == h and runs[0][0] < T_EMPTY:
            out.append(runs[0][0])
        else:
            runs = runs[:15]
            out.append(T_EMPTY | len(runs))
            for a, b in runs:
                out += [a, b]
    return out


def decode(entries, w, h):
    """TERRAIN entries as values (1 sky, 2 dirt), [y][x]"""
    f = [[1] * w for _ in range(h)]
    i = 0
    for x in range(w):
        b = entries[i]
        if b < T_EMPTY:
            for y in range(b, h):
                f[y][x] = 2
            i += 1
        elif b == T_EMPTY:
            i += 1
        else:
            n = b & 15
            for k in range(n):
                for y in range(entries[i + 1 + 2 * k], min(entries[i + 2 + 2 * k], h)):
                    f[y][x] = 2
            i += 1 + 2 * n
    return f


def tap_terrain(log, w, h):
    """the field the TERRAIN frames in a tap log make, [y][x] (None: none whole)"""
    field, building, tv = None, None, 0xFF
    for line in open(log):
        p = line.split()
        if len(p) < 2 or p[0] != "G":
            continue
        b = bytes.fromhex(p[1])
        if b[0] == 0xFF:
            if building is None or tv != 0xFE:
                building = [[1] * w for _ in range(h)]
            tv = 0xFE
        elif field is None or b[0] != tv:
            continue                    # (not what the client had: it asks again)
        else:
            building = [r[:] for r in field]
        x, i = b[3] | b[4] << 8, 5
        while i < len(b) and x < w:
            one = decode(b[i:], 1, h)
            for y in range(h):
                building[y][x] = one[y][0]
            e = b[i]
            i += 1 + 2 * (e & 15) if e > T_EMPTY else 1
            x += 1
        if b[2] & 1:
            field, tv = building, b[1]
    return field


def live(m, log, sim, prefix, png):
    sh = SHAPES[m]
    t = tap_terrain(log, sh["w"], sh["h"])
    if t is None:
        sys.exit("no whole terrain in " + log)
    if m == "coco2":
        f = cocofield.field2(open(os.path.join(sim, prefix + "screen.bin"), "rb").read())
    else:
        f = cocofield.field3([open(os.path.join(sim, "%sb%d.bin" % (prefix, k)), "rb").read() for k in range(6)])
    nd = ntank = nfire = 0
    diff = []
    for y in range(sh["h"]):
        row = []
        for x in range(sh["w"]):
            v = f[y][x]
            if v == 0:
                ntank += 1
            elif v == 3:
                nfire += 1
            elif v != t[y][x]:
                nd += 1
                v = 4
            row.append(v)
        diff.append(row)
    print("%s %s: %d sky/dirt pixels differ (%d tank, %d fire not compared)" % (m, prefix, nd, ntank, nfire))
    if png:
        cocofield.picture(png, diff)
    return nd


def with_terrain(header, entries):
    """the fixture with this terrain, and its script a byte (scene 5 plays
    none: the shots' scripts would overrun a TEST build's low core)"""
    a = header.index("static const uint8_t fix_terrain[")
    b = header.index("};", a) + 2
    body = ",\n".join("    " + ", ".join(str(v) for v in entries[i:i + 16]) for i in range(0, len(entries), 16))
    header = header[:a] + "static const uint8_t fix_terrain[%d] = {\n%s\n};" % (len(entries), body) + header[b:]
    header = re.sub(r"#define FIX_NSEG \d+", "#define FIX_NSEG 1", header)
    header = re.sub(r"static const uint16_t fix_seglen\[\d+\] = \{.*?\};",
                    "static const uint16_t fix_seglen[1] = {\n    1,\n};", header, flags=re.S)
    return re.sub(r"static const uint8_t fix_seg\[\d+\] = \{.*?\};",
                  "static const uint8_t fix_seg[1] = {\n    0,\n};", header, flags=re.S)


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--live" and len(args) >= 5:
        sys.exit(1 if live(args[1], args[2], args[3], args[4], args[5] if len(args) > 5 else None) else 0)
    if not args or args[0] != "--fixture":
        sys.exit(__doc__)
    args = args[1:]
    seed, shots = "5", "4"
    while args and args[0].startswith("-"):
        if args[0] == "-seed":
            seed = args[1]
        elif args[0] == "-shots":
            shots = args[1]
        args = args[2:]
    out = os.path.abspath(args[0])
    machines = args[1:] or ["coco2", "coco3"]
    kept = {}
    refs = {}
    try:
        for m in machines:
            sh = SHAPES[m]
            d = os.path.join(out, m)
            os.makedirs(d, exist_ok=True)
            tmp = os.path.join(d, "fixture.h")
            r = run(["go", "run", "./cmd/refview", "-shots", shots, "-seed", seed, "-buf", "1536",
                     "-w", str(sh["w"]), "-h", str(sh["h"]), "-par", str(sh["par"]),
                     "-tankw", str(sh["tw"]), "-tankh", str(sh["th"]), "-fixture-all", tmp, "-out", d],
                    cwd=os.path.join(TOP, "server"))
            if r.returncode:
                sys.exit("refview: " + r.stderr[-500:])
            px = carve(open(os.path.join(d, "shot%s.px" % shots), "rb").read(), sh["w"], sh["h"])
            ent = encode(px, sh["w"], sh["h"])
            refs[m] = decode(ent, sh["w"], sh["h"])
            spans = sum(1 for e in ent if e > T_EMPTY)
            fix = os.path.join(TOP, sh["fix"])
            kept[fix] = open(fix).read()
            open(fix, "w").write(with_terrain(open(tmp).read(), ent))
            print("%s: %d entry bytes, %d columns with spans" % (m, len(ent), spans))
        b = run(["make", "coco", "TEST=1"], cwd=TOP)
        if b.returncode:
            sys.exit("build: " + (b.stdout + b.stderr)[-1500:])
        bad = 0
        for m in machines:
            sh = SHAPES[m]
            d = os.path.join(out, m)
            scn = os.path.join(d, "scene.txt")
            with open(scn, "w") as f:
                f.write("test 5 0\nwait 600\n")
                if m == "coco2":
                    f.write("peek $0E38 1\ndump 1000 27FF screen.bin\n")
                else:
                    f.write("".join("dumpblk %X b%d.bin\n" % (0x30 + k, k) for k in range(6)))
            sim = os.path.join(d, "sim")
            env = dict(os.environ, MACHINE=sh["mame"])
            r = run(["python3", os.path.join(HERE, "cocosim.py"), os.path.join(TOP, "r2r/coco/barrage.dsk"),
                     os.path.join(TOP, sh["map"]), scn, sim], env=env)
            if m == "coco2":
                sw = "PEEK $0E38 01" in r.stdout
                path = os.path.join(sim, "screen.bin")
                if not os.path.exists(path):
                    print("%s: no screen from cocosim\n%s" % (m, r.stdout[-800:]))
                    bad = 1
                    continue
                f = cocofield.field2(open(path, "rb").read(), sw)
            else:
                paths = [os.path.join(sim, "b%d.bin" % k) for k in range(6)]
                if not all(os.path.exists(p) for p in paths):
                    print("%s: no screen from cocosim\n%s" % (m, r.stdout[-800:]))
                    bad = 1
                    continue
                f = cocofield.field3([open(p, "rb").read() for p in paths])
            ref = refs[m]
            nd = ntank = nfire = 0
            diff = []
            for y in range(sh["h"]):
                row = []
                for x in range(sh["w"]):
                    v, rv = f[y][x], ref[y][x]
                    if v == 0:
                        ntank += 1
                        row.append(0)
                    elif v == 3:
                        nfire += 1
                        row.append(3)
                    elif v != rv:
                        nd += 1
                        row.append(4)
                    else:
                        row.append(v)
                diff.append(row)
            cocofield.picture(os.path.join(d, "coco.png"), f)
            cocofield.picture(os.path.join(d, "ref.png"), ref)
            cocofield.picture(os.path.join(d, "diff.png"), diff)
            print("%s: %d sky/dirt pixels differ (%d tank, %d fire not compared)" % (m, nd, ntank, nfire))
            bad |= nd != 0
    finally:
        for fix, text in kept.items():
            open(fix, "w").write(text)
        if kept:
            run(["make", "coco", "TEST=1"], cwd=TOP)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
