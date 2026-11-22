package game

import (
	"testing"
	"time"

	"fujibarrage/internal/proto"
)

// The live rig's room: dev1 (seed 7 + 101), tanks 10, robin order, timer
// 30, no wind; the person fires 90 at 210 on their turn.
func TestRigRoom(t *testing.T) {
	set := Defaults()
	set[proto.SetTanks] = 8
	set[proto.SetOrder] = proto.OrderRobin
	set[proto.SetTimer] = 4
	set[proto.SetMaxWind] = 0
	g := New(108, set, nil)
	g.Join("ATR")
	g.Act(0, proto.ActStart, nil)
	last := -1
	for j := 0; j < 60*240 && g.Round <= 1; j++ {
		t0 := time.Now()
		g.Advance(1)
		if d := time.Since(t0); d > 200*time.Millisecond {
			t.Logf("jiffy %d took %v (phase %d turn %d)", g.J, d, g.Phase, g.Turn)
		}
		if g.Phase == proto.PhShop || g.Phase == proto.PhDefense {
			g.Act(0, proto.ActDone, nil)
		}
		if g.Phase == proto.PhShot {
			g.Played(0, g.SID)
		}
		if g.Phase == proto.PhAim && g.Turn != last {
			last = g.Turn
			t.Logf("jiffy %d: turn %d", g.J, g.Turn)
			if g.Turn == 0 {
				g.Act(0, proto.ActFire, []byte{90, 210, 0, 0, proto.INone, proto.NoSeat})
			}
		}
	}
}
