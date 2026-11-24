# tools/h89/build.mk -- FujiBarrage for the Heathkit H89 (z88dk: zcc with
# zsdcc, z80asm), run in the defoogi container unless z88dk is installed
# (tools/z80/z88dk):
#
#   make h89-cpm        r2r/h89-cpm/barrage.h8d  (CP/M 2.2.04: BARRAGE.COM)
#   make h89-hdos       r2r/h89-hdos/barrage.h8d (HDOS 2.0: BARRAGE.ABS)
#   make h89-boot       r2r/h89-cpm/barrage-boot.h8d (a bootable 64K CP/M disk)
#
# One program (-DONE_PROGRAM: the menu and the game together; the H89 has
# memory enough and no second program to load). The screen is the H19's
# graphics characters (src/h89/field.c, scr.c, h19.c); the link to the server
# the H89-ESP32 card's 82C55, FujiBus by hand (fbport.c, nstrans.c);
# fujinet-lib (build/fnlib-h89, tools/z80/mkfnlib.sh) opens and closes. The
# variables and a stack must end under D806H (CP/M, a 62K system) and D090H
# (HDOS with FN: loaded): tools/h89/h89check.py says how close.
#   HDOS: the program at H89_HDOS_ORG, its stack under it from 2280H, the
#   variables after the image, out of the file (src/h89/tpa.asm).
# TEST=1 TEST_SCENE=n TEST_ARG=a: the offline scenes (as the other ports').
# H89_RIG_KEYS=1: ` is ESC (for the rig). H89_PROF=1: the clock interrupt
# keeps the PCs it interrupted (h89sim pcprof).

OS ?= cpm
ZCC := zcc
B := build/h89-$(OS)
R := r2r/h89-$(OS)
FNLIB := build/fnlib-h89
H89_HDOS_ORG := 0x2480
NAME := barrage

ifeq ($(OS),hdos)
  # (HDOS has 44K for everything: the compiler's size options, and no scroll by terminal)
  ZOPT ?= -SO3
  SIZEOPT := -Cs--opt-code-size -DH89_NO_SCROLL
else
  ZOPT ?= -SO2
endif
COMMON := -compiler=sdcc $(ZOPT) $(SIZEOPT) -DUSING_FUJINET_LIB=1 -D__H89__ -DONE_PROGRAM
INC := -Isrc -Isrc/platform-specific -Isrc/test -Igen -Isrc/h89 -Isrc/z80 -I$(FNLIB)/include
ifeq ($(TEST),1)
  COMMON += -DTEST_SCENES -DNO_APPKEYS $(TESTX)
endif
ifneq ($(TEST_SCENE),)
  COMMON += -DTEST_SCENE=$(TEST_SCENE) -DTEST_ARG=$(or $(TEST_ARG),0)
endif
ifeq ($(H89_RIG_KEYS),1)
  COMMON += -DH89_RIG_KEYS
endif
COMMON += $(XFLAGS)
ASX := $(if $(filter 1,$(H89_PROF)),-Ca-DH89_PROF=1)
LDCOMMON := -compiler=sdcc -m -create-app -pragma-define:CRT_ENABLE_STDIO=0 \
	-pragma-define:CLIB_FOPEN_MAX=0 -pragma-define:CLIB_OPEN_MAX=0 -pragma-define:CRT_ENABLE_COMMANDLINE=0
ifeq ($(OS),cpm)
  TARGET := +cpm
  LDFLAGS := $(TARGET) $(LDCOMMON) -pragma-define:CRT_INITIALIZE_BSS=1 -pragma-define:noprotectmsdos=1
  EXT := com
  TOP := 0xD806
  SBUF := 2048
else
  TARGET := +hdos -startup=3 -Cz--abs -pragma-define:CLIB_DISABLE_FGETS_CURSOR=1
  OVL_WIN := 0x2480
  OVL_SIZE ?= 0x3C00
  H89_HDOS_ORG := $(shell printf '0x%X' $$(($(OVL_WIN) + $(OVL_SIZE))))
  LDFLAGS := $(TARGET) $(LDCOMMON) -pragma-define:CRT_ORG_CODE=$(H89_HDOS_ORG) \
	-pragma-define:REGISTER_SP=$(shell printf '0x%X' $$(($(OVL_WIN) - 2))) \
	-pragma-define:CLIB_EXIT_STACK_SIZE=0 -pragma-define:CRT_INITIALIZE_BSS=0 -pragma-define:CRT_TRIM_BSS=1 \
	-pragma-define:MMAP=-1 -Ca-Isrc/h89 -pragma-define:H89_OVL_WIN=$(OVL_WIN)
  ASX += -Ca-DH89_HDOS -Ca-DH89_OVL_WIN=$(OVL_WIN) -Ca-DH89_ORG=$(H89_HDOS_ORG)
  EXT := abs
  TOP := 0xD090
  SBUF := 1024
endif
CFLAGS := $(TARGET) $(COMMON) -DSCRIPT_BUF=$(SBUF) $(INC)

# the overlays (HDOS): the waiting room, shop, results and menus; and the field, the script
# player and everything that draws it. The rest is resident. An entry point of an overlay
# is compiled as name_ovl and reached by a resident stub (tools/h89/mkovl.py).
ROOM_C := src/room.c src/menus.c src/appkey.c
FIELD_C := src/play.c src/h89/field.c src/h89/fld.c
ROOM_ENT := message name_entry results_enter results_frame room_list shop_enter shop_frame shop_state title \
	wait_enter wait_frame wait_state appkey_load appkey_save_name appkey_save_room appkey_clear_room
FIELD_ENT := field_column field_init field_loading field_sync play_enter play_esc play_frame play_script \
	play_skip play_state
RES_C := src/main.c src/net.c src/cat.c src/test.c \
	src/h89/plat.c src/h89/input.c src/h89/nstrans.c src/h89/fbport.c src/h89/scr.c \
	src/h89/sound.c src/h89/h19.c
RES_ASM := src/h89/h89io.asm src/h89/nsfast.asm src/h89/tpa.asm src/h89/h19cell.asm src/h89/h19run.asm
PLAYER := ply trk ops pt
ifeq ($(OS),hdos)
  SRC := $(RES_C) $(ROOM_C) $(FIELD_C) $(RES_ASM) src/h89/ovl.asm
  OBJ := $(patsubst %,$(B)/%.o,$(basename $(SRC))) $(B)/ovlstubs.o $(B)/ovl/fldglue.o \
	$(foreach p,$(PLAYER),$(B)/ovl/$(p).o)
else
  SRC := $(RES_C) $(ROOM_C) $(FIELD_C) $(RES_ASM) src/h89/fldglue.asm $(foreach p,$(PLAYER),src/z80/$(p).asm)
  OBJ := $(patsubst %,$(B)/%.o,$(basename $(SRC)))
endif
HDRS := $(wildcard src/*.h src/platform-specific/*.h src/h89/*.h) gen/config.h
FLAGS := $(B)/flags

all: $(R)/$(NAME).h8d

$(FLAGS): FORCE
	@mkdir -p $(B)
	@echo '$(CFLAGS) $(ASX)' | cmp -s - $@ || echo '$(CFLAGS) $(ASX)' > $@

$(B)/%.o: %.c $(HDRS) $(FLAGS)
	@mkdir -p $(dir $@)
	$(ZCC) $(CFLAGS) -c -o $@ $<
$(B)/%.o: %.asm $(FLAGS) $(wildcard src/z80/*.inc src/h89/*.inc)
	@mkdir -p $(dir $@)
	$(ZCC) $(CFLAGS) $(ASX) -Ca-Isrc/z80 -Ca-Isrc/h89 -Ca-Isrc -c -o $@ $<

ifeq ($(OS),hdos)
# the overlays' own flags: their code and constants in the overlay's sections, their entry
# points renamed
$(patsubst %.c,$(B)/%.o,$(ROOM_C)): CFLAGS += --codeseg=code_ovl_room --constseg=rodata_ovl_room \
	$(foreach n,$(ROOM_ENT),-D$(n)=$(n)_ovl)
$(patsubst %.c,$(B)/%.o,$(FIELD_C)): CFLAGS += --codeseg=code_ovl_field --constseg=rodata_ovl_field \
	$(foreach n,$(FIELD_ENT),-D$(n)=$(n)_ovl)
# the player's assembly: the same files with their code in the field overlay's section
$(B)/ovl/%.asm: src/z80/%.asm
	@mkdir -p $(dir $@)
	sed 's/SECTION[[:space:]]\+code_user/SECTION code_ovl_field/' $< > $@
$(B)/ovl/fldglue.asm: src/h89/fldglue.asm
	@mkdir -p $(dir $@)
	sed 's/SECTION[[:space:]]\+code_user/SECTION code_ovl_field/' $< > $@
$(B)/ovlstubs.asm: tools/h89/mkovl.py $(FLAGS)
	@mkdir -p $(dir $@)
	python3 tools/h89/mkovl.py $@ $(foreach n,$(ROOM_ENT),room:$(n)) $(foreach n,$(FIELD_ENT),field:$(n))
$(B)/ovl/%.o: $(B)/ovl/%.asm $(FLAGS) $(wildcard src/z80/*.inc src/h89/*.inc)
	$(ZCC) $(CFLAGS) $(ASX) -Ca-Isrc/z80 -Ca-Isrc/h89 -Ca-Isrc -c -o $@ $<
$(B)/ovlstubs.o: $(B)/ovlstubs.asm $(FLAGS)
	$(ZCC) $(CFLAGS) $(ASX) -c -o $@ $<
endif

# the link: the image (and its map) come out at $(R)/$(NAME).$(EXT)
$(R)/$(NAME).$(EXT): $(OBJ) $(FNLIB)/fujinet-h89.lib
	@mkdir -p $(R)
	$(ZCC) $(LDFLAGS) -o $(R)/$(NAME).bin $(OBJ) -L$(FNLIB) -lfujinet-h89
	@for e in COM ABS; do test -f $(R)/BARRAGE.$$e && mv -f $(R)/BARRAGE.$$e $(R)/$(NAME).`echo $$e | tr A-Z a-z`; done; true
	@test -f $@ || { echo "build.mk: no $@ from appmake"; exit 1; }
ifeq ($(OS),hdos)
	python3 tools/h89/hdosovl.py $(R)/$(NAME) --objs $(B) --win $(OVL_WIN) --size $(OVL_SIZE) --top $(TOP)
else
	python3 tools/h89/h89check.py $(R)/$(NAME) --os $(OS) --top $(TOP)
endif

ifeq ($(OS),cpm)
$(R)/$(NAME).h8d: $(R)/$(NAME).com
	@mkdir -p $(B)/disk
	cp $(R)/$(NAME).com $(B)/disk/BARRAGE.COM
	cd $(B)/disk && rm -f disk && z88dk-appmake +cpmdisk -f hz89 --container raw --no-com-file -b BARRAGE.COM -o disk
	python3 -I tools/h89/cpm-dir-fixup.py $(B)/disk/disk
	mv $(B)/disk/disk $@
else
$(R)/$(NAME).h8d: $(R)/$(NAME).abs
	@mkdir -p $(B)/disk
	cp $(R)/$(NAME).abs $(B)/disk/disk.bin
	cd $(B)/disk && rm -f disk.h8d && z88dk-appmake +hdos --dumb -b disk.bin -o BARRAGE.ABS
	python3 -I tools/h89/hdos-dir-fixup.py $(B)/disk/disk.h8d BARRAGE.ABS $(B)/disk/disk.bin
	mv $(B)/disk/disk.h8d $@
endif

.PHONY: all FORCE
FORCE:
