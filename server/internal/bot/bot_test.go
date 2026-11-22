package bot

import (
	"math/rand/v2"
	"testing"
	"time"

	"fujibarrage/internal/proto"
	"fujibarrage/internal/sim"
	"fujibarrage/internal/world"
)

var tierName = [proto.NTiers]string{"rookie", "gunner", "ricochet", "lobber", "tactician", "sniper", "deadeye", "wildcard"}

// field: a random field, n tanks placed, a random wind.
func field(seed uint64, n int) *sim.Sim {
	rng := rand.New(rand.NewPCG(seed, 11))
	s := &sim.Sim{T: world.Generate(world.Params{Bumpiness: 30, Slope: 30}, rng, seed%2 == 0), Rng: rng,
		S: sim.Settings{Gravity: 0.2, Wind: float64(rng.IntN(41) - 20), Tunnel: true, TanksFall: true,
			ImpactDamage: true, Scale: 1, Borders: 150}}
	var order []int
	for i := 0; i < n; i++ {
		s.Tanks[i] = &sim.Tank{Seat: i, Energy: 100, Alive: true, Shield: proto.INone}
		order = append(order, i)
	}
	s.Place(order)
	return s
}

// harm is what seat 0's shot does to the others.
func harm(s *sim.Sim, sh sim.Shot, seed uint64) float64 {
	lg := s.Clone(seed).Resolve([]sim.Shot{sh}, false)
	h := 0.0
	for to := 1; to < proto.Seats; to++ {
		h += lg.Damage[0][to]
	}
	return h
}

// The better the tier, the more its first shot hurts, with the same
// missiles, on the same fields, in the same winds.
func TestTiersImprove(t *testing.T) {
	const trials = 120
	var avg [proto.NTiers]float64
	for tier := proto.TierRookie; tier <= proto.TierDeadeye; tier++ {
		start := time.Now()
		for k := uint64(0); k < trials; k++ {
			s := field(k, 4)
			inv := [proto.NItems]int{proto.IMissile: 20}
			mem := Memory{}
			b := &Seat{Seat: 0, Tier: tier, Inv: &inv, Mem: &mem}
			rng := rand.New(rand.NewPCG(k, uint64(tier)))
			NewRound(b, rng)
			avg[tier] += harm(s, Aim(s, b, rng), k) / trials
		}
		t.Logf("%-9s %5.1f harm a shot, %v a shot to think", tierName[tier], avg[tier], time.Since(start)/trials)
	}
	plain := (avg[proto.TierGunner] + avg[proto.TierRicochet] + avg[proto.TierLobber]) / 3
	if !(avg[proto.TierRookie] < plain && plain < avg[proto.TierTactician] &&
		avg[proto.TierTactician] < avg[proto.TierSniper] && avg[proto.TierSniper] <= avg[proto.TierDeadeye]*1.05) {
		t.Errorf("tiers out of order: rookie %.1f, plain %.1f, tactician %.1f, sniper %.1f, deadeye %.1f",
			avg[proto.TierRookie], plain, avg[proto.TierTactician], avg[proto.TierSniper], avg[proto.TierDeadeye])
	}
}

// flatField: dirt from row 300; seat 0 at x 80, the rest where given.
func flatField(xs ...float64) *sim.Sim {
	tr := world.New()
	for x := 0; x < world.W; x++ {
		for y := 300; y < world.H; y++ {
			tr.Set(x, y, true)
		}
	}
	s := &sim.Sim{T: tr, Rng: rand.New(rand.NewPCG(3, 4)),
		S: sim.Settings{Gravity: 0.2, Tunnel: true, TanksFall: true, ImpactDamage: true, Scale: 1, Borders: 150}}
	s.Tanks[0] = &sim.Tank{Seat: 0, X: 80, Y: 300, Energy: 100, Alive: true, Shield: proto.INone}
	for i, x := range xs {
		s.Tanks[i+1] = &sim.Tank{Seat: i + 1, X: x, Y: 300, Energy: 100, Alive: true, Shield: proto.INone}
	}
	return s
}

func deadeyeWeapon(s *sim.Sim, inv [proto.NItems]int) int {
	mem := Memory{}
	b := &Seat{Seat: 0, Tier: proto.TierDeadeye, Inv: &inv, Mem: &mem}
	rng := rand.New(rand.NewPCG(1, 1))
	NewRound(b, rng)
	return Aim(s, b, rng).Weapon
}

// A Deadeye nukes a cluster, and goes under a buried tank.
func TestDeadeyeChoosesWeapons(t *testing.T) {
	s := flatField(440, 480, 520)
	inv := [proto.NItems]int{proto.IMissile: 10, proto.INuke: 1, proto.IMole: 5}
	if w := deadeyeWeapon(s, inv); w != proto.INuke {
		t.Errorf("a cluster of three: weapon %d, want the nuke", w)
	}
	s = flatField(450)
	s.T.Fill(450, 300, 70) // a heap over it
	s.T.Settle(world.Span{X0: 0, X1: world.W})
	inv = [proto.NItems]int{proto.IMissile: 10, proto.IMole: 5}
	w := deadeyeWeapon(s, inv)
	if w != proto.IMole {
		t.Errorf("a buried tank: weapon %d, want the mole", w)
	}
}

// A Lobber closes in: its misses at one target shrink, shot by shot.
func TestLobberBrackets(t *testing.T) {
	var first, last float64
	n := 0
	for k := uint64(0); k < 40; k++ {
		s := field(100+k, 2)
		inv := [proto.NItems]int{}
		mem := Memory{}
		b := &Seat{Seat: 0, Tier: proto.TierLobber, Inv: &inv, Mem: &mem}
		rng := rand.New(rand.NewPCG(k, 9))
		NewRound(b, rng)
		var miss []float64
		for shot := 0; shot < 4; shot++ {
			sh := Aim(s, b, rng)
			h := s.Trace(sh)
			if h.Lost {
				break
			}
			Observe(b, s, h.X)
			miss = append(miss, mem.Miss)
		}
		if len(miss) == 4 {
			first += abs(miss[0])
			last += abs(miss[3])
			n++
		}
	}
	t.Logf("%d rounds: first miss %.1f, fourth %.1f on average", n, first/float64(n), last/float64(n))
	if n < 20 || last >= first {
		t.Errorf("no closing in")
	}
}

func abs(v float64) float64 {
	if v < 0 {
		return -v
	}
	return v
}
