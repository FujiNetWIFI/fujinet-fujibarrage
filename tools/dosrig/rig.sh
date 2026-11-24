#!/usr/bin/env bash
# rig.sh -- the MS-DOS client live: a private barraged, a fujinet-pc (the
# RS232 build) on BoIP, and MAME's IBM PC with FUJINET.SYS on COM1, its
# null modem a socket to that FujiNet (tools/dossim/dossim.py), all in a
# network namespace of their own (unshare -rn). FujiBlast's
# tools/dosrig/rig.sh, for barraged as tools/adamrig/rig.sh.
#
#   tools/dosrig/rig.sh SCENARIO OUTDIR
#
# Uses build/msdos's programs as built (make msdos) and builds the server
# into RIG. barraged on 9913 with DEVSET (barraged -devset; default
# tanks=3,order=robin,timer=20: its bots fill the seats), and a copy of
# fujinet-pc-rs232's dist (FNPC; never the live one) with the lobby's
# appkeys seeded: name NAME (DOS), server tcp://127.0.0.1:9913/?table=TABLE
# (default dev1; ROOM= overrides, ROOM= empty: none, so the menu: build
# it with make msdos SERVER=127.0.0.1:9913 for its room list). VGA=1: the
# VGA build on MAME's -isa1 vga. PROG names the program dossim watches
# (default the game; BMENU / BMENUVGA for the menu: its map). BAUD
# (default 115200) is FUJINET.SYS's and the null modem's, PORT its
# FUJI_PORT. The scenario is dossim.py's. EXTRA: a command run in the
# namespace first (e.g. tools/rigs/dropproxy.py). Never the production
# server or the lobby.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SCEN=$(realpath "$1")
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-msdos}")
FNPC=${FNPC:-$HOME/Workspace/fujinet-pc-rs232/build/dist}
BOOT=${BOOT:-$HOME/Vintage/IBM 5160 PCXT/fn-dos21.img}
NAME=${NAME-DOS}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9913/?table=$TABLE}
DEVSET=${DEVSET-tanks=3,order=robin,timer=20}
BAUD=${BAUD:-115200}
PORT=${PORT:-1}
BOIP=${BOIP:-1989}
B=${BDIR:-$HERE/build/msdos}
if [ "${VGA:-0}" = 1 ]; then
    GAME=BARRVGA; MAMEARGS="-isa1 vga"
else
    GAME=BARRAGE; MAMEARGS=""
fi
MAMEARGS="$MAMEARGS ${MAMEX:-}"      # (MAMEX: more, e.g. -isa3 sblaster1_0: a game port)
PROG=${PROG:-$GAME}
prog=$(echo "$PROG" | tr A-Z a-z)
mkdir -p "$RIG" "$OUT"
( cd "$HERE/server" && go build -o "$RIG/barraged" ./cmd/barraged )

# FujiNet: the binary and its data, a fresh SD with the appkeys, the config
# without the cloud sections, BoIP on our port
rm -rf "$RIG/fn"; mkdir -p "$RIG/fn/SD/FujiNet"
cp "$FNPC/fujinet" "$RIG/fn/"
cp -a "$FNPC/data" "$RIG/fn/"
python3 - "$FNPC/fnconfig.ini" "$RIG/fn/fnconfig.ini" "$BOIP" "$BAUD" <<'PY'
import re, sys
out, sec = [], ""
for line in open(sys.argv[1]).read().splitlines():
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
    if sec == "Serial" and line.startswith("baud="):
        line = "baud=" + sys.argv[4]
    out.append(line)
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/fn/SD/FujiNet/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/fn/SD/FujiNet/00010113.key"

# the boot disk: FUJINET.SYS on COM1 (MAME's isa2:com:serport0) at BAUD
cp "$BOOT" "$RIG/boot.img"
printf 'DEVICE=FUJINET.SYS FUJI_PORT=%s FUJI_BPS=%s\r\n' "$PORT" "$BAUD" > "$RIG/config.sys"
mcopy -o -i "$RIG/boot.img" "$RIG/config.sys" ::CONFIG.SYS

export RIG OUT DEVSET EXTRA BAUD BOIP HERE SCEN B GAME PROG prog MAMEARGS
exec unshare -rn bash -c '
    ip link set lo up
    BARRAGE_STREAM_DEBUG=${DEBUG:-} "$RIG/barraged" -stream :9913 -seed 7 -devset "$DEVSET" > "$OUT/server.log" 2>&1 &
    srv=$!
    ( cd "$RIG/fn" && exec ./fujinet > "$OUT/fujinet.log" 2>&1 ) &
    fn=$!
    others=
    trap "kill $srv $fn \$others 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9913 " && ss -ltn | grep -q ":$BOIP " && break; sleep 0.25; done
    if [ -n "${EXTRA:-}" ]; then bash -c "$EXTRA" > "$OUT/extra.log" 2>&1 & others="$!"; fi
    DOSSIM_BOOT="$RIG/boot.img" DOSSIM_FUJINET=127.0.0.1:$BOIP DOSSIM_BAUD=$BAUD DOSSIM_PROG=$GAME \
    DOSSIM_MAMEARGS="$MAMEARGS" DOSSIM_SECONDS=${SECONDS_MAX:-600} \
    DOSSIM_FILES="$B/barrage.exe $B/bmenu.exe $B/barrvga.exe $B/bmenuvga.exe $B/bsel.com" \
        python3 "$HERE/tools/dossim/dossim.py" "$B/$(echo $GAME | tr A-Z a-z).exe" \
        "$B/$prog.map" "$SCEN" "$OUT"
'
