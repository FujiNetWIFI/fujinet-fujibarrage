package project

import (
	"math"
	"testing"
)

// The ARC fitter's integer path stays within tolerance of a true parabola
// with drag, and needs few arcs for it.
func TestFitArcs(t *testing.T) {
	var pts []fpt
	x, y, vx, vy := 10.0, 150.0, 2.5, -4.0
	for i := 0; i < 200; i++ {
		pts = append(pts, fpt{x: x, y: y})
		x += vx
		y += vy
		vx *= 0.995
		vy = vy*0.995 + 0.07
	}
	segs := fitArcs(pts)
	worst := 0.0
	for _, s := range segs {
		a := s.a
		for k := 1; k <= s.n; k++ {
			a.Step()
			p := pts[s.k0+k]
			worst = math.Max(worst, math.Max(math.Abs(float64(a.X)/fixOne-p.x), math.Abs(float64(a.Y)/fixOne-p.y)))
		}
	}
	if worst > arcTol {
		t.Errorf("worst error %.2f px", worst)
	}
	if len(segs) > 12 {
		t.Errorf("%d arcs for 200 frames", len(segs))
	}
	t.Logf("%d arcs, worst %.2f px", len(segs), worst)
}
