#!/usr/bin/env python3
"""a78sim.py -- drive the 7800 FujiBarrage in MAME from a scenario
(tools/a52sim/a52sim.py's, with FujiBlast's 7800 ports).

usage: a78sim.py CART.a78 GAME.map SCENARIO OUTDIR

Runs the image in MAME's 7800 on the FujiNet 7800 cartridge (MAME with
fujinet-firmware's pico/atari-7800/emu grafted in, ~/Workspace/mame-a7800:
its "fujinet" cart slot; no window, as fast as it goes; MAME's own
settings go to OUTDIR/cfg). A claimed image takes CONFIG's place. Times
are 60ths of a second on the game's clock (frame_count, maria.s: it counts
on through a script). One command a line:

    test SCENE [ARG]            a TEST=1 build's scene (src/test.c): poked into
                                test_hook once the game marks [1] $C3 (the first step)
    wait N                      let N frames pass
    mwait N                     let N of MAME's frames pass
    until SYMBOL VALUE [N]      frames till the byte at SYMBOL is VALUE (N at most)
    snap NAME                   save OUTDIR/NAME.png
    hold DIRS                   hold directions: any of u d l r (or 'none')
    press                       tap the button
    key CHAR|NUM                a key (RETURN = 13, ESC = 27, TAB = 9)
    peek SYMBOL [N]             print N bytes at a symbol (from the map)
    pc N                        print the CPU's PC for N frames, then quit
    trace FILE | trace off      log every instruction to OUTDIR/FILE (slow)
    dump FROM TO FILE           save memory (hex addresses) to OUTDIR/FILE
    poke SYMBOL[+OFFSET] VALUE
    watch N SYMBOL[+OFFSET]...  for N frames, print the frame and a byte at
                                each symbol whenever one of them changes (WATCH)
    pad DIRS|1|2|pause|select|reset|none   the real stick, buttons and
                                switches (MAME's ports), not the test hook
                                (1 the left button, 2 the right)
    swaps N                     N MAME frames later, print the cart's swaps
    wcount N SYMBOL             for N frames, count the stores to SYMBOL
    wpc N SYMBOL|0xADDR         for N frames, print each store's PC and value (WPC)
    burst N PREFIX              a snapshot every frame for N frames: OUTDIR/PREFIX-001.png...
    auto N                      plays the game for N frames of its clock: the host starts
                                (the button, every 2 s in the waiting room), the shop is
                                scrolled to its end and back, a weapon bought and done
                                (RETURN), the defence window done, our turns change
                                weapon (TAB) and fire; logs AUTO lines at each phase,
                                AUTO STUCK when a phase lasts over 90 s or the clock stops
    dmacheck N                  for N frames (in the background: the next steps go on),
                                walk the DLL shown (m_cur) and every zone's display list:
                                MARIA's cycles a line (16, 8 or 10 a header, 3 a direct
                                byte, 6 an indirect character; a zone's last line 6 more
                                and 17 with a DLI below) and each list's shape (ends
                                within its 64-byte slot, headers' widths). Prints DMA MAX
                                as the worst grows, DMA OVER past 400, DL BAD; DMACHECK
                                at the end (the count of each).

The input goes through the game's test hook (inp_hook in input.c) but for
pad; the addresses come from the linker map. A78WAV=out.wav records the
sound; A78SIM_SYSTEM=a7800p is a PAL console. The cartridge is pointed at
a port nothing listens on (FUJINET_TCP), so no fujinet-pc is needed or
disturbed; FUJINET_TCP=host:port names a private fujinet-pc's BoIP and
A78SIM_THROTTLE=1 runs MAME in real time (a live server's clock).
"""
import os
import re
import subprocess
import sys

MAME = os.environ.get("MAME", os.path.expanduser("~/Workspace/mame-a7800"))
ROMS = os.environ.get("ROMS", os.path.expanduser("~/Workspace/mame/roms"))
SECS = int(os.environ.get("A78SIM_SECONDS", "600"))   # a run's limit, in the console's seconds


def symbols(mapfile):
    syms = {}
    text = open(mapfile).read()
    part = text.split("Exports list by name:")[1].split("Exports list by value:")[0]
    for name, addr in re.findall(r"(\S+)\s+([0-9A-F]{6})\s+\S+", part):
        syms[name] = int(addr, 16)
    return syms


DIRBITS = {"u": 1, "d": 2, "l": 4, "r": 8}      # (input.h IN_*)


def main():
    disk, mapfile, scen, out = sys.argv[1:5]
    out = os.path.abspath(out)
    os.makedirs(out, exist_ok=True)
    syms = symbols(mapfile)
    def sym(n):                     # NAME, or NAME+OFFSET[+OFFSET...]
        n, *off = n.split("+")
        return (syms["_" + n] if "_" + n in syms else syms[n]) + sum(int(x, 0) for x in off)
    steps = []
    for line in open(scen):
        line = line.split("#")[0].strip()
        if not line:
            continue
        w = line.split()
        c = w[0]
        if c == "test":
            args = [int(x, 0) for x in w[1:]] + [0, 0]
            steps.append(("test", sym("test_hook"), args[0], args[1]))
        elif c == "until":
            steps.append(("until", sym(w[1]), int(w[2], 0), int(w[3]) if len(w) > 3 else 3600))
        elif c == "wait":
            steps.append(("wait", int(w[1])))
        elif c == "mwait":
            steps.append(("mwait", int(w[1])))
        elif c == "snap":
            steps.append(("snap", w[1]))
        elif c == "hold":
            bits = 0 if w[1] == "none" else sum(DIRBITS[ch] for ch in w[1])
            steps.append(("poke", sym("inp_hook"), 1))
            steps.append(("poke", sym("inp_hook_dir"), bits))
        elif c == "press":
            steps.append(("poke", sym("inp_hook"), 1))
            steps.append(("poke", sym("inp_hook_btn"), 1))
            steps.append(("wait", 3))
            steps.append(("poke", sym("inp_hook_btn"), 0))
        elif c == "key":
            k = int(w[1]) if w[1].isdigit() and len(w[1]) > 1 else ord(w[1][0])
            steps.append(("poke", sym("inp_hook"), 1))
            steps.append(("poke", sym("inp_hook_key"), k))
            steps.append(("wait", 2))
        elif c == "peek":
            steps.append(("peek", w[1], sym(w[1]), int(w[2]) if len(w) > 2 else 1))
        elif c == "dump":
            steps.append(("dump", int(w[1], 16), int(w[2], 16), w[3]))
        elif c == "trace":
            steps.append(("trace", w[1]))
        elif c == "type":
            steps.append(("type", line.split(None, 1)[1].replace("\\n", "\n").replace("\\s", " ")))
        elif c == "pc":
            steps.append(("pc", int(w[1]) if len(w) > 1 else 1))
        elif c == "poke":
            steps.append(("poke", sym(w[1]), int(w[2], 0)))
        elif c == "pad":
            steps.append(("pad", w[1]))
        elif c == "swaps":
            steps.append(("swaps", int(w[1])))
        elif c == "wcount":
            steps.append(("wcount", int(w[1]), sym(w[2])))
        elif c == "watch":
            steps.append(("watch", int(w[1]), [sym(x) for x in w[2:]]))
        elif c == "burst":
            steps.append(("burst", int(w[1]), w[2]))
        elif c == "dmacheck":
            steps.append(("dmacheck", int(w[1])))
        elif c == "auto":
            steps.append(("auto", int(w[1])))
        elif c == "wpc":
            a = int(w[2], 16) if w[2].startswith("0x") else sym(w[2])
            steps.append(("wpc", int(w[1]), a))
        else:
            sys.exit("unknown command: " + line)

    lua = ["local steps = {"]
    for s in steps:
        if s[0] == "poke":
            lua.append('{"poke", %d, %d},' % (s[1], s[2]))
        elif s[0] == "wait":
            lua.append('{"wait", %d},' % s[1])
        elif s[0] == "mwait":
            lua.append('{"mwait", %d},' % s[1])
        elif s[0] == "snap":
            lua.append('{"snap", "%s"},' % s[1])
        elif s[0] == "peek":
            lua.append('{"peek", "%s", %d, %d},' % (s[1], s[2], s[3]))
        elif s[0] == "pc":
            lua.append('{"pc", %d},' % s[1])
        elif s[0] == "pad":
            lua.append('{"pad", "%s"},' % s[1])
        elif s[0] == "swaps":
            lua.append('{"swaps", %d},' % s[1])
        elif s[0] == "wcount":
            lua.append('{"wcount", %d, %d},' % (s[1], s[2]))
        elif s[0] == "watch":
            lua.append('{"watch", %d, {%s}},' % (s[1], ", ".join(str(a) for a in s[2])))
        elif s[0] == "burst":
            lua.append('{"burst", %d, "%s"},' % (s[1], s[2]))
        elif s[0] == "dmacheck":
            lua.append('{"dmacheck", %d},' % s[1])
        elif s[0] == "auto":
            lua.append('{"auto", %d},' % s[1])
        elif s[0] == "wpc":
            lua.append('{"wpc", %d, %d},' % (s[1], s[2]))
        elif s[0] == "dump":
            lua.append('{"dump", %d, %d, "%s"},' % (s[1], s[2], os.path.join(out, s[3])))
        elif s[0] == "type":
            lua.append('{"type", %s},' % ('"' + s[1].replace('"', '\\"').replace("\n", "\\n") + '"'))
        elif s[0] == "trace":
            f = "off" if s[1] == "off" else os.path.join(out, s[1])
            lua.append('{"trace", "%s"},' % f)
        elif s[0] == "test":
            lua.append('{"test", %d, %d, %d},' % s[1:])
        elif s[0] == "until":
            lua.append('{"until", %d, %d, %d},' % s[1:])
    lua.append("}")
    lua.append("""
local FC = %d
local MCUR, DLLB, DLLT, DLLF = %d, %d, %d, %d
local ST, HOOK, HDIR, HBTN, HKEY = %d, %d, %d, %d, %d
local out = "%s"
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local started = false
local i = 1
local wait_until = nil
local last_fc = 0
local frames = 0
local pcdump = 0
local asked = false
local watch_until = nil
local swap_until = nil
local watch_last = ""
-- MAME's 7800 ports: the stick (P1 Up/Down/Left/Right), the buttons, the
-- console's switches (FujiBlast's)
local ports = manager.machine.ioport.ports
local function field(name)
  for _, p in pairs(ports) do
    for fname, f in pairs(p.fields) do
      if fname == name then return f end
    end
  end
  return nil
end
local padnames = { u = "P1 Up", d = "P1 Down", l = "P1 Left", r = "P1 Right",
                   ["1"] = "P1 Button 1", ["2"] = "P1 Button 2",
                   pause = "Pause", select = "Select", reset = "Reset" }
local padheld = {}
local function pad(spec)
  for _, f in pairs(padheld) do f:clear_value() end
  padheld = {}
  if spec == "none" then return end
  local keys = {}
  if padnames[spec] ~= nil then keys = { spec }
  else for ch in spec:gmatch(".") do keys[#keys + 1] = ch end end
  for _, k in ipairs(keys) do
    local f = padnames[k] and field(padnames[k])
    if f ~= nil then f:set_value(1); padheld[#padheld + 1] = f
    else print("PAD no field " .. k) end
  end
end
local DLLS = { [0] = DLLB, [1] = DLLT, [2] = DLLF }
local au = nil          -- the autopilot's state
local function poke(a, v) mem:write_u8(a, v) end
local function austep(now)
  local ph = mem:read_u8(ST)
  local turn = mem:read_u8(ST + 4)
  local me = mem:read_u8(ST + 18)
  local rnd = mem:read_u8(ST + 1)
  if ph ~= au.ph then
    print(string.format("AUTO phase %%d round %%d turn %%d me %%d frame %%d", ph, rnd, turn, me, now))
    au.ph = ph; au.t = now; au.k = 0
  end
  local dt = (now - au.t) & 0xFFFF
  if now == au.last then
    au.still = au.still + 1
    if au.still == 600 then print("AUTO STUCK the clock stopped at " .. now) end
  else au.still = 0 end
  au.last = now
  if dt > 5400 and au.k ~= 99 then
    print(string.format("AUTO STUCK phase %%d for %%d frames (frame %%d)", ph, dt, now)); au.k = 99
  end
  poke(HOOK, 1)
  local dir, btn, key = 0, 0, 0
  if ph == 0 then
    if dt %% 120 == 60 then btn = 1 end
  elseif ph == 1 then
    if dt < 30 then dir = 0
    elseif dt < 300 then dir = 2                      -- down to the end
    elseif dt < 570 then dir = 1                      -- and back up
    elseif dt == 600 then btn = 1                     -- buy the top one
    elseif dt == 660 then key = 13 end                -- done
  elseif ph == 2 then
    if dt == 90 then key = 13 end
  elseif ph == 3 and turn == me then
    if dt == 60 then key = 9                          -- the next weapon
    elseif dt >= 90 and dt < 120 then dir = 4         -- a little left
    elseif dt == 150 then btn = 1 end                 -- fire
  elseif ph == 6 then
    if au.over == nil then au.over = now; print("AUTO GAMEOVER frame " .. now) end
  end
  poke(HDIR, dir)
  poke(HBTN, btn)
  if key ~= 0 then poke(HKEY, key) end
end
local dma = { left = 0, max = 0, over = 0, bad = 0 }
local function dlcost(dl, z)
  -- one line's cycles for the list at dl; nil, why if it is bad
  local c, n = 16, 0
  while true do
    local b1 = mem:read_u8(dl + 1)
    if (b1 & 0x5F) == 0 then return c end
    if n >= 64 then return nil, "no end in 64 bytes" end
    if (b1 & 0x1F) == 0 then
      local w = ((mem:read_u8(dl + 3) ~ 0xFF) & 0x1F) + 1
      local ind = (b1 & 0x20) ~= 0
      c = c + 10 + w * (ind and 6 or 3)
      dl = dl + 5; n = n + 5
    else
      local w = ((b1 ~ 0xFF) & 0x1F) + 1
      c = c + 8 + w * 3
      dl = dl + 4; n = n + 4
    end
  end
end
local function dmaframe(now)
  local cur = mem:read_u8(MCUR)
  local dll = DLLS[cur]
  if dll == nil then return end
  local lines, e = 0, 0
  while lines < 243 and e < 40 do
    local a = dll + e * 3
    local f = mem:read_u8(a)
    local dl = mem:read_u8(a + 1) * 256 + mem:read_u8(a + 2)
    local h = (f & 15) + 1
    local c, why = dlcost(dl, e)
    if c == nil then
      dma.bad = dma.bad + 1
      print(string.format("DL BAD screen %%d entry %%d at %%04X: %%s (frame %%d)", cur, e, dl, why, now))
    else
      local last = c + 6 + (((mem:read_u8(a + 3) & 0x80) ~= 0) and 17 or 0)
      if last > dma.max then
        dma.max = last
        print(string.format("DMA MAX %%d screen %%d entry %%d (frame %%d)", last, cur, e, now))
      end
      if last > 400 then
        dma.over = dma.over + 1
        print(string.format("DMA OVER %%d screen %%d entry %%d (frame %%d)", last, cur, e, now))
      end
    end
    lines = lines + h
    e = e + 1
  end
end
if _G.__a78sim then return end
_G.__a78sim = emu.add_machine_frame_notifier(function()
  frames = frames + 1
  if dma.left > 0 then
    dmaframe(mem:read_u8(FC) + mem:read_u8(FC + 1) * 256)
    dma.left = dma.left - 1
    if dma.left == 0 then
      print(string.format("DMACHECK max %%d over %%d bad %%d", dma.max, dma.over, dma.bad))
    end
  end
  if frames > 60 * %d then manager.machine:exit() end
  if pcdump > 0 then
    print(string.format("PC %%04X", manager.machine.devices[":maincpu"].state["PC"].value))
    pcdump = pcdump - 1
    if pcdump == 0 then manager.machine:exit() end
    return
  end
  local now = mem:read_u8(FC) + mem:read_u8(FC + 1) * 256
  if not started then
    -- until the game has started: the first step may be the test hook,
    -- poked every frame until the game answers $A5
    local s = steps[1]
    if s ~= nil and s[1] == "test" then
      local h0 = mem:read_u8(s[2])
      if h0 == 0xA5 and asked then started = true; i = 2
      elseif not asked and mem:read_u8(s[2] + 1) == 0xC3 then
        -- the game asks (test.c's test_check): the scene, its argument
        mem:write_u8(s[2] + 2, s[3]); mem:write_u8(s[2] + 3, s[4])
        mem:write_u8(s[2], 0x5A)
        asked = true
        return
      else return end
    elseif frames > 60 * 25 then started = true
    else return end
  end
  while i <= #steps do
    local s = steps[i]
    if s[1] == "wait" then
      if wait_until == nil then wait_until = now + s[2] end
      if ((now - wait_until) & 0xFFFF) < 0x8000 then wait_until = nil; i = i + 1 else return end
    elseif s[1] == "poke" then
      mem:write_u8(s[2], s[3]); i = i + 1
    elseif s[1] == "mwait" then
      if swap_until == nil then swap_until = frames + s[2] end
      if frames >= swap_until then swap_until = nil; i = i + 1 else return end
    elseif s[1] == "until" then
      if wait_until == nil then wait_until = now + s[4] end
      if mem:read_u8(s[2]) == s[3] then wait_until = nil; i = i + 1
      elseif ((now - wait_until) & 0xFFFF) < 0x8000 then
        print("UNTIL ran out: " .. s[2]); wait_until = nil; i = i + 1
      else return end
    elseif s[1] == "snap" then
      manager.machine.video:snapshot()
      print("SNAP " .. s[2] .. " game frame " .. now .. " mame frame " .. frames)
      local f = io.open(out .. "/snaps.txt", "a"); f:write(s[2] .. "\\n"); f:close()
      i = i + 1
      return
    elseif s[1] == "dump" then
      local f = io.open(s[4], "wb")
      for a = s[2], s[3] do f:write(string.char(mem:read_u8(a))) end
      f:close(); i = i + 1
    elseif s[1] == "type" then
      manager.machine.natkeyboard:post(s[2]); i = i + 1
    elseif s[1] == "trace" then
      if s[2] == "off" then manager.machine.debugger:command("trace off")
      else manager.machine.debugger:command("trace " .. s[2] .. ",0,noloop") end
      i = i + 1
    elseif s[1] == "pc" then
      pcdump = s[2]; i = i + 1; return
    elseif s[1] == "swaps" then
      -- N MAME frames later (the game's clock stops when CONFIG takes over): the cart's swaps
      if swap_until == nil then swap_until = frames + s[2] end
      if frames >= swap_until then
        print("SWAPS " .. manager.machine.output:get_value("fujinet_swaps")); swap_until = nil; i = i + 1
      else return end
    elseif s[1] == "pad" then
      pad(s[2]); i = i + 1
    elseif s[1] == "wcount" then
      if watch_until == nil then
        watch_until = now + s[2]
        _G.__wc = 0
        _G.__wtap = mem:install_write_tap(s[3], s[3], "wc", function() _G.__wc = _G.__wc + 1 end)
      end
      if ((now - watch_until) & 0xFFFF) < 0x8000 then
        print(string.format("WCOUNT %%d stores %%d frames", _G.__wc, s[2]))
        _G.__wtap:remove(); _G.__wtap = nil
        watch_until = nil; i = i + 1
      else return end
    elseif s[1] == "burst" then
      -- a snapshot every MAME frame for N frames (OUTDIR/PREFIX-nnn.png)
      _G.__burst = (_G.__burst or 0) + 1
      manager.machine.video:snapshot()
      local f = io.open(out .. "/snaps.txt", "a"); f:write(string.format("%%s-%%03d\\n", s[3], _G.__burst)); f:close()
      if _G.__burst >= s[2] then _G.__burst = nil; i = i + 1 end
      return
    elseif s[1] == "dmacheck" then
      dma.left = s[2]; i = i + 1
    elseif s[1] == "auto" then
      if au == nil then au = { ph = -1, t = now, k = 0, last = -1, still = 0, prev = now, n = 0 } end
      au.n = au.n + ((now - au.prev) & 0xFFFF); au.prev = now
      austep(now)
      if au.n >= s[2] or (au.over ~= nil and ((now - au.over) & 0xFFFF) > 300) then
        poke(HOOK, 0); au = nil; i = i + 1
      else return end
    elseif s[1] == "wpc" then
      if watch_until == nil then
        watch_until = now + s[2]
        local cpu = manager.machine.devices[":maincpu"]
        _G.__wptap = mem:install_write_tap(s[3], s[3], "wpc", function(o, d)
          local sp = cpu.state["SP"].value & 0xFF
          local t = {}
          for k = 1, 8 do t[#t + 1] = string.format("%%02X", mem:read_u8(0x100 + ((sp + k) & 0xFF))) end
          print(string.format("WPC %%04X %%02X at %%d stack %%s", cpu.state["PC"].value, d,
            mem:read_u8(FC) + mem:read_u8(FC + 1) * 256, table.concat(t, " "))) end)
      end
      if ((now - watch_until) & 0xFFFF) < 0x8000 then
        _G.__wptap:remove(); _G.__wptap = nil; watch_until = nil; i = i + 1
      else return end
    elseif s[1] == "watch" then
      if watch_until == nil then watch_until = now + s[2]; watch_last = "" end
      local t = {}
      for _, a in ipairs(s[3]) do t[#t + 1] = string.format("%%d", mem:read_u8(a)) end
      local v = table.concat(t, " ")
      if v ~= watch_last then print("WATCH " .. now .. " " .. v); watch_last = v end
      if ((now - watch_until) & 0xFFFF) < 0x8000 then watch_until = nil; i = i + 1 else return end
    elseif s[1] == "peek" then
      local t = {}
      for k = 0, s[4] - 1 do t[#t + 1] = string.format("%%02X", mem:read_u8(s[3] + k)) end
      print("PEEK " .. s[2] .. " " .. table.concat(t, " ") .. "  (mame frame " .. frames .. ")")
      i = i + 1
    end
  end
  if dma.left > 0 then
    print(string.format("DMACHECK max %%d over %%d bad %%d", dma.max, dma.over, dma.bad))
  end
  manager.machine:exit()
end)
""" % (sym("frame_count"), sym("m_cur"), sym("dll_b"), sym("dll_t"), sym("dll_f"),
       sym("st"), sym("inp_hook"), sym("inp_hook_dir"), sym("inp_hook_btn"), sym("inp_hook_key"), out, SECS))
    luafile = os.path.join(out, "scenario.lua")
    open(luafile, "w").write("\n".join(lua))
    snapdir = os.path.join(out, "raw")
    subprocess.run(["rm", "-rf", snapdir, os.path.join(out, "snaps.txt")])
    cfg = os.path.join(out, "cfg")
    system = os.environ.get("A78SIM_SYSTEM", "a7800")
    cmd = ["./a7800", system, "-rompath", ROMS, "-cartslot", "fujinet", "-cart", os.path.abspath(disk),
           "-video", "none", "-sound", "none", "-nothrottle", "-snapshot_directory", snapdir,
           "-cfg_directory", cfg, "-nvram_directory", cfg,
           "-autoboot_script", luafile, "-skip_gameinfo"]
    if os.environ.get("A78SIM_THROTTLE"):  # real time (a live server's clock)
        cmd.remove("-nothrottle")
    if any(x[0] == "trace" for x in steps):
        cmd[2:2] = ["-debug", "-debugger", "none"]
    if os.environ.get("A78WAV"):        # record the sound: A78WAV=out.wav
        i = cmd.index("-sound")
        cmd[i + 1] = "auto"
        cmd += ["-wavwrite", os.path.abspath(os.environ["A78WAV"])]
    env = dict(os.environ)
    env.setdefault("FUJINET_TCP", "127.0.0.1:1")
    env.setdefault("SDL_VIDEODRIVER", "dummy")
    r = subprocess.run(cmd, cwd=MAME, capture_output=True, text=True, timeout=SECS + 300, env=env)
    for line in r.stdout.splitlines():
        if line.startswith(("PEEK", "SNAP", "PC ", "PAD", "WATCH", "WCOUNT", "SWAPS", "UNTIL", "WPC", "[LUA",
                            "DMA", "DL BAD", "AUTO")):
            print(line)
    if r.returncode:
        print(r.stdout[-2000:], r.stderr[-2000:])
    names = []
    if os.path.exists(os.path.join(out, "snaps.txt")):
        names = open(os.path.join(out, "snaps.txt")).read().split()
    sd = os.path.join(snapdir, system)
    shots = sorted(os.listdir(sd)) if os.path.isdir(sd) else []
    for n, f in zip(names, shots):
        os.replace(os.path.join(sd, f), os.path.join(out, n + ".png"))
        print(os.path.join(out, n + ".png"))


if __name__ == "__main__":
    main()
