// Package client speaks the stream protocol from the player's side, as the
// 8-bit clients do (docs/PROTOCOL.md): for the test players and the tests.
// Its field is a view.View, so what it draws is what a client draws.
package client

import (
	"bufio"
	"errors"
	"net"
	"os"
	"strings"
	"time"

	"fujibarrage/internal/project"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/view"
)

// Segment is a whole SCRIPT segment, ready to play.
type Segment struct {
	SID, Seg byte
	Last     bool // the script's last
	Hold     bool // the link holds until D
	B        []byte
}

// Client is one connection.
type Client struct {
	nc    net.Conn
	br    *bufio.Reader
	buf   []byte
	stage [proto.StateSize]byte
	owed  int
	seq   byte
	seg   []byte
	segID [2]byte

	Taken   uint8
	Caps    project.Caps
	State   [proto.StateSize]byte // the last whole STATE
	Commits int
	Acked   byte
	Rooms   [][]byte // CS_ROOM records, as listed
	Err     byte
	BytesIn int
	Catalog []byte
	V       *view.View
	Ready   []Segment // segments whole and not played yet
	Held    bool      // a CS_HOLD came (W)
	TerrErr error
	Garbled int
}

// Dial connects to a server's stream port.
func Dial(addr string) (*Client, error) {
	nc, err := net.DialTimeout("tcp", addr, 5*time.Second)
	if err != nil {
		return nil, err
	}
	return &Client{nc: nc, br: bufio.NewReader(nc)}, nil
}

func (c *Client) Close() error { return c.nc.Close() }

func (c *Client) send(typ byte, p []byte) error {
	c.nc.SetWriteDeadline(time.Now().Add(5 * time.Second))
	_, err := c.nc.Write(proto.Frame(typ, p))
	return err
}

func pad(s string, n int) []byte {
	b := make([]byte, n)
	copy(b, strings.ToUpper(s))
	return b
}

// Atari is the Atari client's shape.
var Atari = project.Caps{FieldW: 160, FieldH: 168, Par: 109, Hz: 60, TankW: 7, TankH: 4, Buf: 3072,
	Bands: 5, Flags: proto.CapHold, Plat: proto.PlatAtari}

// Intv is the Intellivision client's shape: no CAP_HOLD (the link stays up
// while it plays; the next segment still waits for its D).
var Intv = project.Caps{FieldW: 160, FieldH: 80, Par: 54, Hz: 60, TankW: 7, TankH: 5, Buf: 1024,
	Bands: 8, Plat: proto.PlatIntv}

// Astrocade is the Bally Astrocade client's shape: its 4K of RAM (screen
// included) holds a 160x56 field and a 240-byte script buffer.
var Astrocade = project.Caps{FieldW: 160, FieldH: 56, Par: 54, Hz: 60, TankW: 7, TankH: 5, Buf: 240,
	Bands: 2, Flags: proto.CapHold, Plat: proto.PlatAstrocade}

// Apple2 is the Apple II client's shape: HGR as the field itself, 140 colour
// pixels (two dots each) by 160 lines over mixed mode's text rows.
var Apple2 = project.Caps{FieldW: 140, FieldH: 160, Par: 117, Hz: 60, TankW: 5, TankH: 5, Buf: 1536,
	Bands: 0, Flags: proto.CapHold, Plat: proto.PlatApple2}

// Coco is the CoCo 1/2 client's shape: PMODE 4 artifact colour, 128 colour
// pixels (two dots each) by 168 lines over a 3-row HUD.
var Coco = project.Caps{FieldW: 128, FieldH: 168, Par: 109, Hz: 60, TankW: 5, TankH: 5, Buf: 1536,
	Bands: 0, Flags: proto.CapHold, Plat: proto.PlatCoco}

// Coco3 is the CoCo 3 client's shape: GIME 320x200 in 16 colours, the field
// 320x176 over a 3-row HUD.
var Coco3 = project.Caps{FieldW: 320, FieldH: 176, Par: 55, Hz: 60, TankW: 12, TankH: 6, Buf: 1536,
	Bands: 0, Flags: proto.CapHold, Plat: proto.PlatCoco3}

// Adam is the Coleco Adam client's shape: TMS9918 Graphics II, the field
// 256x168 (21 character rows) over a 3-row HUD; the tanks are sprites.
var Adam = project.Caps{FieldW: 256, FieldH: 168, Par: 73, Hz: 60, TankW: 7, TankH: 5, Buf: 1536,
	Bands: 4, Flags: proto.CapHold, Plat: proto.PlatAdam}

// Atari5200 is the Atari 5200 client's shape: the Atari's field, with a
// smaller script buffer (16K of RAM).
var Atari5200 = project.Caps{FieldW: 160, FieldH: 168, Par: 109, Hz: 60, TankW: 7, TankH: 5, Buf: 1024,
	Bands: 4, Flags: proto.CapHold, Plat: proto.PlatAtari5200}

// Msdos is the PC client's shape with CGA: 320x200 in 4 colours, the field
// 320x176 over a 3-row HUD (the CoCo 3's shape, a bigger buffer).
var Msdos = project.Caps{FieldW: 320, FieldH: 176, Par: 55, Hz: 60, TankW: 12, TankH: 6, Buf: 4096,
	Bands: 0, Flags: proto.CapHold, Plat: proto.PlatMsdos}

// MsdosVga is the PC client's shape with VGA or MCGA (mode 13h): the CGA
// build's.
var MsdosVga = project.Caps{FieldW: 320, FieldH: 176, Par: 55, Hz: 60, TankW: 12, TankH: 6, Buf: 4096,
	Bands: 0, Flags: proto.CapHold, Plat: proto.PlatMsdosVga}

// Arcadia is the Emerson Arcadia 2001 client's shape: the 2637's block
// graphics make the screen RAM a 48x48-block field (a cell is 3x2 blocks,
// a block 2.7 pixels by 4 lines: 78/64 wide), its 4 sprites the tanks; 1K
// of cart RAM behind a mailbox holds a small script buffer.
var Arcadia = project.Caps{FieldW: 48, FieldH: 48, Par: 78, Hz: 60, TankW: 2, TankH: 1, Buf: 160,
	Bands: 0, Slots: project.PlatSlots(proto.PlatArcadia), Flags: proto.CapHold, Plat: proto.PlatArcadia}

// H89 is the Heathkit H89 client's shape: the H19's graphics characters, a
// cell two blocks by two (a block 4 dots by 5 lines of the desktop's 640x250
// screen): the field 160x44 blocks over a 2-row HUD; the tanks are cells.
var H89 = project.Caps{FieldW: 160, FieldH: 44, Par: 26, Hz: 60, TankW: 4, TankH: 2, Buf: 2048,
	Bands: 0, Flags: proto.CapHold, Plat: proto.PlatH89}

// H89Hdos is the same client under HDOS 2.0, whose overlays leave a 1024-byte
// script buffer.
var H89Hdos = project.Caps{FieldW: 160, FieldH: 44, Par: 26, Hz: 60, TankW: 4, TankH: 2, Buf: 896,
	Bands: 0, Flags: proto.CapHold, Plat: proto.PlatH89}

// Sms is the Sega Master System client's shape: Mode 4 tiles, the Adam's
// field (256x168 over a 3-row HUD); the tanks are sprites, 8 on a line.
var Sms = project.Caps{FieldW: 256, FieldH: 168, Par: 73, Hz: 60, TankW: 7, TankH: 5, Buf: 1536,
	Bands: 8, Flags: proto.CapHold, Plat: proto.PlatSms}

// SmsPal is the same on a 50 Hz console.
var SmsPal = project.Caps{FieldW: 256, FieldH: 168, Par: 73, Hz: 50, TankW: 7, TankH: 5, Buf: 1536,
	Bands: 8, Flags: proto.CapHold, Plat: proto.PlatSms}

// Nes is the NES client's shape: the Adam's field (256x168, 21 rows of
// tiles in CHR-RAM over a 3-row HUD); the tanks are sprites, 8 on a line.
// Its RAM is 2K and the cartridge's 8K: a 1K script buffer.
var Nes = project.Caps{FieldW: 256, FieldH: 168, Par: 73, Hz: 60, TankW: 7, TankH: 5, Buf: 1024,
	Bands: 8, Flags: proto.CapHold, Plat: proto.PlatNes}

// NesPal is the same on a 50 Hz console.
var NesPal = project.Caps{FieldW: 256, FieldH: 168, Par: 73, Hz: 50, TankW: 7, TankH: 5, Buf: 1024,
	Bands: 8, Flags: proto.CapHold, Plat: proto.PlatNes}

// ChannelF is the Fairchild Channel F client's shape: the 128x64 VRAM shows
// 102x58 pixels (a pixel ~0.76 as wide as tall: 48/64), the field 102x46
// over two text rows; the tanks are drawn into the field. The cart's 30K of
// RAM holds a 1.5K script buffer.
var ChannelF = project.Caps{FieldW: 102, FieldH: 46, Par: 48, Hz: 60, TankW: 5, TankH: 3, Buf: 1536,
	Bands: 0, Flags: proto.CapHold, Plat: proto.PlatChannelF}

// Hello sits down in room as name, with these capabilities.
func (c *Client) Hello(room, name string, caps project.Caps, flags byte) error {
	c.Caps = caps
	c.V = view.New(caps)
	p := make([]byte, proto.HelloLen)
	p[0], p[proto.HFlags] = proto.Version, flags
	copy(p[proto.HRoom:], pad(room, proto.RoomLen))
	copy(p[proto.HName:], pad(name, proto.NameLen))
	p[proto.HFieldW], p[proto.HFieldW+1] = byte(caps.FieldW), byte(caps.FieldW>>8)
	p[proto.HFieldH] = byte(caps.FieldH)
	p[proto.HPar], p[proto.HHz] = byte(caps.Par), byte(caps.Hz)
	p[proto.HTankW], p[proto.HTankH] = byte(caps.TankW), byte(caps.TankH)
	p[proto.HBuf], p[proto.HBuf+1] = byte(caps.Buf), byte(caps.Buf>>8)
	p[proto.HBands], p[proto.HCaps], p[proto.HPlat] = byte(caps.Bands), byte(caps.Flags), byte(caps.Plat)
	return c.send(proto.SCHello, p)
}

func (c *Client) ListRooms(dev bool) error {
	if dev {
		return c.send(proto.SCRooms, []byte{1})
	}
	return c.send(proto.SCRooms, nil)
}

// Act sends an action and returns its seq (never 0).
func (c *Client) Act(act int, args ...byte) (byte, error) {
	if c.seq++; c.seq == 0 {
		c.seq = 1
	}
	return c.seq, c.send(proto.SCAct, append([]byte{c.seq, byte(act)}, args...))
}

// Done says segment seg of shot sid is played (sid 0: a W hold is over).
func (c *Client) Done(sid, seg byte) error { return c.send(proto.SCDone, []byte{sid, seg, c.Taken}) }

// Want asks to hold the link for secs.
func (c *Client) Want(reason, secs byte) error { return c.send(proto.SCWant, []byte{reason, secs}) }

// Resync asks for everything again (nothing kept: terrain unknown).
func (c *Client) Resync() error {
	return c.send(proto.SCResync, []byte{c.Taken, 0xFF, 0, 0, 0, 0, byte(len(c.Catalog)), byte(len(c.Catalog) >> 8)})
}

func (c *Client) Ping() error { return c.send(proto.SCPing, nil) }
func (c *Client) Bye() error  { return c.send(proto.SCBye, nil) }

// Poll reads what arrives within wait and applies it. It returns the
// number of commits read.
func (c *Client) Poll(wait time.Duration) (int, error) {
	n0 := c.Commits
	c.nc.SetReadDeadline(time.Now().Add(wait))
	for {
		b, err := c.br.ReadByte()
		if err != nil {
			if errors.Is(err, os.ErrDeadlineExceeded) {
				err = nil
			}
			return c.Commits - n0, err
		}
		c.BytesIn++
		c.Taken++
		if c.owed++; c.owed >= 48 {
			c.owed = 0
			if err := c.send(proto.SCCredit, []byte{c.Taken}); err != nil {
				return c.Commits - n0, err
			}
		}
		if b != 0 {
			if len(c.buf) < 2*proto.MaxFrame {
				c.buf = append(c.buf, b)
			}
			continue
		}
		raw, ok := proto.COBSDecode(c.buf)
		c.buf = c.buf[:0]
		if !ok {
			c.Garbled++
			continue
		}
		typ, p, ok := proto.Unframe(raw)
		if !ok {
			c.Garbled++
			continue
		}
		c.frame(typ, p)
		if typ == proto.CSCommit && c.br.Buffered() == 0 {
			return c.Commits - n0, nil
		}
	}
}

func (c *Client) frame(typ byte, p []byte) {
	switch typ {
	case proto.CSPatch:
		proto.ApplyPatch(c.stage[:], p)
	case proto.CSCommit:
		c.State = c.stage
		c.Commits++
		if len(p) > 0 && p[0] != 0 {
			c.Acked = p[0]
		}
	case proto.CSRoom:
		if len(p) >= 2 && p[1] > 0 && len(p) >= 2+proto.RoomRec {
			if p[0] == 0 {
				c.Rooms = nil
			}
			c.Rooms = append(c.Rooms, append([]byte(nil), p[2:2+proto.RoomRec]...))
		}
	case proto.CSError:
		if len(p) > 0 {
			c.Err = p[0]
		}
	case proto.CSCatalog:
		if len(p) >= 2 {
			off := int(p[0]) | int(p[1])<<8
			if off <= len(c.Catalog) {
				c.Catalog = append(c.Catalog[:off], p[2:]...)
			}
		}
	case proto.CSTerrain:
		if c.V != nil {
			if err := c.V.Terrain(p); err != nil {
				c.TerrErr = err
			}
		}
	case proto.CSScript:
		if len(p) < 5 {
			return
		}
		sid, seg, fl, off := p[0], p[1], p[2], int(p[3])|int(p[4])<<8
		if off == 0 || [2]byte{sid, seg} != c.segID {
			c.seg, c.segID = nil, [2]byte{sid, seg}
		}
		if off != len(c.seg) {
			return // a frame missing: the segment comes again after R
		}
		c.seg = append(c.seg, p[5:]...)
		if fl&proto.SLastFrame != 0 {
			c.Ready = append(c.Ready, Segment{SID: sid, Seg: seg, Last: fl&proto.SLastSeg != 0,
				Hold: fl&proto.SHold != 0, B: c.seg})
			c.seg = nil
		}
	case proto.CSHold:
		c.Held = true
	}
}

// Feed applies one decoded frame as if it had come from the server (a
// replay of a tap log: cmd/tapview).
func (c *Client) Feed(typ byte, p []byte) { c.frame(typ, p) }

// PlayReady plays every ready segment to its end, with nothing said to the
// server.
func (c *Client) PlayReady() error {
	for len(c.Ready) > 0 {
		s := c.Ready[0]
		c.Ready = c.Ready[1:]
		pl := c.V.Play(s.B)
		for {
			more, err := pl.Step()
			if err != nil {
				return err
			}
			if !more {
				break
			}
		}
		c.V.EndSegment()
	}
	return nil
}

// PlayAll plays every ready segment to its end (all at once: no frame
// pacing) and says so to the server. It returns the frames played.
func (c *Client) PlayAll() (int, error) {
	n := 0
	for len(c.Ready) > 0 {
		s := c.Ready[0]
		c.Ready = c.Ready[1:]
		pl := c.V.Play(s.B)
		for {
			more, err := pl.Step()
			if err != nil {
				return n, err
			}
			n++
			if !more {
				break
			}
		}
		c.V.EndSegment()
		if err := c.Done(s.SID, s.Seg); err != nil {
			return n, err
		}
	}
	return n, nil
}

// Seat is this client's seat in State, or -1.
func (c *Client) Seat() int {
	s := c.State[proto.OffMySeat]
	if s == proto.NoSeat {
		return -1
	}
	return int(s)
}

// P is seat's record in State.
func (c *Client) P(seat int) []byte {
	o := proto.OffSeats + seat*proto.SeatSize
	return c.State[o : o+proto.SeatSize]
}

// U16 reads a little-endian uint16 from State.
func (c *Client) U16(off int) int { return int(c.State[off]) | int(c.State[off+1])<<8 }
