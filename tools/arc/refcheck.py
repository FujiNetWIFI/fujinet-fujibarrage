#!/usr/bin/env python3
"""refcheck.py -- the Arcadia plays a weapon's shot as the reference does
(tools/astrosim/refcheck.py's, for the Arcadia's block field).

usage: tools/arc/refcheck.py [-flat] [-seed N] [-shield I] [-aim ANGLE POWER] OUTDIR WEAPON...

For each weapon (proto I_* number): refview makes a shot with it at the
Arcadia's caps exactly as the server projects them from its HELLO (48x48
at 60 Hz, PAR 78, tank 2x1, a 160-byte buffer, PLAT_ARCADIA: 6 track
slots) and writes it as src/arcadia/test/fixture.inc (every segment); the
image is built; arcsim plays it (test scene 3: the field, the tanks, each
segment in turn on the PLY page, as in a room); the field's 384 cells are
read from screen RAM and compared with tools/arc/mock_arc.py's Mock('B',
'night') of refview's field and tanks after the shot (the cells the
reference client shows, look B, our seat 0, the shooter's turn): cells
equal is the goal. Also the sprites' seats. Prints the cells that differ
and how many frames the shot ran behind the script (the scene marks its
start and end in TMARK: 98, 99), and writes OUTDIR/W/{arc,mock}.png and
diff.txt. Leaves the fixture as it was.
"""
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import mock_arc as M  # noqa: E402
import arcmodel as A  # noqa: E402

FIX = os.path.join(TOP, "src/arcadia/test/fixture.inc")
CAPS = ["-w", "48", "-h", "48", "-par", "78", "-buf", "160", "-tankw", "2", "-tankh", "1", "-plat", "11"]


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


def peek(text, name):
    m = re.search(r"PEEK %s: ((?:[0-9A-F]{2} ?)+)" % name, text)
    return bytes.fromhex(m.group(1).replace(" ", "")) if m else None


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
    rows = []
    rv = os.path.join(out, "refview")
    run(["go", "build", "-o", rv, "./cmd/refview"], cwd=os.path.join(TOP, "server"))
    try:
        for w in weapons:
            d = os.path.join(out, str(w))
            os.makedirs(d, exist_ok=True)
            cmd = [rv] + CAPS + ["-shots", "1", "-seed", seed, "-weapon", str(w), "-fixture-asm", FIX,
                                 "-out", d, "-font", os.path.join(TOP, "gfx/font3x5.txt")] + extra
            if flat:
                cmd.append("-flat")
            log = run(cmd).stdout.strip().splitlines()[-1]
            b = subprocess.run(["bash", "src/arcadia/build.sh"], cwd=TOP, capture_output=True, text=True)
            if b.returncode:
                print(b.stdout[-3000:], b.stderr[-3000:])
                raise SystemExit("the build failed")
            scn = os.path.join(d, "scene.txt")
            with open(scn, "w") as f:
                f.write("test 3\nuntil TMARK 98 3000\nuntil TMARK 99 12000\nwait 2\nsnap end\n"
                        "peek SCR 208\npeek SCR2 176\npeek SPRSEL 4\npeek TKX 10\npeek TKY 10\n"
                        "peek C3N 1\npeek NSEGS 1\n")
            r = run(["python3", os.path.join(TOP, "tools/arcsim/arcsim.py"), scn, d], cwd=TOP).stdout
            open(os.path.join(d, "arcsim.txt"), "w").write(r)
            if "timeout" in r:
                print("weapon %2d: the shot never ended (%s)" % (w, log))
                fails += 1
                rows.append((w, None, None, log))
                continue
            u = [int(x) for x in re.findall(r"UNTIL [0-9A-F]+ [0-9A-F]+ (\d+)", r)]
            sf = re.search(r"(\d+) frames", log)
            lag = (u[1] - u[0]) - int(sf.group(1)) if len(u) == 2 and sf else None
            scr = peek(r, "SCR") or b""
            scr2 = None
            for m in re.finditer(r"PEEK ([0-9A-F]{4}): ((?:[0-9A-F]{2} ?)+)", r):
                if m.group(1) == "1A00":
                    scr2 = bytes.fromhex(m.group(2).replace(" ", ""))
                if m.group(1) == "1800":
                    scr = bytes.fromhex(m.group(2).replace(" ", ""))
            mine = scr + (scr2 or b"")
            px = open(os.path.join(d, "shot1.px"), "rb").read()
            tk = open(os.path.join(d, "shot1.tanks"), "rb").read()
            shooter = int(re.search(r"seat (\d+)", log).group(1))
            tanks = [(tk[4 * i], tk[4 * i + 1], tk[4 * i + 2], tk[4 * i + 3] * 2) for i in range(10)]
            f = {"frame": 0, "flash": 0, "shooter": shooter, "tanks": tanks, "px": px}
            M.OWN = 0
            mk = M.Mock("B", "night")
            u2, conf, wrong, theirs = mk.build(f, 0)
            spr = mk.sprites_for(f, 0)
            diff = []
            for i in range(384):
                if mine[i] != theirs[i]:
                    diff.append((i // 16, i % 16, mine[i], theirs[i]))
            sel = []
            for m in re.finditer(r"PEEK ([0-9A-F]{4}): ((?:[0-9A-F]{2} ?)+)", r):
                pass
            sprsel = None
            lines = r.splitlines()
            for ln in lines:
                if ln.startswith("PEEK ") and len(ln.split(":")[1].split()) == 4:
                    sprsel = [int(x, 16) for x in ln.split(":")[1].split()]
            want = (spr + [255] * 4)[:4]
            with open(os.path.join(d, "diff.txt"), "w") as fo:
                fo.write("%s\nsprites: arcadia %s, mock %s\n" % (log, sprsel, want))
                for rr, cc, a, b2 in diff:
                    fo.write("row %2d col %2d: arcadia %02X mock %02X\n" % (rr, cc, a, b2))
            try:
                A.to_image(u2.frame()).save(os.path.join(d, "mock.png"))
            except Exception:
                pass
            sp = "" if sprsel == want else "  sprites %s, mock %s" % (sprsel, want)
            print("weapon %2d: %3d cells differ, %s frames behind%s  (%s)"
                  % (w, len(diff), "?" if lag is None else "%3d" % lag, sp, log))
            rows.append((w, len(diff), lag, log))
            if diff or sp:
                fails += 1
    finally:
        shutil.move(keep, FIX)
        os.utime(FIX, None)
        subprocess.run(["bash", "src/arcadia/build.sh"], cwd=TOP, capture_output=True)
    with open(os.path.join(out, "summary.txt"), "w") as fo:
        for w, n, lag, log in rows:
            fo.write("%2d %s %s %s\n" % (w, n, lag, log))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
