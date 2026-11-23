#!/usr/bin/env bash
# mixed.sh -- a CoCo 1/2 and a CoCo 3 in one room: a private barraged, two
# fujinet-pc and two MAMEs (rig.sh's, twice over), never the production
# server. MAME's Becker port can't move from 65504 (it aborts), so the CoCo 3
# and its FujiNet are in a network namespace of their own nested in the
# CoCo 1/2's, the server reached through a Unix socket (socat).
#
#   tools/cocosim/mixed.sh SCRIPT2 SCRIPT3 OUTDIR
#
# SCRIPT2 drives the CoCo 1/2 (name CC2), SCRIPT3 the CoCo 3 (CC3); both join
# room TABLE (dev1) with DEVSET's settings (whoever joins first hosts: press
# in both). Outputs: OUTDIR/{c2,c3}/ (cocosim), OUTDIR/server.log.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
S2=$(realpath "$1"); S3=$(realpath "$2"); OUT=$(realpath -m "$3")
FNPC=${FNPC:-$HOME/Workspace/fujinet-pc-coco/build/dist}
TABLE=${TABLE:-dev1}
DEVSET=${DEVSET-tanks=4,rounds=3,order=robin,timer=10}
mkdir -p "$OUT/c2" "$OUT/c3"
( cd "$HERE/server" && go build -o "$OUT/barraged" ./cmd/barraged )
for n in 2 3; do
    rm -rf "${OUT:?}/fn$n"; mkdir -p "$OUT/fn$n/SD/FujiNet"
    cp -a "$FNPC/fujinet" "$FNPC/data" "$OUT/fn$n/"
    python3 - "$FNPC/fnconfig.ini" "$OUT/fn$n/fnconfig.ini" <<'PY'
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
    cp "$HERE/r2r/coco/barrage.dsk" "$OUT/fn$n/SD/barrage.dsk"
    name=CC2; [ $n = 3 ] && name=CC3
    printf '%s' "$name" > "$OUT/fn$n/SD/FujiNet/00010100.key"
    printf '%s' "tcp://127.0.0.1:9913/?table=$TABLE" > "$OUT/fn$n/SD/FujiNet/00010113.key"
done
rm -f "$OUT/srv.sock"

# one machine's FujiNet and MAME in the current namespace: client N MACHINE BIOS MAP SCRIPT
cat > "$OUT/client.sh" <<'SH'
set -u
n=$1; machine=$2; bios=$3; map=$4; script=$5
( cd "$OUT/fn$n" && exec ./fujinet -c fnconfig.ini -s SD -u "http://127.0.0.1:8123" > "$OUT/fujinet$n.log" 2>&1 ) &
fpid=$!
for i in $(seq 40); do ss -ltn | grep -q ":65504 " && break; sleep 0.25; done
sleep 1
curl -s -X POST "http://127.0.0.1:8123/api/v1/drives/1/mount" -H "Content-Type: application/json" \
    -d "{\"path\":\"/barrage.dsk\",\"host_slot\":0,\"mode\":\"r\"}" > "$OUT/mount$n.log" 2>&1 || true
cd "$HERE"
MACHINE=$machine COCO_BECKER=1 COCO_BIOS=$bios COCOSIM_SECONDS=${COCOSIM_SECONDS:-600} \
    python3 tools/cocosim/cocosim.py - "$map" "$script" "$OUT/c$n" > "$OUT/c$n/cocosim.log" 2>&1
kill $fpid 2>/dev/null
SH
export OUT HERE DEVSET S2 S3
exec unshare -rn bash -c '
    ip link set lo up
    "$OUT/barraged" -stream :9913 -seed 7 -devset "$DEVSET" > "$OUT/server.log" 2>&1 &
    pids="$!"
    trap "kill $pids 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9913 " && break; sleep 0.25; done
    socat UNIX-LISTEN:"$OUT/srv.sock",fork TCP:127.0.0.1:9913 > "$OUT/bridge.log" 2>&1 &
    pids="$pids $!"
    unshare -n bash -c "
        ip link set lo up
        socat TCP-LISTEN:9913,fork,reuseaddr UNIX-CONNECT:$OUT/srv.sock > $OUT/bridge3.log 2>&1 &
        trap \"kill \$! 2>/dev/null; true\" EXIT
        bash $OUT/client.sh 3 coco3 hdbk3 build/barrage-game3.map $S3
    " &
    b=$!
    bash "$OUT/client.sh" 2 coco2b hdbk12 build/barrage-game2.map "$S2"
    wait $b
    grep -h "cocosim exit\|SNAP" "$OUT/c2/cocosim.log" "$OUT/c3/cocosim.log" | tail -4
'
