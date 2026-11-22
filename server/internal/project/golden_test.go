package project_test

import (
	"crypto/sha256"
	"flag"
	"fmt"
	"image"
	"image/color"
	"image/png"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"fujibarrage/internal/items"
	. "fujibarrage/internal/project"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/sim"
	"fujibarrage/internal/view"
)

var update = flag.Bool("update", false, "rewrite testdata/golden.txt and the PNGs")

// golden shapes: the Atari, the Apple II, a TMS9918 at 50 Hz, the coarsest.
var goldenShapes = []Caps{shapes[0], shapes[2], shapes[3], shapes[4]}

var palette = color.Palette{
	color.RGBA{0x60, 0x90, 0xE0, 0xFF}, // sky
	color.RGBA{0x80, 0x58, 0x30, 0xFF}, // dirt
	color.RGBA{0xFF, 0xD0, 0x40, 0xFF}, // fx
	color.RGBA{0x00, 0x00, 0x00, 0xFF}, // tank
}

// Each weapon's shot, played at each golden shape: the script bytes and
// the field after it must not change unless meant to (go test -update, and
// look at testdata/golden/*.png).
func TestGolden(t *testing.T) {
	var got strings.Builder
	for w := 0; w < proto.NWeapons; w++ {
		s := field(uint64(500 + w))
		s.Tanks[5].Shield, s.Tanks[5].ShieldHP = proto.IShield, 100
		pre := s.T.Clone()
		lg := s.Resolve([]sim.Shot{{Seat: 3, Angle: 55 + w, Power: 300 + 7*w, Weapon: w, Guide: proto.INone, Target: proto.NoSeat}}, false)
		for _, c := range goldenShapes {
			v := view.New(c)
			for _, f := range TerrainFrames(nil, c.Entries(pre), 0, 1) {
				v.Terrain(f)
			}
			h := sha256.New()
			for _, seg := range Script(lg, pre, c) {
				h.Write(seg)
				pl := v.Play(seg)
				for {
					more, err := pl.Step()
					if err != nil {
						t.Fatal(err)
					}
					if !more {
						break
					}
				}
				v.EndSegment()
			}
			h.Write(v.Px)
			name := strings.ReplaceAll(strings.ToLower(items.All[w].Name), " ", "-")
			fmt.Fprintf(&got, "%-16s %-14s %x\n", name, c.Key(), h.Sum(nil)[:8])
			if *update {
				img := image.NewPaletted(image.Rect(0, 0, c.FieldW, c.FieldH), palette)
				copy(img.Pix, v.Px)
				dir := filepath.Join("testdata", "golden", fmt.Sprintf("%dx%d", c.FieldW, c.FieldH))
				os.MkdirAll(dir, 0o755)
				f, _ := os.Create(filepath.Join(dir, fmt.Sprintf("%02d-%s.png", w, name)))
				png.Encode(f, img)
				f.Close()
			}
		}
	}
	path := filepath.Join("testdata", "golden.txt")
	if *update {
		os.WriteFile(path, []byte(got.String()), 0o644)
		return
	}
	want, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("%v (go test -update makes it)", err)
	}
	if string(want) != got.String() {
		wl, gl := strings.Split(string(want), "\n"), strings.Split(got.String(), "\n")
		for i := range gl {
			if i >= len(wl) || wl[i] != gl[i] {
				t.Errorf("changed: %s", gl[i])
			}
		}
	}
}
