package game

import (
	"testing"

	"fujibarrage/internal/proto"
	"fujibarrage/internal/sim"
)

// toAim runs g until it's aiming (people mark the shop and defence done).
func toAim(t *testing.T, g *Game) {
	t.Helper()
	for j := 0; j < 60*120 && g.Phase != proto.PhAim; j++ {
		g.Advance(1)
		for i := range g.P {
			if g.P[i].Kind == proto.KindHuman {
				g.Played(i, g.SID)
				if g.Phase == proto.PhShop || g.Phase == proto.PhDefense {
					g.Act(i, proto.ActDone, nil)
				}
			}
		}
	}
	if g.Phase != proto.PhAim {
		t.Fatalf("not aiming: phase %d", g.Phase)
	}
}

// A room for people only: no start alone, no bots, a leaver's tank gone,
// the game over below two people; and the STATE says so.
func TestPeopleOnly(t *testing.T) {
	g := New(7, Defaults(), nil)
	g.PeopleOnly = true
	g.Join("ANN")
	g.Act(0, proto.ActStart, nil)
	if g.Phase != proto.PhWait {
		t.Fatalf("started with one person: phase %d", g.Phase)
	}
	if st := g.State(Viewer{Seat: 0}); st[proto.OffRoomFlags] != proto.RoomPeople {
		t.Errorf("room flags %d", st[proto.OffRoomFlags])
	}
	g.Join("BOB")
	g.Join("CAT")
	g.Act(g.Host, proto.ActStart, nil)
	if g.Phase == proto.PhWait {
		t.Fatal("three people didn't start")
	}
	for i := range g.P {
		if g.P[i].Kind == proto.KindBot {
			t.Fatalf("a bot in seat %d", i)
		}
	}
	toAim(t, g)
	cat := g.SeatOf("CAT")
	g.Act(cat, proto.ActLeave, nil)
	if g.P[cat].Kind != proto.KindEmpty {
		t.Errorf("CAT's seat kind %d after leaving", g.P[cat].Kind)
	}
	if tk := g.tank(cat); tk != nil && tk.Alive {
		t.Error("CAT's tank still alive")
	}
	if g.Phase == proto.PhWait {
		t.Fatal("two people left, but the game is over")
	}
	if err := sane(g); err != nil {
		t.Fatal(err)
	}
	for i := range g.P {
		if g.P[i].Kind == proto.KindBot {
			t.Fatalf("a bot in seat %d after CAT left", i)
		}
	}
	g.Act(g.SeatOf("BOB"), proto.ActLeave, nil)
	if g.Phase != proto.PhWait || g.SeatOf("ANN") < 0 {
		t.Errorf("one person left: phase %d, ANN's seat %d", g.Phase, g.SeatOf("ANN"))
	}
}

// A person whose link is gone too long in a room for people only loses the
// tank (no bot takes it).
func TestPeopleOnlyGone(t *testing.T) {
	g := New(8, Defaults(), nil)
	g.PeopleOnly = true
	for _, n := range []string{"ANN", "BOB", "CAT"} {
		g.Join(n)
	}
	g.Act(g.Host, proto.ActStart, nil)
	toAim(t, g)
	cat := g.SeatOf("CAT")
	g.SetConn(cat, false)
	g.Advance(GoneJ + Hz)
	if g.P[cat].Kind == proto.KindBot {
		t.Fatal("a bot took CAT's tank")
	}
}

// In a cavern every tank stands on the floor, under air, in the field.
func TestCavernPlacement(t *testing.T) {
	set := Defaults()
	set[proto.SetSky] = proto.SkyCavern
	for seed := uint64(1); seed <= 40; seed++ {
		g := botGame(seed, set)
		for j := 0; j < 60*60 && g.Phase != proto.PhAim; j++ {
			g.Advance(1)
		}
		if g.Sky != proto.SkyCavern {
			t.Fatalf("seed %d: sky %d", seed, g.Sky)
		}
		for i := range g.P {
			tk := g.tank(i)
			if tk == nil || !tk.Alive {
				continue
			}
			y, x := int(tk.Y), int(tk.X)
			if y < int(sim.TankH) || g.Sim.T.At(x, y-1) || !g.Sim.T.At(x, y) {
				t.Errorf("seed %d: seat %d at (%.0f,%.0f): not on the floor", seed, i, tk.X, tk.Y)
			}
		}
	}
}
