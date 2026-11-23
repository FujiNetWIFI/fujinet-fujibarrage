#!/usr/bin/env bash
# 86box.sh -- FujiBarrage's speed on a real PC: 86Box (cycle-exact: a
# 4.77 MHz 8088 with CGA's wait states, an 8 MHz 8086 with VGA ...),
# headless, offline: a fixture's shot (tools/dossim/fixture.py) played as
# the test scene does, then the jiffies it took. FujiBlast's
# tools/dosrig/86box.sh, without the link (MAME's 8088 runs 2-3x too fast
# to time anything).
#
#   tools/dosrig/86box.sh FIXDIR SECONDS OUTDIR
#
# EXE (default build/msdos/barrage.exe) must be a `make msdos STATS=1
# TEST=1 TEST_SCENE=1 TEST_ARG=2` build: it plays the fixture's shot once
# (FIX.BIN, SEGnn.BIN from FIXDIR, put on the floppy) and writes A:\LAG.TXT
# (PLAYED: the jiffies the shot took; less the script's frames, it is the
# lag) and A:\PROF.BIN (tools/dosrig/prof86.py with MAP). VM is the 86Box
# machine copied (default ~/Vintage/IBM 5150 PC); VMSET, settings over the
# VM's in the rig's copy only: "Section|key = value" lines separated by ';'
# (an empty value drops the key). The VGA build on an 8 MHz 8086 with VGA
# (the PS/2 Model 25/30's stand-in):
#   PROG=BARRVGA EXE=build/msdos/barrvga.exe VMSET='Machine|machine = pc2086;
#     Machine|cpu_family = 8086;Machine|cpu_speed = 8000000;Machine|mem_size = 640;
#     Video|gfxcard = internal;Input devices|keyboard_type =;
#     Floppy and CD-ROM drives|fdd_01_type = 525_2dd'
# An XT with an 8-bit VGA: VMSET='Machine|machine = ibmxt;Video|gfxcard = oti037c'
#
# LIVE=1: live instead (FIXDIR unused: give -): a private barraged
# (DEVSET, as tools/dosrig/rig.sh's) and a fujinet-pc on BoIP in a network
# namespace, FUJINET.SYS on the VM's COM2 at BAUD (9600; 86Box's port runs
# a byte a bit time, faster than a real one), the game a `make msdos
# STATS=1` build: it plays by itself and writes A:\STATS.TXT every 10 s
# (jiffies, late passes, the link's bad, lost, resent replies, reads lost,
# bytes the UART lost, bytes received).
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
FIX=$(realpath "$1")
SECS=$2
OUT=$(realpath -m "$3")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-86box}")
VM=${VM:-$HOME/Vintage/IBM 5150 PC}
BOOT=${BOOT:-$HOME/Vintage/IBM 5160 PCXT/fn-dos21.img}
PROG=${PROG:-BARRAGE}
EXE=$(realpath "${EXE:-$HERE/build/msdos/$(echo "$PROG" | tr A-Z a-z).exe}")
MAP=${MAP:-${EXE%.exe}.map}
VMSET=${VMSET:-}
mkdir -p "$RIG" "$OUT"

# the floppy: DOS, the game run by AUTOEXEC.BAT; the fixture (no
# FUJINET.SYS), or FUJINET.SYS on COM2 (LIVE)
LIVE=${LIVE:-0}
BAUD=${BAUD:-9600}
BOIP=${BOIP:-1991}
cp "$BOOT" "$RIG/boot.img"
printf '@ECHO OFF\r\n%s\r\n' "$PROG" > "$RIG/autoexec.bat"
mcopy -o -i "$RIG/boot.img" "$RIG/autoexec.bat" ::AUTOEXEC.BAT
mcopy -o -i "$RIG/boot.img" "$EXE" "::$PROG.EXE"
mdel -i "$RIG/boot.img" ::LAG.TXT ::PROF.BIN ::STATS.TXT 2>/dev/null || true
if [ "$LIVE" = 1 ]; then
    printf 'DEVICE=FUJINET.SYS FUJI_PORT=2 FUJI_BPS=%s\r\n' "$BAUD" > "$RIG/config.sys"
    mcopy -o -i "$RIG/boot.img" "$RIG/config.sys" ::CONFIG.SYS
    FNPC=${FNPC:-$HOME/Workspace/fujinet-pc-rs232/build/dist}
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
    printf '%s' "${NAME:-DOS}" > "$RIG/fn/SD/FujiNet/00010100.key"
    printf '%s' "tcp://127.0.0.1:9913/?table=${TABLE:-dev1}" > "$RIG/fn/SD/FujiNet/00010113.key"
    ( cd "$HERE/server" && go build -o "$RIG/barraged" ./cmd/barraged )
else
    mdel -i "$RIG/boot.img" ::CONFIG.SYS 2>/dev/null || true
    for f in "$FIX"/FIX.BIN "$FIX"/SEG*.BIN; do mcopy -o -i "$RIG/boot.img" "$f" ::; done
fi

# the machine: the VM's settings, headless, our floppy
rm -rf "$RIG/vm"; mkdir -p "$RIG/vm"
[ -d "$VM/nvr" ] && cp -a "$VM/nvr" "$RIG/vm/"
CFG=$VM/86box.cfg; [ -f "$CFG" ] || CFG=$VM/86Box.cfg
python3 - "$CFG" "$RIG/vm/86box.cfg" "$RIG/boot.img" "$VMSET" "$BOIP" <<'PY'
import re, sys
sets = {}
for item in sys.argv[4].split(";"):
    if "|" in item:
        sec_, kv = item.split("|", 1)
        k, _, v = kv.partition("=")
        sets.setdefault(sec_.strip(), {})[k.strip()] = v.strip()
out, sec = [], ""
lines = open(sys.argv[1]).read().splitlines()
for i, line in enumerate(lines):
    m = re.match(r"\[(.*)\]", line)
    if m:
        sec = m.group(1)
    if line.startswith(("fdd_01_fn", "fdd_0", "vid_renderer")) and "history" not in line and "type" not in line:
        continue
    if sec.startswith("FujiNet (BoIP)") and line.startswith("port"):
        line = "port = " + sys.argv[5]
    k = line.split("=")[0].strip()
    if not m and k in sets.get(sec, {}):
        v = sets[sec].pop(k)
        if not v:
            continue
        line = k + " = " + v
    out.append(line)
    if sec == "General" and m:
        out.append("vid_renderer = qt_software")
    if sec == "Floppy and CD-ROM drives" and m:
        out.append("fdd_01_fn = " + sys.argv[3])
    nxt = lines[i + 1] if i + 1 < len(lines) else "["
    if nxt.startswith("[") and sec in sets:              # the section's end: what it lacked
        out += [k2 + " = " + v2 for k2, v2 in sets.pop(sec).items() if v2]
for sec_, kv in sets.items():                            # sections it lacked
    out += ["", "[" + sec_ + "]"] + [k2 + " = " + v2 for k2, v2 in kv.items() if v2]
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY

if [ "$LIVE" = 1 ]; then
    export RIG OUT SECS BOIP
    DEVSET=${DEVSET-tanks=4,order=robin,timer=10} unshare -rn bash -c '
        ip link set lo up
        "$RIG/barraged" -stream :9913 -seed 7 -devset "$DEVSET" > "$OUT/server.log" 2>&1 &
        srv=$!
        ( cd "$RIG/fn" && exec ./fujinet > "$OUT/fujinet.log" 2>&1 ) &
        fn=$!
        trap "kill $srv $fn 2>/dev/null; true" EXIT
        for i in $(seq 40); do ss -ltn | grep -q ":9913 " && ss -ltn | grep -q ":$BOIP " && break; sleep 0.25; done
        QT_QPA_PLATFORM=offscreen timeout -s KILL "$SECS" 86Box -N -P "$RIG/vm" > "$RIG/86box.log" 2>&1 || true
    '
    mtype -i "$RIG/boot.img" ::STATS.TXT > "$OUT/stats.txt" 2>/dev/null || echo "(no STATS.TXT)" > "$OUT/stats.txt"
    cat "$OUT/stats.txt"
    grep -c " fires " "$OUT/server.log" || true
    exit 0
fi
QT_QPA_PLATFORM=offscreen timeout -s KILL "$SECS" 86Box -N -P "$RIG/vm" > "$RIG/86box.log" 2>&1 || true
mtype -i "$RIG/boot.img" ::LAG.TXT > "$OUT/lag.txt" 2>/dev/null || echo "(no LAG.TXT)" > "$OUT/lag.txt"
mcopy -o -i "$RIG/boot.img" ::PROF.BIN "$OUT/prof.bin" 2>/dev/null || true
cat "$OUT/lag.txt"
[ -f "$OUT/prof.bin" ] && python3 "$HERE/tools/dosrig/prof86.py" "$OUT/prof.bin" "$MAP" | head -25
