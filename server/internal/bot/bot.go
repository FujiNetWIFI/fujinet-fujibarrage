// Package bot plays a seat: it picks a target and a weapon, then aims by
// flying candidate shots through the real simulator (sim.Trace), and
// misses on purpose by its tier (proto.Tier*):
//
//	Rookie     random angle, power and weapon, roughly the right way
//	Gunner     flat, hard shots at the nearest tank it can see
//	Ricochet   also tries banking off bouncy walls and ceilings
//	Lobber     high lobs, closing in on its last miss at the same target
//	Tactician  the best target, and the weapon that does it most harm (each
//	           tried out on a copy of the field), small error
//	Sniper     the same at the leader, slight error
//	Deadeye    the same, more aims tried, no error; uses defences well
//	Wildcard   a different one of the above each round
package bot

import (
	"math"
	"math/rand/v2"

	"fujibarrage/internal/items"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/sim"
)

// Memory is what a bot remembers between its shots in a round.
type Memory struct {
	Target    int
	Angle     int
	Power     int
	Miss      float64 // last miss, + right of the target
	Shots     int
	PrevPower int // the shot before (a Lobber's secant)
	PrevMiss  float64
	Seen      bool // the last shot's miss is known
	RoundTier int  // the tier played this round (Wildcard rolls it)
}

// Seat is what a bot knows of its own seat.
type Seat struct {
	Seat   int
	Tier   int
	Inv    *[proto.NItems]int
	Scores [proto.Seats]int // for "the leader"
	Mem    *Memory
}

// noise is the aim error a tier makes: degrees and power, one sigma.
var noise = [proto.NTiers]struct{ a, p float64 }{
	proto.TierRookie:    {25, 200},
	proto.TierGunner:    {4, 60},
	proto.TierRicochet:  {3, 45},
	proto.TierLobber:    {3, 40},
	proto.TierTactician: {1.5, 20},
	proto.TierSniper:    {0.7, 8},
	proto.TierDeadeye:   {0, 0},
}

// NewRound readies the memory for a round (a Wildcard picks its tier).
func NewRound(b *Seat, rng *rand.Rand) {
	*b.Mem = Memory{Target: -1, RoundTier: b.Tier}
	if b.Tier == proto.TierWildcard {
		b.Mem.RoundTier = rng.IntN(proto.TierWildcard)
	}
}

// Aim picks the bot's shot.
func Aim(s *sim.Sim, b *Seat, rng *rand.Rand) sim.Shot {
	me := s.Tanks[b.Seat]
	tier := b.Mem.RoundTier
	shot := sim.Shot{Seat: b.Seat, Weapon: proto.IMiniMissile, Guide: proto.INone, Target: proto.NoSeat}
	tgt := target(s, b, tier, rng)
	if tgt < 0 {
		shot.Angle, shot.Power = 90, 100
		return shot
	}
	shot.Target = tgt
	maxP := int(me.Energy * 10)
	t := s.Tanks[tgt]
	switch tier {
	case proto.TierRookie:
		shot.Weapon = weapon(s, b, tier, tgt, rng)
		a := 20 + rng.IntN(70)
		if t.X < me.X {
			a = 180 - a
		}
		shot.Angle, shot.Power = a, 150+rng.IntN(max(maxP-150, 1))
		return shot
	}
	lo, hi := 10, 85
	switch tier {
	case proto.TierGunner:
		lo, hi = 8, 40
	case proto.TierLobber:
		lo, hi = 55, 86
	}
	var best cand
	if tier >= proto.TierTactician {
		shot.Weapon, best = choose(s, b, tier, tgt, maxP, rng)
	} else {
		shot.Weapon = weapon(s, b, tier, tgt, rng)
		best = search(s, b.Seat, tgt, shot.Weapon, lo, hi, maxP, tier == proto.TierRicochet)[0]
	}
	n := noise[tier]
	a := float64(best.angle) + rng.NormFloat64()*n.a
	p := float64(best.power) + rng.NormFloat64()*n.p
	if tier == proto.TierLobber && b.Mem.Target == tgt && b.Mem.Shots > 0 && b.Mem.Seen {
		// trial and error from the last shot: two shots give how the miss
		// moves with the power (a secant step); one, a guess
		a = float64(b.Mem.Angle)
		dir := 1.0
		if t.X < me.X {
			dir = -1
		}
		step := -dir * b.Mem.Miss * 0.6
		if b.Mem.Shots > 1 && b.Mem.Power != b.Mem.PrevPower && b.Mem.Miss != b.Mem.PrevMiss {
			slope := (b.Mem.Miss - b.Mem.PrevMiss) / float64(b.Mem.Power-b.Mem.PrevPower)
			if slope*dir > 0 { // more power, further on: as it should be
				step = -b.Mem.Miss / slope
			}
		}
		p = float64(b.Mem.Power) + math.Max(-300, math.Min(300, step)) + rng.NormFloat64()*5
	}
	shot.Angle = max(min(int(math.Round(a)), 180), 0)
	shot.Power = max(min(int(math.Round(p)), maxP), 50)
	if b.Mem.Target != tgt {
		b.Mem.Shots = 0
	}
	b.Mem.PrevPower, b.Mem.PrevMiss = b.Mem.Power, b.Mem.Miss
	b.Mem.Target, b.Mem.Angle, b.Mem.Power = tgt, shot.Angle, shot.Power
	b.Mem.Shots++
	b.Mem.Seen = false
	return shot
}

// Observe tells a Lobber where its shot came down.
func Observe(b *Seat, s *sim.Sim, x float64) {
	if b.Mem.Target >= 0 && s.Tanks[b.Mem.Target] != nil {
		b.Mem.Miss = x - s.Tanks[b.Mem.Target].X
		b.Mem.Seen = true
	}
}

// target: nearest for the plain tiers, the leader or the weakest for the
// clever ones.
func target(s *sim.Sim, b *Seat, tier int, rng *rand.Rand) int {
	me := s.Tanks[b.Seat]
	best, bestScore := -1, math.Inf(1)
	var live []int
	for _, t := range s.Tanks {
		if t == nil || !t.Alive || t.Seat == b.Seat {
			continue
		}
		live = append(live, t.Seat)
		d := math.Abs(t.X - me.X)
		score := d
		switch tier {
		case proto.TierGunner:
			if !sight(s, b.Seat, t.Seat) {
				score += 10000
			}
		case proto.TierTactician:
			score = d*0.5 + t.Energy*2 + t.ShieldHP
		case proto.TierSniper, proto.TierDeadeye:
			score = -float64(b.Scores[t.Seat])*0.01 + t.Energy + t.ShieldHP*0.5 + d*0.1
		}
		if score < bestScore {
			best, bestScore = t.Seat, score
		}
	}
	if len(live) == 0 {
		return -1
	}
	if tier == proto.TierRookie {
		return live[rng.IntN(len(live))]
	}
	if b.Mem.Target >= 0 && s.Tanks[b.Mem.Target] != nil && s.Tanks[b.Mem.Target].Alive && tier == proto.TierLobber {
		return b.Mem.Target
	}
	return best
}

// sight: nothing but air from seat's barrel to tank tgt?
func sight(s *sim.Sim, seat, tgt int) bool {
	x0, y0 := s.Tanks[seat].Muzzle(90)
	x1, y1 := s.Tanks[tgt].Centre()
	n := int(math.Hypot(x1-x0, y1-y0) / 2)
	for k := 1; k < n; k++ {
		f := float64(k) / float64(n)
		if s.T.At(int(x0+(x1-x0)*f), int(y0+(y1-y0)*f)) {
			return false
		}
	}
	return true
}

// weapon: Rookies anything they own; the plain tiers the biggest plain
// blast that won't reach back to them.
func weapon(s *sim.Sim, b *Seat, tier, tgt int, rng *rand.Rand) int {
	inv := b.Inv
	has := func(i int) bool { return i == items.Unlimited || inv[i] > 0 }
	if tier == proto.TierRookie {
		var own []int
		for i := 0; i < proto.NWeapons; i++ {
			if has(i) {
				own = append(own, i)
			}
		}
		return own[rng.IntN(len(own))]
	}
	me, t := s.Tanks[b.Seat], s.Tanks[tgt]
	d := math.Abs(t.X - me.X)
	for _, w := range []int{proto.IMiniNuke, proto.IMissile} {
		if has(w) && items.All[w].R*2*s.S.Scale+sim.TankW*1.5 < d {
			return w
		}
	}
	return items.Unlimited
}

// harms: weapons that hurt tanks (the rest move dirt).
func harms(w int) bool {
	switch items.All[w].B {
	case items.BTracer, items.BShovel, items.BBuster, items.BDrill, items.BDirt, items.BGeyser, items.BQuake:
		return false
	}
	return true
}

// Value is what shot sh is worth to its seat, played out on a copy of the
// field: harm to others (shields count for less), kills, less what it
// costs the shooter, many times over.
func Value(s *sim.Sim, sh sim.Shot, seed uint64) float64 {
	c := s.Clone(seed)
	me := c.Tanks[sh.Seat]
	before := me.Energy
	lg := c.Resolve([]sim.Shot{sh}, false)
	v := 0.0
	for to := range lg.Damage[sh.Seat] {
		if to != sh.Seat {
			v += lg.Damage[sh.Seat][to] + lg.Shield[sh.Seat][to]*0.3
		}
	}
	for _, k := range lg.Kills[sh.Seat] {
		if k != sh.Seat {
			v += 60
		}
	}
	v -= (before - math.Max(me.Energy, 0)) * 2
	if !me.Alive {
		v -= 300
	}
	return v
}

// choose: the clever tiers try their strongest weapons, each at its best
// aims, on copies of the field, and take the one worth most (a little less
// for what it costs to buy again).
func choose(s *sim.Sim, b *Seat, tier, tgt, maxP int, rng *rand.Rand) (int, cand) {
	inv := b.Inv
	var ws []int
	for w := 0; w < proto.NWeapons; w++ {
		if (w == items.Unlimited || inv[w] > 0) && harms(w) && items.All[w].B != items.BShovel {
			ws = append(ws, w)
		}
	}
	// the strongest few
	strength := func(w int) float64 {
		it := items.All[w]
		return math.Max(it.R, it.R2+it.R3) * math.Max(it.Mult, 1) * float64(max(it.N, 1))
	}
	for i := 1; i < len(ws); i++ {
		for j := i; j > 0 && strength(ws[j]) > strength(ws[j-1]); j-- {
			ws[j], ws[j-1] = ws[j-1], ws[j]
		}
	}
	keep, aims := 4, 1
	if tier == proto.TierDeadeye {
		keep, aims = 6, 3
	}
	if len(ws) > keep {
		ws = ws[:keep]
	}
	if !contains(ws, items.Unlimited) {
		ws = append(ws, items.Unlimited)
	}
	bestW, bestC, bestV := items.Unlimited, cand{angle: 45, power: maxP / 2}, math.Inf(-1)
	seed := rng.Uint64()
	// a shell flies the same whatever it carries, but for boring into dirt:
	// one search for each way of flying
	found := map[int][]cand{}
	for _, w := range ws {
		fly := flight(w)
		cs, ok := found[fly]
		if !ok {
			cs = search(s, b.Seat, tgt, w, 10, 85, maxP, s.S.Wall != proto.WallNone && s.S.Wall != proto.WallConcrete)
			found[fly] = cs
		}
		for k := 0; k < aims && k < len(cs); k++ {
			c := cs[k]
			// worth, over the spread of its own aim (a big blast forgives)
			v := 0.0
			for _, o := range spread[:spreadN(tier)] {
				a := max(min(c.angle+int(math.Round(o[0]*noise[tier].a)), 180), 0)
				p := max(min(c.power+int(math.Round(o[1]*noise[tier].p)), maxP), 30)
				v += Value(s, sim.Shot{Seat: b.Seat, Angle: a, Power: p, Weapon: w, Guide: proto.INone, Target: tgt}, seed)
			}
			v /= float64(spreadN(tier))
			v -= float64(items.All[w].Price) / float64(max(items.All[w].Bundle, 1)) / 1000
			if v > bestV {
				bestW, bestC, bestV = w, c, v
			}
		}
	}
	return bestW, bestC
}

// flight: how a weapon's shell flies (0 tunnels, 1 bursts on contact,
// 2 a laser's beam).
func flight(w int) int {
	switch items.All[w].B {
	case items.BLaser:
		return 2
	case items.BTracer, items.BRoller, items.BFlow, items.BDirt, items.BGeyser:
		return 1
	}
	return 0
}

// spread: aims round the true one, in sigmas, for a bot that misses.
var spread = [][2]float64{{0, 0}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}}

func spreadN(tier int) int {
	if noise[tier].a == 0 && noise[tier].p == 0 {
		return 1
	}
	return len(spread)
}

func contains(a []int, v int) bool {
	for _, x := range a {
		if x == v {
			return true
		}
	}
	return false
}

type cand struct {
	angle, power int
	miss         float64
}

// search: for each angle the power whose shot lands nearest the target
// (bisection on where it comes down); the best few, best first.
func search(s *sim.Sim, seat, tgt, w, lo, hi, maxP int, bank bool) []cand {
	me, t := s.Tanks[seat], s.Tanks[tgt]
	dir := 1.0
	if t.X < me.X {
		dir = -1
	}
	var cs []cand
	blast := items.All[w].R * 2 * s.S.Scale
	try := func(a, p int) float64 {
		h := s.Trace(sim.Shot{Seat: seat, Angle: a, Power: p, Weapon: w, Guide: proto.INone, Target: tgt})
		if h.Seat == tgt {
			return 0
		}
		if h.Seat == seat {
			return 1e6 // never shoot yourself
		}
		if h.Lost {
			return 1e5 + float64(p)
		}
		mx, my := me.Centre()
		if math.Hypot(h.X-mx, h.Y-my) < blast+sim.TankW { // inside its own blast
			return 5e5
		}
		_, ty := t.Centre()
		return math.Hypot(h.X-t.X, (h.Y-ty)*0.5)
	}
	dirs := []float64{dir}
	if bank {
		dirs = append(dirs, -dir)
	}
	for _, d := range dirs {
		for a := lo; a <= hi; a += 5 {
			ang := a
			if d < 0 {
				ang = 180 - a
			}
			// bisection on the landing side
			pl, ph := 30, maxP
			for k := 0; k < 9 && ph-pl > 4; k++ {
				pm := (pl + ph) / 2
				h := s.Trace(sim.Shot{Seat: seat, Angle: ang, Power: pm, Weapon: w, Guide: proto.INone, Target: tgt})
				if (h.X-t.X)*d < 0 && !h.Lost { // short of it
					pl = pm
				} else {
					ph = pm
				}
			}
			bestHere := cand{miss: math.Inf(1)}
			for _, p := range []int{pl, (pl + ph) / 2, ph} {
				if m := try(ang, p); m < bestHere.miss {
					bestHere = cand{ang, p, m}
				}
			}
			cs = append(cs, bestHere)
		}
	}
	for i := 1; i < len(cs); i++ {
		for j := i; j > 0 && cs[j].miss < cs[j-1].miss; j-- {
			cs[j], cs[j-1] = cs[j-1], cs[j]
		}
	}
	if len(cs) == 0 {
		cs = append(cs, cand{angle: 45, power: maxP / 2, miss: math.Inf(1)})
	}
	return cs
}

// lists: what each kind of bot keeps in stock, in the order it buys (item,
// how many it wants to hold).
type want struct{ item, n int }

var (
	basics = []want{{proto.IParachute, 2}, {proto.IShield, 1}, {proto.IMissile, 10}, {proto.IMiniNuke, 3},
		{proto.IBattery, 4}, {proto.IMissile, 20}}
	priority = []want{{proto.IHeavyShield, 1}, {proto.IParachute, 2}, {proto.IBattery, 5}, {proto.IMiniNuke, 6},
		{proto.INapalm, 5}, {proto.IMIRV, 3}, {proto.INuke, 1}, {proto.IRoller, 5}, {proto.IMole, 5},
		{proto.IShield, 1}, {proto.IMissile, 10}, {proto.IHopper, 2}, {proto.IPlasmaBurst, 5}}
	optimal = []want{{proto.ISuperDeflector, 1}, {proto.IHeavyShield, 1}, {proto.IParachute, 3}, {proto.IBattery, 8},
		{proto.INuke, 2}, {proto.IInferno, 2}, {proto.IMegaMIRV, 1}, {proto.IMiniNuke, 6}, {proto.IHeavyRoller, 2},
		{proto.IHeavyMole, 2}, {proto.IBounceShield, 1}, {proto.IPlasmaBurst, 5}, {proto.ILaser, 5},
		{proto.IClusterBomb, 2}, {proto.INuke, 4}}
)

// Shop spends a bot's cash by tier (when the host lets bots shop): Rookies
// at random, the plain tiers the basics, Tacticians and Snipers a longer
// list, Deadeyes the best kit.
func Shop(tier int, cash *int, inv *[proto.NItems]int, arms int, rng *rand.Rand) {
	buy := func(i int) bool {
		it := items.All[i]
		if it.Arms > arms || *cash < it.Price || inv[i]+it.Bundle > items.Most || i == items.Unlimited {
			return false
		}
		*cash -= it.Price
		inv[i] += it.Bundle
		return true
	}
	if tier == proto.TierWildcard {
		tier = rng.IntN(proto.TierWildcard)
	}
	list := basics
	switch tier {
	case proto.TierRookie:
		for k := 0; k < 10; k++ {
			buy(rng.IntN(proto.NItems))
		}
		return
	case proto.TierTactician, proto.TierSniper:
		list = priority
	case proto.TierDeadeye:
		list = optimal
	}
	for _, w := range list {
		for inv[w.item] < w.n && buy(w.item) {
		}
	}
}
