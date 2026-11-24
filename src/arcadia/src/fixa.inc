; fixa.inc -- fixed A, $0000-$05FF: the same in every page's assembly and
; always mapped (2650 page 0, with the screen, console RAM and RAM-L).
;
; The start-up, the vblank's work, the input, the display primitives and
; the main loop (main.inc): it runs the link's step and the screen the
; state's phase wants, one page each. Only fixed code may switch pages
; (PGCALL), since the next fetch from the window comes from the new page.
;
; Register conventions: R0 data, R1-R3 arguments; a routine says what it
; clobbers. PSL is PSLBASE (logical compares, no carry-in) everywhere but
; inside 16-bit arithmetic.

        ORG     $0000
        BCTA,UN START
        DB      $17             ; RETC,UN: the interrupt guard at $0003

; ---- the zero-page vectors: ZBSR *Zx is a 2-byte call from either page ----
        ORG     $0004
ZDPOS:  DWBE    DPOS
ZDPUTC: DWBE    DPUTC
ZDTXT:  DWBE    DTXT
ZDNUM:  DWBE    DNUM
ZSNDQ:  DWBE    SNDQUE
ZDFILL: DWBE    DFILL
ZDCLR:  DWBE    DCLR
ZFRAME: DWBE    FRAME
ZCENTR: DWBE    CENTRE
ZTEXT:  DWBE    TEXTSCR
ZA2G:   DWBE    A2G
ZACTQ:  DWBE    ACTQ
ZSCRCLR: DWBE   SCRCLR
ZNAMEAT: DWBE   NAMEAT
ZDGLY:  DWBE    DGLYPH
ZRAMPUT: DWBE   RAMPUT
ZRAMP1: DWBE    RAMPUT1
ZRAMEND: DWBE   RAMPEND

        ORG     $0040
; ---- pointer words: page-0 code's way into page 1 ------------------------
PBANK:  DWBE    FNBANK          ; *PBANK,Rn selects page Rn
PRSEL:  DWBE    FNRSEL
PRDAT:  DWBE    FNRDAT          ; *PRDAT,Rn: Rn into the register selected (a
                                ;  stream: into cart RAM, the pointer on)
PRDATA: DWBE    FNRDATA         ; the reply
PTXD:   DWBE    FNDATA          ; *PTXD,Rn appends Rn to the TX stream
PRXLL:  DWBE    FNRXLL
PMAGF:  DWBE    FNMAGF
PMAGN:  DWBE    FNMAGN
PPVER:  DWBE    FNPVER
PSNDH:  DWBE    SNDTH           ; each sound's first note, its priority (fixed
PSNDL:  DWBE    SNDTL           ;  B's: sndequ.inc)
PSNDP:  DWBE    SNDPRT
PKT0:   DWBE    KT0
PKT1:   DWBE    KT1
PKT2:   DWBE    KT2
PKTP:   DWBE    KTP
PDEFSP: DWBE    DEFSPEC
PDEFURL: DWBE   DEFURL
PSCRIPT: DWBE   SCRIPT          ; the segment: *PSCRIPT,Rn (the player)
PTXTG:  DWBE    TXTGLY-1        ; the text screens' user glyphs (TEXTSCR)
PIMENU: DWBE    IMENU           ; the items line (main.inc: Clear shuts it)

; ---- start-up ----------------------------------------------------------------
; RESET restarts here but never resets the cart: the page, cart RAM and the
; socket are as they were. So everything is set up afresh (the socket is
; closed before it is opened again; the cart's RAM zeroed).
START:  LODI,R0 $20             ; interrupts inhibited (none are wired anyway)
        LPSU
        LODI,R0 PSLBASE
        LPSL
        LODI,R1 0
        STRA,R1 CURPG
        LODA,R0 *PBANK,R1       ; page 0 into the window
        EORZ    R0
        STRA,R0 PITCH
        STRA,R0 VOLUME
        LODI,R3 ZA_HI-ZA_LO     ; zone A cleared
ST2:    STRA,R0 ZA_LO-1,R3
        BDRR,R3 ST2
        LODI,R3 ZI_HI-ZI_LO     ; the island
ST2A:   STRA,R0 ZI_LO-1,R3
        BDRR,R3 ST2A
        LODI,R3 0               ; TZ and zone C ($1A00-$1AFF)
ST3:    STRA,R0 $1A00,R3,-
        BRNR,R3 ST3
        LODI,R0 $FF
        STRA,R0 KRAW
        LODA,R0 *PMAGF          ; the cart: 'F','N', protocol 2 or later
        COMI,R0 $46
        BCFA,EQ STBAD
        LODA,R0 *PMAGN
        COMI,R0 $4E
        BCFA,EQ STBAD
        LODA,R0 *PPVER
        COMI,R0 2
        BCTA,LT STBAD
        BSTA,UN CARTCLR         ; its RAM zeroed (a RESET leaves it)
        LODI,R0 NONE
        CPUT    MYSEAT
        BSTA,UN SCRINIT
        BCTA,UN MAININIT

; SCRINIT: the screen: 13 rows of 8x16 cells from the top, text, the
; look's ground and inks; cleared; the sprites off; no longer the field's
; (FL_FIELD, multicolour). SCRCLR: the same but the ground and inks kept.
; Calls two down. Clobbers R0-R3.
SCRINIT: LODI,R0 LOOKBG
        CPUT    BGCB
SCRCLR: LODA,R0 FLAGS           ; the field's no more
        TMI,R0  FL_FIELD
        BCFR,EQ SCC1
        ANDI,R0 ~FL_FIELD&$FF
        CPUT    FLAGS
        EORZ    R0
        STRA,R0 PITCH           ; (multicolour off)
SCC1:   LODI,R0 $FF
        STRA,R0 VPOS            ; rows from the top of the screen
        EORZ    R0
        STRA,R0 DCOLOR
        STRA,R0 GFXMOD
        STRA,R0 SPRC01
        STRA,R0 SPRC23
        LODI,R3 8
SCI1:   STRA,R0 SPRPOS-1,R3     ; sprites off (Y 0)
        BDRR,R3 SCI1
        LODA,R0 BGCB
        STRA,R0 BGCOL
        LODI,R1 0
        LODI,R3 13
        BCTA,UN DCLR            ; tail call

; TEXTSCR: a text screen: SCRCLR, the text's user glyphs in all eight
; UDCs (the sprites are off: '%' '<' '!' '?' '>' '-' ':' '/', fixed B's
; TXTGLY), DCOLOR the text's ink. After it TZ is the screen's own.
; Calls three down. Clobbers R0-R3.
TEXTSCR: BSTA,UN SCRINIT
        LODI,R3 64
TXS1:   LODA,R0 *PTXTG,R3
        STRA,R0 UDC-1,R3
        BDRR,R3 TXS1
        DCOL    TXTI
        RETC,UN

; ---- the vblank ----------------------------------------------------------------
; VBSYNC: wait for the next vertical blank, then what must happen in it:
; the disc (the 2637 digitises it only then), the sound. A pass that ran
; late, into a blank (VSEEN: the main loop saw the display since the
; last), has it now instead of waiting a frame more. Clobbers R0-R3.
VBSYNC: TPSU    $80
        BCFR,EQ VBS1            ; the display: wait for the blank
        LODA,R0 VSEEN           ; a blank: a new one?
        BCFR,EQ VBS0
VBSW:   TPSU    $80             ; the same one: its end, then the next
        BCTR,EQ VBSW
VBS1:   TPSU    $80
        BCFR,EQ VBS1
VBS0:   EORZ    R0
        STRA,R0 VSEEN
; the disc: one axis a frame, each kept, the mux set for the other (not
; if the blank is over: outside it the pot reads $FF)
        TPSU    $80
        BCFR,EQ VBN0
        LODA,R0 POTP1
        LODA,R1 AXSEL
        BCFR,EQ VBS3
        STRA,R0 AXV
        LODA,R0 BGCB
        IORI,R0 MUXH
        STRA,R0 BGCOL
        LODI,R0 1
        BCTR,UN VBS4
VBS3:   STRA,R0 AXH
        LODA,R0 BGCB
        STRA,R0 BGCOL
        EORZ    R0
VBS4:   STRA,R0 AXSEL
; the sound (tools/arc/mksnd_arc.py; p_snd.inc): the note's jiffies down
; by DT (the cart's clock: PAL plays in time), then the next note from the
; sound's page (SNDPH b7: the title's), the frames a note ran over owed by
; the next; a 0 ends it, $FF goes on at the address after it.
VBN0:   LODA,R0 SNDPH
        RETC,EQ                 ; none
        LODA,R0 SNDCNT
        SUBA,R0 DT
        STRA,R0 SNDCNT
        RETC,GT                 ; (1-127: it goes on)
        LODI,R1 PG_SND
        LODA,R0 SNDPH
        BCFR,LT VBN1
        LODI,R1 PG_SNDT
VBN1:   LODA,R0 *PBANK,R1       ; (the window's page back at the end)
VBN2:   LODA,R0 *SNDPH          ; its jiffies: 0 the end, $FF on there
        BCTA,EQ VBN8
        COMI,R0 $FF
        BCFR,EQ VBN3
        LODI,R3 1
        LODA,R0 *SNDPH,R3
        STRZ    R1
        LODI,R3 2
        LODA,R0 *SNDPH,R3
        STRA,R0 SNDPL
        STRA,R1 SNDPH
        BCTR,UN VBN2
VBN3:   ADDA,R0 SNDCNT          ; (what the last one owes: 0 or less)
        BCTR,GT VBN4
        LODI,R0 1
VBN4:   STRA,R0 SNDCNT
        LODI,R3 1
        LODA,R0 *SNDPH,R3
        BSTR,UN PITW
        LODI,R3 2
        LODA,R0 *SNDPH,R3
        IORA,R0 SHAKEB          ; (the shake's shift kept: OP_SHAKE)
        STRA,R0 VOLUME
        LODA,R0 SNDPL           ; on 3
        ADDI,R0 3
        STRA,R0 SNDPL
        TPSL    PSLC
        BCFR,EQ VBN9
        LODA,R0 SNDPH
        ADDI,R0 1
        STRA,R0 SNDPH
VBN9:   LODA,R1 CURPG           ; the window's page again
        LODA,R0 *PBANK,R1
        RETC,UN
VBN8:   STRA,R0 SNDPH           ; (R0 0: none plays)
        LODA,R0 SHAKEB
        STRA,R0 VOLUME
        EORZ    R0
        BSTR,UN PITW
        BCTR,UN VBN9
; PITW: PITCH = R0, with b7 (multicolour) kept while the field shows.
; Leaf. Clobbers R0, R2.
PITW:   STRZ    R2
        LODA,R0 FLAGS
        ANDI,R0 FL_FIELD
        IORZ    R2
        STRA,R0 PITCH
        RETC,UN
        IF      FL_FIELD <> $80
        ERROR   "PITW: FL_FIELD must be PITCH's multicolour bit"
        ENDIF

; SNDQUE: sound R0 (an id, sndequ.inc) from the next vblank, unless muted
; or one of a higher priority plays (SNDPRI: the ticks don't cut a tune;
; the talk's letters only when nothing plays). Leaf. Clobbers R0, R1.
SNDQUE: STRZ    R1
        LODA,R0 FLAGS
        TMI,R0  FL_MUTE
        RETC,EQ
        LODA,R0 SNDPH
        BCTR,EQ SQ1
        LODA,R0 *PSNDP,R1
        COMA,R0 SNDPRI
        RETC,LT
SQ1:    LODA,R0 *PSNDP,R1
        STRA,R0 SNDPRI
        LODA,R0 *PSNDL,R1
        STRA,R0 SNDPL
        LODA,R0 *PSNDH,R1
        STRA,R0 SNDPH
        LODA,R0 DT              ; (the vblank takes DT off: its first note whole)
        STRA,R0 SNDCNT
        RETC,UN

; FRAME: the next frame: the vblank, the clock, the controls; Option turns
; the sound off and on. Calls two down. Clobbers R0-R3.
FRAME:  BSTA,UN VBSYNC
        BSTA,UN TICKRD
        LODA,R0 INDIR
        STRA,R0 PRVDIR
        BSTR,UN KEYDEC
        LODA,R0 EVKEY
        COMI,R0 K_OPT
        RETC,LT
        RETC,GT
        LODA,R0 FLAGS
        EORI,R0 FL_MUTE
        CPUT    FLAGS
        EORZ    R0              ; (and whatever plays stops)
        STRA,R0 SNDPH
        STRA,R0 VOLUME
        RETC,UN

; ---- input ----------------------------------------------------------------------
; KEYDEC: this frame's controls from the disc (as last digitised), the
; keypad and the panel. INDIR: directions held (diagonals too). INBTN:
; FIRE held, EVBTN: it went down. EVKEY: a key that went down (K_*,
; digits; FIRE is keypad 2's wire, so 2 is never a digit). Leaf.
; Clobbers R0-R2.
KEYDEC: LODI,R2 0
        LODA,R0 AXV
        COMI,R0 JOYLO
        BCFR,LT KD1
        IORI,R2 D_UP
KD1:    COMI,R0 JOYHI
        BCFR,GT KD2
        IORI,R2 D_DOWN
KD2:    LODA,R0 AXH
        COMI,R0 JOYLO
        BCFR,LT KD3
        IORI,R2 D_LEFT
KD3:    COMI,R0 JOYHI
        BCFR,GT KD4
        IORI,R2 D_RIGHT
KD4:    STRA,R2 INDIR
        LODA,R0 KEYPAD+1        ; FIRE
        ANDI,R0 $08
        BCTR,EQ KD5
        LODA,R0 INBTN           ; down: an event if it was up
        EORI,R0 1
        STRA,R0 EVBTN
        LODI,R0 1
        STRA,R0 INBTN
        BCTR,UN KD6
KD5:    STRA,R0 EVBTN
        STRA,R0 INBTN
; a key: the first found, edge-detected
KD6:    LODI,R1 K_NONE
        LODA,R0 KEYPAD          ; b3 1, b2 4, b1 7, b0 Clear
        ANDI,R0 $0F
        BCTR,EQ KD10
        STRZ    R2
        LODA,R0 *PKT0,R2
        BCTR,UN KD12
KD10:   LODA,R0 KEYPAD+1        ; b2 5, b1 8, b0 0 (b3 is FIRE)
        ANDI,R0 $07
        BCTR,EQ KD11
        STRZ    R2
        LODA,R0 *PKT1,R2
        BCTR,UN KD12
KD11:   LODA,R0 KEYPAD+2        ; b3 3, b2 6, b1 9, b0 Enter
        ANDI,R0 $0F
        BCTR,EQ KD13
        STRZ    R2
        LODA,R0 *PKT2,R2
        BCTR,UN KD12
KD13:   LODA,R0 PANEL           ; b0 Start, b1 Option, b2 Select
        ANDI,R0 $07
        BCTR,EQ KD14
        STRZ    R2
        LODA,R0 *PKTP,R2
KD12:   STRZ    R1
KD14:   LODZ    R1
        COMA,R0 KRAW
        STRA,R1 KRAW
        BCFR,EQ KD15
        LODI,R1 K_NONE          ; held: no event
KD15:   STRA,R1 EVKEY
        RETC,UN

; ---- the display ------------------------------------------------------------------
; DPOS: point the display at row R1 (0-25: 13 on are $1A00's, the 26-row
; mode's); R2 is left alone (the cell before the first one written: see
; DAT). Leaf. Clobbers R0.
DPOS:   LODZ    R1
        COMI,R0 13
        BCTR,LT DPS1
        SUBI,R0 13
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        STRA,R0 DROWL
        LODI,R0 SCR2>>8
        STRA,R0 DROWH
        RETC,UN
DPS1:   RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        STRA,R0 DROWL
        LODI,R0 SCR>>8
        STRA,R0 DROWH
        RETC,UN

; DPUTC: glyph R0 (0-63) in DCOLOR at the next cell (R2 pre-incremented).
; DGLYPH: R0 already carries its colour bits. An exact $C0 (a blank in
; the fourth ink) would start block graphics: it is written as $00, which
; looks the same. Leaf. Clobbers R0, advances R2.
DPUTC:  IORA,R0 DCOLOR
DGLYPH: COMI,R0 GMOSAIC
        BCFR,EQ DPC1
        EORZ    R0
DPC1:   STRA,R0 *DROWH,R2,+
        RETC,UN

; DTXT: the glyph string at R0:R1 (glyph codes, $FF ends it; DB strings
; are glyphs, see GLYPHS). DTXTS: the one at DSTRH:DSTRL. Calls one
; down. Clobbers R0, R3; advances R2.
DTXT:   STRA,R0 DSTRH
        STRA,R1 DSTRL
DTXTS:  LODI,R3 $FF
DTX1:   LODA,R0 *DSTRH,R3,+
        COMI,R0 $FF
        RETC,EQ
        BSTR,UN DPUTC
        BCTR,UN DTX1

; CENTRE: the glyph string at R0:R1 centred on row R3 (16 cells), in
; DCOLOR. Calls two down. Clobbers R0-R3.
CENTRE: STRA,R0 DSTRH
        STRA,R1 DSTRL
        LODI,R2 $FF             ; its length
CEN1:   LODA,R0 *DSTRH,R2,+
        COMI,R0 $FF
        BCFR,EQ CEN1
        LODI,R0 16              ; the cell before (16 - n) / 2
        SUBZ    R2
        RRR,R0
        ANDI,R0 $7F
        SUBI,R0 1
        STRZ    R2
        LODZ    R3
        STRZ    R1
        BSTA,UN DPOS
        BCTR,UN DTXTS           ; tail call

; DFILL: R3 cells of R0 (glyph and colour bits) at the next cells.
; Leaf. Clobbers R0, R3; advances R2.
DFILL:  STRA,R0 *DROWH,R2,+
        BDRR,R3 DFILL
        RETC,UN

; DCLR: R3 rows of spaces from row R1. Calls one down. Clobbers R0-R3.
DCLR:   BSTA,UN DPOS
        LODI,R2 $FF
        LODI,R0 0
DCL1:   STRA,R0 *DROWH,R2,+
        COMI,R2 15
        BCFR,EQ DCL1
        ADDI,R1 1
        BDRR,R3 DCLR
        RETC,UN

; DNUM: R0 (0-255) in decimal, right-aligned in R1 cells (1-3), in DCOLOR,
; at the next cells (a leading place blank). Calls one down. Clobbers
; R0, R1, R3; advances R2.
DNUM:   STRA,R0 NUMV
        LODI,R3 0               ; the places shown so far
        COMI,R1 3
        BCFR,EQ DNU2
        LODI,R1 100
        BSTR,UN DNPL
        LODI,R1 2
DNU2:   COMI,R1 2
        BCFR,EQ DNU3
        LODI,R1 10
        BSTR,UN DNPL
DNU3:   LODA,R0 NUMV            ; the units: always
        IORI,R0 GDIGIT
        BCTA,UN DPUTC           ; tail call
; one place: R1 its power; a leading zero is a space (R3 counts digits shown)
DNPL:   LODI,R0 0
        STRA,R0 NUMD
DNP1:   LODA,R0 NUMV
        COMZ    R1
        BCTR,LT DNP2
        SUBZ    R1
        STRA,R0 NUMV
        LODA,R0 NUMD
        ADDI,R0 1
        STRA,R0 NUMD
        BCTR,UN DNP1
DNP2:   LODA,R0 NUMD
        IORZ    R3
        BCTR,EQ DNP3            ; a leading zero
        LODI,R3 1
        LODA,R0 NUMD
        IORI,R0 GDIGIT
        BCTA,UN DPUTC
DNP3:   EORZ    R0
        BCTA,UN DPUTC

; A2G: R0 an ASCII letter or digit to its glyph (anything else a space).
; Leaf.
A2G:    COMI,R0 $61
        BCTR,LT A2G0
        SUBI,R0 $20
A2G0:   COMI,R0 $41
        BCTR,LT A2G1
        COMI,R0 $5A
        BCTR,GT A2G9
        SUBI,R0 $41-GALPHA
        RETC,UN
A2G1:   COMI,R0 $30
        BCTR,LT A2G9
        COMI,R0 $39
        BCTR,GT A2G9
        SUBI,R0 $30-GDIGIT
        RETC,UN
A2G9:   EORZ    R0
        RETC,UN

; NAMEAT: seat R3's name in 8 cells (spaces after it), in DCOLOR, at the
; next cells. Calls one down. Clobbers R0, R1; R3 kept.
NAMEAT: STRA,R3 T3
        LODA,R0 SEAT25,R3
        STRZ    R3
        LODI,R1 NAME_LEN
NA1:    LODA,R0 ST+OFF_SEATS+P_NAME,R3
        BSTR,UN A2G
        BSTA,UN DPUTC
        ADDI,R3 1
        BDRR,R1 NA1
        LODA,R3 T3
        RETC,UN
; each seat's offset in ST's seats (SEAT_SIZE apart)
SEAT25: DB      0,SEAT_SIZE,2*SEAT_SIZE,3*SEAT_SIZE,4*SEAT_SIZE
        DB      5*SEAT_SIZE,6*SEAT_SIZE,7*SEAT_SIZE,8*SEAT_SIZE,9*SEAT_SIZE

; ---- an action ---------------------------------------------------------------------
; ACTQ: an action to send: R1 its kind (ACT_*), R2 its args' count (0-6),
; the args already in AARGS (CPTR AARGS, RAMPUT1 each, RAMPEND). The next
; seq (never 0). One waits at a time: a second replaces it. Calls one
; down. Clobbers R0-R3.
ACTQ:   STRA,R1 T0
        STRA,R2 T1
        CPTR    ASEQ
        LODA,R0 ASEQ
        ADDI,R0 1
        BCFR,EQ ACQ1
        LODI,R0 1
ACQ1:   BSTA,UN RAMPUT1
        LODA,R0 T0
        BSTA,UN RAMPUT1
        BSTA,UN RAMPEND
        LODA,R0 T1
        CPUT    ANARG
        LODA,R0 TXF
        IORI,R0 TX_ACT
        STRA,R0 TXF
        RETC,UN

; ---- pages ---------------------------------------------------------------------------
; PGCALL: entry R3 of page R1, which stays in the window. Calls the page,
; costing one return address, not two. R2 passes through.
PGCALL: STRA,R1 CURPG
        LODA,R0 *PBANK,R1
        BXA     WIN,R3          ; (the entry's return is our caller's)
; PGFAR: the same, from a page: the page that was there comes back. One
; level only (PGSAVE). R2, R3 come back as the entry left them.
PGFAR:  LODA,R0 CURPG
        STRA,R0 PGSAVE
        BSTR,UN PGCALL
        LODA,R1 PGSAVE
        STRA,R1 CURPG
        LODA,R0 *PBANK,R1
        RETC,UN

        INCLUDE "src/main.inc"

        IF      $ > $0600
        ERROR   "fixed A runs into RAM-L"
        ENDIF
