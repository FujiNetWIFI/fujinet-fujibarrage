# tools/coco/build.mk -- FujiBarrage for the CoCo 1/2 and the CoCo 3, both on
# one disk (CMOC 0.1.101, lwtools, toolshed's decb). From the top: make coco.
#
#   make -f tools/coco/build.mk             r2r/coco/barrage.dsk
#   ... TEST=1                              with the offline test scenes
#   ... VIDEO_CARD=1                        M0's test card on the screen
#
# Each machine's program is a low core (LOADM'd by AUTOEXEC.BAS: the CoCo
# 1/2's at $2800, the CoCo 3's at $1F00) and the files src/coco/bank.c
# loads after it: <name>H.BIN (the high core at $8000; on the CoCo 1/2 the
# art at $E000 too) and on the CoCo 3 <name>A.BIN (its art block). CMOC has
# no section pragma: a module goes high (FB_SECT=hi) or into the art
# (FB_SECT=art) through tools/coco/lwasm-sect, and tools/coco/lwlink-co.py
# links with src/coco/barrage{,3}.link, checks the regions and splits the
# result. AUTOEXEC.BAS picks the CoCo 3's on a CoCo 3 (its IRQ vector is
# $FEF7, so $FFF8 reads $FE).

DECB ?= $(firstword $(wildcard $(HOME)/Workspace/toolshed/build/unix/decb/decb) decb)
CMOC ?= cmoc
LIB := _cache/fujinet-lib/5.1.0-coco
B := build/coco
R := r2r/coco
TOOLS := $(CURDIR)/tools/coco

CFLAGS := -O1 -funsigned-char -fomit-frame-pointer -Wno-const -Wgives-byte \
	--lwasm=$(TOOLS)/lwasm-sect -DUSING_FUJINET_LIB=1 \
	-Isrc/coco/include -Isrc -Isrc/platform-specific -Isrc/test -Igen -Isrc/coco -I$(LIB)
ifeq ($(TEST),1)
  TESTFLAGS := -DTEST_SCENES -DNO_APPKEYS $(TESTX)   # (the game only: the menu stays as it is; TESTX: more)
endif
ifeq ($(VIDEO_CARD),1)
  CFLAGS += -DVIDEO_CARD
endif
LDFLAGS := --lwlink=$(TOOLS)/lwlink-co.py --initial-s=8000
.DEFAULT_GOAL := all
HDRS := $(wildcard src/*.h src/platform-specific/*.h src/coco/*.h src/coco2/*.h src/coco3/*.h src/coco2/test/*.h src/coco3/test/*.h src/coco/include/*.h) gen/config.h

# the game (BARRAGE, BARRAGE3) and the menu (BMENU, BMENU3: -DMENU, the
# title, the name and the room list), as the Atari's and the Apple's
SHARED := src/main.c src/menus.c src/appkey.c src/net.c src/cat.c
GAME := src/play.c src/room.c src/test.c src/6809/ply.asm src/6809/trk.asm src/6809/ops.asm src/6809/pt.asm
COMMON := src/coco/bank.c src/coco/plat.c src/coco/input.c src/coco/nstrans.c src/coco/nsta.asm \
	src/coco/hibss.asm src/coco/chain.asm src/coco/fixed.asm

PROGS := game2 game3 menu2 menu3
COCO2 := src/coco2/video.c src/coco/sound.c src/coco2/snd2a.asm src/coco2/scr2.c src/coco2/scr2a.asm \
	gen/coco2/font2.asm gen/coco2/flip2.asm gen/coco2/sfx.asm
COCO3 := src/coco3/video.c src/coco/sound.c src/coco3/snd3a.asm src/coco3/scr3.c src/coco3/scr3a.asm \
	gen/coco3/font3.asm gen/coco3/pal3.asm gen/coco3/sfx.asm
FIELD2 := src/coco/field.c src/coco2/fld2.asm gen/coco2/tank2.asm
FIELD3 := src/coco/field.c src/coco3/fld3.asm gen/coco3/fld3d.asm
game2_SRC := $(SHARED) $(GAME) $(COMMON) $(COCO2) $(FIELD2) gen/coco2/tunes_game.asm
game3_SRC := $(SHARED) $(GAME) $(COMMON) $(COCO3) $(FIELD3) gen/coco3/tunes_game.asm
menu2_SRC := $(SHARED) $(COMMON) $(COCO2) src/coco/lzd.asm gen/coco2/title2.asm gen/coco2/tunes_menu.asm
menu3_SRC := $(SHARED) $(COMMON) $(COCO3) src/coco/lzd.asm gen/coco3/title3.asm gen/coco3/tunes_menu.asm
game2_FLAGS := -Isrc/coco2 $(TESTFLAGS)
game3_FLAGS := -DCOCO3 -Isrc/coco3 $(TESTFLAGS)
menu2_FLAGS := -DMENU -Isrc/coco2
menu3_FLAGS := -DMENU -DCOCO3 -Isrc/coco3
game2_LINK := coco2
game3_LINK := coco3
menu2_LINK := coco2
menu3_LINK := coco3
game2_OUT := $(R)/barrage
game3_OUT := $(R)/barrage3
menu2_OUT := $(R)/bmenu
menu3_OUT := $(R)/bmenu3

# a program: its objects (build/coco/<prog>/<source>.o), its link; its
# flags in build/coco/<prog>.flags, rewritten when they change (TEST=1 and
# back), so its objects build again
define PROG_template
$(1)_OBJ := $$(patsubst %,$(B)/$(1)/%.o,$$(basename $$($(1)_SRC)))
$(B)/$(1).flags: FORCE
	@mkdir -p $(B)
	@echo '$$(CFLAGS) $$($(1)_FLAGS)' | cmp -s - $$@ || echo '$$(CFLAGS) $$($(1)_FLAGS)' > $$@
$(B)/$(1)/%.o: %.c $(HDRS) $(B)/$(1).flags
	@mkdir -p $$(dir $$@)
	$(CMOC) -c $$(CFLAGS) $$($(1)_FLAGS) -o $$@ $$<
$(B)/$(1)/%.o: %.asm $(B)/$(1).flags
	@mkdir -p $$(dir $$@)
	$(CMOC) -c $$(CFLAGS) $$($(1)_FLAGS) -o $$@ $$<
$$($(1)_OUT).bin: $$($(1)_OBJ) src/coco/barrage.link src/coco/barrage3.link tools/coco/lwlink-co.py
	@mkdir -p $$(dir $$@)
	CO_LINK=$$($(1)_LINK) CO_MAP=build/barrage-$(1).map $(CMOC) -o $$@ $$(LDFLAGS) $$($(1)_OBJ) -L$(LIB) -lfujinet-coco-5.1.0
endef
$(foreach p,$(PROGS),$(eval $(call PROG_template,$(p))))

# where the modules go: the low core holds what bank_init needs (it loads
# the rest) and all the C's variables
$(B)/game2/src/play.o $(B)/game3/src/play.o: export FB_SECT = hi
$(B)/game2/src/room.o: export FB_SECT = hi
$(B)/game2/src/coco/field.o: export FB_SECT = art
$(B)/game3/src/coco/field.o: export FB_SECT = art
$(B)/game3/src/coco/sound.o $(B)/menu3/src/coco/sound.o: export FB_SECT = mid
$(foreach p,$(PROGS),$(B)/$(p)/src/menus.o): export FB_SECT = hi
$(foreach p,game2 menu2,$(B)/$(p)/src/coco2/scr2.o $(B)/$(p)/src/coco2/video.o): export FB_SECT = hi
$(foreach p,game3 menu3,$(B)/$(p)/src/coco3/scr3.o $(B)/$(p)/src/coco3/video.o): export FB_SECT = hi

# the art (tools/mkgfx_coco.py)
gen/coco2/font2.asm: tools/mkgfx_coco.py gfx/coco/font3x7.png
	python3 tools/mkgfx_coco.py font2 $@
gen/coco3/font3.asm: tools/mkgfx_coco.py gfx/coco/namco.bin
	python3 tools/mkgfx_coco.py font3 $@
gen/coco3/pal3.asm: tools/mkgfx_coco.py tools/cocopal.py
	python3 tools/mkgfx_coco.py pal3 $@
gen/coco3/fld3d.asm: tools/mkgfx_coco.py tools/cocopal.py
	python3 tools/mkgfx_coco.py fld3 $@
gen/coco2/tank2.asm: tools/mkgfx_coco.py
	python3 tools/mkgfx_coco.py tank2 $@
gen/coco2/flip2.asm: tools/mkgfx_coco.py
	python3 tools/mkgfx_coco.py flip2 $@

# the sound (tools/mksfx_coco.py): the effects, the tunes (the menu's title,
# the game's others), each where its program has room
SNDDEP := tools/mksfx_coco.py tools/mus.py tools/mkmus_intv.py src/intv/sfx.asm $(wildcard music/*.txt)
gen/coco2/sfx.asm gen/coco3/sfx.asm: $(SNDDEP)
	python3 tools/mksfx_coco.py $(word 2,$(subst /, ,$@)) sfx $@
gen/coco2/tunes_menu.asm gen/coco3/tunes_menu.asm: $(SNDDEP)
	python3 tools/mksfx_coco.py $(word 2,$(subst /, ,$@)) tunes $@ -s hicode title=music/title.txt
gen/coco2/tunes_game.asm: $(SNDDEP)
	python3 tools/mksfx_coco.py coco2 tunes $@ -s hicode win=music/win.txt over=music/over.txt shop=music/shop.txt
gen/coco3/tunes_game.asm: $(SNDDEP)
	python3 tools/mksfx_coco.py coco3 tunes $@ -s art win=music/win.txt over=music/over.txt shop=music/shop.txt
gen/coco2/title2.asm: tools/mktitle_coco.py gfx/coco/font3x7.png
	python3 tools/mktitle_coco.py title2 $@
gen/coco3/title3.asm: tools/mktitle_coco.py tools/cocopal.py gfx/coco/namco.bin
	python3 tools/mktitle_coco.py title3 $@

BINS := $(R)/barrage.bin $(R)/barrageh.bin $(R)/barrage3.bin $(R)/barrage3h.bin $(R)/barrage3a.bin \
	$(R)/bmenu.bin $(R)/bmenuh.bin $(R)/bmenu3.bin $(R)/bmenu3h.bin $(R)/bmenu3a.bin
$(R)/barrageh.bin: $(R)/barrage.bin
$(R)/barrage3h.bin $(R)/barrage3a.bin: $(R)/barrage3.bin
$(R)/bmenuh.bin: $(R)/bmenu.bin
$(R)/bmenu3h.bin $(R)/bmenu3a.bin: $(R)/bmenu3.bin

all: $(R)/barrage.dsk

$(R)/barrage.dsk: $(BINS)
	rm -f $@
	$(DECB) dskini $@
	printf '10 PCLEAR1:IF PEEK(&HFFF8)=&HFE THEN LOADM"BARRAGE3":EXEC\n20 LOADM"BARRAGE":EXEC\n' > $(B)/autoexec.bas
	$(DECB) copy -0 -a -l -r $(B)/autoexec.bas $@,AUTOEXEC.BAS
	$(DECB) copy -b -2 -r $(R)/barrage.bin $@,BARRAGE.BIN
	$(DECB) copy -b -2 -r $(R)/barrageh.bin $@,BARRAGEH.BIN
	$(DECB) copy -b -2 -r $(R)/barrage3.bin $@,BARRAGE3.BIN
	$(DECB) copy -b -2 -r $(R)/barrage3h.bin $@,BARRAG3H.BIN
	$(DECB) copy -b -2 -r $(R)/barrage3a.bin $@,BARRAG3A.BIN
	$(DECB) copy -b -2 -r $(R)/bmenu.bin $@,BMENU.BIN
	$(DECB) copy -b -2 -r $(R)/bmenuh.bin $@,BMENUH.BIN
	$(DECB) copy -b -2 -r $(R)/bmenu3.bin $@,BMENU3.BIN
	$(DECB) copy -b -2 -r $(R)/bmenu3h.bin $@,BMENU3H.BIN
	$(DECB) copy -b -2 -r $(R)/bmenu3a.bin $@,BMENU3A.BIN
	@echo "$@: $$(($$(stat -c %s $(BINS) | paste -sd+))) bytes of programs (a disk holds 156K)"

.PHONY: all

FORCE:
.PHONY: FORCE
