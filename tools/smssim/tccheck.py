#!/usr/bin/env python3
"""tccheck.py TC.bin -- the tile cache's tables (src/sms/tcache.asm, the cart's
RAM half 1 as smsrun's `cartram FILE 1` dumps it) in order: every bucket's
chain ends (FFFFh) without a loop, each tile in it says it is that bucket's
(TC_HB) and is in no other chain, hashed tiles have a kind. Exits non-zero
if not."""
import sys

KEYS, NEXT, REFS, HB, HEADS = 0, 0x1F80, 0x2300, 0x24C0, 0x2680
h = open(sys.argv[1], "rb").read()
bad, where, n = 0, {}, 0
for b in range(256):
    t = h[HEADS + b * 2] | h[HEADS + b * 2 + 1] << 8
    seen = set()
    while t != 0xFFFF:
        if t in seen or not 32 <= t < 448:
            print("bucket %d: %s at tile %d" % (b, "a loop" if t in seen else "a bad link", t))
            bad += 1
            break
        seen.add(t)
        if h[HB + t] != b:
            print("tile %d: in bucket %d, its own says %d" % (t, b, h[HB + t]))
            bad += 1
        if h[KEYS + t * 18] == 0:
            print("tile %d: in bucket %d with no key" % (t, b))
            bad += 1
        if t in where:
            print("tile %d: in buckets %d and %d" % (t, where[t], b))
            bad += 1
        where[t] = b
        n += 1
        t = h[NEXT + t * 2] | h[NEXT + t * 2 + 1] << 8
used = sum(1 for t in range(32, 448) if h[REFS + t])
print("%d tiles hashed, %d in use; %d faults" % (n, used, bad))
sys.exit(1 if bad else 0)
