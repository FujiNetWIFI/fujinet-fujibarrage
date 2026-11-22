// Package sim resolves shots: shells fly through the world (gravity, wind,
// air, walls, tunnelling, guidance, shields), blow up, cut and fill dirt,
// hurt tanks; then the aftermath (dirt settling, tanks falling, tanks dying,
// over and over until nothing moves). Everything it does goes into a Log in
// world units and seconds; internal/project turns that into each client's
// SCRIPT. Only the server runs it, so every client sees the same outcome.
package sim

import (
	"math"
	"math/rand/v2"
	"sort"

	"fujibarrage/internal/items"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/world"
)

// Units: world units (640x400, square), seconds. The reference game's
// numbers are in its own pixels; ours are twice them.
const (
	DT        = 1.0 / 120
	TankW     = 24.0 // the body, bottom centre at (X, Y)
	TankH     = 12.0
	Barrel    = 14.0
	ShieldR   = 30.0 // a shield's ring, round the body's centre
	MaxSpeed  = 2000.0
	SpeedUnit = 2.0    // muzzle speed per point of power
	GravUnit  = 5000.0 // world/s^2 per point of the gravity setting
	WindUnit  = 2.5    // world/s^2 per point of wind
	TunnelK   = 0.866  // speed kept per unit of dirt bored through
	TunnelMin = 90.0   // slower than this in dirt: it stops there
	FallSpeed = 160.0  // a falling tank, units a second
	DirtSpeed = 240.0  // falling dirt
	FallHurt  = 1.0    // energy per unit fallen
	TalkBase  = 0.8    // seconds a phrase shows: base + per character
	TalkChar  = 0.045
	MaxTime   = 60.0 // a shot's events stop here, whatever happens
)

// Tank is one seat's tank on the field.
type Tank struct {
	Seat      int
	X, Y      float64 // bottom centre
	Energy    float64 // 0-100
	Alive     bool
	Shield    int // proto.I* or proto.INone
	ShieldHP  float64
	Chute     bool // a parachute armed
	Chutes    int
	Batteries int
	Angle     int
	Bot       bool

	lift   float64 // a deflector's uses left this shot
	lastBy int     // who hurt it last (-1: nobody, a fall)
}

// Centre is the middle of the body.
func (t *Tank) Centre() (float64, float64) { return t.X, t.Y - TankH/2 }

// Muzzle is the barrel's end at angle a (degrees: 0 right, 90 up).
func (t *Tank) Muzzle(a int) (float64, float64) {
	r := float64(a) * math.Pi / 180
	return t.X + Barrel*math.Cos(r), t.Y - TankH - Barrel*math.Sin(r)
}

// Settings are the physics of the round (the host's, resolved).
type Settings struct {
	Gravity      float64 // the setting: 0.2 is the usual
	Wind         float64 // + blows right
	Viscosity    int     // 0-20
	Wall         int     // proto.Wall*, resolved (never Random/Erratic)
	Ceiling      bool    // a cavern: a solid roof
	Tunnel       bool
	Suspend      int // % of edits whose floating dirt stays up
	TanksFall    bool
	ImpactDamage bool
	Scale        float64 // explosion scale: 0.5, 0.75, 1
	Borders      float64 // units past the side a shell may go (no walls)
}

// Shot is one tank firing.
type Shot struct {
	Seat, Angle, Power, Weapon int
	Guide, Target              int // guidance item (INone) and its target seat
	Fuse                       bool
	Talk                       string // the phrase it says first ("": none)
}

// Pt is a track's position at one step; Jump: it got there by a wrap.
type Pt struct {
	X, Y float64
	Jump bool
}

// Track is a moving thing, sampled every DT from T0.
type Track struct {
	ID    int
	Style int // proto.St*
	Seat  int
	T0    float64
	Pts   []Pt
}

// Ev is one thing that happens, at T. Op is the SCRIPT op it becomes.
type Ev struct {
	T          float64
	Op         byte
	Seat       int
	X, Y, R    float64
	X1, Y1, R2 float64
	Kind, A, B int
	Dur        float64
	Text       string
	Trk        *Track
	Span       world.Span
	Cols       []uint8 // OpSettle: columns Span.X0..X1-1 after, H cells each
	Cells      []Cell  // OpFlow: the cells, in the order they fill
	Hold       float64 // OpFlow: seconds the fire burns once it has spread
}

// Log is a resolved shot (or volley).
type Log struct {
	Evs    []Ev
	End    float64
	Damage [proto.Seats][proto.Seats]float64 // [by][to], energy
	Shield [proto.Seats][proto.Seats]float64 // [by][to], shield HP
	Kills  [proto.Seats][]int                // by: the seats it killed
}

// Clone is a copy to try a shot on: its own terrain, tanks and dice, no
// phrases (the bots weigh their shots with it).
func (s *Sim) Clone(seed uint64) *Sim {
	c := &Sim{T: s.T.Clone(), S: s.S, Rng: rand.New(rand.NewPCG(seed, seed^0x5BD1E995))}
	for i, t := range s.Tanks {
		if t != nil {
			tt := *t
			c.Tanks[i] = &tt
		}
	}
	return c
}

// Sim is the field: terrain, tanks, physics.
type Sim struct {
	T     *world.Terrain
	Tanks [proto.Seats]*Tank
	S     Settings
	Rng   *rand.Rand
	// LastWords picks a dying tank's phrase ("": none).
	LastWords func(seat int) string

	now          float64
	log          *Log
	shells       []*shell
	spawned      []*shell // shells started during a step: they move from the next
	pending      []timed
	nsched       int
	ntrack       int
	pendingDirty world.Span // cells bored by tunnelling shells
	quiet        bool       // tracing for a bot: no log, no edits
	hit          Hit        // what the trace met
}

type timed struct {
	t float64
	n int // order of scheduling, for ties
	f func()
}

func (s *Sim) at(t float64, f func()) {
	s.nsched++
	s.pending = append(s.pending, timed{t, s.nsched, f})
}

func (s *Sim) sortPending() {
	sort.Slice(s.pending, func(i, j int) bool {
		a, b := s.pending[i], s.pending[j]
		return a.t < b.t || a.t == b.t && a.n < b.n
	})
}

func (s *Sim) emit(e Ev) {
	if s.quiet {
		return
	}
	e.T = s.now
	s.log.Evs = append(s.log.Evs, e)
}

// Resolve plays shots: together (simultaneous: one volley) or one after
// another, each with its own aftermath (sequential, synchronous).
func (s *Sim) Resolve(shots []Shot, together bool) *Log {
	s.log = &Log{}
	s.now = 0
	s.pending = nil
	s.shells = nil
	for _, t := range s.Tanks {
		if t != nil {
			t.lift = 1000
			t.lastBy = -1
		}
	}
	if together {
		talk := 0.0
		for _, sh := range shots {
			talk = max(talk, s.say(sh.Seat, sh.Talk))
		}
		s.now += talk
		for _, sh := range shots {
			s.fire(sh)
		}
		s.run()
		s.aftermath()
	} else {
		for _, sh := range shots {
			if t := s.Tanks[sh.Seat]; t == nil || !t.Alive {
				continue
			}
			s.now += s.say(sh.Seat, sh.Talk)
			s.fire(sh)
			s.run()
			s.aftermath()
			s.now += 0.3
		}
	}
	s.log.End = s.now
	return s.log
}

// say shows a phrase over seat's tank; the seconds it takes.
func (s *Sim) say(seat int, text string) float64 {
	if text == "" {
		return 0
	}
	if len(text) > proto.MaxTalk {
		text = text[:proto.MaxTalk]
	}
	d := TalkBase + TalkChar*float64(len(text))
	s.emit(Ev{Op: proto.OpTalk, Seat: seat, Text: text, Dur: d})
	return d
}

// ---- shells -----------------------------------------------------------------

type shell struct {
	x, y, vx, vy float64
	seat, w      int
	it           *items.Item
	trk          *Track
	hops         int // hopper: blasts left after this one
	up           bool
	guide        int
	target       int
	fuse         bool
	bounces      int
	dead         bool
	homed        bool // heat seeker locked on (hx, hy), coming from (hsx, hsy)
	hx, hy       float64
	hsx, hsy     int
	guided       bool    // steered: no gravity, wind or air
	digs         bool    // bores through dirt without slowing (autopilot)
	ax           float64 // a sideways push (cluster bomblets curve)
	grp          *cluster
	mode         int     // modeFly, modeRoll, modeDig
	dir          int     // rolling: -1 left, 1 right
	speed        float64 // rolling, units a second
	left         float64 // digging: units still to bore
	ang          float64 // digging: heading, radians (0 right, pi/2 down)
}

func (s *Sim) newTrack(style, seat int, x, y float64) *Track {
	s.ntrack++
	tr := &Track{ID: s.ntrack, Style: style, Seat: seat, T0: s.now, Pts: []Pt{{X: x, Y: y}}}
	s.emit(Ev{Op: proto.OpArc, Seat: seat, Trk: tr})
	return tr
}

// fire launches seat's shot from its barrel.
func (s *Sim) fire(sh Shot) {
	t := s.Tanks[sh.Seat]
	if t == nil || !t.Alive {
		return
	}
	t.Angle = sh.Angle
	it := &items.All[sh.Weapon]
	s.emit(Ev{Op: proto.OpFire, Seat: sh.Seat, A: sh.Angle, B: sh.Power, Kind: sh.Weapon})
	s.emit(Ev{Op: proto.OpSound, Kind: proto.SndFire, A: soundSize(it.R)})
	switch it.B {
	case items.BShovel:
		if s.quiet {
			s.hit = Hit{Seat: -1, Lost: true}
			return
		}
		s.shovel(t, sh.Angle, it, sh.Seat)
		return
	case items.BLaser:
		s.laser(t, sh, it)
		return
	}
	mx, my := t.Muzzle(sh.Angle)
	r := float64(sh.Angle) * math.Pi / 180
	v := float64(sh.Power) * SpeedUnit
	style := proto.StShell
	if it.B == items.BTracer {
		style = proto.StTracer
		if sh.Weapon == proto.ISmokeTracer {
			style = proto.StSmoke
		}
	}
	p := &shell{x: mx, y: my, vx: math.Trunc(math.Cos(r) * v), vy: -math.Trunc(math.Sin(r) * v),
		seat: sh.Seat, w: sh.Weapon, it: it, guide: sh.Guide, target: sh.Target, fuse: sh.Fuse}
	p.up = p.vy < 0
	switch it.B {
	case items.BHopper:
		p.hops = 2
	}
	p.trk = s.newTrack(style, sh.Seat, mx, my)
	s.shells = append(s.shells, p)
}

// run steps the shells and the scheduled events until all are done.
func (s *Sim) run() {
	for s.now < MaxTime {
		if len(s.shells) > 0 {
			s.now += DT
			live := s.shells[:0]
			for _, p := range s.shells {
				s.step(p)
				if !p.dead {
					live = append(live, p)
				}
			}
			s.shells = live
			s.due()
			s.adopt()
			continue
		}
		if len(s.pending) == 0 {
			return
		}
		s.sortPending()
		if s.pending[0].t > s.now {
			s.now = s.pending[0].t
		}
		s.due()
		s.adopt()
	}
}

// due runs the scheduled events whose time has come.
func (s *Sim) due() {
	for {
		s.sortPending()
		if len(s.pending) == 0 || s.pending[0].t > s.now+1e-9 {
			return
		}
		f := s.pending[0].f
		s.pending = s.pending[1:]
		f()
	}
}

func (s *Sim) wallCoef(n int) float64 {
	k := 1.0
	switch s.S.Wall {
	case proto.WallPadded:
		k = 0.5
	case proto.WallSpring:
		k = 2
	}
	for i := 6; i <= n; i++ {
		k *= 0.8
	}
	return k
}

func (s *Sim) bouncy() bool {
	w := s.S.Wall
	return w == proto.WallPadded || w == proto.WallRubber || w == proto.WallSpring
}

// step moves shell p one DT, cell by cell, and handles what it meets.
func (s *Sim) step(p *shell) {
	switch p.mode {
	case modeRoll:
		s.roll(p)
		return
	case modeDig:
		s.dig(p)
		return
	}
	wasUp := p.vy < 0
	s.steer(p)
	if p.dead {
		return
	}
	if !p.guided {
		visc := math.Pow(1-float64(s.S.Viscosity)/10000, 50*DT)
		p.vx *= visc
		p.vy *= visc
		p.vy += GravUnit * s.S.Gravity * DT
		p.vx += (WindUnit*s.S.Wind + p.ax) * DT
		s.lift(p)
	}
	if sp := math.Hypot(p.vx, p.vy); sp > MaxSpeed {
		p.vx, p.vy = p.vx*MaxSpeed/sp, p.vy*MaxSpeed/sp
	}
	dx, dy := p.vx*DT, p.vy*DT
	n := int(math.Ceil(math.Max(math.Abs(dx), math.Abs(dy))))
	if n < 1 {
		n = 1
	}
	sx, sy := dx/float64(n), dy/float64(n)
	jump := false
	for i := 0; i < n && !p.dead; i++ {
		p.x += sx
		p.y += sy
		// the side walls
		if p.x < 0 || p.x >= world.W {
			switch {
			case s.S.Wall == proto.WallWrap:
				if p.x < 0 {
					p.x += world.W
				} else {
					p.x -= world.W
				}
				jump = true
			case s.bouncy():
				p.bounces++
				if p.x < 0 {
					p.x = -p.x
				} else {
					p.x = 2*world.W - p.x - 1
				}
				p.vx = -p.vx * s.wallCoef(p.bounces)
				sx = -sx
				s.emit(Ev{Op: proto.OpSound, Kind: proto.SndBounce})
			case s.S.Wall == proto.WallConcrete:
				p.x = math.Max(0, math.Min(p.x, world.W-1))
				s.impact(p, -1)
				continue
			default: // open: gone once far enough out
				if p.x < -s.S.Borders || p.x >= world.W+s.S.Borders {
					s.lose(p)
					if s.quiet {
						s.hit = Hit{X: p.x, Y: p.y, Seat: -1, Lost: true}
					}
					continue
				}
			}
		}
		// the ceiling (open, unless the walls close it or a cavern roof)
		if p.y < 0 && !s.S.Ceiling {
			switch {
			case s.bouncy():
				p.bounces++
				p.y = -p.y
				p.vy = -p.vy * s.wallCoef(p.bounces)
				sy = -sy
				s.emit(Ev{Op: proto.OpSound, Kind: proto.SndBounce})
			case s.S.Wall == proto.WallConcrete:
				p.y = 0
				s.impact(p, -1)
				continue
			}
		}
		if s.S.Ceiling && p.y < 0 {
			p.y = 0
			s.impact(p, -1)
			continue
		}
		// the floor
		if p.y >= world.H {
			if (s.S.Wall == proto.WallRubber || s.S.Wall == proto.WallSpring) && math.Abs(p.vy) >= 100 {
				p.bounces++
				p.y = 2*world.H - p.y - 1
				p.vy = -p.vy * s.wallCoef(p.bounces)
				sy = -sy
				continue
			}
			p.y = world.H - 1
			s.impact(p, -1)
			continue
		}
		// tanks: their shields, then their bodies
		if hit := s.touch(p); hit >= 0 {
			s.impact(p, hit)
			continue
		}
		if p.dead {
			continue
		}
		// dirt
		ix, iy := int(math.Floor(p.x)), int(math.Floor(p.y))
		if s.T.At(ix, iy) {
			if p.digs {
				if !s.quiet {
					s.T.Set(ix, iy, false)
					s.dirtyAt(ix)
				}
				continue
			}
			if s.tunnels(p) {
				sp := math.Hypot(p.vx, p.vy) * TunnelK
				if sp < TunnelMin {
					s.impact(p, -1)
					continue
				}
				k := TunnelK
				p.vx *= k
				p.vy *= k
				if !s.quiet {
					s.T.Set(ix, iy, false)
					s.dirtyAt(ix)
				}
				continue
			}
			s.impact(p, -1)
		}
	}
	if p.dead {
		return
	}
	p.trk.Pts = append(p.trk.Pts, Pt{X: p.x, Y: p.y, Jump: jump})
	if wasUp && p.vy >= 0 && p.it.B == items.BMIRV && !s.quiet {
		s.split(p)
	}
}

func (s *Sim) tunnels(p *shell) bool {
	if !s.S.Tunnel || p.fuse {
		return false
	}
	switch p.it.B {
	case items.BTracer, items.BRoller, items.BFlow, items.BDirt, items.BGeyser:
		return false
	}
	return true
}

// touch: the tank p hit (its body), or -1; a shield's ring may stop or
// turn it first.
func (s *Sim) touch(p *shell) int {
	for _, t := range s.Tanks {
		if t == nil || !t.Alive {
			continue
		}
		cx, cy := t.Centre()
		dx, dy := s.dx(p.x, cx), p.y-cy
		if dx > ShieldR || dx < -ShieldR || dy > ShieldR || dy < -ShieldR {
			continue // nowhere near it (the body is inside the ring's box)
		}
		if t.Shield != proto.INone && t.ShieldHP > 0 && t.Seat != p.seat && !items.IsDeflector(t.Shield) {
			d2 := dx*dx + dy*dy
			if d2 <= ShieldR*ShieldR && d2 > (ShieldR-3)*(ShieldR-3) && dx*p.vx+dy*p.vy < 0 { // coming in through the ring
				d := math.Sqrt(d2)
				if t.Shield == proto.IBounceShield {
					s.deflect(p, t, dx/d, dy/d)
					return -1
				}
				if s.quiet {
					return t.Seat
				}
				s.shieldTake(t, 10, p.seat)
				s.emit(Ev{Op: proto.OpExplode, X: p.x, Y: p.y, R: 6, Kind: proto.ExPuff, Dur: 0.15})
				s.lose(p)
				return -1
			}
		}
		if math.Abs(dx) <= TankW/2 && p.y >= t.Y-TankH && p.y < t.Y {
			return t.Seat
		}
	}
	return -1
}

// deflect turns p off t's bounce shield: mirrored about the ring's normal,
// 70% of the speed; the shield pays speed/100.
func (s *Sim) deflect(p *shell, t *Tank, nx, ny float64) {
	dot := p.vx*nx + p.vy*ny
	p.vx = (p.vx - 2*dot*nx) * 0.7
	p.vy = (p.vy - 2*dot*ny) * 0.7
	cx, cy := t.Centre()
	p.x, p.y = cx+nx*(ShieldR+0.5), cy+ny*(ShieldR+0.5) // outside the ring again
	if s.quiet {
		return
	}
	s.shieldTake(t, math.Hypot(p.vx, p.vy)/100/0.7, p.seat)
	s.emit(Ev{Op: proto.OpShield, Seat: t.Seat, Kind: proto.ShDeflect, A: int(t.ShieldHP)})
	s.emit(Ev{Op: proto.OpSound, Kind: proto.SndDeflect})
	if math.Hypot(p.vx, p.vy) < 80 { // too slow to fly off: it fizzles on the ring
		s.emit(Ev{Op: proto.OpExplode, X: p.x, Y: p.y, R: 6, Kind: proto.ExPuff, Dur: 0.15})
		s.lose(p)
	}
}

// lift: an enemy deflector's field pushes p up as it passes over the tank.
func (s *Sim) lift(p *shell) {
	for _, t := range s.Tanks {
		if t == nil || !t.Alive || t.Seat == p.seat || !items.IsDeflector(t.Shield) || t.ShieldHP <= 0 || t.lift <= 0 {
			continue
		}
		above := t.Y - TankH - p.y
		if math.Abs(s.dx(p.x, t.X)) > 40 || above < 2 || above > 160 {
			continue
		}
		a := 8000.0
		if t.Shield == proto.ISuperDeflector {
			a = 16000
		}
		p.vy -= a * DT
		t.lift--
		s.emit(Ev{Op: proto.OpShield, Seat: t.Seat, Kind: proto.ShLift, A: int(t.ShieldHP)})
	}
}

// dx is a - b, the short way round under wrap walls.
func (s *Sim) dx(a, b float64) float64 {
	d := a - b
	if s.S.Wall == proto.WallWrap {
		if d > world.W/2 {
			d -= world.W
		} else if d < -world.W/2 {
			d += world.W
		}
	}
	return d
}

// lose ends p's track with nothing more.
func (s *Sim) lose(p *shell) {
	p.trk.Pts = append(p.trk.Pts, Pt{X: p.x, Y: p.y})
	p.dead = true
	if g := p.grp; g != nil {
		p.grp = nil
		if g.left--; g.left == 0 && !s.quiet {
			s.at(s.now+0.5, func() { s.explode(g.x, g.y, g.r, proto.ExFire, g.mult, g.seat) })
		}
	}
}

// impactAt: a guided shell reached its target.
func (s *Sim) impactAt(p *shell, seat int) { s.impact(p, -1) }

// impact: p hit something (a tank's body: seat hit, else -1).
func (s *Sim) impact(p *shell, hit int) {
	s.lose(p)
	if s.quiet {
		s.hit = Hit{X: p.x, Y: p.y, Seat: hit}
		return
	}
	if hit >= 0 && contactKills(p.it.B) {
		t := s.Tanks[hit]
		s.hurt(t, t.Energy+t.ShieldHP+1, p.seat, true)
	}
	s.land(p, hit)
}

func contactKills(b int) bool {
	return b == items.BBlast || b == items.BCluster || b == items.BHopper
}

// split p at its apex into the MIRV's warheads, fanned out.
func (s *Sim) split(p *shell) {
	n := p.it.N
	coef := 50.0
	if n > 5 {
		coef = 20
	}
	s.lose(p)
	for i := 0; i < n; i++ {
		q := &shell{x: p.x, y: p.y, vx: p.vx + (float64(i)-float64(n-1)/2)*coef*SpeedUnit, vy: p.vy,
			seat: p.seat, w: p.w, it: &items.Item{B: items.BBlast, R: p.it.R, Mult: p.it.Mult}, guide: proto.INone}
		q.trk = s.newTrack(proto.StBomblet, p.seat, q.x, q.y)
		s.spawn(q)
	}
}

// ---- blasts and damage ----------------------------------------------------

// radius is r (reference units) in world units at the host's scale.
func (s *Sim) radius(r float64) float64 { return r * 2 * s.S.Scale }

func blastDur(r float64) float64 { return 0.2 + r/300 }

func soundSize(r float64) int {
	switch {
	case r >= 60:
		return 3
	case r >= 35:
		return 2
	case r >= 15:
		return 1
	}
	return 0
}

// explode: a blast of radius r (reference units) at (x, y); from 60 on a
// fire blast is a nuke's.
func (s *Sim) explode(x, y, r float64, kind int, mult float64, by int) {
	if r >= 60 && kind == proto.ExFire {
		kind = proto.ExNuke
	}
	s.blast(x, y, s.radius(r), kind, mult, by)
}

// blast: radius wr (world units) at (x, y). The fireball grows; when it is
// done the crater is cut (not for plasma or a puff), tanks inside are hurt,
// and the dirt above may fall.
func (s *Sim) blast(x, y, wr float64, kind int, mult float64, by int) {
	d := blastDur(wr)
	s.emit(Ev{Op: proto.OpExplode, X: x, Y: y, R: wr, Kind: kind, Dur: d})
	snd := proto.SndBoom
	switch kind {
	case proto.ExNuke:
		snd = proto.SndNuke
		s.emit(Ev{Op: proto.OpFlash, Kind: 15, Dur: 0.1})
	case proto.ExPlasma:
		snd = proto.SndPlasma
	case proto.ExClear:
		snd = proto.SndDirt
	}
	s.emit(Ev{Op: proto.OpSound, Kind: snd, A: soundSize(wr / 2)})
	crater := kind != proto.ExPlasma && kind != proto.ExPuff
	s.at(s.now+d, func() {
		span := world.Span{}
		if crater {
			span = s.T.Cut(x, y, wr)
			s.cutWrapped(x, y, wr, &span)
		}
		if mult > 0 {
			s.areaDamage(x, y, wr, mult, by)
		}
		s.settle(span)
	})
}

// cutWrapped: under wrap walls a blast at an edge reaches the other side.
func (s *Sim) cutWrapped(x, y, r float64, span *world.Span) {
	if s.S.Wall != proto.WallWrap {
		return
	}
	if x-r < 0 {
		sp := s.T.Cut(x+world.W, y, r)
		*span = span.Union(sp)
	}
	if x+r >= world.W {
		sp := s.T.Cut(x-world.W, y, r)
		*span = span.Union(sp)
	}
}

// areaDamage hurts each tank within r: (r - d) / r * 100 * mult, more than
// 100 counts 110.
func (s *Sim) areaDamage(x, y, r, mult float64, by int) {
	for _, t := range s.Tanks {
		if t == nil || !t.Alive {
			continue
		}
		d := s.boxDist(t, x, y)
		if d >= r {
			continue
		}
		dmg := math.Trunc((r-d)*100/r) * mult
		if dmg > 100 {
			dmg = 110
		}
		s.hurt(t, dmg, by, false)
	}
}

// boxDist is the distance from (x, y) to t's body.
func (s *Sim) boxDist(t *Tank, x, y float64) float64 {
	dx := math.Max(math.Abs(s.dx(x, t.X))-TankW/2, 0)
	dy := 0.0
	if y < t.Y-TankH {
		dy = t.Y - TankH - y
	} else if y > t.Y {
		dy = y - t.Y
	}
	return math.Hypot(dx, dy)
}

// hurt takes dmg from t: its shield first unless through.
func (s *Sim) hurt(t *Tank, dmg float64, by int, through bool) {
	if dmg <= 0 || !t.Alive {
		return
	}
	if !through && t.Shield != proto.INone && t.ShieldHP > 0 {
		take := math.Min(dmg, t.ShieldHP)
		s.shieldTake(t, take, by)
		dmg -= take
		if dmg <= 0 {
			return
		}
	}
	before := t.Energy
	t.Energy -= dmg
	if by >= 0 {
		s.log.Damage[by][t.Seat] += math.Min(dmg, math.Max(before, 0))
		t.lastBy = by
	}
	s.emit(Ev{Op: proto.OpDamage, Seat: t.Seat, A: int(math.Round(dmg)), B: max(int(math.Ceil(t.Energy)), 0)})
}

func (s *Sim) shieldTake(t *Tank, hp float64, by int) {
	hp = math.Min(hp, t.ShieldHP)
	t.ShieldHP -= hp
	if by >= 0 {
		s.log.Shield[by][t.Seat] += hp
	}
	if t.ShieldHP <= 0.5 {
		t.ShieldHP = 0
		t.Shield = proto.INone
		s.emit(Ev{Op: proto.OpShield, Seat: t.Seat, Kind: proto.ShBreak, A: 0})
		return
	}
	s.emit(Ev{Op: proto.OpShield, Seat: t.Seat, Kind: proto.ShHit, A: int(t.ShieldHP)})
	s.emit(Ev{Op: proto.OpSound, Kind: proto.SndShield})
}

// ---- terrain ------------------------------------------------------------------

func (s *Sim) dirtyAt(x int) { s.pendingDirty = s.pendingDirty.Union(world.Span{X0: x, X1: x + 1}) }

// settle: dirt was cut or added over span by a blast or new dirt. Floating
// dirt falls unless this edit's roll says it stays (Suspend %); the clients
// get the columns after.
func (s *Sim) settle(span world.Span) { s.settleIf(span, true) }

// settleHold: dirt was bored (a tunnel, a laser's channel): the dirt over it
// holds up; only the columns go to the clients.
func (s *Sim) settleHold(span world.Span) { s.settleIf(span, false) }

func (s *Sim) settleIf(span world.Span, fall bool) {
	span = span.Union(s.pendingDirty)
	s.pendingDirty = world.Span{}
	if span.Empty() {
		return
	}
	span = world.Span{X0: max(span.X0-5, 0), X1: min(span.X1+5, world.W)}
	drop := 0
	if fall && s.Rng.IntN(100) >= s.S.Suspend {
		drop = s.T.Settle(span)
	}
	d := 0.0
	if drop > 0 {
		d = float64(drop) / DirtSpeed
		s.emit(Ev{Op: proto.OpSound, Kind: proto.SndDirt})
	}
	s.emitCols(span, d)
	s.now += d
}

func (s *Sim) emitCols(span world.Span, d float64) {
	if s.quiet {
		return
	}
	cols := make([]uint8, 0, (span.X1-span.X0)*world.H)
	for x := span.X0; x < span.X1; x++ {
		cols = append(cols, s.T.Col(x)...)
	}
	s.emit(Ev{Op: proto.OpSettle, Span: span, Cols: cols, Dur: d})
}

// ---- the aftermath --------------------------------------------------------

// aftermath: tanks with nothing under them fall; dead tanks go up (which
// may blast more dirt and tanks); until nothing changes.
func (s *Sim) aftermath() {
	if !s.pendingDirty.Empty() { // a shell bored through, then left the field
		s.settleHold(world.Span{})
	}
	for round := 0; round < 20; round++ {
		moved := s.fall()
		died := false
		for _, t := range s.Tanks {
			if t != nil && t.Alive && t.Energy <= 0 {
				s.die(t)
				died = true
				s.run()
			}
		}
		if !moved && !died {
			return
		}
	}
}

// supported: is there dirt under enough of t's footprint?
func (s *Sim) supported(t *Tank) bool {
	y := int(math.Floor(t.Y))
	if y >= world.H {
		return true
	}
	n, mid := 0, false
	x0 := int(math.Floor(t.X - TankW/2))
	for x := x0; x < x0+int(TankW); x++ {
		if s.T.At(s.wrapX(x), y) {
			n++
			if math.Abs(float64(x)+0.5-t.X) <= 3 {
				mid = true
			}
		}
	}
	return mid || n > int(TankW)/4
}

func (s *Sim) wrapX(x int) int {
	if s.S.Wall == proto.WallWrap {
		return (x%world.W + world.W) % world.W
	}
	return x
}

// fall drops every unsupported tank to where it rests; true if one moved.
func (s *Sim) fall() bool {
	if !s.S.TanksFall {
		return false
	}
	any := false
	end := s.now
	var hurt []func()
	for _, t := range s.Tanks {
		if t == nil || !t.Alive || s.supported(t) {
			continue
		}
		y0, x0 := t.Y, t.X
		for !s.supported(t) && t.Y < world.H {
			t.Y++
		}
		dist := t.Y - y0
		chute := t.Chute && t.Chutes > 0 && dist*FallHurt > 5
		dur := dist / FallSpeed
		if chute {
			dur *= 2
			if math.Abs(s.S.Wind) > 10 {
				drift := math.Copysign(math.Min(dist/4, math.Abs(s.S.Wind)/10), s.S.Wind)
				t.X = math.Max(TankW/2, math.Min(world.W-TankW/2, t.X+drift))
				for !s.supported(t) && t.Y < world.H {
					t.Y++
				}
			}
			t.Chutes--
			if t.Chutes == 0 {
				t.Chute = false
			}
			s.emit(Ev{Op: proto.OpSound, Kind: proto.SndChute})
		}
		_ = x0
		s.emit(Ev{Op: proto.OpFall, Seat: t.Seat, X: t.X, Y: t.Y, Dur: dur, A: b2i(chute)})
		end = math.Max(end, s.now+dur)
		if !chute && dist > 0 && s.S.ImpactDamage {
			t, d := t, dist*FallHurt
			hurt = append(hurt, func() { s.hurt(t, d, -1, true) })
		}
		any = true
	}
	s.now = end
	for _, f := range hurt {
		f()
	}
	return any
}

func b2i(b bool) int {
	if b {
		return 1
	}
	return 0
}

// die: t goes up, says its last words, and its death may blast the others.
func (s *Sim) die(t *Tank) {
	t.Alive = false
	t.Energy = 0
	if t.lastBy >= 0 {
		s.log.Kills[t.lastBy] = append(s.log.Kills[t.lastBy], t.Seat)
	}
	if s.LastWords != nil {
		s.now += s.say(t.Seat, s.LastWords(t.Seat))
	}
	// mostly a fade or a small blast; now and then a chain-starter
	fx := []int{proto.DeathFade, proto.DeathFade, proto.DeathFade, proto.DeathRays, proto.DeathSpray,
		proto.DeathPlume, proto.DeathScatter, proto.DeathBlast, proto.DeathBlast, proto.DeathBlast,
		proto.DeathDouble, proto.DeathDouble, proto.DeathTriple}[s.Rng.IntN(13)]
	cx, cy := t.Centre()
	s.emit(Ev{Op: proto.OpDeath, Seat: t.Seat, Kind: fx})
	s.emit(Ev{Op: proto.OpSound, Kind: proto.SndDeath})
	switch fx {
	case proto.DeathBlast:
		s.explode(cx, cy, 20, proto.ExDeath, 1, -1)
	case proto.DeathDouble:
		s.explode(cx, cy, 20, proto.ExDeath, 1, -1)
		s.at(s.now+0.4, func() { s.explode(cx, cy, 40, proto.ExDeath, 1, -1) })
	case proto.DeathTriple:
		s.explode(cx, cy, 20, proto.ExDeath, 1, -1)
		s.at(s.now+0.4, func() { s.explode(cx, cy, 40, proto.ExDeath, 1, -1) })
		s.at(s.now+0.9, func() { s.explode(cx, cy, 75, proto.ExDeath, 1, -1) })
	default:
		s.explode(cx, cy, 8, proto.ExDeath, 0, -1)
	}
}
