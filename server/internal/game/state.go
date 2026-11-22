package game

import (
	"math"

	"fujibarrage/internal/items"
	"fujibarrage/internal/project"
	"fujibarrage/internal/proto"
)

// Viewer is the client a STATE is for.
type Viewer struct {
	Caps   project.Caps
	Seat   int  // its seat, or -1
	Played byte // the last shot it finished watching
	CatLen int
	CatSum [2]byte
}

// Gated: the client hasn't watched the current shot yet, so its STATE shows
// the seats as they were before it (the script tells what happened).
func (g *Game) Gated(v Viewer) bool {
	return g.Shot != nil && v.Played != g.Shot.SID
}

// State is the STATE for v (docs/PROTOCOL.md, proto.Off*).
func (g *Game) State(v Viewer) []byte {
	st := make([]byte, proto.StateSize)
	gated := g.Gated(v)
	st[proto.OffPhase] = byte(g.Phase)
	if gated {
		st[proto.OffPhase] = proto.PhShot
	}
	st[proto.OffRound] = byte(g.Round)
	st[proto.OffRounds] = byte(g.Rounds)
	st[proto.OffHost] = seatByte(g.Host)
	st[proto.OffTurn] = seatByte(g.Turn)
	if g.Phase != proto.PhWait {
		left := (g.DeadJ - g.J + Hz - 1) / Hz
		st[proto.OffTimer] = byte(max(min(left, 255), 0))
	}
	st[proto.OffMode] = byte(g.Set[proto.SetMode])
	st[proto.OffTanks] = byte(g.count())
	w := int(math.Round(g.Wind))
	st[proto.OffWind], st[proto.OffWind+1] = byte(w), byte(w>>8)
	st[proto.OffWall] = byte(g.Wall)
	st[proto.OffSky] = byte(g.Sky)
	st[proto.OffTV] = g.TV
	if gated {
		st[proto.OffTV] = g.Shot.PreTV
	}
	st[proto.OffSID] = g.SID
	st[proto.OffEpoch] = g.Epoch
	st[proto.OffSeq] = g.Seq
	st[proto.OffSpect] = byte(min(g.Spect, 255))
	st[proto.OffResult] = seatByte(g.Result)
	st[proto.OffMySeat] = seatByte(v.Seat)
	st[proto.OffPalette] = byte(g.Pal)
	st[proto.OffCatLen], st[proto.OffCatLen+1] = byte(v.CatLen), byte(v.CatLen>>8)
	st[proto.OffCatSum], st[proto.OffCatSum+1] = v.CatSum[0], v.CatSum[1]
	for k := 0; k < proto.NSettings; k++ {
		st[proto.OffSettings+k] = byte(g.Set[k])
	}
	terr := g.Sim
	for i := range g.P {
		p := &g.P[i]
		o := proto.OffSeats + i*proto.SeatSize
		r := st[o : o+proto.SeatSize]
		copy(r[proto.PName:proto.PName+proto.NameLen], p.Name)
		r[proto.PColour] = byte(i)
		r[proto.PKind] = byte(p.Kind)
		r[proto.PTier] = byte(p.Tier)
		r[proto.PAngle] = byte(p.Angle)
		r[proto.PPower], r[proto.PPower+1] = byte(p.Power), byte(p.Power>>8)
		r[proto.PWeapon] = byte(p.Weapon)
		r[proto.PWins] = byte(min(p.Wins, 255))
		sc := max(p.Score/10, 0)
		r[proto.PScore], r[proto.PScore+1] = byte(sc), byte(sc>>8)
		fl := byte(0)
		if p.Conn {
			fl |= proto.PFConn
		}
		if p.Chute {
			fl |= proto.PFChute
		}
		if p.Fuse {
			fl |= proto.PFFuse
		}
		if p.Done {
			fl |= proto.PFDone
		}
		if p.Inv[proto.IDefenseComputer] > 0 {
			fl |= proto.PFDefense
		}
		r[proto.PShield] = proto.INone
		if terr != nil && terr.Tanks[i] != nil {
			t := terr.Tanks[i]
			alive, x, y, energy, shp, sh, ang := t.Alive, t.X, t.Y, t.Energy, t.ShieldHP, t.Shield, p.Angle
			tt := terr.T
			if gated {
				b := g.Shot.before[i]
				alive, x, y, energy, shp, sh, ang = b.alive, b.x, b.y, b.energy, b.shield, b.shieldItem, b.angle
				tt = g.Shot.Pre
			}
			if alive {
				fl |= proto.PFAlive
			}
			r[proto.PEnergy] = byte(max(math.Ceil(energy), 0))
			r[proto.PShield] = byte(sh)
			r[proto.PShieldHP] = byte(max(min(math.Ceil(shp), 255), 0))
			r[proto.PAngle] = byte(ang)
			cx := int(math.Floor(v.Caps.X(x)))
			r[proto.PX], r[proto.PX+1] = byte(cx), byte(cx>>8)
			r[proto.PY] = byte(v.Caps.TankY(tt, x, y))
		}
		r[proto.PFlags] = fl
	}
	if v.Seat >= 0 && v.Seat < proto.Seats {
		p := &g.P[v.Seat]
		c := uint32(max(p.Cash, 0))
		st[proto.OffCash], st[proto.OffCash+1], st[proto.OffCash+2], st[proto.OffCash+3] = byte(c), byte(c>>8), byte(c>>16), byte(c>>24)
		st[proto.OffFuel], st[proto.OffFuel+1] = byte(p.Inv[proto.IFuel]), 0
		st[proto.OffGuide] = byte(p.Guide)
		st[proto.OffTarget] = seatByte(p.Target)
		for i := 0; i < proto.NItems; i++ {
			st[proto.OffInv+i] = byte(min(p.Inv[i], 99))
		}
		st[proto.OffInv+items.Unlimited] = 99
	} else {
		st[proto.OffGuide] = proto.INone
		st[proto.OffTarget] = proto.NoSeat
	}
	return st
}

func seatByte(s int) byte {
	if s < 0 || s >= proto.Seats {
		return proto.NoSeat
	}
	return byte(s)
}
