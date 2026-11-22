;
; hw.inc -- the chips and the graphics memory (barrage.cfg; atari.h has the
; same for C).
;
GTIA    = $D000
POKEY   = $D200
ANTIC   = $D400
DLICODE = $9000         ; two 1K buffers of compiled DLIs
PMG     = $9800         ; PMBASE (single-line)
MISSILES = PMG + $0300
PLAYERS = PMG + $0400
FIELD   = $A010         ; mode E, 168 lines of 40 bytes
FIELD2  = $B000         ; line 102: the second LMS
TEXT    = $A010         ; a text screen, 24 x 40 (in the field's memory)
STATUS  = $BA50         ; the field's status rows, 3 x 40
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
