// barraged is the FujiBarrage server.
//
//	barraged [-stream :9913] [-seed n]
//	barraged -cheader > ../src/proto.h
//
// Lobby listing is off unless BARRAGE_LOBBY is qa, prod or a lobby URL;
// then BARRAGE_SERVER_URL (tcp://host:9913/) and BARRAGE_CLIENT_<PLATFORM>
// (BARRAGE_CLIENT_ATARI=tnfs://host/path/barrage.atr, ...) say what it
// lists. BARRAGE_TALK_FIRE / BARRAGE_TALK_LAST: phrase files of your own
// (one a line, at most 40 characters) instead of the built-in ones.
package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"
	"time"

	"fujibarrage/internal/game"
	"fujibarrage/internal/lobby"
	"fujibarrage/internal/proto"
	"fujibarrage/internal/server"
	"fujibarrage/internal/talk"
)

func env(k, def string) string {
	if v := os.Getenv(k); v != "" {
		return v
	}
	return def
}

// platforms the lobby can list a client image for (BARRAGE_CLIENT_<NAME>)
var platforms = []struct{ env, lobby string }{
	{"ATARI", "atari"}, {"APPLE2", "apple2"}, {"MSDOS", "msdos"}, {"INTV", "intv"}, {"ADAM", "adam"},
	{"O2", "o2"}, {"ARCADIA", "arcadia"}, {"ASTROCADE", "astrocade"}, {"CHANNELF", "channelf"},
	{"COLECO", "coleco"}, {"ATARI2600", "a2600"}, {"ATARI7800", "atari7800"}, {"ATARI5200", "atari5200"},
	{"COCO", "coco"}, {"NES", "nes"}, {"SMS", "sms"}, {"H89", "h89"}, {"MSX", "msx"}, {"STUDIO2", "studio2"},
}

func main() {
	addr := flag.String("stream", env("BARRAGE_STREAM_ADDR", ":9913"), "stream listen address")
	seed := flag.Uint64("seed", uint64(time.Now().UnixNano()), "field seed")
	cheader := flag.Bool("cheader", false, "write src/proto.h to stdout and exit")
	devset := flag.String("devset", os.Getenv("BARRAGE_DEVSET"), "dev rooms' settings: tanks=3,rounds=2,order=robin,mode=0,walls=1,sky=4,timer=10,arms=4,tier=0,kit=1")
	flag.Parse()
	if err := parseDevset(*devset); err != nil {
		log.Fatal(err)
	}
	if *cheader {
		proto.CHeader(os.Stdout)
		return
	}
	book, err := talk.Load(os.Getenv("BARRAGE_TALK_FIRE"), os.Getenv("BARRAGE_TALK_LAST"))
	if err != nil {
		log.Fatal(err)
	}
	s := server.New(*seed, book)
	stop := make(chan struct{})
	done := make(chan struct{})
	if lb := os.Getenv("BARRAGE_LOBBY"); lb != "" {
		ep := map[string]string{"qa": lobby.QA, "prod": lobby.Prod}[lb]
		if ep == "" {
			ep = lb
		}
		cfg := lobby.Config{Endpoint: ep, Game: "FujiBarrage", AppKey: 0x13,
			Region: env("BARRAGE_REGION", "us"), ServerURL: env("BARRAGE_SERVER_URL", "tcp://localhost:9913/")}
		for _, p := range platforms {
			if u := os.Getenv("BARRAGE_CLIENT_" + p.env); u != "" {
				cfg.Clients = append(cfg.Clients, lobby.Client{Platform: p.lobby, URL: u})
			}
		}
		pub := lobby.New(cfg)
		s.OnChange = func(ri server.RoomInfo) { pub.Notify(lobby.Table{ID: ri.ID, Name: ri.Name, Players: ri.Humans}) }
		var ts []lobby.Table
		for _, ri := range s.Rooms(false) {
			ts = append(ts, lobby.Table{ID: ri.ID, Name: ri.Name, Players: ri.Humans})
		}
		log.Printf("lobby: %s", cfg)
		go func() { pub.Run(ts, stop); close(done) }()
	} else {
		close(done)
	}
	if _, err := s.Listen(*addr); err != nil {
		log.Fatal(err)
	}
	log.Printf("rooms: %s", strings.Join(func() (n []string) {
		for _, r := range s.Rooms(false) {
			n = append(n, r.ID+" "+r.Name)
		}
		return
	}(), ", "))
	go s.Run(time.Second/60, stop)

	sig := make(chan os.Signal, 1)
	signal.Notify(sig, os.Interrupt, syscall.SIGTERM)
	<-sig
	close(stop)
	select {
	case <-done:
	case <-time.After(15 * time.Second):
	}
}

// parseDevset fills server.DevSettings from "name=value,...": tanks and
// rounds are counts, timer seconds, order a name (random, losers, winners,
// robin), the rest the setting's index.
func parseDevset(s string) error {
	if s == "" {
		return nil
	}
	keys := map[string]int{"tanks": proto.SetTanks, "rounds": proto.SetRounds, "order": proto.SetOrder,
		"mode": proto.SetMode, "walls": proto.SetWalls, "sky": proto.SetSky, "timer": proto.SetTimer,
		"talk": proto.SetTalkPct, "cash": proto.SetCash, "wind": proto.SetMaxWind, "scale": proto.SetScale,
		"arms": proto.SetArms}
	for _, kv := range strings.Split(s, ",") {
		k, v, ok := strings.Cut(kv, "=")
		if k == "kit" { // people start with a bundle of everything
			game.DevKit = v == "1"
			continue
		}
		if k == "tier" { // every bot this tier (0 Rookie ... 7 Wildcard)
			n, err := strconv.Atoi(v)
			if err != nil || n < 0 || n >= proto.NTiers {
				return fmt.Errorf("devset: %q", kv)
			}
			game.DevTier = n
			continue
		}
		key, known := keys[k]
		if !ok || !known {
			return fmt.Errorf("devset: %q", kv)
		}
		n, err := strconv.Atoi(v)
		switch {
		case k == "order":
			n, err = map[string]int{"random": proto.OrderRandom, "losers": proto.OrderLosers,
				"winners": proto.OrderWinners, "robin": proto.OrderRobin}[v], nil
		case k == "tanks":
			n -= 2
		case k == "rounds":
			n--
		case k == "timer":
			n = map[int]int{0: 0, 10: 1, 15: 2, 20: 3, 30: 4, 45: 5, 60: 6, 90: 7}[n]
		}
		if err != nil {
			return fmt.Errorf("devset: %q", kv)
		}
		server.DevSettings[key] = n
	}
	return nil
}
