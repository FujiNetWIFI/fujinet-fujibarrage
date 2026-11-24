; blk.inc -- the field's blocks for the shot player (p_ply.inc, p_ops.inc
; include it: each page its own copy, as fld.inc). A block is sky, dirt or
; fire (view.go's PxSky, PxDirt, PxFx); the screen holds them (equ.inc):
; a cell is 3x2 blocks, b6 its ink (HOT), b7 its ground (SKY), and its
; code is tools/arc/mock_arc.py's Mock.build -- sky only $80, dirt only
; $00, sky and dirt $80 | the dirt blocks, HOT and sky $C0 | the HOT ones,
; HOT and dirt $40 | the HOT ones, all HOT $FF; with all three the HOT
; blocks and the majority of the rest as the ground (sky if as many), the
; true dirt blocks kept in CELL3 (C3N of them) so that they come back
; when the fire goes.
;
; The block tanks are taken off the field (TKOFF, fixed B: KTANK) before
; anything here reads or writes a cell row they are on: their HOT blocks
; aren't the field's (p_ply.inc draws them again after the pass).
;
; Scratch: PLYSCR's B*, SX*, E* and the borrowed E* (ram.inc).

; ---- the tables ---------------------------------------------------------------------
; a block column's cell column (x / 3) and its bit in the cell's top row
KCOL:   DB      0,0,0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,6,7,7,7
        DB      8,8,8,9,9,9,10,10,10,11,11,11,12,12,12,13,13,13,14,14,14,15,15,15
KBIT:   DB      4,2,1,4,2,1,4,2,1,4,2,1,4,2,1,4,2,1,4,2,1,4,2,1
        DB      4,2,1,4,2,1,4,2,1,4,2,1,4,2,1,4,2,1,4,2,1,4,2,1
; a block row's cell row: its index in its half (b4-7, 16 a row), its half
; (b0: SCR2), the cell's bottom row (b1)
KROW:   DB      $00,$02,$10,$12,$20,$22,$30,$32,$40,$42,$50,$52,$60,$62
        DB      $70,$72,$80,$82,$90,$92,$A0,$A2,$B0,$B2,$C0,$C2
        DB      $01,$03,$11,$13,$21,$23,$31,$33,$41,$43,$51,$53,$61,$63
        DB      $71,$73,$81,$83,$91,$93,$A1,$A3
; the bits set in a 6-bit mask
KPOP:   DB      0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5
        DB      1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6

; ---- a block ------------------------------------------------------------------------
; KAT: block (BX, BY)'s cell (BIDX, BHALF) and bit (BMSK). R0 = 0 and CC
; EQ on the field, else not. Leaf. Clobbers R0-R3.
KAT:    LODA,R2 BY
        COMI,R2 FIELD_H
        BCFR,LT KAT9            ; (a negative one too: unsigned)
        LODA,R1 BX
        COMI,R1 FIELD_W
        BCFR,LT KAT9
        LODA,R0 KROW,R2
        STRZ    R3
        ANDI,R0 1
        STRA,R0 BHALF
        LODZ    R3
        ANDI,R0 $F0
        ADDA,R0 KCOL,R1
        STRA,R0 BIDX
        LODA,R0 KBIT,R1
        TMI,R3  2               ; the cell's bottom row: 3 higher
        BCFR,EQ KAT1
        RRL,R0
        RRL,R0
        RRL,R0
KAT1:   STRA,R0 BMSK
        EORZ    R0
        RETC,UN
KAT9:   LODI,R0 1
        RETC,UN

; KGET: R0 = the cell's code (BIDX, BHALF). KPUT: R0 into it. Leaves.
; KGET clobbers R3; KPUT R1, R3.
KGET:   LODA,R3 BIDX
        LODA,R0 BHALF
        BCFR,EQ KG2
        LODA,R0 SCR,R3
        RETC,UN
KG2:    LODA,R0 SCR2,R3
        RETC,UN
KPUT:   LODA,R3 BIDX
        LODA,R1 BHALF
        BCFR,EQ KP2
        STRA,R0 SCR,R3
        RETC,UN
KP2:    STRA,R0 SCR2,R3
        RETC,UN

; KDEC: code R0 decoded: BH its HOT blocks, BD its dirt blocks (none of
; them HOT; the rest sky), BC3 its CELL3 entry (2k, $FF none). Leaf (the
; CELL3 search inline: the depth). Clobbers R0-R3.
KDEC:   LODI,R1 $FF
        STRA,R1 BC3
        TMI,R0  $40
        BCTR,EQ KDC2
        LODI,R1 0               ; no HOT: sky and dirt ($00 all dirt)
        STRA,R1 BH
        COMI,R0 0
        BCFR,EQ KDC1
        LODI,R0 $3F
KDC1:   ANDI,R0 $3F
        STRA,R0 BD
        RETC,UN
KDC2:   STRZ    R2              ; HOT: the rest from CELL3 (all three kinds),
        ANDI,R0 $3F             ;  else the ground
        STRA,R0 BH
        LODA,R1 C3N             ; CELL3's entries: this cell's?
        BCTR,EQ KDC4
        LODI,R3 $FF
KDC3:   LODA,R0 CELL3,R3,+      ; (R3: the entry's first byte)
        COMA,R0 BIDX
        BCFR,EQ KDC3A
        LODA,R0 CELL3+1,R3      ; its half: b7
        ANDI,R0 $80
        BCTR,EQ KDC3B
        LODA,R0 BHALF
        BCTR,EQ KDC3A
        BCTR,UN KDC3C
KDC3B:  LODA,R0 BHALF
        BCFR,EQ KDC3A
KDC3C:  STRA,R3 BC3             ; found: its dirt
        LODA,R0 CELL3+1,R3
        ANDI,R0 $3F
        BCTR,UN KDC5
KDC3A:  ADDI,R3 1
        BDRR,R1 KDC3
KDC4:   TMI,R2  $80
        BCTR,EQ KDC6
        LODA,R0 BH              ; dirt ground: the rest dirt
        EORI,R0 $3F
KDC5:   STRA,R0 BD
        RETC,UN
KDC6:   EORZ    R0              ; sky ground
        STRA,R0 BD
        RETC,UN

; KENC: BH, BD encoded into the cell (KPUT), CELL3 kept (BC3 the entry
; KDEC found). Calls one down. Clobbers R0-R3.
KENC:   LODA,R0 BH
        BCFR,EQ KE2
        BSTA,UN KDEL            ; no HOT: sky and dirt
        LODA,R0 BD
        COMI,R0 $3F
        BCFR,EQ KE1
        EORZ    R0
        BCTA,UN KPUT
KE1:    IORI,R0 $80
        BCTA,UN KPUT
KE2:    LODA,R0 BD
        BCFR,EQ KE3
        BSTA,UN KDEL            ; HOT and sky
        LODA,R0 BH
        IORI,R0 $C0
        BCTA,UN KPUT
KE3:    IORA,R0 BH              ; the sky blocks: none?
        EORI,R0 $3F
        BCFR,EQ KE4
        BSTA,UN KDEL            ; HOT and dirt
        LODA,R0 BH
        IORI,R0 $40
        BCTA,UN KPUT
KE4:    BSTR,UN KADD            ; all three: the dirt kept; the ground the
        LODA,R0 BH              ;  majority (sky if as many)
        IORA,R0 BD
        EORI,R0 $3F
        STRZ    R3
        LODA,R0 KPOP,R3
        STRZ    R2
        LODA,R3 BD
        LODA,R0 KPOP,R3
        COMZ    R2
        BCTR,GT KE5
        LODA,R0 BH
        IORI,R0 $C0
        BCTA,UN KPUT
KE5:    LODA,R0 BH
        IORI,R0 $40
        BCTA,UN KPUT

; KSEL: the cart RAM stream at pointer register R1, offset R3 (fixed B's
; RAMSET, inline: KADD and KDEL are leaves, for the depth). KPUT1: R2 into
; it, on. KEND: the stream closed. (Clobber R0, KSEL R1 too.)
KSEL    MACRO
        LODA,R0 *PRSEL,R1
        LODA,R0 *PRDAT,R3
        LODI,R1 FRRAMPK
        LODA,R0 *PRSEL,R1
        ENDM
KPUT1   MACRO
        LODA,R0 *PRDAT,R2
        ENDM
KEND    MACRO
        LODI,R1 FRRAMP0
        LODA,R0 *PRSEL,R1
        ENDM
KC3P    EQU     FRRAMP0+((CELL3-$0600)>>8)      ; CELL3's pointer register,
KC3O    EQU     (CELL3-$0600)&$FF               ;  its offset
KC3NO   EQU     (C3N-$0600)&$FF

; KADD: the cell's dirt BD kept in CELL3 (its entry BC3, or a new one; none
; if full). Leaf. Clobbers R0-R3.
KADD:   LODA,R3 BC3
        COMI,R3 $FF
        BCTR,EQ KA1
        ADDI,R3 KC3O+1          ; found: its second byte again
        LODI,R1 KC3P
        KSEL
        LODA,R0 BHALF
        RRR,R0
        IORA,R0 BD
        STRZ    R2
        KPUT1
        KEND
        RETC,UN
KA1:    LODA,R0 C3N
        COMI,R0 24
        RETC,EQ                 ; (full: the dirt under is lost)
        RRL,R0
        ADDI,R0 KC3O
        STRZ    R3
        LODI,R1 KC3P
        KSEL
        LODA,R2 BIDX
        KPUT1
        LODA,R0 BHALF
        RRR,R0
        IORA,R0 BD
        STRZ    R2
        KPUT1
        LODA,R2 C3N             ; C3N + 1
        ADDI,R2 1
        LODI,R1 KC3P
        LODI,R3 KC3NO
        KSEL
        KPUT1
        KEND
        RETC,UN

; KDEL: the cell's CELL3 entry (BC3) gone: the last one moved into its
; place. Leaf. Clobbers R0-R3.
KDEL:   LODA,R3 BC3
        COMI,R3 $FF
        RETC,EQ
        LODI,R0 $FF
        STRA,R0 BC3
        LODA,R0 C3N             ; the last one's offset (2 (n - 1))
        SUBI,R0 1
        RRL,R0
        COMZ    R3
        BCTR,EQ KDL1            ; (the last itself)
        ADDI,R3 KC3O            ; its two bytes into the entry's place
        LODI,R1 KC3P
        KSEL
        LODA,R0 C3N
        SUBI,R0 1
        RRL,R0
        STRZ    R3
        LODA,R0 CELL3,R3
        STRZ    R2
        KPUT1
        LODA,R0 CELL3+1,R3
        STRZ    R2
        KPUT1
KDL1:   LODA,R2 C3N             ; C3N - 1
        SUBI,R2 1
        LODI,R1 KC3P
        LODI,R3 KC3NO
        KSEL
        KPUT1
        KEND
        RETC,UN
        IF      (C3N-$0600)>>8 <> (CELL3-$0600)>>8
        ERROR   "C3N and CELL3 must share a cart RAM pointer register"
        ENDIF

; KMOD: the cell's blocks BMSK of the kinds BFROM become BTO. R0 = the
; cell's new code, or 0 if none changed (a block made fire: never 0; test
; R0, not CC). Calls one down (KDEC, KADD, KDEL: leaves). Clobbers R0-R3.
KMOD:   BSTA,UN KGET
        BSTA,UN KDEC
        LODA,R2 BFROM           ; R1: the blocks of the kinds that change
        LODI,R1 0
        TMI,R2  FR_SKY
        BCFR,EQ KM1
        LODA,R0 BH
        IORA,R0 BD
        EORI,R0 $3F
        STRZ    R1
KM1:    TMI,R2  FR_DIRT
        BCFR,EQ KM2
        LODZ    R1
        IORA,R0 BD
        STRZ    R1
KM2:    TMI,R2  FR_FX
        BCFR,EQ KM3
        LODZ    R1
        IORA,R0 BH
        STRZ    R1
KM3:    LODZ    R1
        ANDA,R0 BMSK
        RETC,EQ
        STRZ    R1              ; R1: those; out of BH, BD
        EORI,R0 $FF
        STRZ    R2
        ANDA,R0 BH
        STRA,R0 BH
        LODZ    R2
        ANDA,R0 BD
        STRA,R0 BD
        LODA,R0 BTO             ; into the kind
        COMI,R0 PXDIRT
        BCFR,EQ KM4
        LODZ    R1
        IORA,R0 BD
        STRA,R0 BD
        BCTR,UN KM5
KM4:    COMI,R0 PXFX
        BCFR,EQ KM5
        LODZ    R1
        IORA,R0 BH
        STRA,R0 BH
KM5:    BCTA,UN KENC            ; (tail: R0 the new code)

; KPLOT: block (BX, BY) by BFROM, BTO (nothing off the field). R0 as
; KMOD's (0: off the field). Calls one down. Clobbers R0-R3.
KPLOT:  BSTA,UN KAT
        BCTA,EQ KMOD            ; tail call
        EORZ    R0
        RETC,UN

; ---- the block tanks -----------------------------------------------------------
; KTANK: the block tanks off if one is on block rows R1 to R2 (signed;
; clipped to the field), as their cell rows (TKROWS, p_fld.inc TKDRAW):
; their HOT blocks aren't the field's; they're drawn again after the pass
; (PF_TK). Calls two down. Clobbers R0-R3, BMSK, T0-T3.
KTANK:  LODA,R0 TBN
        RETC,EQ
        LODZ    R1              ; from: at least 0
        TMI,R0  $80
        BCFR,EQ KTK1
        LODI,R1 0
KTK1:    LODZ    R2              ; to: none above the field, at most 47
        TMI,R0  $80
        RETC,EQ
        COMI,R0 FIELD_H
        BCTR,LT KTK2
        LODI,R2 FIELD_H-1
KTK2:    LODZ    R1
        COMI,R0 FIELD_H
        RETC,GT
        RETC,EQ
        COMZ    R2
        RETC,GT
        ANDI,R0 $FE             ; their cell rows
        RRR,R0
        STRZ    R1
        LODZ    R2
        ANDI,R0 $FE
        RRR,R0
        STRZ    R2
KTK3:    LODZ    R1              ; cell row R1: a block tank's?
        ANDI,R0 7
        STRZ    R3
        LODA,R0 KTBIT,R3
        STRA,R0 BMSK
        LODZ    R1
        RRR,R0
        RRR,R0
        RRR,R0
        ANDI,R0 $1F
        STRZ    R3
        LODA,R0 *KPTKR,R3
        ANDA,R0 BMSK
        BCFR,EQ KTK4
        LODZ    R1
        COMZ    R2
        RETC,EQ
        ADDI,R1 1
        BCTR,UN KTK3
KTK4:    LODA,R0 PFLG            ; (drawn again after the pass)
        IORI,R0 PF_TK
        STRA,R0 PFLG
        BCTA,UN TKOFF           ; tail call
KTBIT:  DB      1,2,4,8,$10,$20,$40,$80
KPTKR:  DWBE    TKROWS
