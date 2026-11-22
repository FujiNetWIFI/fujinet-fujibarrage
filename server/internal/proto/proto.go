// Package proto is FujiBarrage's wire format (docs/PROTOCOL.md): frame
// types, the per-client documents (STATE layout, TERRAIN and SCRIPT
// encodings), and the ids both sides share. CHeader writes the same
// constants as src/proto.h for the 8-bit clients.
package proto

import (
	"fmt"
	"io"
)

const (
	Version    = 1
	MaxFrame   = 128          // decoded frame, type and sum included
	MaxPayload = MaxFrame - 3 // type + 2 sum bytes
	Window     = 192          // encoded bytes in flight to one client
	Port       = 9913
	NameLen    = 8
	RoomLen    = 8
	Seats      = 10
	NoSeat     = 0xFF
	WorldW     = 640 // the world, square units (16:10)
	WorldH     = 400
	MaxFieldW  = 1024 // a client's playfield, pixels
	MaxFieldH  = 240
	RoomRec    = 24
	HelloLen   = 30
	MaxTalk    = 40 // a phrase, characters
)

// HELLO (client -> server), HelloLen bytes:
//
//	0 version  1 flags  2-9 room  10-17 name
//	18-19 field width (px, LE)  20 field height (px)
//	21 pixel aspect (width/height, 1/64ths: 64 square)
//	22 frame rate (50/60)  23 tank sprite width  24 tank sprite height
//	25-26 script buffer (bytes, LE)  27 colour objects a band (0: any)
//	28 capability flags (Cap*)  29 platform id (Plat*)
const (
	HFlags  = 1
	HRoom   = 2
	HName   = 10
	HFieldW = 18
	HFieldH = 20
	HPar    = 21
	HHz     = 22
	HTankW  = 23
	HTankH  = 24
	HBuf    = 25
	HBands  = 27
	HCaps   = 28
	HPlat   = 29
)

// HELLO flags.
const HelloWatch = 1 // watch, never take a seat

// Capability flags (HELLO byte 28).
const (
	CapHold = 1 // pause the link while a segment plays: send it whole, then hold
)

// Platform ids (HELLO byte 29): for the logs and the lobby only.
const (
	PlatTest = iota
	PlatAtari
	PlatIntv
)

// Client -> server frame types.
const (
	SCHello  = 'H' // HelloLen bytes (above)
	SCRooms  = 'T' // list the rooms
	SCAct    = 'A' // seq, act, args (0-6)
	SCDone   = 'D' // sid, seg, taken: played a script segment (sid 0: a W hold ended)
	SCWant   = 'W' // reason (Hold*), seconds: ask to hold the link (the shop)
	SCResync = 'R' // have-list: tv, sid, seg, off16: send again what is missing
	SCCredit = 'C' // bytes taken from the receive ring so far, mod 256
	SCPing   = 'P' // keepalive
	SCBye    = 'B' // leave the room and hang up
)

// Server -> client frame types.
const (
	CSPatch   = 'Q' // runs of off16, len, bytes: into the STATE being built
	CSCommit  = 'C' // ack seq: the STATE being built is whole
	CSRoom    = 'T' // index, count, room record (count 0: no rooms)
	CSError   = 'E' // Err*
	CSCatalog = 'K' // off16, bytes: the CATALOG
	CSTerrain = 'G' // base tv, new tv, flags (GLast), x16, column entries
	CSScript  = 'S' // sid, seg, flags (S*), off16, bytes
	CSHold    = 'Z' // the hold asked for (W) is granted: nothing more until D
)

// CS_TERRAIN flags.
const GLast = 1 // the version is whole after this frame

// CS_SCRIPT flags.
const (
	SLastFrame = 1 // the segment is whole after this frame
	SLastSeg   = 2 // the script's last segment
	SHold      = 4 // (CapHold) the link holds after this frame until D
)

// Hold reasons (SC_WANT).
const (
	HoldShop = 1
	HoldMenu = 2
)

// Errors (CS_ERROR).
const (
	ErrNoRoom = 1 + iota
	ErrVersion
	ErrFull
	ErrCaps
)

// Actions (SC_ACT act). Args follow.
const (
	ActNone   = iota
	ActAim    // angle, power lo, power hi
	ActWeapon // item
	ActFire   // angle, power lo, power hi, weapon, guidance (0: none), target seat
	ActUse    // item: a battery, a shield, the parachute or fuse (toggle)
	ActDrive  // dir (0 left, 1 right): Fuel
	ActBuy    // item, bundles
	ActSell   // item, units
	ActDone   // shop, defence window or results: done
	ActSet    // setting, value (host, waiting room)
	ActStart  // host, waiting room
	ActLeave  // give up the seat, keep watching
)

// Phases (OffPhase).
const (
	PhWait = iota
	PhShop
	PhDefense
	PhAim
	PhShot
	PhRoundEnd
	PhGameOver
)

// Play modes, walls, skies, turn orders, talk modes (settings values).
const (
	ModeSequential = iota
	ModeSimultaneous
	ModeSynchronous
)

const (
	WallNone = iota
	WallWrap
	WallPadded
	WallRubber
	WallSpring
	WallConcrete
	WallRandom
	WallErratic
)

const (
	SkyPlain = iota
	SkyGradient
	SkyStars
	SkyStorm
	SkySunset
	SkyCavern
	SkyBlack
	SkyRandom
)

const (
	OrderRandom = iota
	OrderLosers
	OrderWinners
	OrderRobin
)

const (
	TalkOff = iota
	TalkBots
	TalkAll
)

// Seat kinds (P_KIND) and bot tiers (P_TIER).
const (
	KindEmpty = iota
	KindHuman
	KindBot
)

const (
	TierRookie = iota
	TierGunner
	TierRicochet
	TierLobber
	TierTactician
	TierSniper
	TierDeadeye
	TierWildcard
	NTiers
)

// Items (inventory index, P_WEAPON, ActUse...): 33 weapons, then 15 others.
const (
	IMiniMissile = iota
	IMissile
	IMiniNuke
	INuke
	IHopper
	IClusterBomb
	IMIRV
	IMegaMIRV
	INapalm
	IInferno
	ITracer
	ISmokeTracer
	ILightRoller
	IRoller
	IHeavyRoller
	IShovelCharge
	IShovelBlast
	IDirtBuster
	IHeavyBuster
	ILightDrill
	IDrill
	IHeavyDrill
	ILightMole
	IMole
	IHeavyMole
	IDirtClump
	IDirtHeap
	ILandslide
	IMudslide
	IDirtGeyser
	IQuake
	IPlasmaBurst
	ILaser
	IHeatSeeker // the misc items
	IBallistic
	ILevelSeeker
	IDropSeeker
	IAutopilot
	IFuel
	IParachute
	IBattery
	IDeflector
	IShield
	IBounceShield
	IHeavyShield
	ISuperDeflector
	IDefenseComputer
	IImpactFuse
	NItems
	NWeapons = IHeatSeeker
	INone    = 0xFF
)

// Settings (OffSettings + id: the value's index; the CATALOG says what
// each means).
const (
	SetSky = iota
	SetBumpiness
	SetSlope
	SetFlatten
	SetRandomLand
	SetMaxWind
	SetChangeWind
	SetGravity
	SetViscosity
	SetSuspend
	SetTanksFall
	SetImpactDamage
	SetTunnel
	SetScale
	SetBorders
	SetWalls
	SetTanks
	SetRounds
	SetMode
	SetOrder
	SetTimer
	SetTalk
	SetTalkPct
	SetArms
	SetCash
	SetInterest
	SetBotsBuy
	NSettings
)

// STATE layout: the header, the settings, ten seats, then this client's
// own cash and inventory.
const (
	OffPhase   = 0  // Ph*
	OffRound   = 1  // 1-based, 0: none yet
	OffRounds  = 2  // in this game
	OffHost    = 3  // seat that edits the settings and starts (NoSeat)
	OffTurn    = 4  // seat aiming (NoSeat: everyone, or nobody)
	OffTimer   = 5  // seconds left (aim, shop, defence, results)
	OffMode    = 6  // Mode*
	OffTanks   = 7  // tanks in this game
	OffWind    = 8  // int16 LE, the settings' wind units (+: to the right)
	OffWall    = 10 // Wall*, resolved (never Random/Erratic)
	OffSky     = 11 // Sky*, resolved
	OffTV      = 12 // terrain version this STATE goes with
	OffSID     = 13 // the script id last sent (0: none yet)
	OffEpoch   = 14 // +1 each game
	OffSeq     = 15 // +1 each round: the client redraws the field
	OffSpect   = 16 // watchers
	OffResult  = 17 // round or game winner's seat, or NoSeat
	OffMySeat  = 18 // this client's seat, or NoSeat
	OffPalette = 19 // the round's dirt palette, 0-7
	OffCatLen  = 20 // uint16 LE: CATALOG length
	OffCatSum  = 22 // its Fletcher-16 (s1, s2)

	OffSettings = 32
	OffSeats    = OffSettings + 40
	SeatSize    = 25
	OffCash     = OffSeats + Seats*SeatSize // uint32 LE
	OffFuel     = OffCash + 4               // uint16 LE, fuel units
	OffGuide    = OffFuel + 2               // guidance item selected (INone)
	OffTarget   = OffGuide + 1              // its target seat (NoSeat)
	OffInv      = OffTarget + 1             // NItems counts (Mini Missile: 99)
	StateSize   = OffInv + NItems
)

// A seat's record (OffSeats + seat*SeatSize + P*).
const (
	PName     = 0  // NameLen, NUL padded
	PColour   = 8  // 0-9: the client's palette
	PKind     = 9  // Kind*
	PTier     = 10 // Tier* (bots)
	PEnergy   = 11 // 0-100
	PShield   = 12 // the shield up (item), or INone
	PShieldHP = 13
	PAngle    = 14 // degrees, 0 right, 90 up, 180 left
	PPower    = 15 // uint16 LE, 0-1000
	PX        = 17 // uint16 LE: the tank's bottom centre, client px
	PY        = 19 // the row under the tank, client px
	PWeapon   = 20 // item
	PFlags    = 21 // PF*
	PWins     = 22
	PScore    = 23 // uint16 LE, in tens
)

const (
	PFAlive   = 0x01
	PFConn    = 0x02 // a human's link is up
	PFChute   = 0x04 // parachute armed
	PFFuse    = 0x08 // impact fuse armed
	PFDone    = 0x10 // done (shop, defence window) or fired (simultaneous)
	PFFiring  = 0x20 // this seat's shot is in the script playing
	PFDefense = 0x40 // owns a Defense Computer: the window is for it
)

// TERRAIN column entries (CS_TERRAIN, SETTLE): one byte 0..MaxFieldH-1 is a
// plain column, solid from that row to the bottom; TEmpty no solid at all;
// TSpans+n (n 1-15) is n spans of (top, end), end exclusive.
const (
	TEmpty = 0xF0
	TSpans = 0xF0
)

// SCRIPT ops (docs/PROTOCOL.md). Coordinates are client pixels, x and y
// int16 LE (y can be above the field: negative).
const (
	OpEnd     = 0x00 // the segment ends
	OpWait    = 0x01 // n: let n frames pass (0: 256)
	OpArc     = 0x10 // slot, style, x, y, vx24, vy24, ax16, ay16, frames16
	OpPath    = 0x11 // slot (0-3), style, x16, y16, n, n steps (dx<<4 | dy&15, signed nibbles): one a frame
	OpStop    = 0x12 // slot: the track ends now
	OpExplode = 0x20 // x, y, rx, ry, kind, frames
	OpDirt    = 0x21 // x, y, rx, ry, frames: dirt fills the air in the ellipse
	OpTunnel  = 0x22 // x0, y0, x1, y1, width: a channel cut through dirt
	OpBeam    = 0x23 // x0, y0, x1, y1, kind, frames: laser / plasma beam
	OpFlow    = 0x24 // kind, frames, hold, n, n cells (x16, y8): they appear over frames; hold frames later a fire goes out
	OpSettle  = 0x25 // x16, n, frames, n column entries: columns fall to these
	OpTalk    = 0x30 // seat, len, text
	OpSound   = 0x31 // id, size
	OpShield  = 0x32 // seat, event (Sh*), hp left
	OpDamage  = 0x33 // seat, amount, energy left
	OpFall    = 0x34 // seat, x, y, frames, chute: the tank moves there
	OpDeath   = 0x35 // seat, effect
	OpTank    = 0x36 // seat, x, y, angle: placed (a drive, a new round)
	OpWind    = 0x37 // int16: the wind now
	OpShake   = 0x38 // frames
	OpFlash   = 0x39 // colour, frames
	OpFire    = 0x3A // seat, angle, power16, weapon: the shot leaves the barrel
)

// Track styles (OpArc/OpPath).
const (
	StShell   = 0 // a shell: drawn as it moves, gone after
	StTracer  = 1 // leaves nothing
	StSmoke   = 2 // leaves a trail in the shooter's colour (style | seat<<4)
	StRoller  = 3 // rolling on the surface
	StDigger  = 4 // under the ground: cuts as it goes
	StBomblet = 5
)

// Explosion kinds (OpExplode).
const (
	ExFire   = 0 // fireball, crater
	ExNuke   = 1 // big fireball, flash, crater
	ExClear  = 2 // dirt blown away, no fire (shovel, buster)
	ExPlasma = 3 // energy sphere, no crater
	ExPuff   = 4 // a small flash, no crater (a shield took it)
	ExDeath  = 5 // a tank going up
)

// Flow kinds (OpFlow).
const (
	FlowFire = 0
	FlowMud  = 1
)

// Shield events (OpShield).
const (
	ShHit = iota
	ShBreak
	ShDeflect
	ShUp
	ShLift
)

// Sound ids (OpSound): what happens, each platform's own sound for it.
const (
	SndFire = iota
	SndBoom
	SndNuke
	SndNapalm
	SndLaser
	SndPlasma
	SndDig
	SndRoll
	SndDirt
	SndShield
	SndDeflect
	SndChute
	SndDeath
	SndBounce
	SndThunder
	SndTick
)

// Death effects (OpDeath).
const (
	DeathFade = iota
	DeathBlast
	DeathDouble
	DeathTriple
	DeathRays
	DeathSpray
	DeathPlume
	DeathScatter
	NDeaths
)

// The constants, in src/proto.h's order.
var cdefs = []struct {
	name string
	val  int
}{
	{"PROTO_VERSION", Version}, {"MAX_FRAME", MaxFrame}, {"STREAM_PORT", Port},
	{"NAME_LEN", NameLen}, {"ROOM_LEN", RoomLen}, {"SEATS", Seats}, {"NO_SEAT", NoSeat},
	{"WORLD_W", WorldW}, {"WORLD_H", WorldH}, {"ROOM_REC", RoomRec}, {"HELLO_LEN", HelloLen},
	{"MAX_TALK", MaxTalk},
	{"HELLO_WATCH", HelloWatch}, {"CAP_HOLD", CapHold}, {"PLAT_ATARI", PlatAtari}, {"PLAT_INTV", PlatIntv},
	{"SC_HELLO", SCHello}, {"SC_ROOMS", SCRooms}, {"SC_ACT", SCAct}, {"SC_DONE", SCDone},
	{"SC_WANT", SCWant}, {"SC_RESYNC", SCResync}, {"SC_CREDIT", SCCredit}, {"SC_PING", SCPing},
	{"SC_BYE", SCBye},
	{"CS_PATCH", CSPatch}, {"CS_COMMIT", CSCommit}, {"CS_ROOM", CSRoom}, {"CS_ERROR", CSError},
	{"CS_CATALOG", CSCatalog}, {"CS_TERRAIN", CSTerrain}, {"CS_SCRIPT", CSScript}, {"CS_HOLD", CSHold},
	{"G_LAST", GLast}, {"S_LAST_FRAME", SLastFrame}, {"S_LAST_SEG", SLastSeg}, {"S_HOLD", SHold},
	{"HOLD_SHOP", HoldShop}, {"HOLD_MENU", HoldMenu},
	{"ERR_NO_ROOM", ErrNoRoom}, {"ERR_VERSION", ErrVersion}, {"ERR_FULL", ErrFull}, {"ERR_CAPS", ErrCaps},
	{"ACT_AIM", ActAim}, {"ACT_WEAPON", ActWeapon}, {"ACT_FIRE", ActFire}, {"ACT_USE", ActUse},
	{"ACT_DRIVE", ActDrive}, {"ACT_BUY", ActBuy}, {"ACT_SELL", ActSell}, {"ACT_DONE", ActDone},
	{"ACT_SET", ActSet}, {"ACT_START", ActStart}, {"ACT_LEAVE", ActLeave},
	{"PH_WAIT", PhWait}, {"PH_SHOP", PhShop}, {"PH_DEFENSE", PhDefense}, {"PH_AIM", PhAim},
	{"PH_SHOT", PhShot}, {"PH_ROUNDEND", PhRoundEnd}, {"PH_GAMEOVER", PhGameOver},
	{"MODE_SEQUENTIAL", ModeSequential}, {"MODE_SIMULTANEOUS", ModeSimultaneous}, {"MODE_SYNCHRONOUS", ModeSynchronous},
	{"WALL_NONE", WallNone}, {"WALL_WRAP", WallWrap}, {"WALL_PADDED", WallPadded}, {"WALL_RUBBER", WallRubber},
	{"WALL_SPRING", WallSpring}, {"WALL_CONCRETE", WallConcrete}, {"WALL_RANDOM", WallRandom}, {"WALL_ERRATIC", WallErratic},
	{"SKY_PLAIN", SkyPlain}, {"SKY_GRADIENT", SkyGradient}, {"SKY_STARS", SkyStars}, {"SKY_STORM", SkyStorm},
	{"SKY_SUNSET", SkySunset}, {"SKY_CAVERN", SkyCavern}, {"SKY_BLACK", SkyBlack}, {"SKY_RANDOM", SkyRandom},
	{"KIND_EMPTY", KindEmpty}, {"KIND_HUMAN", KindHuman}, {"KIND_BOT", KindBot}, {"N_TIERS", NTiers},
	{"N_ITEMS", NItems}, {"N_WEAPONS", NWeapons}, {"I_NONE", INone},
	{"I_MINI_MISSILE", IMiniMissile}, {"I_HEAT_SEEKER", IHeatSeeker}, {"I_AUTOPILOT", IAutopilot},
	{"I_FUEL", IFuel}, {"I_PARACHUTE", IParachute}, {"I_BATTERY", IBattery}, {"I_DEFLECTOR", IDeflector},
	{"I_SHIELD", IShield}, {"I_BOUNCE_SHIELD", IBounceShield}, {"I_HEAVY_SHIELD", IHeavyShield},
	{"I_SUPER_DEFLECTOR", ISuperDeflector}, {"I_DEFENSE_COMPUTER", IDefenseComputer}, {"I_IMPACT_FUSE", IImpactFuse},
	{"N_SETTINGS", NSettings},
	{"SET_SKY", SetSky}, {"SET_BUMPINESS", SetBumpiness}, {"SET_SLOPE", SetSlope}, {"SET_FLATTEN", SetFlatten},
	{"SET_RANDOM_LAND", SetRandomLand}, {"SET_MAX_WIND", SetMaxWind}, {"SET_CHANGE_WIND", SetChangeWind},
	{"SET_GRAVITY", SetGravity}, {"SET_VISCOSITY", SetViscosity}, {"SET_SUSPEND", SetSuspend},
	{"SET_TANKS_FALL", SetTanksFall}, {"SET_IMPACT_DAMAGE", SetImpactDamage}, {"SET_TUNNEL", SetTunnel},
	{"SET_SCALE", SetScale}, {"SET_BORDERS", SetBorders}, {"SET_WALLS", SetWalls}, {"SET_TANKS", SetTanks},
	{"SET_ROUNDS", SetRounds}, {"SET_MODE", SetMode}, {"SET_ORDER", SetOrder}, {"SET_TIMER", SetTimer},
	{"SET_TALK", SetTalk}, {"SET_TALK_PCT", SetTalkPct}, {"SET_ARMS", SetArms}, {"SET_CASH", SetCash},
	{"SET_INTEREST", SetInterest}, {"SET_BOTS_BUY", SetBotsBuy},
	{"OFF_PHASE", OffPhase}, {"OFF_ROUND", OffRound}, {"OFF_ROUNDS", OffRounds}, {"OFF_HOST", OffHost},
	{"OFF_TURN", OffTurn}, {"OFF_TIMER", OffTimer}, {"OFF_MODE", OffMode}, {"OFF_TANKS", OffTanks},
	{"OFF_WIND", OffWind}, {"OFF_WALL", OffWall}, {"OFF_SKY", OffSky}, {"OFF_TV", OffTV}, {"OFF_SID", OffSID},
	{"OFF_EPOCH", OffEpoch}, {"OFF_SEQ", OffSeq}, {"OFF_SPECT", OffSpect}, {"OFF_RESULT", OffResult},
	{"OFF_MY_SEAT", OffMySeat}, {"OFF_PALETTE", OffPalette}, {"OFF_CAT_LEN", OffCatLen}, {"OFF_CAT_SUM", OffCatSum},
	{"OFF_SETTINGS", OffSettings}, {"OFF_SEATS", OffSeats}, {"SEAT_SIZE", SeatSize},
	{"OFF_CASH", OffCash}, {"OFF_FUEL", OffFuel}, {"OFF_GUIDE", OffGuide}, {"OFF_TARGET", OffTarget},
	{"OFF_INV", OffInv}, {"STATE_SIZE", StateSize},
	{"P_NAME", PName}, {"P_COLOUR", PColour}, {"P_KIND", PKind}, {"P_TIER", PTier}, {"P_ENERGY", PEnergy},
	{"P_SHIELD", PShield}, {"P_SHIELD_HP", PShieldHP}, {"P_ANGLE", PAngle}, {"P_POWER", PPower},
	{"P_X", PX}, {"P_Y", PY}, {"P_WEAPON", PWeapon}, {"P_FLAGS", PFlags}, {"P_WINS", PWins}, {"P_SCORE", PScore},
	{"PF_ALIVE", PFAlive}, {"PF_CONN", PFConn}, {"PF_CHUTE", PFChute}, {"PF_FUSE", PFFuse},
	{"PF_DONE", PFDone}, {"PF_FIRING", PFFiring}, {"PF_DEFENSE", PFDefense},
	{"T_EMPTY", TEmpty}, {"T_SPANS", TSpans},
	{"OP_END", OpEnd}, {"OP_WAIT", OpWait}, {"OP_ARC", OpArc}, {"OP_PATH", OpPath}, {"OP_STOP", OpStop},
	{"OP_EXPLODE", OpExplode}, {"OP_DIRT", OpDirt}, {"OP_TUNNEL", OpTunnel}, {"OP_BEAM", OpBeam},
	{"OP_FLOW", OpFlow}, {"OP_SETTLE", OpSettle}, {"OP_TALK", OpTalk}, {"OP_SOUND", OpSound},
	{"OP_SHIELD", OpShield}, {"OP_DAMAGE", OpDamage}, {"OP_FALL", OpFall}, {"OP_DEATH", OpDeath},
	{"OP_TANK", OpTank}, {"OP_WIND", OpWind}, {"OP_SHAKE", OpShake}, {"OP_FLASH", OpFlash}, {"OP_FIRE", OpFire},
	{"ST_SHELL", StShell}, {"ST_TRACER", StTracer}, {"ST_SMOKE", StSmoke}, {"ST_ROLLER", StRoller},
	{"ST_DIGGER", StDigger}, {"ST_BOMBLET", StBomblet},
	{"EX_FIRE", ExFire}, {"EX_NUKE", ExNuke}, {"EX_CLEAR", ExClear}, {"EX_PLASMA", ExPlasma},
	{"EX_PUFF", ExPuff}, {"EX_DEATH", ExDeath}, {"FLOW_FIRE", FlowFire}, {"FLOW_MUD", FlowMud},
	{"DEATH_FADE", DeathFade}, {"DEATH_BLAST", DeathBlast}, {"DEATH_DOUBLE", DeathDouble}, {"DEATH_TRIPLE", DeathTriple},
	{"DEATH_RAYS", DeathRays}, {"DEATH_SPRAY", DeathSpray}, {"DEATH_PLUME", DeathPlume}, {"DEATH_SCATTER", DeathScatter},
	{"SH_HIT", ShHit}, {"SH_BREAK", ShBreak}, {"SH_DEFLECT", ShDeflect}, {"SH_UP", ShUp}, {"SH_LIFT", ShLift},
	{"SND_FIRE", SndFire}, {"SND_BOOM", SndBoom}, {"SND_NUKE", SndNuke}, {"SND_NAPALM", SndNapalm},
	{"SND_LASER", SndLaser}, {"SND_PLASMA", SndPlasma}, {"SND_DIG", SndDig}, {"SND_ROLL", SndRoll},
	{"SND_DIRT", SndDirt}, {"SND_SHIELD", SndShield}, {"SND_DEFLECT", SndDeflect}, {"SND_CHUTE", SndChute},
	{"SND_DEATH", SndDeath}, {"SND_BOUNCE", SndBounce}, {"SND_THUNDER", SndThunder}, {"SND_TICK", SndTick},
}

// CHeader writes src/proto.h.
func CHeader(w io.Writer) {
	fmt.Fprintln(w, "/* proto.h -- generated by `barraged -cheader` (make proto); do not edit. */")
	fmt.Fprintln(w, "#ifndef PROTO_H\n#define PROTO_H")
	for _, d := range cdefs {
		fmt.Fprintf(w, "#define %-18s %d\n", d.name, d.val)
	}
	fmt.Fprintln(w, "#endif")
}
