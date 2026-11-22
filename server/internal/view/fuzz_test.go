package view_test

import (
	"math/rand/v2"
	"testing"

	"fujibarrage/internal/project"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/sim"
	"fujibarrage/internal/view"
	"fujibarrage/internal/world"
)

var atari = project.Caps{FieldW: 160, FieldH: 168, Hz: 60, Buf: 2048, TankW: 7, TankH: 4}

// seeds: real scripts, one per weapon.
func seeds() [][]byte {
	var out [][]byte
	for w := 0; w < proto.NWeapons; w++ {
		rng := rand.New(rand.NewPCG(uint64(w), 3))
		s := &sim.Sim{T: world.Generate(world.Params{Bumpiness: 20, Slope: 20}, rng, true), Rng: rng,
			S: sim.Settings{Gravity: 0.2, Wind: 10, Tunnel: true, TanksFall: true, Scale: 1, Borders: 150}}
		var order []int
		for i := 0; i < 4; i++ {
			s.Tanks[i] = &sim.Tank{Seat: i, Energy: 100, Alive: true, Shield: proto.INone}
			order = append(order, i)
		}
		s.Place(order)
		pre := s.T.Clone()
		lg := s.Resolve([]sim.Shot{{Seat: 1, Angle: 50 + w, Power: 350, Weapon: w, Guide: proto.INone}}, false)
		out = append(out, project.Script(lg, pre, atari)...)
	}
	return out
}

// Whatever bytes come, the player stops with an error or plays them out:
// no panic, no endless loop.
func FuzzPlay(f *testing.F) {
	for _, s := range seeds() {
		f.Add(s)
	}
	f.Fuzz(func(t *testing.T, seg []byte) {
		v := view.New(atari)
		pl := v.Play(seg)
		for frames := 0; frames < 20000; frames++ {
			more, err := pl.Step()
			if err != nil || !more {
				break
			}
		}
		v.EndSegment()
		for i := 0; i < 2000 && v.Busy(); i++ { // what's left winds down
			pl.Step()
		}
	})
}
