# What every weapon and item sends (SCRIPT ops)

The server resolves everything; a client only plays ops (docs/PROTOCOL.md).
This is what each of FujiBarrage's 48 items puts in a script, so a new
platform knows what it must draw. Every change to the dirt ends in
`OP_SETTLE` columns; every shot starts with `OP_TALK` (maybe) and `OP_FIRE`.

| Item | Ops |
|---|---|
| Mini Missile, Missile, Mini Nuke, Nuke | `ARC` (shell; more arcs as keyframes, after bounces and wraps), `EXPLODE` (fire; nuke from r60: `EX_NUKE` + `FLASH`), `SETTLE`, `DAMAGE`/`SHIELD` |
| Hopper | `ARC`, `EXPLODE`, `SETTLE`, then a new `ARC` from the crater: three times |
| Cluster Bomb | `ARC`, `EXPLODE` (centre), 5-10 `ARC`s (`ST_BOMBLET`, curving), their `EXPLODE`s, a last big `EXPLODE`, `SETTLE`s |
| MIRV, Mega MIRV | `ARC` up to the apex, then 5 / 9 `ARC`s (`ST_BOMBLET`) in a fan, their `EXPLODE`s and `SETTLE`s |
| Napalm, Inferno | `ARC`, `FLOW` (fire: it runs downhill and pools; it burns 2 / 3 s, then goes), `DAMAGE` (through shields) |
| Tracer | `ARC` (`ST_TRACER`): nothing else |
| Smoke Tracer | `ARC` (`ST_SMOKE` + seat): the trail stays, in the seat's colour |
| Light Roller, Roller, Heavy Roller | `ARC` to the ground, `PATH` (`ST_ROLLER`) along the surface (off an edge: a new `ARC`, then `PATH` again), `EXPLODE`, `SETTLE` |
| Shovel Charge, Shovel Blast | `EXPLODE` (`EX_CLEAR`, no fire) fanned out from the barrel, `SETTLE` |
| Dirt Buster, Heavy Buster | `ARC`, `EXPLODE` (`EX_CLEAR`), `SETTLE` |
| Light Drill, Drill, Heavy Drill | `ARC`, `PATH` (`ST_DIGGER`: cuts as it goes), `SETTLE` |
| Light Mole, Mole, Heavy Mole | `ARC`, `PATH` (`ST_DIGGER`) toward a tank, `EXPLODE`, `SETTLE` |
| Dirt Clump, Dirt Heap, Landslide | `ARC`, `DIRT` (the disc fills), `SETTLE` |
| Mudslide | `ARC`, `FLOW` (mud: cells become dirt), `SETTLE` |
| Dirt Geyser | `ARC`, `FLOW` (mud, spraying up), `SETTLE` |
| Quake | `ARC`, `SHAKE`, `SETTLE` (frames > 0: the dirt falls) |
| Plasma Burst | `ARC`, `EXPLODE` (`EX_PLASMA`: no crater), `DAMAGE` |
| Laser | `BEAM` (from the barrel; two under wrap walls), `SETTLE` (the channel), `DAMAGE`/`SHIELD` |
| Heat Seeker, Level Seeker, Drop Seeker | more `ARC` keyframes where it steers |
| Ballistic Computer | nothing: the server solved the power before firing |
| Autopilot | `ARC`s in a straight line to the target, `SETTLE` (what it bored) |
| Fuel | no script: the tank's x and y change in STATE |
| Parachute | `FALL` with chute 1, `SOUND` (`SND_CHUTE`) |
| Battery | no script: energy changes in STATE |
| Shield, Heavy Shield | `SHIELD` (hit, break), `EXPLODE` (`EX_PUFF`) when a shell meets the ring |
| Bounce Shield | `SHIELD` (deflect); the shell's new `ARC` goes off the ring |
| Deflector, Super Deflector | `SHIELD` (lift) while a shell is pushed up over the tank; new `ARC` keyframes |
| Defense Computer | no script: the shields are up in STATE when the round begins |
| Impact Fuse | no script of its own: the shell explodes at the first dirt (no tunnelling) |

Tunnels hold up: dirt bored by a drill, a mole, a tunnelling shell or a
laser stays over the tunnel whatever Dirt Stays Up says (that is what Quake
is for). Blasts and new dirt let floating dirt fall unless the round's roll
says it stays.

Tanks falling: `FALL`. Tanks dying: `TALK` (last words, maybe), `DEATH`, then
their blasts as `EXPLODE` and `SETTLE`. Dirt falling after an edit: `SETTLE`
with frames > 0.
