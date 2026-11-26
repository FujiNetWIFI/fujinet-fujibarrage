#!/usr/bin/env bash
# rig.sh -- the SMS client live: a private barraged, FujiNet-PC (the rs232
# build, its BoIP listener) and FujiNet Go SMS's core (tools/smssim/smsrun,
# in real time), all in a network namespace of their own (unshare -rn), so
# nothing reaches the real server (the Adam's tools/adamrig/rig.sh).
#
#   tools/smssim/rig.sh SCENARIO OUTDIR
#
# Uses r2r/sms/barrage.sms as built (BUILD=1: `make sms` first) and
# build/sms/barrage.map. Starts barraged on 9913 with DEVSET (barraged
# -devset; default tanks=3,order=robin,timer=20: the dev rooms' bots fill
# the room when the host starts), a copy of fujinet-pc-rs232's dist (BoIP
# on 9988, CONFIG off, its cloud sections left out) with the lobby's
# appkeys: name NAME (default SMS), server tcp://127.0.0.1:9913/?table=TABLE
# (default dev1; ROOM= overrides, ROOM= empty: none, so the menu). EXTRA: a
# command run in the namespace first (a proxy in front of the server:
# tools/rigs). SMSRUN_MODEL as smsrun's. RIG= another rig's directory (one
# at a time each). Never the production server or the lobby.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SCEN=$(realpath "$1")
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-sms}")
NAME=${NAME-SMS}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9913/?table=$TABLE}
DEVSET=${DEVSET-tanks=3,order=robin,timer=20}
ROM=${ROM:-$HERE/r2r/sms/barrage.sms}
MAP=${MAP:-$HERE/build/sms/barrage.map}
FNPC_DIST=${FNPC_DIST:-$HOME/Workspace/fujinet-pc-rs232/build/dist}
mkdir -p "$RIG" "$OUT"
if [ "${BUILD:-0}" = 1 ]; then
    make -C "$HERE" sms > "$RIG/build.log" 2>&1 || { tail -20 "$RIG/build.log"; exit 1; }
fi
( cd "$HERE/server" && go build -o "$RIG/barraged" ./cmd/barraged )
[ -x "$HERE/tools/smssim/smsrun" ] || "$HERE/tools/smssim/build.sh"
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
    SMSRUN_BOIP=127.0.0.1:9988 SMSRUN_THROTTLE=1 "$HERE/tools/smssim/smsrun" "$ROM" "$MAP" "$SCEN" "$OUT" > "$OUT/run.log" 2>&1 || true
    tail -5 "$OUT/run.log"
'
