#!/bin/sh
# build.sh -- tools/nessim/nesrun against FujiNet Go NES's core: the desktop
# app's build tree ($DESKTOP, default ~/Workspace/fujinet-go-nes-desktop;
# its build/ holds libnes_session.a, libmesen_core.a and the staged MesenCE
# sources, mesen-generated/), built into this repo's build/nescore (its
# configure: cmake -S $D -B build/nescore -G Ninja -DFRONTEND=none; then
# cmake --build build/nescore --target nes_session mesen_core): the
# session's struct must be the source's it was built from.
set -e
D=${DESKTOP:-$HOME/Workspace/fujinet-go-nes-desktop}
B=${DESKTOP_BUILD:-$(cd "$(dirname "$0")/../.." && pwd)/build/nescore}
G=$B/mesen-generated
cd "$(dirname "$0")"
c++ -O2 -g -std=c++17 -I"$D/core/include" -I"$D/core/src" -I"$D/core/mesen" \
    -I"$G" -I"$G/Core" -I"$G/Utilities" -c nesrun.cpp -o nesrun.o
c++ -o nesrun nesrun.o "$B/core/libnes_session.a" "$B/core/libmesen_core.a" \
    -ldl -lpthread $(ls /usr/lib/libSDL3.so* 2>/dev/null | head -1)
rm -f nesrun.o
