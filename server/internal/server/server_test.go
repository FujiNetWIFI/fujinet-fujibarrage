package server

import (
	"math/rand/v2"
	"net"
	"sync"
	"testing"
	"time"

	"fujibarrage/internal/client"
	"fujibarrage/internal/game"
	"fujibarrage/internal/project"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/view"
)

type clock struct {
	mu sync.Mutex
	t  time.Time
}

func (c *clock) now() time.Time { c.mu.Lock(); defer c.mu.Unlock(); return c.t }
func (c *clock) add(d time.Duration) {
	c.mu.Lock()
	c.t = c.t.Add(d)
	c.mu.Unlock()
}

func serve(t *testing.T) (*Server, *clock, string) {
	t.Helper()
	PushEvery = time.Millisecond
	ck := &clock{t: time.Unix(1e9, 0)}
	s := New(1, nil)
	s.Now = ck.now
	for _, r := range s.rooms {
		r.t0 = ck.now()
	}
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { ln.Close() })
	go s.Serve(ln)
	return s, ck, ln.Addr().String()
}

func waitFor(t *testing.T, c *client.Client, what string, ok func() bool) {
	t.Helper()
	for i := 0; i < 400; i++ {
		if _, err := c.Poll(5 * time.Millisecond); err != nil {
			t.Fatal(err)
		}
		if ok() {
			return
		}
	}
	t.Fatalf("no %s", what)
}

func TestPatches(t *testing.T) {
	r := rand.New(rand.NewPCG(1, 2))
	for k := 0; k < 500; k++ {
		prev := make([]byte, proto.StateSize)
		for i := range prev {
			prev[i] = byte(r.IntN(4))
		}
		cur := append([]byte(nil), prev...)
		for n := r.IntN(80); n > 0; n-- {
			cur[r.IntN(len(cur))] = byte(r.IntN(256))
		}
		if k%50 == 0 {
			prev = nil
		}
		got := make([]byte, proto.StateSize)
		if prev != nil {
			copy(got, prev)
		}
		for _, p := range proto.Patches(prev, cur) {
			if len(p) > proto.MaxPayload {
				t.Fatalf("payload %d bytes", len(p))
			}
			if !proto.ApplyPatch(got, p) {
				t.Fatal("bad patch")
			}
		}
		if string(got) != string(cur) {
			t.Fatalf("patch %d: wrong result", k)
		}
	}
}

// fieldOK: the client's field is the server's world, downsampled.
func fieldOK(t *testing.T, r *Room, c *client.Client) {
	t.Helper()
	r.mu.Lock()
	want := c.Caps.Entries(r.g.Sim.T)
	tv := r.g.TV
	r.mu.Unlock()
	if c.V.TV != int(tv) {
		t.Fatalf("client terrain v%d, server v%d", c.V.TV, tv)
	}
	bad := 0
	for x := 0; x < c.Caps.FieldW; x++ {
		col, _, _ := project.DecodeEntry(want[x], c.Caps.FieldH)
		for y := range col {
			if col[y] != (c.V.At(x, y) == view.PxDirt) {
				bad++
			}
		}
	}
	if bad > 0 {
		t.Fatalf("%d pixels of the client's field differ from the server's", bad)
	}
}

// A person (the Atari's shape) hosts a 10-tank game against bots in every
// play mode and plays it to the end: every script plays, the hold is
// granted and released, and after each shot the field matches the world.
func TestGameAgainstBots(t *testing.T) {
	for mode := 0; mode < 3; mode++ {
		botGame(t, client.Atari, mode)
	}
}

// The Intellivision's shape: no CAP_HOLD. Its segments come without HOLD,
// each after the last one's D, and its field matches after every shot.
func TestIntvGameAgainstBots(t *testing.T) {
	for mode := 0; mode < 2; mode++ {
		botGame(t, client.Intv, mode)
	}
}

// The Astrocade's shape: CAP_HOLD and a 240-byte buffer, so big shots come
// in several segments, each held and each after the last one's D.
func TestAstrocadeGameAgainstBots(t *testing.T) {
	for mode := 0; mode < 3; mode++ {
		botGame(t, client.Astrocade, mode)
	}
}

// The Arcadia's shape: 48x48 blocks, CAP_HOLD and a 160-byte buffer.
func TestArcadiaGameAgainstBots(t *testing.T) {
	for mode := 0; mode < 3; mode++ {
		botGame(t, client.Arcadia, mode)
	}
}

// The H89's shape: 160x44 blocks, CAP_HOLD and a 2048-byte buffer (1024 under HDOS).
func TestH89GameAgainstBots(t *testing.T) {
	for mode := 0; mode < 3; mode++ {
		botGame(t, client.H89, mode)
		botGame(t, client.H89Hdos, mode)
	}
}

// The Apple II's shape: CAP_HOLD, 140x160, a 1536-byte buffer.
func TestApple2GameAgainstBots(t *testing.T) {
	for mode := 0; mode < 3; mode++ {
		botGame(t, client.Apple2, mode)
	}
}

// The CoCo 1/2's shape (128x168) and the CoCo 3's (320x176: a field wider
// than 256, its tank x past a byte).
func TestCocoGameAgainstBots(t *testing.T) {
	for mode := 0; mode < 3; mode++ {
		botGame(t, client.Coco, mode)
		botGame(t, client.Coco3, mode)
	}
}

// The Adam's shape (256x168: the TMS9918's full width).
func TestAdamGameAgainstBots(t *testing.T) {
	for mode := 0; mode < 3; mode++ {
		botGame(t, client.Adam, mode)
	}
}

// The Master System's shape (the Adam's, 8 sprites a line), at 60 and 50 Hz.
func TestSmsGameAgainstBots(t *testing.T) {
	for mode := 0; mode < 3; mode++ {
		botGame(t, client.Sms, mode)
		botGame(t, client.SmsPal, mode)
	}
}

// The NES's shape (the Adam's, a 1K script buffer), at 60 and 50 Hz.
func TestNesGameAgainstBots(t *testing.T) {
	for mode := 0; mode < 3; mode++ {
		botGame(t, client.Nes, mode)
		botGame(t, client.NesPal, mode)
	}
}

// The Channel F's shape: 102x46, CAP_HOLD, a 1.5K buffer.
func TestChannelFGameAgainstBots(t *testing.T) {
	for mode := 0; mode < 3; mode++ {
		botGame(t, client.ChannelF, mode)
	}
}

func TestAtari5200GameAgainstBots(t *testing.T) {
	for mode := 0; mode < 3; mode++ {
		botGame(t, client.Atari5200, mode)
	}
}

// The PC's shapes (CGA and VGA: 320x176, a 4096-byte buffer).
func TestMsdosGameAgainstBots(t *testing.T) {
	for mode := 0; mode < 3; mode++ {
		botGame(t, client.Msdos, mode)
		botGame(t, client.MsdosVga, mode)
	}
}

func botGame(t *testing.T, caps project.Caps, mode int) {
	t.Helper()
	s, ck, addr := serve(t)
	r := s.Room("dev1")
	c, err := client.Dial(addr)
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()
	if err := c.Hello("dev1", "ANN", caps, 0); err != nil {
		t.Fatal(err)
	}
	waitFor(t, c, "a seat", func() bool { return c.Commits > 0 && c.Seat() == 0 })
	if len(c.Catalog) != len(s.cat) {
		t.Fatalf("catalog %d of %d bytes", len(c.Catalog), len(s.cat))
	}
	c.Act(proto.ActSet, proto.SetRounds, 1) // 2 rounds
	c.Act(proto.ActSet, proto.SetMode, byte(mode))
	c.Act(proto.ActSet, proto.SetWalls, byte(mode*2))
	c.Act(proto.ActStart)
	waitFor(t, c, "the shop", func() bool { return c.State[proto.OffPhase] == proto.PhShop })
	if c.State[proto.OffTanks] != 10 {
		t.Fatalf("%d tanks", c.State[proto.OffTanks])
	}
	c.Act(proto.ActBuy, proto.INuke, 1)
	c.Act(proto.ActDone)
	rng := rand.New(rand.NewPCG(uint64(mode), 3))
	shots, played := 0, 0
	lastSID := byte(0)
	for step := 0; step < 200000; step++ {
		ck.add(time.Second / 60)
		s.Update()
		if _, err := c.Poll(time.Millisecond); err != nil {
			t.Fatal(err)
		}
		if c.TerrErr != nil {
			t.Fatal(c.TerrErr)
		}
		ph := c.State[proto.OffPhase]
		if ph == proto.PhGameOver {
			break
		}
		if ph == proto.PhShop || ph == proto.PhDefense {
			if c.P(0)[proto.PFlags]&proto.PFDone == 0 && step%30 == 0 {
				c.Act(proto.ActDone)
			}
		}
		if ph == proto.PhAim && c.P(0)[proto.PFlags]&proto.PFAlive != 0 && step%20 == 0 {
			mine := c.State[proto.OffTurn] == 0 ||
				(c.State[proto.OffTurn] == proto.NoSeat && c.P(0)[proto.PFlags]&proto.PFDone == 0)
			if mine {
				p := 200 + rng.IntN(500)
				w := byte(proto.IMiniMissile)
				if c.State[proto.OffInv+proto.INuke] > 0 {
					w = proto.INuke
				}
				c.Act(proto.ActFire, byte(20+rng.IntN(140)), byte(p), byte(p>>8), w, proto.INone, proto.NoSeat)
				shots++
			}
		}
		if len(c.Ready) > 0 {
			if len(c.Ready) > 1 {
				t.Fatal("a segment came before the last one's D")
			}
			if c.Ready[0].Hold != caps.Hold() {
				t.Fatalf("a segment's HOLD is %v for a client whose CAP_HOLD is %v", c.Ready[0].Hold, caps.Hold())
			}
			for _, sg := range c.Ready {
				if len(sg.B) > caps.Buf {
					t.Fatalf("a %d-byte segment for a %d-byte buffer", len(sg.B), caps.Buf)
				}
			}
			sid := c.Ready[0].SID
			if _, err := c.PlayAll(); err != nil {
				t.Fatal(err)
			}
			played++
			if sid != lastSID {
				lastSID = sid
				// the version bump comes after D
				waitFor(t, c, "the terrain version after the shot", func() bool {
					if len(c.Ready) > 0 { // the volley's later segments
						if _, err := c.PlayAll(); err != nil {
							t.Fatal(err)
						}
					}
					r.mu.Lock()
					defer r.mu.Unlock()
					return r.g.Shot == nil || c.V.TV == int(r.g.Shot.PostTV) || r.g.TV != r.g.Shot.PostTV
				})
				r.mu.Lock()
				same := r.g.Shot != nil && r.g.TV == r.g.Shot.PostTV
				r.mu.Unlock()
				if same {
					fieldOK(t, r, c)
				}
			}
		}
	}
	if c.State[proto.OffPhase] != proto.PhGameOver {
		t.Fatalf("%s mode %d: no game over (phase %d, round %d)", caps.Key(), mode, c.State[proto.OffPhase], c.State[proto.OffRound])
	}
	t.Logf("%s mode %d: %d of my shots, %d scripts played, %d bytes in, winner seat %d",
		caps.Key(), mode, shots, played, c.BytesIn, c.State[proto.OffResult])
}

// A client that never plays its scripts is left behind when the game stops
// waiting; asking again (R, terrain unknown) brings its field back exact.
func TestSlowClientSkipped(t *testing.T) {
	s, ck, addr := serve(t)
	r := s.Room("dev1")
	c, err := client.Dial(addr)
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()
	if err := c.Hello("dev1", "ANN", client.Intv, 0); err != nil {
		t.Fatal(err)
	}
	waitFor(t, c, "a seat", func() bool { return c.Commits > 0 && c.Seat() == 0 })
	c.Act(proto.ActStart)
	skipped := 0
	for step := 0; step < 60*60*20 && skipped < 2; step++ {
		ck.add(time.Second / 60)
		s.Update()
		if _, err := c.Poll(time.Millisecond); err != nil {
			t.Fatal(err)
		}
		ph := c.State[proto.OffPhase]
		if (ph == proto.PhShop || ph == proto.PhDefense) && step%30 == 0 {
			c.Act(proto.ActDone)
		}
		if ph == proto.PhAim && c.State[proto.OffTurn] == 0 && step%20 == 0 {
			c.Act(proto.ActFire, 60, 0x90, 1, proto.IMissile, proto.INone, proto.NoSeat)
		}
		// never play: the shot's segments pile up unplayed until the game
		// moves on without us
		if len(c.Ready) > 0 && ph != proto.PhShot {
			c.Ready = nil
			c.V.TV = -1
			if err := c.Resync(); err != nil {
				t.Fatal(err)
			}
			waitFor(t, c, "the field again", func() bool {
				r.mu.Lock()
				defer r.mu.Unlock()
				return c.V.TV == int(r.g.TV)
			})
			fieldOK(t, r, c)
			skipped++
		}
	}
	if skipped < 2 {
		t.Fatalf("skipped %d times", skipped)
	}
}

// An Atari and an Intellivision in one room: each gets its own shape (one
// held, one not), and both fields match after the game.
func TestMixedRoom(t *testing.T) {
	s, ck, addr := serve(t)
	r := s.Room("dev1")
	var cs []*client.Client
	for i, caps := range []project.Caps{client.Atari, client.Intv} {
		c, err := client.Dial(addr)
		if err != nil {
			t.Fatal(err)
		}
		defer c.Close()
		if err := c.Hello("dev1", []string{"ANN", "BOB"}[i], caps, 0); err != nil {
			t.Fatal(err)
		}
		waitFor(t, c, "a seat", func() bool { return c.Commits > 0 && c.Seat() == i })
		cs = append(cs, c)
	}
	cs[0].Act(proto.ActSet, proto.SetRounds, 0)
	cs[0].Act(proto.ActSet, proto.SetTanks, 2) // 4 tanks
	cs[0].Act(proto.ActStart)
	rng := rand.New(rand.NewPCG(9, 9))
	scripts := [2]int{}
	for step := 0; step < 200000; step++ {
		ck.add(time.Second / 60)
		s.Update()
		over := true
		for i, c := range cs {
			if _, err := c.Poll(time.Millisecond); err != nil {
				t.Fatal(err)
			}
			ph := c.State[proto.OffPhase]
			if ph != proto.PhGameOver {
				over = false
			}
			if (ph == proto.PhShop || ph == proto.PhDefense) && step%30 == 0 && c.P(i)[proto.PFlags]&proto.PFDone == 0 {
				c.Act(proto.ActDone)
			}
			if ph == proto.PhAim && c.State[proto.OffTurn] == byte(i) && step%20 == 0 {
				p := 200 + rng.IntN(500)
				c.Act(proto.ActFire, byte(20+rng.IntN(140)), byte(p), byte(p>>8), proto.IMiniMissile, proto.INone, proto.NoSeat)
			}
			if len(c.Ready) > 0 {
				if c.Ready[0].Hold != c.Caps.Hold() {
					t.Fatalf("seat %d: HOLD %v", i, c.Ready[0].Hold)
				}
				if _, err := c.PlayAll(); err != nil {
					t.Fatal(err)
				}
				scripts[i]++
			}
		}
		if over {
			break
		}
	}
	for i, c := range cs {
		if c.State[proto.OffPhase] != proto.PhGameOver {
			t.Fatalf("seat %d: no game over", i)
		}
		waitFor(t, c, "the last terrain", func() bool {
			r.mu.Lock()
			defer r.mu.Unlock()
			return c.V.TV == int(r.g.TV)
		})
		fieldOK(t, r, c)
	}
	if scripts[0] == 0 || scripts[1] == 0 {
		t.Fatalf("scripts played: %v", scripts)
	}
	t.Logf("scripts played: Atari %d, Intellivision %d", scripts[0], scripts[1])
}

func TestCatalogDecodes(t *testing.T) {
	b := game.Catalog()
	if b[1] != proto.NItems || b[2] != proto.NSettings {
		t.Fatal("catalog header")
	}
}

// A HELLO for a room that isn't there is answered ERR_NO_ROOM (it was
// queued, then dropped with the rest of the queue: clients timed out).
func TestHelloNoRoom(t *testing.T) {
	_, _, addr := serve(t)
	c, err := client.Dial(addr)
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()
	if err := c.Hello("nowhere", "ANN", client.Arcadia, 0); err != nil {
		t.Fatal(err)
	}
	waitFor(t, c, "ERR_NO_ROOM", func() bool { return c.Err == proto.ErrNoRoom })
}
