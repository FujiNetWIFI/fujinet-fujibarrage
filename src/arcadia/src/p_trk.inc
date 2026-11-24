; p_trk.inc -- the shot's tracks (OP_ARC, OP_STOP; view.go's track:
; drawHead, eraseHead, Arc.Step), a page of their own with the field's
; blocks (blk.inc). The PLY page calls them (FARPG): a frame of every track
; (E_TSTEP), an OP_ARC or OP_STOP whose arguments are at PPC.

        BCTA,UN TRKSTEP         ; E_TSTEP: every live track a step
        BCTA,UN OPARC           ; E_TLOAD: OP_ARC
        BCTA,UN OPSTOP          ; E_TSTOP: OP_STOP

TK_ON   EQU     $10             ; a track's style byte: b0-3 the style, the flags
TK_DRAWN EQU    $20             ;  (its head drawn: TK_HX, TK_HY)

; a track's bytes (R3 its offset, TOFF): its style, X (lo mid hi), Y, VX,
; VY, n (16), AX (16), AY (16), the head drawn at (x, y)
TP0:    DWBE    TRACKS
TP1:    DWBE    TRACKS+1
TP2:    DWBE    TRACKS+2
TP3:    DWBE    TRACKS+3
TP4:    DWBE    TRACKS+4
TP5:    DWBE    TRACKS+5
TP6:    DWBE    TRACKS+6
TP7:    DWBE    TRACKS+7
TP8:    DWBE    TRACKS+8
TP9:    DWBE    TRACKS+9
TP10:   DWBE    TRACKS+10
TP11:   DWBE    TRACKS+11
TP12:   DWBE    TRACKS+12
TP13:   DWBE    TRACKS+13
TP14:   DWBE    TRACKS+14
TP15:   DWBE    TRACKS+15
TP16:   DWBE    TRACKS+16
TP17:   DWBE    TRACKS+17
TP18:   DWBE    TRACKS+18
TP19:   DWBE    TRACKS+19
TP20:   DWBE    TRACKS+20
TK_HX   EQU     19
TK_HY   EQU     20
TOFF:   DB      0,TKSZ,2*TKSZ,3*TKSZ,4*TKSZ,5*TKSZ
; ---- tracks (OP_ARC, OP_STOP) -----------------------------------------------
; OP_ARC slot, style, x16, y16, vx24, vy24, ax16, ay16, n16: a track (its
; old head away first): X = x16 << 8 (12.12), the head drawn at once.
OPARC:  LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3
        ANDI,R0 15
        COMI,R0 NTRACK
        BCFA,LT OPA9            ; (the server sends none past our 6)
        STRZ    R1
        LODA,R0 TOFF,R1
        STRA,R0 PTMP
        BSTA,UN TROFF
        LODI,R1 FRRAMP2+((TRACKS-$2900)>>8)
        LODA,R3 PTMP
        BSTA,UN RAMSET
        LODA,R3 PPC             ; (R3: the slot's byte)
        LODA,R0 *PSCRIPT,R3,+   ; style, on
        ANDI,R0 15
        IORI,R0 TK_ON
        BSTA,UN TW
        LODI,R0 0               ; X: 0, x16
        BSTA,UN TW
        LODA,R0 *PSCRIPT,R3,+
        BSTR,UN TW
        LODA,R0 *PSCRIPT,R3,+
        BSTR,UN TW
        LODI,R0 0               ; Y: 0, y16
        BSTR,UN TW
        LODA,R0 *PSCRIPT,R3,+
        BSTR,UN TW
        LODA,R0 *PSCRIPT,R3,+
        BSTR,UN TW
        LODI,R1 6               ; VX, VY
OA1:    LODA,R0 *PSCRIPT,R3,+
        BSTR,UN TW
        BDRR,R1 OA1
        ADDI,R3 4               ; n (its bytes 16, 17)
        LODA,R0 *PSCRIPT,R3,+
        BSTR,UN TW
        LODA,R0 *PSCRIPT,R3,+
        BSTR,UN TW
        SUBI,R3 6               ; AX, AY (12-15)
        LODI,R1 4
OA2:    LODA,R0 *PSCRIPT,R3,+
        BSTR,UN TW
        BDRR,R1 OA2
        BSTA,UN RAMPEND
        BSTA,UN TRON
OPA9:   RETC,UN
; TW: R0 into the stream. Leaf. Clobbers R2.
TW:     STRZ    R2
        LODA,R0 *PRDAT,R2
        RETC,UN

; OP_STOP slot: the track ends now (its head away).
OPSTOP: LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3
        ANDI,R0 15
        COMI,R0 NTRACK
        BCFR,LT OPS9
        STRZ    R1
        LODA,R0 TOFF,R1
        STRA,R0 PTMP
        BSTA,UN TROFF
        BSTA,UN TKILL
OPS9:   RETC,UN

; TKILL: track PTMP off. Calls one down. Clobbers R0-R3.
TKILL:  LODA,R3 PTMP
        LODA,R0 *TP0,R3
        ANDI,R0 ~(TK_ON|TK_DRAWN)&$FF
        CPUTR   TRACKS
        RETC,UN

; TRKSTEP: every live track a step (view.go animate), in slot order: none
; left: its head away, it ends; else X += VX, Y += VY, VX += AX, VY += AY
; (24-bit), n -= 1, streamed back; its head away and drawn again (the
; erasing is a block's, no matter that the step came first). Calls four down.
; Clobbers R0-R3, PTMP, EY.
TRKSTEP: EORZ   R0
        STRA,R0 EY
TS1:    LODA,R1 EY
        LODA,R0 TOFF,R1
        STRA,R0 PTMP
        STRZ    R3
        LODA,R0 *TP0,R3
        TMI,R0  TK_ON
        BCFA,EQ TS9
        LODA,R0 *TP13,R3        ; n: none left, it ends (its head away)
        IORA,R0 *TP14,R3
        BCFR,EQ TS2
        BSTA,UN TROFF
        BSTA,UN TKILL
        BCTA,UN TS9
TS2:    LODI,R1 FRRAMP2+((TRACKS-$2900)>>8)
        LODA,R0 PTMP            ; the stream at X
        ADDI,R0 1
        STRZ    R3
        BSTA,UN RAMSET
        LODA,R3 PTMP
        LODI,R1 0               ; R1: AX's sign
        LODA,R0 *TP16,R3
        BCFR,LT TS3
        LODI,R1 $FF
TS3:    CPSL    PSLC
        PPSL    PSLWC
        LODA,R0 *TP1,R3         ; X += VX
        ADDA,R0 *TP7,R3
        STRZ    R2
        LODA,R0 *PRDAT,R2
        LODA,R0 *TP2,R3
        ADDA,R0 *TP8,R3
        STRZ    R2
        LODA,R0 *PRDAT,R2
        LODA,R0 *TP3,R3
        ADDA,R0 *TP9,R3
        STRZ    R2
        LODA,R0 *PRDAT,R2
        CPSL    PSLC            ; Y += VY
        LODA,R0 *TP4,R3
        ADDA,R0 *TP10,R3
        STRZ    R2
        LODA,R0 *PRDAT,R2
        LODA,R0 *TP5,R3
        ADDA,R0 *TP11,R3
        STRZ    R2
        LODA,R0 *PRDAT,R2
        LODA,R0 *TP6,R3
        ADDA,R0 *TP12,R3
        STRZ    R2
        LODA,R0 *PRDAT,R2
        CPSL    PSLC            ; VX += AX
        LODA,R0 *TP7,R3
        ADDA,R0 *TP15,R3
        STRZ    R2
        LODA,R0 *PRDAT,R2
        LODA,R0 *TP8,R3
        ADDA,R0 *TP16,R3
        STRZ    R2
        LODA,R0 *PRDAT,R2
        LODA,R0 *TP9,R3
        ADDZ    R1
        STRZ    R2
        LODA,R0 *PRDAT,R2
        CPSL    PSLWC           ; R1: AY's sign
        LODI,R1 0
        LODA,R0 *TP18,R3
        BCFR,LT TS4
        LODI,R1 $FF
TS4:    CPSL    PSLC            ; VY += AY
        PPSL    PSLWC
        LODA,R0 *TP10,R3
        ADDA,R0 *TP17,R3
        STRZ    R2
        LODA,R0 *PRDAT,R2
        LODA,R0 *TP11,R3
        ADDA,R0 *TP18,R3
        STRZ    R2
        LODA,R0 *PRDAT,R2
        LODA,R0 *TP12,R3
        ADDZ    R1
        STRZ    R2
        LODA,R0 *PRDAT,R2
        PPSL    PSLC            ; n -= 1
        LODA,R0 *TP13,R3
        SUBI,R0 1
        STRZ    R2
        LODA,R0 *PRDAT,R2
        LODA,R0 *TP14,R3
        SUBI,R0 0
        STRZ    R2
        LODA,R0 *PRDAT,R2
        CPSL    PSLWC|PSLC
        BSTA,UN RAMPEND
        BSTA,UN TPIX            ; the head on the same block, drawn, still fire
        COMI,R0 0               ;  there: its erasing and drawing again would
        BCFR,EQ TS8             ;  leave it so (view.go eraseHead, drawHead):
        LODA,R3 PTMP            ;  nothing to do -- the common case, a shell
        LODA,R0 *TP0,R3         ;  moving under a block a frame
        TMI,R0  TK_DRAWN
        BCFR,EQ TS8
        LODA,R0 *TP19,R3
        COMA,R0 BX
        BCFR,EQ TS8
        LODA,R0 *TP20,R3
        COMA,R0 BY
        BCFR,EQ TS8
        LODA,R1 BY
        LODA,R2 BY
        BSTA,UN KTANK
        BSTA,UN KAT
        BSTA,UN KGET
        TMI,R0  $40
        BCFR,EQ TS8
        ANDA,R0 BMSK
        BCFR,EQ TS9
TS8:    BSTA,UN TROFF           ; else its head away, then on again
        BSTA,UN TRON
TS9:    LODA,R0 EY
        ADDI,R0 1
        STRA,R0 EY
        COMI,R0 NTRACK
        BCTA,LT TS1
        RETC,UN

; TPIX: track PTMP's head: BX = X >> 12, BY = Y >> 12; R0 = 0 on the
; field (else not). Leaf. Clobbers R0-R3.
TPIX:   LODA,R3 PTMP
        LODA,R0 *TP3,R3         ; x: its high byte under 16 (0-255 px), then
        ANDI,R0 $F0             ;  (hi << 4) | (mid >> 4) under 48
        BCFR,EQ TPX9
        LODA,R0 *TP3,R3
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        ANDI,R0 $F0
        STRZ    R1
        LODA,R0 *TP2,R3
        RRR,R0
        RRR,R0
        RRR,R0
        RRR,R0
        ANDI,R0 $0F
        IORZ    R1
        COMI,R0 FIELD_W
        BCFR,LT TPX9
        STRA,R0 BX
        LODA,R0 *TP6,R3         ; y the same
        ANDI,R0 $F0
        BCFR,EQ TPX9
        LODA,R0 *TP6,R3
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        ANDI,R0 $F0
        STRZ    R1
        LODA,R0 *TP5,R3
        RRR,R0
        RRR,R0
        RRR,R0
        RRR,R0
        ANDI,R0 $0F
        IORZ    R1
        COMI,R0 FIELD_H
        BCFR,LT TPX9
        STRA,R0 BY
        EORZ    R0
        RETC,UN
TPX9:   LODI,R0 1
        RETC,UN

; TRON: track PTMP's head drawn (view.go drawHead): fire over air (a smoke
; trail's over fire too); drawn (TK_DRAWN, TK_HX, TK_HY) if so. Calls
; four down. Clobbers R0-R3.
TRON:   BSTA,UN TPIX
        COMI,R0 0
        BCFA,EQ TRN9            ; (off the field: nothing to draw)
        LODA,R1 BY
        LODA,R2 BY
        BSTA,UN KTANK
        LODI,R0 FR_SKY
        STRA,R0 BFROM
        LODA,R3 PTMP
        LODA,R0 *TP0,R3
        ANDI,R0 15
        COMI,R0 ST_SMOKE
        BCFR,EQ TRN1
        LODI,R0 FR_SKY|FR_FX
        STRA,R0 BFROM
TRN1:   LODI,R0 PXFX
        STRA,R0 BTO
        BSTA,UN KPLOT
        COMI,R0 0
        BCTR,EQ TRN9
        LODA,R0 BX              ; drawn: where
        LODA,R3 PTMP
        CPUTR   TRACKS+TK_HX
        LODA,R0 BY
        LODA,R3 PTMP
        CPUTR   TRACKS+TK_HY
        LODA,R3 PTMP
        LODA,R0 *TP0,R3
        IORI,R0 TK_DRAWN
        CPUTR   TRACKS
TRN9:   RETC,UN

; TROFF: track PTMP's head away (view.go eraseHead): air again if it is
; still fire (a smoke trail stays). Calls four down. Clobbers R0-R3.
TROFF:  LODA,R3 PTMP
        LODA,R0 *TP0,R3
        TMI,R0  TK_DRAWN
        RETC,LT
        ANDI,R0 ~TK_DRAWN&$FF
        CPUTR   TRACKS
        LODA,R3 PTMP
        LODA,R0 *TP0,R3
        ANDI,R0 15
        COMI,R0 ST_SMOKE
        RETC,EQ
        LODA,R0 *TP19,R3
        STRA,R0 BX
        LODA,R0 *TP20,R3
        STRA,R0 BY
        STRZ    R1
        STRZ    R2
        BSTA,UN KTANK
        LODI,R0 FR_FX
        STRA,R0 BFROM
        LODI,R0 PXSKY
        STRA,R0 BTO
        BCTA,UN KPLOT           ; tail call


        INCLUDE "src/blk.inc"
