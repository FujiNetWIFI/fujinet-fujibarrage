# FujiBarrage for FujiNet 8-bit computers (MekkoGX).
#
#   make atari                     r2r/atari/barrage.atr (48K Atari 400/800, XL/XE)
#   defoogi make atari             the same in the toolchain container
#   make SERVER=host:9913          server used when the lobby gave none (remembered)
#   make server                    server/barraged and server/refview (Go)
#   make proto                     src/proto.h from the server's constants
#   make fixture                   src/test/fixture.h: the test scenes' field and shot
#   make test                      the server's tests
#   make atari TEST=1              with the offline test scenes (tools/atarisim;
#                                  rm -rf build/barrage first: make doesn't see flags change)
#   make apple2                    r2r/apple2/barrage.po (128K //e, //c, IIgs; HGR)
#   make apple2 TEST=1             with the offline test scenes (tools/a2sim)
#   make fixture-apple2            src/apple2/test/fixture.h: its test scenes' field and shot
#   make intv                      r2r/intv/barrage.rom (Intellivision, FujiNet cartridge;
#                                  src/intv/Makefile: IntyBASIC + as1600, TEST=1 as above)
#   make fixture-intv              src/intv/test/fixture.bas: its test scenes' field and shot
#   make astrocade                 r2r/astrocade/barrage.bin (Bally Astrocade, FujiNet cartridge;
#                                  src/astrocade/build.sh: zmac, the APPBANK image)
#   make astrocade-rig             build/astrocade-rig/barrage.bin: default server 127.0.0.1:9913
#   make coco                      r2r/coco/barrage.dsk (CoCo 1/2 64K and CoCo 3, one disk;
#                                  tools/coco/build.mk: CMOC, TEST=1 and VIDEO_CARD=1 as above)
#   make fixture-astrocade         src/astrocade/test/fixture.inc: its test scenes' field and shot
#   make atari5200                 r2r/atari5200/barrage.a52 (Atari 5200, FujiNet cartridge;
#                                  tools/atari5200/build.mk: cc65 in defoogi, TEST=1, TEST_SCENE=n)
#   make adam                      r2r/adam/barrage.ddp (Coleco Adam; tools/adam/build.mk:
#                                  z88dk in defoogi, TEST=1, TEST_SCENE=n and VIDEO_CARD=1)
#   make arcadia                   r2r/arcadia/barrage.bin (Emerson Arcadia 2001, FujiNet
#                                  cartridge; src/arcadia/build.sh: AS, the APPBANK image)
#   make arcadia-rig               build/arcadia-rig/barrage.bin: default server 127.0.0.1:9913
#   make fixture-arcadia           src/arcadia/test/fixture.inc: its test scene's field and shot
#   make msdos                     r2r/msdos/barrage.img (an IBM PC: CGA, 128K, PCjr; or VGA
#                                  and MCGA: tools/msdos/build.mk, Watcom in defoogi;
#                                  TEST=1, TEST_SCENE=n, STATS=1, VIDEO_CARD=1, FIXDIR=)
#
# src/ is platform neutral; src/platform-specific/*.h is what each platform
# implements in src/<platform>/. The server sends every client its field and
# shots at its own resolution (docs/PROTOCOL.md).

PRODUCT = barrage
PLATFORMS = atari apple2

# src/6502: the script player (ply.s, trk.s, ops.s) every 6502 port shares,
# drawing through its own fld.s
SRC_DIRS = src src/%PLATFORM% src/6502 gen/%PLATFORM%
EXTRA_INCLUDE = src src/platform-specific src/test gen src/6502

# fujinet-lib (appkeys): a release, fetched into _cache (works in defoogi)
FUJINET_LIB = 5.1.0

SERVER ?=
PYTHON ?= python3

# ---- Atari 400/800 (48K), XL/XE -------------------------------------------
# cc65, no DOS: picoboot loads BARRAGE.COM, whose first chunk (src/atari/
# start.s) switches BASIC off. Memory: src/atari/barrage.cfg.
CFLAGS_EXTRA_ATARI += -Os -Cl
# TEST=1: with the offline test scenes (src/test.c) for tools/atarisim
ifeq ($(TEST),1)
  CFLAGS_EXTRA_ATARI += -DTEST_SCENES -DNO_APPKEYS
endif
LDFLAGS_EXTRA_ATARI += -C src/atari/barrage.cfg -m build/barrage-atari.map
EXECUTABLE_EXTRA_DEPS_ATARI += src/atari/barrage.cfg gen/font35.h src/test/fixture.h

r2r/atari/$(PRODUCT): gen-atari

# MENU.COM (-DMENU): the title, the name and the room list (src/main.c).
# BARRAGE.COM boots; it loads MENU.COM when the appkeys name no room
# (src/atari/xload.s), and MENU.COM boots the disk again once one is picked.
MENU_SRC_ATARI = src/main.c src/menus.c src/appkey.c src/net.c src/cat.c \
	src/atari/scr.c src/atari/input.c src/atari/plat.c src/atari/sound.c src/atari/nsatari.c \
	src/atari/snd.s src/atari/nstream.s src/atari/dsp.s src/atari/start.s
MENU_DIR_ATARI = build/menu-atari
MENU_COM_ATARI = $(MENU_DIR_ATARI)/menu.com
MENU_OBJ_ATARI = $(patsubst src/%,$(MENU_DIR_ATARI)/%.o,$(MENU_SRC_ATARI)) $(MENU_DIR_ATARI)/title.s.o
MENU_HDR = $(wildcard src/*.h src/platform-specific/*.h src/atari/*.h src/atari/*.inc) gen/config.h
MENU_LIB_ATARI = _cache/fujinet-lib/$(FUJINET_LIB)-atari
MENU_CFLAGS = -t atari -Os -Cl -O --cpu 6502 -DMENU -DUSING_FUJINET_LIB=1 \
	--include-dir src --include-dir src/platform-specific --include-dir gen --include-dir $(MENU_LIB_ATARI)

$(MENU_DIR_ATARI)/%.c.o: src/%.c $(MENU_HDR)
	@mkdir -p $(dir $@)
	cl65 $(MENU_CFLAGS) -c -o $@ $<

$(MENU_DIR_ATARI)/%.s.o: src/%.s $(MENU_HDR)
	@mkdir -p $(dir $@)
	cl65 -t atari --asm-include-dir src/atari -c -o $@ $<

src/atari/menu.cfg: src/atari/barrage.cfg
	$(PYTHON) -c "import sys; c=open(sys.argv[1]).read(); c=c.replace('# barrage.cfg -- FujiBarrage on a 48K Atari 400/800 (and up).', '# menu.cfg -- MENU.COM (the title, the name, the rooms): barrage.cfg without the boot code.\\n# GENERATED from barrage.cfg by the Makefile.'); c=''.join(l for l in c.splitlines(True) if 'BOOT' not in l or l.lstrip().startswith('#')); open(sys.argv[2],'w').write(c)" $< $@

$(MENU_DIR_ATARI)/title.s: tools/mkmus_atari.py tools/mus.py music/title.txt
	@mkdir -p $(dir $@)
	$(PYTHON) tools/mkmus_atari.py $@ t_title=music/title.txt

$(MENU_DIR_ATARI)/title.s.o: $(MENU_DIR_ATARI)/title.s
	cl65 -t atari -c -o $@ $<

$(MENU_COM_ATARI): $(MENU_OBJ_ATARI) src/atari/menu.cfg
	cl65 -t atari -C src/atari/menu.cfg -m $(MENU_DIR_ATARI)/menu.map -o $@ $(MENU_OBJ_ATARI) \
	  --lib-path $(MENU_LIB_ATARI) fujinet-atari-$(FUJINET_LIB).lib

DISK_EXTRA_FILES_ATARI += $(MENU_COM_ATARI)
DISK_EXTRA_DEPS_ATARI += $(MENU_COM_ATARI)

.PHONY: gen-atari server proto fixture test FORCE

gen-atari: gen/config.h gen/font35.h gen/atari/tunes.s

# the game's jingles (music/*.txt -> POKEY steps); the title is MENU.COM's
gen/atari/tunes.s: tools/mkmus_atari.py tools/mus.py music/win.txt music/over.txt music/shop.txt
	@mkdir -p $(dir $@)
	$(PYTHON) tools/mkmus_atari.py $@ t_win=music/win.txt t_over=music/over.txt t_shop=music/shop.txt

gen/config.h: FORCE
	@mkdir -p gen
	@[ -z "$(SERVER)" ] || echo "$(SERVER)" > gen/server
	@[ -f gen/server ] || echo "localhost:9913" > gen/server
	@printf '/* GENERATED by the Makefile. */\n#define DEFAULT_SERVER "%s"\n' "$$(cat gen/server)" > gen/config.h.new
	@cmp -s gen/config.h.new gen/config.h || cp gen/config.h.new gen/config.h

gen/font35.h: tools/mkfont35.py gfx/font3x5.txt
	@mkdir -p gen
	$(PYTHON) tools/mkfont35.py gfx/font3x5.txt $@

server:
	cd server && go build -o barraged ./cmd/barraged && go build -o refview ./cmd/refview

proto:
	cd server && go run ./cmd/barraged -cheader > ../src/proto.h
	sed -n 's/^#define \([A-Z_0-9]*\) *\([0-9][0-9]*\)$$/\1 = \2/p' src/proto.h > src/6502/proto.inc
	sed -n 's/^#define \([A-Z_0-9]*\) *\([0-9][0-9]*\)$$/\1 EQU \2/p' src/proto.h > src/6809/proto.inc
	@mkdir -p src/z80
	sed -n 's/^#define \([A-Z_0-9]*\) *\([0-9][0-9]*\)$$/\tdefc \1 = \2/p' src/proto.h > src/z80/proto.inc
	@mkdir -p src/8086
	sed -n 's/^#define \([A-Z_0-9]*\) *\([0-9][0-9]*\)$$/\1 EQU \2/p' src/proto.h > src/8086/proto.inc

# ---- Atari 5200 (the FujiNet 5200 cartridge) ---------------------------------
# The Atari's client (src/atari) assembled for the 5200 (src/atari5200): cc65
# in the defoogi container (tools/atari5200/build.mk), a 128K Super Cart, the
# menu in bank 0 and the game in banks 1-3. fujinet-lib is
# fujinet-lib-experimental's add-atari5200 branch, built on the host
# (tools/atari5200/mkfnlib.sh: in defoogi $HOME is another). The catalog is
# in ROM (16K of RAM): the server's own bytes (barraged -catalog).
FNLIB_5200 ?= $(HOME)/Workspace/fnlib-5200
SERVER_GO := $(shell find server -name '*.go' -not -name '*_test.go')

atari5200: gen/config.h gen/font35.h gen/atari/tunes.s gen/atari5200/catalog.h build/fnlib-a52/fujinet-a52.lib
	defoogi $(MAKE) -f tools/atari5200/build.mk $(if $(TEST),TEST=$(TEST)) \
	    $(if $(TEST_SCENE),TEST_SCENE=$(TEST_SCENE) TEST_ARG=$(TEST_ARG)) $(if $(XFLAGS),XFLAGS="$(XFLAGS)")

build/fnlib-a52/fujinet-a52.lib: tools/atari5200/mkfnlib.sh
	tools/atari5200/mkfnlib.sh $(FNLIB_5200) build/fnlib-a52

gen/atari5200/catalog.h: $(SERVER_GO) tools/atari5200/mkcat.py src/proto.h
	@mkdir -p build/a52 gen/atari5200
	cd server && go run ./cmd/barraged -catalog ../build/a52/catalog.bin
	$(PYTHON) tools/atari5200/mkcat.py build/a52/catalog.bin src/proto.h gen/atari5200/catalog.s $@

# the test scenes' field and shot at the 5200's script buffer
fixture-atari5200:
	@mkdir -p src/atari5200/test
	cd server && go run ./cmd/refview -flat -shots 1 -seed 5 -weapon 3 -buf 1024 -fixture-all ../src/atari5200/test/fixture.h -out ../build/refview-a52

.PHONY: atari5200 fixture-atari5200

# ---- Coleco Adam (the TMS9918A, the SN76489) --------------------------------
# z88dk's zcc with zsdcc and z80asm, in the defoogi container unless z88dk is
# installed (tools/z80/z88dk): tools/adam/build.mk. fujinet-lib is
# fujinet-lib-experimental built with zsdcc (tools/z80/fnlib_adam.sh, on
# the host: it reads the other repository).
FNLIB_EXP ?= $(HOME)/Workspace/fujinet-lib-experimental
FNLIB_COMMIT ?= a29e57f
adam: gen/config.h build/fnlib-adam/fujinet-adam.lib
	tools/z80/z88dk $(MAKE) -f tools/adam/build.mk $(if $(TEST),TEST=$(TEST)) \
	    $(if $(TEST_SCENE),TEST_SCENE=$(TEST_SCENE) TEST_ARG=$(TEST_ARG)) $(if $(VIDEO_CARD),VIDEO_CARD=$(VIDEO_CARD)) \
	    $(if $(TESTX),TESTX="$(TESTX)") $(if $(XFLAGS),XFLAGS="$(XFLAGS)")

build/fnlib-adam/fujinet-adam.lib:
	tools/z80/fnlib_adam.sh $(FNLIB_EXP) $(FNLIB_COMMIT) build/fnlib-adam

.PHONY: adam

# ---- MS-DOS: an IBM PC, XT or PCjr (CGA, 128K) or a VGA/MCGA PC, one disk ----
# Open Watcom in defoogi (tools/msdos/build.mk, the whole make there):
# BARRAGE.EXE and BMENU.EXE for CGA (src/msdos/cga), BARRVGA.EXE and
# BMENUVGA.EXE for VGA and MCGA (src/msdos/vga), BSEL.COM picks.
# fujinet-lib is 5.1.0's msdos release (small model), fetched here.
MSDOS_FNLIB = _cache/fujinet-lib/$(FUJINET_LIB)-msdos
msdos: gen/config.h $(MSDOS_FNLIB)/fujinet-msdos-$(FUJINET_LIB).lib
	defoogi $(MAKE) -f tools/msdos/build.mk $(if $(TEST),TEST=$(TEST)) \
	    $(if $(TEST_SCENE),TEST_SCENE=$(TEST_SCENE) TEST_ARG=$(TEST_ARG)) $(if $(STATS),STATS=$(STATS)) \
	    $(if $(VIDEO_CARD),VIDEO_CARD=$(VIDEO_CARD)) $(if $(XFLAGS),XFLAGS="$(XFLAGS)") $(if $(FIXDIR),FIXDIR=$(FIXDIR))

$(MSDOS_FNLIB)/fujinet-msdos-$(FUJINET_LIB).lib:
	@mkdir -p $(MSDOS_FNLIB)
	curl -sSL -o _cache/fujinet-lib/fujinet-lib-msdos-$(FUJINET_LIB).zip \
	    https://github.com/FujiNetWIFI/fujinet-lib/releases/download/v$(FUJINET_LIB)/fujinet-lib-msdos-$(FUJINET_LIB).zip
	cd $(MSDOS_FNLIB) && unzip -oq ../fujinet-lib-msdos-$(FUJINET_LIB).zip

.PHONY: msdos

# ---- TRS-80 Color Computer: CoCo 1/2 (PMODE 4) and CoCo 3 (GIME), one disk ----
# CMOC + lwtools + toolshed's decb, outside defoogi: tools/coco/build.mk
coco: gen/config.h
	$(MAKE) -f tools/coco/build.mk $(if $(TEST),TEST=$(TEST)) $(if $(VIDEO_CARD),VIDEO_CARD=$(VIDEO_CARD))

# the test scenes' field and shot at each CoCo's shape (every segment)
fixture-coco:
	cd server && go run ./cmd/refview -shots 1 -seed 5 -weapon 3 -buf 1536 -w 128 -h 168 -par 109 \
	    -tankw 5 -tankh 5 -fixture-all ../src/coco2/test/fixture.h -out ../build/refview-coco2
	cd server && go run ./cmd/refview -shots 1 -seed 5 -weapon 3 -buf 1536 -w 320 -h 176 -par 55 \
	    -tankw 12 -tankh 6 -fixture-all ../src/coco3/test/fixture.h -out ../build/refview-coco3

.PHONY: coco fixture-coco

fixture:
	cd server && go run ./cmd/refview -flat -shots 1 -seed 5 -weapon 3 -buf 1536 -fixture ../src/test/fixture.h -out ../build/refview

test:
	cd server && go test ./...

# ---- Apple II (128K //e, //c, IIgs), high-res ----------------------------
# cc65, ProDOS: BARRAGE.SYSTEM (tools/a2loader) loads BARRAGE around HGR
# page 1, which is the field itself (src/apple2/fld.s). Memory:
# src/apple2/barrage.cfg.
CFLAGS_EXTRA_APPLE2 += -Os -Cl
ifeq ($(TEST),1)
  CFLAGS_EXTRA_APPLE2 += -DTEST_SCENES -DNO_APPKEYS
endif
LDFLAGS_EXTRA_APPLE2 += -C src/apple2/barrage.cfg -m build/barrage-apple2.map
EXECUTABLE_EXTRA_DEPS_APPLE2 += src/apple2/barrage.cfg src/apple2/test/fixture.h
ifeq ($(PLATFORM),apple2)
  DISK_BASIC_SYSTEM =
endif
# MAIN ($4000-$BDFF) can't hold it all: start-up, the link and the clock
# below HGR page 1, the field's code and input in the language card
A2OBJ = build/$(PRODUCT)/apple2/src
$(A2OBJ)/main.o $(A2OBJ)/apple2/nstrans.o $(A2OBJ)/apple2/plat.o: CFLAGS += --code-name LOWCODE --rodata-name LOWDATA
$(A2OBJ)/apple2/input.o: CFLAGS += --code-name LC

r2r/apple2/$(PRODUCT): gen-apple2

gen-apple2: gen/config.h gen/apple2/fldtab.s gen/apple2/sfx.s gen/apple2/tunes.s

# the sound (src/apple2/mock.s): the Intellivision's effects at this clock,
# the game's jingles (the title is MENU's)
gen/apple2/sfx.s: tools/mksfx_a2.py src/intv/sfx.asm
	@mkdir -p $(dir $@)
	$(PYTHON) tools/mksfx_a2.py src/intv/sfx.asm $@

gen/apple2/tunes.s: tools/mkmus_a2.py tools/mkmus_intv.py tools/mus.py music/win.txt music/over.txt music/shop.txt
	@mkdir -p $(dir $@)
	$(PYTHON) tools/mkmus_a2.py $@ t_win=music/win.txt t_over=music/over.txt t_shop=music/shop.txt

# the field's tables: HGR's lines, the colour pixels' dots
gen/apple2/fldtab.s: tools/mktab_a2.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/mktab_a2.py $@

# MENU (-DMENU): the title, the name and the room list, as the Atari's
# MENU.COM; the same memory map (it chains, src/apple2/chain.s, through
# the same loader). BARRAGE boots; it runs MENU.SYSTEM when the appkeys
# name no room, and MENU runs BARRAGE.SYSTEM once one is picked.
MENU_SRC_APPLE2 = src/main.c src/menus.c src/appkey.c src/net.c src/cat.c \
	src/apple2/scr.c src/apple2/input.c src/apple2/plat.c src/apple2/nstrans.c \
	src/apple2/lowlevel.s src/apple2/chain.s src/apple2/pages.s src/apple2/txt.s \
	src/apple2/mock.s
MENU_DIR_APPLE2 = build/menu-apple2
MENU_BIN_APPLE2 = $(MENU_DIR_APPLE2)/menu.a2s
MENU_OBJ_APPLE2 = $(patsubst src/%,$(MENU_DIR_APPLE2)/%.o,$(MENU_SRC_APPLE2)) \
	$(MENU_DIR_APPLE2)/title.s.o $(MENU_DIR_APPLE2)/sfx.s.o
MENU_HDR_APPLE2 = $(wildcard src/*.h src/platform-specific/*.h) gen/config.h
MENU_LIB_APPLE2 = _cache/fujinet-lib/$(FUJINET_LIB)-apple2
MENU_CFLAGS_APPLE2 = -t apple2 -Os -Cl -O --cpu 6502 -DMENU -DUSING_FUJINET_LIB=1 \
	--include-dir src --include-dir src/platform-specific --include-dir gen --include-dir $(MENU_LIB_APPLE2)

$(MENU_DIR_APPLE2)/%.c.o: src/%.c $(MENU_HDR_APPLE2)
	@mkdir -p $(dir $@)
	cl65 $(MENU_CFLAGS_APPLE2) -c -o $@ $<

$(MENU_DIR_APPLE2)/%.s.o: src/%.s
	@mkdir -p $(dir $@)
	cl65 -t apple2 --asm-include-dir src --asm-include-dir src/6502 -c -o $@ $<

$(MENU_DIR_APPLE2)/title.s: tools/mkmus_a2.py tools/mkmus_intv.py tools/mus.py music/title.txt
	@mkdir -p $(dir $@)
	$(PYTHON) tools/mkmus_a2.py $@ t_title=music/title.txt

$(MENU_DIR_APPLE2)/title.s.o: $(MENU_DIR_APPLE2)/title.s
	cl65 -t apple2 -c -o $@ $<

$(MENU_DIR_APPLE2)/sfx.s.o: gen/apple2/sfx.s
	@mkdir -p $(dir $@)
	cl65 -t apple2 -c -o $@ $<

$(MENU_BIN_APPLE2): $(MENU_OBJ_APPLE2) src/apple2/barrage.cfg
	cl65 -t apple2 -C src/apple2/barrage.cfg -m $(MENU_DIR_APPLE2)/menu.map -o $@ $(MENU_OBJ_APPLE2) \
	  --lib-path $(MENU_LIB_APPLE2) fujinet-apple2-$(FUJINET_LIB).lib

# BARRAGE.SYSTEM and MENU.SYSTEM are our own loader: it skips HGR page 1's
# zeros in the file. (ProDOS boots the first .SYSTEM file: BARRAGE.SYSTEM
# goes back in the slot it had.)
apple2/disk-post:: build/a2loader.bin build/a2loader-menu.bin $(MENU_BIN_APPLE2)
	ac -d $(DISK) BARRAGE.SYSTEM
	ac -p $(DISK) BARRAGE.SYSTEM SYS 0x2000 < build/a2loader.bin
	ac -p $(DISK) MENU.SYSTEM SYS 0x2000 < build/a2loader-menu.bin
	ac -as $(DISK) MENU < $(MENU_BIN_APPLE2)

build/a2loader.bin: tools/a2loader/loader.s tools/a2loader/loader.cfg
	@mkdir -p build
	ca65 -o build/a2loader.o tools/a2loader/loader.s
	ld65 -C tools/a2loader/loader.cfg -o $@ build/a2loader.o

build/a2loader-menu.bin: tools/a2loader/loader.s tools/a2loader/loader.cfg
	@mkdir -p build
	ca65 -D MENU=1 -o build/a2loader-menu.o tools/a2loader/loader.s
	ld65 -C tools/a2loader/loader.cfg -o $@ build/a2loader-menu.o

fixture-apple2:
	cd server && go run ./cmd/refview -w 140 -h 160 -par 117 -buf 1536 -flat -shots 1 -seed 5 -weapon 3 \
	    -fixture ../src/apple2/test/fixture.h -out ../build/refview-apple2

.PHONY: gen-apple2 fixture-apple2

# ---- Intellivision (the FujiNet cartridge) -------------------------------
# not a MekkoGX platform: IntyBASIC and CP1610 assembly, src/intv/Makefile
intv:
	$(MAKE) -C src/intv $(if $(SERVER),SERVER=$(SERVER)) $(if $(TEST),TEST=$(TEST))

fixture-intv:
	cd server && go run ./cmd/refview -w 160 -h 80 -par 54 -buf 1024 -shots 1 -seed 5 -weapon 9 \
	    -fixture-bas ../src/intv/test/fixture.bas -out ../build/refview-intv

.PHONY: intv fixture-intv

# ---- Bally Astrocade (the FujiNet cartridge) -------------------------------
# Z80 assembly (zmac 1.3), not MekkoGX: src/astrocade/build.sh builds the
# APPBANK image and runs its checks
astrocade:
	$(if $(SERVER),DEFSRV=$(SERVER)) bash src/astrocade/build.sh

astrocade-rig:
	DEFSRV=127.0.0.1:9913 OUT=build/astrocade-rig/obj R2R=build/astrocade-rig bash src/astrocade/build.sh

fixture-astrocade:
	cd server && go run ./cmd/refview -w 160 -h 56 -par 54 -buf 240 -shots 1 -seed 5 -weapon 9 \
	    -fixture-asm ../src/astrocade/test/fixture.inc -out ../build/refview-astrocade

.PHONY: astrocade astrocade-rig fixture-astrocade

# ---- Emerson Arcadia 2001 (the FujiNet cartridge) ----------------------------
# Signetics 2650 assembly (Macroassembler AS), not MekkoGX: src/arcadia/build.sh
# builds the APPBANK image and runs its checks (call depth, clobbers, the RAM
# ledger)
arcadia:
	$(if $(SERVER),DEFSRV=$(SERVER)) bash src/arcadia/build.sh

arcadia-rig:
	DEFSRV=127.0.0.1:9913 OUT=build/arcadia-rig/obj R2R=build/arcadia-rig bash src/arcadia/build.sh

# the test scene's shot (tools/arc/refcheck.py swaps in each weapon's): the
# Arcadia's caps as the server projects its HELLO (-plat: 6 track slots)
fixture-arcadia:
	cd server && go run ./cmd/refview -w 48 -h 48 -par 78 -buf 160 -tankw 2 -tankh 1 -plat 11 \
	    -shots 1 -seed 5 -weapon 9 -fixture-asm ../src/arcadia/test/fixture.inc -out ../build/refview-arcadia

.PHONY: arcadia arcadia-rig fixture-arcadia

include mekkogx/toplevel-rules.mk

# ---- Heathkit H89: CP/M 2.2.04 or HDOS 2.0, the H19 terminal ----------------
# z88dk in defoogi (tools/h89/build.mk, the whole make there): one program with
# the menu and the game, the H19's graphics characters for the field (a cell
# two blocks by two), fujinet-lib built with zsdcc (tools/z80/mkfnlib.sh). The
# disks: r2r/h89-cpm/barrage.h8d, r2r/h89-hdos/barrage.h8d (overlays); h89-boot a bootable
# CP/M disk (needs fujinet-go-h89-desktop built, tools/h89/mkboot.sh).
FNLIB_H89 ?= $(HOME)/Workspace/fujinet-lib
H89_ARGS = $(if $(ZOPT),ZOPT=$(ZOPT)) $(if $(TEST),TEST=$(TEST)) $(if $(TEST_SCENE),TEST_SCENE=$(TEST_SCENE) TEST_ARG=$(TEST_ARG)) \
    $(if $(XFLAGS),XFLAGS="$(XFLAGS)") $(if $(H89_RIG_KEYS),H89_RIG_KEYS=$(H89_RIG_KEYS)) $(if $(H89_PROF),H89_PROF=$(H89_PROF))
h89-cpm: gen/config.h build/fnlib-h89/fujinet-h89.lib
	tools/z80/z88dk $(MAKE) -f tools/h89/build.mk OS=cpm $(H89_ARGS)
h89-hdos: gen/config.h build/fnlib-h89/fujinet-h89.lib
	tools/z80/z88dk $(MAKE) -f tools/h89/build.mk OS=hdos $(H89_ARGS)
h89-boot: h89-cpm
	tools/h89/mkboot.sh r2r/h89-cpm/barrage.com r2r/h89-cpm/barrage-boot.h8d
build/fnlib-h89/fujinet-h89.lib: tools/z80/mkfnlib.sh $(wildcard $(FNLIB_H89)/common/*.c $(FNLIB_H89)/bus/h89/* $(FNLIB_H89)/include/*.h)
	tools/z80/mkfnlib.sh $(FNLIB_H89) build/fnlib-h89 h89
.PHONY: h89-cpm h89-hdos h89-boot

# ---- Sega Master System (the FujiNet SMS cartridge) ---------------------------
# z88dk in defoogi (tools/sms/build.mk, the whole make there): one program
# (the menu and the game) in a Sega-mapper image, Mode 4 (src/sms/), the cart's
# 32K of RAM for the field's store and the tile cache's tables, the SN76489
# and, when there is one, the YM2413. fujinet-lib is
# fujinet-lib-experimental's add-sms branch (FNLIB_SMS, a worktree) built
# with zsdcc -SO2 (tools/z80/mkfnlib.sh).
FNLIB_SMS ?= $(HOME)/Workspace/fnlib-sms
SMS_ARGS = $(if $(TEST),TEST=$(TEST)) $(if $(TEST_SCENE),TEST_SCENE=$(TEST_SCENE) TEST_ARG=$(TEST_ARG)) \
    $(if $(VIDEO_CARD),VIDEO_CARD=$(VIDEO_CARD)) $(if $(TESTX),TESTX="$(TESTX)") $(if $(XFLAGS),XFLAGS="$(XFLAGS)")
sms: gen/config.h build/fnlib-sms/fujinet-sms.lib
	tools/z80/z88dk $(MAKE) -f tools/sms/build.mk $(SMS_ARGS)
build/fnlib-sms/fujinet-sms.lib: tools/z80/mkfnlib.sh $(wildcard $(FNLIB_SMS)/common/*.c $(FNLIB_SMS)/bus/sms/* $(FNLIB_SMS)/include/*.h)
	ZOPT=-SO2 tools/z80/mkfnlib.sh $(FNLIB_SMS) build/fnlib-sms sms
.PHONY: sms
