// tilestat measures how many distinct 8x8 tiles a tile-mapped client (the
// Sega Master System's Mode 4) needs to show a field exactly: every frame of
// bot shots played through internal/view at the client's resolution, each
// cell turned into the palette indices of a look model, cells deduplicated
// (and their horizontal mirrors). It prints the high-water marks per scene.
//
// -pools: instead, the blocks of detail a cell store needs (the NES's
// src/nes/fld.s): each frame, the cells mixing sky and dirt, the cells with
// fire and one other value (a block each: the other pixels follow) and the
// cells with all three (two blocks); the high-water of the blocks in all,
// and of the fire's.
//
//	tilestat [-pools] [-w 256 -h 168] [-seeds 3] [-shots 10] [-games 40] [-gshots 30]
package main

import (
	"flag"
	"fmt"
	"math/rand/v2"
	"runtime"
	"sort"
	"strings"
	"sync"

	"fujibarrage/internal/bot"
	"fujibarrage/internal/items"
	"fujibarrage/internal/project"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/sim"
	"fujibarrage/internal/view"
	"fujibarrage/internal/world"
)

// A look turns a field pixel into a palette index.
type look struct {
	name    string
	rowSky  bool // sky and dirt bands by cell row (else by line)
	topsoil int  // dirt lines under sky drawn as topsoil (0: none)
	stars   int  // star tiles for pure-sky cells (0: none)
}

func (l look) px(v *view.View, x, y int) byte {
	p := v.At(x, y)
	ly := y
	if l.rowSky {
		ly = y &^ 7
	}
	switch p {
	case view.PxSky:
		return 1 + byte(ly*5/168) // 5 shades
	case view.PxDirt:
		if l.topsoil > 0 {
			for k := 1; k <= l.topsoil; k++ {
				if v.At(x, y-k) == view.PxSky && y-k >= 0 {
					return 15
				}
			}
		}
		return 7 + byte((ly/9)%4) // 4 strata, every 9 lines
	case view.PxFx:
		return 11 + byte(y&7)%3 // fire by line in the tile
	}
	return 0
}

// model keeps the distinct tiles of a field under one look.
type model struct {
	l     look
	w, h  int
	keys  []string
	count map[string]int
	max   int
}

func newModel(l look, w, h int) *model {
	return &model{l: l, w: w, h: h, keys: make([]string, (w/8)*(h/8)), count: map[string]int{}}
}

func (m *model) key(v *view.View, cx, cy int) string {
	var b, r [64]byte
	sky := true
	for y := 0; y < 8; y++ {
		for x := 0; x < 8; x++ {
			b[y*8+x] = m.l.px(v, cx*8+x, cy*8+y)
			r[y*8+7-x] = b[y*8+x]
			if v.At(cx*8+x, cy*8+y) != view.PxSky {
				sky = false
			}
		}
	}
	if sky && m.l.stars > 0 {
		h := uint32(cx*37+cy*101) * 2654435761
		return fmt.Sprintf("S%d/%d", (h>>24)%uint32(m.l.stars+1), b[0]) // a star tile or plain, per band
	}
	s, t := string(b[:]), string(r[:])
	if t < s {
		return t
	}
	return s
}

func (m *model) update(v *view.View) {
	cw := m.w / 8
	for i := range m.keys {
		k := m.key(v, i%cw, i/cw)
		if k == m.keys[i] {
			continue
		}
		if o := m.keys[i]; o != "" {
			if m.count[o]--; m.count[o] == 0 {
				delete(m.count, o)
			}
		}
		m.keys[i] = k
		m.count[k]++
	}
	if len(m.count) > m.max {
		m.max = len(m.count)
	}
}

var looks = []look{
	{name: "line"},
	{name: "row", rowSky: true},
	{name: "line+stars4", stars: 4},
	{name: "line+top2", topsoil: 2},
	{name: "row+top2+stars4", rowSky: true, topsoil: 2, stars: 4},
}

// pool counts the blocks a cell store needs for v: sky and dirt mixed (d),
// fire and one other (f), all three (f3: two blocks).
type pool struct {
	w, h          int
	max, maxF, d0 int
}

func (p *pool) update(v *view.View) {
	d, f, f3 := 0, 0, 0
	for cy := 0; cy < p.h/8; cy++ {
		for cx := 0; cx < p.w/8; cx++ {
			var has [3]bool
			for y := 0; y < 8; y++ {
				for x := 0; x < 8; x++ {
					switch v.At(cx*8+x, cy*8+y) {
					case view.PxSky:
						has[0] = true
					case view.PxDirt:
						has[1] = true
					case view.PxFx:
						has[2] = true
					}
				}
			}
			n := 0
			for _, b := range has {
				if b {
					n++
				}
			}
			switch {
			case n == 3:
				f3++
			case n == 2 && has[2]:
				f++
			case n == 2:
				d++
			}
		}
	}
	if t := d + f + 2*f3; t > p.max {
		p.max = t
	}
	if t := f + 2*f3; t > p.maxF {
		p.maxF = t
	}
	if d > p.d0 {
		p.d0 = d
	}
}

var pools bool

type result struct {
	scene string
	max   []int
}

func run(c project.Caps, seed uint64, wp world.Params, ceiling bool, shots, weapon, shield int) []int {
	rng := rand.New(rand.NewPCG(seed, 99))
	s := &sim.Sim{T: world.Generate(wp, rng, true), Rng: rng,
		S: sim.Settings{Gravity: 0.2, Wind: float64(rng.IntN(60) - 30), Tunnel: true, TanksFall: true,
			ImpactDamage: true, Scale: 1, Borders: 150, Ceiling: ceiling}}
	var order []int
	for i := 0; i < proto.Seats; i++ {
		s.Tanks[i] = &sim.Tank{Seat: i, Energy: 100, Alive: true, Shield: proto.INone, Angle: 90}
		if shield != proto.INone && i%2 == 0 {
			s.Tanks[i].Shield, s.Tanks[i].ShieldHP = shield, float64(items.All[shield].HP)
		}
		order = append(order, i)
	}
	rng.Shuffle(len(order), func(i, j int) { order[i], order[j] = order[j], order[i] })
	s.Place(order)
	v := view.New(c)
	for _, f := range project.TerrainFrames(nil, c.Entries(s.T), 0, 1) {
		if err := v.Terrain(f); err != nil {
			panic(err)
		}
	}
	ms := make([]*model, len(looks))
	for i, l := range looks {
		ms[i] = newModel(l, c.FieldW, c.FieldH)
		if !pools {
			ms[i].update(v)
		}
	}
	pl0 := &pool{w: c.FieldW, h: c.FieldH}
	if pools {
		ms = nil
		pl0.update(v)
	}
	inv := [proto.NItems]int{}
	for i := range inv {
		inv[i] = 9
	}
	turn := 0
	for n := 1; n <= shots; n++ {
		seat, alive := 0, 0
		for k := 0; k < len(order); k++ {
			if s.Tanks[order[(turn+k)%len(order)]].Alive {
				alive++
			}
		}
		if alive < 2 {
			break
		}
		for k := 0; k < len(order); k++ {
			seat = order[(turn+k)%len(order)]
			if s.Tanks[seat].Alive {
				turn += k + 1
				break
			}
		}
		mem := bot.Memory{Target: -1, RoundTier: proto.TierSniper}
		b := &bot.Seat{Seat: seat, Tier: proto.TierSniper, Inv: &inv, Mem: &mem}
		sh := bot.Aim(s, b, rng)
		if weapon >= 0 {
			sh.Weapon = weapon
		}
		pre := s.T.Clone()
		lg := s.Resolve([]sim.Shot{sh}, false)
		for _, seg := range project.Script(lg, pre, c) {
			pl := v.Play(seg)
			for {
				more, err := pl.Step()
				if err != nil {
					panic(err)
				}
				for _, m := range ms {
					m.update(v)
				}
				if pools {
					pl0.update(v)
				}
				if !more {
					break
				}
			}
			v.EndSegment()
			for _, m := range ms {
				m.update(v)
			}
			if pools {
				pl0.update(v)
			}
		}
	}
	if pools {
		return []int{pl0.max, pl0.maxF, pl0.d0}
	}
	out := make([]int, len(ms))
	for i, m := range ms {
		out[i] = m.max
	}
	return out
}

func main() {
	w := flag.Int("w", 256, "field width")
	h := flag.Int("h", 168, "field height")
	seeds := flag.Int("seeds", 3, "seeds per weapon scene")
	shots := flag.Int("shots", 10, "shots per weapon scene")
	games := flag.Int("games", 40, "bot games per terrain kind")
	gshots := flag.Int("gshots", 30, "shots per bot game")
	flag.BoolVar(&pools, "pools", false, "the cell store's blocks (the NES), not tiles")
	flag.Parse()
	c := project.Caps{FieldW: *w, FieldH: *h, Hz: 60, Par: 73, Buf: 1536, TankW: 7, TankH: 5, Plat: proto.PlatSms}
	type task struct {
		scene string
		f     func() []int
	}
	var tasks []task
	add := func(scene string, f func() []int) { tasks = append(tasks, task{scene, f}) }
	bumpy := world.Params{Bumpiness: 20, Slope: 20}
	for wpn := 0; wpn < proto.NWeapons; wpn++ {
		for sd := 1; sd <= *seeds; sd++ {
			wpn, sd := wpn, sd
			add(fmt.Sprintf("w%02d bumpy s%d", wpn, sd), func() []int { return run(c, uint64(sd), bumpy, false, *shots, wpn, proto.INone) })
			add(fmt.Sprintf("w%02d flat s%d", wpn, sd), func() []int { return run(c, uint64(sd), world.Params{}, false, *shots, wpn, proto.INone) })
			add(fmt.Sprintf("w%02d cavern s%d", wpn, sd), func() []int {
				return run(c, uint64(sd), world.Params{Bumpiness: 20, Slope: 20, Cavern: true}, true, *shots, wpn, proto.INone)
			})
		}
	}
	for g := 1; g <= *games; g++ {
		r := rand.New(rand.NewPCG(uint64(g), 5))
		wp := world.Params{Bumpiness: r.IntN(101), Slope: r.IntN(101)}
		g, wc := g, wp
		wc.Cavern = true
		add(fmt.Sprintf("game b%d/s%d g%d", wp.Bumpiness, wp.Slope, g), func() []int { return run(c, uint64(1000+g), wp, false, *gshots, -1, proto.INone) })
		add(fmt.Sprintf("cave b%d/s%d g%d", wp.Bumpiness, wp.Slope, g), func() []int { return run(c, uint64(2000+g), wc, true, *gshots, -1, proto.INone) })
	}
	res := make([]result, len(tasks))
	var wg sync.WaitGroup
	next := make(chan int)
	for k := 0; k < runtime.NumCPU(); k++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for i := range next {
				res[i] = result{tasks[i].scene, tasks[i].f()}
			}
		}()
	}
	for i := range tasks {
		next <- i
	}
	close(next)
	wg.Wait()
	var names []string
	for _, l := range looks {
		names = append(names, l.name)
	}
	if pools {
		names = []string{"blocks", "fire", "sky/dirt"}
	}
	fmt.Printf("%-28s %s\n", "scene", strings.Join(names, " "))
	for i := range names {
		sort.SliceStable(res, func(a, b int) bool { return res[a].max[i] > res[b].max[i] })
		fmt.Printf("\n== worst 12 by %s ==\n", names[i])
		for _, r := range res[:12] {
			fmt.Printf("%-28s %v\n", r.scene, r.max)
		}
		var all []int
		for _, r := range res {
			all = append(all, r.max[i])
		}
		sort.Ints(all)
		fmt.Printf("median %d  p90 %d  p99 %d  max %d (of %d scenes)\n", all[len(all)/2], all[len(all)*9/10], all[len(all)*99/100], all[len(all)-1], len(all))
	}
}
