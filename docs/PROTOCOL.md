# FujiBarrage stream protocol (version 1)

One TCP connection per client: FujiNet netstream on the Atari, a socket
elsewhere. The constants are in `server/internal/proto/proto.go`; `make proto`
writes them as `src/proto.h`. Multi-byte numbers are little-endian.

The server owns the whole game. The **world** is 640×400 square units (16:10)
and all physics, collisions and damage happen there. Each client says what it
is (its HELLO capabilities), and the server sends it everything **already in
its own pixels and frames**. A client never tests a collision; it draws what
it is told, so every screen agrees on every outcome, whatever its resolution.

## Framing

Every frame is COBS-encoded and ends in a `0` byte. Decoded, a frame is

    type, payload..., sum1, sum2

`sum1`/`sum2` are Fletcher-16 (mod 255) over type and payload. A decoded
frame is at most 128 bytes, so a payload is at most 125. A receiver that
loses or garbles a byte drops that frame and finds the next at the next `0`.

## Flow control

The server keeps at most 192 encoded bytes in flight to a client. The
client reports how many bytes it has taken from its receive ring, mod 256
(`C`), at least every 48 bytes. A client that is behind gets no new STATE.

## Client → server

| type | payload | meaning |
|---|---|---|
| `H` | 30 bytes, below | hello: sit down in a room (or watch) |
| `T` | (1: dev rooms too) | list the rooms |
| `A` | seq, act, args | an action (`ACT_*`); seq is never 0, a repeated seq is acknowledged and not applied again |
| `D` | sid, seg, taken | segment `seg` of shot `sid` played (sid 0: a `W` hold is over); credit starts again from `taken` |
| `W` | reason, seconds | ask to hold the link (the shop) |
| `R` | taken, tv, sid, seg, off16, cat16 | resync: what it has (tv `FF`: its field is not to be trusted) |
| `C` | taken | credit |
| `P` | — | keepalive (answered with a commit) |
| `B` | — | leave and hang up |

### HELLO

| byte | |
|---|---|
| 0 | version (1) |
| 1 | flags: 1 watch only |
| 2-9 | room id, NUL padded |
| 10-17 | name, NUL padded |
| 18-19 | playfield width, pixels (16-1024) |
| 20 | playfield height, pixels (16-240) |
| 21 | pixel aspect, width/height in 64ths (64: square; Atari mode E ≈ 109) |
| 22 | frame rate: 50 or 60 |
| 23-24 | its tank sprite, width and height in pixels |
| 25-26 | script buffer, bytes (128 or more) |
| 27 | colour objects it shows in a band (0: any) |
| 28 | capabilities: 1 `CAP_HOLD` (pause the link while a segment plays) |
| 29 | platform id |

### Actions

| act | args | when |
|---|---|---|
| `ACT_AIM` | angle, power16 | your turn: aim (the others see the turret move) |
| `ACT_WEAPON` | item | pick a weapon you own |
| `ACT_FIRE` | angle, power16, weapon, guidance, target | fire (guidance `I_NONE`/`FF`, target a seat) |
| `ACT_USE` | item | battery, a shield, parachute or impact fuse on/off |
| `ACT_DRIVE` | dir (0 left, 1 right) | use Fuel |
| `ACT_BUY` | item, bundles | shop |
| `ACT_SELL` | item, units | shop |
| `ACT_DONE` | — | done shopping / with the defence window |
| `ACT_SET` | setting, value | the host, in the waiting room |
| `ACT_START` | — | the host starts the game |
| `ACT_LEAVE` | — | give up the seat (a bot takes the tank), keep watching |

## Server → client

| type | payload | meaning |
|---|---|---|
| `Q` | runs of off16, len, bytes | into the STATE being built |
| `C` | ack seq | the STATE being built is whole: it becomes the STATE |
| `T` | index, count, record[24] | a room (count 0: none) |
| `E` | code | `ERR_NO_ROOM`, `ERR_VERSION`, `ERR_FULL`, `ERR_CAPS` |
| `K` | off16, bytes | the CATALOG |
| `G` | base tv, new tv, flags, x16, column entries | TERRAIN |
| `S` | sid, seg, flags, off16, bytes | a SCRIPT segment |
| `Z` | — | the hold asked for is granted: nothing more until `D` |

Room record: id[8], name[12], people seated, seats (10), phase, round.

## The client's documents

The server keeps four documents for each client, at its resolution.

### CATALOG

Sent once after HELLO, in `K` frames. STATE says its length and Fletcher-16
(`OFF_CAT_LEN`, `OFF_CAT_SUM`), so a client knows when it has it all.

    version, items (48), settings (27), pages (3)
    pages:    name (length-prefixed) × pages
    items:    kind (0 weapon, 1 other), arms level, bundle, price24, name × items
    settings: page, type, label, then
                type 0 (choice):  n, names × n
                type 1 (number):  min16, step16, count, decimals
    tiers:    n, names × n          (the bots' skill levels)

A setting's value is an index: for a number it means `min + index × step`,
shown with `decimals` places.

### STATE

A fixed record of `STATE_SIZE` bytes, built for each client and sent as
patches (`Q`) then a commit (`C`). The layout is in `proto.h` (`OFF_*`,
`P_*`): the phase, round, turn, timer, wind, resolved walls and sky, the
settings, then ten seats (name, colour, kind, tier, energy, shield, angle,
power, **x and y in the client's pixels**, weapon, flags, wins, score), then
this client's own cash, fuel and inventory.

**A STATE never gives away a shot.** Until a client has played a shot's
script, its STATE shows the seats as they were before it, the phase is
`PH_SHOT`, and `OFF_TV` is the terrain version the shot started from. The
script's own `DAMAGE`, `FALL` and `DEATH` ops move the HUD meanwhile.

### TERRAIN

Each column of the client's playfield, top to bottom, as one entry:

| entry | column |
|---|---|
| `00`-`EF` | solid from this row down to the bottom (one byte: the usual case) |
| `F0` | no dirt at all |
| `F0`+n (n 1-15) | n spans follow, each top, end (end exclusive) |

A client pixel is dirt when most of the world cells it covers are dirt.

`G` frames carry entries for consecutive columns from x. A version (tv) is
whole after the frame with flags bit 0 (`G_LAST`). Base `FF` means the whole
field: the client applies it whatever it had. Otherwise the client applies the
frame only if its own tv is the base; if not, it asks again (`R` with tv
`FF`).

The field changes only through scripts (their `SETTLE` ops), then a `G` frame
bumps the version after the client acks the last segment (`D`). A new round
sends the whole field.

### SCRIPT

One shot, or one volley, as a timeline in **client frames and client
pixels**. It is cut into segments no bigger than the client's buffer. Each
segment ends in `OP_END`, and the client plays it from its own buffer.

A `CAP_HOLD` client (the Atari) gets each segment whole, with `S_HOLD` on its
last frame. After that the server sends nothing at all until the client's `D`.
The client then:

1. sends a credit,
2. pauses the link (the Atari stops netstream and has all four POKEY voices),
3. plays the segment,
4. resumes the link and sends a 0, then `D(sid, seg, taken)`.

`D` re-baselines the credit count, so bytes lost across the pause do not
shrink the window. A client without `CAP_HOLD` keeps its link up while it
plays; it too gets the next segment only after its `D`.

#### Playing a segment

Each frame:

1. Every track, path, blast, flow, dirt ball, beam and settle moves one step.
2. The ops due this frame run.

Ops run in order until `OP_WAIT n` (n frames; 0 means 256) or `OP_END`. A
client that falls behind skips drawing frames and keeps the timeline: frames
are the clock.

| op | args | |
|---|---|---|
| `OP_END` 00 | | the segment ends (the script's last: the shot is over) |
| `OP_WAIT` 01 | n | let n frames pass |
| `OP_ARC` 10 | slot, style, x16, y16, vx24, vy24, ax16, ay16, n16 | a track |
| `OP_PATH` 11 | slot (0-3), style, x16, y16, n, steps × n | a track a pixel a frame |
| `OP_STOP` 12 | slot | a track ends now |
| `OP_EXPLODE` 20 | x, y, rx, ry, kind, frames | a blast |
| `OP_DIRT` 21 | x16, y16, rx, ry, frames | dirt fills the air in the ellipse |
| `OP_TUNNEL` 22 | x0, y0, x1, y1, width | (reserved: not sent) |
| `OP_BEAM` 23 | x0 16, y0 16, x1 16, y1 16, kind, frames | a laser's beam |
| `OP_FLOW` 24 | kind, frames, hold, n, cells (x16, y8) × n | fire or mud running |
| `OP_SETTLE` 25 | x16, n, frames, n column entries | these columns become these, in `frames` |
| `OP_TALK` 30 | seat, len, text | a phrase over a tank |
| `OP_SOUND` 31 | id, size (0-3) | what happened, for the platform's own sound |
| `OP_SHIELD` 32 | seat, event, hp | `SH_HIT/BREAK/DEFLECT/UP/LIFT` |
| `OP_DAMAGE` 33 | seat, amount, energy left | |
| `OP_FALL` 34 | seat, x16, y, frames, chute | the tank goes there |
| `OP_DEATH` 35 | seat, effect | the tank is gone (its blast comes as `OP_EXPLODE`) |
| `OP_TANK` 36 | seat, x16, y, angle | placed |
| `OP_WIND` 37 | wind16 | |
| `OP_SHAKE` 38 | frames | |
| `OP_FLASH` 39 | colour, frames | the screen flashes (nukes, lightning) |
| `OP_FIRE` 3A | seat, angle, power16, weapon | a shot leaves the barrel |

x and y are signed 16-bit client pixels; y can be negative (above the field).

**Tracks.** A slot (0-15) holds a moving thing. `OP_ARC` starts it, or
restarts a live slot from a new keyframe. Position and velocity are 12.12
fixed point and acceleration 4.12: x16/y16 are 12.4, the start in 1/16
pixels, and the client shifts them left by 8. Each frame:

    X += VX;  Y += VY;  VX += AX;  VY += AY        (24-bit signed)

and the head is drawn at `(X >> 12, Y >> 12)`, arithmetic shifts. The track
lives for n steps. At its start the head is shown at its first position. A
track with no steps left is erased at the next frame's step, unless an op of
that frame restarts its slot. The server runs the very same integers when it
fits arcs, so a client can trust them to within ½ pixel.

Style: `ST_SHELL` 0, `ST_TRACER` 1, `ST_SMOKE` 2 (bits 4-7: the seat, whose
colour the trail stays in until the round ends), `ST_ROLLER` 3, `ST_DIGGER` 4,
`ST_BOMBLET` 5.

**Paths.** Rollers and diggers follow the ground, so they come as
`OP_PATH`: a start pixel, then a step a frame, one byte each, `dx << 4 | dy &
15` (signed nibbles, -8 to 7). Path slots 0-3 are their own, apart from the
tracks'. A new `OP_PATH` on a live slot ends the old one first (a bigger
step, or more than 180, starts a new op). `ST_ROLLER` draws its head over
air only and erases it as it moves. `ST_DIGGER` draws its head over air or
dirt and leaves air behind (it bores; the `OP_SETTLE` after the dig has the
true tunnel). When the steps run out the head goes at the next frame's step.

**Flows.** `OP_FLOW` cells (napalm, mud, a geyser's spray) appear in order,
evenly over `frames` frames: `FLOW_FIRE` lights air pixels, `FLOW_MUD` turns
air into dirt. `hold` frames after the last one is in, a fire's pixels go
back to air (mud stays: the `OP_SETTLE` that follows is the truth). A big
flow comes as several ops, one after another, ending together.

**Dirt balls.** `OP_DIRT` grows an ellipse of dirt over the air, like a
blast, over `frames` frames. Its `OP_SETTLE` follows.

**Beams.** `OP_BEAM` draws a line, over air and dirt, for `frames` frames;
then its pixels are air (it burned through). The channel's `OP_SETTLE`
follows.

**A segment's end.** A client reuses its buffer for the next segment, so at
`OP_END` whatever still reads the old one finishes at once: settles are
written, paths stop, flows run out. Tracks, blasts, dirt balls and beams
keep their own copies and go on.

**Blasts.** `OP_EXPLODE` grows a fireball ellipse from nothing to (rx, ry) over
`frames` frames. On its last frame a crater kind (`EX_FIRE`, `EX_NUKE`,
`EX_CLEAR`, `EX_DEATH`) cuts the ellipse to air. `EX_PLASMA` and `EX_PUFF`
only light the air, then go. The crater is the client's own drawing, so it
may be a pixel off. The `OP_SETTLE` that follows on the same frame writes
the true columns.

**Effects.** `OP_DEATH`'s effect (`DEATH_FADE` .. `DEATH_SCATTER`), the
`OP_SHIELD` events and `OP_SHAKE` are the client's to show as it likes, as
long as the field comes out the same: the Atari throws sparks that draw
only on sky and are gone by the segment's end, and moves its whole display
a few lines for a shake (and for every `EX_NUKE`).

**Settles.** These are the field's truth: every change to the dirt reaches
the client as `OP_SETTLE` columns. Craters, tunnelling, falling dirt and dirt
added all come this way. With frames 0 the columns are written at once.
Otherwise the client moves them there over that many frames: dirt falling,
or just written at the end.

## Turn sync

The game waits for every connected, seated person's last `D` of a shot, up to
the script's length plus 4 seconds. A client that is still behind when the
next turn starts skips to the end. The server sends it the field's version
after the shot and the current STATE, and it stops playing.

## Resync

On a garbled frame or a ring overrun, the client sends `R` with what it can
still trust:

- `taken`: its credit count;
- `tv`: its field's version, `FF` if a terrain frame went missing;
- the shot and segment it is on;
- `cat16`: how much of the CATALOG it holds whole.

The server resends the STATE whole, the field (whole, if tv is `FF` or not
what it sent), the current segment from its start, and the CATALOG from
`cat16`.

## Lobby

The FujiNet lobby lists each public room under app key creator 1, app 1,
key `0x13`, as `tcp://host:9913/?table=room1`. The name is key 0, as in
every FujiNet game.
