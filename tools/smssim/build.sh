#!/bin/sh
# build.sh -- tools/smssim/smsrun: smsrun.c and FujiNet Go SMS's core (GO,
# default ~/Workspace/fujinet-go-sms-desktop), the core compiled here from
# its sources (into build/smsrun-core: GO's own build may be older than its
# headers, and the structures smsrun reads must match).
set -e
GO=${GO:-$HOME/Workspace/fujinet-go-sms-desktop}
here=$(cd "$(dirname "$0")" && pwd)
obj=$here/../../build/smsrun-core
mkdir -p "$obj"
C="$GO/core"
INC="-I$C/include -I$C/sms -I$C/sms/fuji-generated -I$C/debugger"
DEF="-D_GNU_SOURCE -D_DEFAULT_SOURCE -DNDEBUG"
objs=""
for s in sms/machine.c sms/vdp.c sms/psg.c sms/fujinet_cart.c sms/fujitcp_host.c sms/host.c \
         sms/fuji-generated/fujimail.c sms/fuji-generated/fujibus.c sms/fuji-generated/smsmap.c \
         sms/fuji-generated/smsmap_db.c sms/fuji-generated/fuji_load.c src/pad_assign.c \
         debugger/debugger.c debugger/expr.c debugger/fujitrace.c debugger/prompt.c debugger/z80dasm.c \
         debugger/symbols.c debugger/symbols_builtin.c debugger/vdpview.c debugger/cartview.c \
         debugger/memtools.c debugger/vidtab.c; do
    [ -f "$C/$s" ] || continue
    o=$obj/$(echo "$s" | tr / _).o
    [ "$o" -nt "$C/$s" ] || gcc -O2 -std=gnu11 $DEF $INC -c -o "$o" "$C/$s"
    objs="$objs $o"
done
for s in sms/fm_ym2413.cpp sms/ymfm/ymfm_opl.cpp sms/ymfm/ymfm_adpcm.cpp sms/ymfm/ymfm_pcm.cpp; do
    o=$obj/$(echo "$s" | tr / _).o
    [ "$o" -nt "$C/$s" ] || g++ -O2 -std=c++17 $DEF $INC -c -o "$o" "$C/$s"
    objs="$objs $o"
done
gcc -O2 -g -std=gnu11 $DEF $INC -o "$here/smsrun" "$here/smsrun.c" $objs -lstdc++ -lm -lpthread
