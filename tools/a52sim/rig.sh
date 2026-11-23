#!/usr/bin/env bash
# rig.sh -- the 5200 client live: a private barraged, FujiNet-PC (the rs232
# build: the 5200 cartridge's bus over BoIP) and MAME's 5200, all in a
# network namespace of their own (unshare -rn), so nothing reaches the real
# server (FujiBlast's tools/a52sim/rig.sh, for barraged as the other ports'
# rigs).
#
#   tools/a52sim/rig.sh SCENARIO OUTDIR
#
# Uses r2r/atari5200/barrage.a52 as built (a playable build: no TEST=1) and
# build/a52/game.map. Starts barraged on 9913 with DEVSET (barraged
# -devset; default tanks=3,order=robin,timer=20: the dev rooms' bots fill
# the room when the host starts), a copy of fujinet-pc-rs232 (BoIP 9988, web
# 8014) with the lobby's appkeys seeded: name NAME (default A52), server
# tcp://127.0.0.1:9913/?table=TABLE (default dev1; ROOM= overrides, ROOM=
# empty: none, so the menu). Then a52sim.py on SCENARIO, throttled (the
# server's clock is wall time); SECS the run's limit (300). EXTRA: a
# command run in the namespace first (a proxy in front of the server:
# tools/rigs). RIG= another rig directory for a second rig at once (give it
# its own OUTDIR). A52WAV=file records the sound. Never the production
# server or the lobby.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SCEN=$(realpath "$1")
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-a52}")
NAME=${NAME-A52}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9913/?table=$TABLE}
DEVSET=${DEVSET-tanks=3,order=robin,timer=20}
SECS=${SECS:-300}
CART=${CART:-$HERE/r2r/atari5200/barrage.a52}
MAP=${MAP:-$HERE/build/a52/game.map}
FNPC_DIST=${FNPC_DIST:-$HOME/Workspace/fujinet-pc-rs232/build/dist}
mkdir -p "$RIG" "$OUT"
( cd "$HERE/server" && go build -o "$RIG/barraged" ./cmd/barraged )

rm -rf "$RIG/fn"; mkdir -p "$RIG/fn/SD/FujiNet"
cp -a "$FNPC_DIST/fujinet" "$FNPC_DIST/run-fujinet" "$FNPC_DIST/data" "$RIG/fn/"
python3 - "$FNPC_DIST/fnconfig.ini" "$RIG/fn/fnconfig.ini" <<'PY'
import re, sys
out, sec = [], ""
for line in open(sys.argv[1]).read().splitlines():
    line = line.rstrip("\r")
    m = re.match(r"\[(.*)\]", line)
    if m:
        sec = m.group(1)
    if sec in ("GoogleDrive", "S3", "OneDrive"):
        continue
    if sec == "General" and line.startswith("configenabled="):
        line = "configenabled=0"
    if sec == "BOIP" and line.startswith("port="):
        line = "port=9988"
    if sec == "BOIP" and line.startswith("enabled="):
        line = "enabled=1"
    out.append(line)
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/fn/SD/FujiNet/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/fn/SD/FujiNet/00010113.key"

export RIG OUT HERE SCEN CART MAP DEVSET EXTRA SECS A52WAV
exec unshare -rn bash -c '
    ip link set lo up
    BARRAGE_STREAM_DEBUG=${DEBUG:-} "$RIG/barraged" -stream :9913 -seed 7 -devset "$DEVSET" > "$OUT/server.log" 2>&1 &
    srv=$!
    ( cd "$RIG/fn" && exec setsid ./run-fujinet -u http://127.0.0.1:8014 -c fnconfig.ini -s SD ) \
        > "$OUT/fn.log" 2>&1 &
    fn=$!
    pids=
    trap "kill -- -$fn $srv \$pids 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9913 " && break; sleep 0.25; done
    for i in $(seq 40); do ss -ltn | grep -q ":9988 " && break; sleep 0.25; done
    if [ -n "${EXTRA:-}" ]; then bash -c "$EXTRA" > "$OUT/extra.log" 2>&1 & pids="$pids $!"; fi
    sleep 1
    FUJINET_TCP=127.0.0.1:9988 A52SIM_THROTTLE=1 A52SIM_SECONDS=$SECS \
        python3 "$HERE/tools/a52sim/a52sim.py" "$CART" "$MAP" "$SCEN" "$OUT" 2>&1 | tail -40
'
