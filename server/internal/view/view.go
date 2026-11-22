// Package view is a client's picture of the field, kept the way an 8-bit
// client keeps it: a bitmap at its own resolution built from TERRAIN
// columns, and SCRIPT segments played into it frame by frame. It is the
// reference the clients are checked against (cmd/refview draws it), so it
// follows docs/PROTOCOL.md to the letter and does nothing else.
package view

import (
	"encoding/binary"
	"fmt"

	"fujibarrage/internal/project"
	"fujibarrage/internal/proto"
)

// Pixel values (the Atari's four, mode E).
const (
	PxSky  = 0
	PxDirt = 1
	PxFx   = 2 // fire, shells, trails
	PxTank = 3
)

// Tank is a seat as drawn.
type Tank struct {
	On       bool
	X, Y     int // bottom centre
	Angle    int
	Energy   int
	ShieldHP int
	Talk     string
	TalkLeft int
}

type track struct {
	on    bool
	style int
	arc   project.Arc
	left  int // steps
	x, y  int // drawn at
	drawn bool
	trail bool
}

type blast struct {
	on       bool
	x, y     int
	rx, ry   int
	kind     int
	n, total int
}

// path is an OP_PATH track: a pixel a frame.
type path struct {
	on    bool
	style int
	x, y  int
	steps []byte
	drawn bool
}

// flow is an OP_FLOW: cells appearing, then (fire) burning out.
type flow struct {
	on        bool
	kind      int
	cells     [][2]int
	shown     int
	frames, n int
	hold      int
}

// dirt is an OP_DIRT: an ellipse of air filling with dirt.
type dirt struct {
	on       bool
	x, y     int
	rx, ry   int
	n, total int
}

// beam is an OP_BEAM: a line drawn, then gone (it burned what it crossed).
type beam struct {
	on   bool
	pts  [][2]int
	left int
}

type settle struct {
	on   bool
	x0   int
	cols [][]bool
	left int
}

// View is one client's field.
type View struct {
	C     project.Caps
	Px    []uint8 // FieldW*FieldH, row major
	Tanks [proto.Seats]Tank
	TV    int // terrain version (-1: none)
	Frame int // frames played

	tr     [16]track
	pa     [4]path
	bl     [16]blast
	st     []settle
	fl     []flow
	di     []dirt
	be     []beam
	wait   int
	Flash  int
	Sounds []int // OP_SOUND ids heard this frame
}

func New(c project.Caps) *View {
	return &View{C: c, Px: make([]uint8, c.FieldW*c.FieldH), TV: -1}
}

func (v *View) At(x, y int) uint8 {
	if x < 0 || y < 0 || x >= v.C.FieldW || y >= v.C.FieldH {
		return PxSky
	}
	return v.Px[y*v.C.FieldW+x]
}

func (v *View) set(x, y int, p uint8) {
	if x < 0 || y < 0 || x >= v.C.FieldW || y >= v.C.FieldH {
		return
	}
	v.Px[y*v.C.FieldW+x] = p
}

// Column writes column x from its cells (dirt or sky; tanks are redrawn).
func (v *View) Column(x int, col []bool) {
	for y, c := range col {
		p := uint8(PxSky)
		if c {
			p = PxDirt
		}
		v.set(x, y, p)
	}
}

// Terrain applies a CS_TERRAIN payload.
func (v *View) Terrain(p []byte) error {
	if len(p) < 5 {
		return fmt.Errorf("short terrain frame")
	}
	base, tv, fl, x := int(p[0]), int(p[1]), p[2], int(p[3])|int(p[4])<<8
	if base != 0xFF && base != v.TV {
		return fmt.Errorf("terrain base %d, have %d", base, v.TV)
	}
	b := p[5:]
	for len(b) > 0 {
		col, n, ok := project.DecodeEntry(b, v.C.FieldH)
		if !ok {
			return fmt.Errorf("bad column entry at %d", x)
		}
		v.Column(x, col)
		x++
		b = b[n:]
	}
	if fl&proto.GLast != 0 {
		v.TV = tv
	}
	return nil
}

// Player plays a segment, one frame per Step.
type Player struct {
	v    *View
	b    []byte
	i    int
	Done bool
}

func (v *View) Play(seg []byte) *Player { return &Player{v: v, b: seg} }

// Step plays one frame: every track, blast and settle moves one step,
// then the ops due this frame run (a blast that ends this frame cuts its
// crater before a SETTLE of the same frame writes the true columns). False
// once OP_END has been reached (the segment is over).
func (pl *Player) Step() (bool, error) {
	v := pl.v
	v.Sounds = v.Sounds[:0]
	if pl.Done {
		return false, nil
	}
	if v.Frame > 0 || pl.i > 0 {
		v.animate()
	}
	if v.wait > 0 {
		v.wait--
	}
	for v.wait == 0 && !pl.Done {
		if err := pl.op(); err != nil {
			return false, err
		}
	}
	v.Frame++
	return !pl.Done, nil
}

func (pl *Player) need(n int) ([]byte, error) {
	if pl.i+n > len(pl.b) {
		return nil, fmt.Errorf("op 0x%02x runs past the segment", pl.b[pl.i-1])
	}
	a := pl.b[pl.i : pl.i+n]
	pl.i += n
	return a, nil
}

func s16(b []byte) int { return int(int16(binary.LittleEndian.Uint16(b))) }
func u16(b []byte) int { return int(binary.LittleEndian.Uint16(b)) }
func s24(b []byte) int32 {
	v := int32(b[0]) | int32(b[1])<<8 | int32(b[2])<<16
	return v << 8 >> 8
}

func (pl *Player) op() error {
	v := pl.v
	if pl.i >= len(pl.b) {
		return fmt.Errorf("segment without OP_END")
	}
	o := pl.b[pl.i]
	pl.i++
	switch o {
	case proto.OpEnd:
		pl.Done = true
	case proto.OpWait:
		a, err := pl.need(1)
		if err != nil {
			return err
		}
		v.wait = int(a[0])
		if v.wait == 0 {
			v.wait = 256
		}
	case proto.OpArc:
		a, err := pl.need(18)
		if err != nil {
			return err
		}
		t := &v.tr[a[0]&15]
		v.eraseHead(t)
		t.on, t.style = true, int(a[1])
		t.arc = project.Arc{X: int32(s16(a[2:])) << 8, Y: int32(s16(a[4:])) << 8,
			VX: s24(a[6:]), VY: s24(a[9:]), AX: int32(int16(u16(a[12:]))), AY: int32(int16(u16(a[14:])))}
		t.left = u16(a[16:])
		t.trail = t.style&15 == proto.StSmoke
		v.drawHead(t)
	case proto.OpPath:
		a, err := pl.need(7)
		if err != nil {
			return err
		}
		steps, err := pl.need(int(a[6]))
		if err != nil {
			return err
		}
		p := &v.pa[a[0]&3]
		v.pathEnd(p)
		*p = path{on: true, style: int(a[1]), x: s16(a[2:]), y: s16(a[4:]), steps: append([]byte(nil), steps...)}
		v.pathHead(p)
	case proto.OpFlow:
		a, err := pl.need(4)
		if err != nil {
			return err
		}
		c, err := pl.need(3 * int(a[3]))
		if err != nil {
			return err
		}
		f := flow{on: true, kind: int(a[0]), frames: max(int(a[1]), 1), hold: int(a[2])}
		for k := 0; k < len(c); k += 3 {
			f.cells = append(f.cells, [2]int{u16(c[k:]), int(c[k+2])})
		}
		v.fl = append(v.fl, f)
	case proto.OpDirt:
		a, err := pl.need(7)
		if err != nil {
			return err
		}
		v.di = append(v.di, dirt{on: true, x: s16(a), y: s16(a[2:]), rx: int(a[4]), ry: int(a[5]), total: max(int(a[6]), 1)})
	case proto.OpBeam:
		a, err := pl.need(10)
		if err != nil {
			return err
		}
		b := beam{on: true, pts: line(s16(a), s16(a[2:]), s16(a[4:]), s16(a[6:])), left: max(int(a[9]), 1)}
		for _, q := range b.pts {
			if p := v.At(q[0], q[1]); p == PxSky || p == PxDirt {
				v.set(q[0], q[1], PxFx)
			}
		}
		v.be = append(v.be, b)
	case proto.OpStop:
		a, err := pl.need(1)
		if err != nil {
			return err
		}
		t := &v.tr[a[0]&15]
		v.eraseHead(t)
		t.on = false
	case proto.OpExplode:
		a, err := pl.need(8)
		if err != nil {
			return err
		}
		for i := range v.bl {
			if !v.bl[i].on {
				v.bl[i] = blast{on: true, x: s16(a), y: s16(a[2:]), rx: int(a[4]), ry: int(a[5]),
					kind: int(a[6]), total: max(int(a[7]), 1)}
				break
			}
		}
	case proto.OpSettle:
		a, err := pl.need(4)
		if err != nil {
			return err
		}
		x0, n, frames := u16(a), int(a[2]), int(a[3])
		s := settle{on: true, x0: x0, left: frames}
		for k := 0; k < n; k++ {
			col, used, ok := project.DecodeEntry(pl.b[pl.i:], v.C.FieldH)
			if !ok {
				return fmt.Errorf("bad settle column")
			}
			pl.i += used
			s.cols = append(s.cols, col)
		}
		if frames == 0 {
			v.applySettle(&s)
		} else {
			v.st = append(v.st, s)
		}
	case proto.OpTalk:
		a, err := pl.need(2)
		if err != nil {
			return err
		}
		txt, err := pl.need(int(a[1]))
		if err != nil {
			return err
		}
		if int(a[0]) < proto.Seats {
			v.Tanks[a[0]].Talk = string(txt)
			v.Tanks[a[0]].TalkLeft = 60
		}
	case proto.OpSound:
		a, err := pl.need(2)
		if err != nil {
			return err
		}
		v.Sounds = append(v.Sounds, int(a[0]))
	case proto.OpShield:
		a, err := pl.need(3)
		if err != nil {
			return err
		}
		if int(a[0]) < proto.Seats {
			v.Tanks[a[0]].ShieldHP = int(a[2])
		}
	case proto.OpDamage:
		a, err := pl.need(3)
		if err != nil {
			return err
		}
		if int(a[0]) < proto.Seats {
			v.Tanks[a[0]].Energy = int(a[2])
		}
	case proto.OpFall:
		a, err := pl.need(6)
		if err != nil {
			return err
		}
		if int(a[0]) < proto.Seats {
			t := &v.Tanks[a[0]]
			t.X, t.Y = s16(a[1:]), int(a[3])
		}
	case proto.OpDeath:
		a, err := pl.need(2)
		if err != nil {
			return err
		}
		if int(a[0]) < proto.Seats {
			v.Tanks[a[0]].On = false
		}
	case proto.OpFlash:
		a, err := pl.need(2)
		if err != nil {
			return err
		}
		v.Flash = int(a[1])
	case proto.OpFire:
		a, err := pl.need(5)
		if err != nil {
			return err
		}
		if int(a[0]) < proto.Seats {
			v.Tanks[a[0]].Angle = int(a[1])
		}
	case proto.OpWind:
		if _, err := pl.need(2); err != nil {
			return err
		}
	case proto.OpShake:
		if _, err := pl.need(1); err != nil {
			return err
		}
	case proto.OpTank:
		a, err := pl.need(5)
		if err != nil {
			return err
		}
		if int(a[0]) < proto.Seats {
			t := &v.Tanks[a[0]]
			t.X, t.Y, t.Angle = s16(a[1:]), int(a[3]), int(a[4])
		}
	default:
		return fmt.Errorf("unknown op 0x%02x at %d", o, pl.i-1)
	}
	return nil
}

func (v *View) eraseHead(t *track) {
	if t.drawn && !t.trail && v.At(t.x, t.y) == PxFx {
		v.set(t.x, t.y, PxSky)
	}
	t.drawn = false
}

func (v *View) drawHead(t *track) {
	x, y := t.arc.Pixel()
	t.x, t.y = x, y
	if p := v.At(x, y); p == PxSky || t.trail && p == PxFx {
		v.set(x, y, PxFx)
		t.drawn = true
	}
}

// animate moves every track and blast one frame.
func (v *View) animate() {
	for i := range v.tr {
		t := &v.tr[i]
		if !t.on {
			continue
		}
		if t.left == 0 {
			v.eraseHead(t)
			t.on = false
			continue
		}
		v.eraseHead(t)
		t.arc.Step()
		t.left--
		v.drawHead(t)
	}
	for i := range v.bl {
		b := &v.bl[i]
		if !b.on {
			continue
		}
		b.n++
		rx, ry := b.rx*b.n/b.total, b.ry*b.n/b.total
		crater := b.kind != proto.ExPlasma && b.kind != proto.ExPuff
		v.ellipse(b.x, b.y, max(rx, 1), max(ry, 1), func(x, y int) {
			// a crater's fireball eats into the dirt; a flash only lights the air
			if p := v.At(x, y); p == PxSky || crater && p == PxDirt {
				v.set(x, y, PxFx)
			}
		})
		if b.n >= b.total {
			v.ellipse(b.x, b.y, b.rx, b.ry, func(x, y int) {
				if crater || v.At(x, y) == PxFx {
					v.set(x, y, PxSky)
				}
			})
			b.on = false
		}
	}
	for i := range v.pa {
		p := &v.pa[i]
		if !p.on {
			continue
		}
		if len(p.steps) == 0 {
			v.pathEnd(p)
			continue
		}
		v.pathLeave(p)
		st := p.steps[0]
		p.steps = p.steps[1:]
		p.x += int(int8(st) >> 4)
		p.y += int(int8(st<<4) >> 4)
		v.pathHead(p)
	}
	fl := v.fl[:0]
	for _, f := range v.fl {
		if f.n < f.frames {
			f.n++
			v.flowShow(&f, len(f.cells)*f.n/f.frames)
		} else if f.hold > 0 {
			f.hold--
		}
		if f.n >= f.frames && f.hold == 0 {
			v.flowEnd(&f)
			continue
		}
		fl = append(fl, f)
	}
	v.fl = fl
	di := v.di[:0]
	for _, d := range v.di {
		d.n++
		rx, ry := max(d.rx*d.n/d.total, 1), max(d.ry*d.n/d.total, 1)
		v.ellipse(d.x, d.y, rx, ry, func(x, y int) {
			if p := v.At(x, y); p == PxSky || p == PxFx {
				v.set(x, y, PxDirt)
			}
		})
		if d.n < d.total {
			di = append(di, d)
		}
	}
	v.di = di
	be := v.be[:0]
	for _, b := range v.be {
		if b.left--; b.left > 0 {
			be = append(be, b)
			continue
		}
		for _, q := range b.pts {
			if v.At(q[0], q[1]) == PxFx {
				v.set(q[0], q[1], PxSky)
			}
		}
	}
	v.be = be
	keep := v.st[:0]
	for _, s := range v.st {
		s.left--
		if s.left <= 0 {
			v.applySettle(&s)
			continue
		}
		keep = append(keep, s)
	}
	v.st = keep
	for i := range v.Tanks {
		if v.Tanks[i].TalkLeft > 0 {
			v.Tanks[i].TalkLeft--
		}
	}
	if v.Flash > 0 {
		v.Flash--
	}
}

// pathHead draws a path's head: a roller over sky only, a digger anywhere
// (it is boring through).
func (v *View) pathHead(p *path) {
	p.drawn = false
	if q := v.At(p.x, p.y); q == PxSky || p.style == proto.StDigger && q == PxDirt {
		v.set(p.x, p.y, PxFx)
		p.drawn = true
	}
}

// pathLeave: the head moves on; a digger leaves air behind it.
func (v *View) pathLeave(p *path) {
	if p.drawn && v.At(p.x, p.y) == PxFx {
		v.set(p.x, p.y, PxSky)
	}
	p.drawn = false
}

func (v *View) pathEnd(p *path) {
	if p.on {
		v.pathLeave(p)
	}
	p.on = false
}

// flowShow: the first n cells are in (fire over the air, mud as dirt).
func (v *View) flowShow(f *flow, n int) {
	for ; f.shown < n; f.shown++ {
		c := f.cells[f.shown]
		p := v.At(c[0], c[1])
		switch {
		case f.kind == proto.FlowMud && (p == PxSky || p == PxFx):
			v.set(c[0], c[1], PxDirt)
		case f.kind != proto.FlowMud && p == PxSky:
			v.set(c[0], c[1], PxFx)
		}
	}
}

// flowEnd: all of it in, then a fire goes out.
func (v *View) flowEnd(f *flow) {
	v.flowShow(f, len(f.cells))
	if f.kind != proto.FlowMud {
		for _, c := range f.cells {
			if v.At(c[0], c[1]) == PxFx {
				v.set(c[0], c[1], PxSky)
			}
		}
	}
}

// EndSegment: the segment's bytes are about to go (a client reuses its
// buffer), so what still reads them finishes now: settles land, paths stop,
// flows run out. Tracks, blasts, dirt and beams carry on.
func (v *View) EndSegment() {
	for i := range v.st {
		v.applySettle(&v.st[i])
	}
	v.st = v.st[:0]
	for i := range v.pa {
		v.pathEnd(&v.pa[i])
	}
	for i := range v.fl {
		v.flowEnd(&v.fl[i])
	}
	v.fl = v.fl[:0]
}

// line is the pixels from (x0, y0) to (x1, y1), Bresenham.
func line(x0, y0, x1, y1 int) [][2]int {
	dx, dy := x1-x0, y1-y0
	sx, sy := 1, 1
	if dx < 0 {
		dx, sx = -dx, -1
	}
	if dy < 0 {
		dy, sy = -dy, -1
	}
	var out [][2]int
	err := dx - dy
	for {
		out = append(out, [2]int{x0, y0})
		if x0 == x1 && y0 == y1 || len(out) > 4096 {
			return out
		}
		e2 := 2 * err
		if e2 > -dy {
			err -= dy
			x0 += sx
		}
		if e2 < dx {
			err += dx
			y0 += sy
		}
	}
}

func (v *View) applySettle(s *settle) {
	for k, col := range s.cols {
		v.Column(s.x0+k, col)
	}
}

func (v *View) ellipse(cx, cy, rx, ry int, f func(x, y int)) {
	for y := -ry; y <= ry; y++ {
		for x := -rx; x <= rx; x++ {
			if x*x*ry*ry+y*y*rx*rx <= rx*rx*ry*ry {
				f(cx+x, cy+y)
			}
		}
	}
}

// Busy reports whether anything still moves (the segment's tail).
func (v *View) Busy() bool {
	for _, t := range v.tr {
		if t.on {
			return true
		}
	}
	for _, b := range v.bl {
		if b.on {
			return true
		}
	}
	for _, p := range v.pa {
		if p.on {
			return true
		}
	}
	return len(v.st)+len(v.fl)+len(v.di)+len(v.be) > 0
}

// Tracks reports where each live track's head is (for tests).
func (v *View) Tracks() [][3]int {
	var out [][3]int
	for i, t := range v.tr {
		if t.on {
			out = append(out, [3]int{i, t.x, t.y})
		}
	}
	return out
}
