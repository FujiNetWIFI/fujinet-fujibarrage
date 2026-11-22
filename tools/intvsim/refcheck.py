#!/usr/bin/env python3
"""refcheck.py -- the Intellivision plays a weapon's shot as the reference
does (tools/atarisim/refcheck.py's, for the INTV).

usage: tools/intvsim/refcheck.py [-flat] [-seed N] [-shield I] [-aim ANGLE POWER] OUTDIR WEAPON...

For each weapon (proto I_* number): refview makes a shot with it (the
INTV's shape: 160x80 at 60 Hz, PAR 54, a 1024-byte buffer) and writes it
as src/intv/test/fixture.bas; a TEST=1 build plays it in jzIntv (scene 3:
the terrain, then each segment); then the INTV's planes (DIRT, FXP) are
compared with refview's field after the shot: dirt against dirt, fire
against fire. Prints the pixels that differ (0 is the goal), and writes
OUTDIR/W/{intv,ref,diff}.png. Leaves the fixture as it was.
"""
import os
import shutil
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))

W, H = 160, 80
DIRT, FXP = 0x8CA0, 0x8FC0
FIX = os.path.join(TOP, "src/intv/test/fixture.bas")


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


def plane(words):
    """a plane's words (GRAM layout, 4 a cell) as rows of pixels"""
    px = [[0] * W for _ in range(H)]
    for c in range(200):
        cy, cx = divmod(c, 20)
        for k in range(4):
            w = words[c * 4 + k]
            for half, row in ((w & 255, 2 * k), (w >> 8, 2 * k + 1)):
                for b in range(8):
                    if half & (0x80 >> b):
                        px[cy * 8 + row][cx * 8 + b] = 1
    return px


def peek(out, addr):
    for ln in out.splitlines():
        if ln.startswith("peek %X 800 " % addr) or ln.startswith("peek %04X 800 " % addr):
            return [int(v, 16) for v in ln.split()[3:]]
    raise SystemExit("no peek of $%04X in the run's output" % addr)


def picture(path, rows, scale=3):
    """0 sky, 1 dirt, 2 fire, 3 a difference: an RGB PNG"""
    pal = {0: b"\x28\x3c\x78", 1: b"\x96\x6e\x3c", 2: b"\xff\xc8\x28", 3: b"\xff\x00\xff"}
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
    try:
        for w in weapons:
            d = os.path.join(out, str(w))
            os.makedirs(d, exist_ok=True)
            cmd = ["go", "run", "./cmd/refview", "-w", "160", "-h", "80", "-par", "54", "-buf", "1024",
                   "-shots", "1", "-seed", seed, "-weapon", str(w), "-fixture-bas", FIX, "-out", d] + extra
            if flat:
                cmd.append("-flat")
            log = run(cmd, cwd=os.path.join(TOP, "server")).stdout.strip()
            b = subprocess.run(["make", "-C", "src/intv", "TEST=1"], cwd=TOP, capture_output=True, text=True)
            if b.returncode:
                print(b.stdout[-2000:], b.stderr[-2000:])
                raise SystemExit("the build failed")
            scn = os.path.join(d, "scene.txt")
            with open(scn, "w") as f:
                f.write("test 3\nuntilvar tst_s 99 7200\nsnap end\npeek %04X 800\npeek %04X 800\n" % (DIRT, FXP))
            env = dict(os.environ, SDL_AUDIODRIVER="dummy")
            env.pop("FUJINET", None)
            r = run(["python3", os.path.join(HERE, "intvsim.py"), os.path.join(TOP, "r2r/intv/barrage.rom"),
                     scn, d], env=env, cwd=TOP).stdout
            if "TIMEOUT" in r:
                print("weapon %d: the shot never ended (%s)" % (w, log))
                fails += 1
                continue
            dirt, fire = plane(peek(r, DIRT)), plane(peek(r, FXP))
            ref = open(os.path.join(d, "shot1.px"), "rb").read()
            nd = nf = 0
            mine, theirs, diff = [], [], []
            for y in range(H):
                mr, tr, dr = [], [], []
                for x in range(W):
                    rv = ref[y * W + x]
                    iv = 1 if dirt[y][x] else 2 if fire[y][x] else 0
                    rd = 1 if rv == 1 else 2 if rv == 2 else 0
                    mr.append(iv)
                    tr.append(rd)
                    if (iv == 1) != (rd == 1):
                        nd += 1
                    elif (iv == 2) != (rd == 2):
                        nf += 1
                    dr.append(3 if iv != rd else iv)
                mine.append(mr)
                theirs.append(tr)
                diff.append(dr)
            picture(os.path.join(d, "intv.png"), mine)
            picture(os.path.join(d, "ref.png"), theirs)
            picture(os.path.join(d, "diff.png"), diff)
            print("weapon %2d: %4d dirt, %4d fire pixels differ  (%s)" % (w, nd, nf, log.splitlines()[-1]))
            if nd or nf:
                fails += 1
    finally:
        shutil.move(keep, FIX)
        os.utime(FIX, None)                     # (newer than gen/'s copy: make takes it again)
        subprocess.run(["make", "-C", "src/intv"], cwd=TOP, capture_output=True)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
