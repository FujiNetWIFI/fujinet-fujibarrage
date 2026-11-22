package sim

import (
	"math"
	"math/rand/v2"
	"testing"

	"fujibarrage/internal/items"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/world"
)

// ground is the flat field's top row.
const ground = 300

// flat: dirt from row 300 down; seat 0 at x 100, seat 1 at x 400.
func flat() *Sim {
	t := world.New()
	for x := 0; x < world.W; x++ {
		for y := ground; y < world.H; y++ {
			t.Set(x, y, true)
		}
	}
	s := &Sim{T: t, Rng: rand.New(rand.NewPCG(1, 2)),
		S: Settings{Gravity: 0.2, Scale: 1, TanksFall: true, ImpactDamage: true, Borders: 150}}
	s.Tanks[0] = &Tank{Seat: 0, X: 100, Y: ground, Energy: 100, Alive: true, Shield: proto.INone}
	s.Tanks[1] = &Tank{Seat: 1, X: 400, Y: ground, Energy: 100, Alive: true, Shield: proto.INone}
	return s
}

// drop lets weapon w (seat 0's) fall onto (x, ground) from 40 above.
func (s *Sim) drop(w int, x float64) *Log {
	return s.dropV(w, x, ground-40, 0, 300)
}

func (s *Sim) dropV(w int, x, y, vx, vy float64) *Log {
	s.log = &Log{}
	s.now = 0
	for _, t := range s.Tanks {
		if t != nil {
			t.lift, t.lastBy = 1000, -1
		}
	}
	p := &shell{x: x, y: y, vx: vx, vy: vy, seat: 0, w: w, it: &items.All[w], guide: proto.INone}
	if p.it.B == items.BHopper {
		p.hops = 2
	}
	p.trk = s.newTrack(proto.StShell, 0, x, y)
	s.shells = []*shell{p}
	s.run()
	s.aftermath()
	s.log.End = s.now
	return s.log
}

func solid(t *world.Terrain) int {
	n := 0
	for _, c := range t.Cells {
		n += int(c)
	}
	return n
}

func evs(lg *Log, op byte) []Ev {
	var out []Ev
	for _, e := range lg.Evs {
		if e.Op == op {
			out = append(out, e)
		}
	}
	return out
}

func damaged(lg *Log, seat int) bool {
	for _, e := range evs(lg, proto.OpDamage) {
		if e.Seat == seat {
			return true
		}
	}
	return false
}

func TestMissileCratersAndHurts(t *testing.T) {
	s := flat()
	before := solid(s.T)
	lg := s.drop(proto.IMissile, 380)
	if solid(s.T) >= before {
		t.Error("no crater")
	}
	if !damaged(lg, 1) || s.Tanks[1].Energy >= 100 {
		t.Error("the tank 20 away wasn't hurt")
	}
	if damaged(lg, 0) {
		t.Error("the shooter, far off, was hurt")
	}
}

func TestDirectHitKills(t *testing.T) {
	s := flat()
	s.drop(proto.IMiniMissile, 400)
	if s.Tanks[1].Alive {
		t.Error("a direct hit left the tank alive")
	}
}

func TestHopperThreeBlasts(t *testing.T) {
	s := flat()
	lg := s.dropV(proto.IHopper, 200, ground-40, 150, 300)
	if n := len(evs(lg, proto.OpExplode)); n != 3 {
		t.Errorf("%d blasts, want 3", n)
	}
}

func TestClusterBomb(t *testing.T) {
	s := flat()
	lg := s.drop(proto.IClusterBomb, 250)
	if n := len(evs(lg, proto.OpExplode)); n < 7 || n > 12 {
		t.Errorf("%d blasts, want centre + 5-10 bomblets + the last", n)
	}
	last := evs(lg, proto.OpExplode)
	if e := last[len(last)-1]; math.Abs(e.X-250) > 1 || e.R < 79 {
		t.Errorf("last blast at %.0f r %.0f, want the big one where it landed", e.X, e.R)
	}
}

func TestMIRVSplits(t *testing.T) {
	for _, c := range []struct{ w, n int }{{proto.IMIRV, 5}, {proto.IMegaMIRV, 9}} {
		s := flat()
		lg := s.Resolve([]Shot{{Seat: 0, Angle: 60, Power: 300, Weapon: c.w, Guide: proto.INone, Target: proto.NoSeat}}, false)
		bombs := 0
		for _, e := range evs(lg, proto.OpArc) {
			if e.Trk.Style == proto.StBomblet {
				bombs++
			}
		}
		if bombs != c.n {
			t.Errorf("%s: %d warheads, want %d", items.All[c.w].Name, bombs, c.n)
		}
	}
}

func TestNapalmBurnsThroughShields(t *testing.T) {
	s := flat()
	s.Tanks[1].Shield, s.Tanks[1].ShieldHP = proto.IShield, 100
	before := solid(s.T)
	lg := s.drop(proto.INapalm, 360) // outside the shield's ring
	fl := evs(lg, proto.OpFlow)
	if len(fl) != 1 || fl[0].Kind != proto.FlowFire || len(fl[0].Cells) != 15*flowCells {
		t.Fatalf("flows %v", len(fl))
	}
	if solid(s.T) != before {
		t.Error("napalm changed the dirt")
	}
	if s.Tanks[1].Energy >= 100 || s.Tanks[1].ShieldHP != 100 {
		t.Errorf("energy %.0f shield %.0f: the fire should go through the shield", s.Tanks[1].Energy, s.Tanks[1].ShieldHP)
	}
}

func TestMudslideFillsAPit(t *testing.T) {
	s := flat()
	for x := 300; x < 340; x++ { // a pit 40 x 30: 1200 cells, the mudslide's volume
		for y := ground; y < ground+30; y++ {
			s.T.Set(x, y, false)
		}
	}
	s.drop(proto.IMudslide, 320)
	for x := 300; x < 340; x++ {
		for y := ground; y < ground+30; y++ {
			if !s.T.At(x, y) {
				t.Fatalf("(%d,%d) still air: the pit isn't full", x, y)
			}
		}
	}
	if s.T.Surface(250) != ground || s.T.Surface(390) != ground {
		t.Error("mud spilled outside the pit")
	}
}

func TestTracerLeavesNothing(t *testing.T) {
	for _, w := range []int{proto.ITracer, proto.ISmokeTracer} {
		s := flat()
		before := solid(s.T)
		lg := s.drop(w, 400)
		if solid(s.T) != before || len(evs(lg, proto.OpExplode)) > 0 || s.Tanks[1].Energy != 100 {
			t.Errorf("%s did something", items.All[w].Name)
		}
	}
	s := flat()
	lg := s.Resolve([]Shot{{Seat: 0, Angle: 45, Power: 300, Weapon: proto.ISmokeTracer, Guide: proto.INone}}, false)
	if evs(lg, proto.OpArc)[0].Trk.Style != proto.StSmoke {
		t.Error("smoke tracer's track isn't smoke")
	}
}

// ramp: the ground falls from left to right, 1 in 4.
func ramp() *Sim {
	s := flat()
	for x := 0; x < world.W; x++ {
		top := 200 + x/4
		for y := 0; y < world.H; y++ {
			s.T.Set(x, y, y >= top)
		}
	}
	s.Tanks[0].Y = float64(s.T.Surface(100))
	s.Tanks[1].X, s.Tanks[1].Y = 560, float64(s.T.Surface(560))
	return s
}

func TestRollerRollsDownhill(t *testing.T) {
	s := ramp()
	lg := s.drop(proto.IRoller, 300)
	rolled := false
	for _, e := range evs(lg, proto.OpArc) {
		if e.Trk.Style == proto.StRoller {
			rolled = true
		}
	}
	ex := evs(lg, proto.OpExplode)
	if !rolled || len(ex) == 0 {
		t.Fatalf("rolled %v, %d blasts", rolled, len(ex))
	}
	if x := ex[0].X; math.Abs(x-560) > TankW/2+3 {
		t.Errorf("blew up at %.0f, want at the tank downhill at 560", x)
	}
	if !damaged(lg, 1) {
		t.Error("the tank it rolled into wasn't hurt")
	}
}

func TestRollerFuseBlowsOnLanding(t *testing.T) {
	s := ramp()
	p := &shell{}
	_ = p
	s.drop(proto.IRoller, 0) // warm the rng the same way: not needed
	s = ramp()
	lgFused := func() *Log {
		s.log = &Log{}
		q := &shell{x: 300, y: 200, vy: 300, seat: 0, w: proto.IRoller, it: &items.All[proto.IRoller], guide: proto.INone, fuse: true}
		q.trk = s.newTrack(proto.StShell, 0, q.x, q.y)
		s.shells = []*shell{q}
		s.run()
		return s.log
	}()
	ex := evs(lgFused, proto.OpExplode)
	if len(ex) == 0 || math.Abs(ex[0].X-300) > 1 {
		t.Errorf("fused roller: blasts %v", ex)
	}
}

func TestShovelClearsNoHarm(t *testing.T) {
	s := flat()
	s.Tanks[1].X = 150 // in the fan
	before := solid(s.T)
	lg := s.Resolve([]Shot{{Seat: 0, Angle: 0, Power: 300, Weapon: proto.IShovelBlast, Guide: proto.INone}}, false)
	if solid(s.T) >= before {
		t.Error("no dirt cleared")
	}
	for to := range lg.Damage[0] {
		if lg.Damage[0][to] > 0 {
			t.Error("a shovel hurt a tank")
		}
	}
	for _, e := range evs(lg, proto.OpExplode) {
		if e.Kind != proto.ExClear {
			t.Errorf("blast kind %d, want clear", e.Kind)
		}
	}
}

func TestBusterClearsNoHarm(t *testing.T) {
	s := flat()
	before := solid(s.T)
	lg := s.drop(proto.IHeavyBuster, 380)
	if solid(s.T) >= before || lg.Damage[0][1] > 0 {
		t.Errorf("cleared %d, damage %.0f", before-solid(s.T), lg.Damage[0][1])
	}
}

func TestDrillBoresDown(t *testing.T) {
	s := flat()
	lg := s.drop(proto.IDrill, 250)
	air := 0
	for y := ground + 20; y < ground+100; y++ {
		for x := 200; x < 300; x++ {
			if !s.T.At(x, y) {
				air++
				break
			}
		}
	}
	if air < 60 {
		t.Errorf("tunnel reaches %d rows down, want 60+", air)
	}
	if lg.Damage[0][1] > 0 || lg.Damage[0][0] > 0 {
		t.Error("a drill hurt a tank")
	}
}

func TestMoleGoesUnderTheEnemy(t *testing.T) {
	s := flat()
	s.Tanks[1].X = 300
	lg := s.drop(proto.IHeavyMole, 220)
	ex := evs(lg, proto.OpExplode)
	if len(ex) == 0 {
		t.Fatal("no blast")
	}
	if e := ex[0]; math.Abs(e.X-300) > 10 || e.Y < ground {
		t.Errorf("blast at (%.0f,%.0f), want under the tank at 300", e.X, e.Y)
	}
	if !damaged(lg, 1) {
		t.Error("the enemy wasn't hurt")
	}
}

func TestDirtBuriesTank(t *testing.T) {
	s := flat()
	before := solid(s.T)
	s.Tanks[1].X = 300
	s.drop(proto.IDirtHeap, 330)
	added := solid(s.T) - before
	want := math.Pi * 70 * 70 / 2 // the half disc above the ground
	if math.Abs(float64(added)-want) > want*0.1 {
		t.Errorf("%d cells of dirt, want about %.0f", added, want)
	}
	cx, cy := s.Tanks[1].Centre()
	if !s.T.At(int(cx), int(cy)) {
		t.Error("the tank isn't buried")
	}
}

func TestGeyserRaisesACone(t *testing.T) {
	s := flat()
	s.drop(proto.IDirtGeyser, 250)
	if top := s.T.Surface(250); top > ground-70 {
		t.Errorf("cone top at %d, want 70+ above the ground", top)
	}
	if s.T.Surface(250-15) < s.T.Surface(250) {
		t.Error("the cone isn't highest in the middle")
	}
}

func TestQuakeDropsFloatingDirt(t *testing.T) {
	s := flat()
	s.S.Suspend = 100
	for x := 300; x < 340; x++ {
		for y := 100; y < 110; y++ {
			s.T.Set(x, y, true)
		}
	}
	lg := s.drop(proto.IQuake, 320)
	if s.T.Floating(world.Span{X0: 0, X1: world.W}) {
		t.Error("dirt still floating")
	}
	if len(evs(lg, proto.OpShake)) != 1 {
		t.Error("no shake")
	}
}

func TestPlasmaNoCrater(t *testing.T) {
	s := flat()
	before := solid(s.T)
	s.Tanks[1].X = 460
	lg := s.drop(proto.IPlasmaBurst, 370)
	if solid(s.T) != before {
		t.Error("plasma cut a crater")
	}
	if !damaged(lg, 1) {
		t.Error("plasma didn't hurt the tank 90 away")
	}
}

func TestLaserBurnsAndHits(t *testing.T) {
	s := flat()
	s.Tanks[1].X = 220
	for x := 150; x < 170; x++ { // a wall in the way
		for y := 250; y < ground; y++ {
			s.T.Set(x, y, true)
		}
	}
	_, my := s.Tanks[0].Muzzle(0)
	lg := s.Resolve([]Shot{{Seat: 0, Angle: 0, Power: 500, Weapon: proto.ILaser, Guide: proto.INone}}, false)
	if s.T.At(160, int(my)) {
		t.Error("the beam didn't burn through the wall")
	}
	if s.Tanks[1].Energy != 100-items.All[proto.ILaser].Mult {
		t.Errorf("energy %.0f", s.Tanks[1].Energy)
	}
	if len(evs(lg, proto.OpBeam)) != 1 {
		t.Error("no beam")
	}
	// a shield takes it
	s = flat()
	s.Tanks[1].X = 220
	s.Tanks[1].Shield, s.Tanks[1].ShieldHP = proto.IShield, 100
	s.Resolve([]Shot{{Seat: 0, Angle: 0, Power: 500, Weapon: proto.ILaser, Guide: proto.INone}}, false)
	if s.Tanks[1].Energy != 100 || s.Tanks[1].ShieldHP != 60 {
		t.Errorf("shielded: energy %.0f shield %.0f", s.Tanks[1].Energy, s.Tanks[1].ShieldHP)
	}
}

func TestAutopilotFindsTarget(t *testing.T) {
	s := flat()
	lg := s.Resolve([]Shot{{Seat: 0, Angle: 45, Power: 300, Weapon: proto.IMissile, Guide: proto.IAutopilot, Target: 1}}, false)
	if !damaged(lg, 1) {
		t.Error("the autopilot missed")
	}
}

func TestShields(t *testing.T) {
	// a shield stops a shell at its ring: a puff, 10 HP, no harm
	s := flat()
	s.Tanks[1].Shield, s.Tanks[1].ShieldHP = proto.IShield, 100
	s.dropV(proto.IMissile, 400, ground-80, 0, 300)
	if s.Tanks[1].Energy != 100 || s.Tanks[1].ShieldHP != 90 {
		t.Errorf("shield: energy %.0f shield HP %.0f", s.Tanks[1].Energy, s.Tanks[1].ShieldHP)
	}
	// a bounce shield turns it away
	s = flat()
	s.Tanks[1].Shield, s.Tanks[1].ShieldHP = proto.IBounceShield, 100
	lg := s.dropV(proto.IMissile, 400, ground-80, 0, 300)
	deflected := false
	for _, e := range evs(lg, proto.OpShield) {
		if e.Kind == proto.ShDeflect {
			deflected = true
		}
	}
	if !deflected || s.Tanks[1].Energy != 100 {
		t.Errorf("bounce shield: deflected %v energy %.0f", deflected, s.Tanks[1].Energy)
	}
	// a deflector lifts a shell passing over
	s = flat()
	s.Tanks[1].Shield, s.Tanks[1].ShieldHP = proto.ISuperDeflector, 200
	lg = s.dropV(proto.IMissile, 400, ground-150, 0, 100)
	lifted := false
	for _, e := range evs(lg, proto.OpShield) {
		if e.Kind == proto.ShLift {
			lifted = true
		}
	}
	if !lifted {
		t.Error("the deflector didn't lift the shell")
	}
}

func TestParachute(t *testing.T) {
	for _, chute := range []bool{false, true} {
		s := flat()
		s.Tanks[1].Chute, s.Tanks[1].Chutes = chute, 1
		for x := 380; x < 420; x++ { // the ground under it goes
			for y := ground; y < ground+60; y++ {
				s.T.Set(x, y, false)
			}
		}
		s.drop(proto.ITracer, 600) // anything: the aftermath drops it
		if s.Tanks[1].Y != ground+60 {
			t.Fatalf("chute %v: tank at %.0f, want %d", chute, s.Tanks[1].Y, ground+60)
		}
		hurt := s.Tanks[1].Energy < 100
		if hurt == chute {
			t.Errorf("chute %v: energy %.0f", chute, s.Tanks[1].Energy)
		}
		if chute && s.Tanks[1].Chutes != 0 {
			t.Error("the chute wasn't used up")
		}
	}
}

func TestWalls(t *testing.T) {
	shot := func(wall int) *Log {
		s := flat()
		s.S.Wall = wall
		s.Tanks[0].X = 600
		return s.Resolve([]Shot{{Seat: 0, Angle: 30, Power: 400, Weapon: proto.IMissile, Guide: proto.INone}}, false)
	}
	jumped := false
	for _, p := range evs(shot(proto.WallWrap), proto.OpArc)[0].Trk.Pts {
		jumped = jumped || p.Jump
	}
	if !jumped {
		t.Error("wrap: the shell never came round")
	}
	if ex := evs(shot(proto.WallConcrete), proto.OpExplode); len(ex) == 0 || ex[0].X < world.W-2 {
		t.Error("concrete: the shell didn't go off at the wall")
	}
	if ex := evs(shot(proto.WallRubber), proto.OpExplode); len(ex) == 0 || ex[0].X >= world.W {
		t.Error("rubber: the shell didn't come back")
	}
	if ex := evs(shot(proto.WallNone), proto.OpExplode); len(ex) != 0 {
		t.Error("open: the shell should have gone")
	}
}

// Each seeker brings a near miss closer to its target than the same shot
// unguided.
func TestSeekersSteer(t *testing.T) {
	s := flat()
	plain := Shot{Seat: 0, Angle: 45, Weapon: proto.IMissile, Guide: proto.INone, Target: 1}
	// a power that overshoots tank 1 by 40-120 units
	for plain.Power = 200; plain.Power < 900; plain.Power += 5 {
		if h := s.Trace(plain); h.X > 440 && h.X < 520 {
			break
		}
	}
	miss := math.Abs(s.Trace(plain).X - 400)
	for _, g := range []int{proto.IHeatSeeker, proto.ILevelSeeker, proto.IDropSeeker, proto.IAutopilot} {
		sh := plain
		sh.Guide = g
		h := s.Trace(sh)
		if d := math.Abs(h.X - 400); d >= miss && h.Seat != 1 {
			t.Errorf("%s: lands %.0f from the target, unguided %.0f", items.All[g].Name, d, miss)
		}
	}
}

// Tunnelling: a shell bores into the dirt; an Impact Fuse stops it at the
// surface.
func TestTunnelAndFuse(t *testing.T) {
	for _, fuse := range []bool{false, true} {
		s := flat()
		s.S.Tunnel = true
		s.log = &Log{}
		q := &shell{x: 250, y: ground - 40, vy: 600, seat: 0, w: proto.IMissile, it: &items.All[proto.IMissile], guide: proto.INone, fuse: fuse}
		q.trk = s.newTrack(proto.StShell, 0, q.x, q.y)
		s.shells = []*shell{q}
		s.run()
		y := evs(s.log, proto.OpExplode)[0].Y
		if fuse && y > ground+1 || !fuse && y < ground+3 {
			t.Errorf("fuse %v: went off at %.0f (ground %d)", fuse, y, ground)
		}
	}
}

// Falls Hurt off: a fall costs nothing.
func TestFallsHurtOff(t *testing.T) {
	s := flat()
	s.S.ImpactDamage = false
	for x := 380; x < 420; x++ {
		for y := ground; y < ground+60; y++ {
			s.T.Set(x, y, false)
		}
	}
	s.drop(proto.ITracer, 600)
	if s.Tanks[1].Y != ground+60 || s.Tanks[1].Energy != 100 {
		t.Errorf("tank at %.0f, energy %.0f", s.Tanks[1].Y, s.Tanks[1].Energy)
	}
}
