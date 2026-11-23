#!/usr/bin/env bash
# probe.sh -- run a .a52 in MAME headless, print the PC every EVERY frames
# and dump memory at the end: probe.sh CART.a52 OUTDIR FRAMES [EVERY]
# (DUMP="addr:len ..." hex addresses; a snapshot at the end; TRACE_AT=n
# logs every instruction from frame n to OUTDIR/trace.txt)
set -euo pipefail
CART=$(realpath "$1"); OUT=$(realpath -m "$2"); N=${3:-600}; E=${4:-100}
MAME=${MAME:-$HOME/Workspace/mame-a5200}
rm -rf "$OUT"; mkdir -p "$OUT/cfg"
cat > "$OUT/probe.lua" <<L
if _G.__probe then return end
_G.__probe = { n = 0 }
local P = _G.__probe
P.sub = emu.add_machine_frame_notifier(function()
  P.n = P.n + 1
  local cpu = manager.machine.devices[":maincpu"]
  if P.n % $E == 0 then print(string.format("PC %d %04X", P.n, cpu.state["PC"].value)) end
  if P.n == ${TRACE_AT:-0} then manager.machine.debugger:command("trace " .. "$OUT/trace.txt" .. ",0,noloop") end
  if P.n == $N then
    local mem = cpu.spaces["program"]
    for spec in string.gmatch("${DUMP:-}", "%S+") do
      local a, n = string.match(spec, "(%x+):(%d+)")
      local t = {}
      for i = 0, tonumber(n) - 1 do t[#t + 1] = string.format("%02X", mem:read_u8(tonumber(a, 16) + i)) end
      print("DUMP " .. spec .. " " .. table.concat(t, " "))
    end
    manager.machine.screens[":screen"]:snapshot()
    manager.machine:exit()
  end
end)
L
( cd "$MAME" && SDL_VIDEODRIVER=dummy FUJINET_TCP=${FUJINET_TCP:-127.0.0.1:1} \
  ./a5200 a5200 ${TRACE_AT:+-debug -debugger none} -rompath "$HOME/Workspace/mame/roms" -cartslot fujinet -cart "$CART" \
  -snapshot_directory "$OUT" -cfg_directory "$OUT/cfg" -nvram_directory "$OUT/cfg" \
  -autoboot_script "$OUT/probe.lua" -video none -sound none -nothrottle -seconds_to_run 600 2>&1 \
  | grep -E '^(PC|DUMP|fujinet)' )
