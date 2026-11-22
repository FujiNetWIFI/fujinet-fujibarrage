package server

import (
	"bufio"
	"bytes"
	"fmt"
	"log"
	"net"
	"os"
	"strings"
	"sync"
	"time"

	"fujibarrage/internal/game"
	"fujibarrage/internal/project"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/world"
)

// Stream timing (package vars so tests can change them).
var (
	PushEvery = 50 * time.Millisecond // how often to look for changes
	Idle      = 35 * time.Second      // no frame from the client: drop it
	HoldMax   = 90 * time.Second      // longest a client may hold its link
)

var debug = os.Getenv("BARRAGE_STREAM_DEBUG") != ""

// Serve accepts stream clients on ln until it fails.
func (s *Server) Serve(ln net.Listener) error {
	for {
		nc, err := ln.Accept()
		if err != nil {
			return err
		}
		go s.handle(nc)
	}
}

// Listen serves on addr (":9913") in the background.
func (s *Server) Listen(addr string) (net.Addr, error) {
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		return nil, err
	}
	log.Printf("stream on %s", ln.Addr())
	go func() { log.Println("stream:", s.Serve(ln)) }()
	return ln.Addr(), nil
}

// conn is one client. Lock order: the room's mutex, then the conn's.
type conn struct {
	s  *Server
	nc net.Conn

	mu      sync.Mutex
	room    *Room
	name    string
	watch   bool
	caps    project.Caps
	seat    int
	played  byte   // the last shot this client finished watching
	last    []byte // the STATE the client has once q is out
	q       [][]byte
	sent    uint8 // encoded bytes sent, mod 256
	taken   uint8 // bytes the client took from its ring, mod 256
	lastSeq byte
	acked   bool
	ack     bool
	ackSeq  byte
	catSent int      // CATALOG bytes queued
	tv      int      // terrain version the client has (-1: none)
	have    [][]byte // its columns
	seg     int      // the script segment being sent or played
	segSent bool
	hold    bool // the link is held: nothing more until D
	until   time.Time
	total   int
	t0      time.Time
	kick    chan struct{}
	done    chan struct{}
}

func (s *Server) handle(nc net.Conn) {
	if tc, ok := nc.(*net.TCPConn); ok {
		tc.SetNoDelay(true)
	}
	c := &conn{s: s, nc: nc, seat: -1, tv: -1, kick: make(chan struct{}, 1), done: make(chan struct{}), t0: time.Now()}
	defer nc.Close()
	go c.writer()
	c.reader()
	close(c.done)
	c.mu.Lock()
	r := c.room
	c.mu.Unlock()
	if r != nil {
		r.part(c)
	}
	if secs := time.Since(c.t0).Seconds(); secs > 0 {
		log.Printf("stream %s: closed, %d bytes out (%.0f B/s)", nc.RemoteAddr(), c.total, float64(c.total)/secs)
	}
}

func (c *conn) poke() {
	select {
	case c.kick <- struct{}{}:
	default:
	}
}

func (c *conn) reader() {
	br := bufio.NewReader(c.nc)
	var buf []byte
	for {
		wait := Idle
		c.mu.Lock()
		if c.hold {
			if d := time.Until(c.until) + 10*time.Second; d > wait {
				wait = d
			}
		}
		c.mu.Unlock()
		c.nc.SetReadDeadline(time.Now().Add(wait))
		b, err := br.ReadByte()
		if err != nil {
			return
		}
		if b != 0 {
			if len(buf) < 2*proto.MaxFrame {
				buf = append(buf, b)
			}
			continue
		}
		raw, ok := proto.COBSDecode(buf)
		if debug {
			log.Printf("stream %s: in % x", c.nc.RemoteAddr(), buf)
		}
		buf = buf[:0]
		if !ok {
			continue // noise, or a lost byte's frame
		}
		typ, p, ok := proto.Unframe(raw)
		if !ok {
			continue
		}
		if !c.frame(typ, p) {
			return
		}
	}
}

func cstr(b []byte) string {
	if i := bytes.IndexByte(b, 0); i >= 0 {
		b = b[:i]
	}
	return strings.ToUpper(strings.TrimSpace(string(b)))
}

func (c *conn) queue(f []byte) { c.q = append(c.q, f) }

// frame handles one frame; false hangs up.
func (c *conn) frame(typ byte, p []byte) bool {
	s := c.s
	switch typ {
	case proto.SCHello:
		if len(p) < proto.HelloLen {
			return false
		}
		c.mu.Lock()
		old := c.room
		c.mu.Unlock()
		if old != nil {
			old.part(c)
		}
		if p[0] != proto.Version {
			c.mu.Lock()
			c.queue(proto.Frame(proto.CSError, []byte{proto.ErrVersion}))
			c.mu.Unlock()
			break
		}
		caps, err := project.ParseHello(p)
		r := s.Room(cstr(p[proto.HRoom : proto.HRoom+proto.RoomLen]))
		c.mu.Lock()
		switch {
		case err != nil:
			log.Printf("stream %s: %v", c.nc.RemoteAddr(), err)
			c.queue(proto.Frame(proto.CSError, []byte{proto.ErrCaps}))
			r = nil
		case r == nil:
			c.queue(proto.Frame(proto.CSError, []byte{proto.ErrNoRoom}))
		}
		c.room, c.caps = r, caps
		c.name = cstr(p[proto.HName : proto.HName+proto.NameLen])
		c.watch = p[proto.HFlags]&proto.HelloWatch != 0 || c.name == ""
		c.last, c.q, c.acked, c.ack = nil, nil, false, false
		c.catSent, c.tv, c.have, c.seg, c.segSent, c.hold = 0, -1, nil, 0, false, false
		c.mu.Unlock()
		if r != nil {
			r.join(c)
		}
	case proto.SCRooms:
		list := s.Rooms(len(p) > 0 && p[0] == 1)
		c.mu.Lock()
		if len(list) == 0 {
			c.queue(proto.Frame(proto.CSRoom, []byte{0, 0}))
		}
		for i, ri := range list {
			c.queue(proto.Frame(proto.CSRoom, append([]byte{byte(i), byte(len(list))}, record(ri)...)))
		}
		c.mu.Unlock()
	case proto.SCAct:
		if len(p) < 2 {
			return false
		}
		r := c.roomOf()
		if r == nil {
			return true
		}
		r.mu.Lock()
		c.mu.Lock()
		again := c.acked && p[0] == c.lastSeq
		c.lastSeq, c.acked, c.ack, c.ackSeq = p[0], true, true, p[0]
		seat := c.seat
		c.mu.Unlock()
		if !again {
			r.syncLocked()
			r.g.Act(seat, int(p[1]), p[2:])
			if p[1] == proto.ActLeave {
				c.mu.Lock()
				c.seat, c.watch = -1, true
				c.mu.Unlock()
			}
		}
		r.mu.Unlock()
	case proto.SCDone:
		if len(p) < 3 {
			return false
		}
		c.done3(p[0], int(p[1]), p[2])
	case proto.SCWant:
		if len(p) < 2 {
			return false
		}
		secs := time.Duration(p[1]) * time.Second
		if secs > HoldMax {
			secs = HoldMax
		}
		c.mu.Lock()
		if !c.hold {
			c.queue(proto.Frame(proto.CSHold, nil))
			c.hold, c.until = true, time.Now().Add(secs)
		}
		c.mu.Unlock()
	case proto.SCResync:
		if len(p) < 8 { // taken, tv, sid, seg, off16, cat16
			return false
		}
		c.mu.Lock()
		c.last, c.q = nil, nil
		c.sent, c.taken = p[0], p[0]
		if p[1] == 0xFF || int(p[1]) != c.tv {
			c.tv, c.have = -1, nil
		}
		c.segSent = false
		c.catSent = min(int(p[6])|int(p[7])<<8, c.catSent)
		c.hold = false
		c.mu.Unlock()
	case proto.SCCredit:
		if len(p) > 0 {
			c.mu.Lock()
			c.taken = p[0]
			if int(c.sent-c.taken) > proto.Window {
				c.taken = c.sent // credit for more than was sent: don't wedge
			}
			c.mu.Unlock()
		}
	case proto.SCBye:
		if r := c.roomOf(); r != nil {
			r.mu.Lock()
			r.g.Act(c.seat, proto.ActLeave, nil)
			r.mu.Unlock()
		}
		return false
	case proto.SCPing:
		c.mu.Lock()
		c.ack, c.ackSeq = true, 0 // answered with a commit
		c.mu.Unlock()
	default:
		return true
	}
	c.poke()
	return true
}

func (c *conn) roomOf() *Room {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.room
}

// done3: the client played segment seg of shot sid (sid 0: a W hold
// ended) and has taken `taken` bytes: credit starts again from there (bytes
// lost across the pause would otherwise shrink the window for good).
func (c *conn) done3(sid byte, seg int, taken byte) {
	r := c.roomOf()
	if r == nil {
		return
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.sent != taken && debug {
		log.Printf("stream %s: %d bytes lost over the hold", c.nc.RemoteAddr(), c.sent-taken)
	}
	c.sent, c.taken = taken, taken
	c.hold = false
	g := r.g
	if sid == 0 || g.Shot == nil || sid != g.Shot.SID || seg != c.seg || c.played == sid {
		return
	}
	segs := g.Shot.Script(c.caps)
	if c.seg+1 < len(segs) {
		c.seg++
		c.segSent = false
		return
	}
	c.played = sid
	c.seg, c.segSent = 0, false
	g.Played(c.seat, sid)
	// its settles brought its field to the shot's end: say so
	post := r.entries(g.Shot.Post, g.Shot.PostTV, c.caps)
	if c.tv == int(g.Shot.PreTV) {
		for _, f := range project.TerrainFrames(post, post, g.Shot.PreTV, g.Shot.PostTV) {
			c.queue(proto.Frame(proto.CSTerrain, f))
		}
		c.have, c.tv = post, int(g.Shot.PostTV)
	}
}

func (c *conn) writer() {
	tk := time.NewTicker(PushEvery)
	defer tk.Stop()
	for {
		select {
		case <-c.done:
			return
		case <-tk.C:
		case <-c.kick:
		}
		if err := c.push(); err != nil {
			c.nc.Close()
			return
		}
	}
}

// push queues what the client lacks (catalog, terrain, script, state) if
// nothing is waiting and it isn't behind, then sends as much of the queue as
// its credit allows. A held link gets nothing new.
func (c *conn) push() error {
	c.mu.Lock()
	r, ack, hold := c.room, c.ack, c.hold
	behind := int(c.sent-c.taken) > proto.Window/2
	idle := len(c.q) == 0
	c.mu.Unlock()
	if r != nil && idle && !hold && (ack || !behind) {
		r.mu.Lock()
		r.syncLocked()
		c.mu.Lock()
		c.build(r)
		c.mu.Unlock()
		r.mu.Unlock()
	}
	return c.flush()
}

// build queues the documents (both locks held).
func (c *conn) build(r *Room) {
	g := r.g
	cat := c.s.cat
	// a shot the game left behind (it stopped waiting): skip to its end
	if g.Shot != nil && c.played != g.Shot.SID && g.Phase != proto.PhShot {
		c.played, c.seg, c.segSent = g.Shot.SID, 0, false
		g.Played(c.seat, c.played)
	}
	v := game.Viewer{Caps: c.caps, Seat: c.seat, Played: c.played, CatLen: len(cat), CatSum: c.s.sum}
	gated := g.Gated(v)
	for c.catSent < len(cat) {
		n := min(len(cat)-c.catSent, proto.MaxPayload-2)
		p := append([]byte{byte(c.catSent), byte(c.catSent >> 8)}, cat[c.catSent:c.catSent+n]...)
		c.queue(proto.Frame(proto.CSCatalog, p))
		c.catSent += n
	}
	if g.Sim != nil {
		t, tv := g.Sim.T, g.TV
		if gated {
			t, tv = g.Shot.Pre, g.Shot.PreTV
		}
		if c.tv != int(tv) {
			want := r.entries(t, tv, c.caps)
			have, base := c.have, byte(c.tv)
			if c.tv < 0 {
				have = nil
			}
			for _, f := range project.TerrainFrames(have, want, base, tv) {
				c.queue(proto.Frame(proto.CSTerrain, f))
			}
			c.have, c.tv = want, int(tv)
		}
	}
	st := g.State(v)
	if c.ack || !bytes.Equal(st, c.last) {
		for _, p := range proto.Patches(c.last, st) {
			c.queue(proto.Frame(proto.CSPatch, p))
		}
		var seq byte
		if c.ack {
			seq = c.ackSeq
		}
		c.queue(proto.Frame(proto.CSCommit, []byte{seq}))
		c.last, c.ack = st, false
	}
	if gated && g.Phase == proto.PhShot && !c.segSent {
		segs := g.Shot.Script(c.caps)
		if c.seg < len(segs) {
			last := c.seg == len(segs)-1
			hold := c.caps.Hold()
			for _, f := range project.ScriptFrames(g.Shot.SID, byte(c.seg), last, hold, segs[c.seg]) {
				c.queue(proto.Frame(proto.CSScript, f))
			}
			c.segSent = true
			if hold {
				c.hold, c.until = true, time.Now().Add(time.Duration(g.Shot.Log.End*float64(time.Second))+30*time.Second)
			}
		}
	}
}

// flush sends the queue's head as far as the client's credit goes.
func (c *conn) flush() error {
	c.mu.Lock()
	inflight := int(c.sent - c.taken)
	var out []byte
	for len(c.q) > 0 && inflight+len(out)+len(c.q[0]) <= proto.Window {
		out = append(out, c.q[0]...)
		c.q = c.q[1:]
	}
	if len(out) == 0 && len(c.q) > 0 && inflight == 0 {
		out, c.q = c.q[0], c.q[1:] // a frame bigger than the window: alone
	}
	c.sent += uint8(len(out))
	c.total += len(out)
	c.mu.Unlock()
	if len(out) == 0 {
		return nil
	}
	if debug {
		log.Printf("stream %s: out %d bytes, in flight %d", c.nc.RemoteAddr(), len(out), inflight+len(out))
	}
	c.nc.SetWriteDeadline(time.Now().Add(Idle))
	_, err := c.nc.Write(out)
	return err
}

// entries is a terrain version's columns for a client shape (cached: every
// client of that shape gets the same).
func (r *Room) entries(t *world.Terrain, tv byte, cp project.Caps) [][]byte {
	k := fmt.Sprintf("%dx%d/%d/%d/%d", cp.FieldW, cp.FieldH, r.g.Epoch, r.g.Seq, tv)
	if r.cache == nil {
		r.cache = map[string][][]byte{}
	}
	if e, ok := r.cache[k]; ok {
		return e
	}
	if len(r.cache) > 64 {
		r.cache = map[string][][]byte{}
	}
	e := cp.Entries(t)
	r.cache[k] = e
	return e
}
