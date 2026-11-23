// refview draws a client's view of a field and a few bot shots, at that
// client's resolution, from the very bytes the server would send it
// (TERRAIN and SCRIPT through internal/view). It is the reference every
// client is checked against.
//
//	refview [-w 160 -h 168 -hz 60 -par 109] [-seed 1] [-shots 4] [-weapon n] [-out dir]
//
// It writes OUT/field.png (the field and tanks before the first shot),
// OUT/shotN.gif (each shot, every frame) and OUT/shotN.png (its last frame),
// OUT/shotN.bin (the SCRIPT bytes) and OUT/field.bin (the TERRAIN frames).
package main

import (
	"flag"
	"fmt"
	"image"
	"image/color"
	"image/gif"
	"image/png"
	"log"
	"math"
	"math/rand/v2"
	"os"
	"path/filepath"

	"fujibarrage/internal/bot"
	"fujibarrage/internal/items"
	"fujibarrage/internal/project"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/sim"
	"fujibarrage/internal/view"
	"fujibarrage/internal/world"
)

func main() {
	w := flag.Int("w", 160, "field width, pixels")
	h := flag.Int("h", 168, "field height, pixels")
	hz := flag.Int("hz", 60, "frame rate")
	par := flag.Int("par", 109, "pixel aspect, 64ths")
	buf := flag.Int("buf", 3072, "script buffer")
	seed := flag.Uint64("seed", 1, "field seed")
	shots := flag.Int("shots", 4, "shots")
	weapon := flag.Int("weapon", -1, "weapon for every shot (-1: the bots')")
	walls := flag.Int("walls", proto.WallNone, "walls")
	out := flag.String("out", "refview", "output directory")
	flat := flag.Bool("flat", false, "a flat field (every tank on the same lines)")
	fixture := flag.String("fixture", "", "also write the field and the first shot as a C header (the clients' test scenes)")
	fixtureBas := flag.String("fixture-bas", "", "also write the field and the first shot as IntyBASIC DATA (the Intellivision's test scenes)")
	fixtureAsm := flag.String("fixture-asm", "", "also write the field and the first shot, every segment, as zmac DB lines (the Astrocade's test scenes)")
	sky := flag.Int("sky", proto.SkyGradient, "sky (the fixture's)")
	events := flag.Bool("events", false, "print each shot's events")
	angle := flag.Int("angle", -1, "the shot's angle (-1: the bot's)")
	power := flag.Int("power", -1, "and power")
	shield := flag.Int("shield", proto.INone, "a shield (proto I_*) on every other tank (the even seats)")
	tankW := flag.Int("tankw", 7, "the client's tank, pixels wide")
	tankH := flag.Int("tankh", 4, "and high")
	fixtureAll := flag.String("fixture-all", "", "also write the field and the first shot, every segment, as a C header with 16-bit tank x (the CoCo's test scenes)")
	flag.Parse()
	c := project.Caps{FieldW: *w, FieldH: *h, Hz: *hz, Par: *par, Buf: *buf, TankW: *tankW, TankH: *tankH}
	if err := c.Check(); err != nil {
		log.Fatal(err)
	}
	os.MkdirAll(*out, 0o755)

	rng := rand.New(rand.NewPCG(*seed, 99))
	wp := world.Params{Bumpiness: 20, Slope: 20}
	if *flat {
		wp = world.Params{}
	}
	s := &sim.Sim{T: world.Generate(wp, rng, true), Rng: rng,
		S: sim.Settings{Gravity: 0.2, Wind: float64(rng.IntN(60) - 30), Tunnel: true, TanksFall: true,
			ImpactDamage: true, Scale: 1, Borders: 150, Wall: *walls}}
	var order []int
	for i := 0; i < proto.Seats; i++ {
		s.Tanks[i] = &sim.Tank{Seat: i, Energy: 100, Alive: true, Shield: proto.INone, Angle: 90}
		if *shield != proto.INone && i%2 == 0 {
			s.Tanks[i].Shield, s.Tanks[i].ShieldHP = *shield, float64(items.All[*shield].HP)
		}
		order = append(order, i)
	}
	rng.Shuffle(len(order), func(i, j int) { order[i], order[j] = order[j], order[i] })
	s.Place(order)

	v := view.New(c)
	var tbin []byte
	for _, f := range project.TerrainFrames(nil, c.Entries(s.T), 0, 1) {
		tbin = append(tbin, f...)
		if err := v.Terrain(f); err != nil {
			log.Fatal(err)
		}
	}
	os.WriteFile(filepath.Join(*out, "field.bin"), tbin, 0o644)
	place := func() {
		for i, t := range s.Tanks {
			if t == nil {
				continue
			}
			v.Tanks[i] = view.Tank{On: t.Alive, X: c.IX(t.X), Y: c.TankY(s.T, t.X, t.Y), Angle: t.Angle, Energy: int(t.Energy)}
		}
	}
	place()
	tanks0 := v.Tanks
	r := newRenderer(c)
	writePNG(filepath.Join(*out, "field.png"), r.draw(v))

	inv := [proto.NItems]int{}
	for i := range inv {
		inv[i] = 9
	}
	turn := 0
	for n := 1; n <= *shots; n++ {
		var seat int
		for k := 0; k < len(order); k++ {
			seat = order[(turn+k)%len(order)]
			if s.Tanks[seat].Alive {
				turn += k + 1
				break
			}
		}
		mem := bot.Memory{Target: -1, RoundTier: proto.TierSniper}
		b := &bot.Seat{Seat: seat, Tier: proto.TierSniper, Inv: &inv, Mem: &mem}
		sh := bot.Aim(s, b, rng)
		if *weapon >= 0 {
			sh.Weapon = *weapon
		}
		if *angle >= 0 {
			sh.Angle = *angle
		}
		if *power >= 0 {
			sh.Power = *power
		}
		sh.Talk = "TAKE THAT!"
		pre := s.T.Clone()
		lg := s.Resolve([]sim.Shot{sh}, false)
		if *events {
			for _, e := range lg.Evs {
				fmt.Printf("  %6.3f op %02x seat %d x %.1f y %.1f r %.1f kind %d a %d b %d dur %.2f cells %d\n",
					e.T, e.Op, e.Seat, e.X, e.Y, e.R, e.Kind, e.A, e.B, e.Dur, len(e.Cells))
			}
		}
		segs := project.Script(lg, pre, c)
		var sbin []byte
		anim := &gif.GIF{}
		var last *image.Paletted
		f0 := v.Frame
		for _, seg := range segs {
			sbin = append(sbin, seg...)
			pl := v.Play(seg)
			for {
				more, err := pl.Step()
				if err != nil {
					log.Fatalf("shot %d: %v", n, err)
				}
				last = r.draw(v)
				if v.Frame%2 == 0 {
					anim.Image = append(anim.Image, last)
					anim.Delay = append(anim.Delay, 100*2 / *hz)
				}
				if !more {
					break
				}
			}
			v.EndSegment()
		}
		place()
		last = r.draw(v)
		anim.Image = append(anim.Image, last)
		anim.Delay = append(anim.Delay, 100)
		os.WriteFile(filepath.Join(*out, fmt.Sprintf("shot%d.bin", n)), sbin, 0o644)
		os.WriteFile(filepath.Join(*out, fmt.Sprintf("shot%d.px", n)), v.Px, 0o644) // the field after it, a byte a pixel
		if n == 1 && *fixtureAsm != "" {
			writeFixtureAsm(*fixtureAsm, c, pre, tanks0, segs, *sky)
		}
		if n == 1 && *fixtureBas != "" {
			writeFixtureBas(*fixtureBas, c, pre, tanks0, segs, *sky)
		}
		if n == 1 && *fixtureAll != "" {
			writeFixtureAll(*fixtureAll, c, pre, tanks0, segs, *sky)
		}
		if n == 1 && *fixture != "" {
			writeFixture(*fixture, c, pre, tanks0, segs, *sky)
		}
		writePNG(filepath.Join(*out, fmt.Sprintf("shot%d.png", n)), last)
		f, _ := os.Create(filepath.Join(*out, fmt.Sprintf("shot%d.gif", n)))
		gif.EncodeAll(f, anim)
		f.Close()
		fmt.Printf("shot %d: seat %d %s angle %d power %d: %d bytes in %d segment(s), %.1f s, %d frames\n",
			n, seat, itemName(sh.Weapon), sh.Angle, sh.Power, len(sbin), len(segs), lg.End, v.Frame-f0)
	}
}

func itemName(i int) string { return fmt.Sprint(i) }

func writePNG(path string, img image.Image) {
	f, err := os.Create(path)
	if err != nil {
		log.Fatal(err)
	}
	png.Encode(f, img)
	f.Close()
}

// renderer draws a view at a pixel aspect like the TV's.
type renderer struct {
	c      project.Caps
	sx, sy int
	pal    color.Palette
}

var seatCol = []color.RGBA{{230, 60, 60, 255}, {60, 140, 255, 255}, {80, 220, 80, 255}, {250, 220, 40, 255},
	{230, 80, 230, 255}, {60, 230, 230, 255}, {255, 150, 40, 255}, {200, 200, 200, 255},
	{150, 100, 255, 255}, {160, 255, 120, 255}}

func newRenderer(c project.Caps) *renderer {
	r := &renderer{c: c, sy: 2}
	r.sx = max(int(math.Round(float64(r.sy)*float64(max(c.Par, 1))/64)), 1)
	var pal color.Palette
	for i := 0; i < 32; i++ { // sky, dark blue to light
		f := float64(i) / 31
		pal = append(pal, color.RGBA{uint8(20 + 80*f), uint8(30 + 110*f), uint8(90 + 140*f), 255})
	}
	for i := 0; i < 32; i++ { // dirt, light to dark brown
		f := float64(i) / 31
		pal = append(pal, color.RGBA{uint8(170 - 100*f), uint8(120 - 80*f), uint8(60 - 40*f), 255})
	}
	pal = append(pal, color.RGBA{255, 200, 60, 255}) // 64 fire
	pal = append(pal, color.RGBA{0, 0, 0, 255})      // 65 black
	for _, s := range seatCol {
		pal = append(pal, s) // 66.. seats
	}
	pal = append(pal, color.RGBA{255, 255, 255, 255}) // 76 white
	r.pal = pal
	return r
}

func (r *renderer) draw(v *view.View) *image.Paletted {
	c := r.c
	img := image.NewPaletted(image.Rect(0, 0, c.FieldW*r.sx, c.FieldH*r.sy), r.pal)
	put := func(x, y int, ci uint8) {
		for dy := 0; dy < r.sy; dy++ {
			for dx := 0; dx < r.sx; dx++ {
				img.SetColorIndex(x*r.sx+dx, y*r.sy+dy, ci)
			}
		}
	}
	for y := 0; y < c.FieldH; y++ {
		band := uint8(y * 32 / c.FieldH)
		for x := 0; x < c.FieldW; x++ {
			var ci uint8
			switch v.At(x, y) {
			case view.PxSky:
				ci = band
			case view.PxDirt:
				ci = 32 + band
			case view.PxFx:
				ci = 64
			default:
				ci = 65
			}
			if v.Flash > 0 {
				ci = 76
			}
			put(x, y, ci)
		}
	}
	for i, t := range v.Tanks {
		if !t.On {
			continue
		}
		ci := uint8(66 + i)
		tw, th := c.TankW, c.TankH
		for yy := 0; yy < th; yy++ {
			inset := 0
			if yy == 0 {
				inset = 1
			}
			for xx := inset; xx < tw-inset; xx++ {
				put(t.X-tw/2+xx, t.Y-th+yy, ci)
			}
		}
		a := float64(t.Angle) * math.Pi / 180
		for k := 1; k <= 3; k++ {
			bx := t.X + int(math.Round(float64(k)*math.Cos(a)*1.0))
			by := t.Y - th - int(math.Round(float64(k)*math.Sin(a)*1.6))
			if bx >= 0 && by >= 0 && bx < c.FieldW && by < c.FieldH {
				put(bx, by, 65)
			}
		}
		if t.TalkLeft > 0 {
			drawText(img, r, t.X, t.Y-th-14, t.Talk)
		}
	}
	return img
}

func drawText(img *image.Paletted, r *renderer, cx, y int, s string) {
	w := len(s) * 4
	x0 := cx - w/2
	x0 = max(min(x0, r.c.FieldW-w), 0)
	for i, ch := range s {
		g := glyph(ch)
		for row := 0; row < 5; row++ {
			for col := 0; col < 3; col++ {
				if g[row]&(4>>col) != 0 {
					px, py := (x0+i*4+col)*r.sx, (y+row)*r.sy
					for dy := 0; dy < r.sy; dy++ {
						for dx := 0; dx < r.sx; dx++ {
							img.SetColorIndex(px+dx, py+dy, 76)
						}
					}
				}
			}
		}
	}
}

var font map[rune][5]uint8

// glyph is ch's 3x5 bitmap (gfx/font3x5.txt), bit 2 the left column.
func glyph(ch rune) [5]uint8 {
	if font == nil {
		font = map[rune][5]uint8{}
		data, err := os.ReadFile(*fontPath)
		if err == nil {
			var cur rune = -1
			row := 0
			for _, l := range splitLines(string(data)) {
				if len(l) > 5 && l[:5] == "char " {
					cur, row = []rune(l[5:])[0], 0
					continue
				}
				if cur < 0 || len(l) != 3 || l[0] == '#' && l == "# f" {
					continue
				}
				if row < 5 {
					g := font[cur]
					for i := 0; i < 3; i++ {
						if l[i] == '#' {
							g[row] |= 4 >> i
						}
					}
					font[cur] = g
					row++
				}
			}
		}
	}
	if g, ok := font[ch]; ok {
		return g
	}
	return [5]uint8{7, 5, 5, 5, 7}
}

func splitLines(s string) []string {
	var out []string
	start := 0
	for i := 0; i <= len(s); i++ {
		if i == len(s) || s[i] == '\n' {
			out = append(out, s[start:i])
			start = i + 1
		}
	}
	return out
}

var fontPath = flag.String("font", "../gfx/font3x5.txt", "the 3x5 font")

// writeFixture: the field before the shot, the tanks, the shot's first
// segment, as C arrays (gen/fixture.h): a client's offline test scene.
// writeFixtureBas: the field, the tanks and every segment of the first
// shot as IntyBASIC DATA for the Intellivision's test scenes. Bytes go two
// to a word (the first in the low byte), as the client's catalog does:
// fix_terrain's entries (FIX_NTERR bytes), the tanks' x, y and angle,
// FIX_NSEG segments' lengths, then the segments end to end.
func writeFixtureBas(path string, c project.Caps, pre *world.Terrain, tanks [proto.Seats]view.Tank, segs [][]byte, sky int) {
	f, err := os.Create(path)
	if err != nil {
		log.Fatal(err)
	}
	defer f.Close()
	words := func(name string, b []byte) {
		fmt.Fprintf(f, "%s:", name)
		for i := 0; i < len(b); i += 2 {
			w := int(b[i])
			if i+1 < len(b) {
				w |= int(b[i+1]) << 8
			}
			if i%32 == 0 {
				fmt.Fprint(f, "\n    DATA ")
			} else {
				fmt.Fprint(f, ",")
			}
			fmt.Fprintf(f, "$%04X", w)
		}
		fmt.Fprint(f, "\n")
	}
	vals := func(name string, v []int) {
		fmt.Fprintf(f, "%s:\n    DATA ", name)
		for i, x := range v {
			if i > 0 {
				fmt.Fprint(f, ",")
			}
			fmt.Fprintf(f, "%d", x)
		}
		fmt.Fprint(f, "\n")
	}
	fmt.Fprintf(f, "' GENERATED by refview -fixture-bas (%dx%d at %d Hz).\n", c.FieldW, c.FieldH, c.Hz)
	var terr, all []byte
	for _, e := range c.Entries(pre) {
		terr = append(terr, e...)
	}
	var lens []int
	for _, sg := range segs {
		lens = append(lens, len(sg))
		all = append(all, sg...)
	}
	fmt.Fprintf(f, "    CONST FIX_SKY = %d\n    CONST FIX_PAL = 0\n    CONST FIX_NTERR = %d\n    CONST FIX_NSEG = %d\n",
		sky, len(terr), len(segs))
	var tx, ty, ta []int
	for _, t := range tanks {
		tx, ty, ta = append(tx, t.X), append(ty, t.Y), append(ta, t.Angle)
	}
	vals("fix_tx", tx)
	vals("fix_ty", ty)
	vals("fix_ta", ta)
	vals("fix_seglen", lens)
	words("fix_terrain", terr)
	words("fix_seg", all)
}

// writeFixtureAsm writes the field before the shot and every segment of the
// shot as zmac source for the Astrocade's test scenes: FIXTERR's entries
// (FIXNTER bytes), the tanks' x, y and angle, FIXNSEG segments' lengths
// (FIXSLEN, words), then the segments end to end (FIXSEG).
func writeFixtureAsm(path string, c project.Caps, pre *world.Terrain, tanks [proto.Seats]view.Tank, segs [][]byte, sky int) {
	f, err := os.Create(path)
	if err != nil {
		log.Fatal(err)
	}
	defer f.Close()
	bytes := func(name string, b []int, dir string) {
		fmt.Fprintf(f, "%s:", name)
		for i, x := range b {
			if i%16 == 0 {
				fmt.Fprintf(f, "\n\t%s\t", dir)
			} else {
				fmt.Fprint(f, ",")
			}
			if dir == "DW" {
				fmt.Fprintf(f, "%d", x&0xFFFF)
			} else {
				fmt.Fprintf(f, "%d", x&0xFF)
			}
		}
		fmt.Fprint(f, "\n")
	}
	ints := func(b []byte) []int {
		v := make([]int, len(b))
		for i, x := range b {
			v[i] = int(x)
		}
		return v
	}
	fmt.Fprintf(f, "; GENERATED by refview -fixture-asm (%dx%d at %d Hz).\n", c.FieldW, c.FieldH, c.Hz)
	var terr, all []byte
	for _, e := range c.Entries(pre) {
		terr = append(terr, e...)
	}
	var lens []int
	for _, sg := range segs {
		lens = append(lens, len(sg))
		all = append(all, sg...)
	}
	fmt.Fprintf(f, "FIXSKY\tEQU\t%d\nFIXNTER\tEQU\t%d\nFIXNSEG\tEQU\t%d\n", sky, len(terr), len(segs))
	var tx, ty, ta []int
	for _, t := range tanks {
		tx, ty, ta = append(tx, t.X), append(ty, t.Y), append(ta, t.Angle)
	}
	bytes("FIXTX", tx, "DB")
	bytes("FIXTY", ty, "DB")
	bytes("FIXTA", ta, "DB")
	bytes("FIXSLEN", lens, "DW")
	bytes("FIXTERR", ints(terr), "DB")
	bytes("FIXSEG", ints(all), "DB")
}

func writeFixture(path string, c project.Caps, pre *world.Terrain, tanks [proto.Seats]view.Tank, segs [][]byte, sky int) {
	f, err := os.Create(path)
	if err != nil {
		log.Fatal(err)
	}
	defer f.Close()
	arr := func(name string, b []byte) {
		fmt.Fprintf(f, "static const uint8_t %s[%d] = {", name, len(b))
		for i, x := range b {
			if i%16 == 0 {
				fmt.Fprint(f, "\n   ")
			}
			fmt.Fprintf(f, " %d,", x)
		}
		fmt.Fprint(f, "\n};\n")
	}
	fmt.Fprintf(f, "/* GENERATED by refview -fixture (%dx%d at %d Hz). */\n", c.FieldW, c.FieldH, c.Hz)
	fmt.Fprintf(f, "#define FIX_SKY %d\n#define FIX_PAL %d\n", sky, 0)
	var terr []byte
	for _, e := range c.Entries(pre) {
		terr = append(terr, e...)
	}
	arr("fix_terrain", terr)
	var tx, ty, ta []byte
	for _, t := range tanks {
		tx = append(tx, byte(t.X))
		ty = append(ty, byte(t.Y))
		ta = append(ta, byte(t.Angle))
	}
	arr("fix_tx", tx)
	arr("fix_ty", ty)
	arr("fix_ta", ta)
	arr("fix_script", segs[0])
}

// writeFixtureAll: writeFixture's field and tanks, the tanks' x 16-bit (a
// field past 256 wide), and every segment of the first shot end to end
// (FIX_NSEG of them, fix_seglen[] long): a client that plays them in turn.
func writeFixtureAll(path string, c project.Caps, pre *world.Terrain, tanks [proto.Seats]view.Tank, segs [][]byte, sky int) {
	f, err := os.Create(path)
	if err != nil {
		log.Fatal(err)
	}
	defer f.Close()
	arr := func(typ, name string, v []int) {
		fmt.Fprintf(f, "static const %s %s[%d] = {", typ, name, len(v))
		for i, x := range v {
			if i%16 == 0 {
				fmt.Fprint(f, "\n   ")
			}
			fmt.Fprintf(f, " %d,", x)
		}
		fmt.Fprint(f, "\n};\n")
	}
	ints := func(b []byte) []int {
		v := make([]int, len(b))
		for i, x := range b {
			v[i] = int(x)
		}
		return v
	}
	fmt.Fprintf(f, "/* GENERATED by refview -fixture-all (%dx%d at %d Hz, tank %dx%d). */\n", c.FieldW, c.FieldH, c.Hz, c.TankW, c.TankH)
	fmt.Fprintf(f, "#define FIX_SKY %d\n#define FIX_PAL %d\n#define FIX_NSEG %d\n", sky, 0, len(segs))
	var terr, all []byte
	for _, e := range c.Entries(pre) {
		terr = append(terr, e...)
	}
	var lens []int
	for _, sg := range segs {
		lens = append(lens, len(sg))
		all = append(all, sg...)
	}
	var tx, ty, ta []int
	for _, t := range tanks {
		tx, ty, ta = append(tx, t.X), append(ty, t.Y), append(ta, t.Angle)
	}
	arr("uint8_t", "fix_terrain", ints(terr))
	arr("uint16_t", "fix_tx", tx)
	arr("uint8_t", "fix_ty", ty)
	arr("uint8_t", "fix_ta", ta)
	arr("uint16_t", "fix_seglen", lens)
	arr("uint8_t", "fix_seg", ints(all))
}
