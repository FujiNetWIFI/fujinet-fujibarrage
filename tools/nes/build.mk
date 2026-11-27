# build.mk -- FujiBarrage for the NES (cc65, in the defoogi container: the
# top-level Makefile's `make nes` runs it, after the host has built
# fujinet-lib for the NES and written the catalog). The image is MMC3
# (mapper 4), 256K PRG, 32K CHR-RAM, 8K WRAM (src/nes/barrage-nes.cfg; the
# header is src/nes/crt0.s's), checked by tools/nes/checkrom.py. One
# program (ONE_PROGRAM: the menu and the game, as the SMS's): the core in
# the fixed banks and R7's, the overlays in R6 -- the field (src/play.c),
# the rooms (src/room.c, src/cat.c and the catalog), the menus
# (src/menus.c, src/appkey.c's saves), the boot (src/appkey.c's load,
# src/test.c) -- reached through the wrappers tools/nes/far.txt lists.
#
#   ... CARD=1              M0's test card (src/nes/card.c) instead of the game
#   ... TEST=1              with the offline test scenes (tools/nessim)
#   ... TEST_SCENE=n TEST_ARG=a   a scene built in

B      := build/nes
R      := r2r/nes
CFG    := src/nes/barrage-nes.cfg
FNLIB  := build/fnlib-nes
FAR    := tools/nes/far.txt

CFLAGS := -t nes -g -Os -Cl -O --cpu 6502 -DONE_PROGRAM -DCAT_ROM -DUSING_FUJINET_LIB=1 \
	-I src -I src/platform-specific -I src/nes -I gen -I gen/nes -I src/test -I $(FNLIB)/include
AFLAGS := -t nes -g -D FLD_WIDE -I src/nes -I src/6502 -I gen/nes $(XAFLAGS)
ifeq ($(TEST),1)
  CFLAGS += -DTEST_SCENES -DNO_APPKEYS
endif
ifneq ($(TEST_SCENE),)
  CFLAGS += -DTEST_SCENES -DNO_APPKEYS -DTEST_SCENE=$(TEST_SCENE) -DTEST_ARG=$(or $(TEST_ARG),0)
endif
CFLAGS += $(XFLAGS)

HDR := $(wildcard src/*.h src/platform-specific/*.h src/nes/*.h src/nes/test/*.h src/test/*.h) \
	gen/config.h

ifeq ($(CARD),1)

SRC_C := src/nes/card.c
SRC_S := src/nes/crt0.s gen/nes/font.s
OBJ   := $(patsubst %,$(B)/card/%.o,$(SRC_C) $(SRC_S))
IMG   := $(R)/card.nes
MAP   := $(B)/card.map
LIBS  := nes.lib

else

FIELD_DEFS := $(shell python3 tools/nes/mkfar.py $(FAR) --defs 2)
ROOM_DEFS  := $(shell python3 tools/nes/mkfar.py $(FAR) --defs 3)
MENU_DEFS  := $(shell python3 tools/nes/mkfar.py $(FAR) --defs 4)
BOOT_DEFS  := $(shell python3 tools/nes/mkfar.py $(FAR) --defs 5)
FOPEN_DEFS := $(shell python3 tools/nes/mkfar.py $(FAR) --defs 7)

CORE_C  := src/main.c src/net.c src/nes/plat.c src/nes/input.c src/nes/nstrans.c \
	src/nes/scr.c src/nes/field.c
CORE_S  := src/nes/crt0.s src/nes/bank.s src/nes/fld.s src/nes/render.s src/nes/pt.s src/nes/snd.s \
	src/6502/ops.s src/6502/ply.s src/6502/trk.s gen/nes/far.s gen/nes/font.s gen/nes/spr.s \
	gen/nes/title.s
FIELD_C := src/play.c
ROOM_C  := src/room.c src/cat.c
ROOM_S  := gen/nes/catalog.s
MENU_C  := src/menus.c src/appkey.c
BOOT_C  := src/test.c
FOPEN_C := src/nes/fieldo.c
FOPEN_S := src/nes/hud.s
SND_S   := gen/nes/sfx.s gen/nes/mus.s

G := $(B)/game
CORE_O  := $(patsubst %,$(G)/%.o,$(CORE_C) $(CORE_S))
FIELD_O := $(patsubst %,$(G)/%.o,$(FIELD_C))
ROOM_O  := $(patsubst %,$(G)/%.o,$(ROOM_C) $(ROOM_S))
MENU_O  := $(patsubst %,$(G)/%.o,$(MENU_C))
BOOT_O  := $(patsubst %,$(G)/%.o,$(BOOT_C))
FOPEN_O := $(patsubst %,$(G)/%.o,$(FOPEN_C) $(FOPEN_S))
SND_O   := $(patsubst %,$(G)/%.o,$(SND_S))
OBJ     := $(CORE_O) $(FIELD_O) $(ROOM_O) $(MENU_O) $(BOOT_O) $(FOPEN_O) $(SND_O)
IMG     := $(R)/barrage.nes
MAP     := $(B)/barrage.map
LIBS    := $(FNLIB)/fujinet-nes.lib nes.lib
HDR     += gen/nes/catalog.h

endif

all: $(IMG)

# flags in a stamp, rewritten when they change (TEST=1 and back, or this
# file: a module moved to another bank)
$(B)/flags.stamp: FORCE
	@mkdir -p $(B)
	@echo '$(CFLAGS) $(AFLAGS) $(CARD)' > $(B)/flags.new
	@md5sum tools/nes/build.mk >> $(B)/flags.new
	@cmp -s $(B)/flags.new $@ || cp $(B)/flags.new $@
FORCE:

$(B)/card/%.c.o: %.c $(HDR) $(B)/flags.stamp
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) -c -o $@ $<
$(B)/card/%.s.o: %.s $(B)/flags.stamp
	@mkdir -p $(dir $@)
	ca65 $(AFLAGS) -o $@ $<

$(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) -c -o $@ $<
$(G)/%.s.o: %.s $(B)/flags.stamp
	@mkdir -p $(dir $@)
	ca65 $(AFLAGS) -o $@ $<
# R7's bank (CODE2: resident, as the fixed banks): the link and the loop
R7_O := $(G)/src/net.c.o $(G)/src/nes/nstrans.c.o $(G)/src/main.c.o
$(R7_O): $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name CODE2 --rodata-name CODE2 -c -o $@ $<
# the overlays: their own segments, their far functions renamed ovl_*
# (play.c's and test.c's words in R7's bank: field.c's overlay reads them)
$(FIELD_O): $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name FIELDCODE --rodata-name CODE2 $(FIELD_DEFS) -c -o $@ $<
$(patsubst %,$(G)/%.o,$(FOPEN_C)): $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name FOPEN --rodata-name FOPEN $(FOPEN_DEFS) -c -o $@ $<
$(G)/src/room.c.o $(G)/src/cat.c.o: $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name ROOMCODE --rodata-name ROOMRO $(ROOM_DEFS) -c -o $@ $<
$(G)/src/menus.c.o: $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name MENUCODE --rodata-name MENURO $(MENU_DEFS) -c -o $@ $<
# (appkey.c is two banks': its literals, pooled at the file's end, in the
# fixed banks, where its boot half (bank 5) sees them too)
$(G)/src/appkey.c.o: $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name MENUCODE --rodata-name RODATA $(MENU_DEFS) $(BOOT_DEFS) -c -o $@ $<
$(BOOT_O): $(G)/%.c.o: %.c $(HDR) $(B)/flags.stamp $(FAR)
	@mkdir -p $(dir $@)
	cl65 $(CFLAGS) --code-name BOOTCODE --rodata-name CODE2 $(BOOT_DEFS) -c -o $@ $<

gen/nes/far.s: $(FAR) tools/nes/mkfar.py
	python3 tools/nes/mkfar.py $(FAR) $@

gen/nes/font.s: tools/nes/mkgfx_nes.py
	python3 tools/nes/mkgfx_nes.py font $@
gen/nes/title.s: tools/nes/mkgfx_nes.py
	python3 tools/nes/mkgfx_nes.py title $@
gen/nes/spr.s: tools/nes/mkgfx_nes.py tools/mkgfx_tms.py
	python3 tools/nes/mkgfx_nes.py sprites $@

# the sound's data (bank 6: src/nes/snd.s plays it): the Adam's and the
# SMS's effects and tunes (tools/mksfx_z80.py) for ca65
SNDDEP := tools/mksfx_z80.py tools/mksfx_coco.py tools/mus.py tools/mkmus_intv.py src/intv/sfx.asm $(wildcard music/*.txt)
gen/nes/sfx.s: $(SNDDEP)
	python3 tools/mksfx_z80.py sfx $@ --ca65 SNDDATA
gen/nes/mus.s: $(SNDDEP)
	python3 tools/mksfx_z80.py tunes $@ --ca65 SNDDATA title=music/title.txt win=music/win.txt \
	  over=music/over.txt shop=music/shop.txt

ifneq ($(CARD),1)
$(IMG): $(OBJ) $(CFG) tools/nes/checkrom.py tools/nes/farcheck.py $(FNLIB)/fujinet-nes.lib
	python3 tools/nes/farcheck.py field $(FIELD_O) -- room $(ROOM_O) -- menu $(MENU_O) -- \
	  boot $(BOOT_O) -- fopen $(FOPEN_O) -- sound $(SND_O) -- core $(CORE_O)
else
$(IMG): $(OBJ) $(CFG) tools/nes/checkrom.py
endif
	@mkdir -p $(R)
	cl65 -t nes -C $(CFG) -vm -m $(MAP) -Wl --dbgfile,$(MAP:.map=.dbg) -o $@ $(OBJ) $(LIBS)
	python3 tools/nes/checkrom.py $(CFG) $(MAP) $@

.PHONY: all FORCE
