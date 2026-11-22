#!/usr/bin/env bash
# rig.sh -- the Apple II client live: a private server and a2rig (AppleWin's
# core with an in-process FujiNet on SmartPort over SLIP), all in a network
# namespace of their own (unshare -rn). (FujiBlast's tools/a2rig/rig.sh;
# the Apple II's counterpart of tools/atarisim/rig.sh.)
#
#   tools/a2rig/rig.sh SCRIPT OUTDIR
#
# Uses r2r/apple2/barrage.po as built (BUILD=1: `defoogi make apple2`
# first) and builds the server into RIG. Starts barraged on 9913 (its bots
# fill the room when the host starts: the Apple is the host) and a2rig with
# the disk in FujiNet's slot 1 from its SD and the lobby's appkeys seeded:
# name NAME (A2), server tcp://127.0.0.1:9913/?table=TABLE (default dev1;
# ROOM= overrides, ROOM= empty: none, so the menu). DEVSET: the dev rooms'
# settings (barraged -devset; default tanks=3,order=robin,timer=20: the
# Apple shoots first). FujiNet is fujinet-go-apple2-desktop's
# libfujinet.so (A2D).
#
# The script is a2rig's language (a2rig.c); @symbol[+n] stands for that
# address in build/barrage-apple2.map, in hex. FAST=1 runs frames flat
# out. NOMOCK=1: no Mockingboard. EXTRA: a command run in the namespace (in the background) before
# a2rig, e.g. a proxy in front of the server. The copied FujiNet config
# loses its cloud sections (they hold tokens). Kills what it started when
# a2rig ends. Never the production server or the lobby.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SCRIPT=$(realpath "$1")
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-apple2}")
A2D=${A2D:-$HOME/Workspace/fujinet-go-apple2-desktop}
FNOUT=$A2D/tools/fujinet/work/out
NAME=${NAME-A2}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9913/?table=$TABLE}
A2RIG=$HERE/tools/a2rig/a2rig
mkdir -p "$RIG" "$OUT"

if [ "${BUILD:-0}" = 1 ]; then
    ( cd "$HERE" && defoogi make apple2 > "$RIG/build.log" 2>&1 ) || { tail -20 "$RIG/build.log"; exit 1; }
fi
make -s -C "$HERE/tools/a2rig" a2rig
( cd "$HERE/server" && go build -o "$RIG/barraged" ./cmd/barraged )

# the script, its symbols resolved
python3 - "$HERE/build/barrage-apple2.map" "$SCRIPT" "$OUT/script.txt" <<'PY'
import re, sys
t = open(sys.argv[1]).read().split("Exports list by name:")[1].split("Exports list by value:")[0]
s = {n: int(a, 16) for n, a in re.findall(r"(\S+)\s+([0-9A-F]{6})\s+\S+", t)}
def addr(m):
    n = m.group(1)
    return "0x%x" % ((s["_" + n] if "_" + n in s else s[n]) + int(m.group(2) or "0", 0))
src = open(sys.argv[2]).read()
open(sys.argv[3], "w").write(re.sub(r"@(\w+)(?:\+(\w+))?", lambda m: addr(m), src))
PY

# FujiNet's root: the desktop's runtime tree, booting our disk
rm -rf "${RIG:?}/fn"; mkdir -p "$RIG/fn/SD/FujiNet"
cp -a "$FNOUT/data" "$RIG/fn/"
python3 - "$FNOUT/fnconfig.ini" "$RIG/fn/fnconfig.ini" <<'PY'
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
    if sec == "BOIP" and line.startswith("host="):
        line = "host=127.0.0.1"
    if sec == "BOIP" and line.startswith("port="):
        line = "port=1985"
    out.append(line)
out += ["", "[Mount1]", "hostslot=1", "path=/barrage.po", "mode=r"]
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
cp "$HERE/r2r/apple2/barrage.po" "$RIG/fn/SD/barrage.po"
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/fn/SD/FujiNet/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/fn/SD/FujiNet/00010113.key"

DEVSET=${DEVSET-tanks=3,order=robin,timer=20}
export RIG OUT A2RIG FNOUT EXTRA FAST DEVSET NOMOCK
exec unshare -rn bash -c '
    ip link set lo up
    BARRAGE_STREAM_DEBUG=${DEBUG:-} "$RIG/barraged" -stream :9913 -seed 7 -devset "$DEVSET" > "$RIG/server.log" 2>&1 &
    srv=$!
    bots=
    trap "kill $srv \$bots 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9913 " && break; sleep 0.25; done
    if [ -n "${EXTRA:-}" ]; then bash -c "$EXTRA" > "$RIG/extra.log" 2>&1 & bots="$bots $!"; fi
    "$A2RIG" ${FAST:+-fast} -fn "$FNOUT/libfujinet.so" "$RIG/fn" "$OUT/script.txt" "$OUT" \
        > "$OUT/a2rig.log" 2>&1 || true
    echo "a2rig exit $?" >> "$OUT/a2rig.log"
    grep -v "^$" "$OUT/a2rig.log" | tail -40
'
