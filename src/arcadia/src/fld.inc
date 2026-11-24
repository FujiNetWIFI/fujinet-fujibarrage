; fld.inc -- the field's column primitives: one block column's rows made
; sky or dirt, a whole cell at a time where the rows cover it. Included by
; every page that draws terrain (the decoder's TERRAIN, p_net.inc; M3's
; settles): page code reaches the screen directly (fixed B could only
; through pointer words, a third slower).
;
; The column: GX its cell column (0-15), GMT its top-row block's bit (4, 2,
; 1: the cell's left, middle, right third; the bottom row's is GMT x 8).
; A cell here is sky and dirt only ($80 | its dirt blocks, all dirt $00):
; nothing HOT may be in it (the decoder takes the block tanks off first;
; the shot player's settles have their own HOT-aware writer: p_set.inc).
; A smoke trail's fire left on the field outlives the shot: a terrain
; frame over it later (a whole field asked for again mid-round) ORs its
; dirt into the HOT blocks -- rare, and the next round's field is fresh.
; The rows: 0-47, two to a cell row (24 of them: 13 at SCR, 11 at SCR2).

; FILLS: rows R1 to R2 - 1 (R2 past 48: 48) of the column sky. FILLD: dirt.
; Nothing if R1 >= R2. Calls one down. Clobbers R0-R3, T0-T3, NUMD.
FILLS:  EORZ    R0
        BCTR,UN FIL0
FILLD:  LODI,R0 1
FIL0:   STRA,R0 T1              ; T1: the kind (0 sky)
        COMI,R2 FIELD_H
        BCTA,LT FIL1
        LODI,R2 FIELD_H
FIL1:   LODZ    R1
        COMZ    R2
        RETC,GT                 ; (from >= to: none)
        RETC,EQ
        STRA,R1 T3              ; T3: from; T0: to's cell row (whole cells end)
        LODZ    R2
        ANDI,R0 $FE
        RRR,R0
        STRA,R0 T0
        LODZ    R2              ; to odd: its cell's top row
        ANDI,R0 1
        BCTR,EQ FIL2
        LODA,R0 T0
        STRA,R0 T2
        ADDI,R0 1
        STRZ    R2
        LODA,R1 GMT
        BSTA,UN CELLOP
FIL2:   LODA,R0 T3              ; from odd: its cell's bottom row
        ANDI,R0 1
        BCTA,EQ FIL3
        LODA,R0 T3
        ANDI,R0 $FE
        RRR,R0
        STRA,R0 T2
        ADDI,R0 1
        STRZ    R2
        LODA,R0 GMT
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R1
        BSTA,UN CELLOP
        LODA,R0 T3
        ADDI,R0 1
        STRA,R0 T3
FIL3:   LODA,R0 T3              ; the whole cells between
        RRR,R0                  ; (from even now)
        STRA,R0 T2
        COMA,R0 T0
        RETC,GT
        RETC,EQ
        LODA,R0 GMT
        STRZ    R1
        RRL,R0
        RRL,R0
        RRL,R0
        IORZ    R1
        STRZ    R1
        LODA,R2 T0              ; tail call
; CELLOP: the column's blocks R1 (a mask) in cell rows T2 to R2 - 1 made
; the kind T1 (0 sky, else dirt). Leaf. Clobbers R0-R3, T2, NUMD.
CELLOP: STRA,R2 NUMD            ; NUMD: the rows' end
        LODA,R0 T1
        BCFA,EQ CEO1
        EORI,R1 $FF             ; (sky: the mask's complement, ANDed)
CEO1:   LODA,R0 T2              ; rows 0-12 (SCR) first
        COMI,R0 13
        BCFA,LT CEO3
        LODA,R0 NUMD            ; up to min(end, 13)
        COMI,R0 13
        BCTR,LT CEO2
        LODI,R0 13
CEO2:   SUBA,R0 T2
        STRZ    R2
        LODA,R0 T2
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        ADDA,R0 GX
        STRZ    R3
        LODA,R0 T1
        BCTR,EQ CSK1
CDI1:   LODA,R0 SCR,R3          ; dirt (SCR): the blocks set; all dirt is $00
        BCTR,EQ CDI1B
        IORZ    R1
        COMI,R0 CDIRTP
        BCFA,EQ CDI1A
        EORZ    R0
CDI1A:  STRA,R0 SCR,R3
CDI1B:  ADDI,R3 16
        BDRR,R2 CDI1
        BCTR,UN CEO2A
CSK1:   LODA,R0 SCR,R3          ; sky (SCR): the blocks cleared
        BCFA,EQ CSK1A
        LODI,R0 CDIRTP
CSK1A:  ANDZ    R1
        STRA,R0 SCR,R3
        ADDI,R3 16
        BDRR,R2 CSK1
CEO2A:  LODI,R0 13
        STRA,R0 T2
CEO3:   LODA,R0 NUMD            ; rows 13-23 (SCR2): end - max(r, 13)
        SUBA,R0 T2
        RETC,LT
        RETC,EQ
        STRZ    R2
        LODA,R0 T2
        SUBI,R0 13
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        ADDA,R0 GX
        STRZ    R3
        LODA,R0 T1
        BCTA,EQ CSK2
CDI2:   LODA,R0 SCR2,R3
        BCTR,EQ CDI2B
        IORZ    R1
        COMI,R0 CDIRTP
        BCFA,EQ CDI2A
        EORZ    R0
CDI2A:  STRA,R0 SCR2,R3
CDI2B:  ADDI,R3 16
        BDRR,R2 CDI2
        RETC,UN
CSK2:   LODA,R0 SCR2,R3
        BCFR,EQ CSK2A
        LODI,R0 CDIRTP
CSK2A:  ANDZ    R1
        STRA,R0 SCR2,R3
        ADDI,R3 16
        BDRR,R2 CSK2
        RETC,UN
