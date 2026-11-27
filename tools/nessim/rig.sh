#!/usr/bin/env bash
# rig.sh -- the NES client live: a private barraged, FujiNet-PC (the rs232
# build, its BoIP listener) and FujiNet Go NES's core (tools/nessim/nesrun,
# in real time), all in a network namespace of their own (unshare -rn), so
# nothing reaches the real server (the Adam's tools/adamrig/rig.sh).
#
#   tools/nessim/rig.sh SCENARIO OUTDIR
#
# Uses r2r/nes/barrage.nes as built (BUILD=1: `make nes` first) and
# build/nes/barrage.map. Starts barraged on 9913 with DEVSET (barraged
# -devset; default tanks=3,order=robin,timer=20: the dev rooms' bots fill
# the room when the host starts), a copy of fujinet-pc-rs232's dist (BoIP
# on 9988, CONFIG off, its cloud sections left out) with the lobby's
# appkeys: name NAME (default NES), server tcp://127.0.0.1:9913/?table=TABLE
# (default dev1; ROOM= overrides, ROOM= empty: none, so the menu). EXTRA: a
# command run in the namespace first (a proxy in front of the server:
# tools/rigs). NESRUN_REGION as nesrun's. RIG= another rig's directory (one
# at a time each). Never the production server or the lobby.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SCEN=$(realpath "$1")
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-nes}")
NAME=${NAME-NES}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9913/?table=$TABLE}
DEVSET=${DEVSET-tanks=3,order=robin,timer=20}
ROM=${ROM:-$HERE/r2r/nes/barrage.nes}
MAP=${MAP:-$HERE/build/nes/barrage.map}
FNPC_DIST=${FNPC_DIST:-$HOME/Workspace/fujinet-pc-rs232/build/dist}
mkdir -p "$RIG" "$OUT"
if [ "${BUILD:-0}" = 1 ]; then
    make -C "$HERE" nes > "$RIG/build.log" 2>&1 || { tail -20 "$RIG/build.log"; exit 1; }
fi
( cd "$HERE/server" && go build -o "$RIG/barraged" ./cmd/barraged )
[ -x "$HERE/tools/nessim/nesrun" ] || "$HERE/tools/nessim/build.sh"
rm -rf "$RIG/fn"; mkdir -p "$RIG/fn/SD/FujiNet"
cp -a "$FNPC_DIST/fujinet" "$FNPC_DIST/run-fujinet" "$FNPC_DIST/data" "$RIG/fn/"
python3 - "$FNPC_DIST/fnconfig.ini" "$RIG/fn/fnconfig.ini" <<'PY'
import re, sys
out, sec = [], ""
for line in open(sys.argv[1]).read().splitlines():
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
export RIG OUT HERE SCEN ROM MAP DEVSET EXTRA
exec unshare -rn bash -c '
    ip link set lo up
    BARRAGE_STREAM_DEBUG=${DEBUG:-} "$RIG/barraged" -stream :9913 -seed 7 -devset "$DEVSET" > "$OUT/server.log" 2>&1 &
    srv=$!
    ( cd "$RIG/fn" && exec setsid ./run-fujinet -u http://127.0.0.1:8013 -c fnconfig.ini -s SD ) > "$OUT/fn.log" 2>&1 &
    fn=$!
    pids=
    trap "kill -- -$fn 2>/dev/null; kill $srv \$pids 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9913 " && break; sleep 0.25; done
    for i in $(seq 40); do ss -ltn | grep -q ":9988 " && break; sleep 0.25; done
    if [ -n "${EXTRA:-}" ]; then bash -c "$EXTRA" > "$OUT/extra.log" 2>&1 & pids="$!"; fi
    NESRUN_R6LOG=${NESRUN_R6LOG:-} NESRUN_R6FROM=${NESRUN_R6FROM:-} NESRUN_BOIP=127.0.0.1:9988 NESRUN_THROTTLE=1 "$HERE/tools/nessim/nesrun" "$ROM" "$MAP" "$SCEN" "$OUT" > "$OUT/run.log" 2>&1 || true
    tail -5 "$OUT/run.log"
'
