# FujiBarrage

FujiBarrage is an artillery game for two to ten tanks over FujiNet. Seats with
no player get bots. It runs on the Atari 400/800/XL/XE with 48K, on the Apple
//e, //c and IIgs with 128K, on the Color Computer 1 and 2 with 64K and the
Color Computer 3 (one disk for all three), on the Coleco Adam, and on the
Atari 5200, the Intellivision (NTSC or PAL) and the Bally Astrocade with
the FujiNet cartridge; other FujiNet machines are to follow (see
`docs/PORTING.md`).

The server plays the whole game. A client only shows it: the server sends each
one its field and every shot already worked out at that machine's resolution
and frame rate, so every screen agrees on every hit.

FujiBarrage is an original game in the tradition of the classic artillery
games. Its names, art, music and phrases are its own.

## How to play

Take turns aiming and firing until one tank is left; the last tank standing
wins the round. Money comes from damage and kills. Spend it in the shop
between rounds on 33 weapons (missiles, nukes, MIRVs, napalm, rollers, drills,
moles, dirt, lasers) and on items (shields, deflectors, parachutes, fuel,
guidance, batteries, a defence computer).

On the Atari:

| | |
|---|---|
| Stick left / right | Angle (hold to go faster) |
| Stick up / down | Power |
| Fire | Shoot |
| SELECT, W or TAB | Next weapon |
| OPTION or I | The items line. The stick picks an item and Fire uses it. Guidance asks for a target (stick, then Fire). Fuel drives the tank with the stick. |
| Return | Done (shop, defence window) |
| Esc | Close the items line; otherwise leave the room |

On the Apple II:

| | |
|---|---|
| Arrows left / right, or the joystick | Angle (hold to go faster) |
| Arrows up / down, or the joystick | Power |
| Space, Open-Apple or Solid-Apple (the joystick's buttons) | Shoot |
| W or TAB | Next weapon |
| I | The items line, as the Atari's: the arrows pick, Space uses |
| Return | Done (shop, defence window) |
| Esc | Close the items line; otherwise leave the room |

On the Color Computer:

| | |
|---|---|
| Arrows left / right, or the right joystick | Angle (hold to go faster) |
| Arrows up / down, or the right joystick | Power |
| SPACE or the joystick's button | Shoot |
| W (or SHIFT-RIGHT) | Next weapon |
| I | The items line, as the Atari's: the arrows pick, SPACE uses |
| ENTER | Done (shop, defence window) |
| BREAK | Close the items line; otherwise leave the room |
| CLEAR | The colours: the CoCo 1/2's artifact colours the other way round, the CoCo 3's RGB or composite palette |
| SHIFT-CLEAR | Sound off and on |

On the Coleco Adam:

| | |
|---|---|
| Arrows left / right, or the joystick | Angle (hold to go faster) |
| Arrows up / down, or the joystick | Power |
| SPACE or either fire button | Shoot |
| W, TAB, SmartKey II or keypad 0 | Next weapon |
| I, SmartKey I or keypad 1 | The items line, as the Atari's: the arrows pick, SPACE uses |
| RETURN or keypad # | Done (shop, defence window) |
| ESCAPE or keypad * | Close the items line; otherwise leave the room |

On the Atari 5200:

| | |
|---|---|
| Stick left / right | Angle (hold to go faster) |
| Stick up / down | Power |
| Top button | Shoot |
| Lower button | Next weapon |
| # | The items line, as the Atari's: the stick picks, the top button uses |
| START | Done (shop, defence window) |
| * | Close the items line; otherwise leave the room |
| PAUSE | Sound off and on |

On the Intellivision:

| | |
|---|---|
| Disc left / right | Angle (hold to go faster) |
| Disc up / down | Power |
| Top buttons | Shoot |
| Lower left button | Next weapon |
| Lower right button | The items line, as the Atari's: the disc picks, a top button uses |
| ENTER | Done (shop, defence window) |
| CLEAR | Close the items line; otherwise leave the room |
| 0 | Sound off and on |

On the Bally Astrocade:

| | |
|---|---|
| Stick left / right | Angle (hold to go faster) |
| Stick up / down | Power |
| Trigger | Shoot |
| + | Next weapon |
| × | The items line, as the Atari's: the stick picks, the trigger uses |
| = | Done (shop, defence window) |
| C | Close the items line; otherwise leave the room |
| MR | Sound off and on |

- **Shop:** the stick (disc) moves up and down, Fire (a top button) buys, Left (the lower left button) sells, Return (ENTER) is done.
- **Waiting room:** the host (the first person in) presses Fire to start, and bots fill the empty seats. S (the lower right button) shows the settings and TAB (lower left) turns their pages. The host changes a setting with the stick: up and down pick it, left and right change it.
- **The Intellivision's name:** the disc's up and down pick a letter, right or a top button takes it, left or CLEAR takes one back, the keypad's digits type themselves, and ENTER is done.
- **The Apple II:** the screens are the Atari's, in text; the name is typed, and the left arrow or DELETE takes a letter back.
- **The Color Computer:** the screens are the Atari's: 32 columns in colour on the CoCo 1/2, 40 on the CoCo 3; W (or SHIFT-RIGHT) turns the settings' pages; the name is typed, and LEFT takes a letter back. At power-on the CoCo 1/2 asks whether the screen is blue (which way round its artifact colours are) and the CoCo 3 whether the monitor is RGB or composite; CLEAR changes either later.
- **The 5200:** the screens are the Atari's; 0 shows the waiting room's settings and the lower button turns their pages. With no name from the lobby, the name comes from a wheel: the stick's up and down pick a letter, right or the top button takes it, left takes one back, the keypad's digits type themselves, START is done.
- **The Adam:** the screens are the Atari's, 32 columns in colour; S (SmartKey III, keypad 2) shows the waiting room's settings and TAB turns their pages; the name is typed, and BACKSPACE takes a letter back.
- **The Astrocade:** the shop and waiting room are the Atari's, with the trigger for Fire and = for Return; ÷ shows the settings and + turns their pages. The name: the stick's up and down pick a letter, right or the trigger takes it, left, C or CE takes one back, = is done.

The host's settings:

| Page | Settings |
|---|---|
| Field | Sky, bumpiness, slopes, flattened peaks, random land, wind (most, and whether it changes), gravity, air thickness, how much dirt stays up, tanks fall, falls hurt, tunnelling, blast size |
| Walls | The walls (none, wrap, padded, rubber, spring, concrete, random, erratic) and how far the edges extend |
| Game | Tanks (2–10, default 10), rounds (1–100, default 10), play (take turns, all at once, or aim together), turn order and timer, talking tanks and how often, arms level, starting cash, interest, whether bots shop |

The server's six rooms are OPEN RANGE, BOUNCE HOUSE (rubber walls),
WRAPAROUND, MAYHEM (erratic walls and changing wind, all at once), and two
for people only: PEOPLE ONLY (OPEN RANGE's settings) and PEOPLE MAYHEM
(MAYHEM's). Those never have bots: a game needs two people to start, a
person who leaves loses their tank, and a game left with one person ends.

Each round opens with a 30-second defence window if anyone has a defence
computer; its items line opens by itself with the defences only (shields,
parachute, battery, impact fuse). Your turn flashes YOUR TURN!, marks your
tank, and plays its fanfare again after 10 seconds without a press.

## Layout

| Path | What |
|---|---|
| `server/` | `barraged`, the game server (Go), and `refview`, the reference client: it draws what a client of any shape should show, and writes the test scenes' fixtures. |
| `server/internal/` | `sim` the physics and every weapon; `project` each client's view (columns, scripts); `game` the rules and rounds; `bot` the eight bot tiers; `talk` the tanks' phrases; `server` the rooms and links; `view` the reference player. |
| `src/*.c` | The client, platform neutral: menus, rooms, shop, play, the link. |
| `src/platform-specific/*.h` | The API each platform implements. |
| `src/atari/` | The Atari implementation: cc65, ANTIC mode E with compiled DLIs, players and missiles behind the tanks, POKEY, netstream. The script player, the field primitives and the display list compiler are in assembly. |
| `src/6502/` | The script player every 6502 port shares (`ply.s`, `ops.s`, `trk.s`): it draws through the port's own `fld.s`. |
| `src/apple2/` | The Apple II client: cc65, ProDOS. High-res page 1 is the field itself: a colour pixel is two dots, and the game's four values are HGR's four colours in a palette (tank black, sky blue or violet, dirt orange or green, fire white), so `fld.s` draws the field straight into the screen. Mixed mode's four text rows are the HUD; the other screens are 40x24 text. The tanks are black, yours with a pennant, the turn's under a blinking arrow. Sound is a Mockingboard's AY (the Intellivision's effects at this clock), or a few clicks of the speaker. |
| `src/intv/` | The Intellivision client, for the FujiNet cartridge. There is no C compiler for the CP1610, so it is `src/*.c` again in IntyBASIC 1.5, with the field, the script player, the GRAM card pool, the tanks and the sound effects in CP1610 assembly. The field is 160x80 in two bit planes in the cartridge's 16-bit RAM; each 8x8 cell that changes gets a GRAM card from a shared pool; the tanks are MOBs (past eight, the extra ones blink). |
| `gfx/intv/` | The Intellivision's cards, drawn as text. |
| `src/coco/` | The Color Computer client, both machines: CMOC and lwtools, the FujiNet over DriveWire (the Disk ROM's, HDB-DOS), the menu and the game as two programs each (`chain.asm`), the timer, the keyboard, the sound's control (`sound.c`: the Apple's Mockingboard driver over three voices) and the tanks and balloons (`field.c`). |
| `src/coco2/` | The CoCo 1/2: PMODE 4's artifact colours are the field's four values (tank black, sky blue, dirt orange, fire white), 128x168 at $1000, drawn by `fld2.asm`; the text, HUD included, is 32 columns of a 3x7 font in colour. Its sound is three voices mixed by the CPU while it would otherwise wait. |
| `src/coco3/` | The CoCo 3: the GIME's 320x200 in 16 colours, the field 320x176 (`fld3.asm`): each value a colour pattern a line (sky gradients by dither, stars, dirt strata), fire cycled through the palette, ten tanks in their seats' colours, RGB and composite palettes; 40-column text in an 8x8 font. Its sound is three voices the GIME timer's IRQ makes. |
| `src/6809/` | The script player both Color Computers share (`ply.asm`, `ops.asm`, `trk.asm`, `pt.asm`: the 6502's, with 16-bit x), drawing through the machine's own `fld*.asm`. |
| `src/atari5200/` | The Atari 5200 client, for the FujiNet cartridge: `src/atari` assembled for the 5200 (its chips' addresses and the graphics in 16K of RAM: `src/atari/hw.inc`, `atari.h`), with the 5200's own start-up (`crt0.s`: the BIOS's vectors, the keypad's interrupt), controller, the cartridge's mailbox as the link (`nstrans.c`) and banks (`bank.s`). A 128K Super Cart: the menu in bank 0, the game in banks 1-3, its core in every bank and the field's, the rooms' and the boot code's overlays in a bank each, called through far wrappers (`tools/atari5200/far.txt`). The catalog is in ROM (16K has no room for it). |
| `src/adam/` | The Coleco Adam client: z88dk (zsdcc and z80asm), EOS. The menu and the game are two programs in one DDP (`chain.asm` loads either from the device it booted from), plus a block of art (the sprites and the font) loaded into the VRAM at start-up. The VDP's interrupt (an NMI) is the clock; the FujiNet is AdamNet DCBs, read without waiting (`nstrans.c`). |
| `src/tms9918/` | The TMS9918A's field, text and title, all assembly, for the Adam now and the ColecoVision and the MSX to come: Graphics II, the field 256x168 kept as two exact bit planes (`fld.asm`) and drawn a changed 8x8 cell at a time (`render.asm`), each line's colours from the round's sky and dirt (solid bands, no dither); the tanks are sprites in their seats' colours, and a tank that would be a fifth on a line is drawn into the cells instead (`field.asm`). |
| `src/z80/` | The script player in Z80 (`ply.asm`, `ops.asm`, `trk.asm`, `pt.asm`: the 6809's), drawing through `src/tms9918/fld.asm`. |
| `src/sound/`, `src/sn76489/` | The Z80 machines' sound: a chip-neutral engine (`snd.asm`: effects, tunes, the shell's whistle, as three voices) and its SN76489 back end (`sn.asm`: the Adam's and the ColecoVision's). |
| `src/astrocade/` | The Bally Astrocade client, for the FujiNet cartridge: Z80 assembly (zmac), an APPBANK image of 4K pages. The field is the screen's own 160x56 bitmap, as on the Atari; a band interrupt every 4 lines gives each band its sky, two tank colours and the split between them (past two tanks in a band, the extra ones shimmer at 30 Hz). The 4K of screen RAM is all the RAM there is. |
| `music/` | The tunes, in FujiBlast's text format (`tools/mus.py`). |
| `docs/PROTOCOL.md` | The wire protocol, the state layout and the script ops. |
| `docs/OPS.md` | How every weapon and item reaches the client as script ops. |
| `docs/PORTING.md` | How to bring FujiBarrage to another machine. |
| `tools/` | The font, art and music converters; the Atari rigs (`atarisim/`); the Apple II's (`a2sim/`, `a2rig/`); the Intellivision's (`intvsim/`, `intvrig/`); the Color Computer's (`cocosim/`); the Adam's (`adamsim/`, `adamrig/`); the 5200's (`a52sim/`, `atari5200/`). |

## Building

```
defoogi make atari      # r2r/atari/barrage.atr
defoogi make apple2     # r2r/apple2/barrage.po
make server             # server/barraged, server/refview
make proto              # src/proto.h (and src/6502/proto.inc, src/6809/proto.inc) from the server's constants
make test               # the server's tests
make intv               # r2r/intv/barrage.rom
make astrocade          # r2r/astrocade/barrage.bin
make coco               # r2r/coco/barrage.dsk
make adam               # r2r/adam/barrage.ddp (and barrage.dsk)
make atari5200          # r2r/atari5200/barrage.a52
```

- **Default server:** `make SERVER=host:port` sets the server the game uses when the lobby gave none. It is remembered.
- **The disk:** single density, picoboot, no DOS. It holds two programs:
  - `BARRAGE.COM` is the game. It boots, reads the FujiNet appkeys (creator 1, app 0x13: the name and the room), and plays.
  - `MENU.COM` is the title, the name entry and the room list. The game loads it from the disk itself when the appkeys don't say who is playing where. The menu saves the keys and reboots into the game. Leaving a room clears the room key and reboots back to the menu.
- **Memory:** the game fills 48K. `src/atari/barrage.cfg` says what goes where, and the link fails when it doesn't fit.
- **The Apple II's disk:** a 140K ProDOS disk holding two programs, as the Atari's: `BARRAGE` (the game) and `MENU` (the title, the name and the rooms), each loaded by its own `.SYSTEM` loader (`tools/a2loader`), which skips the 8K of high-res page 1 in the file. Either runs the other by loading its loader (`src/apple2/chain.s`). It needs a 128K //e, //c or IIgs; `/RAM` is unplugged. The clock is a Mockingboard's timer if there is one, else the vertical blank. `src/apple2/barrage.cfg` says what goes where: code below the field, above it to $BDFF, and in the language card. `SERVER=host:port` works as the Atari's.
- **The Astrocade's image:** `r2r/astrocade/barrage.bin`, 52K for the FujiNet cartridge's APPBANK: 8K at $2000 (page 0, then the resident code at $3000), then 4K pages that the code maps at $2000 one at a time. `src/astrocade/build.sh` needs zmac 1.3's source (`~/Workspace/zmac-1.3`, or `ZMACSRC=`) and builds it with a bigger symbol table. It writes the catalog into the image from `barraged -catalog`. Its checks fail a build that overruns a page, the resident code or RAM, or jumps between pages without `FAR`. `SERVER=host:port` works as the Atari's.
- **The Color Computer's disk:** `r2r/coco/barrage.dsk`, a Disk BASIC disk for both machines: `BARRAGE.BIN` (`src/coco/launch.asm`) loads the CoCo 3's game on a CoCo 3, else the CoCo 1/2's; the lobby types `RUNM"BARRAGE"` (the disk's name) and `AUTOEXEC.BAS` LOADMs it. Each machine has a game (`BARRAG2`, `BARRAGE3`) and a menu (`BMENU`, `BMENU3`), as the Atari's two programs, and each program is a low core LOADM'd by BASIC and the files its `bank.c` loads after it into all-RAM (the high core at $8000, the art; the CoCo 3's art in a block of its own). The FujiNet boots it over DriveWire (HDB-DOS). `tools/coco/build.mk` needs CMOC 0.1.101, lwtools and toolshed's `decb` (`~/Workspace/toolshed`); `src/coco/barrage{,3}.link` say what goes where, and `tools/coco/lwlink-co.py` fails a link that overruns a region. `SERVER=host:port` works as the Atari's.
- **The 5200's cartridge:** `r2r/atari5200/barrage.a52`, a 128K Super Cart (the claim's kind 4) for the FujiNet 5200 cartridge. Bank 0 is the menu (`src/atari5200/menu-a52.cfg`), banks 1-3 the game (`barrage-a52.cfg`); the console starts in bank 3, and each program starts the other as the Atari's two programs do. `tools/atari5200/build.mk` runs cc65 in defoogi; fujinet-lib is fujinet-lib-experimental's add-atari5200 branch (`FNLIB_5200`, default `~/Workspace/fnlib-5200`), built on the host (`tools/atari5200/mkfnlib.sh`). The catalog comes from the server's sources (`barraged -catalog`, `tools/atari5200/mkcat.py`): a server whose catalog has changed says UPDATE FUJIBARRAGE until the image is built again. `tools/atari5200/farcheck.py` fails a build that calls into another bank's overlay without a far wrapper, and `mkrom.py` one that writes into the cartridge's window, reads the mailbox or the bank switch outside their code, or overruns a bank. `SERVER=host:port` works as the Atari's.
- **The Adam's image:** `r2r/adam/barrage.ddp`, a 256K tape image (the FujiNet boots it), and the same blocks as `barrage.dsk`, a 160K disk. Block 0 is z88dk's bootstrap; then the game, the menu and the art block. The game boots; the menu, the art and the game again are read with EOS's READ_BLOCK from the device it booted from. `tools/adam/build.mk` runs z88dk in the defoogi container (`tools/z80/z88dk`), at `-SO2` (`-SO3` miscompiles), and builds its fujinet-lib. `tools/adam/mkimage.py` writes the blocks and fails a build whose program overruns C800h (the bootstrap) or whose variables reach the field's store at A700h. `SERVER=host:port` works as the Atari's.
- **The Intellivision's ROM:** `r2r/intv/barrage.rom`, for the FujiNet cartridge, which gives it 7K words of 16-bit RAM at $8000-$9BFF. It needs `as1600` (jzIntv's SDK) on the PATH and IntyBASIC 1.5's source (`IB_SRC`, default `~/Workspace/IntyBASIC`): the Makefile builds the compiler, since 1.4 has no `OPTION MAP`. The ROM is about 24K words of the 26.5K in the windows no other hardware uses; `tools/intv/checkcfg.py` fails a build that strays outside them. `SERVER=host:port` works as the Atari's.

## Running a server

```
server/barraged -stream :9913
```

- **Rooms:** `barraged` serves `room1`–`room4`, plus the hidden `dev1`–`dev3`, whose settings `-devset` (or `BARRAGE_DEVSET`) changes for tests, e.g. `tanks=3,rounds=2,timer=10,tier=0`.
- **Phrases:** `BARRAGE_TALK_FIRE` and `BARRAGE_TALK_LAST` name phrase files of your own: a line each, 40 characters at most.
- **Lobby:** it lists itself in the FujiNet lobby (key 0x13) only when `BARRAGE_LOBBY` is `qa`, `prod` or a URL. `BARRAGE_SERVER_URL` and `BARRAGE_CLIENT_ATARI` (`BARRAGE_CLIENT_INTV`, and so on) then say what it lists.
- **systemd:** `server/barraged.service` is a unit for it.

## Testing

```
cd server && go test ./...
make -C tools/atarisim
make atari TEST=1       # with the offline test scenes (rm -rf build/barrage first)
python3 tools/atarisim/atarisim.py r2r/atari/barrage.atr build/barrage-atari.map SCENARIO OUTDIR
tools/atarisim/refcheck.py OUTDIR WEAPON...
tools/atarisim/rig.sh SCRIPT OUTDIR
tools/atarisim/rig800.sh SECONDS OUTDIR [STICKS]
make apple2 TEST=1      # (rm -rf build/barrage/apple2 first)
python3 tools/a2sim/a2sim.py r2r/apple2/barrage.po build/barrage-apple2.map SCENARIO OUTDIR
tools/a2sim/refcheck.py [-gate] OUTDIR [WEAPON...]
tools/a2rig/rig.sh SCRIPT OUTDIR
make intv TEST=1
python3 tools/intvsim/intvsim.py r2r/intv/barrage.rom SCENARIO OUTDIR
tools/intvsim/refcheck.py OUTDIR WEAPON...
make -C src/intv rig    # build/intv-rig/barrage.rom: its room list the rig's server
tools/intvrig/rig.sh SCENARIO OUTDIR
python3 tools/astrosim/astrosim.py SCENARIO OUTDIR
tools/astrosim/refcheck.py OUTDIR WEAPON...
tools/astrosim/sndwav.py OUTDIR [SOUND...]
make astrocade-rig      # build/astrocade-rig/barrage.bin: its room list the rig's server
tools/astrorig/rig.sh SCENARIO OUTDIR
make coco TEST=1
python3 tools/cocosim/cocosim.py r2r/coco/barrage.dsk build/barrage-game2.map SCENARIO OUTDIR
tools/cocosim/refcheck.py coco2|coco3 [-gate] OUTDIR [WEAPON...]
tools/cocosim/terrcheck.py --fixture OUTDIR
tools/cocosim/rig.sh SCRIPT OUTDIR
tools/cocosim/mixed.sh SCRIPT2 SCRIPT3 OUTDIR
make atari5200 TEST=1
python3 tools/a52sim/a52sim.py r2r/atari5200/barrage.a52 build/a52/game.map SCENARIO OUTDIR
tools/a52sim/refcheck.py [-gate] [-j N] OUTDIR [WEAPON...]
tools/a52sim/rig.sh SCENARIO OUTDIR
make adam TEST=1
tools/adamsim/adamrun r2r/adam/barrage.ddp build/adam/game.map SCENARIO OUTDIR
tools/adamsim/refcheck.py [-gate] OUTDIR [WEAPON...]
tools/adamsim/terrcheck.py OUTDIR
tools/adamsim/vramcheck.py RAM.bin VRAM.bin
tools/adamrig/rig.sh OUTDIR
```

- **`go test`** covers:
  - the physics and every weapon, item and wall;
  - each client shape's columns and scripts, through `refview`'s player, with golden pictures and a fuzzer;
  - the rules, the shop and the rounds;
  - ten-bot games in every mode and with every wall;
  - the bots' tiers.
- **`atarisim.py`** runs the offline test scenes from `src/test.c` in libatari800: the field, a shot, the rooms, the results and the tunes. It takes screenshots, sound and POKEY logs. `PAL=1` makes the Atari a PAL machine.
- **`refcheck.py`** plays one shot per weapon on the Atari and compares its field, pixel for pixel, with `refview`'s.
- **`rig.sh`** plays live in a private network namespace: the server, bots, a copy of fujinet-pc-atari, and asim over NetSIO.
- **`rig800.sh`** does the same with the standalone atari800, whose pacing and interrupt timing are closer to a real machine's.
  - It runs NTSC unless `PAL=1`.
  - It runs atari800 with `-nojoystick`: SDL reads this machine's controllers even headless, and a controller's Y button is atari800's warm start.
- **`a2sim.py`** runs the Apple II's test scenes in MAME's enhanced //e (`MACHINE=apple2c`, `NOMOCK=1` for no Mockingboard): screenshots, memory dumps (`hgrfield.py` decodes high-res page 1 into the field's values), CPU traces (`prof.py`), sound (`A2WAV=`, cut by `sndwav.py`). Scene 7 plays every effect.
- **`tools/a2sim/refcheck.py`** is `refcheck.py` for the Apple II, with each shot's lag; `-gate` runs every weapon on hilly and flat fields and with shields (99 shots), all at 0 pixels.
- **`tools/a2rig/rig.sh`** plays the Apple II live: AppleWin's core (from fujinet-go-apple2-desktop) with its FujiNet on SmartPort, the server and bots in a network namespace; `NOMOCK=1` takes the Mockingboard out. `EXTRA` and `ROOM` put the proxies in the link; `terrcheck.py` holds the field against the server's terrain from `tapproxy.py`'s log. The menu's room list needs a build with `SERVER=127.0.0.1:9913`.
- **`intvsim.py`** runs an Intellivision ROM in jzIntv's debugger, a frame at a time: the controller, screenshots and the screen as text, sound, the stack's high water mark and CPU profiles. `PAL=1` makes it a PAL console. The test scenes (`src/intv/test.bas`) are the field, a shot, the rooms, the results, game over, every effect and the tunes.
- **`tools/intvsim/refcheck.py`** is `refcheck.py` for the Intellivision. Every weapon comes out at 0 pixels, on hilly and flat fields and with shields.
- **`astrosim.py`** runs the Astrocade image in MAME's `astrocde` with the FujiNet cartridge, headless: the controls, screenshots and the screen as text, RAM peeks and watches, CPU traces (`traceprof.py`), sound. Its test scenes (`src/astrocade/src/p_test.inc`) are the raster, a sound, and a whole shot from `make fixture-astrocade`.
- **`tools/astrosim/refcheck.py`** is `refcheck.py` for the Astrocade, and also says how many frames each shot runs behind its script. Every weapon comes out at 0 pixels, on hilly and flat fields and with shields.
- **`tools/astrosim/sndwav.py`** writes every effect and tune as a WAV.
- **`tools/astrorig/rig.sh`** plays the Astrocade live, with a copy of fujinet-pc on BoIP; `EXTRA` and `ROOM` put `cutproxy.py`, `dropproxy.py`, `stallproxy.py` (which holds what the client sends, so the server gives up on a shot) or `tapproxy.py` (which logs what the server sends) in the link.
- **`cocosim.py`** runs the Color Computer's test scenes in MAME, headless: the CoCo 2B by default, `MACHINE=coco3` (or `coco3p`, a PAL one). Screenshots, memory and screen-block dumps (`cocofield.py` decodes the field), CPU traces (`prof.py`), sound (`COCOWAV=`). Scene 7 plays every effect, 6 the tunes.
- **`tools/cocosim/refcheck.py`** is `refcheck.py` for either machine, with each shot's lag; `-gate` runs every weapon on hilly and flat fields and with shields (99 shots), all at 0 pixels on both. The slowest shot (MEGA MIRV on a flat field) ends about 0.8 s behind its script on the CoCo 1/2 and 2.2 s on the CoCo 3.
- **`tools/cocosim/terrcheck.py`** holds the field against its terrain: `--fixture` offline, `--live` against `tapproxy.py`'s log.
- **`tools/cocosim/rig.sh`** plays a Color Computer live: MAME with HDB-DOS on the Becker port, a copy of fujinet-pc (the CoCo build) and the server and bots in a network namespace; `MACHINE=coco3` for the CoCo 3, `EXTRA` and `ROOM` as the Apple's. **`mixed.sh`** puts a CoCo 1/2 and a CoCo 3 in one room (MAME's Becker port can't move from 65504, so the CoCo 3 gets a namespace of its own inside the first).
- **`a52sim.py`** runs the 5200 image in MAME's 5200 with the FujiNet cartridge (`~/Workspace/mame-a5200`), headless: the test scenes, the test hook's input or the real controller (`pad`), screenshots, memory dumps, a store's PC (`wpc`), traces, sound (`A52WAV=`). `shot.sh` and `probe.sh` take a picture or the PC as it boots.
- **`tools/a52sim/refcheck.py`** is `refcheck.py` for the 5200, with each shot's lag; `-gate` runs every weapon on hilly and flat fields and with shields (99 shots), all at 0 pixels. The slowest shot (MEGA MIRV on a flat field) ends about 3.1 s behind its script.
- **`tools/a52sim/rig.sh`** plays the 5200 live: MAME with a copy of fujinet-pc (the rs232 build: the cartridge's bus over BoIP), the server and bots in a network namespace; `EXTRA` and `ROOM` as the Apple's, `A52WAV` records the sound.
- **`adamrun`** runs the Adam's image in adamcore (FujiNet Go Adam's core, as a library) with no FujiNet, as fast as the host goes: the test scenes, RAM and VRAM dumps, screenshots, sound (`wav`), CPU profiles (`hist`, `prof`), and a shot's segments served from files. Build it with `gcc -O2 -o tools/adamsim/adamrun tools/adamsim/adamrun.c -I$ADAMCORE/include $ADAMCORE/build/libadamcore.a -lm -lpthread`. Scene 7 plays every effect, 6 the tunes, 9 the whistle.
- **`tools/adamsim/refcheck.py`** is `refcheck.py` for the Adam, with each shot's lag; `-gate` runs every weapon on hilly and flat fields and with shields (99 shots), all at 0 pixels. The slowest shot (MEGA MIRV on a flat field) ends about 1.7 s behind its script.
- **`tools/adamsim/terrcheck.py`** holds the field's store against its terrain (`--live` against `tapproxy.py`'s log), and **`vramcheck.py`** holds the screen against the store: every field cell's pattern and colours as `render.asm` should make them.
- **`tools/adamrig/rig.sh`** plays the Adam live: adamcore with a copy of fujinet-pc (the Adam build) over AdamNet, the server and bots in a network namespace. `KEYS` types at given seconds, `WAV` records the sound, `CONFIG=1` boots through CONFIG as a real FujiNet would, and `EXTRA` and `ROOM` put the proxies in the link.
- **`tools/intvrig/rig.sh`** plays the Intellivision live, as `rig.sh` does the Atari, with a copy of fujinet-pc on BoIP and the FujiNet build of jzIntv. `EXTRA` runs `tools/rigs/cutproxy.py` or `dropproxy.py` in the namespace to drop the link.
