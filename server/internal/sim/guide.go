package sim

import (
	"math"

	"fujibarrage/internal/proto"
)

// steer applies a shell's guidance, once per step, before the forces.
//
//   - Heat Seeker: the first enemy within reach locks it; it is pulled to
//     that point (more the further) until it passes it.
//   - Level Seeker: at the target's height it turns level and flies at it.
//   - Drop Seeker: over the target's column it drops straight down.
//   - Autopilot: straight at the target from the barrel, no gravity or
//     wind, boring through dirt.
//   - Ballistic Computer: nothing in flight (the power was solved at fire).
func (s *Sim) steer(p *shell) {
	if p.guide == proto.INone || p.guide == proto.IBallistic {
		return
	}
	var tx, ty float64
	if p.target >= 0 && p.target < proto.Seats && s.Tanks[p.target] != nil {
		tx, ty = s.Tanks[p.target].Centre()
	}
	switch p.guide {
	case proto.IHeatSeeker:
		if !p.homed {
			for _, t := range s.Tanks {
				if t == nil || !t.Alive || t.Seat == p.seat {
					continue
				}
				cx, cy := t.Centre()
				if math.Hypot(s.dx(cx, p.x), cy-p.y) < 80 {
					p.homed, p.hx, p.hy = true, cx, cy
					p.hsx, p.hsy = sign(p.hx-p.x), sign(p.hy-p.y)
					break
				}
			}
		}
		if p.homed {
			dx, dy := s.dx(p.hx, p.x), p.hy-p.y
			if sign(dx) != p.hsx || sign(dy) != p.hsy { // passed it
				p.guide = proto.INone
				return
			}
			d := math.Hypot(dx, dy)
			if d > 0.5 {
				k := 20000 * DT * math.Sqrt(d) / d
				p.vx += dx * k
				p.vy += dy * k
			}
		}
	case proto.ILevelSeeker:
		if !p.guided && (p.y-ty)*(p.y-p.vy*DT-ty) <= 0 && p.vy > 0 {
			p.guided = true
			sp := math.Max(math.Hypot(p.vx, p.vy), 400)
			p.vx, p.vy = math.Copysign(sp, s.dx(tx, p.x)), 0
		}
	case proto.IDropSeeker:
		if !p.guided && (p.x-tx)*(p.x-p.vx*DT-tx) <= 0 {
			p.guided = true
			p.vx, p.vy = 0, math.Max(math.Hypot(p.vx, p.vy), 400)
		}
	case proto.IAutopilot:
		p.guided = true
		dx, dy := s.dx(tx, p.x), ty-p.y
		d := math.Hypot(dx, dy)
		if d < 4 {
			p.guide = proto.INone
			s.impactAt(p, p.target)
			return
		}
		p.vx, p.vy = dx/d*1000, dy/d*1000
		p.digs = true
	}
}

func sign(v float64) int {
	switch {
	case v > 0:
		return 1
	case v < 0:
		return -1
	}
	return 0
}
