package project

import (
	"math"
	"sort"

	"fujibarrage/internal/proto"
	"fujibarrage/internal/sim"
	"fujibarrage/internal/world"
)

// Arc is an OP_ARC track as a client runs it: position and velocity in
// 12.12 fixed point pixels, acceleration 4.12; each frame
//
//	X += VX; Y += VY; VX += AX; VY += AY
//
// and the pixel shown is (X >> 12, Y >> 12), arithmetic shifts. The fitter
// below runs the very same integers, so it knows what every client draws.
type Arc struct{ X, Y, VX, VY, AX, AY int32 }

func (a *Arc) Step() {
	a.X += a.VX
	a.Y += a.VY
	a.VX += a.AX
	a.VY += a.AY
}

func (a Arc) Pixel() (int, int) { return int(a.X >> 12), int(a.Y >> 12) }

const (
	fixOne   = 4096
	arcTol   = 0.6 // pixels the drawn path may stray from the true one
	maxSlots = 16  // tracks a client keeps at once
)

type fpt struct {
	x, y float64
	jump bool
}

// arcSeg is one OP_ARC: the track from frame k0 for n steps.
type arcSeg struct {
	k0, n int
	a     Arc
}

func q12(v float64) int32 { return int32(math.Round(v * fixOne)) }

// fitArcs covers pts (a position per frame) with as few arcs as keep the
// drawn path within arcTol of it.
func fitArcs(pts []fpt) []arcSeg {
	var segs []arcSeg
	for k0 := 0; k0 < len(pts); {
		// the start is sent as 12.4: its low 8 fraction bits are 0
		var start Arc
		start.X = int32(math.Round(pts[k0].x*16)) << 8
		start.Y = int32(math.Round(pts[k0].y*16)) << 8
		if k0 == len(pts)-1 {
			segs = append(segs, arcSeg{k0, 0, start})
			break
		}
		vx, vy, ax, ay := fitQuad(pts, k0)
		start.VX, start.VY, start.AX, start.AY = q12(vx), q12(vy), q12(ax), q12(ay)
		st := start
		n := 0
		for k := k0 + 1; k < len(pts); k++ {
			if pts[k].jump {
				break
			}
			st.Step()
			ex := float64(st.X)/fixOne - pts[k].x
			ey := float64(st.Y)/fixOne - pts[k].y
			if math.Abs(ex) > arcTol || math.Abs(ey) > arcTol {
				break
			}
			n++
		}
		if n == 0 && !pts[k0+1].jump {
			n = 1 // can't do better than one step: a keyframe each frame
		}
		segs = append(segs, arcSeg{k0, n, start})
		if n == 0 {
			k0++ // a jump right after: the next arc starts past it
		} else {
			k0 += n
		}
	}
	return segs
}

// fitQuad: velocity and acceleration (per frame) that best fit
// p_k = p_0 + k v + k(k-1)/2 a over the next frames, least squares.
func fitQuad(pts []fpt, k0 int) (vx, vy, ax, ay float64) {
	L := min(24, len(pts)-1-k0)
	for k := 1; k <= L; k++ {
		if pts[k0+k].jump {
			L = k - 1
			break
		}
	}
	if L <= 0 {
		return 0, 0, 0, 0
	}
	if L == 1 {
		return pts[k0+1].x - pts[k0].x, pts[k0+1].y - pts[k0].y, 0, 0
	}
	var s11, s12, s22, b1x, b2x, b1y, b2y float64
	for k := 1; k <= L; k++ {
		f1 := float64(k)
		f2 := float64(k*(k-1)) / 2
		dx := pts[k0+k].x - pts[k0].x
		dy := pts[k0+k].y - pts[k0].y
		s11 += f1 * f1
		s12 += f1 * f2
		s22 += f2 * f2
		b1x += f1 * dx
		b2x += f2 * dx
		b1y += f1 * dy
		b2y += f2 * dy
	}
	det := s11*s22 - s12*s12
	if math.Abs(det) < 1e-9 {
		return pts[k0+1].x - pts[k0].x, pts[k0+1].y - pts[k0].y, 0, 0
	}
	vx = (b1x*s22 - b2x*s12) / det
	ax = (s11*b2x - s12*b1x) / det
	vy = (b1y*s22 - b2y*s12) / det
	ay = (s11*b2y - s12*b1y) / det
	return
}

// ---- the script -------------------------------------------------------------

type op struct {
	f, seq int
	b      []byte
}

type enc struct {
	c      Caps
	ops    []op
	seq    int
	slots  [maxSlots]int  // frame each slot is free from
	pslots [pathSlots]int // and each path slot
}

func (e *enc) frame(t float64) int { return int(math.Round(t * float64(e.c.Hz))) }

func (e *enc) add(f int, b ...byte) {
	e.seq++
	e.ops = append(e.ops, op{f, e.seq, b})
}

func le16(v int) []byte { return []byte{byte(v), byte(v >> 8)} }
func le24(v int32) []byte {
	return []byte{byte(v), byte(v >> 8), byte(v >> 16)}
}

func clamp16(v int) int { return max(min(v, 32767), -32768) }

func (e *enc) px(x float64) int { return clamp16(int(math.Floor(e.c.X(x)))) }
func (e *enc) py(y float64) int { return clamp16(int(math.Floor(e.c.Y(y)))) }

func (e *enc) slot(f0, f1 int) int {
	for i, free := range e.slots {
		if e.c.Slots > 0 && i >= e.c.Slots {
			break
		}
		if free <= f0 {
			e.slots[i] = f1 + 1
			return i
		}
	}
	return -1 // full: this track isn't shown
}

// samples is tr at each client frame from f0, in client pixels (a jump:
// it got there by a wrap).
func (e *enc) samples(tr *sim.Track) (int, []fpt) {
	hz := float64(e.c.Hz)
	tEnd := tr.T0 + float64(len(tr.Pts)-1)*sim.DT
	f0 := int(math.Ceil(tr.T0*hz - 1e-9))
	f1 := int(math.Floor(tEnd*hz + 1e-9))
	if f1 < f0 {
		f1 = f0
	}
	pts := make([]fpt, 0, f1-f0+1)
	last := -1 // the sample the last frame was at
	for f := f0; f <= f1; f++ {
		s := math.Max((float64(f)/hz-tr.T0)/sim.DT, 0)
		i := min(int(math.Floor(s)), len(tr.Pts)-1)
		jump := false
		for k := last + 1; k <= min(i+1, len(tr.Pts)-1) && last >= 0; k++ {
			if tr.Pts[k].Jump {
				jump = true
			}
		}
		var p sim.Pt
		switch {
		case i >= len(tr.Pts)-1:
			p = tr.Pts[len(tr.Pts)-1]
		case jump:
			p = tr.Pts[i+1]
		default:
			a, b := tr.Pts[i], tr.Pts[i+1]
			fr := s - float64(i)
			p = sim.Pt{X: a.X + (b.X-a.X)*fr, Y: a.Y + (b.Y-a.Y)*fr}
		}
		last = i
		pts = append(pts, fpt{e.c.X(p.X), e.c.Y(p.Y), jump})
	}
	return f0, pts
}

// track emits a sim track: arcs, or for rollers and diggers (which follow
// the ground) paths.
func (e *enc) track(tr *sim.Track) {
	if len(tr.Pts) == 0 {
		return
	}
	if tr.Style == proto.StRoller || tr.Style == proto.StDigger {
		e.path(tr)
		return
	}
	f0, pts := e.samples(tr)
	f1 := f0 + len(pts) - 1
	slot := e.slot(f0, f1)
	if slot < 0 {
		return
	}
	style := tr.Style
	if style == proto.StSmoke {
		style |= tr.Seat << 4
	}
	for _, s := range fitArcs(pts) {
		b := []byte{proto.OpArc, byte(slot), byte(style)}
		b = append(b, le16(int(s.a.X>>8))...)
		b = append(b, le16(int(s.a.Y>>8))...)
		b = append(b, le24(s.a.VX)...)
		b = append(b, le24(s.a.VY)...)
		b = append(b, le16(int(s.a.AX))...)
		b = append(b, le16(int(s.a.AY))...)
		b = append(b, le16(s.n)...)
		e.add(f0+s.k0, b...)
	}
}

const (
	pathSlots = 4   // OP_PATH tracks a client keeps at once
	pathMax   = 180 // steps in one OP_PATH
	flowMax   = 60  // cells in one OP_FLOW
)

// path emits OP_PATHs: the pixel the track is on each frame, as steps of
// -8..7 pixels; a bigger step (or a full op) starts a new OP_PATH there.
func (e *enc) path(tr *sim.Track) {
	f0, pts := e.samples(tr)
	f1 := f0 + len(pts) - 1
	slot := -1
	for i, free := range e.pslots {
		if free <= f0 {
			slot = i
			e.pslots[i] = f1 + 1
			break
		}
	}
	if slot < 0 {
		return
	}
	px := func(p fpt) (int, int) { return int(math.Floor(p.x)), int(math.Floor(p.y)) }
	for k := 0; k < len(pts); {
		x, y := px(pts[k])
		b := []byte{proto.OpPath, byte(slot), byte(tr.Style)}
		b = append(b, le16(clamp16(x))...)
		b = append(b, le16(clamp16(y))...)
		b = append(b, 0)
		n := 0
		k0 := k
		for k+1 < len(pts) && n < pathMax {
			nx, ny := px(pts[k+1])
			dx, dy := nx-x, ny-y
			if dx < -8 || dx > 7 || dy < -8 || dy > 7 {
				break
			}
			b = append(b, byte(dx<<4)|byte(dy&15))
			x, y = nx, ny
			k++
			n++
		}
		b[7] = byte(n)
		e.add(f0+k0, b...)
		k++
	}
}

// flow emits a flow's cells as the client's pixels they fall in (each
// once, in the order they first fill), in OP_FLOWs of flowMax cells that
// follow one another over the spreading time and end together.
func (e *enc) flow(f int, ev sim.Ev) {
	seen := map[[2]int]bool{}
	var px [][2]int
	for _, c := range ev.Cells {
		x := int(math.Floor(e.c.X(float64(c.X) + 0.5)))
		y := int(math.Floor(e.c.Y(float64(c.Y) + 0.5)))
		k := [2]int{x, y}
		if !seen[k] && y >= 0 && y < e.c.FieldH {
			seen[k] = true
			px = append(px, k)
		}
	}
	if len(px) == 0 {
		return
	}
	spread := max(e.frame(ev.T+ev.Dur)-f, 1)
	end := f + spread + e.frame(ev.Hold)
	n := (len(px) + flowMax - 1) / flowMax
	for i := 0; i < n; i++ {
		a, b := i*len(px)/n, (i+1)*len(px)/n
		fa, fb := f+spread*i/n, f+spread*(i+1)/n
		hold := min(max(end-fb, 0), 255)
		if ev.Kind == proto.FlowMud {
			hold = 0 // mud is in as it lands: nothing to wait for
		}
		op := []byte{proto.OpFlow, byte(ev.Kind), byte(max(min(fb-fa, 255), 1)), byte(hold), byte(b - a)}
		for _, p := range px[a:b] {
			op = append(op, le16(p[0])...)
			op = append(op, byte(p[1]))
		}
		e.add(fa, op...)
	}
}

// settleOps: the client columns over span, as the replayed terrain has
// them now, to be reached in frames.
func (e *enc) settleOps(f int, t *world.Terrain, span world.Span, frames int) {
	// two columns more each side: a client's ellipse (a crater, dirt) is
	// whole pixels round its centre's, and may reach a column past the
	// world's disc
	cx0 := max(span.X0*e.c.FieldW/proto.WorldW-2, 0)
	cx1 := (span.X1*e.c.FieldW + proto.WorldW - 1) / proto.WorldW
	cx1 = min(cx1+2, e.c.FieldW)
	for x := cx0; x < cx1; {
		b := []byte{proto.OpSettle, byte(x), byte(x >> 8), 0, byte(min(frames, 255))}
		n := 0
		for x < cx1 && n < 255 && len(b) < 200 {
			b = append(b, Entry(e.c.Column(t, x))...)
			x++
			n++
		}
		b[3] = byte(n)
		e.add(f, b...)
	}
}

// Script encodes log for caps: the segments of the SCRIPT, each at most
// caps.Buf bytes and ending in OP_END. pre is the terrain before the shot
// (replayed through the log's settles for the columns).
func Script(log *sim.Log, pre *world.Terrain, c Caps) [][]byte {
	e := &enc{c: c}
	t := pre.Clone()
	evs := append([]sim.Ev(nil), log.Evs...)
	sort.SliceStable(evs, func(i, j int) bool { return evs[i].T < evs[j].T })
	for _, ev := range evs {
		f := e.frame(ev.T)
		frames := max(e.frame(ev.T+ev.Dur)-f, 0)
		switch ev.Op {
		case proto.OpArc:
			e.track(ev.Trk)
		case proto.OpExplode:
			rx := max(int(math.Round(c.X(ev.R))), 1)
			ry := max(int(math.Round(c.Y(ev.R))), 1)
			b := []byte{proto.OpExplode}
			b = append(b, le16(e.px(ev.X))...)
			b = append(b, le16(e.py(ev.Y))...)
			b = append(b, byte(min(rx, 255)), byte(min(ry, 255)), byte(ev.Kind), byte(max(min(frames, 255), 1)))
			e.add(f, b...)
		case proto.OpSettle:
			n := ev.Span.X1 - ev.Span.X0
			for i := 0; i < n; i++ {
				copy(t.Col(ev.Span.X0+i), ev.Cols[i*world.H:(i+1)*world.H])
			}
			e.settleOps(f, t, ev.Span, frames)
		case proto.OpTalk:
			b := []byte{proto.OpTalk, byte(ev.Seat), byte(len(ev.Text))}
			e.add(f, append(b, ev.Text...)...)
		case proto.OpSound:
			e.add(f, proto.OpSound, byte(ev.Kind), byte(ev.A))
		case proto.OpShield:
			e.add(f, proto.OpShield, byte(ev.Seat), byte(ev.Kind), byte(max(min(ev.A, 255), 0)))
		case proto.OpDamage:
			e.add(f, proto.OpDamage, byte(ev.Seat), byte(min(ev.A, 255)), byte(ev.B))
		case proto.OpFall:
			b := []byte{proto.OpFall, byte(ev.Seat)}
			b = append(b, le16(e.px(ev.X))...)
			b = append(b, byte(c.TankY(t, ev.X, ev.Y)), byte(max(min(frames, 255), 1)), byte(ev.A))
			e.add(f, b...)
		case proto.OpDeath:
			e.add(f, proto.OpDeath, byte(ev.Seat), byte(ev.Kind))
		case proto.OpFlash:
			e.add(f, proto.OpFlash, byte(ev.Kind), byte(max(min(frames, 255), 1)))
		case proto.OpFire:
			b := []byte{proto.OpFire, byte(ev.Seat), byte(ev.A)}
			b = append(b, le16(ev.B)...)
			e.add(f, append(b, byte(ev.Kind))...)
		case proto.OpWind:
			e.add(f, append([]byte{proto.OpWind}, le16(ev.A)...)...)
		case proto.OpShake:
			e.add(f, proto.OpShake, byte(max(min(frames, 255), 1)))
		case proto.OpDirt:
			rx := max(int(math.Round(c.X(ev.R))), 1)
			ry := max(int(math.Round(c.Y(ev.R))), 1)
			b := []byte{proto.OpDirt}
			b = append(b, le16(e.px(ev.X))...)
			b = append(b, le16(e.py(ev.Y))...)
			e.add(f, append(b, byte(min(rx, 255)), byte(min(ry, 255)), byte(max(min(frames, 255), 1)))...)
		case proto.OpBeam:
			b := []byte{proto.OpBeam}
			b = append(b, le16(e.px(ev.X))...)
			b = append(b, le16(e.py(ev.Y))...)
			b = append(b, le16(e.px(ev.X1))...)
			b = append(b, le16(e.py(ev.Y1))...)
			e.add(f, append(b, byte(ev.Kind), byte(max(min(frames, 255), 1)))...)
		case proto.OpFlow:
			e.flow(f, ev)
		}
	}
	return e.segments(e.frame(log.End))
}

// segments orders the ops, puts WAITs between frames, and cuts the stream
// into buffer-sized segments, each ending in OP_END.
func (e *enc) segments(end int) [][]byte {
	sort.SliceStable(e.ops, func(i, j int) bool {
		a, b := e.ops[i], e.ops[j]
		return a.f < b.f || a.f == b.f && a.seq < b.seq
	})
	var segs [][]byte
	var cur []byte
	room := e.c.Buf - 1
	put := func(b []byte) {
		if len(cur)+len(b) > room && len(cur) > 0 {
			segs = append(segs, append(cur, proto.OpEnd))
			cur = nil
		}
		cur = append(cur, b...)
	}
	f := 0
	wait := func(to int) {
		for to > f {
			n := min(to-f, 256)
			put([]byte{proto.OpWait, byte(n)}) // 256 is sent as 0
			f += n
		}
	}
	for _, o := range e.ops {
		wait(o.f)
		put(o.b)
	}
	wait(end)
	segs = append(segs, append(cur, proto.OpEnd))
	return segs
}

// ScriptFrames is the CS_SCRIPT payloads for one segment.
func ScriptFrames(sid, seg byte, last, hold bool, b []byte) [][]byte {
	var out [][]byte
	const head = 5
	for off := 0; off < len(b) || off == 0; {
		n := min(len(b)-off, proto.MaxPayload-head)
		p := []byte{sid, seg, 0, byte(off), byte(off >> 8)}
		p = append(p, b[off:off+n]...)
		off += n
		if off >= len(b) {
			p[2] |= proto.SLastFrame
			if last {
				p[2] |= proto.SLastSeg
			}
			if hold {
				p[2] |= proto.SHold
			}
		}
		out = append(out, p)
		if off >= len(b) {
			break
		}
	}
	return out
}
