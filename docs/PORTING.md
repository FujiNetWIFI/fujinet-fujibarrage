# Porting FujiBarrage

The Atari 400/800 is the first client. These notes say what another machine
needs, and what the Atari taught.

## What the server does for you

All of the game runs on the server: the physics, every weapon, the dirt, the
falls, the damage, the bots and the rules. A client never decides anything; it
shows what it is sent, so a port is a screen, a sound chip and a link.

The server shapes everything for the client that asks. It holds the world at
640x400 square units, and its HELLO tells the server:

| HELLO byte | What | The Atari's |
|---|---|---|
| 18-19 | field width, pixels (16-1024) | 160 |
| 20 | field height, pixels (16-240) | 168 |
| 21 | pixel aspect, 64ths | 109 |
| 22 | frames a second, 50 or 60 (`plat_hz`) | 60, or 50 on PAL |
| 23-24 | a tank's width and height, pixels | 7, 5 |
| 25-26 | the script buffer, bytes (128 or more) | 1536 |
| 27 | colour bands | 4 |
| 28 | capabilities (`CAP_HOLD`) | `CAP_HOLD` |
| 29 | platform (`PLAT_*`) | `PLAT_ATARI` |

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
| `scr.h` | A text screen of 24x40 with a palette per row (the menus, the rooms, the shop, the results) |
| `field.h` | The playfield: a bitmap of four pixel values (`PX_SKY`, `PX_DIRT`, `PX_FX`, `PX_TANK`), the tanks, the status rows, the balloon, the sparks, the tracks (`trk_*`). Its sizes (`FIELD_W` and so on) are per platform, under `#ifdef`. |
| `sound.h` | Cues by meaning (`SND_*`, `UI_*`), the falling shell's whistle, the talk blip, two-voice tunes |

**The script player.**
- **Who calls it:** `play.c` drives the player through `ply_reset`, `ply_step`, `ply_draw` and `ply_end`.
- **The Atari's:** assembly (`src/atari/ply.s`, `ops.s`, `trk.s`). It calls back into `play.c` for what C does better: `c_op` handles TALK, SOUND, SHIELD, DAMAGE, FALL, DEATH, TANK, FLASH and FIRE, and `c_anim` handles the talk chatter.
- **For a new machine:** write one from `docs/PROTOCOL.md`, or start from `server/internal/view/view.go`, the reference player in Go. It follows PROTOCOL.md to the letter, and `refview` draws with it.
- **Before porting:** the `ply_*` declarations sit in `play.c` with a cc65 `zpsym`; move them to a platform header first.

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

## Memory

The Atari's 48K is full, and it took these to fit:
- **Two programs:** the menu (title, name, room list) is a separate `MENU.COM`, and the game reboots into the other.
- **Assembly where C was big or hot:** the script player, the ops, the balloon, the sparks, and the display list compiler (1.3K of assembly against 2.9K of C).
- **Tables for switches:** the sound cues.
- **Server-side data:** the catalog (item names, prices, the settings forms) comes from the server, about 1.6K.
- **Memory nobody else uses:** the cassette buffer at $0400; screen memory for the text screen between rounds.
- **Buffers:** the script buffer is 1536 bytes, and smaller works: the server cuts segments to fit.

## Testing a port

- **Exact pictures:** `refview -fixture FILE` writes a field and one shot as a C header (`src/test/fixture.h`). A test build plays it offline: the `src/test.c` scenes are the field, a shot, the rooms, the results and the tunes. Compare your field after the shot with refview's `shot1.px`, pixel for pixel, for every weapon. `tools/atarisim/refcheck.py` does this for the Atari: 0 pixels must differ.
  - `-shield I_*` puts shields on every other tank.
  - `-flat` puts all ten tanks on the same lines.
  - `-angle` and `-power` aim the shot.
- **Live:** run the server in a private network namespace with fujinet-pc and the emulator, as `tools/atarisim/rig.sh` does. Never test against the public server.
- **Headless emulators:** pass `-nojoystick` or its like. SDL reads the host's game controllers even without a window, and a stray button press can reset the machine.
