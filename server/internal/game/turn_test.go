package game

import (
	"testing"

	"fujibarrage/internal/proto"
)

// A person hosting against bots gets their turn (sequential), and the turn
// passes on when they don't act (the timer).
func TestPersonGetsTurns(t *testing.T) {
	set := Defaults()
	set[proto.SetOrder] = proto.OrderRobin // seat 0 first in round 1
	g := New(7, set, nil)
	if s := g.Join("ATR"); s != 0 {
		t.Fatalf("seat %d", s)
	}
	g.Act(0, proto.ActStart, nil)
	turns := map[int]int{}
	last := proto.NoSeat
	for j := 0; j < 60*60*20 && g.Round <= 1; j++ {
		g.Advance(1)
		if g.Phase == proto.PhShop || g.Phase == proto.PhDefense {
			g.Act(0, proto.ActDone, nil)
		}
		if g.Phase == proto.PhShot {
			if g.Played(0, g.SID); g.P[0].Played == g.SID && g.Shot != nil && g.Shot.Log != nil && len(g.Shot.Log.Evs) > 0 && j%1 == 0 {
				var dead []int
				for i, tk := range g.Sim.Tanks {
					if tk != nil && !tk.Alive {
						dead = append(dead, i)
					}
				}
				if len(dead) > 0 && g.J%60 == 0 {
					t.Logf("after shot %d: dead %v, energies %v", g.SID, dead, func() (e []int) {
						for _, tk := range g.Sim.Tanks {
							if tk != nil {
								e = append(e, int(tk.Energy))
							}
						}
						return
					}())
				}
			}
		}
		if g.Phase == proto.PhAim && g.Turn != last {
			last = g.Turn
			turns[g.Turn]++
		}
	}
	t.Logf("turns by seat: %v; order %v", turns, g.order)
	if turns[0] == 0 {
		t.Error("the person never got a turn")
	}
}
