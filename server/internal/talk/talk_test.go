package talk

import (
	"math/rand/v2"
	"testing"

	"fujibarrage/internal/proto"
)

// Off: nobody talks; Bots: only bots; All: everyone; about Pct% of shots.
func TestRules(t *testing.T) {
	b := Default()
	rng := rand.New(rand.NewPCG(1, 2))
	count := func(r Rules, bot bool) int {
		n := 0
		for i := 0; i < 1000; i++ {
			if s := b.Shout(r, bot, rng); s != "" {
				n++
				if len(s) > proto.MaxTalk {
					t.Fatalf("%q: longer than %d", s, proto.MaxTalk)
				}
			}
		}
		return n
	}
	if count(Rules{proto.TalkOff, 100}, true) != 0 {
		t.Error("talked with talk off")
	}
	if count(Rules{proto.TalkBots, 100}, false) != 0 {
		t.Error("a person talked with bots only")
	}
	if count(Rules{proto.TalkBots, 100}, true) != 1000 {
		t.Error("a bot kept quiet at 100%")
	}
	if n := count(Rules{proto.TalkAll, 60}, false); n < 540 || n > 660 {
		t.Errorf("%d of 1000 at 60%%", n)
	}
	for _, l := range append(b.Fire, b.Last...) {
		if len(l) > proto.MaxTalk {
			t.Errorf("%q: longer than %d", l, proto.MaxTalk)
		}
	}
}
