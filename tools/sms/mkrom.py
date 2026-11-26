#!/usr/bin/env python3
"""Make FujiBarrage's Master System image, r2r/sms/barrage.sms: a Sega-mapper
ROM for the FujiNet SMS cartridge, from what z88dk linked (src/sms/map.asm
has the map). FujiBlast's tools/sms/mkrom.py (M.U.L.E.'s), with banks.

usage: mkrom.py BUILD OUT   (BUILD/barrage.sms as z88dk made it, barrage.map,
                            barrage_<SECTION>.bin for the banks)

  banks 0-1   0000h-7FFFh, fixed: the program; the "FUJI" claim at 7FDCh
              (the cartridge keeps its mailbox on for a claimed image) and
              the SEGA header at 7FF0h, its checksum made again over it
  banks 2-    slot 2's (8000h-AFFFh): the art, the title, the tunes, in the
              order of BANKS (each 12K at most: B000h-BFFFh is the mailbox)

The cartridge picks the mapper from the image's first 32K (its heuristic,
ported from fujinet-firmware pico/sms/firmware/src/smsmap.c): the image is
refused unless that says Sega, and the cart's 32K of RAM comes only with a
Sega mapper. It also checks: the program under the claim, the variables
under the stack's room, no constant store to slot 2's RAM.
"""
import os
import re
import sys

BANK = 0x4000
WIN = 0x3000
CLAIM = 0x7FDC
FENCE = (0x7FD8, 0x7FF0)
BSS_MAX = 0xDD00                # the stack: DFF0h down, at least 750 bytes (a live game: ~200 used)
# slot 2's banks, from bank 2: a section z88dk linked (barrage_NAME.bin) or
# a file of gen/sms ("file:..."): FAR (src/sms/map.asm; build.mk FAR_BANK),
# the title (tools/sms/mktitle_sms.py; scr.asm's TITLE_BANK)
BANKS = ["FAR", "file:gen/sms/title.bin", "file:gen/sms/mus_psg.bin", "file:gen/sms/mus_fm.bin"]
MIN_BANKS = 8                   # (128K at least: a power of two, a Sega-mapper image's size)


def symbols(path):
    out = {}
    for line in open(path):
        m = re.match(r"(\S+)\s+=\s+\$([0-9A-Fa-f]+)", line)
        if m:
            out[m.group(1)] = int(m.group(2), 16)
    return out


def mapper(img):
    """smsmap_heuristic: the mapper an unknown image gets."""
    n = dict.fromkeys(["0002", "8000", "a000", "ffff", "3ffe", "4000", "6000"], 0)
    keys = {0xFFFF: "ffff", 0x0002: "0002", 0x0003: "0002", 0x0004: "0002", 0x8000: "8000",
            0xA000: "a000", 0x3FFE: "3ffe", 0x4000: "4000", 0x6000: "6000"}
    i = 0
    while i < 0x8000:
        if img[i] == 0x32:
            a = img[i + 1] | img[i + 2] << 8
            if a in keys:
                n[keys[a]] += 1
                i += 3
                continue
        i += 1
    if len(img) > 0x10000 and (n["0002"] > n["ffff"] + 2 or (n["0002"] and not n["ffff"])):
        return "Zemina", n
    if n["8000"] > n["ffff"] + 2 or (n["8000"] and not n["ffff"]):
        return "Codemasters", n
    if n["a000"] > n["ffff"] + 2 or (n["a000"] and not n["ffff"]):
        return "Korean", n
    if n["3ffe"] > n["ffff"] + 2 or n["3ffe"]:
        return "4 PAK", n
    if n["4000"] and n["6000"] and n["8000"] and n["a000"]:
        return "Janggun", n
    return "Sega", n


def main():
    d, outdir = sys.argv[1:3]
    bss_max = BSS_MAX if "--test" not in sys.argv else 0xDE00   # (the offline scenes: a shallow stack)
    img = open(os.path.join(d, "barrage.sms"), "rb").read()
    sym = symbols(os.path.join(d, "barrage.map"))
    end = sym["__ROMABLE_END_tail"] + (sym["__DATA_END_tail"] - sym["__DATA_head"])
    bss = sym["__BSS_END_tail"]
    errors = []
    if end > FENCE[0]:
        errors.append("the program ends at %04Xh, past the claim's fence (%04Xh): %d bytes over"
                      % (end, FENCE[0], end - FENCE[0]))
    if bss > bss_max:
        errors.append("the variables end at %04Xh: too little room for the stack (%d bytes over)" % (bss, bss - bss_max))
    if len(img) < 2 * BANK or img[0x7FF0:0x7FF8] != b"TMR SEGA":
        errors.append("z88dk's image has no SEGA header at 7FF0h")
    out = bytearray(img[:2 * BANK].ljust(2 * BANK, b"\xff"))
    out[CLAIM:CLAIM + 4] = b"\xff" * 4
    if any(b != 0xFF for b in out[FENCE[0]:FENCE[1]]):
        errors.append("7FD8h-7FEFh isn't free for the claim")
    sizes = []
    top = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    for name in BANKS:
        if name.startswith("file:"):
            name = name[5:]
            data = open(os.path.join(top, name), "rb").read()
        else:
            data = open(os.path.join(d, "barrage_%s.bin" % name), "rb").read()
        if len(data) > WIN:
            errors.append("%s is %d bytes: over the window's %d" % (name, len(data), WIN))
        out += data[:WIN].ljust(BANK, b"\xff")
        sizes.append("%s %d" % (name, len(data)))
    while len(out) < MIN_BANKS * BANK:
        out += b"\xff" * BANK
    out[CLAIM:CLAIM + 4] = b"FUJI"
    s = sum(out[:0x7FF0]) & 0xFFFF
    out[0x7FFA] = s & 0xFF
    out[0x7FFB] = s >> 8
    kind, n = mapper(out)
    if kind != "Sega":
        errors.append("the cartridge would take it for a %s mapper (stores: %s)" %
                      (kind, ", ".join("%s %d" % kv for kv in n.items() if kv[1])))
    if n["8000"] or n["a000"]:
        errors.append("a constant store to 8000h or A000h (slot 2's RAM is written through pointers)")
    if errors:
        for e in errors:
            print("mkrom.py: " + e, file=sys.stderr)
        sys.exit(1)
    os.makedirs(outdir, exist_ok=True)
    open(os.path.join(outdir, "barrage.sms"), "wb").write(out)
    print("sms: program 0000-%04X (%d free), variables C000-%04X (stack %d); %s; %dK, Sega mapper (%d stores to FFFFh)" %
          (end - 1, FENCE[0] - end, bss - 1, 0xDFF0 - bss, ", ".join(sizes) or "no banks", len(out) // 1024, n["ffff"]))


if __name__ == "__main__":
    main()
