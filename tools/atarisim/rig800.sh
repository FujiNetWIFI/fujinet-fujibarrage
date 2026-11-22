#!/usr/bin/env bash
# rig800.sh -- the Atari client live on the standalone atari800 (its own
# real-time pacing and interrupt timing, closer to FujiNet Go's core than
# asim's libatari800), headless, in a network namespace with barraged and a
# copy of fujinet-pc-atari, as rig.sh.
#
#   tools/atarisim/rig800.sh SECONDS OUTDIR [STICKS]
#
# Runs SECONDS of wall time. STICKS: a file of "SECOND DIR BTN" lines; at
# each, the game's input hook (input.c: inp_hook, inp_hook_dir, inp_hook_btn)
# is set through atari800's monitor (SIGINT, commands on stdin); DIR 4 is
# left, 8 right. Every 5 seconds the monitor's registers, instruction
# history, jumps, POKEY, PIA and ns_on.. go to OUTDIR/regs.log (a JAM shows
# as atari800's crash menu: the PC stops moving). A8= another atari800
# binary (always -nojoystick: SDL reads this machine's controllers, headless or
# not, and a controller's Y button is atari800's warm start: three runs
# reset mid-game that way); A8ARGS more of its options (-bpc E477 -bbrk: stop in the monitor
# there, and the next sample shows how it got there). MON: more monitor
# commands for each sample, ";" between them.
# DEVSET as rig.sh (default here: 10 tanks, robin, 30 s). NTSC, or PAL=1
# (atari800 itself defaults to PAL: the -ntsc is needed).
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SECS=$1
OUT=$(realpath -m "$2")
STICKS=${3:+$(realpath "$3")}
RIG=$(realpath -m "${RIG:-$HERE/build/rig800}")
NAME=${NAME-ATR}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9913/?table=$TABLE}
FNPC_DIST=${FNPC_DIST:-$HOME/Workspace/fujinet-pc-atari/build/dist}
DEVSET=${DEVSET-tanks=10,order=robin,timer=30}
mkdir -p "$RIG" "$OUT"
( cd "$HERE/server" && go build -o "$RIG/barraged" ./cmd/barraged )

sym() {
    python3 - "$HERE/build/barrage-atari.map" "$1" <<'PY'
import re, sys
t = open(sys.argv[1]).read().split("Exports list by name:")[1].split("Exports list by value:")[0]
s = {n: a for n, a in re.findall(r"(\S+)\s+([0-9A-F]{6})\s+\S+", t)}
print(s["_" + sys.argv[2]][2:])
PY
}
HOOK=$(sym inp_hook); HDIR=$(sym inp_hook_dir); HBTN=$(sym inp_hook_btn)
NSON=$(sym ns_on)

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
    if sec == "Mount1" and line.startswith("hostslot="):
        line = "hostslot=1"
    if sec == "Mount1" and line.startswith("path="):
        line = "path=/barrage.atr"
    out.append(line)
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
cp "$HERE/r2r/atari/barrage.atr" "$RIG/fn/SD/barrage.atr"
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/fn/SD/FujiNet/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/fn/SD/FujiNet/00010113.key"
: > "$RIG/a8.cfg"

export RIG OUT SECS STICKS DEVSET HOOK HDIR HBTN NSON A8 MON
exec unshare -rn bash -c '
    ip link set lo up
    "$RIG/barraged" -stream :9913 -seed 7 -devset "$DEVSET" > "$RIG/server.log" 2>&1 &
    srv=$!
    ( cd "$RIG/fn" && exec setsid ./run-fujinet -u http://127.0.0.1:8013 -c fnconfig.ini -s SD ) \
        > "$RIG/fn.log" 2>&1 &
    fn=$!
    for i in $(seq 40); do ss -ltn | grep -q ":9913 " && break; sleep 0.25; done
    sleep 2
    rm -f "$RIG/mon.fifo"; mkfifo "$RIG/mon.fifo"
    exec 3<>"$RIG/mon.fifo"
    SDL_VIDEODRIVER=dummy stdbuf -o0 "${A8:-atari800}" -config "$RIG/a8.cfg" -atari $([ -n "$PAL" ] && echo -pal || echo -ntsc) $A8ARGS -nojoystick -nosound -netsio 9997 \
        < "$RIG/mon.fifo" > "$OUT/a8.log" 2>&1 &
    a8=$!
    trap "kill -- -$fn 2>/dev/null; kill $srv $a8 2>/dev/null; true" EXIT
    mon() { kill -INT $a8; sleep 0.3; for c in "$@"; do echo "$c" | tr ";" "\n" >&3; done; echo CONT >&3; }
    : > "$OUT/regs.log"
    for s in $(seq "$SECS"); do
        sleep 1
        if [ -n "$STICKS" ]; then
            while read -r at dir btn; do
                [ "$at" = "$s" ] && mon "C $HOOK 1" "C $HDIR $dir" "C $HBTN $btn"
            done < "$STICKS"
        fi
        if [ $((s % 5)) = 0 ]; then
            echo "== $s s" >> "$OUT/regs.log"
            n0=$(wc -c < "$OUT/a8.log")
            mon "H" "JUMPS" "POKEY" "PIA" "M $NSON 4" ${MON:+"$MON"} ; sleep 0.5
            tail -c +$((n0 + 1)) "$OUT/a8.log" | grep -av "^>" >> "$OUT/regs.log" || true
        fi
    done
'
