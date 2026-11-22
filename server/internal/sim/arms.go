package sim

import (
	"container/heap"
	"math"

	"fujibarrage/internal/items"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/world"
)

// The arsenal past plain blasts (docs/OPS.md): what each behaviour does
// when its shell lands, or when it leaves the barrel.

// Cell is one world cell (a flow's, a geyser's).
type Cell struct{ X, Y int16 }

// Shell modes: flying (the default), rolling on the surface, digging.
const (
	modeFly = iota
	modeRoll
	modeDig
)

const (
	rollClimb   = 5     // the most a roller climbs in one unit
	rollFall    = 6     // a drop deeper than this: it flies off the edge
	rollMin     = 60.0  // a roller's speed at landing, at least
	rollMax     = 320.0 // and at most
	rollDrag    = 45.0  // speed lost a second
	digSpeed    = 150.0 // a drill or mole, units a second
	digR        = 4.0   // its tunnel's half-width
	flowCells   = 60    // world cells of liquid per hot spot
	burnNapalm  = 2.0   // seconds the fire burns
	burnInferno = 3.0
	beamDur     = 0.35 // a laser's beam shows this long
	beamHalf    = 3.0  // the channel it burns, half-width
	quakeDur    = 0.8
)

// cluster: a Cluster Bomb's bomblets in the air; the last one down sets off
// the big blast where the bomb landed.
type cluster struct {
	left    int
	x, y    float64
	r, mult float64
	seat    int
}

// spawn adds a shell; it starts moving on the next step.
func (s *Sim) spawn(q *shell) { s.spawned = append(s.spawned, q) }

func (s *Sim) adopt() {
	if len(s.spawned) > 0 {
		s.shells = append(s.shells, s.spawned...)
		s.spawned = nil
	}
}

// land: what p's behaviour does where it came down (hit: a tank's body, or
// -1). The shell is already lost.
func (s *Sim) land(p *shell, hit int) {
	it := p.it
	switch it.B {
	case items.BTracer:
	case items.BHopper:
		r := []float64{it.R, it.R2, it.R3}[2-p.hops]
		s.explode(p.x, p.y, r, proto.ExFire, it.Mult, p.seat)
		if p.hops > 0 {
			q := *p
			q.dead, q.grp = false, nil
			q.hops--
			q.vx /= 1.5
			q.vy = -math.Abs(q.vy) / 1.5
			q.y -= 2
			x, y := q.x, q.y
			s.at(s.now+blastDur(s.radius(r)), func() {
				q.trk = s.newTrack(proto.StShell, q.seat, x, y)
				s.shells = append(s.shells, &q)
			})
		}
	case items.BCluster:
		s.clusterBomb(p)
	case items.BMIRV: // came down before the apex: one warhead's blast
		s.explode(p.x, p.y, it.R, proto.ExFire, it.Mult, p.seat)
	case items.BFlow:
		s.flow(p)
	case items.BRoller:
		if hit >= 0 || p.fuse {
			s.explode(p.x, p.y, it.R, proto.ExFire, it.Mult, p.seat)
		} else {
			s.startRoll(p)
		}
	case items.BBuster:
		s.explode(p.x, p.y, it.R, proto.ExClear, 0, p.seat)
	case items.BDrill:
		s.startDig(p, -1)
	case items.BMole:
		if hit >= 0 {
			s.explode(p.x, p.y, it.R2, proto.ExFire, it.Mult, p.seat)
		} else {
			s.startDig(p, s.nearestEnemy(p.x, p.y, p.seat))
		}
	case items.BDirt:
		s.dirtBall(p.x, p.y, it.R)
	case items.BGeyser:
		s.geyser(p.x, p.y)
	case items.BQuake:
		s.quake(p.x, it.R)
	case items.BPlasma:
		s.blast(p.x, p.y, s.radius(it.R), proto.ExPlasma, it.Mult, p.seat)
	default:
		s.explode(p.x, p.y, max(it.R, 10), proto.ExFire, max(it.Mult, 1), p.seat)
	}
}

// ---- Cluster Bomb -------------------------------------------------------------

func (s *Sim) clusterBomb(p *shell) {
	it := p.it
	s.explode(p.x, p.y, it.R, proto.ExFire, it.Mult, p.seat)
	n := 5 + s.Rng.IntN(6)
	g := &cluster{left: n, x: p.x, y: p.y, r: it.R3, mult: it.Mult + 1, seat: p.seat}
	for i := 0; i < n; i++ {
		a := math.Pi/2 + (s.Rng.Float64()-0.5)*math.Pi*0.8 // up, fanned out
		v := 180 + s.Rng.Float64()*220
		q := &shell{x: p.x, y: p.y - 3, vx: math.Cos(a) * v, vy: -math.Sin(a) * v, seat: p.seat, w: p.w,
			it: &items.Item{B: items.BBlast, R: it.R2 + s.Rng.Float64()*10, Mult: it.Mult},
			ax: (s.Rng.Float64() - 0.5) * 500, grp: g, guide: proto.INone}
		q.trk = s.newTrack(proto.StBomblet, p.seat, q.x, q.y)
		s.spawn(q)
	}
}

// ---- Napalm, Inferno, Mudslide, Dirt Geyser ---------------------------------

// cellHeap orders the free cells next to a pool: lowest first, then
// nearest the start.
type cellHeap struct {
	c  []Cell
	x0 int
}

func (h *cellHeap) Len() int { return len(h.c) }
func (h *cellHeap) Less(i, j int) bool {
	a, b := h.c[i], h.c[j]
	if a.Y != b.Y {
		return a.Y > b.Y
	}
	da, db := absInt(int(a.X)-h.x0), absInt(int(b.X)-h.x0)
	if da != db {
		return da < db
	}
	return a.X < b.X
}
func (h *cellHeap) Swap(i, j int) { h.c[i], h.c[j] = h.c[j], h.c[i] }
func (h *cellHeap) Push(x any)    { h.c = append(h.c, x.(Cell)) }
func (h *cellHeap) Pop() any {
	n := len(h.c) - 1
	c := h.c[n]
	h.c = h.c[:n]
	return c
}

func absInt(v int) int {
	if v < 0 {
		return -v
	}
	return v
}

// pour runs vol cells of liquid from (x, y): each goes to the lowest free
// cell next to the pool, falling first where there is nothing under it,
// nearest the start when level. So it runs downhill and fills a basin from
// the bottom up. The cells in the order they filled.
func (s *Sim) pour(x, y float64, vol int) []Cell {
	ix := max(min(int(math.Floor(x)), world.W-1), 0)
	iy := min(int(math.Floor(y)), world.H-1)
	for iy >= 0 && s.T.At(ix, iy) {
		iy--
	}
	if iy < 0 {
		return nil
	}
	pool := make([]bool, world.W*world.H)
	full := func(x, y int) bool {
		return y >= world.H || s.T.At(x, y) || y >= 0 && pool[x*world.H+y]
	}
	h := &cellHeap{x0: ix}
	push := func(x, y int) {
		if x < 0 || x >= world.W || y < 0 || full(x, y) {
			return
		}
		for y+1 < world.H && !full(x, y+1) {
			y++
		}
		heap.Push(h, Cell{int16(x), int16(y)})
	}
	push(ix, iy)
	var out []Cell
	for len(out) < vol && h.Len() > 0 {
		c := heap.Pop(h).(Cell)
		x, y := int(c.X), int(c.Y)
		if full(x, y) {
			continue
		}
		pool[x*world.H+y] = true
		out = append(out, c)
		push(x-1, y)
		push(x+1, y)
		push(x, y-1)
	}
	return out
}

func (s *Sim) flow(p *shell) {
	it := p.it
	cells := s.pour(p.x, p.y, it.N*flowCells)
	if len(cells) == 0 {
		return
	}
	spread := 0.5 + float64(len(cells))/2000
	if p.w == proto.IMudslide {
		s.emit(Ev{Op: proto.OpFlow, Kind: proto.FlowMud, Dur: spread, Cells: cells})
		s.emit(Ev{Op: proto.OpSound, Kind: proto.SndDirt, A: 1})
		s.at(s.now+spread, func() { s.fillCells(cells) })
		return
	}
	burn := burnNapalm
	if p.w == proto.IInferno {
		burn = burnInferno
	}
	s.emit(Ev{Op: proto.OpFlow, Kind: proto.FlowFire, Dur: spread, Hold: burn, Cells: cells})
	s.emit(Ev{Op: proto.OpSound, Kind: proto.SndNapalm, A: soundSize(it.R)})
	r, mult, n, by := s.radius(it.R), it.Mult, it.N, p.seat
	s.at(s.now+spread+burn, func() { s.burn(cells, n, r, mult, by) })
}

// burn: n hot spots spread over the pool's cells; each hurts every tank
// within r of it, through shields.
func (s *Sim) burn(cells []Cell, n int, r, mult float64, by int) {
	for _, t := range s.Tanks {
		if t == nil || !t.Alive {
			continue
		}
		dmg := 0.0
		for k := 0; k < n; k++ {
			c := cells[k*len(cells)/n]
			if d := s.boxDist(t, float64(c.X)+0.5, float64(c.Y)+0.5); d < r {
				dmg += (r - d) / r * mult / 3
			}
		}
		s.hurt(t, math.Round(dmg), by, true)
	}
}

// fillCells turns cells to dirt and lets them settle.
func (s *Sim) fillCells(cells []Cell) {
	span := world.Span{}
	for _, c := range cells {
		s.T.Set(int(c.X), int(c.Y), true)
		span = span.Union(world.Span{X0: int(c.X), X1: int(c.X) + 1})
	}
	s.settle(span)
}

// geyser: a cone of dirt sprays up from (x, y), widening as it rises.
func (s *Sim) geyser(x, y float64) {
	tall := 80 * s.S.Scale
	var cells []Cell
	for dy := 0; dy < int(tall); dy++ {
		cy := int(math.Floor(y)) - dy
		if cy < 0 {
			break
		}
		hw := 2 + float64(dy)*0.25
		for cx := int(math.Floor(x - hw)); cx <= int(math.Floor(x+hw)); cx++ {
			if cx >= 0 && cx < world.W && !s.T.At(cx, cy) {
				cells = append(cells, Cell{int16(cx), int16(cy)})
			}
		}
	}
	if len(cells) == 0 {
		return
	}
	s.emit(Ev{Op: proto.OpFlow, Kind: proto.FlowMud, Dur: 0.6, Cells: cells})
	s.emit(Ev{Op: proto.OpSound, Kind: proto.SndDirt, A: 2})
	s.at(s.now+0.6, func() { s.fillCells(cells) })
}

// ---- Dirt Clump, Dirt Heap, Landslide; Quake ---------------------------------

// dirtBall fills the air in a disc of r (reference units) with dirt.
func (s *Sim) dirtBall(x, y, r float64) {
	wr := s.radius(r)
	d := 0.3 + wr/400
	s.emit(Ev{Op: proto.OpDirt, X: x, Y: y, R: wr, Dur: d})
	s.emit(Ev{Op: proto.OpSound, Kind: proto.SndDirt, A: soundSize(r)})
	s.at(s.now+d, func() {
		span := s.T.Fill(x, y, wr)
		if s.S.Wall == proto.WallWrap {
			if x-wr < 0 {
				span = span.Union(s.T.Fill(x+world.W, y, wr))
			}
			if x+wr >= world.W {
				span = span.Union(s.T.Fill(x-world.W, y, wr))
			}
		}
		s.settle(span)
	})
}

// quake: the ground shakes and every floating cell within r (reference
// units) of x comes down, whatever Dirt Stays Up says.
func (s *Sim) quake(x, r float64) {
	wr := s.radius(r)
	s.emit(Ev{Op: proto.OpShake, Dur: quakeDur})
	s.emit(Ev{Op: proto.OpSound, Kind: proto.SndThunder, A: 3})
	span := world.Span{X0: max(int(x-wr), 0), X1: min(int(x+wr)+1, world.W)}.Union(s.pendingDirty)
	s.pendingDirty = world.Span{}
	s.now += quakeDur / 2
	drop := s.T.Settle(span)
	d := float64(drop) / DirtSpeed
	s.emitCols(span, d)
	s.now += d
}

// ---- rollers ------------------------------------------------------------------

// restY: the air cell resting on the dirt at or below (ix, iy), as y.
func (s *Sim) restY(ix, iy int) int {
	for iy >= 0 && s.T.At(ix, iy) {
		iy--
	}
	for iy+1 < world.H && !s.T.At(ix, iy+1) {
		iy++
	}
	return iy
}

// startRoll: a roller came down on dirt; it rolls off downhill.
func (s *Sim) startRoll(p *shell) {
	ix := max(min(int(math.Floor(p.x)), world.W-1), 0)
	y := s.restY(ix, int(math.Floor(p.y)))
	l := s.restY(s.wrapX(max(ix-4, 0)), y)
	r := s.restY(s.wrapX(min(ix+4, world.W-1)), y)
	dir := 1
	switch {
	case l > r:
		dir = -1
	case l == r && p.vx < 0:
		dir = -1
	}
	q := &shell{x: float64(ix) + 0.5, y: float64(y) + 0.5, seat: p.seat, w: p.w, it: p.it,
		mode: modeRoll, dir: dir, speed: math.Max(math.Min(math.Abs(p.vx)*0.5, rollMax), rollMin), guide: proto.INone}
	q.trk = s.newTrack(proto.StRoller, p.seat, q.x, q.y)
	s.spawn(q)
	s.emit(Ev{Op: proto.OpSound, Kind: proto.SndRoll})
}

func (s *Sim) rollEnd(p *shell) {
	s.lose(p)
	s.explode(p.x, p.y, p.it.R, proto.ExFire, p.it.Mult, p.seat)
}

// roll moves a roller one DT along the surface.
func (s *Sim) roll(p *shell) {
	dist := p.speed * DT
	for dist > 0 && !p.dead {
		dist--
		nx := p.x + float64(p.dir)
		if nx < 0 || nx >= world.W {
			switch {
			case s.S.Wall == proto.WallWrap:
				nx = math.Mod(nx+world.W, world.W)
			case s.bouncy():
				p.bounces++
				p.dir = -p.dir
				p.speed *= s.wallCoef(p.bounces)
				s.emit(Ev{Op: proto.OpSound, Kind: proto.SndBounce})
				continue
			default:
				s.rollEnd(p)
				return
			}
		}
		ix, iy := int(math.Floor(nx)), int(math.Floor(p.y))
		climb := 0
		for climb <= rollClimb && s.T.At(ix, iy-climb) {
			climb++
		}
		if climb > rollClimb { // a wall
			s.rollEnd(p)
			return
		}
		ny := iy - climb
		drop := 0
		for ny+1 < world.H && !s.T.At(ix, ny+1) {
			ny++
			drop++
			if drop > rollFall {
				break
			}
		}
		if drop > rollFall { // over an edge: it flies again
			p.x, p.y = nx, float64(iy-climb)+0.5
			s.lose(p)
			q := &shell{x: p.x, y: p.y, vx: float64(p.dir) * p.speed, seat: p.seat, w: p.w, it: p.it, guide: proto.INone}
			q.trk = s.newTrack(proto.StShell, p.seat, q.x, q.y)
			s.spawn(q)
			return
		}
		p.x, p.y = nx, float64(ny)+0.5
		p.speed = math.Min(p.speed+float64(drop)*25-float64(climb)*30, rollMax)
		if s.rollerTouches(p) {
			s.rollEnd(p)
			return
		}
	}
	if p.dead {
		return
	}
	p.speed -= rollDrag * DT
	if p.speed <= 5 {
		s.rollEnd(p)
		return
	}
	p.trk.Pts = append(p.trk.Pts, Pt{X: p.x, Y: p.y})
}

func (s *Sim) rollerTouches(p *shell) bool {
	for _, t := range s.Tanks {
		if t == nil || !t.Alive {
			continue
		}
		if math.Abs(s.dx(p.x, t.X)) <= TankW/2+1 && p.y >= t.Y-TankH-1 && p.y <= t.Y+1 {
			return true
		}
	}
	return false
}

// ---- drills and moles ----------------------------------------------------------

func (s *Sim) nearestEnemy(x, y float64, seat int) int {
	best, bd := -1, math.Inf(1)
	for _, t := range s.Tanks {
		if t == nil || !t.Alive || t.Seat == seat {
			continue
		}
		if d := math.Hypot(s.dx(t.X, x), t.Y-y); d < bd {
			best, bd = t.Seat, d
		}
	}
	return best
}

// startDig: a drill bores down from where it landed; a mole toward seat
// toward (-1: down, like a drill), and goes off under it.
func (s *Sim) startDig(p *shell, toward int) {
	it := p.it
	q := &shell{x: p.x, y: p.y, seat: p.seat, w: p.w, it: it, mode: modeDig, ang: math.Pi / 2,
		target: toward, guide: proto.INone}
	if it.B == items.BMole {
		q.left = s.radius(it.R) * 5
	} else {
		q.left = s.radius(it.R) * 3
	}
	q.trk = s.newTrack(proto.StDigger, p.seat, q.x, q.y)
	s.spawn(q)
	s.emit(Ev{Op: proto.OpSound, Kind: proto.SndDig})
}

func (s *Sim) digEnd(p *shell) {
	s.lose(p)
	if p.it.B == items.BMole {
		s.explode(p.x, p.y, p.it.R2, proto.ExFire, p.it.Mult, p.seat)
	} else {
		s.settleHold(world.Span{})
	}
}

// dig moves a drill or mole one DT, cutting as it goes.
func (s *Sim) dig(p *shell) {
	mole := p.it.B == items.BMole && p.target >= 0
	var tx, ty float64
	if mole {
		t := s.Tanks[p.target]
		tx, ty = t.X, t.Y+digR+2 // just under it
	}
	for d := digSpeed * DT; d > 0 && !p.dead; d-- {
		if mole {
			want := math.Atan2(ty-p.y, s.dx(tx, p.x))
			turn := math.Remainder(want-p.ang, 2*math.Pi)
			p.ang += math.Max(-0.08, math.Min(0.08, turn))
			if math.Hypot(s.dx(tx, p.x), ty-p.y) < 3 {
				s.digEnd(p)
				return
			}
		} else { // wanders, pulled back toward straight down
			p.ang += (math.Pi/2 - p.ang) * 0.04
			p.ang = math.Max(math.Pi/2-0.8, math.Min(math.Pi/2+0.8, p.ang))
		}
		p.ang += (s.Rng.Float64() - 0.5) * 0.2
		p.x += math.Cos(p.ang)
		p.y += math.Sin(p.ang)
		if s.S.Wall == proto.WallWrap {
			p.x = math.Mod(p.x+world.W, world.W)
		}
		p.left--
		if p.x < 0 || p.x >= world.W || p.y < 0 || p.y >= world.H-1 || p.left <= 0 {
			p.x = math.Max(0, math.Min(p.x, world.W-1))
			p.y = math.Max(0, math.Min(p.y, world.H-1))
			s.digEnd(p)
			return
		}
		sp := s.T.Cut(p.x, p.y, digR)
		s.pendingDirty = s.pendingDirty.Union(sp)
	}
	p.trk.Pts = append(p.trk.Pts, Pt{X: p.x, Y: p.y})
}

// ---- from the barrel: Shovel Charge / Blast, Laser -------------------------

// shovel clears a fan of dirt out along the barrel.
func (s *Sim) shovel(t *Tank, angle int, it *items.Item, seat int) {
	reach := s.radius(it.R)
	a := float64(angle) * math.Pi / 180
	mx, my := t.Muzzle(angle)
	s.emit(Ev{Op: proto.OpSound, Kind: proto.SndDirt, A: soundSize(it.R)})
	for d := 6.0; d <= reach; d += 8 {
		x, y, r := mx+math.Cos(a)*d, my-math.Sin(a)*d, 3+d*0.4
		s.at(s.now+d/400, func() { s.blast(x, y, r, proto.ExClear, 0, seat) })
	}
}

// laser: a beam straight out of the barrel, up to a range set by the power;
// it burns a channel through dirt and stops at the first tank or shield.
func (s *Sim) laser(t *Tank, sh Shot, it *items.Item) {
	a := float64(sh.Angle) * math.Pi / 180
	cs, sn := math.Cos(a)*0.5, -math.Sin(a)*0.5
	x, y := t.Muzzle(sh.Angle)
	reach := math.Max(float64(sh.Power)*0.8, 40)
	segs := [][4]float64{{x, y, x, y}}
	var hit, ring *Tank
walk:
	for d := 0.0; d < reach; d += 0.5 {
		nx, ny := x+cs, y+sn
		if ny < 0 || ny >= world.H {
			break // the beam ends at its last point on the field
		}
		if nx < 0 || nx >= world.W {
			if s.S.Wall != proto.WallWrap {
				break
			}
			segs[len(segs)-1][2], segs[len(segs)-1][3] = x, y
			nx = math.Mod(nx+world.W, world.W)
			segs = append(segs, [4]float64{nx, ny, nx, ny})
		}
		x, y = nx, ny
		for _, o := range s.Tanks {
			if o == nil || !o.Alive || o.Seat == sh.Seat {
				continue
			}
			cx, cy := o.Centre()
			dd := math.Hypot(s.dx(x, cx), y-cy)
			if o.Shield != proto.INone && o.ShieldHP > 0 && dd <= ShieldR && dd > ShieldR-3 {
				ring = o
				break walk
			}
			if math.Abs(s.dx(x, o.X)) <= TankW/2 && y >= o.Y-TankH && y < o.Y {
				hit = o
				break walk
			}
		}
	}
	segs[len(segs)-1][2], segs[len(segs)-1][3] = x, y
	if s.quiet {
		s.hit = Hit{X: x, Y: y, Seat: -1}
		if hit != nil {
			s.hit.Seat = hit.Seat
		}
		if ring != nil {
			s.hit.Seat = ring.Seat
		}
		return
	}
	for _, g := range segs {
		s.emit(Ev{Op: proto.OpBeam, X: g[0], Y: g[1], X1: g[2], Y1: g[3], Kind: 0, Dur: beamDur})
	}
	s.emit(Ev{Op: proto.OpSound, Kind: proto.SndLaser})
	s.at(s.now+beamDur, func() {
		span := world.Span{}
		for _, g := range segs {
			span = span.Union(s.T.CutLine(g[0], g[1], g[2], g[3], beamHalf))
		}
		if ring != nil {
			s.shieldTake(ring, it.Mult, sh.Seat)
			s.emit(Ev{Op: proto.OpExplode, X: x, Y: y, R: 6, Kind: proto.ExPuff, Dur: 0.15})
		}
		if hit != nil {
			s.hurt(hit, it.Mult, sh.Seat, false)
		}
		s.settleHold(span)
	})
}
