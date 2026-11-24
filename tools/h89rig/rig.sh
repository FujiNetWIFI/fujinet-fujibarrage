#!/usr/bin/env bash
# rig.sh -- the H89 client live: a private server, bots and fujinet-go-h89-
# desktop's headless H89 (its FujiNet in process), all in a network namespace
# of their own (unshare -rn), so nothing reaches the real server.
#
#   tools/h89rig/rig.sh SCENARIO OUTDIR
#
# Uses r2r/h89-cpm/barrage.h8d as built (make h89-cpm; OS=hdos:
# r2r/h89-hdos, HDOS from the boot disk, FN0=1 from the FujiNet's FN0:).
# Starts barraged on 9913 with DEVSET (its bots fill the seats) at TABLE (default
# dev1: the first is host and starts once START players are in, default
# BOTS + 1, for ROUNDS rounds); the lobby's appkeys are seeded: name NAME
# (H89), server tcp://127.0.0.1:9910/?table=TABLE (ROOM= overrides, ROOM=
# empty: none). SCENARIO is tools/h89sim's (live: no warp). EXTRA: a command
# run in the namespace in the background (a proxy in front of the server:
# tools/rigs, with ROOM= through it). fujinet.online is the private server
# in there too (a hosts file over /etc/hosts), so NAME= ROOM= tries the
# no-lobby path: the name, the default server's rooms. Never two at once:
# the H89's ports are fixed.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SCEN=$(realpath "$1")
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-h89}")
NAME=${NAME-H89}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9913/?table=$TABLE}
DEVSET=${DEVSET-tanks=3,order=robin,timer=20}
BOTS=${BOTS:-3}
START=${START:-$((BOTS + 1))}
ROUNDS=${ROUNDS:-2}
OS=${OS:-cpm}
mkdir -p "$RIG" "$OUT"
( cd "$HERE/server" && go build -o "$RIG/barraged" ./cmd/barraged )
DISK=${DISK:-$HERE/r2r/h89-$OS/barrage.h8d}
MAP=${MAP:-$HERE/r2r/h89-$OS/barrage.map}
[ "$OS" = hdos ] && export H89SIM_OS=hdos
[ "${FN0:-}" = 1 ] && export H89SIM_FN0=1
export RIG OUT HERE TABLE EXTRA SCEN DISK MAP NAME ROOM DEVSET
printf '127.0.0.1 localhost fujinet.online\n' > "$RIG/hosts"
exec unshare -rnm bash -c '
    mount --bind "$RIG/hosts" /etc/hosts
    ip link set lo up
    "$RIG/barraged" -stream :9913 -seed 7 -devset "$DEVSET" > "$OUT/server.log" 2>&1 &
    srv=$!
    bots=
    trap "kill $srv \$bots 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9913 " && break; sleep 0.25; done
    if [ -n "${EXTRA:-}" ]; then bash -c "$EXTRA" > "$OUT/extra.log" 2>&1 & bots="$bots $!"; fi
    H89SIM_FUJINET=1 H89SIM_NAME="$NAME" H89SIM_ROOM="$ROOM" \
        python3 "$HERE/tools/h89sim/h89sim.py" "$DISK" "$MAP" "$SCEN" "$OUT" > "$OUT/run.log" 2>&1 || true
    grep -v "^\[fujinet\]" "$OUT/run.log" | tail -40
'
