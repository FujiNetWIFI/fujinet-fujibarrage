#!/usr/bin/env bash
# rig.sh -- the Adam client live: a private barraged, FujiNet-PC (the Adam
# build) and adamcore (FujiNet Go Adam's core), all in a network namespace
# of their own (unshare -rn), so nothing reaches the real server
# (FujiBlast's tools/adamrig/rig.sh, for barraged as tools/cocosim/rig.sh).
#
#   tools/adamrig/rig.sh OUTDIR
#
# Uses r2r/adam/barrage.ddp as built (BUILD=1: `make adam` first). Starts
# barraged on 9913 with DEVSET (barraged -devset; default
# tanks=3,order=robin,timer=20: the dev rooms' bots fill the room when the
# host starts) and seeds the lobby's appkeys: name NAME (default ADM),
# server tcp://127.0.0.1:9913/?table=TABLE (default dev1; ROOM= overrides,
# ROOM= empty: none, so the menu). KEYS, FRAMES, EVERY, WAV, CONFIG,
# EMUARGS are run-adam.sh's (its pictures and the end's RAM and VRAM go to
# OUTDIR). EXTRA: a command run in the namespace first (a proxy in front
# of the server: tools/rigs). INSTANCE: another rig at once (its own ports;
# give it its own OUTDIR). Never the production server or the lobby.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$(realpath -m "$1")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-adam}")
NAME=${NAME-ADM}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9913/?table=$TABLE}
DEVSET=${DEVSET-tanks=3,order=robin,timer=20}
mkdir -p "$RIG/keys" "$OUT/shots"
if [ "${BUILD:-0}" = 1 ]; then
    make -C "$HERE" adam > "$RIG/build.log" 2>&1 || { tail -20 "$RIG/build.log"; exit 1; }
fi
( cd "$HERE/server" && go build -o "$RIG/barraged" ./cmd/barraged )
rm -f "$RIG"/keys/*.key
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/keys/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/keys/00010113.key"
export IMAGE RIG OUT HERE DEVSET EXTRA KEYS FRAMES EVERY WAV EMUARGS CONFIG CONFIG_AT INSTANCE
exec unshare -rn bash -c '
    ip link set lo up
    BARRAGE_STREAM_DEBUG=${DEBUG:-} "$RIG/barraged" -stream :9913 -seed 7 -devset "$DEVSET" > "$OUT/server.log" 2>&1 &
    pids="$!"
    trap "kill $pids 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9913 " && break; sleep 0.25; done
    if [ -n "${EXTRA:-}" ]; then bash -c "$EXTRA" > "$OUT/extra.log" 2>&1 & pids="$pids $!"; fi
    APPKEYS="$RIG/keys" SHOTDIR="$OUT/shots" DUMPDIR="$OUT" \
        "$HERE/tools/adamrig/run-adam.sh" "${IMAGE:-$HERE/r2r/adam/barrage.ddp}" > "$OUT/run.log" 2>&1 || true
    tail -3 "$OUT/run.log"
'
