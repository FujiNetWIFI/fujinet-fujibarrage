package project_test

import (
	"fmt"
	"math/rand/v2"
	"testing"

	"fujibarrage/internal/items"
	. "fujibarrage/internal/project"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/sim"
	"fujibarrage/internal/view"
	"fujibarrage/internal/world"
)

var shapes = []Caps{
	{FieldW: 160, FieldH: 168, Hz: 60, Buf: 3072, TankW: 7, TankH: 4},  // Atari mode E
	{FieldW: 160, FieldH: 168, Hz: 50, Buf: 3072, TankW: 7, TankH: 4},  // the same, PAL
	{FieldW: 280, FieldH: 160, Hz: 60, Buf: 4096, TankW: 9, TankH: 5},  // Apple II high-res
	{FieldW: 256, FieldH: 176, Hz: 50, Buf: 2048, TankW: 8, TankH: 5},  // TMS9918, PAL
	{FieldW: 40, FieldH: 40, Hz: 60, Buf: 512, TankW: 3, TankH: 2},     // a tiny one
	{FieldW: 320, FieldH: 176, Hz: 60, Buf: 8192, TankW: 12, TankH: 6}, // CGA
	{FieldW: 160, FieldH: 80, Hz: 60, Buf: 1024, TankW: 7, TankH: 5},   // Intellivision
	{FieldW: 160, FieldH: 80, Hz: 50, Buf: 1024, TankW: 7, TankH: 5},   // the same, PAL
	{FieldW: 160, FieldH: 56, Hz: 60, Buf: 240, TankW: 7, TankH: 5},    // Astrocade
	{FieldW: 140, FieldH: 160, Hz: 60, Buf: 1536, TankW: 5, TankH: 5},  // Apple II (HGR colour pixels)
	{FieldW: 140, FieldH: 160, Hz: 50, Buf: 1536, TankW: 5, TankH: 5},  // the same, PAL
}

func field(seed uint64) *sim.Sim {
	rng := rand.New(rand.NewPCG(seed, 7))
	s := &sim.Sim{T: world.Generate(world.Params{Bumpiness: 20, Slope: 20}, rng, true), Rng: rng,
		S: sim.Settings{Gravity: 0.2, Wind: 30, Tunnel: true, TanksFall: true, ImpactDamage: true, Scale: 1, Borders: 150}}
	var order []int
	for i := 0; i < proto.Seats; i++ {
		s.Tanks[i] = &sim.Tank{Seat: i, Energy: 100, Alive: true, Shield: proto.INone}
		order = append(order, i)
	}
	s.Place(order)
	return s
}

// check plays lg's script at every shape and says where a client's field
// differs from the world's (downsampled) after it.
func check(t *testing.T, label string, s *sim.Sim, pre *world.Terrain, lg *sim.Log) {
	t.Helper()
	for _, c := range shapes {
		v := view.New(c)
		for _, f := range TerrainFrames(nil, c.Entries(pre), 0, 1) {
			if err := v.Terrain(f); err != nil {
				t.Fatal(err)
			}
		}
		segs := Script(lg, pre, c)
		for _, seg := range segs {
			if len(seg) > c.Buf {
				t.Fatalf("%s %s: segment of %d bytes > %d", label, c.Key(), len(seg), c.Buf)
			}
			pl := v.Play(seg)
			for {
				more, err := pl.Step()
				if err != nil {
					t.Fatalf("%s %s: %v", label, c.Key(), err)
				}
				if !more {
					break
				}
			}
			v.EndSegment()
		}
		if v.Busy() {
			t.Errorf("%s %s: still animating at OP_END", label, c.Key())
		}
		want := c.Entries(s.T)
		bad := 0
		for x := 0; x < c.FieldW; x++ {
			col, _, _ := DecodeEntry(want[x], c.FieldH)
			for y := 0; y < c.FieldH; y++ {
				if col[y] != (v.At(x, y) == view.PxDirt) {
					bad++
					if bad < 4 {
						t.Logf("%s %s: (%d,%d) world %v view %d", label, c.Key(), x, y, col[y], v.At(x, y))
					}
				}
			}
		}
		if bad > 0 {
			t.Errorf("%s %s: %d pixels differ from the world after the script", label, c.Key(), bad)
		}
	}
}

// The drawn path stays on the true one, every frame, at every shape; and
// after the script, each client's field equals the world's, downsampled.
func TestScriptRoundTrip(t *testing.T) {
	for seed := uint64(1); seed <= 12; seed++ {
		s := field(seed)
		pre := s.T.Clone()
		shot := sim.Shot{Seat: 2, Angle: 30 + int(seed*9)%120, Power: 250 + int(seed*37)%500,
			Weapon: proto.IMissile, Guide: proto.INone}
		if seed%3 == 0 {
			shot.Weapon = proto.INuke
		}
		lg := s.Resolve([]sim.Shot{shot}, false)
		check(t, fmt.Sprintf("seed %d", seed), s, pre, lg)
	}
}

// Every weapon, every wall, dirt that falls and dirt that stays: the
// invariant holds.
func TestEveryWeaponRoundTrip(t *testing.T) {
	walls := []int{proto.WallNone, proto.WallWrap, proto.WallRubber, proto.WallConcrete}
	for w := 0; w < proto.NWeapons; w++ {
		for k, wall := range walls {
			seed := uint64(100 + w*7 + k)
			s := field(seed)
			s.S.Wall = wall
			s.S.Suspend = []int{0, 100}[k%2]
			s.Tanks[4].Shield, s.Tanks[4].ShieldHP = proto.IShield, 100
			s.Tanks[6].Shield, s.Tanks[6].ShieldHP = proto.IBounceShield, 100
			s.Tanks[7].Shield, s.Tanks[7].ShieldHP = proto.IDeflector, 55
			pre := s.T.Clone()
			shot := sim.Shot{Seat: 3, Angle: 40 + int(seed*13)%100, Power: 200 + int(seed*41)%450,
				Weapon: w, Guide: proto.INone, Target: proto.NoSeat}
			lg := s.Resolve([]sim.Shot{shot}, false)
			check(t, fmt.Sprintf("%s wall %d", items.All[w].Name, wall), s, pre, lg)
		}
	}
}

// How many segments each weapon's shot takes in a small client's script
// buffer (go test -run SegmentCounts -v): the Astrocade's 4K holds 240 bytes
// (it trades field lines for buffer bytes), the Apple II's main RAM 1536
// (1024 if it must).
func TestSegmentCounts(t *testing.T) {
	segmentCounts(t, Caps{FieldW: 160, FieldH: 56, Hz: 60, TankW: 7, TankH: 5}, []int{240, 320, 400})
	segmentCounts(t, Caps{FieldW: 140, FieldH: 160, Hz: 60, TankW: 5, TankH: 5}, []int{1024, 1536})
}

func segmentCounts(t *testing.T, shape Caps, bufs []int) {
	t.Helper()
	t.Logf("%dx%d:", shape.FieldW, shape.FieldH)
	tot := make([]int, len(bufs))
	for w := 0; w < proto.NWeapons; w++ {
		line := fmt.Sprintf("%-18s", items.All[w].Name)
		for i, b := range bufs {
			n, most := 0, 0
			for k := 0; k < 4; k++ {
				seed := uint64(100 + w*7 + k)
				s := field(seed)
				pre := s.T.Clone()
				shot := sim.Shot{Seat: 3, Angle: 40 + int(seed*13)%100, Power: 200 + int(seed*41)%450,
					Weapon: w, Guide: proto.INone, Target: proto.NoSeat}
				lg := s.Resolve([]sim.Shot{shot}, false)
				c := shape
				c.Buf = b
				segs := Script(lg, pre, c)
				n += len(segs)
				if len(segs) > most {
					most = len(segs)
				}
			}
			tot[i] += n
			line += fmt.Sprintf("  %d: %4.1f (most %d)", b, float64(n)/4, most)
		}
		t.Log(line)
	}
	for i, b := range bufs {
		t.Logf("buffer %d: %.2f segments a shot on average", b, float64(tot[i])/float64(4*proto.NWeapons))
	}
}
