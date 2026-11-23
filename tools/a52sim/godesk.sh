#!/usr/bin/env bash
# godesk.sh -- the FujiNet Go 5200 desktop (atari800 with Altirra's 5200 OS)
# runs a .a52 and takes a picture after FROM, FROM+1, ... FROM+N-1 frames
# (one run each: the desktop saves one picture a run). Nothing is shown and
# nothing reaches the network or the user's settings: a network namespace,
# a private Xvnc (:93), a scratch HOME.
#
#   tools/a52sim/godesk.sh CART.a52 OUTDIR [N] [FROM]
#
# OUTDIR/shot-<frames>.png; tools/a52sim/edges.py says whether the colour
# edges hold still between them. GODESK= the desktop's binary (default the
# GNOME build in ~/Workspace/fujinet-go-atari5200-desktop/build).
set -euo pipefail
CART=$(realpath "$1"); OUT=$(realpath -m "$2"); N=${3:-10}; FROM=${4:-900}
BIN=${GODESK:-$HOME/Workspace/fujinet-go-atari5200-desktop/build/frontends/gnome/fujinet-go-atari5200-gnome}
mkdir -p "$OUT"
for k in $(seq 0 $((N - 1))); do
    f=$((FROM + k))
    W=$(mktemp -d "$OUT/run.XXXX")
    mkdir -p "$W/home"
    timeout 120 unshare -rn bash -c "
        ip link set lo up
        Xvnc :93 -SecurityTypes None -geometry 1200x1000 -depth 24 -rfbunixpath $W/v.sock -nolisten tcp > $W/xvnc.log 2>&1 &
        xv=\$!
        sleep 1.5
        unset WAYLAND_DISPLAY
        HOME=$W/home XDG_CONFIG_HOME=$W/home/.config XDG_DATA_HOME=$W/home/.local/share XDG_CACHE_HOME=$W/home/.cache \
        GDK_BACKEND=x11 DISPLAY=:93 GDK_DEBUG=no-portals GSK_RENDERER=cairo \
        A52_SCREENSHOT=$OUT/shot-$f.png A52_SCREENSHOT_FRAMES=$f A52_AUTOQUIT_FRAMES=$((f + 20)) \
        $BIN $CART > $W/app.log 2>&1 || true
        kill \$xv 2>/dev/null || true
    " || true
    rm -rf "$W"
done
ls "$OUT"/shot-*.png | wc -l
