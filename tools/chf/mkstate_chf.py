#!/usr/bin/env python3
"""mkstate_chf.py -- a canned STATE for the Channel F's screen test scene
(p_test.inc scene 4: tools/chfsim `load` it into ST).

    mkstate_chf.py CATALOG.bin PHASE OUT.bin [key=value ...]

PHASE: wait, shop, aim, round, over. Seats: CHF (person, seat 0, ours,
the host) and bots ROOKIE..; cash, an inventory, wins; the catalog's length
and sums (so CATCHK takes it). key=value sets a byte: the offset an
expression over proto.h's names, e.g. OFF_RESULT=2, OFF_INV+I_FUEL=40.
"""
import re
import sys

PROTO = __import__("os").path.join(__import__("os").path.dirname(__import__("os").path.abspath(__file__)),
                                   "..", "..", "src", "proto.h")


def main():
    d = {k: int(v) for k, v in re.findall(r"#define\s+(\w+)\s+(\d+)", open(PROTO).read())}
    cat = open(sys.argv[1], "rb").read()
    phase = {"wait": d["PH_WAIT"], "shop": d["PH_SHOP"], "aim": d["PH_AIM"],
             "round": d["PH_ROUNDEND"], "over": d["PH_GAMEOVER"]}[sys.argv[2]]
    st = bytearray(d["STATE_SIZE"])
    st[d["OFF_PHASE"]] = phase
    st[d["OFF_ROUND"]] = 1
    st[d["OFF_ROUNDS"]] = 3
    st[d["OFF_HOST"]] = 0
    st[d["OFF_TURN"]] = 0
    st[d["OFF_TIMER"]] = 42
    st[d["OFF_SEQ"]] = 1
    st[d["OFF_MY_SEAT"]] = 0
    st[d["OFF_RESULT"]] = 0
    s1 = s2 = 0
    for b in cat:
        s1 = (s1 + b) % 255
        s2 = (s2 + s1) % 255
    st[d["OFF_CAT_LEN"]], st[d["OFF_CAT_LEN"] + 1] = len(cat) & 255, len(cat) >> 8
    st[d["OFF_CAT_SUM"]], st[d["OFF_CAT_SUM"] + 1] = s1, s2
    names = ["CHF", "ROOKIE", "GUNNER", "LOBBER", "ACE"]
    for s, n in enumerate(names):
        a = d["OFF_SEATS"] + d["SEAT_SIZE"] * s
        st[a:a + len(n)] = n.encode()
        st[a + d["P_KIND"]] = d["KIND_HUMAN"] if s == 0 else d["KIND_BOT"]
        st[a + d["P_TIER"]] = s
        st[a + d["P_ENERGY"]] = 100
        st[a + d["P_FLAGS"]] = d["PF_ALIVE"] | d["PF_CONN"]
        st[a + d["P_WINS"]] = s % 3
        st[a + d["P_ANGLE"]] = 45
        st[a + d["P_POWER"]] = 0xEE
        st[a + d["P_POWER"] + 1] = 0x02
    cash = 123456
    st[d["OFF_CASH"]:d["OFF_CASH"] + 4] = cash.to_bytes(4, "little")
    for i, n in ((0, 99), (1, 3), (2, 5), (8, 1), (39, 2), (42, 1)):
        st[d["OFF_INV"] + i] = n
    st[d["OFF_SETTINGS"] + d["SET_ARMS"]] = 4
    for kv in sys.argv[4:]:
        k, v = kv.split("=")
        st[eval(k, {}, d)] = int(v, 0)
    open(sys.argv[3], "wb").write(st)


if __name__ == "__main__":
    main()
