# build.mk -- FujiBarrage for the Atari 5200 (cc65, in the defoogi container:
# the top-level Makefile's `make atari5200` runs it, after the host has built
# fujinet-lib for the 5200 and written the catalog). The image is a 128K
# Super Cart: bank 0 the menu (-DMENU, src/atari5200/menu-a52.cfg), banks
# 1-3 the game (barrage-a52.cfg): the core in every bank, the field's
# overlay (src/play.c) in bank 3's own part, the rooms' (src/room.c, src/cat.c
# and the catalog) in bank 2's, the boot code's (src/appkey.c's BOOTCODE,
# src/test.c) in bank 1's, called through the wrappers tools/atari5200/far.txt
# lists.
#
#   ... TEST=1              with the offline test scenes (tools/a52sim)
#   ... TEST_SCENE=n TEST_ARG=a   a scene built in

B      := build/a52
R      := r2r/atari5200
FNLIB  := build/fnlib-a52
FAR    := tools/atari5200/far.txt

CFLAGS := -t atari5200 -Os -Cl -O --cpu 6502 -DCAT_ROM -DUSING_FUJINET_LIB=1 \
	-I src -I src/platform-specific -I src/atari -I gen -I gen/atari5200 \
	-I src/atari5200/test -I src/test -I $(FNLIB)/include
AFLAGS := -t atari5200 -I src/atari -I src/6502 -I gen/atari5200
MCFLAGS := $(CFLAGS) -DMENU $(XFLAGS)
MAFLAGS := $(AFLAGS) -D MENU
ifeq ($(TEST),1)
  CFLAGS += -DTEST_SCENES -DNO_APPKEYS
endif
ifneq ($(TEST_SCENE),)
  CFLAGS += -DTEST_SCENES -DNO_APPKEYS -DTEST_SCENE=$(TEST_SCENE) -DTEST_ARG=$(or $(TEST_ARG),0)
endif
CFLAGS += $(XFLAGS)

FIELD_DEFS := $(shell python3 tools/atari5200/mkfar.py $(FAR) --defs 3)
ROOM_DEFS  := $(shell python3 tools/atari5200/mkfar.py $(FAR) --defs 2)
BOOT_DEFS  := $(shell python3 tools/atari5200/mkfar.py $(FAR) --defs 1)

HDR := $(wildcard src/*.h src/platform-specific/*.h src/atari/*.h src/atari/*.inc src/6502/*.inc \
	src/test/*.h src/atari5200/test/*.h) gen/config.h gen/font35.h gen/atari5200/catalog.h

# ---- the game ----------------------------------------------------------------
CORE_C  := src/main.c src/menus.c src/net.c src/appkey.c src/atari/field.c src/atari/plat.c \
	src/atari/scr.c src/atari/sound.c src/atari5200/input.c src/atari5200/nstrans.c
CORE_S  := src/atari5200/crt0.s src/atari5200/bank.s src/atari/bln.s src/atari/dlc.s \
	src/atari/dsp.s src/atari/fld.s src/atari/pt.s src/atari/snd.s \
	src/6502/ops.s src/6502/ply.s src/6502/trk.s gen/atari/tunes.s gen/atari5200/far.s
FIELD_C := src/play.c
ROOM_C  := src/room.c src/cat.c
BOOT_C  := src/test.c
ROOM_S  := gen/atari5200/catalog.s

G := $(B)/game
CORE_O  := $(patsubst %,$(G)/%.o,$(CORE_C) $(CORE_S))
FIELD_O := $(patsubst %,$(G)/%.o,$(FIELD_C))
ROOM_O  := $(patsubst %,$(G)/%.o,$(ROOM_C) $(ROOM_S))
BOOT_O  := $(patsubst %,$(G)/%.o,$(BOOT_C))

# ---- the menu ----------------------------------------------------------------
MENU_C := src/main.c src/menus.c src/appkey.c src/net.c src/cat.c src/atari/scr.c src/atari/plat.c \
	src/atari/sound.c src/atari5200/input.c src/atari5200/nstrans.c
MENU_S := src/atari5200/crt0.s src/atari5200/bank.s src/atari/snd.s src/atari/dsp.s \
	gen/atari5200/catalog.s $(B)/title.s
M := $(B)/menu
MENU_O := $(patsubst %,$(M)/%.o,$(MENU_C) $(MENU_S))

all: $(R)/barrage.a52

# flags in a stamp, rewritten when they change (TEST=1 and back)
$(B)/flags.stamp: FORCE
	@mkdir -p $(B)
	@echo '$(CFLAGS) $(MCFLAGS) $(AFLAGS)' > $(B)/flags.new
	@cmp -s $(B)/flags.new $@ || cp $(B)/flags.new $@
FORCE:

$(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) -c -o $@ $<
$(G)/%.s.o: %.s $(HDR) $(B)/flags.stamp
	@mkdir -p $(dir $@)
	ca65 $(AFLAGS) -o $@ $<
# the overlays: their own segments, their far functions renamed ovl_*
$(FIELD_O): $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name FIELDCODE --rodata-name FIELDRO $(FIELD_DEFS) -c -o $@ $<
$(G)/src/room.c.o $(G)/src/cat.c.o: $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name ROOMCODE --rodata-name ROOMRO $(ROOM_DEFS) -c -o $@ $<
$(BOOT_O): $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name BOOTCODE --rodata-name BOOTRO $(BOOT_DEFS) -c -o $@ $<
# (the core's, but for its BOOTCODE segment: bank 1's)
$(G)/src/appkey.c.o: src/appkey.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) -Dappkey_load=ovl_appkey_load -c -o $@ $<

$(M)/%.c.o: %.c $(HDR) $(B)/flags.stamp
	@mkdir -p $(dir $@)
	cl65 $(MCFLAGS) -c -o $@ $<
$(M)/%.s.o: %.s $(HDR) $(B)/flags.stamp
	@mkdir -p $(dir $@)
	ca65 $(MAFLAGS) -o $@ $<

gen/atari5200/far.s: $(FAR) tools/atari5200/mkfar.py
	python3 tools/atari5200/mkfar.py $(FAR) $@

$(B)/title.s: tools/mkmus_atari.py tools/mus.py music/title.txt
	@mkdir -p $(B)
	python3 tools/mkmus_atari.py $@ t_title=music/title.txt

$(B)/game.bin: $(CORE_O) $(FIELD_O) $(ROOM_O) $(BOOT_O) src/atari5200/barrage-a52.cfg $(FNLIB)/fujinet-a52.lib tools/atari5200/farcheck.py
	python3 tools/atari5200/farcheck.py $(FIELD_O) -- $(ROOM_O) -- $(BOOT_O) -- $(CORE_O)
	cl65 -t atari5200 -C src/atari5200/barrage-a52.cfg -vm -m $(B)/game.map -o $@ \
	  $(CORE_O) $(FIELD_O) $(ROOM_O) $(BOOT_O) $(FNLIB)/fujinet-a52.lib

$(B)/menu.bin: $(MENU_O) src/atari5200/menu-a52.cfg $(FNLIB)/fujinet-a52.lib
	cl65 -t atari5200 -C src/atari5200/menu-a52.cfg -vm -m $(B)/menu.map -o $@ \
	  $(MENU_O) $(FNLIB)/fujinet-a52.lib

$(R)/barrage.a52: $(B)/game.bin $(B)/menu.bin tools/atari5200/mkrom.py
	@mkdir -p $(R)
	python3 tools/atari5200/mkrom.py $(B)/menu.map $(B)/menu.bin $(B)/game.map $(B)/game.bin $@

.PHONY: all FORCE
