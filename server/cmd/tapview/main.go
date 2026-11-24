// Command tapview replays a tools/rigs/tapproxy.py log (the frames a server
// sent a client, a line each: type letter, payload in hex) into a view of
// the field as a platform's client keeps it, and writes the pixels it ends
// with (the shots played whole): the reference a live client's field is
// checked against (tools/h89sim/shotcheck.py).
//
//	tapview -plat 12 LOG OUT.bin
//
// OUT.bin is W*H bytes, view.Px* values (0 sky, 1 dirt, 2 fire, 3 tank).
package main

import (
	"bufio"
	"encoding/hex"
	"flag"
	"fmt"
	"os"
	"strings"

	"fujibarrage/internal/client"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/view"
)

func main() {
	plat := flag.Int("plat", proto.PlatH89, "the client's platform (proto PLAT_*)")
	flag.Parse()
	if flag.NArg() != 2 {
		fmt.Fprintln(os.Stderr, "usage: tapview [-plat N] LOG OUT.bin")
		os.Exit(2)
	}
	caps := client.H89
	switch *plat {
	case proto.PlatH89:
	default:
		fmt.Fprintln(os.Stderr, "tapview: only the H89's caps are known here")
		os.Exit(2)
	}
	f, err := os.Open(flag.Arg(0))
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	defer f.Close()
	c := &client.Client{Caps: caps}
	c.V = view.New(caps)
	sc := bufio.NewScanner(f)
	sc.Buffer(make([]byte, 1<<20), 1<<20)
	nseg := 0
	for sc.Scan() {
		p := strings.Fields(sc.Text())
		if len(p) < 1 || len(p[0]) != 1 {
			continue
		}
		var payload []byte
		if len(p) > 1 {
			if payload, err = hex.DecodeString(p[1]); err != nil {
				continue
			}
		}
		c.Feed(p[0][0], payload)
		nseg += len(c.Ready)
		if err := c.PlayReady(); err != nil {
			fmt.Fprintln(os.Stderr, "tapview: play:", err)
			os.Exit(1)
		}
	}
	if err := os.WriteFile(flag.Arg(1), c.V.Px, 0o644); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Printf("tapview: %d segments played, %dx%d\n", nseg, caps.FieldW, caps.FieldH)
}
