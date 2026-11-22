// Package items is FujiBarrage's arsenal: every weapon and misc item, its
// shop price and what it does (docs/OPS.md). Radii are in the reference
// game's units; the simulator doubles them to world units and applies the
// host's explosion scale.
package items

import "fujibarrage/internal/proto"

// Behaviours.
const (
	BBlast   = iota // explode on impact: R, Mult
	BHopper         // blasts R, R2, R3, relaunching between
	BCluster        // centre blast, bomblets, final blast
	BMIRV           // splits at the apex: N warheads of R
	BFlow           // napalm / inferno / mudslide: N hot spots
	BTracer         // no blast
	BRoller         // rolls downhill, then blasts R
	BShovel         // fan of dirt cleared from the barrel (R: reach)
	BBuster         // clears a disc of dirt (R), no damage
	BDrill          // bores a tunnel downward (R: length)
	BMole           // burrows toward an enemy (R: length), blasts R2
	BDirt           // fills a disc with dirt (R)
	BGeyser         // cone of dirt
	BQuake          // floating dirt settles (R: reach)
	BPlasma         // energy sphere R, no crater
	BLaser          // straight beam
	BItem           // not fired
)

// Item is one entry of the arsenal.
type Item struct {
	Name   string
	Price  int // a bundle
	Bundle int
	Arms   int // shown from this arms level on
	B      int // behaviour
	R      float64
	R2, R3 float64
	Mult   float64 // area damage factor
	N      int     // warheads, hot spots
	HP     int     // shields
}

// All is indexed by proto.I*.
var All = [proto.NItems]Item{
	proto.IMiniMissile:  {Name: "MINI MISSILE", Price: 400, Bundle: 10, B: BBlast, R: 10, Mult: 1},
	proto.IMissile:      {Name: "MISSILE", Price: 1875, Bundle: 5, B: BBlast, R: 20, Mult: 2},
	proto.IMiniNuke:     {Name: "MINI NUKE", Price: 10000, Bundle: 3, B: BBlast, R: 40, Mult: 3},
	proto.INuke:         {Name: "NUKE", Price: 12000, Bundle: 1, Arms: 1, B: BBlast, R: 75, Mult: 4},
	proto.IHopper:       {Name: "HOPPER", Price: 10000, Bundle: 2, Arms: 3, B: BHopper, R: 30, R2: 25, R3: 20, Mult: 2},
	proto.IClusterBomb:  {Name: "CLUSTER BOMB", Price: 7000, Bundle: 2, Arms: 4, B: BCluster, R: 20, R2: 15, R3: 40, Mult: 1, N: 80},
	proto.IMIRV:         {Name: "MIRV", Price: 10000, Bundle: 3, Arms: 2, B: BMIRV, R: 20, Mult: 2, N: 5},
	proto.IMegaMIRV:     {Name: "MEGA MIRV", Price: 20000, Bundle: 1, Arms: 4, B: BMIRV, R: 35, Mult: 3, N: 9},
	proto.INapalm:       {Name: "NAPALM", Price: 10000, Bundle: 10, Arms: 2, B: BFlow, R: 25, Mult: 30, N: 15},
	proto.IInferno:      {Name: "INFERNO", Price: 20000, Bundle: 2, Arms: 4, B: BFlow, R: 40, Mult: 50, N: 20},
	proto.ITracer:       {Name: "TRACER", Price: 10, Bundle: 20, B: BTracer},
	proto.ISmokeTracer:  {Name: "SMOKE TRACER", Price: 500, Bundle: 10, B: BTracer},
	proto.ILightRoller:  {Name: "LIGHT ROLLER", Price: 5000, Bundle: 10, Arms: 2, B: BRoller, R: 10, Mult: 1},
	proto.IRoller:       {Name: "ROLLER", Price: 6000, Bundle: 5, Arms: 2, B: BRoller, R: 20, Mult: 2},
	proto.IHeavyRoller:  {Name: "HEAVY ROLLER", Price: 6750, Bundle: 2, Arms: 3, B: BRoller, R: 45, Mult: 3},
	proto.IShovelCharge: {Name: "SHOVEL CHARGE", Price: 2000, Bundle: 10, Arms: 2, B: BShovel, R: 30},
	proto.IShovelBlast:  {Name: "SHOVEL BLAST", Price: 5000, Bundle: 5, Arms: 3, B: BShovel, R: 60},
	proto.IDirtBuster:   {Name: "DIRT BUSTER", Price: 5000, Bundle: 5, Arms: 3, B: BBuster, R: 30},
	proto.IHeavyBuster:  {Name: "HEAVY BUSTER", Price: 7500, Bundle: 2, Arms: 3, B: BBuster, R: 45},
	proto.ILightDrill:   {Name: "LIGHT DRILL", Price: 3000, Bundle: 10, B: BDrill, R: 10},
	proto.IDrill:        {Name: "DRILL", Price: 5000, Bundle: 5, B: BDrill, R: 20},
	proto.IHeavyDrill:   {Name: "HEAVY DRILL", Price: 6750, Bundle: 2, Arms: 1, B: BDrill, R: 35},
	proto.ILightMole:    {Name: "LIGHT MOLE", Price: 10000, Bundle: 10, B: BMole, R: 10, R2: 15, Mult: 2},
	proto.IMole:         {Name: "MOLE", Price: 16750, Bundle: 5, B: BMole, R: 20, R2: 20, Mult: 2},
	proto.IHeavyMole:    {Name: "HEAVY MOLE", Price: 25000, Bundle: 2, Arms: 1, B: BMole, R: 35, R2: 30, Mult: 3},
	proto.IDirtClump:    {Name: "DIRT CLUMP", Price: 5000, Bundle: 10, B: BDirt, R: 20},
	proto.IDirtHeap:     {Name: "DIRT HEAP", Price: 7500, Bundle: 5, B: BDirt, R: 35},
	proto.ILandslide:    {Name: "LANDSLIDE", Price: 6750, Bundle: 2, Arms: 1, B: BDirt, R: 70},
	proto.IMudslide:     {Name: "MUDSLIDE", Price: 5000, Bundle: 5, Arms: 2, B: BFlow, N: 20},
	proto.IDirtGeyser:   {Name: "DIRT GEYSER", Price: 5000, Bundle: 10, Arms: 1, B: BGeyser},
	proto.IQuake:        {Name: "QUAKE", Price: 5000, Bundle: 10, B: BQuake, R: 120},
	proto.IPlasmaBurst:  {Name: "PLASMA BURST", Price: 9000, Bundle: 5, Arms: 3, B: BPlasma, R: 45, Mult: 3},
	proto.ILaser:        {Name: "LASER", Price: 5000, Bundle: 5, Arms: 2, B: BLaser, R: 2, Mult: 40},

	proto.IHeatSeeker:      {Name: "HEAT SEEKER", Price: 10000, Bundle: 6, Arms: 2, B: BItem, R: 40},
	proto.IBallistic:       {Name: "BALLISTIC COMPUTER", Price: 10000, Bundle: 2, Arms: 2, B: BItem},
	proto.ILevelSeeker:     {Name: "LEVEL SEEKER", Price: 15000, Bundle: 5, Arms: 1, B: BItem},
	proto.IDropSeeker:      {Name: "DROP SEEKER", Price: 20000, Bundle: 5, Arms: 1, B: BItem},
	proto.IAutopilot:       {Name: "AUTOPILOT", Price: 20000, Bundle: 2, Arms: 3, B: BItem},
	proto.IFuel:            {Name: "FUEL", Price: 10000, Bundle: 20, Arms: 3, B: BItem}, // a unit: a step of 4
	proto.IParachute:       {Name: "PARACHUTE", Price: 10000, Bundle: 8, Arms: 2, B: BItem},
	proto.IBattery:         {Name: "BATTERY", Price: 5000, Bundle: 10, Arms: 2, B: BItem},
	proto.IDeflector:       {Name: "DEFLECTOR", Price: 10000, Bundle: 2, Arms: 2, B: BItem, HP: 55},
	proto.IShield:          {Name: "SHIELD", Price: 20000, Bundle: 3, Arms: 3, B: BItem, HP: 100},
	proto.IBounceShield:    {Name: "BOUNCE SHIELD", Price: 25000, Bundle: 3, Arms: 3, B: BItem, HP: 100},
	proto.IHeavyShield:     {Name: "HEAVY SHIELD", Price: 30000, Bundle: 2, Arms: 4, B: BItem, HP: 150},
	proto.ISuperDeflector:  {Name: "SUPER DEFLECTOR", Price: 40000, Bundle: 2, Arms: 4, B: BItem, HP: 200},
	proto.IDefenseComputer: {Name: "DEFENSE COMPUTER", Price: 1500, Bundle: 1, Arms: 3, B: BItem},
	proto.IImpactFuse:      {Name: "IMPACT FUSE", Price: 1000, Bundle: 25, Arms: 3, B: BItem},
}

// IsShield reports whether item i is raised as a shield.
func IsShield(i int) bool {
	switch i {
	case proto.IDeflector, proto.IShield, proto.IBounceShield, proto.IHeavyShield, proto.ISuperDeflector:
		return true
	}
	return false
}

// IsDeflector reports whether shield i also lifts shells passing over.
func IsDeflector(i int) bool { return i == proto.IDeflector || i == proto.ISuperDeflector }

// IsGuidance reports whether item i steers a shot (it needs a target).
func IsGuidance(i int) bool {
	switch i {
	case proto.IHeatSeeker, proto.IBallistic, proto.ILevelSeeker, proto.IDropSeeker, proto.IAutopilot:
		return true
	}
	return false
}

// Unlimited is the weapon every tank always has.
const Unlimited = proto.IMiniMissile

// Most is the most of one item a tank can hold.
const Most = 99
