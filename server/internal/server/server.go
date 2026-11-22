// Package server runs FujiBarrage's rooms and speaks the stream protocol
// (docs/PROTOCOL.md) to clients over TCP: FujiNet netstream on the 8-bits.
package server

import (
	"log"
	"strings"
	"sync"
	"time"

	"fujibarrage/internal/game"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/talk"
)

// RoomInfo is what the lobby and the room list show.
type RoomInfo struct {
	ID, Name string
	Humans   int
	Phase    int
	Round    int
}

// Server holds the rooms.
type Server struct {
	Now      func() time.Time
	OnChange func(RoomInfo) // a room's people or phase changed (the lobby)

	mu    sync.Mutex
	rooms []*Room
	cat   []byte
	sum   [2]byte
}

// Room is one game and the connections in it.
type Room struct {
	ID, Name string
	Dev      bool

	s     *Server
	mu    sync.Mutex
	g     *game.Game
	t0    time.Time
	conns map[*conn]struct{}
	info  RoomInfo
	cache map[string][][]byte // terrain columns by client shape, round and version
}

// roomDefs: the public rooms, each with its own flavour of settings.
var roomDefs = []struct {
	id, name string
	set      func(*[proto.NSettings]int)
}{
	{"room1", "OPEN RANGE", func(s *[proto.NSettings]int) {}},
	{"room2", "BOUNCE HOUSE", func(s *[proto.NSettings]int) { s[proto.SetWalls] = proto.WallRubber }},
	{"room3", "WRAPAROUND", func(s *[proto.NSettings]int) { s[proto.SetWalls] = proto.WallWrap }},
	{"room4", "MAYHEM", func(s *[proto.NSettings]int) {
		s[proto.SetWalls] = proto.WallErratic
		s[proto.SetChangeWind] = 1
		s[proto.SetMaxWind] = 12
		s[proto.SetCash] = 40
		s[proto.SetMode] = proto.ModeSimultaneous
	}},
}

// DevSettings are the dev rooms' settings, changed from the defaults (rigs
// and tests: -devset).
var DevSettings = map[int]int{}

// New makes a server with the public rooms room1..4 and the dev rooms
// dev1..3 (listed only when asked for).
func New(seed uint64, book *talk.Book) *Server {
	s := &Server{Now: time.Now, cat: game.Catalog()}
	s.sum[0], s.sum[1] = proto.Fletcher16(s.cat)
	for i, d := range roomDefs {
		set := game.Defaults()
		d.set(&set)
		s.add(d.id, d.name, false, set, seed+uint64(i+1), book)
	}
	for i := 1; i <= 3; i++ {
		set := game.Defaults()
		for k, v := range DevSettings {
			set[k] = v
		}
		s.add("dev"+string(rune('0'+i)), "DEV "+string(rune('0'+i)), true, set, seed+100+uint64(i), book)
	}
	return s
}

func (s *Server) add(id, name string, dev bool, set [proto.NSettings]int, seed uint64, book *talk.Book) {
	r := &Room{ID: id, Name: name, Dev: dev, s: s, g: game.New(seed, set, book), t0: s.Now(), conns: map[*conn]struct{}{}}
	r.g.Dev = dev
	r.info = r.infoLocked()
	s.rooms = append(s.rooms, r)
}

// Room is the room with id (any case), or nil.
func (s *Server) Room(id string) *Room {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, r := range s.rooms {
		if strings.EqualFold(r.ID, id) {
			return r
		}
	}
	return nil
}

// Rooms lists the rooms (dev: the dev ones too).
func (s *Server) Rooms(dev bool) []RoomInfo {
	s.mu.Lock()
	rs := append([]*Room(nil), s.rooms...)
	s.mu.Unlock()
	var out []RoomInfo
	for _, r := range rs {
		if r.Dev && !dev {
			continue
		}
		r.mu.Lock()
		out = append(out, r.infoLocked())
		r.mu.Unlock()
	}
	return out
}

// Run advances every room to the wall clock, every tick, until stop closes.
func (s *Server) Run(tick time.Duration, stop <-chan struct{}) {
	tk := time.NewTicker(tick)
	defer tk.Stop()
	for {
		select {
		case <-stop:
			return
		case <-tk.C:
		}
		s.Update()
	}
}

// Update advances every room to now.
func (s *Server) Update() {
	s.mu.Lock()
	rs := append([]*Room(nil), s.rooms...)
	s.mu.Unlock()
	for _, r := range rs {
		r.mu.Lock()
		r.syncLocked()
		changed := r.noteChange()
		info := r.info
		r.mu.Unlock()
		if changed && s.OnChange != nil {
			s.OnChange(info)
		}
	}
}

// syncLocked runs the game up to now.
func (r *Room) syncLocked() {
	want := int64(r.s.Now().Sub(r.t0) * game.Hz / time.Second)
	if n := want - r.g.J; n > 0 {
		if n > 600 {
			n = 600 // a stalled server doesn't replay minutes in one go
			r.t0 = r.s.Now().Add(-time.Duration(r.g.J+n) * time.Second / game.Hz)
		}
		r.g.Advance(int(n))
	}
	spect := 0
	for c := range r.conns {
		if c.seat < 0 {
			spect++
		}
	}
	r.g.Spect = spect
}

func (r *Room) infoLocked() RoomInfo {
	return RoomInfo{ID: r.ID, Name: r.Name, Humans: r.g.Humans(), Phase: r.g.Phase, Round: r.g.Round}
}

// noteChange reports (once) that the lobby's view of r changed.
func (r *Room) noteChange() bool {
	i := r.infoLocked()
	if i.Humans == r.info.Humans && (i.Phase == proto.PhWait) == (r.info.Phase == proto.PhWait) {
		return false
	}
	r.info = i
	return true
}

func (r *Room) join(c *conn) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.syncLocked()
	r.conns[c] = struct{}{}
	c.seat = -1
	if !c.watch {
		c.seat = r.g.Join(c.name)
	}
	c.played = r.g.SID
	if r.g.Shot != nil && c.seat >= 0 {
		r.g.Played(c.seat, r.g.SID)
	}
	log.Printf("room %s: %s in seat %d (%s)", r.ID, c.name, c.seat, c.caps.Key())
}

func (r *Room) part(c *conn) {
	r.mu.Lock()
	defer r.mu.Unlock()
	delete(r.conns, c)
	if c.seat < 0 {
		r.g.Unqueue(c.name)
		return
	}
	for o := range r.conns {
		if o.seat == c.seat {
			return // another link for the same player
		}
	}
	r.g.SetConn(c.seat, false)
}

// record is a CS_ROOM record.
func record(ri RoomInfo) []byte {
	b := make([]byte, proto.RoomRec)
	copy(b[0:8], ri.ID)
	copy(b[8:20], ri.Name)
	b[20], b[21], b[22], b[23] = byte(ri.Humans), proto.Seats, byte(ri.Phase), byte(ri.Round)
	return b
}
