#!/usr/bin/env bash
# rig.sh -- the Astrocade client live: a private barraged, a fujinet-pc (the
# RS232 build) on BoIP, and MAME's astrocde with the FujiNet cart
# (-cartslot fujinet) driven by tools/astrosim/astrosim.py, all in a
# namespace of their own (unshare -rnm: no network but its own, and no
# gamepads: a pad on the desk would play a seat). FujiBlast's
# tools/astrorig/rig.sh, for barraged (tools/intvrig/rig.sh's server side).
#
#   tools/astrorig/rig.sh SCENARIO OUTDIR
#
# Uses build/astrocade-rig/barrage.bin (`make astrocade-rig`: its default
# server is the rig's, 127.0.0.1:9913; BUILD=1 builds it first; ROM= another
# image, its listing in LST, default the image's directory's
# obj/barrage.lst). Builds barraged into RIG and starts it on 9913 with
# DEVSET (barraged -devset; default tanks=3,order=robin,timer=20: the dev
# rooms' bots fill the room when the host starts). A copy of
# fujinet-pc-rs232's dist (FNPC; never the live one) with the lobby's
# appkeys seeded: name NAME (AST), server tcp://127.0.0.1:9913/?table=TABLE
# (default dev1; ROOM= overrides, ROOM= empty: none). EXTRA: a command run
# in the namespace first (e.g. tools/rigs/dropproxy.py). ASTROSIM_* pass
# through. MAME runs throttled: the cart's transactions take real time, as
# on a console. Never the production server or the lobby.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SCEN=$(realpath -m "$1")
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-astrocade}")
FNPC=${FNPC:-$HOME/Workspace/fujinet-pc-rs232/build/dist}
NAME=${NAME-AST}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9913/?table=$TABLE}
DEVSET=${DEVSET-tanks=3,order=robin,timer=20}
BOIP=9995
ROM=$(realpath -m "${ROM:-$HERE/build/astrocade-rig/barrage.bin}")
LST=$(realpath -m "${LST:-$(dirname "$ROM")/obj/barrage.lst}")
mkdir -p "$RIG" "$OUT"

if [ "${BUILD:-0}" = 1 ]; then
    DEFSRV=127.0.0.1:9913 OUT="$HERE/build/astrocade-rig/obj" R2R="$HERE/build/astrocade-rig" \
        bash "$HERE/src/astrocade/build.sh" > "$RIG/build.log" 2>&1 || { tail -20 "$RIG/build.log"; exit 1; }
fi
( cd "$HERE/server" && go build -o "$RIG/barraged" ./cmd/barraged )

# FujiNet: the binary and its data, a fresh SD with the appkeys, the config
# without the cloud sections, CONFIG off, BoIP on our port
rm -rf "${RIG:?}/fn"; mkdir -p "$RIG/fn/SD/FujiNet"
cp "$FNPC/fujinet" "$RIG/fn/"
cp -a "$FNPC/data" "$RIG/fn/"
python3 - "$FNPC/fnconfig.ini" "$RIG/fn/fnconfig.ini" "$BOIP" <<'PY'
import re, sys
out, sec = [], ""
for line in open(sys.argv[1]).read().replace("\r", "").splitlines():
    m = re.match(r"\[(.*)\]", line)
    if m:
        sec = m.group(1)
    if sec in ("GoogleDrive", "S3", "OneDrive") or re.match(r"(Host|Mount)\d", sec):
        continue
    if sec == "General" and line.startswith("configenabled="):
        line = "configenabled=0"
    if sec == "BOIP" and line.startswith("host="):
        line = "host=127.0.0.1"
    if sec == "BOIP" and line.startswith("port="):
        line = "port=" + sys.argv[3]
    if sec == "BOIP" and line.startswith("enabled="):
        line = "enabled=1"
    out.append(line)
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/fn/SD/FujiNet/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/fn/SD/FujiNet/00010113.key"

export RIG OUT TABLE DEVSET EXTRA BOIP HERE SCEN ROM LST
exec unshare -rnm bash -c '
    ip link set lo up
    # no gamepads: a pad on the desk would play (SDL opens /dev/input and hidraw)
    mount -t tmpfs tmpfs /dev/input 2>/dev/null || true
    for h in /dev/hidraw*; do [ -e "$h" ] && mount --bind /dev/null "$h" 2>/dev/null || true; done
    "$RIG/barraged" -stream :9913 -seed 7 -devset "$DEVSET" > "$RIG/server.log" 2>&1 &
    srv=$!
    ( cd "$RIG/fn" && exec ./fujinet -u http://127.0.0.1:8001 > "$RIG/fujinet.log" 2>&1 ) &
    fn=$!
    bots=
    trap "kill -9 $srv $fn \$bots 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9913 " && ss -ltn | grep -q ":$BOIP " && break; sleep 0.25; done
    if [ -n "${EXTRA:-}" ]; then bash -c "$EXTRA" > "$RIG/extra.log" 2>&1 & bots="$bots $!"; fi
    ASTROSIM_FUJINET=127.0.0.1:$BOIP ASTROSIM_THROTTLE=1 ASTROSIM_LST="$LST" python3 "$HERE/tools/astrosim/astrosim.py" "$SCEN" "$OUT" "$ROM"
'
