// arcdump is refview's set-up and shots with every frame dumped raw, for the
// Arcadia graphics study (tools/arc/mock_arc.py): not a client reference.
//
//	arcdump [-w 48 -h 48 ...] [-seed 1] [-shots 1] [-weapon n] -out dir
//
// It writes OUT/shotN.dmp: per frame "FR" u16 frame, u8 flash, u8 shooter,
// then 10 x (on, x, y, angle/2) bytes, then the field (W*H bytes, view.Px*).
// The first record of each file is the field before the shot.
package main

import (
	"encoding/binary"
	"flag"
	"fmt"
	"log"
	"math/rand/v2"
	"os"
	"path/filepath"

	"fujibarrage/internal/bot"
	"fujibarrage/internal/items"
	"fujibarrage/internal/project"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/sim"
	"fujibarrage/internal/view"
	"fujibarrage/internal/world"
)

func main() {
	seed := flag.Uint64("seed", 1, "seed")
	w := flag.Int("w", 48, "field width, pixels")
	h := flag.Int("h", 48, "field height, pixels")
	hz := flag.Int("hz", 60, "frame rate")
	par := flag.Int("par", 78, "pixel aspect, 64ths")
	buf := flag.Int("buf", 240, "script buffer")
	shots := flag.Int("shots", 1, "shots")
	weapon := flag.Int("weapon", -1, "weapon for every shot (-1: the bots')")
	walls := flag.Int("walls", proto.WallNone, "walls")
	out := flag.String("out", "arcdump", "output directory")
	flat := flag.Bool("flat", false, "a flat field")
	shield := flag.Int("shield", proto.INone, "a shield on the even seats")
	tankW := flag.Int("tankw", 2, "tank width")
	tankH := flag.Int("tankh", 1, "tank height")
	plat := flag.Int("plat", 0, "the client's platform (proto PLAT_*): its caps as the server projects them from its HELLO (the Arcadia's 6 track slots)")
	flag.Parse()
	c := project.Caps{FieldW: *w, FieldH: *h, Hz: *hz, Par: *par, Buf: *buf, TankW: *tankW, TankH: *tankH, Plat: *plat, Slots: project.PlatSlots(*plat)}
	if err := c.Check(); err != nil {
		log.Fatal(err)
	}
	os.MkdirAll(*out, 0o755)

	rng := rand.New(rand.NewPCG(*seed, 99))
	wp := world.Params{Bumpiness: 20, Slope: 20}
	if *flat {
		wp = world.Params{}
	}
	s := &sim.Sim{T: world.Generate(wp, rng, true), Rng: rng,
		S: sim.Settings{Gravity: 0.2, Wind: float64(rng.IntN(60) - 30), Tunnel: true, TanksFall: true,
			ImpactDamage: true, Scale: 1, Borders: 150, Wall: *walls}}
	var order []int
	for i := 0; i < proto.Seats; i++ {
		s.Tanks[i] = &sim.Tank{Seat: i, Energy: 100, Alive: true, Shield: proto.INone, Angle: 90}
		if *shield != proto.INone && i%2 == 0 {
			s.Tanks[i].Shield, s.Tanks[i].ShieldHP = *shield, float64(items.All[*shield].HP)
		}
		order = append(order, i)
	}
	rng.Shuffle(len(order), func(i, j int) { order[i], order[j] = order[j], order[i] })
	s.Place(order)

	v := view.New(c)
	for _, f := range project.TerrainFrames(nil, c.Entries(s.T), 0, 1) {
		if err := v.Terrain(f); err != nil {
			log.Fatal(err)
		}
	}
	place := func() {
		for i, t := range s.Tanks {
			if t == nil {
				continue
			}
			v.Tanks[i] = view.Tank{On: t.Alive, X: c.IX(t.X), Y: c.TankY(s.T, t.X, t.Y), Angle: t.Angle, Energy: int(t.Energy)}
		}
	}
	place()

	inv := [proto.NItems]int{}
	for i := range inv {
		inv[i] = 9
	}
	turn := 0
	for n := 1; n <= *shots; n++ {
		var seat int
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
		if *weapon >= 0 {
			sh.Weapon = *weapon
		}
		sh.Talk = "TAKE THAT!"
		pre := s.T.Clone()
		lg := s.Resolve([]sim.Shot{sh}, false)
		segs := project.Script(lg, pre, c)
		var dmp []byte
		rec := func() {
			dmp = append(dmp, 'F', 'R')
			dmp = binary.LittleEndian.AppendUint16(dmp, uint16(v.Frame))
			dmp = append(dmp, byte(min(v.Flash, 255)), byte(seat))
			for _, t := range v.Tanks {
				on := byte(0)
				if t.On {
					on = 1
				}
				dmp = append(dmp, on, byte(t.X), byte(t.Y), byte(t.Angle/2))
			}
			dmp = append(dmp, v.Px...)
		}
		rec()
		nb := 0
		for _, seg := range segs {
			nb += len(seg)
			pl := v.Play(seg)
			for {
				more, err := pl.Step()
				if err != nil {
					log.Fatalf("shot %d: %v", n, err)
				}
				rec()
				if !more {
					break
				}
			}
			v.EndSegment()
		}
		place()
		rec()
		os.WriteFile(filepath.Join(*out, fmt.Sprintf("shot%d.dmp", n)), dmp, 0o644)
		fmt.Printf("shot %d: seat %d weapon %d angle %d power %d: %d bytes in %d segment(s), %.1f s\n",
			n, seat, sh.Weapon, sh.Angle, sh.Power, nb, len(segs), lg.End)
	}
}
