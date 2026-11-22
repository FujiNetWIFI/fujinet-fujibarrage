# FujiBarrage

FujiBarrage is an artillery game for two to ten tanks over FujiNet. Seats with
no player get bots. It runs on the Atari 400/800/XL/XE with 48K, NTSC or PAL;
other FujiNet machines are to follow (see `docs/PORTING.md`).

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

- **Shop:** the stick moves up and down, Fire buys, Left sells, Return is done.
- **Waiting room:** the host (the first person in) presses Fire to start, and bots fill the empty seats. S shows the settings and TAB turns their pages. The host changes a setting with the stick: up and down pick it, left and right change it.

The host's settings:

| Page | Settings |
|---|---|
| Field | Sky, bumpiness, slopes, flattened peaks, random land, wind (most, and whether it changes), gravity, air thickness, how much dirt stays up, tanks fall, falls hurt, tunnelling, blast size |
| Walls | The walls (none, wrap, padded, rubber, spring, concrete, random, erratic) and how far the edges extend |
| Game | Tanks (2–10, default 10), rounds (1–100, default 10), play (take turns, all at once, or aim together), turn order and timer, talking tanks and how often, arms level, starting cash, interest, whether bots shop |

The server's four rooms are OPEN RANGE, BOUNCE HOUSE (rubber walls),
WRAPAROUND, and MAYHEM (erratic walls and changing wind, all at once).

## Layout

| Path | What |
|---|---|
| `server/` | `barraged`, the game server (Go), and `refview`, the reference client: it draws what a client of any shape should show, and writes the test scenes' fixtures. |
| `server/internal/` | `sim` the physics and every weapon; `project` each client's view (columns, scripts); `game` the rules and rounds; `bot` the eight bot tiers; `talk` the tanks' phrases; `server` the rooms and links; `view` the reference player. |
| `src/*.c` | The client, platform neutral: menus, rooms, shop, play, the link. |
| `src/platform-specific/*.h` | The API each platform implements. |
| `src/atari/` | The Atari implementation: cc65, ANTIC mode E with compiled DLIs, players and missiles behind the tanks, POKEY, netstream. The script player, the field primitives and the display list compiler are in assembly. |
| `music/` | The tunes, in FujiBlast's text format (`tools/mus.py`). |
| `docs/PROTOCOL.md` | The wire protocol, the state layout and the script ops. |
| `docs/OPS.md` | How every weapon and item reaches the client as script ops. |
| `docs/PORTING.md` | How to bring FujiBarrage to another machine. |
| `tools/` | The font and music converters, and the Atari rigs. |

## Building

```
defoogi make atari      # r2r/atari/barrage.atr
make server             # server/barraged, server/refview
make proto              # src/proto.h (and src/atari/proto.inc) from the server's constants
make test               # the server's tests
```

- **Default server:** `make SERVER=host:port` sets the server the game uses when the lobby gave none. It is remembered.
- **The disk:** single density, picoboot, no DOS. It holds two programs:
  - `BARRAGE.COM` is the game. It boots, reads the FujiNet appkeys (creator 1, app 0x13: the name and the room), and plays.
  - `MENU.COM` is the title, the name entry and the room list. The game loads it from the disk itself when the appkeys don't say who is playing where. The menu saves the keys and reboots into the game. Leaving a room clears the room key and reboots back to the menu.
- **Memory:** the game fills 48K. `src/atari/barrage.cfg` says what goes where, and the link fails when it doesn't fit.

## Running a server

```
server/barraged -stream :9913
```

- **Rooms:** `barraged` serves `room1`–`room4`, plus the hidden `dev1`–`dev3`, whose settings `-devset` (or `BARRAGE_DEVSET`) changes for tests, e.g. `tanks=3,rounds=2,timer=10,tier=0`.
- **Phrases:** `BARRAGE_TALK_FIRE` and `BARRAGE_TALK_LAST` name phrase files of your own: a line each, 40 characters at most.
- **Lobby:** it lists itself in the FujiNet lobby (key 0x13) only when `BARRAGE_LOBBY` is `qa`, `prod` or a URL. `BARRAGE_SERVER_URL` and `BARRAGE_CLIENT_ATARI` then say what it lists.
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
