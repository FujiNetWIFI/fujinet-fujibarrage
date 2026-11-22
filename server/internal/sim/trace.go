package sim

// Hit is where a traced shot ends.
type Hit struct {
	X, Y float64
	Seat int  // the tank whose body or shield it hit, or -1
	Lost bool // it left the field
	T    float64
}

// Trace flies one shot quietly: no log, no dirt cut, no blast; it stops at
// the first thing it meets. The bots aim with it.
func (s *Sim) Trace(sh Shot) Hit {
	saved := s.log
	s.log = &Log{}
	s.quiet = true
	s.now = 0
	s.shells = nil
	s.pending = nil
	s.hit = Hit{Seat: -1, Lost: true}
	for _, t := range s.Tanks {
		if t != nil {
			t.lift = 1000
		}
	}
	s.fire(sh)
	for len(s.shells) > 0 && s.now < 20 {
		s.now += DT
		live := s.shells[:0]
		for _, p := range s.shells {
			s.step(p)
			if !p.dead {
				live = append(live, p)
			}
		}
		s.shells = live
		s.adopt()
	}
	s.shells = nil
	s.spawned = nil
	s.pending = nil
	s.quiet = false
	s.log = saved
	h := s.hit
	h.T = s.now
	return h
}
