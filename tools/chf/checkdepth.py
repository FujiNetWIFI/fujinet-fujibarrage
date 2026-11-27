#!/usr/bin/env python3
"""checkdepth.py -- the K-stack's depth, from the sources.

The F8 keeps one return address; a routine that calls anything saves it on
the BIOS's K-stack (ENTER: two scratch registers from r16), 21 levels
(equ.inc KDEPTH) before it runs into r59, its own pointer. A deeper chain
corrupts the pointer and the returns go astray (M.U.L.E. met it), so this
fails the build when any chain of ENTERs is deeper than --max.

Routines are the labels PI or JMP reach (and the pages' entries: PAGEHDR
and the JMP lists after it; GOPAGE and FAR reach PGWIN + E_*). A routine's
code runs to the next routine's label. Its depth is 1 if it ENTERs, plus
its deepest callee (PI), or its tail's (JMP to a routine).

usage: checkdepth.py [--max N] [--show] FILE...
"""
import re
import sys

MACRO_CALLS = {"SND": "SNDPLAY"}


def main():
    args = sys.argv[1:]
    mx, show = 21, False
    if args and args[0] == "--max":
        mx, args = int(args[1]), args[2:]
    if args and args[0] == "--show":
        show, args = True, args[1:]
    lines = []
    for f in args:
        for n, raw in enumerate(open(f, encoding="latin-1"), 1):
            code = raw.split(";")[0].rstrip()
            lines.append((f, n, code))
    # page entries: PAGEHDR id, e1..e4 then JMP x lines (E_EXTRA on)
    pages = {}
    pg_of = {}
    cur = None
    eq = {}
    for f, n, code in lines:
        m = re.match(r"^([A-Za-z_]\w*)\s+EQU\s+(\S+)", code, re.I)
        if m:
            try:
                eq[m.group(1).upper()] = int(m.group(2).rstrip("Hh"), 16 if m.group(2)[-1] in "Hh" else 10)
            except ValueError:
                pass
    for f, n, code in lines:
        m = re.search(r"PAGEHDR\s+(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)", code, re.I)
        if m:
            cur = eq.get(m.group(1).upper())
            pages[cur] = {2: m.group(2).upper(), 5: m.group(3).upper(), 8: m.group(4).upper(), 11: m.group(5).upper()}
            off = 14
            continue
        if cur is not None:
            w = code.split()
            if len(w) == 2 and w[0].upper() == "JMP" and not re.match(r"^\w+:", code):
                pages[cur][off] = w[1].upper()
                off += 3
            elif w and not code.lstrip().upper().startswith("JMP"):
                cur = None
    # routines: PI/JMP targets and page entries
    targets = set()
    for f, n, code in lines:
        w = code.split()
        if len(w) >= 2 and w[0].upper() in ("PI", "JMP") and re.match(r"^[A-Za-z_]\w*$", w[1]):
            targets.add(w[1].upper())
    for d in pages.values():
        targets.update(d.values())
    body = {}
    who = None
    for f, n, code in lines:
        m = re.match(r"^([A-Za-z_]\w*):", code)
        if m and m.group(1).upper() in targets:
            who = m.group(1).upper()
            body.setdefault(who, [])
            code = code[m.end():]
        if who is None:
            continue
        w = code.split()
        if not w:
            continue
        body[who].append((f, n, [x.upper() for x in re.split(r"[\s,]+", code.strip()) if x]))
    enters, calls, tails = {}, {}, {}
    for r, ls in body.items():
        enters[r] = False
        calls[r], tails[r] = set(), set()
        first = True
        for f, n, w in ls:
            op = w[0]
            if first and op == "ENTER":
                enters[r] = True
            first = False
            if op == "PI" and len(w) > 1:
                calls[r].add(w[1])
            elif op == "JMP" and len(w) > 1 and w[1] in body:
                tails[r].add(w[1])
            elif op in ("GOPAGE", "FAR") and len(w) > 2:
                pg, ent = eq.get(w[1]), eq.get(w[2])
                t = pages.get(pg, {}).get(ent)
                if t:
                    calls[r].add(t)
            elif op in MACRO_CALLS:
                calls[r].add(MACRO_CALLS[op])
    memo, path = {}, {}

    def depth(r, stack=()):
        if r in memo:
            return memo[r]
        if r in stack:
            return 0                      # a loop through tails: counted once
        best, via = 0, None
        for c in calls.get(r, ()):
            if c in body:
                d = depth(c, stack + (r,))
                if d > best:
                    best, via = d, c
        own = (1 if enters.get(r) else 0) + best
        for t in tails.get(r, ()):
            d = depth(t, stack + (r,))
            if d > own:
                own, via = d, t
        memo[r], path[r] = own, via
        return own
    worst = sorted(((depth(r), r) for r in body), reverse=True)
    d, r = worst[0]
    chain = []
    while r:
        chain.append(r)
        r = path.get(r)
    print("checkdepth: deepest %d (max %d): %s" % (d, mx, " > ".join(chain)))
    if show:
        for dd, rr in worst[:15]:
            print("  %2d %s" % (dd, rr))
    if d > mx:
        sys.exit("checkdepth: a chain of %d ENTERs: the K-stack holds %d" % (d, mx))


if __name__ == "__main__":
    main()
