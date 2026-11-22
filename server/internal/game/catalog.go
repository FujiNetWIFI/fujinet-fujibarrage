package game

import (
	"fmt"
	"math"

	"fujibarrage/internal/items"
	"fujibarrage/internal/proto"
)

// Pages of the host's settings.
var pages = []string{"FIELD", "WALLS", "GAME"}

// Spec describes a setting: an enum (Names) or a number (Min + i*Step, i
// below Count, Dec decimals). Values travel as the index i.
type Spec struct {
	Page  int
	Label string
	Names []string
	Min   int
	Step  int
	Count int
	Dec   int
	Def   int
}

func (s Spec) N() int {
	if s.Names != nil {
		return len(s.Names)
	}
	return s.Count
}

// Num is setting value index i as a number.
func (s Spec) Num(i int) float64 {
	return float64(s.Min+i*s.Step) / math.Pow10(s.Dec)
}

var offOn = []string{"OFF", "ON"}

// Specs is indexed by proto.Set*.
var Specs = [proto.NSettings]Spec{
	proto.SetSky:          {Page: 0, Label: "SKY", Names: []string{"PLAIN", "GRADIENT", "STARS", "STORM", "SUNSET", "CAVERN", "BLACK", "RANDOM"}, Def: proto.SkyRandom},
	proto.SetBumpiness:    {Page: 0, Label: "BUMPINESS", Min: 0, Step: 5, Count: 21, Def: 4},
	proto.SetSlope:        {Page: 0, Label: "SLOPES", Min: 0, Step: 5, Count: 21, Def: 4},
	proto.SetFlatten:      {Page: 0, Label: "FLATTEN PEAKS", Names: offOn},
	proto.SetRandomLand:   {Page: 0, Label: "RANDOM LAND", Names: offOn},
	proto.SetMaxWind:      {Page: 0, Label: "MAX WIND", Min: 0, Step: 25, Count: 21, Def: 8},
	proto.SetChangeWind:   {Page: 0, Label: "CHANGING WIND", Names: offOn},
	proto.SetGravity:      {Page: 0, Label: "GRAVITY", Min: 5, Step: 5, Count: 40, Dec: 2, Def: 3},
	proto.SetViscosity:    {Page: 0, Label: "AIR THICKNESS", Min: 0, Step: 1, Count: 21},
	proto.SetSuspend:      {Page: 0, Label: "DIRT STAYS UP %", Min: 0, Step: 10, Count: 11},
	proto.SetTanksFall:    {Page: 0, Label: "TANKS FALL", Names: offOn, Def: 1},
	proto.SetImpactDamage: {Page: 0, Label: "FALLS HURT", Names: offOn, Def: 1},
	proto.SetTunnel:       {Page: 0, Label: "TUNNELLING", Names: offOn, Def: 1},
	proto.SetScale:        {Page: 0, Label: "BLAST SIZE", Names: []string{"SMALL", "MEDIUM", "LARGE"}, Def: 2},
	proto.SetBorders:      {Page: 1, Label: "EDGES EXTEND", Min: 0, Step: 25, Count: 9, Def: 3},
	proto.SetWalls:        {Page: 1, Label: "WALLS", Names: []string{"NONE", "WRAP", "PADDED", "RUBBER", "SPRING", "CONCRETE", "RANDOM", "ERRATIC"}},
	proto.SetTanks:        {Page: 2, Label: "TANKS", Min: 2, Step: 1, Count: 9, Def: 8},
	proto.SetRounds:       {Page: 2, Label: "ROUNDS", Min: 1, Step: 1, Count: 100, Def: 9},
	proto.SetMode:         {Page: 2, Label: "PLAY", Names: []string{"TAKE TURNS", "ALL AT ONCE", "AIM TOGETHER"}},
	proto.SetOrder:        {Page: 2, Label: "TURN ORDER", Names: []string{"RANDOM", "LOSERS FIRST", "WINNERS FIRST", "ROUND ROBIN"}},
	proto.SetTimer:        {Page: 2, Label: "TURN TIMER", Names: []string{"OFF", "10 S", "15 S", "20 S", "30 S", "45 S", "60 S", "90 S"}, Def: 4},
	proto.SetTalk:         {Page: 2, Label: "TALKING TANKS", Names: []string{"OFF", "BOTS", "ALL"}, Def: proto.TalkAll},
	proto.SetTalkPct:      {Page: 2, Label: "TALK %", Min: 0, Step: 10, Count: 11, Def: 6},
	proto.SetArms:         {Page: 2, Label: "ARMS LEVEL", Min: 0, Step: 1, Count: 5, Def: 4},
	proto.SetCash:         {Page: 2, Label: "STARTING CASH", Min: 0, Step: 25000, Count: 41, Def: 8},
	proto.SetInterest:     {Page: 2, Label: "INTEREST %", Min: 0, Step: 5, Count: 7, Def: 2},
	proto.SetBotsBuy:      {Page: 2, Label: "BOTS SHOP", Names: offOn, Def: 1},
}

var timerSecs = []int{0, 10, 15, 20, 30, 45, 60, 90}

// TierNames are the bots' skill levels (proto.Tier*).
var TierNames = [proto.NTiers]string{"ROOKIE", "GUNNER", "RICOCHET", "LOBBER", "TACTICIAN", "SNIPER", "DEADEYE", "WILDCARD"}

func lstr(b []byte, s string) []byte { return append(append(b, byte(len(s))), s...) }

// Catalog is the CATALOG document every client gets at join
// (docs/PROTOCOL.md): the pages, the arsenal, the settings' forms, the
// bots' tier names.
func Catalog() []byte {
	b := []byte{1, proto.NItems, proto.NSettings, byte(len(pages))}
	for _, p := range pages {
		b = lstr(b, p)
	}
	for i, it := range items.All {
		kind := byte(0)
		if i >= proto.NWeapons {
			kind = 1
		}
		b = append(b, kind, byte(it.Arms), byte(it.Bundle), byte(it.Price), byte(it.Price>>8), byte(it.Price>>16))
		b = lstr(b, it.Name)
	}
	for _, s := range Specs {
		if s.Names != nil {
			b = append(b, byte(s.Page), 0)
			b = lstr(b, s.Label)
			b = append(b, byte(len(s.Names)))
			for _, n := range s.Names {
				b = lstr(b, n)
			}
			continue
		}
		b = append(b, byte(s.Page), 1)
		b = lstr(b, s.Label)
		b = append(b, byte(s.Min), byte(s.Min>>8), byte(s.Step), byte(s.Step>>8), byte(s.Count), byte(s.Dec))
	}
	b = append(b, proto.NTiers)
	for _, n := range TierNames {
		b = lstr(b, n)
	}
	return b
}

// SettingText is how a client shows setting k at value index i (tests,
// logs).
func SettingText(k, i int) string {
	s := Specs[k]
	if s.Names != nil {
		return s.Names[min(i, len(s.Names)-1)]
	}
	return fmt.Sprintf("%.*f", s.Dec, s.Num(i))
}
