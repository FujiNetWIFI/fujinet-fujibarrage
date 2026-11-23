# Porting FujiBarrage

The Atari 400/800 is the first client, and the Intellivision the second;
the Apple II, the Astrocade and the Color Computers followed. These notes say
what another machine needs, and what the ports taught.

## What the server does for you

All of the game runs on the server: the physics, every weapon, the dirt, the
falls, the damage, the bots and the rules. A client never decides anything; it
shows what it is sent, so a port is a screen, a sound chip and a link.

The server shapes everything for the client that asks. It holds the world at
640x400 square units, and its HELLO tells the server:

| HELLO byte | What | The Atari's | The Intellivision's |
|---|---|---|---|
| 18-19 | field width, pixels (16-1024) | 160 | 160 |
| 20 | field height, pixels (16-240) | 168 | 80 |
| 21 | pixel aspect, 64ths | 109 | 54 |
| 22 | frames a second, 50 or 60 (`plat_hz`) | 60, or 50 on PAL | 60, or 50 on PAL |
| 23-24 | a tank's width and height, pixels | 7, 5 | 7, 5 |
| 25-26 | the script buffer, bytes (128 or more) | 1536 | 1024 |
| 27 | colour bands | 4 | 8 |
| 28 | capabilities (`CAP_HOLD`) | `CAP_HOLD` | none |
| 29 | platform (`PLAT_*`) | `PLAT_ATARI` | `PLAT_INTV` |

From these the server sends:
- **The field:** TERRAIN columns at your resolution. A column is a list of dirt spans.
- **Every shot:** a SCRIPT in your pixels and your frames, cut into segments that fit your buffer.
- **The state:** STATE patches.

Two clients of different shapes see the same game, and the same hits.
`docs/PROTOCOL.md` is the whole of it.

A new platform needs a `PLAT_*` number in `server/internal/proto/proto.go`
(then `make proto`) and, for the lobby, a line in
`server/cmd/barraged/main.go`'s platform list (`BARRAGE_CLIENT_<NAME>`).

## What you write

`src/*.c` is the client, platform neutral: the menus, the rooms, the shop,
the field's HUD, the link's protocol and the test scenes. Each platform
implements `src/platform-specific/*.h` in `src/<platform>/`:

| Header | What |
|---|---|
| `plat.h` | Start-up, the frame clock (`frame_count` in 60ths, `vbi_count` in display frames), random numbers, rebooting |
| `input.h` | The stick, the button and keys, latched as events; `inp_hook*` lets the rigs drive it |
| `nstrans.h` | The link: open, frames in (COBS + Fletcher-16), bytes out, and hold/wake for a script |
| `scr.h` | A text screen of 24x40 with a palette per row (the menus, the rooms, the shop, the results); `SCR_COLS` 32 for a narrower one, whose columns and words are `src/layout.h`'s |
| `field.h` | The playfield: a bitmap of four pixel values (`PX_SKY`, `PX_DIRT`, `PX_FX`, `PX_TANK`), the tanks, the status rows, the balloon, the sparks, the tracks (`trk_*`). Its sizes (`FIELD_W` and so on) are per platform, under `#ifdef`. |
| `sound.h` | Cues by meaning (`SND_*`, `UI_*`), the falling shell's whistle, the talk blip, two-voice tunes |

**The script player.**
- **Who calls it:** `play.c` drives the player through `ply_reset`, `ply_step`, `ply_draw` and `ply_end`.
- **The Atari's:** assembly (`src/6502/ply.s`, `ops.s`, `trk.s`, shared by every 6502 port: each brings its own `fld.s`). It calls back into `play.c` for what C does better: `c_op` handles TALK, SOUND, SHIELD, DAMAGE, FALL, DEATH, TANK, FLASH and FIRE, and `c_anim` handles the talk chatter.
- **For a new machine:** write one from `docs/PROTOCOL.md`, or start from `server/internal/view/view.go`, the reference player in Go. It follows PROTOCOL.md to the letter, and `refview` draws with it.
- **Its declarations:** `src/platform-specific/ply.h` (with the cc65 `zpsym`). The Color Computer's player is the 6502's in 6809 (`src/6809/`).

## Exact, and your own

**Exact.** A client must draw these as the server computed them, to the
pixel, because the server keeps its own copy of what you have:
- **Tracks:** the 24-bit ARC recurrence;
- **Paths:** nibble steps;
- **Flows:** cells in order;
- **Beams:** Bresenham;
- **Settles:** columns.

**Your own.** A blast's fireball and its crater are your own drawing; they may
be a pixel off. The `OP_SETTLE` that follows writes the true columns. So
never skip a settle column because you think you already have it: the
Atari's crater ellipse differs from refview's at the edges, and when a
blast table is full the Atari drops a blast altogether.

**Effects.** These are yours to show however you like, as long as the field
comes out the same:
- the deaths (`DEATH_*`), shield events (`SH_*`), `OP_SHAKE`, `OP_FLASH` and the sounds;
- the Atari's sparks draw only on sky and are gone by the segment's end.

**At a segment's end** (`ply_end`), whatever reads the old buffer finishes
at once: settles land, paths stop, flows run out. The next segment
overwrites the buffer.

## The link

- **Holding:** a client that can't take bytes while it animates (the Atari plays a shot with all four POKEY voices, and netstream needs two) sets `CAP_HOLD`. The server then holds the link at a segment's last frame. The client plays the segment and sends `D` (script, segment, bytes taken); the server waits for every seated person's `D`, for up to the script's length plus 4 seconds.
- **Falling behind:** a slow client falls behind its script rather than dropping frames (the Atari catches up at most 4 frames a frame). Keep a big shot inside that allowance: on the Atari a nuke on ten tanks once ran 2.5 s behind, until the column writer and the colour rebuilds were made faster.
- **Losses:** a garbled frame, lost bytes or a dead link get `R` with what the client has (STATE sequence, terrain version, script and offset), and the server resends the rest.
- **Without `CAP_HOLD`** (the Intellivision: its sound chip and the FujiNet mailbox don't compete), the link stays up while a segment plays. The client decodes only a little a frame then, and the server still sends the next segment only after the client's `D`.
- **Skipped:** if the server gives up waiting for a slow client, it skips that client to the shot's end. A client without `CAP_HOLD` then sees TERRAIN arrive while its script still plays. It must stop the script, forget its terrain version (FF) and send `R`; the whole field comes again.

## Memory

The Atari's 48K is full, and it took these to fit:
- **Two programs:** the menu (title, name, room list) is a separate `MENU.COM`, and the game reboots into the other.
- **Assembly where C was big or hot:** the script player, the ops, the balloon, the sparks, and the display list compiler (1.3K of assembly against 2.9K of C).
- **Tables for switches:** the sound cues.
- **Server-side data:** the catalog (item names, prices, the settings forms) comes from the server, about 1.6K.
- **Memory nobody else uses:** the cassette buffer at $0400; screen memory for the text screen between rounds.
- **Buffers:** the script buffer is 1536 bytes, and smaller works: the server cuts segments to fit.

## The Intellivision

There is no C compiler for the CP1610, so `src/intv/` is `src/*.c` again in
IntyBASIC 1.5, with the hot paths in CP1610 assembly. What it took:
- **No bitmap.** The screen is 20x12 cards of 8x8, from 64 GRAM cards. The field (rows 2-11) is kept as two exact bit planes, dirt and fire, in GRAM's own layout in the cartridge's RAM. Each cell that changes is drawn into a pool of 44 GRAM cards: identical cells share a card, and a card is loaded only in the frame handler, before any BACKTAB word points at it. Sky and dirt colours are each card's background and foreground. Shell heads are six cards of their own, redrawn each frame.
- **Only 16 card loads a frame,** so a big change (a nuke) takes a few frames to show; the planes, which the field is checked against, are exact at once.
- **Tanks** are MOBs. Ten tanks on eight MOBs: the extra ones share and blink at 30 Hz, never yours, the shooter or the tank whose turn it is.
- **16-bit cartridge RAM:** the ROM asks for it with `MEMATTR $8000,$9BFF,"+RW"` (the FujiNet cartridge's firmware reads the width from the ROM). A reset leaves that RAM as it was, so the game sets up everything it reads at boot.
- **About 26.5K words of ROM** in the windows no other hardware uses. The ROM is about 24K words. It took the tunes at one word a step, the screen shake through the STIC's own delay registers rather than IntyBASIC's scrolling, and cold code moved where there was room.
- **CPU:** about 15,000 cycles a frame, at about 10 cycles an instruction. The terrain is drawn a cell at a time (eight columns at once), not a column at a time, and the tanks' MOB words are cached.
- **IntyBASIC's 24-word stack is too small** for the field's assembly plus the frame handler. The Makefile makes it 40 words and fails a build whose variables then overrun.
- **Sound:** the PSG's three voices. Booms are noise on the chip's envelope, so they die away smoothly; its 16-step decay takes EP x 0.143 ms.

## The Apple II

`src/apple2/` is the Atari's C again on cc65 for ProDOS, with the Atari's
6502 script player (`src/6502/`) drawing through an `fld.s` of its own. What
it took:
- **The field is the screen.** A high-res colour pixel is two dots, and the game's four values are exactly HGR's four colours in a palette: no dots black (tank), the even dot blue or violet (sky), the odd dot orange or green (dirt), both white (fire). So the field is 140x160 straight in high-res page 1, the value in each pixel's dots, and bit 7 of every byte is the round's palette (stars, storm and sunset skies are violet over green; `fld_pal` flips the page when the round's palette differs). A value's dots in a byte are one of two patterns by the byte's parity, so a span is a masked byte at each end and stores between.
- **No colours to spare:** tanks are black; yours carries a black pennant, and the turn's tank, while it aims, a blinking white arrow (fire over sky, taken off before any script). Fire stays white. The flash is full text mode for its frames, the text page's rows above the HUD all inverse spaces. The HUD is mixed mode's four text rows; the other screens are 40x24 text, inverse for highlights.
- **Memory:** 128K //e, //c, IIgs. Main RAM holds the field ($2000-$3FFF), code and data below it from $0803 and above it to $BDFF, the C stack at $BE00; the language card's $D400-$DFFF holds `fld.s`, its tables and the input. The loader skips the field's 8K in the file. The menu and the game are two programs that load each other's loader (with the ROM switched back in first: cc65's start-up asks the ROM what machine it is on). Zero page $80-$EF is full; the sound uses self-modified addresses instead.
- **The clock:** no interrupts. A Mockingboard's 6522 timer counts every cycle (`frame_tick` turns them into jiffies, if it is called at least every 65536 cycles: the field's loops call it per column, per ellipse row, per tank, per HUD row); without one, the vertical blank (polled on a //e, latched on a //c, inverted on a IIgs). Jiffies a polled blank misses come back from the server's round timer (`clock_check`).
- **CPU:** a 1 MHz 6502. An aim step redraws two numbers and the barrel, not the HUD and the tank. The player catches up 8 steps a frame. The worst shots (a MEGA MIRV, a SHOVEL BLAST) run about as far behind their scripts as on the Atari: settles write whole 160-line columns.
- **Input:** the //e's "a key is down" flag holds an arrow; AppleWin's libretro core never sets it, so a direction also lasts two polls after its last key repeat. The joystick is read every other pass.
- **Sound:** a Mockingboard's first AY: the effects are the Intellivision's, every period scaled to the Apple's 1.02 MHz (`tools/mksfx_a2.py`), and the tunes its converter's voicing (`tools/mkmus_a2.py`). Without one, the speaker gives each cue a few milliseconds of tone or noise, and there are no tunes.

## The Astrocade

`src/astrocade/` is Z80 assembly (zmac), as FujiBlast's Astrocade client
is, on the FujiNet cartridge's APPBANK pages. What it took:
- **All the RAM is the screen's 4K.** The cartridge has none. The field (160x56 at 2 bits a pixel, the Atari's own layout) and the HUD take 61 lines. The variables, the script buffer, the tracks and the stack live in the rest. The text screens (82 lines) lie over the shot's tables, which nothing needs between shots. `tools/astro/checksize.py` fails a build that runs past it, and keeps the stack's 64 bytes.
- **Code in 4K pages,** one mapped at $2000 at a time, with 2.75K resident. A page calls another with `FAR`, never by a pointer into its own ROM. Every page that draws has its own copy of the pixel primitives, the same source with a renamed prefix, because a `FAR` call a pixel would cost ten times the pixel. `tools/astro/checkcalls.py` fails a jump between pages.
- **Colour:** eight colour registers split at a movable column. A band interrupt every 4 lines sets that band's sky, its two tank colours and the split between them. A solver gives each tank a colour in every band it touches: yours, the shooter's and the turn's keep theirs. Past two tanks in a band, the others alternate at 30 Hz. The power and energy bars are the border below the field.
- **No ROM catalog from the link:** the K frames are dropped, and the build writes the catalog into a page (`barraged -catalog`). The state's catalog length and sum must match, or the waiting room says UPDATE FUJIBARRAGE.
- **CPU:** a Z80 at 1.79 MHz, about 30,000 cycles a frame, and the band interrupts take about a quarter of them. Explosions and craters are drawn a byte (4 pixels) at a time, and settles write 4 solid columns at once. The player catches up to 8 steps a frame and draws once. A MEGA MIRV still ends about a second late.
- **The link:** `CAP_HOLD`. While a script waits for its next segment, the link runs its transactions back to back. Every read asks only for what STATUS says is waiting, because older firmware loses a short read's bytes.
- **Sound:** three tones and a noise on one master oscillator. The tunes keep the master at 26 (from about 130 Hz up, so the bass is an octave or two up). On the field, where no tune plays, an effect's noise may set the master and modulate it: that gives thumps down to about 30 Hz and the nuke's roar. The tunes are voiced by instrument from music/*.txt, at 3 bytes a note.

## The Color Computer

`src/coco/`, `src/coco2/` and `src/coco3/` are the Atari's C again on CMOC
(lwtools), FujiBlast's CoCo build and link, with the 6502 script player in
6809 (`src/6809/`) drawing through each machine's `fld*.asm`. One disk
serves the CoCo 1/2 and the CoCo 3: `AUTOEXEC.BAS` loads one machine's
programs or the other's. What it took:
- **CMOC is not cc65.** Its ints are big-endian, and an operation on two bytes is done in a byte: `b | b << 8`, `a + b` and `st[OFF + seat_off[s] + f]` wrap at 256. `game.h`'s `LE16`, `PL16`, `PL16_SET`, `SEATKIND` and `SEAT_AT` read the state byte by byte; under cc65 they are the old text, so the Atari's and the Apple's builds stay byte for byte the same. Build with `-Wgives-byte`. It ignores `volatile` and section pragmas: what goes high is per module (`FB_SECT`, `tools/coco/lwasm-sect`). An apostrophe in an inline-asm comment upsets the preprocessor, and a blank line (an `#ifdef` leaves one) ends lwasm's `@` labels.
- **The CoCo 1/2: PMODE 4's artifact colours are the four values.** A colour pixel is two dots, black, blue, orange or white, so the field is the screen at 128x168, four pixels a byte as the Atari's mode E. Which of blue and orange a machine shows depends on its phase: it asks at power-on, CLEAR swaps the screen through a table. Its text is 32 columns of a 3x7 font, each row's ink and ground among the same four colours (`src/layout.h`: the HUD's and the screens' 32-column layouts).
- **The CoCo 3: the GIME's 320x200x16,** 256 bytes a line (HVEN), the field 320x176 in screen blocks mapped into one 8K slot as drawing needs them; the Disk ROM's block is mapped back before any DriveWire call. A value is a colour pattern a line: sky a dither between two colours down to the horizon (or black with stars), dirt in strata, fire one colour the field IRQ cycles, tanks black and the ten seats' colours. Flash and shake are the palette and the GIME's offsets, once a frame.
- **No raster tricks:** the FujiNet's DriveWire masks interrupts for a jiffy whenever it has data, so a split timed off the field's IRQ would flicker on a real machine.
- **Memory:** each program is a low core LOADM'd by BASIC and the files `bank.c` loads after it into all-RAM: the high core at $8000, and the art ($E000 on the CoCo 1/2, a block of its own on the CoCo 3), with the field and the player there. The CoCo 3's $1000-$1EFF (BASIC's, once it is gone) holds the sound engine and the settle's column writer. `tools/coco/lwlink-co.py` fails a link that overruns a region; the CoCo 1/2's game has about 50 bytes to spare in its low core.
- **CPU:** a CoCo 1/2 is 0.89 MHz, a CoCo 3 1.79 MHz with four times the bytes. Settles write a byte's columns at once (four on the CoCo 1/2; two, or four as a word, on the CoCo 3), explosions and craters span whole bytes (the CoCo 3 a word), the ellipse's "sky becomes" modes test a byte's pixels at once, and the HUD draws only the cells that change (`OP_DAMAGE` redraws it). A MEGA MIRV on a flat field ends about 0.8 s behind its script on the CoCo 1/2 and 2.2 s on the CoCo 3.
- **The clock:** the field sync's IRQ; jiffies lost while DriveWire masks it come back from the server's round timer (`clock_check`). The field rate is measured at boot (50 on a PAL machine).
- **Sound:** the 6-bit DAC, three voices from the Apple's Mockingboard driver (`src/coco/sound.c`: the Intellivision's effects, the tunes as the Apple's). The CoCo 3's are square waves and noise whose edges the GIME timer's IRQ makes; noise changes level only at its edges, a random run of half waves each, so it costs a third fewer IRQs than a random level every half wave. The CoCo 1/2 has no timer: the CPU mixes the voices while it would only wait, so a busy shot is partly silent. Every DAC write keeps bit 1 (the bit-banger's serial line: a real FujiNet's DriveWire) at mark.
- **Tests:** `tools/cocosim/` (MAME 0.289: `cocosim.py`, `refcheck.py`, `terrcheck.py`, `rig.sh`, `mixed.sh`). MAME's Becker port can't move from 65504, so a mixed room puts the CoCo 3 in a network namespace inside the CoCo 1/2's.

## Testing a port

- **Exact pictures:** `refview -fixture FILE` writes a field and one shot as a C header (`src/test/fixture.h`). A test build plays it offline: the `src/test.c` scenes are the field, a shot, the rooms, the results and the tunes. Compare your field after the shot with refview's `shot1.px`, pixel for pixel, for every weapon. `tools/atarisim/refcheck.py` does this for the Atari: 0 pixels must differ. `tools/a2sim/refcheck.py` does it for the Apple II (`src/apple2/test/fixture.h`, `make fixture-apple2`), with each shot's lag, and `tools/cocosim/refcheck.py` for both Color Computers (`-fixture-all`: every segment, 16-bit x).
  - `-shield I_*` puts shields on every other tank.
  - `-flat` puts all ten tanks on the same lines.
  - `-angle` and `-power` aim the shot.
  - `-w`, `-h`, `-par` and `-buf` give your shape; `-fixture-bas` writes the fixture as IntyBASIC DATA (`tools/intvsim/refcheck.py`).
- **Live:** run the server in a private network namespace with fujinet-pc and the emulator, as `tools/atarisim/rig.sh` does. Never test against the public server.
- **Headless emulators:** pass `-nojoystick` or its like. SDL reads the host's game controllers even without a window, and a stray button press can reset the machine.
