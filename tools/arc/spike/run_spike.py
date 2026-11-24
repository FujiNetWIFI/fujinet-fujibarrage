#!/usr/bin/env python3
"""run_spike.py -- build spike.asm with scene.inc (gen_spike.py), run it in
MAME (a plain cart, no FujiNet), compare MAME's snapshot with the model's
expected.png pixel for pixel, and print the plot timings.
  run_spike.py OUTDIR     (OUTDIR holds scene.inc and expected.png)"""
import os
import subprocess
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ASL = os.path.expanduser(os.environ.get("ASL", "~/asl/asl"))
P2BIN = os.path.expanduser(os.environ.get("P2BIN", "~/asl/p2bin"))
MAME = os.path.expanduser(os.environ.get("MAME", "~/Workspace/mame"))
CLK = 3579545 / 4 / 60          # 2650 clocks a frame
LUA = r"""
local cpu = manager.machine.devices[":maincpu"]
local mem = cpu.spaces["program"]
local frames, last, at1 = 0, -1, nil
_G.SPIKE = emu.add_machine_frame_notifier(function()
  frames = frames + 1
  local v = mem:read_u8(0x18D2)
  if v ~= last then print("MARK " .. v .. " " .. frames); last = v; if v == 1 then at1 = frames end end
  if at1 and frames == at1 + 30 then manager.machine.video:snapshot(); print("SNAP " .. frames) end
  if v == 4 or frames > 30000 then manager.machine:exit() end
end)
"""


def main(out):
    out = os.path.abspath(out)
    asm = os.path.join(out, "spike.asm")
    open(asm, "w").write(open(os.path.join(HERE, "spike.asm")).read())
    subprocess.run([ASL, "-q", "-L", "-U", "-i", out, "-o", os.path.join(out, "spike.p"),
                    "-olist", os.path.join(out, "spike.lst"), asm], check=True)
    rom = os.path.join(out, "spike.bin")
    subprocess.run([P2BIN, os.path.join(out, "spike.p"), rom, "-r", "$0000-$0FFF", "-l", "0"],
                   check=True, stdout=subprocess.DEVNULL)
    lua = os.path.join(out, "spike.lua")
    open(lua, "w").write(LUA)
    snaps = os.path.join(out, "snap")
    os.makedirs(snaps, exist_ok=True)
    for f in os.listdir(snaps):
        os.remove(os.path.join(snaps, f))
    r = subprocess.run(["timeout", "-s", "KILL", "300", os.path.join(MAME, "mame"), "arcadia", "-cart", rom,
                        "-autoboot_script", lua, "-nothrottle", "-video", "none", "-sound", "none",
                        "-snapshot_directory", snaps, "-snapname", "%i",
                        "-cfg_directory", os.path.join(out, "cfg"), "-nvram_directory", os.path.join(out, "nvram"),
                        "-skip_gameinfo", "-nomouse"], capture_output=True, text=True)
    marks = {}
    for line in r.stdout.splitlines():
        if line.startswith("MARK"):
            _, v, f = line.split()
            marks[int(v)] = int(f)
    print(r.stdout.strip())
    got = Image.open(os.path.join(snaps, sorted(os.listdir(snaps))[0])).convert("RGB")
    want = Image.open(os.path.join(out, "expected.png")).convert("RGB")
    print("snapshot", got.size, "model", want.size)
    if got.size == want.size:
        d = sum(a != b for a, b in zip(got.get_flattened_data(), want.get_flattened_data()))
        print("pixels differing:", d)
        if d:
            diff = Image.new("RGB", got.size)
            diff.putdata([(255, 0, 0) if a != b else (a[0] // 4, a[1] // 4, a[2] // 4)
                          for a, b in zip(got.get_flattened_data(), want.get_flattened_data())])
            diff.save(os.path.join(out, "diff.png"))
    marks.setdefault(3, marks.get(4))
    if 5 in marks and 2 in marks and marks[3]:
        fp = marks[2] - marks[5]
        fs = marks[3] - marks[2]
        print("PLOT: 20736 in %d frames: %.0f clocks a block, %.0f blocks a frame" %
              (fp, fp * CLK / 20736, 20736 / fp))
        print("SPAN: 960 rows (46080 blocks, 15360 cells) in %d frames: %.0f clocks a cell, %.0f blocks a frame" %
              (fs, fs * CLK / 15360, 46080 / fs))


if __name__ == "__main__":
    main(*sys.argv[1:])
