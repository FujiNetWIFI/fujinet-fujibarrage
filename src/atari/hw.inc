;
; hw.inc -- the chips and the graphics memory (barrage.cfg; atari.h has the
; same for C). The 5200 (src/atari5200, barrage-a52.cfg) assembles the same
; files: the same ANTIC, GTIA and POKEY at other addresses, the graphics in
; its 16K of RAM, the crt0's shadows in zero page where the OS's were.
;
.ifdef __ATARI7800__
; (the 7800: src/atari7800/a78.inc; only POKEY, the cartridge's, and the
; field's lines are these files'. The field is MARIA's zones: fld.s's row
; tables.)
GTIA    = $D000
POKEY   = $0450
DLICODE = $0000
PMG     = $0000
FIELD   = $4000
FIELD2  = $4000
TEXT    = $5878
STATUS  = $0000
.elseif .defined(__ATARI5200__)
GTIA    = $C000
POKEY   = $E800
DLICODE = $2B00         ; two buffers of compiled DLIs ($300 each)
PMG     = $0800         ; PMBASE (single-line)
FIELD   = $1010         ; mode E, 168 lines of 40 bytes
FIELD2  = $2000         ; line 102: the second LMS
TEXT    = $1010         ; a text screen, 24 x 40 (in the field's memory)
STATUS  = $2A50         ; the field's status rows, 3 x 40
.else
GTIA    = $D000
POKEY   = $D200
DLICODE = $9000         ; two 1K buffers of compiled DLIs
PMG     = $9800         ; PMBASE (single-line)
FIELD   = $A010         ; mode E, 168 lines of 40 bytes
FIELD2  = $B000         ; line 102: the second LMS
TEXT    = $A010         ; a text screen, 24 x 40 (in the field's memory)
STATUS  = $BA50         ; the field's status rows, 3 x 40
.endif
ANTIC   = $D400
MISSILES = PMG + $0300
PLAYERS = PMG + $0400
FLINES  = 168
FTOP    = 56            ; the field's first scanline (players' line index)

HPOSP0  = GTIA + $00
HPOSM0  = GTIA + $04
SIZEP0  = GTIA + $08
SIZEM   = GTIA + $0C
GRAFM   = GTIA + $11
COLPM0  = GTIA + $12
COLPF0  = GTIA + $16
COLPF1  = GTIA + $17
COLPF2  = GTIA + $18
COLPF3  = GTIA + $19
COLBK   = GTIA + $1A
PRIOR   = GTIA + $1B
GRACTL  = GTIA + $1D
PAL     = GTIA + $14     ; (read) $0E on NTSC, $00 on PAL

DMACTL  = ANTIC + $00
DLISTL  = ANTIC + $02
PMBASE  = ANTIC + $07
CHBASE  = ANTIC + $09
WSYNC   = ANTIC + $0A
NMIEN   = ANTIC + $0E

AUDF1   = POKEY + $00
AUDC1   = POKEY + $01
AUDCTL  = POKEY + $08
SKCTL   = POKEY + $0F

.ifdef __ATARI5200__
; the crt0's (src/atari5200/crt0.s) vectors and zero-page shadows
VVBLKI  = $0202
VDSLST  = $0206
CRITIC  = $03
ATRACT  = $04
SDLSTL  = $05
SDMCTL  = $07
PCOLR0  = $08
COLOR0  = $0C
COLOR4  = $10
RTCLOK  = $02           ; (its low byte; $01 the high)
.else
VDSLST  = $0200
SDMCTL  = $022F
SDLSTL  = $0230
GPRIOR  = $026F
PCOLR0  = $02C0
COLOR0  = $02C4
COLOR4  = $02C8
CHBAS   = $02F4
ATRACT  = $004D
RTCLOK  = $0014
CRITIC  = $0042
KEYDEL  = $02F1
SSKCTL  = $0232
SETVBV  = $E45C
XITVBV  = $E462
SYSVBV  = $E45F
.endif
