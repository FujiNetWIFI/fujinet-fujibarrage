package game

import (
	"fmt"
	"math/rand/v2"
	"testing"

	"fujibarrage/internal/items"
	"fujibarrage/internal/project"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/world"
)

var atariCaps = project.Caps{FieldW: 160, FieldH: 168, Hz: 60, Buf: 2048, TankW: 7, TankH: 5}

// sane: what must hold at every jiffy.
func sane(g *Game) error {
	if g.Phase < proto.PhWait || g.Phase > proto.PhGameOver {
		return fmt.Errorf("phase %d", g.Phase)
	}
	for i := range g.P {
		p := &g.P[i]
		if p.Cash < 0 {
			return fmt.Errorf("seat %d cash %d", i, p.Cash)
		}
		for it, n := range p.Inv {
			if n < 0 || n > items.Most {
				return fmt.Errorf("seat %d has %d of item %d", i, n, it)
			}
		}
		t := g.tank(i)
		if t == nil {
			continue
		}
		if t.Energy > 100 {
			return fmt.Errorf("seat %d energy %.1f", i, t.Energy)
		}
		if t.Alive && (t.X < 0 || t.X >= world.W || t.Y < 0 || t.Y > world.H) {
			return fmt.Errorf("seat %d at (%.0f,%.0f)", i, t.X, t.Y)
		}
	}
	if g.Phase == proto.PhAim && g.Set[proto.SetMode] == proto.ModeSequential {
		if t := g.tank(g.Turn); t == nil || !t.Alive {
			return fmt.Errorf("turn %d isn't a live tank", g.Turn)
		}
	}
	return nil
}

// play runs g (bots only, or people who do nothing but watch) to its game
// over, checking it all the way; the shots fired.
func play(t *testing.T, g *Game, label string, maxJ int) int {
	t.Helper()
	shots, lastSID := 0, g.SID
	for j := 0; j < maxJ; j++ {
		g.Advance(1)
		if err := sane(g); err != nil {
			t.Fatalf("%s, round %d, jiffy %d: %v", label, g.Round, g.J, err)
		}
		if g.Phase == proto.PhShot && g.SID != lastSID {
			lastSID = g.SID
			shots++
			for _, seg := range g.Shot.Script(atariCaps) {
				if len(seg) > atariCaps.Buf {
					t.Fatalf("%s: a %d-byte segment", label, len(seg))
				}
			}
			for i := range g.P {
				g.Played(i, g.SID)
			}
		}
		if (g.Phase == proto.PhShop || g.Phase == proto.PhDefense) && g.J%30 == 0 {
			for i := range g.P {
				if g.P[i].Kind == proto.KindHuman {
					g.Act(i, proto.ActDone, nil)
				}
			}
		}
		if g.Phase == proto.PhGameOver {
			return shots
		}
	}
	t.Fatalf("%s: no game over after %d jiffies (round %d of %d, phase %d)", label, maxJ, g.Round, g.Rounds, g.Phase)
	return shots
}

func botGame(seed uint64, set [proto.NSettings]int) *Game {
	g := New(seed, set, nil)
	g.start()
	return g
}

// Ten bots finish a game in every mode, with every wall.
func TestEveryModeAndWall(t *testing.T) {
	for mode := 0; mode < 3; mode++ {
		for wall := 0; wall <= proto.WallErratic; wall++ {
			set := Defaults()
			set[proto.SetMode], set[proto.SetWalls], set[proto.SetRounds] = mode, wall, 1 // 2 rounds
			g := botGame(uint64(10*mode+wall), set)
			n := play(t, g, fmt.Sprintf("mode %d walls %d", mode, wall), 60*60*60)
			t.Logf("mode %d walls %d: %d shots, winner %s", mode, wall, n, g.P[g.Result].Name)
		}
	}
}

// Any settings at all: a game runs to its end.
func TestSettingsFuzz(t *testing.T) {
	rng := rand.New(rand.NewPCG(77, 1))
	for k := 0; k < 40; k++ {
		var set [proto.NSettings]int
		for i, sp := range Specs {
			set[i] = rng.IntN(sp.N())
		}
		set[proto.SetRounds] = rng.IntN(2) // keep it short
		g := botGame(uint64(k), set)
		play(t, g, fmt.Sprintf("fuzz %d %v", k, set), 60*60*60)
	}
}

// Buying a bundle, selling at 80%, interest between rounds.
func TestShop(t *testing.T) {
	set := Defaults()
	set[proto.SetInterest] = 2 // 10%
	g := New(1, set, nil)
	g.Join("ANN")
	g.Act(0, proto.ActStart, nil)
	if g.Phase != proto.PhShop {
		t.Fatalf("phase %d", g.Phase)
	}
	p := &g.P[0]
	cash := p.Cash
	g.Act(0, proto.ActBuy, []byte{proto.IMissile, 2})
	it := items.All[proto.IMissile]
	if p.Inv[proto.IMissile] != 2*it.Bundle || p.Cash != cash-2*it.Price {
		t.Fatalf("bought: %d missiles, cash %d", p.Inv[proto.IMissile], p.Cash)
	}
	g.Act(0, proto.ActSell, []byte{proto.IMissile, 5})
	want := cash - 2*it.Price + 5*it.Price*8/10/it.Bundle
	if p.Inv[proto.IMissile] != 2*it.Bundle-5 || p.Cash != want {
		t.Fatalf("sold: %d missiles, cash %d, want %d", p.Inv[proto.IMissile], p.Cash, want)
	}
	g.Act(0, proto.ActBuy, []byte{proto.INuke, 1})
	if g.Set[proto.SetArms] < items.All[proto.INuke].Arms && p.Inv[proto.INuke] > 0 {
		t.Error("bought above the arms level")
	}
	before := p.Cash
	g.Act(0, proto.ActDone, nil)
	g.Advance(1)
	if p.Cash != before+before/10 {
		t.Errorf("interest: cash %d, want %d", p.Cash, before+before/10)
	}
}

// Arms level 0: nothing above it in the shop, bots included.
func TestArmsLevel(t *testing.T) {
	set := Defaults()
	set[proto.SetArms] = 0
	g := botGame(5, set)
	for i := range g.P {
		for it, n := range g.P[i].Inv {
			if n > 0 && items.All[it].Arms > 0 {
				t.Errorf("seat %d bought %s", i, items.All[it].Name)
			}
		}
	}
}

// Someone who comes in during a game takes a bot's tank at the next round;
// when every person has gone, the room is back to waiting.
func TestJoinMidGameAndReset(t *testing.T) {
	set := Defaults()
	set[proto.SetRounds] = 2
	g := New(3, set, nil)
	g.Join("ANN")
	g.Act(0, proto.ActStart, nil)
	if s := g.Join("BOB"); s != -1 {
		t.Fatalf("BOB seated at once in seat %d", s)
	}
	for j := 0; j < 60*60*60 && g.Round < 2; j++ {
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
	if g.SeatOf("BOB") < 0 || g.P[g.SeatOf("BOB")].Kind != proto.KindHuman {
		t.Fatal("BOB has no tank in round 2")
	}
	g.Act(g.SeatOf("ANN"), proto.ActLeave, nil)
	if g.Host != g.SeatOf("BOB") {
		t.Errorf("host %d, want BOB's seat", g.Host)
	}
	g.Act(g.SeatOf("BOB"), proto.ActLeave, nil)
	if g.Phase != proto.PhWait || g.count() != 0 {
		t.Errorf("after everyone left: phase %d, %d seats taken", g.Phase, g.count())
	}
}

// Losers first: the fewest wins (then the lowest score) shoot first;
// winners first the other way round.
func TestTurnOrders(t *testing.T) {
	for _, o := range []int{proto.OrderLosers, proto.OrderWinners} {
		set := Defaults()
		set[proto.SetOrder] = o
		g := botGame(9, set)
		for i := range g.P {
			g.P[i].Wins, g.P[i].Score = i%3, i*10
		}
		var seats []int
		for i := range g.P {
			if g.P[i].Kind != proto.KindEmpty {
				seats = append(seats, i)
			}
		}
		ord := g.turnOrder(seats)
		for k := 1; k < len(ord); k++ {
			a, b := &g.P[ord[k-1]], &g.P[ord[k]]
			worse := a.Wins < b.Wins || a.Wins == b.Wins && a.Score <= b.Score
			if (o == proto.OrderLosers) != worse {
				t.Errorf("order %d: seat %d (%d wins) before seat %d (%d wins)", o, ord[k-1], a.Wins, ord[k], b.Wins)
			}
		}
	}
}

// Only the host sets the settings, in the waiting room, within range.
func TestHostSettings(t *testing.T) {
	g := New(1, Defaults(), nil)
	g.Join("ANN")
	g.Join("BOB")
	g.Act(1, proto.ActSet, []byte{proto.SetWalls, 3})
	if g.Set[proto.SetWalls] != 0 {
		t.Error("a guest changed the walls")
	}
	g.Act(0, proto.ActSet, []byte{proto.SetWalls, 3})
	g.Act(0, proto.ActSet, []byte{proto.SetTanks, 200})
	if g.Set[proto.SetWalls] != 3 || g.Set[proto.SetTanks] != Specs[proto.SetTanks].N()-1 {
		t.Errorf("walls %d tanks %d", g.Set[proto.SetWalls], g.Set[proto.SetTanks])
	}
	g.Act(0, proto.ActStart, nil)
	g.Act(0, proto.ActSet, []byte{proto.SetWalls, 1})
	if g.Set[proto.SetWalls] != 3 {
		t.Error("settings changed during the game")
	}
}
