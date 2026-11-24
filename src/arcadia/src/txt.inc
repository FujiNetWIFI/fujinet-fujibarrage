; txt.inc -- what every text screen's page shares (p_room.inc, p_shop.inc,
; p_sets.inc include it: a page can't call another page's code): a string
; padded, the disc's repeat, a seat's name's ink.

; TXTPAD: the glyph string at R0:R1, then spaces to cell R3 (the last one
; written: R2 reaches R3). Calls two down. Clobbers R0, R1, R3; advances R2.
TXTPAD: STRA,R3 T3
        ZBSR    *ZDTXT
TPD1:   COMA,R2 T3
        RETC,EQ
        BCFR,LT TPD2
        EORZ    R0
        ZBSR    *ZDPUTC
        BCTR,UN TPD1
TPD2:   COMI,R2 $FF             ; (nothing written yet: R2 is the cell before)
        RETC,LT
        EORZ    R0
        ZBSR    *ZDPUTC
        BCTR,UN TPD1

; REP: R0 = the directions to act on this frame (room.c rep): a push, then
; every 4 jiffies once held 16 (REPN). Leaf. Clobbers R0, R1.
REP:    LODA,R0 INDIR
        BCFR,EQ RP1
        STRA,R0 REPN
        RETC,UN
RP1:    LODA,R1 REPN            ; (jiffies: PAL's 50 frames as long)
        BCFR,EQ RP2
        LODA,R0 DT              ; the push
        STRA,R0 REPN
        LODA,R0 INDIR
        RETC,UN
RP2:    LODZ    R1
        ADDA,R0 DT
        COMI,R0 128
        BCTR,LT RP3
        SUBI,R0 64              ; (held long: on every 4 still)
        SUBI,R1 64
RP3:    STRA,R0 REPN
        COMI,R0 16
        BCTR,LT RP0
        EORZ    R1              ; a 4-jiffy step crossed
        ANDI,R0 $FC
        BCTR,EQ RP0
        LODA,R0 INDIR
        RETC,UN
RP0:    EORZ    R0
        RETC,UN

; SNAMEI: DCOLOR = seat R3's name's ink (look.inc NAMEI*: seat mod 5).
; Leaf. Clobbers R0.
SNAMEI: LODZ    R3
SNI1:   COMI,R0 5
        BCTR,LT SNI2
        SUBI,R0 5
        BCTR,UN SNI1
SNI2:   STRA,R3 DCOLOR          ; (R3 kept: the index through DCOLOR)
        STRZ    R3
        LODA,R0 SNINK,R3
        LODA,R3 DCOLOR
        STRA,R0 DCOLOR
        RETC,UN
SNINK:  DB      NAMEI0,NAMEI1,NAMEI2,NAMEI3,NAMEI4
