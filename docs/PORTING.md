# Porting FujiBarrage

The Atari 400/800 is the first client, and the Intellivision the second;
the Apple II, the Astrocade, the Color Computers, the 5200, the Adam, the
PC and the Arcadia 2001 followed. These notes say
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
| `scr.h` | A text screen of 24x40 with a palette per row (the menus, the rooms, the shop, the results); `SCR_COLS` 32 for a narrower one, whose columns and words are `src/layout.h`'s. `scr_label` writes a label: a platform that can colour single cells defines `SCR_INK` and draws characters with bit 7 set in its label ink (labels yellow or the nearest, values white); with `SCR_NAMES` too, `field_name` (field.h) puts a HUD row's name in its seat's colour. Elsewhere `scr_label` is `scr_text`. `scr_scroll` is the shop's: a bitmap port should move what it can and draw only the cells that change (the shop's scroll was up to 1.7 s on a 4.77 MHz PC) |
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
- **Speed.** The Z80 is the limit, so: the store's fills are by byte (four
  blocks a store, a block is compared and its cell marked only where a byte
  changed: `rowfill`, `mark_bytes` in `fld.c`), the ellipse steps its
  widths by adds, a cell's character is composed from two nibbles, a blast's
  rings are redrawn only when the client is not behind and no more than
  every third frame (`draw()` in `play.c`: each ring holds the last, the
  blast's end is the script's), and `h19_str` maps the ordinary characters
  inline. `tools/h89/fldtest.c` runs the old and the new primitives on
  random operations on the host: same store, same characters, every changed
  block's cell marked. Under HDOS the field's terrain begins to arrive while the
  shop is still up: calls into the room overlay then would load it again
  each pass (seconds each; round 1 never got to its field), so `field_coming`
  (set by the terrain frames, `net.c`) keeps the text screens' code from
  being called until the state shows the field (`main.c`).
- **Measuring lag:** `-DH89_STATS` keeps, per segment played, the jiffies it
  took and the script frames it was (`h89_seg_log`, tools/h89rig/lagseg.txt,
  `peek`ed by h89sim); `_ovl_loads` and `_ovl_ticks` (2 ms) count and time the
  HDOS overlay loads (tools/h89rig/entry.txt). A FIELD load from FN0: is about
  5 s, ROOM about 2.5 s.
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

## The Arcadia 2001

`src/arcadia/` is Signetics 2650 assembly (Macroassembler AS), on FujiBlast's
Arcadia skeleton (the page model, the linker, the checks), with the
Astrocade's link. What it took:
- **The field is the screen RAM.** The 2637 shows 13 rows of 8x16 cells or, in its 26-row mode, 26 rows of 8x8; a cell in block graphics is 3x2 blocks. So the field is 48x48 blocks, 24 rows of cells, with two HUD rows of 15 cells under it. Multicolour mode gives a cell two of the round's colours: sky and dirt by the blocks, fire (yellow) as a cell of its own colour, so a cell with three values keeps its majority ground (`tools/arc/mock_arc.py` is the rule, and `refcheck.py`'s reference). Night colours only: black sky, red dirt, yellow fire.
- **Four sprites, no multiplexing:** they are your tank, the shooter's and the next two to fire (all of them when four or fewer are alive); the others are 2x1 blocks in the field, saved and restored under them. Sprite images are user glyphs, so a sprite costs one of the eight.
- **RAM is the gate.** 84 bytes of console RAM in 26-row mode and the cartridge's 1K (written through the mailbox, about 39 cycles a byte streamed; read directly). Everything is counted: the state (378 bytes) and the slow variables in $0600-$07FF, the script buffer (160 bytes) and the shot's tracks, blasts, settles, paths, flows and beams in $2900-$2AFF, the hot variables in console RAM. Bytes are shared by phase: the decoder's working set is the player's scratch while a held segment plays, the field's UI (items line, banner, reminder) lies over the beams, a text screen's state is in the screen RAM the field uses. `ram.inc` declares every such overlay, and `tools/arc/checkram.py` fails a build where two live in the same phase share a byte. The catalog is in ROM. The server keeps 6 shell tracks at once for it (`project.PlatSlots`: the HELLO has no byte for it).
- **Code in 2K pages,** one mapped at $0800 at a time, with 3.8K fixed. A page calls another only through the fixed code, one level deep (`FARPG`). The 2650's return stack is eight deep and wraps silently: `tools/arc/checkdepth.py` keeps the deepest path at seven. Its other traps: only R0 can be the data register of an indexed load, `LODI` sets the condition code, and a store into cartridge RAM clobbers registers (`checkcc.py`, `checkclob.py`).
- **CPU:** a 2650 at 0.9 MHz, about 5,000 cycles a frame. Cells are decoded and encoded whole; the player catches up to 8 steps a frame and draws once; a MEGA MIRV on a flat field ends about 1.7 s behind its script.
- **Text:** the console's 16-column font has no lowercase and no `% - : / ! ?`: they are user glyphs on the text screens. Of its two ink sets, set 0 (white, cyan, magenta and more) is the one with white: labels cyan, values white, warnings magenta (the TODO's yellow labels would lose white).
- **The link:** `CAP_HOLD`, the Astrocade's: while a script waits for its next segment, up to six transactions a frame run back to back.
- **Sound:** one voice, a tone or a noise (or both) on one divider, eight levels. The effects are designed for it in `tools/arc/mksnd_arc.py`; the tunes are music/*.txt on that voice, two octaves down (the divider is coarse up high), the melody first, the kick and the snare cutting into it, the bass when it rests. A cue never cuts a tune, and the talk's letters blip only when nothing else plays.
- **PAL:** the timers count the cartridge's 60 Hz tick, not frames (the shot's timeline, the sound, the banner and reminder, driving, the disc's repeats), so a PAL console plays in time and its HELLO says 60, the tick's rate. MAME's Arcadia runs at 60 Hz only: PAL is untested.

## The Color Computer

`src/coco/`, `src/coco2/` and `src/coco3/` are the Atari's C again on CMOC
(lwtools), FujiBlast's CoCo build and link, with the 6502 script player in
6809 (`src/6809/`) drawing through each machine's `fld*.asm`. One disk
serves the CoCo 1/2 and the CoCo 3. What it took:
- **The lobby never runs `AUTOEXEC.BAS`.** It mounts the disk and types `RUNM"<the disk's name>"`, so `BARRAGE.BIN` is a launcher (`src/coco/launch.asm`, at $1000, the loader of `chain.asm` in `chainld.asm`): `BARRAGE3` on a CoCo 3 (its IRQ vector at `$FFF8` reads `$FE`), `BARRAG2` (the CoCo 1/2's game) otherwise, started as LOADM:EXEC would. `AUTOEXEC.BAS` only LOADMs the launcher. Test that path with `NOAUTO=1 COCO_BOOT='RUNM"BARRAGE"' tools/cocosim/rig.sh`.
- **CMOC is not cc65.** Its ints are big-endian, and an operation on two bytes is done in a byte: `b | b << 8`, `a + b` and `st[OFF + seat_off[s] + f]` wrap at 256. `game.h`'s `LE16`, `PL16`, `PL16_SET`, `SEATKIND` and `SEAT_AT` read the state byte by byte; under cc65 they are the old text, so the Atari's and the Apple's builds stay byte for byte the same. Build with `-Wgives-byte`. It ignores `volatile` and section pragmas: what goes high is per module (`FB_SECT`, `tools/coco/lwasm-sect`). An apostrophe in an inline-asm comment upsets the preprocessor, and a blank line (an `#ifdef` leaves one) ends lwasm's `@` labels.
- **The CoCo 1/2: PMODE 4's artifact colours are the four values.** A colour pixel is two dots, black, blue, orange or white, so the field is the screen at 128x168, four pixels a byte as the Atari's mode E. Which of blue and orange a machine shows depends on its phase: it asks at power-on, CLEAR swaps the screen through a table. Its text is 32 columns of a 3x7 font, each row's ink and ground among the same four colours (`src/layout.h`: the HUD's and the screens' 32-column layouts).
- **The CoCo 3: the GIME's 320x200x16,** 256 bytes a line (HVEN), the field 320x176 in screen blocks mapped into one 8K slot as drawing needs them; the Disk ROM's block is mapped back before any DriveWire call. A value is a colour pattern a line: sky a dither between two colours down to the horizon (or black with stars), dirt in strata, fire one colour the field IRQ cycles, tanks black and the ten seats' colours. Flash and shake are the palette and the GIME's offsets, once a frame.
- **No raster tricks:** the FujiNet's DriveWire masks interrupts for a jiffy whenever it has data, so a split timed off the field's IRQ would flicker on a real machine.
- **Memory:** each program is a low core LOADM'd by BASIC and the files `bank.c` loads after it into all-RAM: the high core at $8000, and the art ($E000 on the CoCo 1/2, a block of its own on the CoCo 3), with the field and the player there. The CoCo 3's $1000-$1EFF (BASIC's, once it is gone) holds the sound engine and the settle's column writer. `tools/coco/lwlink-co.py` fails a link that overruns a region; the CoCo 1/2's game has about 50 bytes to spare in its low core.
- **CPU:** a CoCo 1/2 is 0.89 MHz, a CoCo 3 1.79 MHz with four times the bytes. Settles write a byte's columns at once (four on the CoCo 1/2; two, or four as a word, on the CoCo 3), explosions and craters span whole bytes (the CoCo 3 a word), the ellipse's "sky becomes" modes test a byte's pixels at once, and the HUD draws only the cells that change (`OP_DAMAGE` redraws it). A MEGA MIRV on a flat field ends about 0.8 s behind its script on the CoCo 1/2 and 2.2 s on the CoCo 3.
- **The clock:** the field sync's IRQ; jiffies lost while DriveWire masks it come back from the server's round timer (`clock_check`). The field rate is measured at boot (50 on a PAL machine).
- **Sound:** the 6-bit DAC, three voices from the Apple's Mockingboard driver (`src/coco/sound.c`: the Intellivision's effects, the tunes as the Apple's). The CoCo 3's are square waves and noise whose edges the GIME timer's IRQ makes; noise changes level only at its edges, a random run of half waves each, so it costs a third fewer IRQs than a random level every half wave. The CoCo 1/2 has no timer: the CPU mixes the voices while it would only wait, so a busy shot is partly silent. Every DAC write keeps bit 1 (the bit-banger's serial line: a real FujiNet's DriveWire) at mark.
- **Tests:** `tools/cocosim/` (MAME 0.289: `cocosim.py`, `refcheck.py`, `terrcheck.py`, `rig.sh`, `mixed.sh`). MAME's Becker port can't move from 65504, so a mixed room puts the CoCo 3 in a network namespace inside the CoCo 1/2's.

## The Atari 5200

`src/atari5200/` is the Atari's client again (`src/atari`, `src/6502`),
assembled for the 5200: the same ANTIC, GTIA and POKEY at other addresses
(`src/atari/hw.inc`, `atari.h`), on the FujiNet 5200 cartridge, as
FujiBlast's and M.U.L.E.'s 5200 ports do. The Atari's builds stay byte for
byte the same. What it took:
- **16K of RAM** for what takes 18K on the Atari. The field is at $1010 (line 102 at $2000, the 4K boundary, as the Atari's $B000), PMBASE $0800, the compiled DLIs $2B00-$30FF, the variables round them. It fits because the catalog is in ROM (1.6K: the server's own bytes, `barraged -catalog`, indexed by `tools/atari5200/mkcat.py`; a server with another catalog says UPDATE FUJIBARRAGE), the field's line tables are ROM (the field never moves), the script buffer is 1024 bytes (the server cuts segments to fit), and the link's buffers sit in PMBASE's unused part.
- **About 37K of code for 30K a bank.** A 128K Super Cart: bank 0 is the menu (MENU.COM, its own link), banks 1-3 the game. The game's core (everything the link, the VBI or both screens reach: the script player, the field, the link, the sound) is in every bank at the same addresses, copied by `tools/atari5200/mkrom.py`; each bank's own part holds an overlay: bank 3 the field (`play.c`), bank 2 the rooms and the catalog (`room.c`, `cat.c`), bank 1 the boot code and the test scenes. A call into another bank's overlay goes through a far wrapper (`tools/atari5200/far.txt`, `mkfar.py`) that picks the bank and puts the caller's back; `farcheck.py` fails a build that calls across without one. A far function must not be given a pointer into its caller's bank: it reads with its own bank in (`entry_len` is in the core for that).
- **The cartridge:** a read of $BFD0 + 4n picks bank n, any read of $BFE0-$BFFF picks bank 3 (where the console starts). The edge has no R/W line, so nothing may store into the cartridge's window; the mailbox ($B000-$B7FF) is read only by fujinet-lib's code. `mkrom.py` checks both. One program starts the other through a stub in RAM (`bank.s`), as the Atari's reboot between its two programs; the appkeys say who and where.
- **No OS.** The BIOS's NMI pushes nothing: the VBI pushes its registers, sets CRITIC (dsp.s writes the chips itself) and ends in the crt0's tail, which counts the clock and reads the pots before it looks at CRITIC (or the sticks would be dead). The text is the BIOS's font at $F800 (capitals, which is all FujiBarrage has). The 5200 is NTSC only. MAME's 5200 runs the DLI on the last text row a line late, so the VBI sets the field's first sky colour itself.
- **The controller:** the analog stick read through a dead zone, the top button the button, the lower one TAB (the next weapon, the next page). The keypad stands for the keys: START is RETURN, `*` ESC, `#` I, `0` S, PAUSE the sound. A name with no keyboard comes from a letter wheel (`NAME_WHEEL`, `src/menus.c`).
- **Sound:** the Atari's POKEY player with all four voices all the time (there is no SIO to lend two of them to).
- **The link:** the cartridge's mailbox, driven by hand and never waited on (FujiBlast's `nstrans.c`): a write of what's queued, else a STATUS, then a READ of exactly what waits. While a script plays it asks no STATUS (`CAP_HOLD`).
- **Tests:** `tools/a52sim/` (MAME's 5200 with the FujiNet cartridge: `a52sim.py`, `refcheck.py`, `rig.sh`). A scenario's `wait` counts the game's clock, which the other program's start resets: `mwait` counts MAME's frames across one. The Go 5200 desktop (atari800 with Altirra's 5200 OS) runs the image as well.

## The Coleco Adam

`src/adam/` is the Atari's C again on z88dk (zsdcc, `-SO2`: `-SO3`
miscompiles), with the script player in Z80 (`src/z80/`, the 6809's) and
the screen and sound in assembly that the ColecoVision and the MSX can
share: `src/tms9918/` (the VDP), `src/sound/` (the engine) and
`src/sn76489/` (the chip). What it took:
- **The field is kept, not shown.** Graphics II gives each 8 pixels of a line a pattern byte and two colours, so the field (256x168, character rows 0-20) is two exact bit planes, dirt and fire, in the VDP's own cell order (`fld.asm`, at A700h-D0FFh), and each changed 8x8 cell is drawn from them (`render.asm`): a line of sky or dirt is that line's colour pair, a line with both is the dirt as the pattern in the dirt's colour on the sky's, and fire takes the colour of its band. The skies and dirt are solid bands a line (dither only muddied the tanks), and the dirt never takes its line's sky colour. The checks compare the planes with refview's field and the VRAM with the planes (`vramcheck.py`).
- **Tanks are sprites** in their seats' colours (black or white where a seat's colour is its sky's), 8x8 at 19 barrel angles, yours with a pennant. The VDP shows four sprites a line: yours, the shooter's and the turn's always get one, and a tank that would be a fifth on any of its lines is drawn into its cells instead (`field.asm`'s overlay records, a copy of its 8 pattern rows each). Talk balloons are cells too, put back when they close.
- **The VDP's interrupt is the NMI,** which nothing can mask. Every VDP access sets `vdp_busy`; an NMI that lands then only counts the frame and leaves `vdp_pend`, and `vdp_catch_up` does the rest. The registers' effects (the flash, the shake) are written by the NMI. The shake moves the name table between three copies (shifted a cell left, none, right) and the sprites with it.
- **VDP writes:** the TMS9918A needs about 8 us between accesses while it draws, so a byte every 38 T-states then, and OTIR in the blank or while the screen is off. A cell is about 3K T-states.
- **Memory:** 64K, EOS from D390h. The game's code and variables end below A700h (81 bytes to spare), the field's planes fill A700h-D0FFh, the stack D100h-D38Fh. The menu is a second program in the same image, read over the game (and back) with EOS's READ_BLOCK from the device it booted from, through a loader copied to D200h. The tank sprites and the font are a block of their own (the VRAM's 3800h-3BFFh), loaded at start-up through the field's memory and read back from the VRAM when needed (`glyph8`, `vram8`). The text screens and the HUD are assembly too (`scr.asm`, `field.asm`), to fit.
- **CPU:** a Z80 at 3.58 MHz, about 59,600 T-states a frame. Settles and the terrain write a byte's 8 columns at once (`fld_cols`; `field_columns` takes a whole TERRAIN frame), explosions and craters span whole bytes, and the player catches up 8 steps a frame. Fire's colours move up a band every 8 frames: the renderer flags each cell it draws with fire, and those are drawn again. A MEGA MIRV on a flat field ends about 1.7 s behind its script.
- **The clock:** the NMI counts frames; the rate is measured at boot. While a script plays, `vbi_now` runs each frame's sprites, shake and fire as `frame_wait` does.
- **The link:** `CAP_HOLD`, and FujiBlast's AdamNet DCB machine (`nstrans.c`), which never waits on the FujiNet: the field is drawn meanwhile.
- **Input:** the keyboard is an AdamNet device that sends characters, so an arrow counts as held for a while after each one. Controller 1 is read from its port: the stick, either button, and the keypad standing in for the keys (# RETURN, * ESCAPE, 0 TAB, 1 I, 2 S); the SmartKeys I-III are I, TAB and S.
- **Sound:** the engine steps the effects (the Intellivision's), the tunes and the whistle each frame in the NMI, as three voices with AY-style periods and volumes; C only queues requests, so nothing is half done when the NMI lands. The SN76489 back end writes only what changed (a write to the noise's control restarts it). The noise goes to the voice that wants it; a tone below about 109 Hz becomes true bass in the noise's periodic mode, clocked by tone 3 at a fifteenth of the period; white noise is clocked by tone 3 at the effect's own rate unless voice C needs tone 3. Volumes map the AY's 3 dB steps onto the SN's 2 dB ones. An AY back end (the MSX's) only has to set its registers from the same three voices.
- **Tests:** `tools/adamsim/` (`adamrun` on adamcore's library: no FujiNet, as fast as the host; `refcheck.py`, `terrcheck.py`, `vramcheck.py`) and `tools/adamrig/rig.sh` (adamcore-cli and fujinet-pc's Adam build in a network namespace).

## The PC (MS-DOS)

`src/msdos/` is the Atari's C again on Open Watcom (small model, 8086
instructions), with FujiBlast's DOS link, keyboard and lobby return, and the
script player in 8086 (`src/8086/`, the Z80's). There are two builds on one
disk, CGA (`src/msdos/cga/`) and VGA/MCGA (`src/msdos/vga/`); everything
else is both's. What it took:
- **The lobby runs the disk's `AUTOEXEC.BAT`** on the PC's own DOS (its
  boot disk holds FUJINET.SYS). `BSEL.COM` types the build's name into the
  BIOS's keyboard buffer and ends: `BARRVGA` on a VGA or MCGA (INT 10h
  AX=1A00h), `BARRAGE` on anything else. The game and the menu start each
  other the same way (`plat_chain`), and quitting mounts the lobby's disk
  and types `AUTOEXEC` (FujiBlast's `lobby.c`).
- **The screen is the field on both.** CGA's mode 4 is 320x200 in four
  colours, and the game has four values: a pixel's two bits are its value's
  colour (`fld.asm`'s `fillt`): sky colour 0, the background, any of 16;
  tank 1, fire 2, dirt 3, the palette's (green, red and brown, their bright
  set, or cyan, magenta and grey or white). A round's look is the
  palette, set through the BIOS (AH=0Bh; AH=10h on an EGA or a PCjr, whose
  AH=0Bh would load CGA's palette 1), never a redraw; the tanks are one
  colour, yours with a pennant. VGA's mode 13h is a byte a pixel: a value's
  colour comes from its line and x (`vcol`: a sky ramp, stars, dirt strata
  and a grain, fire bands) and back through a table of colours' values; a
  round's look is the DAC, and ten tanks are in their seats' colours. Both
  fields are 320x176, the CoCo 3's shape, over a HUD of three text rows.
- **Exact by the CoCo's engines:** CGA's `fld.asm` is the CoCo 1/2's
  `fld2.asm` (settles write a byte's four columns at once, the rows stepping
  through CGA's interleaved banks), VGA's the CoCo 3's `fld3.asm` with a
  loop a value for columns.
- **The clock and the speaker are one interrupt** (`timer.asm`): the PIT
  ticks four times a jiffy, puts the sound's top two voices on PIT channel
  2 by turns (a noise a random divisor about its middle), counts the jiffy
  every fourth tick and chains to the BIOS's every 65536 counts. It is
  assembly and short, and on the ticks it doesn't chain it tells the 8259
  at once and turns interrupts on: at 115200 bps the UART's interrupt can't
  wait long, and the C version lost three times the bytes. The sequencer
  is the CoCo's (`sound.c`), the effects the Intellivision's at PIT
  divisors (`tools/msdos/mksfx_dos.py`).
- **The link** is FujiBlast's: FUJINET.SYS's serial port driven by the
  UART's interrupt, a transaction in flight at a time, never waited on; no
  STATUS while a script plays (`CAP_HOLD`).
- **Memory:** a 128K PCjr keeps about 24K free with the game loaded (DOS
  2.1, FUJINET.SYS and its 16K screen in it); the game's stack uses a few
  hundred of its 4K. The titles are packed (the CoCo's `lz_pack`) and
  unpacked onto the screen (`lzd.asm`).
- **CPU:** a 4.77 MHz 8088. A MEGA MIRV on a flat field ends about 0.6 s
  behind its script on an IBM 5150 with CGA, 0.3 s on an 8 MHz 8086 with
  VGA, 4.3 s on a 4.77 MHz XT with an 8-bit VGA (86Box, which keeps a real
  PC's time: MAME's 8088 runs two or three times too fast to time
  anything).
- **Tests:** `tools/dossim/` (MAME 0.289: `dossim.py`, `refcheck.py`,
  `terrcheck.py`, the fixture on the test build's disk) and `tools/dosrig/`
  (`rig.sh` live; `86box.sh` the timing and, `LIVE=1`, the serial bytes
  lost).

## The Heathkit H89 (H19)

An H89 has no bitmap: the screen is the H19 terminal's 24 rows of 80
characters (and a 25th line), at 9600 baud, about 960 bytes a second, and
the 2 MHz Z80 spends a quarter of itself on CP/M's clock. So the field is
characters, and the work is not sending them twice.

- **Caps:** 160x44 blocks (`client.H89`: PAR 26, tank 4x2, a 2048-byte
  buffer (896 under HDOS), CAP_HOLD), 80x22 cells over a 3-row HUD (rows 22, 23 and the 25th
  line). A cell is 2x2 blocks, drawn by one graphics character: the 14
  quadrant patterns exist (`l m n o`, halves `p q` and their reverses), the
  two diagonals are the checkerboard, fire is the checkerboard too.
- **The store** (`fld.c`) is two bits a block, 1760 bytes; the tanks are
  never in it, so a script player written for another client (the Adam's,
  `src/z80`) is reused whole. The player calls `fld_plot`, `fld_get`,
  `fld_swap`, `fld_cols`, `fld_ellipse` and the rest through
  `fldglue.asm`, which keeps what its asm expects kept (BC, DE, IX, IY) and
  returns in A and L. The ellipse is `view.go`'s inequality exactly.
- **The screen** (`h19.c`) keeps `bg` (what the game drew), `have` (what the
  terminal shows) and a column span a row that may differ; a flush sends
  at most a few dozen bytes a frame: the cursor moves the cheapest way
  (cells again, BS, LF, ESC Y), a row's blank end is ESC K, a new screen
  with much to wipe ESC E, and a list scrolled a row (the shop) is the
  terminal's own delete-line and insert-line, ten bytes for what a redraw
  would send in hundreds. Reverse video is the only attribute used.
- **Menu and game are one program** (`-DONE_PROGRAM`; `WITH_MENU` and
  `WITH_GAME` in `game.h` replace the two programs' `MENU` switch), since the
  H89 has the room and CP/M cannot start one program from another as easily
  as the others can.
- **Memory, CP/M:** CP/M 2.2.04 in 62K: the program from 0100H, the variables
  and the stack under D806H (about 3K free). The script buffer is 2048 bytes.
- **Memory, HDOS 2.0:** a program loads at 2280H (the stack under 2480H) and
  must end under D090H with FN: loaded: 44K for code, window and variables,
  where the CP/M build is 53K. So the HDOS build has overlays, the recipe of
  FujiNet's H89 gmail client: two overlays take turns in one window, read
  from the end of `BARRAGE.ABS` (HDOS leaves the program's file open on
  channel -1; `.POSIT` and `.READ`, from SY1: or FN0:). `ROOM` is
  `room.c`, `menus.c` and `appkey.c` (the screens between games), `FIELD`
  is `play.c`, `field.c`, `fld.c` and the Z80 player (the player's asm is
  assembled from sed'd copies with their code in the overlay's section).
  An entry point of an overlay is compiled as `name_ovl` and reached through
  a resident stub (`tools/h89/mkovl.py`: `call ovl_tramp; defq name_ovl`);
  `ovl_tramp` loads the overlay if the window holds the other, calls on with
  the arguments where a direct call leaves them, and reloads the caller's on
  the way back. A load is disk I/O, which from FN0: is the FujiNet card's
  own, so the link's request in flight finishes first (`ovl_quiesce`).
  `tools/h89/hdosovl.py` fails a link in which anything refers into an
  overlay but through a stub, a window too small, or BSS past D090H, and
  packs the overlays on. What had to be resident: `tick`, `entry_len`,
  `field_up`/`field_down` (a frame's calls into the other overlay would load
  it every pass). The compiler's size options (`-SO3`, `--opt-code-size`),
  an 896-byte script buffer (the server splits shots into more segments),
  no terminal-side scroll in the shop and no file control blocks
  (`CLIB_OPEN_MAX=0`) make the rest of the room: about 700 bytes spare.
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

## The Arcadia 2001

`src/arcadia/` is Signetics 2650 assembly (Macroassembler AS), on FujiBlast's
Arcadia skeleton (the page model, the linker, the checks), with the
Astrocade's link. What it took:
- **The field is the screen RAM.** The 2637 shows 13 rows of 8x16 cells or, in its 26-row mode, 26 rows of 8x8; a cell in block graphics is 3x2 blocks. So the field is 48x48 blocks, 24 rows of cells, with two HUD rows of 15 cells under it. Multicolour mode gives a cell two of the round's colours: sky and dirt by the blocks, fire (yellow) as a cell of its own colour, so a cell with three values keeps its majority ground (`tools/arc/mock_arc.py` is the rule, and `refcheck.py`'s reference). Night colours only: black sky, red dirt, yellow fire.
- **Four sprites, no multiplexing:** they are your tank, the shooter's and the next two to fire (all of them when four or fewer are alive); the others are 2x1 blocks in the field, saved and restored under them. Sprite images are user glyphs, so a sprite costs one of the eight.
- **RAM is the gate.** 84 bytes of console RAM in 26-row mode and the cartridge's 1K (written through the mailbox, about 39 cycles a byte streamed; read directly). Everything is counted: the state (378 bytes) and the slow variables in $0600-$07FF, the script buffer (160 bytes) and the shot's tracks, blasts, settles, paths, flows and beams in $2900-$2AFF, the hot variables in console RAM. Bytes are shared by phase: the decoder's working set is the player's scratch while a held segment plays, the field's UI (items line, banner, reminder) lies over the beams, a text screen's state is in the screen RAM the field uses. `ram.inc` declares every such overlay, and `tools/arc/checkram.py` fails a build where two live in the same phase share a byte. The catalog is in ROM. The server keeps 6 shell tracks at once for it (`project.PlatSlots`: the HELLO has no byte for it).
- **Code in 2K pages,** one mapped at $0800 at a time, with 3.8K fixed. A page calls another only through the fixed code, one level deep (`FARPG`). The 2650's return stack is eight deep and wraps silently: `tools/arc/checkdepth.py` keeps the deepest path at seven. Its other traps: only R0 can be the data register of an indexed load, `LODI` sets the condition code, and a store into cartridge RAM clobbers registers (`checkcc.py`, `checkclob.py`).
- **CPU:** a 2650 at 0.9 MHz, about 5,000 cycles a frame. Cells are decoded and encoded whole; the player catches up to 8 steps a frame and draws once; a MEGA MIRV on a flat field ends about 1.7 s behind its script.
- **Text:** the console's 16-column font has no lowercase and no `% - : / ! ?`: they are user glyphs on the text screens. Of its two ink sets, set 0 (white, cyan, magenta and more) is the one with white: labels cyan, values white, warnings magenta (the TODO's yellow labels would lose white).
- **The link:** `CAP_HOLD`, the Astrocade's: while a script waits for its next segment, up to six transactions a frame run back to back.
- **Sound:** one voice, a tone or a noise (or both) on one divider, eight levels. The effects are designed for it in `tools/arc/mksnd_arc.py`; the tunes are music/*.txt on that voice, two octaves down (the divider is coarse up high), the melody first, the kick and the snare cutting into it, the bass when it rests. A cue never cuts a tune, and the talk's letters blip only when nothing else plays.
- **PAL:** the timers count the cartridge's 60 Hz tick, not frames (the shot's timeline, the sound, the banner and reminder, driving, the disc's repeats), so a PAL console plays in time and its HELLO says 60, the tick's rate. MAME's Arcadia runs at 60 Hz only: PAL is untested.

## The Color Computer

`src/coco/`, `src/coco2/` and `src/coco3/` are the Atari's C again on CMOC
(lwtools), FujiBlast's CoCo build and link, with the 6502 script player in
6809 (`src/6809/`) drawing through each machine's `fld*.asm`. One disk
serves the CoCo 1/2 and the CoCo 3. What it took:
- **The lobby never runs `AUTOEXEC.BAS`.** It mounts the disk and types `RUNM"<the disk's name>"`, so `BARRAGE.BIN` is a launcher (`src/coco/launch.asm`, at $1000, the loader of `chain.asm` in `chainld.asm`): `BARRAGE3` on a CoCo 3 (its IRQ vector at `$FFF8` reads `$FE`), `BARRAG2` (the CoCo 1/2's game) otherwise, started as LOADM:EXEC would. `AUTOEXEC.BAS` only LOADMs the launcher. Test that path with `NOAUTO=1 COCO_BOOT='RUNM"BARRAGE"' tools/cocosim/rig.sh`.
- **CMOC is not cc65.** Its ints are big-endian, and an operation on two bytes is done in a byte: `b | b << 8`, `a + b` and `st[OFF + seat_off[s] + f]` wrap at 256. `game.h`'s `LE16`, `PL16`, `PL16_SET`, `SEATKIND` and `SEAT_AT` read the state byte by byte; under cc65 they are the old text, so the Atari's and the Apple's builds stay byte for byte the same. Build with `-Wgives-byte`. It ignores `volatile` and section pragmas: what goes high is per module (`FB_SECT`, `tools/coco/lwasm-sect`). An apostrophe in an inline-asm comment upsets the preprocessor, and a blank line (an `#ifdef` leaves one) ends lwasm's `@` labels.
- **The CoCo 1/2: PMODE 4's artifact colours are the four values.** A colour pixel is two dots, black, blue, orange or white, so the field is the screen at 128x168, four pixels a byte as the Atari's mode E. Which of blue and orange a machine shows depends on its phase: it asks at power-on, CLEAR swaps the screen through a table. Its text is 32 columns of a 3x7 font, each row's ink and ground among the same four colours (`src/layout.h`: the HUD's and the screens' 32-column layouts).
- **The CoCo 3: the GIME's 320x200x16,** 256 bytes a line (HVEN), the field 320x176 in screen blocks mapped into one 8K slot as drawing needs them; the Disk ROM's block is mapped back before any DriveWire call. A value is a colour pattern a line: sky a dither between two colours down to the horizon (or black with stars), dirt in strata, fire one colour the field IRQ cycles, tanks black and the ten seats' colours. Flash and shake are the palette and the GIME's offsets, once a frame.
- **No raster tricks:** the FujiNet's DriveWire masks interrupts for a jiffy whenever it has data, so a split timed off the field's IRQ would flicker on a real machine.
- **Memory:** each program is a low core LOADM'd by BASIC and the files `bank.c` loads after it into all-RAM: the high core at $8000, and the art ($E000 on the CoCo 1/2, a block of its own on the CoCo 3), with the field and the player there. The CoCo 3's $1000-$1EFF (BASIC's, once it is gone) holds the sound engine and the settle's column writer. `tools/coco/lwlink-co.py` fails a link that overruns a region; the CoCo 1/2's game has about 50 bytes to spare in its low core.
- **CPU:** a CoCo 1/2 is 0.89 MHz, a CoCo 3 1.79 MHz with four times the bytes. Settles write a byte's columns at once (four on the CoCo 1/2; two, or four as a word, on the CoCo 3), explosions and craters span whole bytes (the CoCo 3 a word), the ellipse's "sky becomes" modes test a byte's pixels at once, and the HUD draws only the cells that change (`OP_DAMAGE` redraws it). A MEGA MIRV on a flat field ends about 0.8 s behind its script on the CoCo 1/2 and 2.2 s on the CoCo 3.
- **The clock:** the field sync's IRQ; jiffies lost while DriveWire masks it come back from the server's round timer (`clock_check`). The field rate is measured at boot (50 on a PAL machine).
- **Sound:** the 6-bit DAC, three voices from the Apple's Mockingboard driver (`src/coco/sound.c`: the Intellivision's effects, the tunes as the Apple's). The CoCo 3's are square waves and noise whose edges the GIME timer's IRQ makes; noise changes level only at its edges, a random run of half waves each, so it costs a third fewer IRQs than a random level every half wave. The CoCo 1/2 has no timer: the CPU mixes the voices while it would only wait, so a busy shot is partly silent. Every DAC write keeps bit 1 (the bit-banger's serial line: a real FujiNet's DriveWire) at mark.
- **Tests:** `tools/cocosim/` (MAME 0.289: `cocosim.py`, `refcheck.py`, `terrcheck.py`, `rig.sh`, `mixed.sh`). MAME's Becker port can't move from 65504, so a mixed room puts the CoCo 3 in a network namespace inside the CoCo 1/2's.

## The Atari 5200

`src/atari5200/` is the Atari's client again (`src/atari`, `src/6502`),
assembled for the 5200: the same ANTIC, GTIA and POKEY at other addresses
(`src/atari/hw.inc`, `atari.h`), on the FujiNet 5200 cartridge, as
FujiBlast's and M.U.L.E.'s 5200 ports do. The Atari's builds stay byte for
byte the same. What it took:
- **16K of RAM** for what takes 18K on the Atari. The field is at $1010 (line 102 at $2000, the 4K boundary, as the Atari's $B000), PMBASE $0800, the compiled DLIs $2B00-$30FF, the variables round them. It fits because the catalog is in ROM (1.6K: the server's own bytes, `barraged -catalog`, indexed by `tools/atari5200/mkcat.py`; a server with another catalog says UPDATE FUJIBARRAGE), the field's line tables are ROM (the field never moves), the script buffer is 1024 bytes (the server cuts segments to fit), and the link's buffers sit in PMBASE's unused part.
- **About 37K of code for 30K a bank.** A 128K Super Cart: bank 0 is the menu (MENU.COM, its own link), banks 1-3 the game. The game's core (everything the link, the VBI or both screens reach: the script player, the field, the link, the sound) is in every bank at the same addresses, copied by `tools/atari5200/mkrom.py`; each bank's own part holds an overlay: bank 3 the field (`play.c`), bank 2 the rooms and the catalog (`room.c`, `cat.c`), bank 1 the boot code and the test scenes. A call into another bank's overlay goes through a far wrapper (`tools/atari5200/far.txt`, `mkfar.py`) that picks the bank and puts the caller's back; `farcheck.py` fails a build that calls across without one. A far function must not be given a pointer into its caller's bank: it reads with its own bank in (`entry_len` is in the core for that).
- **The cartridge:** a read of $BFD0 + 4n picks bank n, any read of $BFE0-$BFFF picks bank 3 (where the console starts). The edge has no R/W line, so nothing may store into the cartridge's window; the mailbox ($B000-$B7FF) is read only by fujinet-lib's code. `mkrom.py` checks both. One program starts the other through a stub in RAM (`bank.s`), as the Atari's reboot between its two programs; the appkeys say who and where.
- **No OS.** The BIOS's NMI pushes nothing: the VBI pushes its registers, sets CRITIC (dsp.s writes the chips itself) and ends in the crt0's tail, which counts the clock and reads the pots before it looks at CRITIC (or the sticks would be dead). The text is the BIOS's font at $F800 (capitals, which is all FujiBarrage has). The 5200 is NTSC only. MAME's 5200 runs the DLI on the last text row a line late, so the VBI sets the field's first sky colour itself.
- **The controller:** the analog stick read through a dead zone, the top button the button, the lower one TAB (the next weapon, the next page). The keypad stands for the keys: START is RETURN, `*` ESC, `#` I, `0` S, PAUSE the sound. A name with no keyboard comes from a letter wheel (`NAME_WHEEL`, `src/menus.c`).
- **Sound:** the Atari's POKEY player with all four voices all the time (there is no SIO to lend two of them to).
- **The link:** the cartridge's mailbox, driven by hand and never waited on (FujiBlast's `nstrans.c`): a write of what's queued, else a STATUS, then a READ of exactly what waits. While a script plays it asks no STATUS (`CAP_HOLD`).
- **Tests:** `tools/a52sim/` (MAME's 5200 with the FujiNet cartridge: `a52sim.py`, `refcheck.py`, `rig.sh`). A scenario's `wait` counts the game's clock, which the other program's start resets: `mwait` counts MAME's frames across one. The Go 5200 desktop (atari800 with Altirra's 5200 OS) runs the image as well.

## The Coleco Adam

`src/adam/` is the Atari's C again on z88dk (zsdcc, `-SO2`: `-SO3`
miscompiles), with the script player in Z80 (`src/z80/`, the 6809's) and
the screen and sound in assembly that the ColecoVision and the MSX can
share: `src/tms9918/` (the VDP), `src/sound/` (the engine) and
`src/sn76489/` (the chip). What it took:
- **The field is kept, not shown.** Graphics II gives each 8 pixels of a line a pattern byte and two colours, so the field (256x168, character rows 0-20) is two exact bit planes, dirt and fire, in the VDP's own cell order (`fld.asm`, at A700h-D0FFh), and each changed 8x8 cell is drawn from them (`render.asm`): a line of sky or dirt is that line's colour pair, a line with both is the dirt as the pattern in the dirt's colour on the sky's, and fire takes the colour of its band. The skies and dirt are solid bands a line (dither only muddied the tanks), and the dirt never takes its line's sky colour. The checks compare the planes with refview's field and the VRAM with the planes (`vramcheck.py`).
- **Tanks are sprites** in their seats' colours (black or white where a seat's colour is its sky's), 8x8 at 19 barrel angles, yours with a pennant. The VDP shows four sprites a line: yours, the shooter's and the turn's always get one, and a tank that would be a fifth on any of its lines is drawn into its cells instead (`field.asm`'s overlay records, a copy of its 8 pattern rows each). Talk balloons are cells too, put back when they close.
- **The VDP's interrupt is the NMI,** which nothing can mask. Every VDP access sets `vdp_busy`; an NMI that lands then only counts the frame and leaves `vdp_pend`, and `vdp_catch_up` does the rest. The registers' effects (the flash, the shake) are written by the NMI. The shake moves the name table between three copies (shifted a cell left, none, right) and the sprites with it.
- **VDP writes:** the TMS9918A needs about 8 us between accesses while it draws, so a byte every 38 T-states then, and OTIR in the blank or while the screen is off. A cell is about 3K T-states.
- **Memory:** 64K, EOS from D390h. The game's code and variables end below A700h (81 bytes to spare), the field's planes fill A700h-D0FFh, the stack D100h-D38Fh. The menu is a second program in the same image, read over the game (and back) with EOS's READ_BLOCK from the device it booted from, through a loader copied to D200h. The tank sprites and the font are a block of their own (the VRAM's 3800h-3BFFh), loaded at start-up through the field's memory and read back from the VRAM when needed (`glyph8`, `vram8`). The text screens and the HUD are assembly too (`scr.asm`, `field.asm`), to fit.
- **CPU:** a Z80 at 3.58 MHz, about 59,600 T-states a frame. Settles and the terrain write a byte's 8 columns at once (`fld_cols`; `field_columns` takes a whole TERRAIN frame), explosions and craters span whole bytes, and the player catches up 8 steps a frame. Fire's colours move up a band every 8 frames: the renderer flags each cell it draws with fire, and those are drawn again. A MEGA MIRV on a flat field ends about 1.7 s behind its script.
- **The clock:** the NMI counts frames; the rate is measured at boot. While a script plays, `vbi_now` runs each frame's sprites, shake and fire as `frame_wait` does.
- **The link:** `CAP_HOLD`, and FujiBlast's AdamNet DCB machine (`nstrans.c`), which never waits on the FujiNet: the field is drawn meanwhile.
- **Input:** the keyboard is an AdamNet device that sends characters, so an arrow counts as held for a while after each one. Controller 1 is read from its port: the stick, either button, and the keypad standing in for the keys (# RETURN, * ESCAPE, 0 TAB, 1 I, 2 S); the SmartKeys I-III are I, TAB and S.
- **Sound:** the engine steps the effects (the Intellivision's), the tunes and the whistle each frame in the NMI, as three voices with AY-style periods and volumes; C only queues requests, so nothing is half done when the NMI lands. The SN76489 back end writes only what changed (a write to the noise's control restarts it). The noise goes to the voice that wants it; a tone below about 109 Hz becomes true bass in the noise's periodic mode, clocked by tone 3 at a fifteenth of the period; white noise is clocked by tone 3 at the effect's own rate unless voice C needs tone 3. Volumes map the AY's 3 dB steps onto the SN's 2 dB ones. An AY back end (the MSX's) only has to set its registers from the same three voices.
- **Tests:** `tools/adamsim/` (`adamrun` on adamcore's library: no FujiNet, as fast as the host; `refcheck.py`, `terrcheck.py`, `vramcheck.py`) and `tools/adamrig/rig.sh` (adamcore-cli and fujinet-pc's Adam build in a network namespace).

## The PC (MS-DOS)

`src/msdos/` is the Atari's C again on Open Watcom (small model, 8086
instructions), with FujiBlast's DOS link, keyboard and lobby return, and the
script player in 8086 (`src/8086/`, the Z80's). There are two builds on one
disk, CGA (`src/msdos/cga/`) and VGA/MCGA (`src/msdos/vga/`); everything
else is both's. What it took:
- **The lobby runs the disk's `AUTOEXEC.BAT`** on the PC's own DOS (its
  boot disk holds FUJINET.SYS). `BSEL.COM` types the build's name into the
  BIOS's keyboard buffer and ends: `BARRVGA` on a VGA or MCGA (INT 10h
  AX=1A00h), `BARRAGE` on anything else. The game and the menu start each
  other the same way (`plat_chain`), and quitting mounts the lobby's disk
  and types `AUTOEXEC` (FujiBlast's `lobby.c`).
- **The screen is the field on both.** CGA's mode 4 is 320x200 in four
  colours, and the game has four values: a pixel's two bits are its value's
  colour (`fld.asm`'s `fillt`): sky colour 0, the background, any of 16;
  tank 1, fire 2, dirt 3, the palette's (green, red and brown, their bright
  set, or cyan, magenta and grey or white). A round's look is the
  palette, set through the BIOS (AH=0Bh; AH=10h on an EGA or a PCjr, whose
  AH=0Bh would load CGA's palette 1), never a redraw; the tanks are one
  colour, yours with a pennant. VGA's mode 13h is a byte a pixel: a value's
  colour comes from its line and x (`vcol`: a sky ramp, stars, dirt strata
  and a grain, fire bands) and back through a table of colours' values; a
  round's look is the DAC, and ten tanks are in their seats' colours. Both
  fields are 320x176, the CoCo 3's shape, over a HUD of three text rows.
- **Exact by the CoCo's engines:** CGA's `fld.asm` is the CoCo 1/2's
  `fld2.asm` (settles write a byte's four columns at once, the rows stepping
  through CGA's interleaved banks), VGA's the CoCo 3's `fld3.asm` with a
  loop a value for columns.
- **The clock and the speaker are one interrupt** (`timer.asm`): the PIT
  ticks four times a jiffy, puts the sound's top two voices on PIT channel
  2 by turns (a noise a random divisor about its middle), counts the jiffy
  every fourth tick and chains to the BIOS's every 65536 counts. It is
  assembly and short, and on the ticks it doesn't chain it tells the 8259
  at once and turns interrupts on: at 115200 bps the UART's interrupt can't
  wait long, and the C version lost three times the bytes. The sequencer
  is the CoCo's (`sound.c`), the effects the Intellivision's at PIT
  divisors (`tools/msdos/mksfx_dos.py`).
- **The link** is FujiBlast's: FUJINET.SYS's serial port driven by the
  UART's interrupt, a transaction in flight at a time, never waited on; no
  STATUS while a script plays (`CAP_HOLD`).
- **Memory:** a 128K PCjr keeps about 24K free with the game loaded (DOS
  2.1, FUJINET.SYS and its 16K screen in it); the game's stack uses a few
  hundred of its 4K. The titles are packed (the CoCo's `lz_pack`) and
  unpacked onto the screen (`lzd.asm`).
- **CPU:** a 4.77 MHz 8088. A MEGA MIRV on a flat field ends about 0.6 s
  behind its script on an IBM 5150 with CGA, 0.3 s on an 8 MHz 8086 with
  VGA, 4.3 s on a 4.77 MHz XT with an 8-bit VGA (86Box, which keeps a real
  PC's time: MAME's 8088 runs two or three times too fast to time
  anything).
- **Tests:** `tools/dossim/` (MAME 0.289: `dossim.py`, `refcheck.py`,
  `terrcheck.py`, the fixture on the test build's disk) and `tools/dosrig/`
  (`rig.sh` live; `86box.sh` the timing and, `LIVE=1`, the serial bytes
  lost).

## The Heathkit H89 (H19)

An H89 has no bitmap: the screen is the H19 terminal's 24 rows of 80
characters (and a 25th line), at 9600 baud, about 960 bytes a second, and
the 2 MHz Z80 spends a quarter of itself on CP/M's clock. So the field is
characters, and the work is not sending them twice.

- **Caps:** 160x44 blocks (`client.H89`: PAR 26, tank 4x2, a 2048-byte
  buffer (1024 under HDOS), CAP_HOLD), 80x22 cells over a 3-row HUD (rows 22, 23 and the 25th
  line). A cell is 2x2 blocks, drawn by one graphics character: the 14
  quadrant patterns exist (`l m n o`, halves `p q` and their reverses), the
  two diagonals are the checkerboard, fire is the checkerboard too.
- **The store** (`fld.c`) is two bits a block, 1760 bytes; the tanks are
  never in it, so a script player written for another client (the Adam's,
  `src/z80`) is reused whole. The player calls `fld_plot`, `fld_get`,
  `fld_swap`, `fld_cols`, `fld_ellipse` and the rest through
  `fldglue.asm`, which keeps what its asm expects kept (BC, DE, IX, IY) and
  returns in A and L. The ellipse is `view.go`'s inequality exactly.
- **The screen** (`h19.c`) keeps `bg` (what the game drew), `have` (what the
  terminal shows) and a column span a row that may differ; a flush sends
  at most a few dozen bytes a frame: the cursor moves the cheapest way
  (cells again, BS, LF, ESC Y), a row's blank end is ESC K, a new screen
  with much to wipe ESC E, and a list scrolled a row (the shop) is the
  terminal's own delete-line and insert-line, ten bytes for what a redraw
  would send in hundreds. Reverse video is the only attribute used.
- **Menu and game are one program** (`-DONE_PROGRAM`; `WITH_MENU` and
  `WITH_GAME` in `game.h` replace the two programs' `MENU` switch), since the
  H89 has the room and CP/M cannot start one program from another as easily
  as the others can.
- **Memory:** CP/M 2.2.04 in 62K: the program from 0100H, the variables
  and the stack under D806H (about 1.4K free in the CP/M build). HDOS 2.0
  does not fit: its program starts at 2480H and the variables must end
  under D090H, and the image alone is 37K and the variables 16K, 5K too
  much. Overlays (as FujiBlast's H89 gmail client's) or a smaller image
  would make it fit.
- **Input:** the H19 sends keys, not keys held, so an arrow counts as held for
  a few frames (`H89_HOLD`), each press moving the aim a few steps; SPACE
  fires, TAB or W the next weapon, I the items, RETURN done, ESC leaves.
- **Sound:** the bell, for a big blast, a death, your turn and the go.
- **Testing:** `tools/h89sim/h89sim.py` drives the headless H89 of
  fujinet-go-h89-desktop (the same as FujiBlast's); `tools/h89rig/rig.sh`
  runs it against a private `barraged` in a network namespace, with a tap
  proxy in front (`tools/rigs/tapproxy.py`). `server/cmd/tapview` replays
  the tap's frames into the reference view, and
  `tools/h89sim/terrcheck.py` and `shotcheck.py` compare the client's store
  with it (the field after the terrain, the field after shots): both at 0
  blocks.

## Testing a port

- **Exact pictures:** `refview -fixture FILE` writes a field and one shot as a C header (`src/test/fixture.h`). A test build plays it offline: the `src/test.c` scenes are the field, a shot, the rooms, the results and the tunes. Compare your field after the shot with refview's `shot1.px`, pixel for pixel, for every weapon. `tools/atarisim/refcheck.py` does this for the Atari: 0 pixels must differ. `tools/a2sim/refcheck.py` does it for the Apple II (`src/apple2/test/fixture.h`, `make fixture-apple2`), with each shot's lag, `tools/cocosim/refcheck.py` for both Color Computers (`-fixture-all`: every segment, 16-bit x), `tools/adamsim/refcheck.py` for the Adam (the fixture loaded at run time, so one test build serves every weapon), `tools/a52sim/refcheck.py` for the 5200 (`-fixture-all`: a 1024-byte buffer splits the biggest shots), and `tools/dossim/refcheck.py` for both PC builds (the fixture read from the test build's disk).
  - `-shield I_*` puts shields on every other tank.
  - `-flat` puts all ten tanks on the same lines.
  - `-angle` and `-power` aim the shot.
  - `-w`, `-h`, `-par` and `-buf` give your shape; `-fixture-bas` writes the fixture as IntyBASIC DATA (`tools/intvsim/refcheck.py`).
- **Live:** run the server in a private network namespace with fujinet-pc and the emulator, as `tools/atarisim/rig.sh` does. Never test against the public server.
- **Headless emulators:** pass `-nojoystick` or its like. SDL reads the host's game controllers even without a window, and a stray button press can reset the machine.
