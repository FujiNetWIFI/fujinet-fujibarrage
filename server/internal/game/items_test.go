package game

import (
	"math"
	"testing"

	"fujibarrage/internal/proto"
	"fujibarrage/internal/sim"
)

// myTurn starts a robin game hosted by seat 0 and runs it to seat 0's first
// aim, with inv given; nil if seat 0 died first.
func myTurn(t *testing.T, inv map[int]int) *Game {
	t.Helper()
	set := Defaults()
	set[proto.SetOrder] = proto.OrderRobin
	set[proto.SetTanks] = 0 // 2 tanks
	g := New(11, set, nil)
	g.Join("ATR")
	g.Act(0, proto.ActStart, nil)
	for j := 0; j < 60*60*5; j++ {
		g.Advance(1)
		switch g.Phase {
		case proto.PhShop:
			for i, n := range inv {
				g.P[0].Inv[i] = n
			}
			g.Act(0, proto.ActDone, nil)
		case proto.PhDefense:
			g.Act(0, proto.ActDone, nil)
		case proto.PhShot:
			g.Played(0, g.SID)
		case proto.PhAim:
			if g.Turn == 0 {
				return g
			}
		}
	}
	t.Fatal("no turn")
	return nil
}

func TestDrive(t *testing.T) {
	g := myTurn(t, map[int]int{proto.IFuel: 20})
	tk := g.tank(0)
	x0 := tk.X
	right := x0 < 320
	moved := 0
	for i := 0; i < 5; i++ {
		if g.drive(0, right) {
			moved++
		}
	}
	if moved == 0 || tk.X == x0 {
		t.Fatalf("didn't move (x %.0f)", tk.X)
	}
	if g.P[0].Inv[proto.IFuel] >= 20 {
		t.Error("no fuel used")
	}
	if got := int(tk.Y); got != g.Sim.T.Surface(int(tk.X)) && g.Sim.T.At(int(tk.X), got-1) {
		t.Errorf("tank at y %d, not on the ground", got)
	}
	// no fuel: no move
	g.P[0].Inv[proto.IFuel] = 0
	x := tk.X
	if g.drive(0, right) || tk.X != x {
		t.Error("drove with no fuel")
	}
	// a wall stops it
	g.P[0].Inv[proto.IFuel] = 20
	nx := int(tk.X + DriveStep)
	if !right {
		nx = int(tk.X - DriveStep)
	}
	for y := int(tk.Y) - 30; y < int(tk.Y); y++ {
		g.Sim.T.Set(nx, y, true)
	}
	if g.drive(0, right) {
		t.Error("drove up a wall")
	}
}

func TestBatteryAndShield(t *testing.T) {
	g := myTurn(t, map[int]int{proto.IBattery: 2, proto.IShield: 1})
	tk := g.tank(0)
	tk.Energy = 85
	g.Act(0, proto.ActUse, []byte{proto.IBattery})
	g.Act(0, proto.ActUse, []byte{proto.IBattery})
	if tk.Energy != 100 || g.P[0].Inv[proto.IBattery] != 0 {
		t.Errorf("energy %.0f batteries %d", tk.Energy, g.P[0].Inv[proto.IBattery])
	}
	g.Act(0, proto.ActUse, []byte{proto.IShield})
	if tk.Shield != proto.IShield || tk.ShieldHP != 100 {
		t.Errorf("shield %d hp %.0f", tk.Shield, tk.ShieldHP)
	}
}

// The Ballistic Computer finds the power that hits, wind and all.
func TestBallistic(t *testing.T) {
	g := myTurn(t, map[int]int{proto.IBallistic: 2})
	g.Sim.S.Wind = 25
	var tgt int
	for i, tk := range g.Sim.Tanks {
		if tk != nil && tk.Alive && i != 0 {
			tgt = i
		}
	}
	me, them := g.tank(0), g.tank(tgt)
	angle := 60
	if them.X < me.X {
		angle = 120
	}
	sh := sim.Shot{Seat: 0, Angle: angle, Power: 100, Weapon: proto.IMissile, Guide: proto.IBallistic, Target: tgt}
	p := g.ballistic(sh)
	sh.Power, sh.Guide = p, proto.INone
	h := g.Sim.Trace(sh)
	if h.Seat != tgt && math.Abs(h.X-them.X) > sim.TankW {
		t.Errorf("power %d lands at %.0f, the target is at %.0f", p, h.X, them.X)
	}
}

// A Defense Computer left to itself raises the best shield and arms the
// parachute, and is used up.
func TestDefenseComputer(t *testing.T) {
	g := myTurn(t, map[int]int{proto.IDefenseComputer: 1, proto.IShield: 1, proto.IHeavyShield: 1, proto.IParachute: 2})
	tk := g.tank(0)
	if tk.Shield != proto.IHeavyShield || !tk.Chute || g.P[0].Inv[proto.IDefenseComputer] != 0 {
		t.Errorf("shield %d chute %v computers %d", tk.Shield, tk.Chute, g.P[0].Inv[proto.IDefenseComputer])
	}
}
