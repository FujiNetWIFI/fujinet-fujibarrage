#!/usr/bin/env bash
# rig.sh -- a CoCo client live: a private barraged, fujinet-pc (the CoCo
# build, DriveWire over the Becker port 65504) and MAME's CoCo, all in a
# network namespace of their own (unshare -rn). FujiBlast's
# tools/cocosim/rig.sh, for barraged (tools/a2rig/rig.sh's server side).
#
#   tools/cocosim/rig.sh SCRIPT OUTDIR
#
# Uses r2r/coco/barrage.dsk as built (BUILD=1: `make coco` first). Starts
# barraged on 9913 with DEVSET (barraged -devset; default
# tanks=3,order=robin,timer=20: the dev rooms' bots fill the room when the
# host starts) and fujinet-pc with the disk mounted and the lobby's appkeys
# seeded: name NAME (default CC; CC3 on a CoCo 3), server
# tcp://127.0.0.1:9913/?table=TABLE (default dev1; ROOM= overrides, ROOM=
# empty: none, so the menu). MACHINE=coco3 runs the CoCo 3 build (HDB-DOS
# hdbk3; the CoCo 1/2's is hdbk12). EXTRA: a command run in the namespace
# first (e.g. tools/rigs/tapproxy.py). SCRIPT is cocosim.py's language; the
# game is booted by HDB-DOS from the FujiNet's disk (AUTOEXEC.BAS). The
# copied FujiNet config loses its cloud sections. NOAUTO=1 takes
# AUTOEXEC.BAS off the rig's copy of the disk, so HDB-DOS stops at OK;
# COCO_BOOT (cocosim.py) types the command, e.g. RUNM"BARRAGE" as the
# lobby does. Never the production
# server or the lobby.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SCRIPT=$(realpath "$1")
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-coco}")
FNPC=${FNPC:-$HOME/Workspace/fujinet-pc-coco/build/dist}
MACHINE=${MACHINE:-coco2b}
if [ "${MACHINE#coco3}" != "$MACHINE" ]; then          # coco3, coco3p (PAL)
    NAME=${NAME-CC3}; MAP=$HERE/build/barrage-game3.map; BIOS=${COCO_BIOS:-hdbk3}
else
    NAME=${NAME-CC}; MAP=$HERE/build/barrage-game2.map; BIOS=${COCO_BIOS:-hdbk12}
fi
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9913/?table=$TABLE}
DEVSET=${DEVSET-tanks=3,order=robin,timer=20}
WEB=${WEB:-8123}
mkdir -p "$RIG" "$OUT"
if [ "${BUILD:-0}" = 1 ]; then
    make -C "$HERE" coco > "$RIG/build.log" 2>&1 || { tail -20 "$RIG/build.log"; exit 1; }
fi
( cd "$HERE/server" && go build -o "$RIG/barraged" ./cmd/barraged )

# FujiNet's root: a copy of the CoCo build's, with our disk
rm -rf "${RIG:?}/fn"; mkdir -p "$RIG/fn/SD/FujiNet"
cp -a "$FNPC/fujinet" "$FNPC/data" "$RIG/fn/"
python3 - "$FNPC/fnconfig.ini" "$RIG/fn/fnconfig.ini" <<'PY'
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
    out.append(line)
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
cp "$HERE/r2r/coco/barrage.dsk" "$RIG/fn/SD/barrage.dsk"
if [ "${NOAUTO:-0}" = 1 ]; then    # as the lobby leaves it: no AUTOEXEC.BAS run (COCO_BOOT='RUNM"BARRAGE"')
    "${DECB:-$HOME/Workspace/toolshed/build/unix/decb/decb}" kill "$RIG/fn/SD/barrage.dsk,AUTOEXEC.BAS"
fi
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/fn/SD/FujiNet/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/fn/SD/FujiNet/00010113.key"

export RIG OUT HERE DEVSET EXTRA MACHINE WEB MAP SCRIPT BIOS
exec unshare -rn bash -c '
    ip link set lo up
    BARRAGE_STREAM_DEBUG=${DEBUG:-} "$RIG/barraged" -stream :9913 -seed 7 -devset "$DEVSET" > "$RIG/server.log" 2>&1 &
    pids="$!"
    trap "kill $pids 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9913 " && break; sleep 0.25; done
    if [ -n "${EXTRA:-}" ]; then bash -c "$EXTRA" > "$RIG/extra.log" 2>&1 & pids="$pids $!"; fi
    cd "$RIG/fn"
    ./fujinet -c fnconfig.ini -s SD -u "http://127.0.0.1:$WEB" > "$RIG/fujinet.log" 2>&1 &
    pids="$pids $!"
    for i in $(seq 40); do ss -ltn | grep -q ":65504 " && break; sleep 0.25; done
    sleep 1
    curl -s -X POST "http://127.0.0.1:$WEB/api/v1/drives/1/mount" -H "Content-Type: application/json" \
        -d "{\"path\":\"/barrage.dsk\",\"host_slot\":0,\"mode\":\"r\"}" > "$RIG/mount.log" 2>&1 || true
    cd "$HERE"
    MACHINE=$MACHINE COCO_BECKER=1 COCO_BIOS=$BIOS COCOSIM_SECONDS=${COCOSIM_SECONDS:-300} \
        python3 tools/cocosim/cocosim.py - "$MAP" "$SCRIPT" "$OUT" > "$OUT/cocosim.log" 2>&1
    echo "cocosim exit $?" >> "$OUT/cocosim.log"
    tail -20 "$OUT/cocosim.log"
'
