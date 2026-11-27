#!/usr/bin/env python3
"""chfsim.py -- drive the Channel F FujiBarrage build in MAME from a scenario
(FujiBlast's tools/chfsim/chfsim.py, after M.U.L.E.'s, for this game).

usage: chfsim.py SCENARIO OUTDIR [IMAGE [LISTING]]

Boots MAME's channelf with the FujiNet cartridge device and plays the
scenario, one command a line. Times are MAME frames (the console's own,
60.53 a second). The language is the other sims' (astrocade's astrosim.py):

    test SCENE [ARG]            an offline scene (p_test.inc)
    wait N                      let N frames pass
    snap NAME                   save OUTDIR/NAME.png
    hold DIRS                   hold directions: any of u d l r (or 'none')
    press                       tap the button
    key CHAR|NUM                a key (13 end, 27 leave, 8 back, '0' sit out;
                                one character is that character: 08 is 8)
    pad NAME [FRAMES]           the real controller or a console button held
                                FRAMES (6): push pull up down left right ccw
                                cw, start mode hold time
    peek SYMBOL|ADDR [N]        print N bytes
    poke SYMBOL|ADDR VALUE
    dump FROM TO FILE           save memory (hex addresses) to OUTDIR/FILE
    save SYMBOL LEN FILE        save LEN bytes from SYMBOL to OUTDIR/FILE
    load FILE SYMBOL            a file's bytes into memory from SYMBOL
    vram FILE                   save the 128 x 64 screen (a byte a pixel)
    watch ADDR [N [LEN]]        print the PC of the next N writes to ADDR..+LEN
    show N SYMBOL...            for N frames, print FRAMES and a byte at each
                                symbol whenever one of them changes (SHOW)
    auto N                      the autopilot plays N frames: the host starts, the
                                shop scrolled down and up with a buy each way, our
                                turns aim, twist, open the items line, fire; the
                                defence window done. Checks VRAM at each turn and
                                text screen (AUTO VCHECK on a miss), prints each
                                text screen (AUTO TEXT), a pass watchdog (AUTO
                                HANG: no main-loop pass or script step for
                                600 frames), a summary line (AUTO ...); it ends
                                early 300 frames into GAME OVER
    vcheck                      VRAM against what the game thinks it shows (the
                                field FLD while VMODE says so, the text shadow
                                TXS, the palettes PALS): any differ is a bug
    marks SYMBOL|ADDR           print each write to it with its clock (MARK v
                                clocks-since-the-first-mark frame f at secs)
    strobes N                   for N frames: VRAM strobes (port 0 bit 5),
                                pixels that changed, and the passes and jiffies
                                the game ran (PASSES, JIFFY)
    tick                        print the cart's tick and the game's FRAMES
    page                        print the page mapped
    pc N                        print the CPU's PC for N frames
    trace FILE | trace off      log every instruction to OUTDIR/FILE (slow)
    traceon ADDR FILE [N [V]]   the same from the first write (of V) to ADDR, N frames

Input goes through the game's test hook (INPHK in input.inc); symbols come
from the listing's symbol table (build/channelf/fujiblst.lst).

The settle guard is always on: a write to port 0, 1, 4 or 5 sooner than
SETTLE clocks (the listing's) after a VRAM strobe's $50 is counted, and
the run ends with "SETTLE n early (min m clocks) ...". CHFSIM_SETTLE=N
checks against N instead.

Environment:
    CHFSIM_FUJINET=host:port    a fujinet-pc's BoIP listener (live run,
                                throttled: the server's clock is real time);
                                default 127.0.0.1:9, nothing there
    CHFSIM_SECONDS=N            give up after N emulated seconds (600)
    CHFSIM_VERBOSE=1            MAME's output too
    CHFSIM_WAV=FILE             the sound recorded to FILE
    CHFSIM_GAPS=1               the settle guard's gaps (strobe to the next
                                write to port 1, 4 or 5) counted, printed
    CHFSIM_CART=FILE            boot this image instead (CONFIG: ours comes later
                                from the SD; the symbols are still ours)
    CHFSIM_MACHINE=NAME         MAME's machine (channelf; sabavdpl a PAL one, its
                                F8 at 2 MHz: CHFSIM_CLOCK=2000000)
    CHFSIM_CLOCK=HZ             the CPU's clock for the settle guard and marks
                                (1789772.5, the NTSC console's; 2000000 PAL)
"""
import os
import re
import subprocess
import sys

MAME = os.environ.get("MAME", os.path.expanduser("~/Workspace/mame"))
HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))


def symbols(lst):
    """AS's listing: 'NAME : VALUE C |' entries in the symbol table."""
    text = open(lst, encoding="latin-1").read()
    syms = {}
    for name, val in re.findall(r"([A-Z_][A-Z0-9_]*)\s*:\s*([0-9A-F]+)\s+\S\s*\|", text):
        syms.setdefault(name.upper(), int(val, 16))
    return syms


DIRBITS = {"d": 1, "r": 2, "u": 4, "l": 8}
PADS = {"push": (":RIGHT_C", "P1 Push Down"), "pull": (":RIGHT_C", "P1 Pull Up"),
        "up": (":RIGHT_C", "P1 Up"), "down": (":RIGHT_C", "P1 Down"),
        "left": (":RIGHT_C", "P1 Left"), "right": (":RIGHT_C", "P1 Right"),
        "ccw": (":RIGHT_C", "P1 Twist Counterclockwise"), "cw": (":RIGHT_C", "P1 Twist Clockwise"),
        "time": (":PANEL", "TIME (Button 1)"), "hold": (":PANEL", "HOLD (Button 2)"),
        "mode": (":PANEL", "MODE (Button 3)"), "start": (":PANEL", "START (Button 4)")}


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    scen, out = sys.argv[1:3]
    image = sys.argv[3] if len(sys.argv) > 3 else os.path.join(TOP, "r2r/channelf/barrage.bin")
    lst = sys.argv[4] if len(sys.argv) > 4 else os.path.join(TOP, "build/channelf/barrage.lst")
    out = os.path.abspath(out)
    os.makedirs(out, exist_ok=True)
    syms = symbols(lst)

    def sym(n):
        """A number, a symbol, or a sum of them (ST+389)."""
        total = 0
        for part in n.split("+"):
            try:
                total += int(part, 0)
            except ValueError:
                total += syms[part.upper()]
        return total

    steps = []
    for line in open(scen):
        line = line.split("#")[0].strip()
        if not line:
            continue
        w = line.split()
        c = w[0]
        if c == "test":
            args = [int(x) for x in w[1:]] + [0, 0]
            steps.append('{"test", %d, %d, %d}' % (sym("TESTHK"), args[0], args[1]))
        elif c == "wait":
            steps.append('{"wait", %d}' % int(w[1]))
        elif c == "snap":
            steps.append('{"snap", "%s"}' % w[1])
        elif c == "waitfor":        # waitfor SYM VAL [MAXFRAMES]: until the byte is VAL
            steps.append('{"waitfor", %d, %d, %d}' % (sym(w[1]), int(w[2], 0), int(w[3]) if len(w) > 3 else 36000))
        elif c == "hold":
            bits = 0 if w[1] == "none" else sum(DIRBITS[ch] for ch in w[1])
            steps.append('{"poke", %d, 1}' % sym("INPHK"))
            steps.append('{"poke", %d, %d}' % (sym("INPHKD"), bits))
        elif c == "press":
            steps.append('{"poke", %d, 1}' % sym("INPHK"))
            steps.append('{"poke", %d, 1}' % sym("INPHKB"))
            steps.append('{"wait", 3}')
            steps.append('{"poke", %d, 0}' % sym("INPHKB"))
        elif c == "key":
            k = int(w[1]) if w[1].isdigit() and len(w[1]) > 1 else ord(w[1][0])
            steps.append('{"poke", %d, 1}' % sym("INPHK"))
            steps.append('{"poke", %d, %d}' % (sym("INPHKK"), k))
            steps.append('{"wait", 2}')
        elif c == "peek":
            steps.append('{"peek", "%s", %d, %d}' % (w[1], sym(w[1]), int(w[2]) if len(w) > 2 else 1))
        elif c == "poke":
            steps.append('{"poke", %d, %d}' % (sym(w[1]), int(w[2], 0)))
        elif c == "dump":
            steps.append('{"dump", %d, %d, "%s"}' % (int(w[1], 16), int(w[2], 16), os.path.join(out, w[3])))
        elif c == "load":           # load FILE SYMBOL: a file's bytes into memory there
            steps.append('{"load", "%s", %d}' % (os.path.abspath(w[1]), sym(w[2])))
        elif c == "save":           # save SYMBOL LEN FILE: memory to OUTDIR/FILE
            steps.append('{"dump", %d, %d, "%s"}' % (sym(w[1]), sym(w[1]) + int(w[2], 0) - 1, os.path.join(out, w[3])))
        elif c == "vram":
            steps.append('{"vram", "%s"}' % os.path.join(out, w[1]))
        elif c == "watch":
            steps.append('{"watch", %d, %d, %d}' % (sym(w[1]), int(w[2]) if len(w) > 2 else 20,
                                                   int(w[3], 0) if len(w) > 3 else 1))
        elif c == "show":
            steps.append('{"show", %d, {%s}}' % (int(w[1]), ", ".join(str(sym(x)) for x in w[2:])))
        elif c == "traceon":
            steps.append('{"traceon", %d, "%s", %d, %d}' % (sym(w[1]), os.path.join(out, w[2]), int(w[3]) if len(w) > 3 else 10,
                                                           int(w[4], 0) if len(w) > 4 else -1))
        elif c == "iowatch":        # iowatch PORT MASK [N]: writes with (data & MASK) set
            steps.append('{"iowatch", %d, %d, %d}' % (int(w[1], 0), int(w[2], 0), int(w[3]) if len(w) > 3 else 20))
        elif c == "reset":          # the console's RESET (MAME's soft reset)
            steps.append('{"reset"}')
        elif c == "vcheck":
            steps.append('{"vcheck"}')
        elif c == "pad":            # pad NAME FRAMES: the real controller (CONFIG has no hook)
            port, field = PADS[w[1]]
            steps.append('{"pad", "%s", "%s", %d}' % (port, field, int(w[2]) if len(w) > 2 else 6))
        elif c == "auto":           # auto N: the autopilot plays N frames
            steps.append('{"auto", %d}' % int(w[1]))
        elif c == "marks":
            steps.append('{"marks", %d}' % sym(w[1]))
        elif c == "strobes":
            steps.append('{"strobes", %d, %d, %d}' % (int(w[1]), sym("PASSES"), sym("JIFFY")))
        elif c == "tick":
            steps.append('{"tick"}')
        elif c == "page":
            steps.append('{"page"}')
        elif c == "trace":
            f = "off" if w[1] == "off" else os.path.join(out, w[1])
            steps.append('{"trace", "%s"}' % f)
        elif c == "pc":
            steps.append('{"pc", %d}' % (int(w[1]) if len(w) > 1 else 1))
        else:
            sys.exit("unknown command: " + line)

    lua = ("if _G._chfsim_ran then return end  -- (MAME runs it again after a soft reset)\n"
           "_G._chfsim_ran = true\n"
           "local steps = {\n" + ",\n".join(steps) + "\n}\n") + r"""
local out = "%(out)s"
local FRAMES, CURPG, FLD, TXS, PALS, VMODE, FNT = %(frames)d, %(curpg)d, %(fld)d, %(txs)d, %(pals)d, %(vmode)d, %(fnt)d
local SETTLE, CLOCK = %(settle)d, %(clock)s
local function now() return manager.machine.time:as_double() * CLOCK end
GAPS = %(gaps)s
local lastsb, nearly, mingap, firstbad, ncall = -1e9, 0, 1e9, nil, 0
_G._chfguard = manager.machine.devices[":maincpu"].spaces["io"]:install_write_tap(0, 5, "chfguard", function(off, data, mask)
  ncall = ncall + 1
  if off == 0 or off == 1 or off == 4 or off == 5 then
    local t = now()
    local gap = t - lastsb
    if gap < mingap then mingap = gap end
    if GAPS and off ~= 0 then local g = math.floor(gap + 0.5); GAPS[g] = (GAPS[g] or 0) + 1 end
    if gap < SETTLE - 0.5 then
      nearly = nearly + 1
      if firstbad == nil then
        local st = manager.machine.devices[":maincpu"].state
        firstbad = string.format("port %%d <- %%02X %%.0f clocks after, PC %%04X page %%d", off, data, gap, st["PC0"].value, mem:readv_u8(CURPG))
      end
    end
    if off == 0 and data == 0x50 then lastsb = t end
  end
  return data
end)
local mark0 = nil
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local vram = manager.machine.memory.regions[":vram"]
local i, frames, wait_until, pcdump, asked, started = 1, 0, nil, 0, false, false
local nstrobe, counting, prev = 0, false, nil
local function vsnap() local t = {} for a = 0, 128 * 64 - 1 do t[a] = vram:read_u8(a) & 3 end return t end
_G._chfstrobe = manager.machine.devices[":maincpu"].spaces["io"]:install_write_tap(0, 0, "chfstrobe", function(off, data, mask)
  if counting and (data & 0x20) ~= 0 then nstrobe = nstrobe + 1 end
  return data
end)
local nchanged, sframes, p0, j0 = 0, 0, 0, 0
local show_last = ""
traceoff = nil
local function vcheck_now()
  local e = {}
  for a = 0, 128 * 64 - 1 do e[a] = 0 end
  if mem:readv_u8(VMODE) == 1 then
    for x = 0, 101 do for y = 0, 45 do
      e[(y + 4) * 128 + x + 4] = ((~mem:readv_u8(FLD + 64 * x + y)) >> 6) & 3
    end end
  end
  local rowy = {5, 11, 17, 23, 29, 35, 41, 47, 53, 50, 56}
  for r = 0, 10 do for c = 0, 23 do
    local a = TXS + 48 * r + 2 * c
    local ch, ink = mem:readv_u8(a), ((~mem:readv_u8(a + 1)) >> 6) & 3
    if ch > 0x20 and ch < 0x60 then
      for gy = 0, 4 do
        local bits = mem:readv_u8(FNT + 64 * gy + ch - 32)
        for gx = 0, 2 do
          if (bits >> (2 - gx)) & 1 == 1 then e[(rowy[r + 1] + gy) * 128 + 7 + 4 * c + gx] = ink end
        end
      end
    end
  end end
  for y = 0, 63 do
    local p = mem:readv_u8(PALS + y)
    e[y * 128 + 125] = (p & 1) * 2
    e[y * 128 + 126] = (p & 2)
  end
  local bad, first = 0, nil
  for a = 0, 128 * 64 - 1 do
    local v = vram:read_u8(a) & 3
    if v ~= e[a] then
      bad = bad + 1
      if first == nil then first = string.format("(%%d,%%d) vram %%d want %%d", a %% 128, a // 128, v, e[a]) end
    end
  end
  return bad, first
end

-- ---- the autopilot (auto N): plays the game for N frames ----------------------
local AS = %(autosyms)s
local A = nil
local auto_until = nil
local function rd(a) return mem:readv_u8(a) end
local function wr(a, v) mem:write_u8(a, v) end
local function txt_rows()
  local out = {}
  for r = 0, 10 do
    local t = {}
    for c = 0, 23 do
      local ch = rd(TXS + 48 * r + 2 * c)
      t[#t + 1] = (ch >= 0x20 and ch < 0x60) and string.char(ch) or "?"
    end
    out[#out + 1] = table.concat(t)
  end
  return out
end
function auto_init()
  A = {turns = 0, shops = 0, screens = 0, vchecks = 0, vbad = 0, hangs = 0, fired = 0,
       shows = -1, showsat = frames, myturn = 0, turnat = 0, plan = {}, release = {},
       passes = -1, passat = frames, hung = false, shopn = 0, k = 0, over = false}
  wr(AS.INPHK, 1)
end
local function vc(why)
  local bad, first = vcheck_now()
  A.vchecks = A.vchecks + 1
  if bad > 0 then
    A.vbad = A.vbad + 1
    print(string.format("AUTO VCHECK %%d differ at %%s (frame %%d)%%s", bad, why, frames, first and (", first " .. first) or ""))
  end
end
-- a plan: a list of {frames to wait, action}; actions: "b" push, "k:N" a key,
-- "h:BITS:N" hold directions N frames
local function plan(list) A.plan = list; A.pat = frames end
local function act(a)
  if a == "b" then wr(AS.INPHKB, 1); A.release[#A.release + 1] = {frames + 3, AS.INPHKB}
  elseif a:sub(1, 2) == "k:" then wr(AS.INPHKK, tonumber(a:sub(3)))
  elseif a:sub(1, 2) == "h:" then
    local b, n = a:match("h:(%%d+):(%%d+)")
    wr(AS.INPHKD, tonumber(b)); A.release[#A.release + 1] = {frames + tonumber(n), AS.INPHKD}
  end
end
function auto_frame()
  for n = #A.release, 1, -1 do
    if frames >= A.release[n][1] then wr(A.release[n][2], 0); table.remove(A.release, n) end
  end
  -- the watchdog: the main loop's passes and the player's script pointer
  -- (a segment can play 10 s)
  local ps = rd(AS.PASSES) * 65536 + rd(AS.PASSES + 1) * 256 + rd(AS.PP + 1) * 4096 + rd(AS.PP)
  if ps ~= A.passes then A.passes = ps; A.passat = frames; A.hung = false
  elseif frames - A.passat > 600 and not A.hung then
    A.hung = true; A.hangs = A.hangs + 1
    local st = manager.machine.devices[":maincpu"].state
    print(string.format("AUTO HANG no pass for 600 frames at frame %%d, PC %%04X page %%d", frames, st["PC0"].value, rd(CURPG)))
  end
  local shows, myturn = rd(AS.SHOWS), rd(AS.MYTURN)
  if shows ~= A.shows then
    A.shows = shows; A.showsat = frames; A.screens = A.screens + 1; A.plan = {}; A.shopn = 0
    if shows == 2 then A.shops = A.shops + 1 end
  end
  if shows == 5 and frames - A.showsat > 300 then A.over = true end
  if frames - A.showsat == 120 and shows ~= 3 and shows ~= 0 then      -- a text screen: its words
    vc("screen " .. shows)
    local t = txt_rows()
    print(string.format("AUTO TEXT screen %%d frame %%d", shows, frames))
    for r = 1, 11 do if t[r]:match("%%S") then print("  |" .. t[r] .. "|") end end
  end
  if myturn == 1 and A.myturn == 0 then
    A.turns = A.turns + 1; A.turnat = frames; A.k = A.k + 1
    local v = A.k %% 4
    local p = {{60, "h:8:12"}, {20, "h:4:8"}}                          -- aim: left, up
    if v == 1 then p = {{60, "h:2:20"}, {20, "h:1:6"}} end            -- right, down
    if v == 2 then p = {{40, "k:2"}, {20, "k:2"}, {20, "h:8:6"}} end  -- twist: weapons
    if v == 3 then p = {{40, "k:8"}, {30, "h:2:3"}, {30, "h:2:3"}, {30, "h:8:3"}, {30, "k:8"}, {20, "h:8:4"}} end   -- the items line
    p[#p + 1] = {40, "b"}
    plan(p)
  end
  if myturn == 0 and A.myturn == 1 then A.fired = A.fired + 1 end
  if myturn == 1 and frames - A.turnat == 30 then vc("turn") end
  A.myturn = myturn
  if #A.plan == 0 then
    if shows == 1 and frames - A.showsat > 90 and rd(AS.ST + AS.OFF_HOST) == rd(AS.ST + AS.OFF_MY_SEAT) then
      plan({{30, "b"}})                                                  -- the host starts it (push)
    elseif shows == 2 and A.shopn == 0 and frames - A.showsat > 60 then  -- the shop: down, buy, up, buy, done
      A.shopn = 1
      local p = {}
      for n = 1, 12 do p[#p + 1] = {12, "h:1:3"} end
      p[#p + 1] = {20, "b"}
      for n = 1, 9 do p[#p + 1] = {12, "h:4:3"} end
      p[#p + 1] = {20, "b"}
      p[#p + 1] = {20, "h:8:3"}
      p[#p + 1] = {40, "k:13"}
      plan(p)
    elseif shows == 3 and rd(AS.HDEF) == 1 and frames - A.showsat > 30 then
      plan({{60, "h:2:3"}, {30, "k:13"}})                                -- the defence window: done
    end
  end
  if #A.plan > 0 and frames - A.pat >= A.plan[1][1] then
    act(A.plan[1][2]); table.remove(A.plan, 1); A.pat = frames
  end
end
local exiting = false
function quit()                 -- (once: MAME calls this notifier again on its way out)
  if not exiting then exiting = true; manager.machine:exit() end
end
padrel = {}
_G._chfsim = emu.add_machine_frame_notifier(function()
  if exiting then return end
  frames = frames + 1
  for n = #padrel, 1, -1 do
    if frames >= padrel[n][1] then padrel[n][2]:set_value(0); table.remove(padrel, n) end
  end
  if frames %% 300 == 0 then
    local st = manager.machine.devices[":maincpu"].state
    io.stderr:write(string.format("HB frame %%d PC %%04X page %%d t %%.1f\n", frames, st["PC0"].value,
      mem:readv_u8(CURPG), manager.machine.time:as_double()))
    io.stderr:flush()
  end
  if traceoff and frames >= traceoff then manager.machine.debugger:command("trace off"); traceoff = nil end
  if frames > 60 * %(secs)d then quit() end
  if pcdump > 0 then
    local st = manager.machine.devices[":maincpu"].state
    print(string.format("PC %%04X DC0 %%04X page %%d", st["PC0"].value, st["DC0"].value, mem:readv_u8(CURPG)))
    pcdump = pcdump - 1
    return
  end
  if not started then
    local s = steps[1]
    if s ~= nil and s[1] == "test" then
      local h0 = mem:readv_u8(s[2])
      if h0 == 0xA5 and asked then started = true; i = 2
      elseif not asked and h0 == 0x3C and mem:readv_u8(s[2] + 1) == 0xC3 then
        mem:write_u8(s[2] + 2, s[3]); mem:write_u8(s[2] + 3, s[4])
        mem:write_u8(s[2], 0x5A)
        asked = true
        return
      else
        if frames > 3000 then print("CHFSIM: the game never asked for the test hook"); quit() end
        return
      end
    else started = true end
  end
  while i <= #steps do
    local s = steps[i]
    if s[1] == "wait" then
      if wait_until == nil then wait_until = frames + s[2] end
      if frames >= wait_until then wait_until = nil; i = i + 1 else return end
    elseif s[1] == "waitfor" then
      if wait_until == nil then wait_until = frames + s[4] end
      if mem:readv_u8(s[2]) == s[3] then wait_until = nil; i = i + 1
      elseif frames >= wait_until then
        print("CHFSIM: waitfor timed out")
        io.stderr:write("CHFSIM: waitfor timed out at frame " .. frames .. "\n"); io.stderr:flush()
        quit(); return
      else return end
    elseif s[1] == "poke" then
      mem:write_u8(s[2], s[3]); i = i + 1
    elseif s[1] == "snap" then
      manager.machine.video:snapshot()
      print("SNAP " .. s[2] .. " frame " .. frames)
      local f = io.open(out .. "/snaps.txt", "a"); f:write(s[2] .. "\n"); f:close()
      i = i + 1
      return
    elseif s[1] == "dump" then
      local f = io.open(s[4], "wb")
      for a = s[2], s[3] do f:write(string.char(mem:readv_u8(a))) end
      f:close(); i = i + 1
    elseif s[1] == "load" then
      local f = io.open(s[2], "rb")
      local data = f:read("*a")
      f:close()
      for k = 1, #data do mem:write_u8(s[3] + k - 1, string.byte(data, k)) end
      print("LOAD " .. #data .. " bytes")
      i = i + 1
    elseif s[1] == "vram" then
      local f = io.open(s[2], "wb")
      for a = 0, 128 * 64 - 1 do f:write(string.char(vram:read_u8(a) & 3)) end
      f:close(); i = i + 1
    elseif s[1] == "watch" then
      local left = s[3]
      _G["_chfwatch" .. s[2]] = mem:install_write_tap(s[2], s[2] + s[4] - 1, "chfwatch" .. s[2], function(off, data, mask)
        if left > 0 then
          left = left - 1
          local st = manager.machine.devices[":maincpu"].state
          local ks, regs = "", nil
          for k, v in pairs(manager.machine.memory.shares) do if k:find("regs") then regs = v end end
          if regs then
            local sp = regs:read_u8(59)
            for r = 0, 8 do ks = ks .. string.format("r%%d=%%02X ", r, regs:read_u8(r)) end
            ks = ks .. string.format("ksp=%%02X ", sp)
            for r = 16, sp - 2, 2 do ks = ks .. string.format("%%02X%%02X ", regs:read_u8(r), regs:read_u8(r + 1)) end
          end
          print(string.format("WATCH %%04X <- %%02X  PC %%04X PC1 %%04X K %%04X page %%d frame %%d  kstack %%s", off, data, st["PC0"].value,
                st["PC1"].value, st["K"] and st["K"].value or 0, mem:readv_u8(CURPG), frames, ks))
        end
        return data
      end)
      i = i + 1
    elseif s[1] == "traceon" then
      local armed = true
      _G["_chftron" .. s[2]] = mem:install_write_tap(s[2], s[2], "chftron" .. s[2], function(off, data, mask)
        if armed and (s[5] < 0 or data == s[5]) then
          armed = false
          manager.machine.debugger:command("trace " .. s[3] .. ",0")
          traceoff = frames + s[4]
          print(string.format("TRACEON %%04X <- %%02X frame %%d", off, data, frames))
        end
        return data
      end)
      i = i + 1
    elseif s[1] == "iowatch" then
      local left = s[4]
      _G["_chfio" .. s[2]] = manager.machine.devices[":maincpu"].spaces["io"]:install_write_tap(s[2], s[2], "chfio" .. s[2], function(off, data, mask)
        if left > 0 and (data & s[3]) ~= 0 then
          left = left - 1
          local st = manager.machine.devices[":maincpu"].state
          print(string.format("IOWATCH port %%d <- %%02X  PC %%04X PC1 %%04X frame %%d", off, data, st["PC0"].value, st["PC1"].value, frames))
        end
        return data
      end)
      i = i + 1
    elseif s[1] == "reset" then
      manager.machine:soft_reset()
      print("RESET frame " .. frames)
      i = i + 1
      return
    elseif s[1] == "pad" then
      local f = manager.machine.ioport.ports[s[2]].fields[s[3]]
      f:set_value(1)
      padrel[#padrel + 1] = {frames + s[4], f}
      i = i + 1
    elseif s[1] == "vcheck" then
      local bad, first = vcheck_now()
      print("VCHECK " .. bad .. " differ" .. (first and (", first " .. first) or "") .. "  (frame " .. frames .. ")")
      i = i + 1
    elseif s[1] == "auto" then
      if auto_until == nil then auto_until = frames + s[2]; auto_init() end
      if frames >= auto_until or A.over then
        auto_until = nil; i = i + 1
        print(string.format("AUTO %%d turns, %%d shops, %%d screens, %%d vchecks (%%d bad), %%d hangs, shots fired %%d%%s",
          A.turns, A.shops, A.screens, A.vchecks, A.vbad, A.hangs, A.fired,
          A.over and ", the game over" or ""))
      else auto_frame(); return end
    elseif s[1] == "marks" then
      _G["_chfmark" .. s[2]] = mem:install_write_tap(s[2], s[2], "chfmark" .. s[2], function(off, data, mask)
        local t = now()
        if mark0 == nil then mark0 = t end
        print(string.format("MARK %%d %%.0f frame %%d at %%.6f", data, t - mark0, frames,
          manager.machine.time:as_double()))
        return data
      end)
      i = i + 1
    elseif s[1] == "strobes" then
      if not counting then
        counting = true; nstrobe = 0; nchanged = 0; sframes = 0; prev = vsnap()
        p0 = mem:readv_u8(s[3]) * 256 + mem:readv_u8(s[3] + 1)
        j0 = mem:readv_u8(s[4]) + 256 * mem:readv_u8(s[4] + 1)
        return
      end
      local cur = vsnap()
      for a = 0, 128 * 64 - 1 do if cur[a] ~= prev[a] then nchanged = nchanged + 1 end end
      prev = cur
      sframes = sframes + 1
      if sframes >= s[2] then
        counting = false
        local p1 = mem:readv_u8(s[3]) * 256 + mem:readv_u8(s[3] + 1)
        local j1 = mem:readv_u8(s[4]) + 256 * mem:readv_u8(s[4] + 1)
        print(string.format("STROBES %%d frames: %%d strobes, %%d pixels changed (%%d redundant), %%d passes, %%d jiffies",
              s[2], nstrobe, nchanged, nstrobe - nchanged, (p1 - p0) & 0xFFFF, (j1 - j0) & 0xFFFF))
        i = i + 1
      else return end
    elseif s[1] == "tick" then
      print(string.format("TICK cart %%d FRAMES %%d mame %%d", mem:readv_u8(0xFC0E) + 256 * mem:readv_u8(0xFC0F),
            mem:readv_u8(FRAMES) + 256 * mem:readv_u8(FRAMES + 1), frames))
      i = i + 1
    elseif s[1] == "page" then
      print("PAGE " .. mem:readv_u8(CURPG)); i = i + 1
    elseif s[1] == "trace" then
      if s[2] == "off" then manager.machine.debugger:command("trace off")
      else manager.machine.debugger:command(string.format("trace %%s,maincpu,noloop,{tracelog \"%%%%02X \",b@%%X}", s[2], CURPG)) end
      i = i + 1
    elseif s[1] == "pc" then
      pcdump = s[2]; i = i + 1; return
    elseif s[1] == "show" then
      if wait_until == nil then wait_until = frames + s[2]; show_last = "" end
      local t = {}
      for _, a in ipairs(s[3]) do t[#t + 1] = string.format("%%d", mem:readv_u8(a)) end
      local v = table.concat(t, " ")
      if v ~= show_last then
        print("SHOW " .. (mem:readv_u8(FRAMES) + 256 * mem:readv_u8(FRAMES + 1)) .. " " .. v)
        show_last = v
      end
      if frames >= wait_until then wait_until = nil; i = i + 1 else return end
    elseif s[1] == "peek" then
      local t = {}
      for k = 0, s[4] - 1 do t[#t + 1] = string.format("%%02X", mem:readv_u8(s[3] + k)) end
      print("PEEK " .. s[2] .. " " .. table.concat(t, " ") .. "  (frame " .. frames .. ")")
      i = i + 1
    end
  end
  if i > #steps then i = i + 1; if i > #steps + 2 then
    print(string.format("SETTLE %%d early (min %%.0f clocks, guard %%d, %%d writes)%%s", nearly, mingap, SETTLE, ncall,
          firstbad and (", first " .. firstbad) or ""))
    if GAPS then
      local ks = {}
      for g, n in pairs(GAPS) do if g < 400 then ks[#ks + 1] = g end end
      table.sort(ks)
      for _, g in ipairs(ks) do print(string.format("GAP %%d clocks: %%d writes", g, GAPS[g])) end
    end
    quit() end end
end)
""" % {"out": out, "frames": sym("FRAMES"), "curpg": sym("CURPG"), "fld": sym("FLD"), "txs": sym("TXS"),
       "pals": sym("PALS"), "vmode": sym("VMODE"), "fnt": sym("FNTR0"),
       "settle": int(os.environ.get("CHFSIM_SETTLE", sym("SETTLE"))),
       "clock": os.environ.get("CHFSIM_CLOCK", "1789772.5"),
       "secs": int(os.environ.get("CHFSIM_SECONDS", "600")),
       "gaps": "{}" if os.environ.get("CHFSIM_GAPS") else "nil",
       "autosyms": "{" + ", ".join("%s = %d" % (n, sym(n)) for n in (
           "INPHK", "INPHKD", "INPHKB", "INPHKK", "PASSES", "SHOWS", "MYTURN", "ST",
           "OFF_HOST", "OFF_MY_SEAT", "HDEF", "PP")) + "}"}
    luafile = os.path.join(out, "scenario.lua")
    open(luafile, "w").write(lua)
    snapdir = os.path.join(out, "raw")
    subprocess.run(["rm", "-rf", snapdir, os.path.join(out, "snaps.txt")])
    cfgdir = os.path.join(out, "cfg")
    os.makedirs(cfgdir, exist_ok=True)
    live = os.environ.get("CHFSIM_FUJINET")
    env = dict(os.environ, FUJINET_TCP=live or "127.0.0.1:9",
               SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    cmd = ["./mame", os.environ.get("CHFSIM_MACHINE", "channelf"), "-bios", "sl31253", "-cartslot", "fujinet",
           "-cart", os.path.abspath(os.environ.get("CHFSIM_CART") or image), "-video", "none", "-sound", "none",
           "-snapshot_directory", snapdir, "-cfg_directory", cfgdir,
           "-nvram_directory", os.path.join(out, "nvram"),
           "-autoboot_script", luafile, "-skip_gameinfo"]
    if not live:
        cmd.append("-nothrottle")
    wav = os.environ.get("CHFSIM_WAV")
    if wav:                     # the sound recorded (CHFSIM_WAV=file.wav)
        i = cmd.index("-sound")
        cmd[i + 1] = "auto"
        cmd += ["-wavwrite", os.path.abspath(wav)]
        env["SDL_AUDIODRIVER"] = "dummy"
    if any('"trace"' in x or '"traceon"' in x for x in steps):
        cmd[2:2] = ["-debug", "-debugger", "none"]
    # MAME's stderr is streamed (kept after a kill); its stdout (the scenario's
    # lines) too, so a MAME that hangs on its way out (it can, with the FujiNet
    # link up) still leaves them: killed 60 s past CHFSIM_SECONDS.
    outf = os.path.join(out, "mame.out")
    with open(os.path.join(out, "mame.err"), "w") as err, open(outf, "w") as so:
        try:
            subprocess.run(cmd, stdout=so, stderr=err, text=True, env=env, cwd=MAME,
                           timeout=int(os.environ.get("CHFSIM_SECONDS", "600")) + 60)
        except subprocess.TimeoutExpired:
            print("CHFSIM: MAME did not exit; killed")

    class R:
        pass
    r = R()
    r.stdout = open(outf).read()
    r.stderr = ""
    if os.environ.get("CHFSIM_VERBOSE"):
        print(r.stdout, open(os.path.join(out, "mame.err")).read())
    for line in r.stdout.splitlines():
        if line.startswith(("PEEK", "SNAP", "PC ", "VCHECK", "TICK", "PAGE", "CHFSIM", "WATCH", "TRACEON", "SHOW",
                            "STROBES", "IOWATCH", "RESET", "MARK", "SETTLE", "LOAD")):
            print(line)
    names = []
    if os.path.exists(os.path.join(out, "snaps.txt")):
        names = open(os.path.join(out, "snaps.txt")).read().split()
    d = os.path.join(snapdir, "channelf")
    shots = sorted(os.listdir(d)) if os.path.isdir(d) else []
    for n, f in zip(names, shots):
        os.replace(os.path.join(d, f), os.path.join(out, n + ".png"))
        print(os.path.join(out, n + ".png"))


if __name__ == "__main__":
    main()
