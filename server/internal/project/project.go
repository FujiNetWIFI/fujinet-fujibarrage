// Package project turns the server's world into what one client sees, at
// that client's resolution and frame rate (its HELLO capabilities): the
// TERRAIN columns, tank positions, and each shot's SCRIPT (docs/PROTOCOL.md).
// Clients never test collisions; they draw what this says.
package project

import (
	"errors"
	"fmt"
	"math"

	"fujibarrage/internal/proto"
	"fujibarrage/internal/world"
)

// Caps is what a client said it is in its HELLO.
type Caps struct {
	FieldW, FieldH int // the playfield, pixels
	Par            int // pixel aspect, width/height in 64ths
	Hz             int // 50 or 60
	TankW, TankH   int // its tank sprite, pixels
	Buf            int // script buffer, bytes
	Bands          int // colour objects a band (0: any)
	Flags          int // proto.Cap*
	Plat           int
}

// Key groups clients that get the same SCRIPT bytes.
func (c Caps) Key() string {
	return fmt.Sprintf("%dx%d@%d/%d", c.FieldW, c.FieldH, c.Hz, c.Buf)
}

// Hold says whether the client pauses its link while a segment plays.
func (c Caps) Hold() bool { return c.Flags&proto.CapHold != 0 }

// ParseHello reads the capability bytes of a HELLO payload.
func ParseHello(p []byte) (Caps, error) {
	if len(p) < proto.HelloLen {
		return Caps{}, errors.New("short hello")
	}
	c := Caps{
		FieldW: int(p[proto.HFieldW]) | int(p[proto.HFieldW+1])<<8,
		FieldH: int(p[proto.HFieldH]),
		Par:    int(p[proto.HPar]),
		Hz:     int(p[proto.HHz]),
		TankW:  int(p[proto.HTankW]),
		TankH:  int(p[proto.HTankH]),
		Buf:    int(p[proto.HBuf]) | int(p[proto.HBuf+1])<<8,
		Bands:  int(p[proto.HBands]),
		Flags:  int(p[proto.HCaps]),
		Plat:   int(p[proto.HPlat]),
	}
	return c, c.Check()
}

// Check reports capabilities the server can't serve.
func (c Caps) Check() error {
	switch {
	case c.FieldW < 16 || c.FieldW > proto.MaxFieldW:
		return fmt.Errorf("field width %d", c.FieldW)
	case c.FieldH < 16 || c.FieldH > proto.MaxFieldH:
		return fmt.Errorf("field height %d", c.FieldH)
	case c.Hz != 50 && c.Hz != 60:
		return fmt.Errorf("frame rate %d", c.Hz)
	case c.Buf < 128:
		return fmt.Errorf("script buffer %d", c.Buf)
	}
	return nil
}

// X and Y map world units to the client's pixels (fractional).
func (c Caps) X(wx float64) float64 { return wx * float64(c.FieldW) / proto.WorldW }
func (c Caps) Y(wy float64) float64 { return wy * float64(c.FieldH) / proto.WorldH }

// IX and IY are the pixel a world point falls in.
func (c Caps) IX(wx float64) int { return int(math.Floor(c.X(wx))) }
func (c Caps) IY(wy float64) int { return int(math.Floor(c.Y(wy))) }

// ---- terrain ------------------------------------------------------------------

// worldCols is client column cx's world columns [a, b).
func (c Caps) worldCols(cx int) (int, int) {
	a := cx * proto.WorldW / c.FieldW
	b := (cx + 1) * proto.WorldW / c.FieldW
	if b <= a {
		b = a + 1
	}
	return a, b
}

func (c Caps) worldRows(cy int) (int, int) {
	a := cy * proto.WorldH / c.FieldH
	b := (cy + 1) * proto.WorldH / c.FieldH
	if b <= a {
		b = a + 1
	}
	return a, b
}

// Column is client column cx's cells, solid where most of the world cells
// a pixel covers are dirt (so a thin tunnel still shows).
func (c Caps) Column(t *world.Terrain, cx int) []bool {
	a, b := c.worldCols(cx)
	out := make([]bool, c.FieldH)
	pre := make([]int, world.H+1)
	cnt := make([]int, c.FieldH)
	for x := a; x < b; x++ {
		col := t.Col(x)
		pre[0] = 0
		for y := 0; y < world.H; y++ {
			pre[y+1] = pre[y] + int(col[y])
		}
		for cy := 0; cy < c.FieldH; cy++ {
			y0, y1 := c.worldRows(cy)
			cnt[cy] += pre[y1] - pre[y0]
		}
	}
	for cy := 0; cy < c.FieldH; cy++ {
		y0, y1 := c.worldRows(cy)
		out[cy] = 2*cnt[cy] > (b-a)*(y1-y0)
	}
	return out
}

// Entry encodes a column (proto.T*): a plain column is one byte, its top.
func Entry(col []bool) []byte {
	type span struct{ top, end int }
	var sp []span
	for y := 0; y < len(col); {
		if !col[y] {
			y++
			continue
		}
		top := y
		for y < len(col) && col[y] {
			y++
		}
		sp = append(sp, span{top, y})
	}
	switch {
	case len(sp) == 0:
		return []byte{proto.TEmpty}
	case len(sp) == 1 && sp[0].end == len(col):
		return []byte{byte(sp[0].top)}
	}
	for len(sp) > 15 { // close the smallest gap
		bi := 0
		for i := 1; i < len(sp)-1; i++ {
			if sp[i+1].top-sp[i].end < sp[bi+1].top-sp[bi].end {
				bi = i
			}
		}
		sp[bi].end = sp[bi+1].end
		sp = append(sp[:bi+1], sp[bi+2:]...)
	}
	out := []byte{byte(proto.TSpans + len(sp))}
	for _, s := range sp {
		out = append(out, byte(s.top), byte(s.end))
	}
	return out
}

// DecodeEntry reads one column entry from b: the column (h cells) and the
// bytes it took.
func DecodeEntry(b []byte, h int) ([]bool, int, bool) {
	if len(b) == 0 {
		return nil, 0, false
	}
	col := make([]bool, h)
	v := int(b[0])
	switch {
	case v == proto.TEmpty:
		return col, 1, true
	case v < proto.TEmpty:
		for y := v; y < h; y++ {
			col[y] = true
		}
		return col, 1, true
	}
	n := v - proto.TSpans
	if len(b) < 1+2*n {
		return nil, 0, false
	}
	for i := 0; i < n; i++ {
		top, end := int(b[1+2*i]), int(b[2+2*i])
		for y := top; y < end && y < h; y++ {
			col[y] = true
		}
	}
	return col, 1 + 2*n, true
}

// Entries is every column's entry, in order.
func (c Caps) Entries(t *world.Terrain) [][]byte {
	out := make([][]byte, c.FieldW)
	for cx := range out {
		out[cx] = Entry(c.Column(t, cx))
	}
	return out
}

// TerrainFrames is the CS_TERRAIN payloads that bring a client from have
// (nil: nothing) at version base to want at version tv. Unchanged runs of
// up to 3 columns are sent anyway rather than start a frame.
func TerrainFrames(have, want [][]byte, base, tv byte) [][]byte {
	if have == nil {
		base = 0xFF
	}
	same := func(x int) bool { return have != nil && string(have[x]) == string(want[x]) }
	var frames [][]byte
	n := len(want)
	for x := 0; x < n; {
		if same(x) {
			x++
			continue
		}
		p := []byte{base, tv, 0, byte(x), byte(x >> 8)}
		for x < n && len(p)+len(want[x]) <= proto.MaxPayload {
			if same(x) { // a gap: worth bridging?
				g := 0
				for x+g < n && same(x+g) && g < 4 {
					g++
				}
				if g >= 4 || x+g >= n {
					break
				}
			}
			p = append(p, want[x]...)
			x++
		}
		frames = append(frames, p)
	}
	if len(frames) == 0 { // nothing changed: still say the version is whole
		frames = append(frames, []byte{base, tv, 0, 0, 0})
	}
	frames[len(frames)-1][2] |= proto.GLast
	return frames
}

// TankY is the client row under a tank standing at world (wx, wy): its
// own surface there, so a tank never floats or sinks on a coarse screen.
func (c Caps) TankY(t *world.Terrain, wx, wy float64) int {
	cx := min(max(c.IX(wx), 0), c.FieldW-1)
	col := c.Column(t, cx)
	y := min(max(int(math.Round(c.Y(wy))), 0), c.FieldH)
	for d := 0; d <= 3; d++ { // the nearest air-over-dirt boundary
		for _, cy := range []int{y + d, y - d} {
			if cy >= 1 && cy <= c.FieldH && (cy == c.FieldH || col[cy]) && !col[cy-1] {
				return cy
			}
		}
	}
	return y
}
