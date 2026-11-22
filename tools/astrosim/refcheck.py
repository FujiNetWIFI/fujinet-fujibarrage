#!/usr/bin/env python3
"""refcheck.py -- the Astrocade plays a weapon's shot as the reference does
(tools/intvsim/refcheck.py's, for the Astrocade).

usage: tools/astrosim/refcheck.py [-flat] [-seed N] [-shield I] [-aim ANGLE POWER] OUTDIR WEAPON...

For each weapon (proto I_* number): refview makes a shot with it (the
Astrocade's shape: 160x56 at 60 Hz, PAR 54, a 240-byte buffer) and writes
it as src/astrocade/test/fixture.inc (every segment); the image is built;
astrosim plays it (scene 3: the field, then each segment in turn); the
field is read from screen RAM and compared with refview's after the shot:
dirt against dirt, fire against fire (a tank's pixels left out: the
reference has none). Prints the pixels that differ (0 is the goal) and how
many frames the shot ran behind the script (the player's lag: the scene
marks its start and end in TESTHK+3), and writes OUTDIR/W/{astro,ref,
diff}.png. Leaves the fixture as it was.
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
W, H = 160, 56
FIX = os.path.join(TOP, "src/astrocade/test/fixture.inc")


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


def picture(path, rows, scale=3):
    """0 sky, 1 dirt, 2 fire, 3 a difference, 4 a tank: an RGB PNG"""
    pal = {0: b"\x28\x3c\x78", 1: b"\x96\x6e\x3c", 2: b"\xff\xc8\x28", 3: b"\xff\x00\xff", 4: b"\x10\x10\x10"}
    raw = b""
    for r in rows:
        line = b"\0" + b"".join(pal[v] * scale for v in r)
        raw += line * scale

    def chunk(t, body):
        return struct.pack(">I", len(body)) + t + body + struct.pack(">I", zlib.crc32(t + body) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W * scale, H * scale, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def main():
    args = sys.argv[1:]
    flat = "-flat" in args
    args = [a for a in args if a != "-flat"]
    seed, extra = "5", []
    while args[0] in ("-seed", "-aim", "-shield"):
        if args[0] == "-seed":
            seed, args = args[1], args[2:]
        elif args[0] == "-shield":
            extra, args = extra + ["-shield", args[1]], args[2:]
        else:
            extra, args = extra + ["-angle", args[1], "-power", args[2]], args[3:]
    out, weapons = os.path.abspath(args[0]), [int(a) for a in args[1:]]
    os.makedirs(out, exist_ok=True)
    keep = FIX + ".keep"
    shutil.copy(FIX, keep)
    fails = 0
    lags = []
    try:
        for w in weapons:
            d = os.path.join(out, str(w))
            os.makedirs(d, exist_ok=True)
            cmd = ["go", "run", "./cmd/refview", "-w", str(W), "-h", str(H), "-par", "54", "-buf", "240",
                   "-shots", "1", "-seed", seed, "-weapon", str(w), "-fixture-asm", FIX, "-out", d] + extra
            if flat:
                cmd.append("-flat")
            log = run(cmd, cwd=os.path.join(TOP, "server")).stdout.strip()
            b = subprocess.run(["bash", "src/astrocade/build.sh"], cwd=TOP, capture_output=True, text=True)
            if b.returncode:
                print(b.stdout[-2000:], b.stderr[-2000:])
                raise SystemExit("the build failed")
            scn = os.path.join(d, "scene.txt")
            with open(scn, "w") as f:
                f.write("test 3\nuntil TESTHK+3 98 9000\nuntil TESTHK+3 99 9000\nsnap end\npeek FIELD %d\n" % (W * H // 4))
            r = run(["python3", os.path.join(HERE, "astrosim.py"), scn, d], cwd=TOP).stdout
            if "timeout" in r:
                print("weapon %d: the shot never ended (%s)" % (w, log.splitlines()[-1] if log else ""))
                fails += 1
                continue
            # the shot's frames here against the script's own (refview's): the lag
            u = [int(x) for x in re.findall(r"UNTIL [0-9A-F]+ [0-9A-F]+ (\d+)", r)]
            sf = re.search(r"(\d+) frames", log)
            lag = (u[1] - u[0]) - int(sf.group(1)) if len(u) == 2 and sf else None
            lags.append((w, lag))
            m = re.search(r"PEEK [0-9A-F]+: ((?:[0-9A-F]{2} )+)", r)
            mem = bytes.fromhex(m.group(1).replace(" ", ""))
            ref = open(os.path.join(d, "shot1.px"), "rb").read()
            nd = nf = 0
            mine, theirs, diff = [], [], []
            for y in range(H):
                mr, tr, dr = [], [], []
                for x in range(W):
                    p = (mem[y * 40 + x // 4] >> (6 - 2 * (x % 4))) & 3
                    rv = ref[y * W + x]
                    rd = 1 if rv == 1 else 2 if rv == 2 else 0
                    iv = {1: 0, 2: 1, 3: 2, 0: 4}[p]
                    mr.append(iv)
                    tr.append(rd)
                    if iv == 4:
                        dr.append(4)
                        continue
                    if (iv == 1) != (rd == 1):
                        nd += 1
                    elif (iv == 2) != (rd == 2):
                        nf += 1
                    dr.append(3 if iv != rd else iv)
                mine.append(mr)
                theirs.append(tr)
                diff.append(dr)
            picture(os.path.join(d, "astro.png"), mine)
            picture(os.path.join(d, "ref.png"), theirs)
            picture(os.path.join(d, "diff.png"), diff)
            print("weapon %2d: %4d dirt, %4d fire pixels differ, %s frames behind  (%s)"
                  % (w, nd, nf, "?" if lag is None else "%3d" % lag, log.splitlines()[-1]))
            if nd or nf:
                fails += 1
    finally:
        shutil.move(keep, FIX)
        os.utime(FIX, None)
        subprocess.run(["bash", "src/astrocade/build.sh"], cwd=TOP, capture_output=True)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
