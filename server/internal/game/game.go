// Package game is one room: up to ten seats (people and bots), the host's
// settings, the shop, the rounds and turns, and each shot (internal/sim).
// It runs on jiffies (60 a second); the server advances it to the wall
// clock and builds each client's STATE from it.
package game

import (
	"fmt"
	"log"
	"math"
	"math/rand/v2"
	"sort"
	"strings"

	"fujibarrage/internal/bot"
	"fujibarrage/internal/items"
	"fujibarrage/internal/project"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/sim"
	"fujibarrage/internal/talk"
	"fujibarrage/internal/world"
)

// Jiffies.
const (
	Hz         = 60
	ThinkJ     = 80 // a bot's turn before it fires: its turret swings
	ShopJ      = 45 * Hz
	DefenseJ   = 10 * Hz
	RoundEndJ  = 6 * Hz
	GameOverJ  = 15 * Hz
	WaitGoneJ  = 10 * Hz // a person's link down in the waiting room: seat freed
	GoneJ      = 30 * Hz // ... during a game: a bot takes the tank
	ShotSlackJ = 4 * Hz  // past a script's length, before we stop waiting
	NoTimerJ   = 120 * Hz
)

// Seat is one of the ten places at the table.
type Seat struct {
	Kind   int // proto.Kind*
	Name   string
	Tier   int
	Conn   bool
	goneJ  int64
	Cash   int
	Inv    [proto.NItems]int
	Wins   int
	Score  int
	Angle  int
	Power  int
	Weapon int
	Guide  int
	Target int
	Fuse   bool
	Chute  bool
	Done   bool
	Played byte // the last shot this person finished watching
	aim    *sim.Shot
	fireJ  int64 // a bot fires then
	plan   *sim.Shot
	mem    bot.Memory
}

type snap struct {
	alive          bool
	x, y           float64
	energy, shield float64
	shieldItem     int
	angle          int
}

// Shot is the last shot (or volley) resolved: the clients play its script.
type Shot struct {
	SID           byte
	Log           *sim.Log
	Pre, Post     *world.Terrain
	PreTV, PostTV byte
	before        [proto.Seats]snap
	scripts       map[string][][]byte
}

// Script is the shot's segments for a client of caps (made once per kind
// of client).
func (s *Shot) Script(c project.Caps) [][]byte {
	k := c.Key()
	if b, ok := s.scripts[k]; ok {
		return b
	}
	b := project.Script(s.Log, s.Pre, c)
	s.scripts[k] = b
	return b
}

// Game is one room.
type Game struct {
	Rng    *rand.Rand
	Talk   *talk.Book
	Set    [proto.NSettings]int
	Phase  int
	Round  int
	Rounds int
	Host   int
	Turn   int
	DeadJ  int64
	J      int64
	P      [proto.Seats]Seat
	Sim    *sim.Sim
	TV     byte
	SID    byte
	Shot   *Shot
	Epoch  byte
	Seq    byte
	Spect  int
	Result int
	Wall   int // resolved
	Sky    int
	Wind   float64
	Pal    int
	Dev    bool // a dev room: DevKit and DevTier apply

	order   []int // turn order this round
	turnIdx int
	queue   []string // people waiting to take over a bot
	names   []string
}

// New makes a room with these settings.
func New(seed uint64, set [proto.NSettings]int, book *talk.Book) *Game {
	g := &Game{Rng: rand.New(rand.NewPCG(seed, seed^0x9E3779B97F4A7C15)), Talk: book, Set: set,
		Host: proto.NoSeat, Turn: proto.NoSeat, Result: proto.NoSeat}
	if g.Talk == nil {
		g.Talk = talk.Default()
	}
	g.names = append([]string(nil), botNames...)
	return g
}

// Defaults are the settings' defaults.
func Defaults() (s [proto.NSettings]int) {
	for i, sp := range Specs {
		s[i] = sp.Def
	}
	return
}

var botNames = []string{"RUSTY", "BOLT", "SPARKY", "GIZMO", "DOZER", "CLANK", "PISTON", "WIDGET",
	"ROVER", "TURBO", "BLITZ", "BOOMER", "SPROCKET", "GASKET", "RIVET", "TREAD", "HOWITZ", "MORTY",
	"KABOOM", "ZAPPER", "FLINT", "COBALT", "NITRO", "BUZZ"}

func (g *Game) num(k int) float64 { return Specs[k].Num(g.Set[k]) }

// ---- seats ------------------------------------------------------------------

// SeatOf is the seat of name (people and bots), or -1.
func (g *Game) SeatOf(name string) int {
	for i := range g.P {
		if g.P[i].Kind != proto.KindEmpty && strings.EqualFold(g.P[i].Name, name) {
			return i
		}
	}
	return -1
}

// Join seats a person by name: their own seat again, an empty one in the
// waiting room, or -1 (watch; in a game they take over a bot next round).
func (g *Game) Join(name string) int {
	if s := g.SeatOf(name); s >= 0 {
		if g.P[s].Kind == proto.KindBot {
			return -1 // a bot's name: watch
		}
		g.P[s].Conn = true
		return s
	}
	if g.Phase != proto.PhWait {
		for _, q := range g.queue {
			if q == name {
				return -1
			}
		}
		g.queue = append(g.queue, name)
		return -1
	}
	for i := range g.P {
		if g.P[i].Kind == proto.KindEmpty {
			g.P[i] = Seat{Kind: proto.KindHuman, Name: name, Conn: true, Played: g.SID}
			g.fixHost()
			return i
		}
	}
	return -1
}

// SetConn notes a person's link going up or down.
func (g *Game) SetConn(seat int, up bool) {
	if seat < 0 || g.P[seat].Kind != proto.KindHuman {
		return
	}
	g.P[seat].Conn = up
	if !up {
		g.P[seat].goneJ = g.J
	}
}

// Unqueue forgets a watcher waiting for a seat.
func (g *Game) Unqueue(name string) {
	for i, q := range g.queue {
		if q == name {
			g.queue = append(g.queue[:i], g.queue[i+1:]...)
			return
		}
	}
}

func (g *Game) fixHost() {
	g.Host = proto.NoSeat
	for i := range g.P {
		if g.P[i].Kind == proto.KindHuman {
			g.Host = i
			return
		}
	}
}

func (g *Game) humans() int {
	n := 0
	for i := range g.P {
		if g.P[i].Kind == proto.KindHuman {
			n++
		}
	}
	return n
}

// Humans is how many people are seated (the lobby's count).
func (g *Game) Humans() int { return g.humans() }

// leave turns a person's seat over: freed in the waiting room, a bot's
// in a game.
func (g *Game) leave(seat int) {
	if seat < 0 || g.P[seat].Kind != proto.KindHuman {
		return
	}
	if g.Phase == proto.PhWait {
		g.P[seat] = Seat{}
	} else {
		p := &g.P[seat]
		p.Kind, p.Tier, p.Conn = proto.KindBot, g.botTier(), false
		p.Name = g.botName()
		g.newBotRound(seat)
	}
	g.fixHost()
	if g.humans() == 0 && g.Phase != proto.PhWait {
		g.reset()
	}
}

// DevKit gives people a bundle of everything at the start (the dev rooms:
// test rigs, trying weapons).
var DevKit bool

// DevTier, 0 or more, makes every dev room's bot that tier (test rigs: Rookies buy
// and fire anything); -1, the usual random ones.
var DevTier = -1

func (g *Game) botTier() int {
	if DevTier >= 0 && g.Dev {
		return DevTier
	}
	return g.Rng.IntN(proto.NTiers)
}

func (g *Game) botName() string {
	for k := 0; k < 100; k++ {
		n := g.names[g.Rng.IntN(len(g.names))]
		if g.SeatOf(n) < 0 {
			return n
		}
	}
	return "BOT"
}

// reset: back to the waiting room, the bots gone.
func (g *Game) reset() {
	for i := range g.P {
		if g.P[i].Kind == proto.KindBot {
			g.P[i] = Seat{}
		}
		if g.P[i].Kind == proto.KindHuman {
			p := &g.P[i]
			*p = Seat{Kind: proto.KindHuman, Name: p.Name, Conn: p.Conn, goneJ: p.goneJ, Played: g.SID}
		}
	}
	g.Phase, g.Round, g.Turn, g.Result = proto.PhWait, 0, proto.NoSeat, proto.NoSeat
	g.Sim, g.Shot = nil, nil
	g.queue = nil
	g.fixHost()
}

// ---- time ---------------------------------------------------------------------

// Advance runs n jiffies.
func (g *Game) Advance(n int) {
	for ; n > 0; n-- {
		g.J++
		g.tick()
	}
}

func (g *Game) tick() {
	for i := range g.P {
		p := &g.P[i]
		if p.Kind != proto.KindHuman || p.Conn {
			continue
		}
		if g.Phase == proto.PhWait && g.J-p.goneJ > WaitGoneJ {
			g.P[i] = Seat{}
			g.fixHost()
		} else if g.Phase != proto.PhWait && g.J-p.goneJ > GoneJ {
			g.leave(i)
		}
	}
	switch g.Phase {
	case proto.PhShop:
		if g.J >= g.DeadJ || g.allDone() {
			g.endShop()
		}
	case proto.PhDefense:
		if g.J >= g.DeadJ || g.allDone() {
			g.endDefense()
		}
	case proto.PhAim:
		g.aimTick()
	case proto.PhShot:
		if g.J >= g.DeadJ || g.allPlayed() {
			g.afterShot()
		}
	case proto.PhRoundEnd:
		if g.J >= g.DeadJ {
			if g.Round >= g.Rounds {
				g.gameOver()
			} else {
				g.enterShop()
			}
		}
	case proto.PhGameOver:
		if g.J >= g.DeadJ {
			g.reset()
		}
	}
}

func (g *Game) allDone() bool {
	for i := range g.P {
		p := &g.P[i]
		if p.Kind == proto.KindHuman && p.Conn && !p.Done {
			return false
		}
	}
	return true
}

func (g *Game) allPlayed() bool {
	for i := range g.P {
		p := &g.P[i]
		if p.Kind == proto.KindHuman && p.Conn && p.Played != g.SID {
			return false
		}
	}
	return true
}

// Played notes that seat's person finished watching shot sid.
// nextSID: shot ids go round 1-255; 0 means "no shot" (a W hold's D).
func nextSID(sid byte) byte {
	if sid == 255 {
		return 1
	}
	return sid + 1
}

// nextTV: terrain versions go round 0-254; 255 is a TERRAIN frame's "no
// base: the whole field".
func nextTV(tv byte) byte {
	if tv >= 254 {
		return 0
	}
	return tv + 1
}

func (g *Game) Played(seat int, sid byte) {
	if seat >= 0 && g.P[seat].Kind == proto.KindHuman {
		g.P[seat].Played = sid
	}
}

// ---- the game's course --------------------------------------------------------

func (g *Game) start() {
	n := Specs[proto.SetTanks].Min + g.Set[proto.SetTanks]
	for i := range g.P {
		if g.count() >= n {
			break
		}
		if g.P[i].Kind == proto.KindEmpty {
			g.P[i] = Seat{Kind: proto.KindBot, Tier: g.botTier(), Name: g.botName()}
		}
	}
	cash := int(g.num(proto.SetCash))
	for i := range g.P {
		p := &g.P[i]
		if p.Kind == proto.KindEmpty {
			continue
		}
		p.Cash, p.Wins, p.Score, p.Inv = cash, 0, 0, [proto.NItems]int{}
		if DevKit && g.Dev && p.Kind == proto.KindHuman {
			for it := range p.Inv {
				if it != items.Unlimited {
					p.Inv[it] = items.All[it].Bundle
				}
			}
		}
		p.Weapon, p.Angle, p.Power = items.Unlimited, 90, 200
		p.Chute, p.Fuse = true, false
	}
	g.Round, g.Rounds = 0, Specs[proto.SetRounds].Min+g.Set[proto.SetRounds]
	g.Epoch++
	g.enterShop()
}

func (g *Game) count() int {
	n := 0
	for i := range g.P {
		if g.P[i].Kind != proto.KindEmpty {
			n++
		}
	}
	return n
}

func (g *Game) enterShop() {
	g.Phase, g.Turn = proto.PhShop, proto.NoSeat
	g.DeadJ = g.J + ShopJ
	for i := range g.P {
		p := &g.P[i]
		p.Done = p.Kind != proto.KindHuman
		if p.Kind == proto.KindBot && g.Set[proto.SetBotsBuy] == 1 {
			bot.Shop(p.Tier, &p.Cash, &p.Inv, g.Set[proto.SetArms], g.Rng)
		}
	}
}

func (g *Game) endShop() {
	rate := g.num(proto.SetInterest) / 100
	for i := range g.P {
		p := &g.P[i]
		if p.Kind != proto.KindEmpty {
			p.Cash += int(float64(p.Cash) * rate)
		}
	}
	g.startRound()
}

func (g *Game) startRound() {
	// people waiting take over bots
	for _, name := range g.queue {
		var bots []int
		for i := range g.P {
			if g.P[i].Kind == proto.KindBot {
				bots = append(bots, i)
			}
		}
		if len(bots) == 0 {
			break
		}
		s := bots[g.Rng.IntN(len(bots))]
		p := &g.P[s]
		p.Kind, p.Name, p.Conn, p.Played = proto.KindHuman, name, true, g.SID
	}
	g.queue = nil
	g.fixHost()

	g.Round++
	g.Seq++
	g.Result = proto.NoSeat
	g.Sky = g.Set[proto.SetSky]
	if g.Sky == proto.SkyRandom {
		g.Sky = g.Rng.IntN(proto.SkyRandom)
	}
	g.Wall = g.Set[proto.SetWalls]
	if g.Wall >= proto.WallRandom {
		g.Wall = g.Rng.IntN(proto.WallRandom)
	}
	g.Pal = g.Rng.IntN(8)
	g.newWind()
	par := world.Params{Bumpiness: int(g.num(proto.SetBumpiness)), Slope: int(g.num(proto.SetSlope)),
		Flatten: g.Set[proto.SetFlatten] == 1, Cavern: g.Sky == proto.SkyCavern}
	if g.Set[proto.SetRandomLand] == 1 {
		par.Bumpiness, par.Slope, par.Flatten = g.Rng.IntN(100), g.Rng.IntN(100), g.Rng.IntN(2) == 1
	}
	s := &sim.Sim{T: world.Generate(par, g.Rng, g.Round%2 == 1), Rng: g.Rng}
	s.LastWords = func(seat int) string {
		return g.Talk.LastWords(g.talkRules(), g.P[seat].Kind == proto.KindBot, g.Rng)
	}
	var seats []int
	for i := range g.P {
		p := &g.P[i]
		if p.Kind == proto.KindEmpty {
			continue
		}
		seats = append(seats, i)
		s.Tanks[i] = &sim.Tank{Seat: i, Energy: 100, Alive: true, Shield: proto.INone,
			Chute: p.Chute, Chutes: p.Inv[proto.IParachute], Batteries: p.Inv[proto.IBattery],
			Angle: 90, Bot: p.Kind == proto.KindBot}
		p.Angle, p.Power, p.Weapon, p.Guide, p.Target = 90, 200, items.Unlimited, proto.INone, proto.NoSeat
		if p.Kind == proto.KindBot {
			g.newBotRound(i)
		}
	}
	g.Rng.Shuffle(len(seats), func(i, j int) { seats[i], seats[j] = seats[j], seats[i] })
	s.Place(seats)
	g.Sim = s
	g.physics()
	g.TV = nextTV(g.TV)
	g.Shot = nil
	g.order = g.turnOrder(seats)
	g.turnIdx = -1
	for i := range g.P {
		g.P[i].Played = g.SID
	}
	// a Defense Computer's owners raise their defences before any shot
	any := false
	for _, i := range seats {
		if g.P[i].Inv[proto.IDefenseComputer] > 0 {
			any = true
		}
	}
	if any {
		g.Phase, g.Turn = proto.PhDefense, proto.NoSeat
		g.DeadJ = g.J + DefenseJ
		for _, i := range seats {
			p := &g.P[i]
			p.Done = p.Kind != proto.KindHuman || p.Inv[proto.IDefenseComputer] == 0
		}
		return
	}
	g.beginAim()
}

func (g *Game) talkRules() talk.Rules {
	return talk.Rules{Mode: g.Set[proto.SetTalk], Pct: int(g.num(proto.SetTalkPct))}
}

func (g *Game) newBotRound(seat int) {
	p := &g.P[seat]
	b := &bot.Seat{Seat: seat, Tier: p.Tier, Inv: &p.Inv, Mem: &p.mem}
	bot.NewRound(b, g.Rng)
}

func (g *Game) newWind() {
	m := g.num(proto.SetMaxWind)
	w := 0.0
	if m > 0 {
		w = g.Rng.Float64()*m/2 - m/4
		if g.Rng.IntN(5) == 0 {
			w *= 2
			if g.Rng.IntN(5) < 2 {
				w *= 2
			}
		}
	}
	g.Wind = math.Round(w)
}

// physics copies the settings into the simulator.
func (g *Game) physics() {
	s := g.Sim
	s.S = sim.Settings{Gravity: g.num(proto.SetGravity), Wind: g.Wind, Viscosity: g.Set[proto.SetViscosity],
		Wall: g.Wall, Ceiling: g.Sky == proto.SkyCavern, Tunnel: g.Set[proto.SetTunnel] == 1,
		Suspend: int(g.num(proto.SetSuspend)), TanksFall: g.Set[proto.SetTanksFall] == 1,
		ImpactDamage: g.Set[proto.SetImpactDamage] == 1, Scale: []float64{0.5, 0.75, 1}[g.Set[proto.SetScale]],
		Borders: g.num(proto.SetBorders) * 2}
}

func (g *Game) turnOrder(seats []int) []int {
	o := append([]int(nil), seats...)
	switch g.Set[proto.SetOrder] {
	case proto.OrderLosers, proto.OrderWinners:
		sort.SliceStable(o, func(i, j int) bool {
			a, b := &g.P[o[i]], &g.P[o[j]]
			less := a.Wins < b.Wins || a.Wins == b.Wins && a.Score < b.Score
			if g.Set[proto.SetOrder] == proto.OrderWinners {
				return !less
			}
			return less
		})
	case proto.OrderRobin:
		sort.Ints(o)
		k := (g.Round - 1) % len(o)
		o = append(o[k:], o[:k]...)
	default:
		g.Rng.Shuffle(len(o), func(i, j int) { o[i], o[j] = o[j], o[i] })
	}
	return o
}

func (g *Game) endDefense() {
	for i := range g.P {
		p := &g.P[i]
		t := g.tank(i)
		if t == nil || p.Inv[proto.IDefenseComputer] == 0 {
			continue
		}
		p.Inv[proto.IDefenseComputer]--
		if t.Shield == proto.INone { // undecided: the best it has
			for _, sh := range []int{proto.ISuperDeflector, proto.IHeavyShield, proto.IBounceShield, proto.IShield, proto.IDeflector} {
				if p.Inv[sh] > 0 {
					g.raise(i, sh)
					break
				}
			}
		}
		if p.Inv[proto.IParachute] > 0 {
			p.Chute, t.Chute = true, true
		}
	}
	g.beginAim()
}

func (g *Game) tank(seat int) *sim.Tank {
	if g.Sim == nil || seat < 0 || seat >= proto.Seats {
		return nil
	}
	return g.Sim.Tanks[seat]
}

func (g *Game) alive() []int {
	var out []int
	for _, i := range g.order {
		if t := g.tank(i); t != nil && t.Alive {
			out = append(out, i)
		}
	}
	return out
}

func (g *Game) timer() int64 {
	s := timerSecs[g.Set[proto.SetTimer]]
	if s == 0 {
		return NoTimerJ
	}
	return int64(s) * Hz
}

// beginAim starts a turn (one seat) or a volley (every live seat).
func (g *Game) beginAim() {
	live := g.alive()
	if len(live) <= 1 {
		g.endRound()
		return
	}
	g.Phase = proto.PhAim
	g.DeadJ = g.J + g.timer()
	if g.Set[proto.SetMode] == proto.ModeSequential {
		for k := 1; k <= len(g.order); k++ {
			i := g.order[(g.turnIdx+k+len(g.order))%len(g.order)]
			if t := g.tank(i); t != nil && t.Alive {
				g.turnIdx = (g.turnIdx + k + len(g.order)) % len(g.order)
				g.Turn = i
				break
			}
		}
		g.prepare(g.Turn)
		return
	}
	g.Turn = proto.NoSeat
	for _, i := range live {
		g.P[i].aim = nil
		g.P[i].Done = false
		g.prepare(i)
	}
}

// prepare a seat's turn: a bot works out its shot and fires after a while.
func (g *Game) prepare(i int) {
	p := &g.P[i]
	p.plan = nil
	if t := g.tank(i); t != nil {
		p.Power = min(p.Power, int(t.Energy*10))
	}
	if p.Kind != proto.KindBot {
		return
	}
	b := &bot.Seat{Seat: i, Tier: p.Tier, Inv: &p.Inv, Mem: &p.mem}
	for k := range g.P {
		b.Scores[k] = g.P[k].Score
	}
	g.botDefend(i)
	sh := bot.Aim(g.Sim, b, g.Rng)
	p.plan = &sh
	p.Weapon = sh.Weapon
	p.fireJ = g.J + ThinkJ + int64(g.Rng.IntN(40))
}

// botDefend: the cleverer bots raise a shield and use batteries when low.
func (g *Game) botDefend(i int) {
	p := &g.P[i]
	t := g.tank(i)
	if p.mem.RoundTier < proto.TierTactician {
		return
	}
	if t.Shield == proto.INone {
		for _, sh := range []int{proto.ISuperDeflector, proto.IHeavyShield, proto.IBounceShield, proto.IShield, proto.IDeflector} {
			if p.Inv[sh] > 0 {
				g.raise(i, sh)
				break
			}
		}
	}
	for t.Energy < 70 && p.Inv[proto.IBattery] > 0 {
		g.use(i, proto.IBattery)
	}
}

func (g *Game) aimTick() {
	if g.Set[proto.SetMode] == proto.ModeSequential {
		i := g.Turn
		p := &g.P[i]
		if p.Kind == proto.KindBot && p.plan != nil {
			g.swing(i)
			if g.J >= p.fireJ {
				g.resolve([]sim.Shot{*p.plan}, false)
			}
			return
		}
		if g.J >= g.DeadJ || (p.Kind == proto.KindHuman && !p.Conn && g.J-p.goneJ > 5*Hz) {
			g.resolve([]sim.Shot{g.shotOf(i)}, false) // time's up: as aimed
		}
		return
	}
	ready := true
	for _, i := range g.alive() {
		p := &g.P[i]
		if p.Kind == proto.KindBot && p.plan != nil {
			g.swing(i)
			if g.J >= p.fireJ && p.aim == nil {
				p.aim, p.Done = p.plan, true
			}
		}
		if p.aim == nil && !(p.Kind == proto.KindHuman && !p.Conn) {
			ready = false
		}
	}
	if !ready && g.J < g.DeadJ {
		return
	}
	var shots []sim.Shot
	for _, i := range g.order {
		if t := g.tank(i); t == nil || !t.Alive {
			continue
		}
		if a := g.P[i].aim; a != nil {
			shots = append(shots, *a)
		} else if g.P[i].Conn || g.P[i].Kind == proto.KindBot {
			shots = append(shots, g.shotOf(i))
		}
	}
	g.resolve(shots, g.Set[proto.SetMode] == proto.ModeSimultaneous)
}

// swing turns a bot's turret toward its aim as its turn runs out.
func (g *Game) swing(i int) {
	p := &g.P[i]
	d := p.plan.Angle - p.Angle
	if d != 0 {
		p.Angle += max(min(d, 2), -2)
	}
	dp := p.plan.Power - p.Power
	p.Power += max(min(dp, 12), -12)
	if t := g.tank(i); t != nil {
		t.Angle = p.Angle
	}
}

// shotOf is seat's shot as it is aimed now.
func (g *Game) shotOf(i int) sim.Shot {
	p := &g.P[i]
	return sim.Shot{Seat: i, Angle: p.Angle, Power: p.Power, Weapon: p.Weapon, Guide: p.Guide, Target: p.Target, Fuse: p.Fuse}
}

// resolve fires the shots and makes the script.
func (g *Game) resolve(shots []sim.Shot, together bool) {
	s := g.Sim
	for k := range shots {
		sh := &shots[k]
		p := &g.P[sh.Seat]
		t := s.Tanks[sh.Seat]
		if t == nil || !t.Alive {
			continue
		}
		// what it fires, and what that costs
		if sh.Weapon >= proto.NWeapons || (sh.Weapon != items.Unlimited && p.Inv[sh.Weapon] <= 0) {
			sh.Weapon = items.Unlimited
		}
		if sh.Weapon != items.Unlimited {
			p.Inv[sh.Weapon]--
		}
		if !items.IsGuidance(sh.Guide) || p.Inv[sh.Guide] <= 0 || sh.Target >= proto.Seats || s.Tanks[sh.Target] == nil {
			sh.Guide = proto.INone
		} else {
			p.Inv[sh.Guide]--
		}
		if sh.Fuse {
			if p.Inv[proto.IImpactFuse] > 0 {
				p.Inv[proto.IImpactFuse]--
			} else {
				sh.Fuse = false
			}
		}
		sh.Power = max(min(sh.Power, int(t.Energy*10)), 0)
		sh.Angle = max(min(sh.Angle, 180), 0)
		if sh.Guide == proto.IBallistic {
			sh.Power = g.ballistic(*sh)
			sh.Guide = proto.INone
		}
		p.Angle, p.Power = sh.Angle, sh.Power
		sh.Talk = g.Talk.Shout(g.talkRules(), p.Kind == proto.KindBot, g.Rng)
	}
	shot := &Shot{SID: nextSID(g.SID), Pre: s.T.Clone(), PreTV: g.TV, scripts: map[string][][]byte{}}
	for i := range g.P {
		if t := s.Tanks[i]; t != nil {
			shot.before[i] = snap{t.Alive, t.X, t.Y, t.Energy, t.ShieldHP, t.Shield, g.P[i].Angle}
		}
	}
	for _, sh := range shots {
		extra := ""
		if sh.Guide != proto.INone {
			extra = fmt.Sprintf(" (%s at seat %d)", items.All[sh.Guide].Name, sh.Target)
		}
		log.Printf("round %d: %s (seat %d, %s) fires %s at %d, power %d%s", g.Round, g.P[sh.Seat].Name, sh.Seat,
			map[int]string{proto.KindHuman: "person", proto.KindBot: TierNames[g.P[sh.Seat].Tier]}[g.P[sh.Seat].Kind],
			items.All[sh.Weapon].Name, sh.Angle, sh.Power, extra)
	}
	lg := s.Resolve(shots, together)
	g.TV = nextTV(g.TV)
	g.SID = shot.SID
	shot.Log, shot.Post, shot.PostTV = lg, s.T, g.TV
	g.Shot = shot
	g.score(lg)
	g.observe(lg)
	for i := range g.P {
		p := &g.P[i]
		t := s.Tanks[i]
		if t == nil {
			continue
		}
		p.Inv[proto.IParachute] = t.Chutes
		p.Chute = t.Chute
	}
	g.Phase = proto.PhShot
	g.DeadJ = g.J + int64(math.Ceil(lg.End*Hz)) + ShotSlackJ
}

// ballistic: the Ballistic Computer finds the power that lands this angle
// on the target.
func (g *Game) ballistic(sh sim.Shot) int {
	t := g.Sim.Tanks[sh.Target]
	me := g.Sim.Tanks[sh.Seat]
	lo, hi := 10, int(me.Energy*10)
	best, bestMiss := sh.Power, math.Inf(1)
	for k := 0; k < 12 && hi-lo > 2; k++ {
		m := (lo + hi) / 2
		h := g.Sim.Trace(sim.Shot{Seat: sh.Seat, Angle: sh.Angle, Power: m, Weapon: sh.Weapon, Guide: proto.INone})
		miss := h.X - t.X
		if h.Seat == sh.Target {
			return m
		}
		if math.Abs(miss) < bestMiss {
			best, bestMiss = m, math.Abs(miss)
		}
		short := (miss < 0) == (t.X > me.X)
		if short && !h.Lost {
			lo = m
		} else {
			hi = m
		}
	}
	return best
}

// score: points for damage to others, kills; the same in cash.
func (g *Game) score(lg *sim.Log) {
	for by := 0; by < proto.Seats; by++ {
		pts := 0.0
		for to := 0; to < proto.Seats; to++ {
			if to == by {
				continue
			}
			pts += lg.Damage[by][to]*30 + lg.Shield[by][to]*2
		}
		for _, k := range lg.Kills[by] {
			if k == by {
				pts -= 1500
			} else {
				pts += 500
			}
		}
		if pts != 0 && g.P[by].Kind != proto.KindEmpty {
			g.P[by].Score += int(pts)
			g.P[by].Cash = max(g.P[by].Cash+int(pts), 0)
		}
	}
}

// observe tells the bots where their shells came down.
func (g *Game) observe(lg *sim.Log) {
	for _, e := range lg.Evs {
		if e.Op != proto.OpArc || e.Trk == nil || len(e.Trk.Pts) == 0 {
			continue
		}
		p := &g.P[e.Trk.Seat]
		if p.Kind != proto.KindBot {
			continue
		}
		last := e.Trk.Pts[len(e.Trk.Pts)-1]
		bot.Observe(&bot.Seat{Seat: e.Trk.Seat, Mem: &p.mem}, g.Sim, last.X)
	}
}

func (g *Game) afterShot() {
	if g.Set[proto.SetChangeWind] == 1 {
		m := g.num(proto.SetMaxWind)
		g.Wind = math.Max(math.Min(g.Wind+float64(g.Rng.IntN(11)-5), m), -m)
	}
	if g.Set[proto.SetWalls] == proto.WallErratic {
		g.Wall = g.Rng.IntN(proto.WallRandom)
	}
	g.physics()
	if len(g.alive()) <= 1 {
		g.endRound()
		return
	}
	g.beginAim()
}

func (g *Game) endRound() {
	g.Phase, g.Turn = proto.PhRoundEnd, proto.NoSeat
	g.DeadJ = g.J + RoundEndJ
	live := g.alive()
	for _, i := range live {
		g.P[i].Wins++
		g.P[i].Score += 5000
		g.P[i].Cash += 5000
	}
	if len(live) == 1 {
		g.Result = live[0]
	}
	for i := range g.P { // shields don't outlast the round
		if t := g.tank(i); t != nil {
			t.Shield, t.ShieldHP = proto.INone, 0
		}
	}
}

func (g *Game) gameOver() {
	g.Phase, g.Turn = proto.PhGameOver, proto.NoSeat
	g.DeadJ = g.J + GameOverJ
	g.Result = proto.NoSeat
	best := -1
	for i := range g.P {
		p := &g.P[i]
		if p.Kind == proto.KindEmpty {
			continue
		}
		if best < 0 || p.Wins > g.P[best].Wins || p.Wins == g.P[best].Wins && p.Score > g.P[best].Score {
			best = i
		}
	}
	if best >= 0 {
		g.Result = best
	}
}

// ---- what people do -----------------------------------------------------------

// Act is seat's action (proto.Act*) with its arguments.
func (g *Game) Act(seat, act int, a []byte) {
	arg := func(i int) int {
		if i < len(a) {
			return int(a[i])
		}
		return 0
	}
	if act == proto.ActLeave {
		g.leave(seat)
		return
	}
	if seat < 0 || g.P[seat].Kind != proto.KindHuman {
		return
	}
	p := &g.P[seat]
	switch act {
	case proto.ActSet:
		if g.Phase == proto.PhWait && seat == g.Host && arg(0) < proto.NSettings {
			k := arg(0)
			g.Set[k] = max(min(arg(1), Specs[k].N()-1), 0)
		}
	case proto.ActStart:
		if g.Phase == proto.PhWait && seat == g.Host {
			g.start()
		}
	case proto.ActDone:
		p.Done = true
	case proto.ActBuy:
		if g.Phase == proto.PhShop && arg(0) < proto.NItems {
			it := items.All[arg(0)]
			for n := max(arg(1), 1); n > 0; n-- {
				if it.Arms > g.Set[proto.SetArms] || p.Cash < it.Price || p.Inv[arg(0)]+it.Bundle > items.Most || arg(0) == items.Unlimited {
					break
				}
				p.Cash -= it.Price
				p.Inv[arg(0)] += it.Bundle
			}
		}
	case proto.ActSell:
		if g.Phase == proto.PhShop && arg(0) < proto.NItems && arg(0) != items.Unlimited {
			it := items.All[arg(0)]
			n := min(max(arg(1), 1), p.Inv[arg(0)])
			p.Inv[arg(0)] -= n
			p.Cash += n * it.Price * 8 / 10 / it.Bundle
		}
	case proto.ActAim:
		if g.canAim(seat) {
			p.Angle = max(min(arg(0), 180), 0)
			p.Power = max(min(arg(1)|arg(2)<<8, g.maxPower(seat)), 0)
			if t := g.tank(seat); t != nil {
				t.Angle = p.Angle
			}
		}
	case proto.ActWeapon:
		if w := arg(0); w < proto.NWeapons && (w == items.Unlimited || p.Inv[w] > 0) {
			p.Weapon = w
		}
	case proto.ActFire:
		if !g.canAim(seat) {
			return
		}
		p.Angle = max(min(arg(0), 180), 0)
		p.Power = max(min(arg(1)|arg(2)<<8, g.maxPower(seat)), 0)
		if w := arg(3); w < proto.NWeapons && (w == items.Unlimited || p.Inv[w] > 0) {
			p.Weapon = w
		}
		p.Guide, p.Target = proto.INone, proto.NoSeat
		if items.IsGuidance(arg(4)) && p.Inv[arg(4)] > 0 && arg(5) < proto.Seats {
			p.Guide, p.Target = arg(4), arg(5)
		}
		sh := g.shotOf(seat)
		if g.Set[proto.SetMode] == proto.ModeSequential {
			g.resolve([]sim.Shot{sh}, false)
		} else {
			p.aim, p.Done = &sh, true
		}
	case proto.ActUse:
		if g.Phase == proto.PhDefense || g.canAim(seat) {
			g.use(seat, arg(0))
		}
	case proto.ActDrive:
		if g.canAim(seat) {
			g.drive(seat, arg(0) != 0)
		}
	}
}

// Driving: a step is DriveStep units for a unit of Fuel, plus a unit per
// DriveClimb units climbed; a rise of DriveWall or more is a wall.
const (
	DriveStep  = 4.0
	DriveClimb = 2.0
	DriveWall  = 8
)

// drive moves seat's tank a step left or right along the ground, if it has
// the fuel, the slope isn't too steep and no tank is in the way.
func (g *Game) drive(seat int, right bool) bool {
	p := &g.P[seat]
	t := g.tank(seat)
	if t == nil || p.Inv[proto.IFuel] <= 0 {
		return false
	}
	nx := t.X - DriveStep
	if right {
		nx = t.X + DriveStep
	}
	if g.Sim.S.Wall == proto.WallWrap {
		nx = math.Mod(nx+world.W, world.W)
	} else if nx < sim.TankW/2 || nx > world.W-sim.TankW/2 {
		return false
	}
	for _, o := range g.Sim.Tanks {
		if o != nil && o.Alive && o != t && math.Abs(o.X-nx) < sim.TankW && math.Abs(o.Y-t.Y) < sim.TankH*2 {
			return false
		}
	}
	probe := int(t.Y) - DriveWall
	ny := g.Sim.T.SurfaceBelow(int(nx), probe)
	if ny <= probe {
		return false // a wall
	}
	cost := 1
	if rise := t.Y - float64(ny); rise > 0 {
		cost += int(rise / DriveClimb)
	}
	if cost > p.Inv[proto.IFuel] {
		return false
	}
	p.Inv[proto.IFuel] -= cost
	t.X, t.Y = nx, float64(ny)
	return true
}

func (g *Game) maxPower(seat int) int {
	if t := g.tank(seat); t != nil {
		return int(t.Energy * 10)
	}
	return 1000
}

func (g *Game) canAim(seat int) bool {
	if g.Phase != proto.PhAim {
		return false
	}
	if t := g.tank(seat); t == nil || !t.Alive {
		return false
	}
	if g.Set[proto.SetMode] == proto.ModeSequential {
		return g.Turn == seat
	}
	return g.P[seat].aim == nil
}

// use an item: a battery, a shield, the parachute or the fuse.
func (g *Game) use(seat, item int) {
	p := &g.P[seat]
	t := g.tank(seat)
	switch {
	case item == proto.IBattery:
		if t != nil && p.Inv[item] > 0 && t.Energy < 100 {
			p.Inv[item]--
			t.Energy = math.Min(t.Energy+10, 100)
		}
	case items.IsShield(item):
		if p.Inv[item] > 0 {
			g.raise(seat, item)
		}
	case item == proto.IParachute:
		if p.Inv[item] > 0 {
			p.Chute = !p.Chute
			if t != nil {
				t.Chute = p.Chute
			}
		}
	case item == proto.IImpactFuse:
		if p.Inv[item] > 0 {
			p.Fuse = !p.Fuse
		}
	}
}

func (g *Game) raise(seat, item int) {
	t := g.tank(seat)
	if t == nil {
		return
	}
	g.P[seat].Inv[item]--
	t.Shield, t.ShieldHP = item, float64(items.All[item].HP)
}
