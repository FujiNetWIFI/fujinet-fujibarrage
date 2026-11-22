// Package world is the field every shot happens in: a 640x400 grid of
// square units, solid (dirt) or air. Only the server has it; each client
// gets its own downsampled copy (internal/project).
package world

import (
	"math"
	"math/rand/v2"

	"fujibarrage/internal/proto"
)

const (
	W = proto.WorldW
	H = proto.WorldH
)

// Terrain is the field's dirt, column major: Cells[x*H+y], 1 solid.
type Terrain struct {
	Cells []uint8
}

func New() *Terrain { return &Terrain{Cells: make([]uint8, W*H)} }

func (t *Terrain) Clone() *Terrain {
	return &Terrain{Cells: append([]uint8(nil), t.Cells...)}
}

// At reports dirt at (x, y). Off the field: below the bottom is solid,
// above the top and beside it is air.
func (t *Terrain) At(x, y int) bool {
	if y >= H {
		return true
	}
	if x < 0 || x >= W || y < 0 {
		return false
	}
	return t.Cells[x*H+y] != 0
}

func (t *Terrain) Set(x, y int, solid bool) {
	if x < 0 || x >= W || y < 0 || y >= H {
		return
	}
	v := uint8(0)
	if solid {
		v = 1
	}
	t.Cells[x*H+y] = v
}

// Col is column x (H cells), shared: don't keep it.
func (t *Terrain) Col(x int) []uint8 { return t.Cells[x*H : (x+1)*H] }

// Surface is the first solid row in column x from the top, or H.
func (t *Terrain) Surface(x int) int {
	if x < 0 || x >= W {
		return H
	}
	c := t.Col(x)
	for y := 0; y < H; y++ {
		if c[y] != 0 {
			return y
		}
	}
	return H
}

// SurfaceBelow is the first solid row in column x at or below y, or H.
func (t *Terrain) SurfaceBelow(x, y int) int {
	if x < 0 || x >= W {
		return H
	}
	if y < 0 {
		y = 0
	}
	c := t.Col(x)
	for ; y < H; y++ {
		if c[y] != 0 {
			return y
		}
	}
	return H
}

// Span is a dirty x range [X0, X1) that an edit touched.
type Span struct{ X0, X1 int }

func (s Span) Empty() bool { return s.X1 <= s.X0 }

func (s Span) Union(o Span) Span {
	if s.Empty() {
		return o
	}
	if o.Empty() {
		return s
	}
	return Span{min(s.X0, o.X0), max(s.X1, o.X1)}
}

func clampSpan(x0, x1 int) Span { return Span{max(x0, 0), min(x1, W)} }

// Disc calls f for each cell of the disc at (cx, cy), radius r.
func disc(cx, cy, r float64, f func(x, y int)) Span {
	x0, x1 := int(math.Floor(cx-r)), int(math.Ceil(cx+r))
	y0, y1 := int(math.Floor(cy-r)), int(math.Ceil(cy+r))
	r2 := r * r
	for x := max(x0, 0); x <= min(x1, W-1); x++ {
		dx := float64(x) + 0.5 - cx
		for y := max(y0, 0); y <= min(y1, H-1); y++ {
			dy := float64(y) + 0.5 - cy
			if dx*dx+dy*dy <= r2 {
				f(x, y)
			}
		}
	}
	return clampSpan(x0, x1+1)
}

// Cut makes the disc air (a crater).
func (t *Terrain) Cut(cx, cy, r float64) Span {
	return disc(cx, cy, r, func(x, y int) { t.Cells[x*H+y] = 0 })
}

// Fill makes the disc's air dirt.
func (t *Terrain) Fill(cx, cy, r float64) Span {
	return disc(cx, cy, r, func(x, y int) { t.Cells[x*H+y] = 1 })
}

// CutLine cuts a channel of half-width hw from (x0, y0) to (x1, y1).
func (t *Terrain) CutLine(x0, y0, x1, y1, hw float64) Span {
	d := math.Hypot(x1-x0, y1-y0)
	n := int(d) + 1
	s := Span{}
	for i := 0; i <= n; i++ {
		f := float64(i) / float64(n)
		s = s.Union(t.Cut(x0+(x1-x0)*f, y0+(y1-y0)*f, hw))
	}
	return s
}

// Floating reports whether any dirt in s hangs over air.
func (t *Terrain) Floating(s Span) bool {
	for x := s.X0; x < s.X1; x++ {
		c := t.Col(x)
		air := false
		for y := H - 1; y >= 0; y-- {
			if c[y] == 0 {
				air = true
			} else if air {
				return true
			}
		}
	}
	return false
}

// Settle lets every column in s fall: its dirt drops to the bottom, gaps
// close. It returns the largest drop, in units (0: nothing moved).
func (t *Terrain) Settle(s Span) int {
	most := 0
	for x := s.X0; x < s.X1; x++ {
		c := t.Col(x)
		w := H - 1 // the next row dirt lands on, from the bottom up
		for y := H - 1; y >= 0; y-- {
			if c[y] == 0 {
				continue
			}
			if y != w {
				c[w], c[y] = 1, 0
				if d := w - y; d > most {
					most = d
				}
			}
			w--
		}
	}
	return most
}

// Params shape a generated field (the host's Field settings, decoded).
type Params struct {
	Bumpiness int // 0-100: % of columns where the slope changes
	Slope     int // 0-100: % of those changes that are doubled
	Flatten   bool
	Cavern    bool
}

// Generate makes a field: a random walk with momentum across the columns
// (in 2-unit steps), from a random height, the direction alternating by
// round; a cavern sky adds a ceiling the same way.
func Generate(p Params, rng *rand.Rand, leftToRight bool) *Terrain {
	t := New()
	floor := walk(p, rng, leftToRight, 140, H-24)
	for x := 0; x < W; x++ {
		for y := floor[x]; y < H; y++ {
			t.Cells[x*H+y] = 1
		}
	}
	if p.Cavern {
		ceil := walk(p, rng, !leftToRight, 24, 110) // dirt hanging from the top
		for x := 0; x < W; x++ {
			top := ceil[x]
			if floor[x]-top < 120 {
				top = max(floor[x]-120, 0)
			}
			for y := 0; y < top; y++ {
				t.Cells[x*H+y] = 1
			}
		}
	}
	return t
}

func walk(p Params, rng *rand.Rand, ltr bool, lo, hi int) []int {
	out := make([]int, W)
	y := float64(lo + rng.IntN(hi-lo))
	m := 0.0
	const step = 2
	for i := 0; i < W; i += step {
		if rng.IntN(100) < p.Bumpiness {
			d := float64(rng.IntN(3) - 1)
			if rng.IntN(100) < p.Slope {
				d *= 2
			}
			m += d
		}
		if p.Flatten && math.Abs(m) > 3 {
			m = 0
		}
		y += m
		if y < float64(lo) {
			y = float64(lo)
			if m < 0 {
				m = 1
			}
		}
		if y > float64(hi) {
			y = float64(hi)
			if m > 0 {
				m = 0
			}
		}
		for k := 0; k < step && i+k < W; k++ {
			x := i + k
			if !ltr {
				x = W - 1 - x
			}
			out[x] = int(y)
		}
	}
	return out
}
