package sim

import (
	"math"

	"fujibarrage/internal/world"
)

// Place puts the tanks in order (seats) into evenly spaced slots across
// the field, each sunk until it sits flat: the dirt above its footprint's
// lowest point is carved away.
func (s *Sim) Place(order []int) {
	n := len(order)
	if n == 0 {
		return
	}
	space := float64(world.W-8) / float64(n)
	for i, seat := range order {
		t := s.Tanks[seat]
		if t == nil {
			continue
		}
		t.X = math.Round(4 + space/2 + float64(i)*space)
		s.Sink(t)
	}
}

// Sink settles t onto the dirt under it, flattening a pad.
func (s *Sim) Sink(t *Tank) {
	x0 := int(math.Floor(t.X - TankW/2))
	y := 0
	for x := x0; x < x0+int(TankW); x++ {
		y = max(y, s.T.Surface(x))
	}
	if y > world.H {
		y = world.H
	}
	for x := x0; x < x0+int(TankW); x++ {
		for yy := 0; yy < y; yy++ {
			if yy >= y-int(TankH)-int(Barrel) {
				s.T.Set(x, yy, false)
			}
		}
	}
	t.Y = float64(y)
}
