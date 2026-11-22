// Package talk is what tanks say: a phrase as they fire, last words as they
// go up. The phrases are FujiBarrage's own (fire.txt, last.txt), one a
// line, at most proto.MaxTalk characters; a server operator can give
// others (Load).
package talk

import (
	_ "embed"
	"math/rand/v2"
	"os"
	"strings"

	"fujibarrage/internal/proto"
)

//go:embed fire.txt
var fireTxt string

//go:embed last.txt
var lastTxt string

// Book holds the two lists.
type Book struct {
	Fire, Last []string
}

// Default is the built-in book.
func Default() *Book { return &Book{Fire: parse(fireTxt), Last: parse(lastTxt)} }

// Load reads the lists from files ("" keeps the built-in one).
func Load(fire, last string) (*Book, error) {
	b := Default()
	for _, f := range []struct {
		path string
		to   *[]string
	}{{fire, &b.Fire}, {last, &b.Last}} {
		if f.path == "" {
			continue
		}
		data, err := os.ReadFile(f.path)
		if err != nil {
			return nil, err
		}
		if l := parse(string(data)); len(l) > 0 {
			*f.to = l
		}
	}
	return b, nil
}

func parse(s string) []string {
	var out []string
	for _, l := range strings.Split(s, "\n") {
		l = strings.ToUpper(strings.TrimSpace(l))
		if l == "" || strings.HasPrefix(l, "#") {
			continue
		}
		if len(l) > proto.MaxTalk {
			l = l[:proto.MaxTalk]
		}
		out = append(out, l)
	}
	return out
}

// Rules: the host's Talking Tanks (proto.Talk*) and Talk %.
type Rules struct {
	Mode, Pct int
}

// Shout is what seat says as it fires ("": nothing): bots talk unless the
// mode is Off, humans only when it is All; then the roll.
func (b *Book) Shout(r Rules, bot bool, rng *rand.Rand) string {
	return b.pick(b.Fire, r, bot, rng)
}

// LastWords is what a dying tank says ("": nothing).
func (b *Book) LastWords(r Rules, bot bool, rng *rand.Rand) string {
	return b.pick(b.Last, r, bot, rng)
}

func (b *Book) pick(l []string, r Rules, bot bool, rng *rand.Rand) string {
	if len(l) == 0 || r.Mode == proto.TalkOff || (!bot && r.Mode != proto.TalkAll) {
		return ""
	}
	if rng.IntN(100) >= r.Pct {
		return ""
	}
	return l[rng.IntN(len(l))]
}
