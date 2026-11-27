# build.mk -- FujiBarrage for the Atari 7800 (cc65, in the defoogi container:
# the top-level Makefile's `make atari7800` runs it, after the host has built
# fujinet-lib for the 7800 and written the catalog). The image is a 128K
# SuperGame with the cart's RAM (src/atari7800/barrage-a78.cfg), checked and
# given its header by tools/atari7800/mkrom.py. One program (ONE_PROGRAM:
# the menu and the game, as the NES's): the core in the fixed bank and REPL,
# the overlays in the window's own half -- the field (src/play.c), the rooms
# (src/room.c, src/cat.c and the catalog), the menus (src/menus.c,
# src/appkey.c's saves), the boot (src/appkey.c's load, src/test.c) --
# reached through the wrappers tools/atari7800/far.txt lists. The field's
# pixels, the shot player and the sound are the Atari's (src/atari,
# src/6502).
#
#   ... TEST=1              with the offline test scenes (tools/a78sim)
#   ... TEST_SCENE=n TEST_ARG=a   a scene built in

B      := build/a78
R      := r2r/atari7800
CFG    := src/atari7800/barrage-a78.cfg
FNLIB  := build/fnlib-a78
FAR    := tools/atari7800/far.txt

CFLAGS := -t atari7800 -g -Os -Cl -O --cpu 6502 -DONE_PROGRAM -DCAT_ROM -DUSING_FUJINET_LIB=1 \
	-I src -I src/platform-specific -I src/atari7800 -I gen -I gen/atari7800 \
	-I src/atari7800/test -I src/test -I $(FNLIB)/include
AFLAGS := -t atari7800 -g -I src/atari7800 -I src/atari -I src/6502 -I gen/atari7800 $(XAFLAGS)
ifeq ($(TEST),1)
  CFLAGS += -DTEST_SCENES -DNO_APPKEYS
endif
ifneq ($(TEST_SCENE),)
  CFLAGS += -DTEST_SCENES -DNO_APPKEYS -DTEST_SCENE=$(TEST_SCENE) -DTEST_ARG=$(or $(TEST_ARG),0)
endif
CFLAGS += $(XFLAGS)

HDR := $(wildcard src/*.h src/platform-specific/*.h src/atari7800/*.h src/atari7800/*.inc src/atari/*.inc \
	src/6502/*.inc src/atari7800/test/*.h src/test/*.h) gen/config.h gen/font35.h gen/atari7800/catalog.h

FIELD_DEFS := $(shell python3 tools/nes/mkfar.py $(FAR) --defs 0)
ROOM_DEFS  := $(shell python3 tools/nes/mkfar.py $(FAR) --defs 1)
MENU_DEFS  := $(shell python3 tools/nes/mkfar.py $(FAR) --defs 2)
BOOT_DEFS  := $(shell python3 tools/nes/mkfar.py $(FAR) --defs 3)
FOPEN_DEFS := $(shell python3 tools/nes/mkfar.py $(FAR) --defs 5)

CORE_C  := src/main.c src/net.c src/atari/field.c src/atari/sound.c src/atari7800/plat.c \
	src/atari7800/input.c src/atari7800/nstrans.c src/atari7800/scr.c src/atari7800/fieldm.c
CORE_S  := src/atari7800/crt0.s src/atari7800/bank.s src/atari7800/maria.s src/atari7800/hud.s src/atari/fld.s \
	src/atari/bln.s src/atari/pt.s src/atari/snd.s src/6502/ops.s src/6502/ply.s src/6502/trk.s \
	gen/atari7800/far.s gen/atari7800/font.s
FIELD_C := src/play.c
ROOM_C  := src/room.c src/cat.c
ROOM_S  := gen/atari7800/catalog.s
MENU_C  := src/menus.c src/appkey.c src/atari7800/title.c
MENU_S  := gen/atari7800/title_hills.s
BOOT_C  := src/test.c
FOPEN_C := src/atari7800/fopen.c
SND_S   := gen/atari7800/title.s gen/atari7800/tunes.s

G := $(B)/game
CORE_O  := $(patsubst %,$(G)/%.o,$(CORE_C) $(CORE_S))
FIELD_O := $(patsubst %,$(G)/%.o,$(FIELD_C))
ROOM_O  := $(patsubst %,$(G)/%.o,$(ROOM_C) $(ROOM_S))
MENU_O  := $(patsubst %,$(G)/%.o,$(MENU_C) $(MENU_S))
BOOT_O  := $(patsubst %,$(G)/%.o,$(BOOT_C))
SND_O   := $(patsubst %,$(G)/%.o,$(SND_S))
FOPEN_O := $(patsubst %,$(G)/%.o,$(FOPEN_C))
OBJ     := $(CORE_O) $(FIELD_O) $(ROOM_O) $(MENU_O) $(BOOT_O) $(SND_O) $(FOPEN_O)
BIN     := $(B)/barrage.bin
IMG     := $(R)/barrage.a78
MAP     := $(B)/barrage.map
LIBS    := $(FNLIB)/fujinet-a78.lib atari7800.lib

all: $(IMG)

# flags in a stamp, rewritten when they change (TEST=1 and back, or this
# file: a module moved to another bank)
$(B)/flags.stamp: FORCE
	@mkdir -p $(B)
	@echo '$(CFLAGS) $(AFLAGS)' > $(B)/flags.new
	@md5sum tools/atari7800/build.mk >> $(B)/flags.new
	@cmp -s $(B)/flags.new $@ || cp $(B)/flags.new $@
FORCE:

$(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) -c -o $@ $<
$(G)/%.s.o: %.s $(HDR) $(B)/flags.stamp
	@mkdir -p $(dir $@)
	ca65 $(AFLAGS) -o $@ $<
# REPL's (the core, in every bank): the link, the text and the HUD
REPL_O := $(G)/src/net.c.o $(G)/src/atari7800/scr.c.o $(G)/src/atari7800/fieldm.c.o \
	$(G)/src/atari7800/input.c.o
$(REPL_O): $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name REPLCODE --rodata-name REPLRO -c -o $@ $<
# the overlays: their own segments, their far functions renamed ovl_*
$(FIELD_O): $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name FIELDCODE --rodata-name FIELDRO $(FIELD_DEFS) -c -o $@ $<
$(G)/src/room.c.o $(G)/src/cat.c.o: $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name ROOMCODE --rodata-name ROOMRO $(ROOM_DEFS) -c -o $@ $<
$(G)/src/menus.c.o $(G)/src/atari7800/title.c.o: $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name MENUCODE --rodata-name MENURO $(MENU_DEFS) -c -o $@ $<
# (appkey.c is two banks': its literals, pooled at the file's end, in the
# core, where its boot half (bank 3) sees them too)
$(G)/src/appkey.c.o: $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name MENUCODE --rodata-name RODATA $(MENU_DEFS) $(BOOT_DEFS) -c -o $@ $<
$(FOPEN_O): $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name FOPEN --rodata-name FOPENRO $(FOPEN_DEFS) -c -o $@ $<
$(BOOT_O): $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name BOOTCODE --rodata-name BOOTRO $(BOOT_DEFS) -c -o $@ $<

gen/atari7800/far.s: $(FAR) tools/nes/mkfar.py
	@mkdir -p gen/atari7800
	python3 tools/nes/mkfar.py $(FAR) $@
gen/atari7800/title_hills.s: tools/atari7800/mktitle_a78.py
	@mkdir -p gen/atari7800
	python3 tools/atari7800/mktitle_a78.py $@
gen/atari7800/font.s: tools/atari7800/mkfont_a78.py gfx/coco/namco.bin
	@mkdir -p gen/atari7800
	python3 tools/atari7800/mkfont_a78.py $@
# (the title's tune in REPL: the frame's DLI plays it whichever bank is in)
# the sound's data in bank 4 (SNDDATA: the frame's DLI puts it in the window
# while it plays, src/atari7800/maria.s): the title's tune, the jingles
# (gen/atari/tunes.s's, the Atari's), the effects' steps (src/atari/sound.c
# puts them there itself: its tables of them, which snd_cue reads, stay in
# the core)
gen/atari7800/title.s: tools/mkmus_atari.py tools/mus.py music/title.txt
	@mkdir -p gen/atari7800
	python3 tools/mkmus_atari.py $@.tmp t_title=music/title.txt
	sed 's/^        \.rodata$$/        .segment "SNDDATA"/' $@.tmp > $@
	rm -f $@.tmp
gen/atari7800/tunes.s: gen/atari/tunes.s
	@mkdir -p gen/atari7800
	sed 's/^        \.rodata$$/        .segment "SNDDATA"/' $< > $@

$(IMG): $(OBJ) $(CFG) tools/atari7800/mkrom.py tools/nes/farcheck.py $(FNLIB)/fujinet-a78.lib
	python3 tools/nes/farcheck.py field $(FIELD_O) -- room $(ROOM_O) -- menu $(MENU_O) -- \
	  boot $(BOOT_O) -- sound $(SND_O) -- fopen $(FOPEN_O) -- core $(CORE_O)
	@mkdir -p $(R)
	cl65 -t atari7800 -C $(CFG) -vm -m $(MAP) -Wl --dbgfile,$(MAP:.map=.dbg) -o $(BIN) $(OBJ) $(LIBS)
	python3 tools/atari7800/mkrom.py $(MAP) $(BIN) $@

.PHONY: all FORCE
