# build.mk -- FujiBarrage for MS-DOS (make msdos): four programs and the
# disk they go on, with Open Watcom (small model, 8086 instructions). The
# whole make runs in the defoogi container (the Makefile): wcc, wasm,
# wlink, python3 and mtools are all there.
#
#   BARRAGE.EXE, BMENU.EXE      CGA: 320x200 in 4 colours (src/msdos/cga)
#   BARRVGA.EXE, BMENUVGA.EXE   VGA and MCGA: mode 13h (src/msdos/vga, DOS_VGA)
#   BSEL.COM                    AUTOEXEC.BAT's: types one or the other
#                               (tools/dos/bsel.asm)
#   -> r2r/msdos/barrage.img, a 360K disk
#
# Knobs: TEST=1 (the game's test scenes; the fixture from the disk's
# files), TEST_SCENE=n TEST_ARG=a (a scene built in, for a rig that can't
# poke), STATS=1 (A:\STATS.TXT), VIDEO_CARD=1 (M0's test card), XFLAGS
# (both programs), FIXDIR (a fixture's files put on the disk:
# tools/dossim/fixture.py). Flags are stamped: a change rebuilds.

FNLIB := _cache/fujinet-lib/5.1.0-msdos
B := build/msdos
OUT := r2r/msdos
PYTHON ?= python3

WCC := wcc
WASM := wasm
WLINK := wlink

CBASE := -zq -ms -os -s -zp1 -0 -bt=dos -w3 -D__MSDOS__ \
	-i=src -i=src/platform-specific -i=gen -i=src/msdos -i=$(FNLIB)
ABASE := -zq -0 -i=src/8086

ifeq ($(TEST),1)
  GAME_X += -DTEST_SCENES -DNO_APPKEYS
endif
ifneq ($(TEST_SCENE),)
  GAME_X += -DTEST_SCENE=$(TEST_SCENE) $(if $(TEST_ARG),-DTEST_ARG=$(TEST_ARG))
endif
ifeq ($(STATS),1)
  BOTH_X += -DSTATS
  ABOTH_X += -dSTATS
endif
ifeq ($(VIDEO_CARD),1)
  BOTH_X += -DVIDEO_CARD
endif
BOTH_X += $(XFLAGS)

HDRS := $(wildcard src/*.h src/platform-specific/*.h src/msdos/*.h src/msdos/cga/*.h \
	src/msdos/vga/*.h src/msdos/test/*.h) gen/config.h

LINK_C := src/msdos/plat.c src/msdos/input.c src/msdos/nstrans.c src/msdos/fbnet.c \
	src/msdos/fbport.c src/msdos/lobby.c src/msdos/sound.c src/msdos/scr.c gen/msdos/font.c
LINK_ASM := src/msdos/uartisr.asm src/msdos/fbasm.asm src/msdos/timer.asm
MENU_SRC := src/main.c src/menus.c src/appkey.c src/net.c src/cat.c src/test.c $(LINK_C) \
	gen/msdos/title.c src/msdos/lzd.asm gen/msdos/sfx.asm gen/msdos/menutune.asm $(LINK_ASM)
GAME_SRC := src/main.c src/menus.c src/appkey.c src/net.c src/cat.c src/room.c \
	src/play.c src/test.c $(LINK_C) src/msdos/fixfile.c gen/msdos/sfx.asm gen/msdos/tunes.asm $(LINK_ASM)
# the field and the script player
FIELD_SRC := src/msdos/field.c gen/msdos/gfx.c src/8086/ply.asm src/8086/trk.asm \
	src/8086/ops.asm src/8086/pt.asm

.PHONY: all FORCE
all: $(OUT)/barrage.img

gen/msdos/gfx.c: tools/msdos/mkgfx_dos.py tools/mkgfx_coco.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/msdos/mkgfx_dos.py $@

gen/msdos/sfx.asm: tools/msdos/mksfx_dos.py tools/mksfx_coco.py src/intv/sfx.asm
	@mkdir -p $(dir $@)
	$(PYTHON) tools/msdos/mksfx_dos.py sfx $@

gen/msdos/tunes.asm: tools/msdos/mksfx_dos.py tools/mksfx_coco.py tools/mkmus_intv.py tools/mus.py \
	music/win.txt music/over.txt music/shop.txt
	@mkdir -p $(dir $@)
	$(PYTHON) tools/msdos/mksfx_dos.py tunes $@ win=music/win.txt over=music/over.txt shop=music/shop.txt

gen/msdos/menutune.asm: tools/msdos/mksfx_dos.py tools/mksfx_coco.py tools/mkmus_intv.py tools/mus.py music/title.txt
	@mkdir -p $(dir $@)
	$(PYTHON) tools/msdos/mksfx_dos.py tunes $@ title=music/title.txt

gen/msdos/title.c: tools/msdos/mktitle_dos.py tools/mktitle_coco.py gfx/coco/namco.bin
	@mkdir -p $(dir $@)
	$(PYTHON) tools/msdos/mktitle_dos.py $@

gen/msdos/font.c: tools/msdos/mkfont.py gfx/coco/namco.bin
	@mkdir -p $(dir $@)
	$(PYTHON) tools/msdos/mkfont.py gfx/coco/namco.bin $@

# $(1) the screen (cga, vga), $(2) the program (game, menu), $(3) its name
define PROG
$(1)_$(2)_DIR := $(B)/$(1)-$(2)
$(1)_$(2)_SRC := $(if $(filter game,$(2)),$(GAME_SRC) $(FIELD_SRC) src/msdos/$(1)/fld.asm,$(MENU_SRC)) \
	src/msdos/$(1)/video.c src/msdos/$(1)/text.c src/msdos/$(1)/scra.asm
$(1)_$(2)_OBJ := $$(patsubst %,$$($(1)_$(2)_DIR)/%.o,$$($(1)_$(2)_SRC))
$(1)_$(2)_CF := $(CBASE) -i=src/msdos/$(1) $(if $(filter vga,$(1)),-DDOS_VGA) \
	$(if $(filter menu,$(2)),-DMENU,$(GAME_X)) $(BOTH_X)
$(1)_$(2)_AF := $(ABASE) $(if $(filter vga,$(1)),-dDOS_VGA) $(if $(filter menu,$(2)),-dMENU) $(ABOTH_X)

$$($(1)_$(2)_DIR)/flags: FORCE
	@mkdir -p $$(dir $$@)
	@echo '$$($(1)_$(2)_CF) | $$($(1)_$(2)_AF)' | cmp -s - $$@ || \
	    echo '$$($(1)_$(2)_CF) | $$($(1)_$(2)_AF)' > $$@

$$($(1)_$(2)_DIR)/%.c.o: %.c $$($(1)_$(2)_DIR)/flags $(HDRS)
	@mkdir -p $$(dir $$@)
	$(WCC) $$($(1)_$(2)_CF) -fo=$$@ $$<

$$($(1)_$(2)_DIR)/%.asm.o: %.asm $$($(1)_$(2)_DIR)/flags
	@mkdir -p $$(dir $$@)
	$(WASM) $$($(1)_$(2)_AF) -fo=$$@ $$<

$(B)/$(3).exe: $$($(1)_$(2)_OBJ)
	$(WLINK) option quiet system dos option map=$(B)/$(3).map option stack=4096 \
	    disable 1014 name $$@ file {$$^} libpath $(FNLIB) library fujinet-msdos-5.1.0.lib
endef

$(eval $(call PROG,cga,game,barrage))
$(eval $(call PROG,cga,menu,bmenu))
$(eval $(call PROG,vga,game,barrvga))
$(eval $(call PROG,vga,menu,bmenuvga))

EXES := $(B)/barrage.exe $(B)/bmenu.exe $(B)/barrvga.exe $(B)/bmenuvga.exe

$(B)/bsel.com: tools/dos/bsel.asm
	@mkdir -p $(B)
	$(WASM) -zq -fo=$(B)/bsel.o $<
	$(WLINK) option quiet format dos com name $@ file $(B)/bsel.o

$(OUT)/barrage.img: $(EXES) $(B)/bsel.com FORCE
	@mkdir -p $(OUT)
	printf '@ECHO OFF\r\nBSEL\r\n' > $(B)/autoexec.bat
	rm -f $@.new
	mformat -C -f 360 -v BARRAGE -i $@.new ::
	mcopy -i $@.new $(B)/barrage.exe ::BARRAGE.EXE
	mcopy -i $@.new $(B)/bmenu.exe ::BMENU.EXE
	mcopy -i $@.new $(B)/barrvga.exe ::BARRVGA.EXE
	mcopy -i $@.new $(B)/bmenuvga.exe ::BMENUVGA.EXE
	mcopy -i $@.new $(B)/bsel.com ::BSEL.COM
	mcopy -i $@.new $(B)/autoexec.bat ::AUTOEXEC.BAT
	$(if $(FIXDIR),for f in $(FIXDIR)/*.BIN; do mcopy -i $@.new $$f ::; done)
	mv $@.new $@
	@$(PYTHON) tools/msdos/memcheck.py $(B)
