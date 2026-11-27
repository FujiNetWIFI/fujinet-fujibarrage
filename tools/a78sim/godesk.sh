#!/usr/bin/env bash
# godesk.sh -- the image on FujiNet Go Atari 7800's own core (its desktop's
# a7800_headless: ~/Workspace/fujinet-go-atari7800-desktop/build/core),
# live, as rig.sh runs MAME: a private barraged, fujinet-pc's rs232 build
# over BoIP (9989) with the lobby's appkeys seeded (NAME, ROOM), in a
# network namespace of their own.
#
#   tools/a78sim/godesk.sh OUTDIR FRAMES SHOT_AT [HEADLESS ARGS...]
#
# e.g. --input 1500:BUTTONS:8:8 --input 1510:BUTTONS:8:0 (the host starts),
# --input 2400:CONSOLE:8:0 --input 2410:CONSOLE:8:8 (PAUSE: the shop done).
# Writes OUTDIR/shot.png, OUTDIR/run.json and the server's log.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$(realpath -m "$1"); FRAMES=$2; AT=$3; shift 3
RIG=$(realpath -m "${RIG:-$HERE/build/rig-a78-go}")
NAME=${NAME-A78}
ROOM=${ROOM-tcp://127.0.0.1:9913/?table=${TABLE:-dev1}}
DEVSET=${DEVSET-tanks=3,order=robin,timer=20}
CART=${CART:-$HERE/r2r/atari7800/barrage.a78}
CORE=${CORE:-$HOME/Workspace/fujinet-go-atari7800-desktop/build/core}
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
        line = "port=9989"
    if sec == "BOIP" and line.startswith("enabled="):
        line = "enabled=1"
    out.append(line)
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/fn/SD/FujiNet/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/fn/SD/FujiNet/00010113.key"
export RIG OUT CART CORE DEVSET FRAMES AT
exec unshare -rn bash -c '
    ip link set lo up
    "$RIG/barraged" -stream :9913 -seed 7 -devset "$DEVSET" > "$OUT/server.log" 2>&1 &
    srv=$!
    ( cd "$RIG/fn" && exec setsid ./run-fujinet -u http://127.0.0.1:8017 -c fnconfig.ini -s SD ) > "$OUT/fn.log" 2>&1 &
    fn=$!
    trap "kill -- -$fn $srv 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9913 " && break; sleep 0.25; done
    for i in $(seq 40); do ss -ltn | grep -q ":9989 " && break; sleep 0.25; done
    sleep 1
    cd "$CORE" && ./a7800_headless --image "$CART" --frames "$FRAMES" --port 9989 --shot "$OUT/shot.ppm" --shot-at "$AT" "$@" > "$OUT/run.json" 2> "$OUT/run.err" || true
    python3 -c "from PIL import Image; Image.open(\"$OUT/shot.ppm\").save(\"$OUT/shot.png\")"
' bash "$@"
