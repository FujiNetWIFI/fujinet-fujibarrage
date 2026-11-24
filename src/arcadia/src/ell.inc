; ell.inc -- a row's span and an ellipse of the field's blocks (the BLS
; page's: the blasts' fireballs, craters and dirt balls), over blk.inc's
; cells: view.go's ellipse to the block, a ring at a time as a blast
; grows.

; r squared, 16-bit (the ellipse's radii, 0-24)
KSQL:   DB      0,1,4,9,16,25,36,49,64,81,100,121,144,169,196,225
        DB      0,33,68,105,144,185,228,17,64
KSQH:   DB      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
        DB      1,1,1,1,1,1,1,2,2
KPTMPW: DWBE    TMPW            ; RAM-H through pointer words (page 0 code)

; ---- a row's span -----------------------------------------------------------------
; KSPAN: blocks SX0 to SX1 (signed; clipped to the field) of row BY (none
; off the field) by BFROM, BTO: a cell at a time. Calls four down.
; Clobbers R0-R3, BX, PTMP, SX0.
KSPAN:  LODA,R2 BY
        COMI,R2 FIELD_H
        RETC,GT
        RETC,EQ
        LODA,R0 KROW,R2         ; PTMP: the row's code (index, half, bottom)
        STRA,R0 PTMP
        ANDI,R0 1
        STRA,R0 BHALF
        LODA,R0 SX1             ; x1: none left of the field, at most 47
        TMI,R0  $80
        RETC,EQ
        COMI,R0 FIELD_W
        BCTR,LT KSP1
        LODI,R0 FIELD_W-1
        STRA,R0 SX1
KSP1:   LODA,R0 SX0             ; x0: none right of it, at least 0
        TMI,R0  $80
        BCFR,EQ KSP2
        EORZ    R0
        STRA,R0 SX0
KSP2:   COMI,R0 FIELD_W
        RETC,GT
        RETC,EQ
        COMA,R0 SX1
        RETC,GT
KSP3:   LODA,R1 SX0             ; the cell of x0: its blocks from x0 on
        LODA,R0 KCOL,R1
        STRA,R0 BX
        LODA,R0 KBIT,R1         ; (b: 2b - 1)
        ADDZ    R0
        SUBI,R0 1
        STRA,R0 BMSK
        LODA,R1 SX1             ; x1 in it too: up to x1 (8 - b)
        LODA,R0 KCOL,R1
        COMA,R0 BX
        BCFR,EQ KSP4
        LODI,R0 8
        SUBA,R0 KBIT,R1
        ANDA,R0 BMSK
        STRA,R0 BMSK
KSP4:   LODA,R0 PTMP            ; the bottom row: 3 higher
        TMI,R0  2
        BCFR,EQ KSP5
        LODA,R0 BMSK
        RRL,R0
        RRL,R0
        RRL,R0
        STRA,R0 BMSK
        LODA,R0 PTMP
KSP5:   ANDI,R0 $F0
        ADDA,R0 BX
        STRA,R0 BIDX
        BSTA,UN KMOD
        LODA,R1 SX1             ; the last cell?
        LODA,R0 KCOL,R1
        COMA,R0 BX
        RETC,EQ
        LODA,R0 BX              ; the next cell's first block: 3 (cell + 1)
        ADDI,R0 1
        STRZ    R1
        ADDZ    R0
        ADDZ    R1
        STRA,R0 SX0
        BCTA,UN KSP3

; ---- an ellipse ---------------------------------------------------------------------
; KELL: the blocks of the ellipse at (ECX, ECY), radii ERXN, ERYN (cart
; RAM: 0-24), by BFROM, BTO -- view.go's: x^2 ry^2 + y^2 rx^2 <=
; rx^2 ry^2 -- less the old one inside it (radii ERX, ERY now, ERYO its ry;
; ERYO $FF: none): a ring as a blast grows. The old one's half widths
; first (TMPW), then the new one's rows, each drawn as the spans out of
; the old one. Calls six down. Clobbers R0-R3, the E* scratch.
KELL:   LODA,R0 ERYO
        COMI,R0 $FF
        BCTR,EQ KEL1
        EORZ    R0              ; the old one's widths (a flow: KW5 comes back
        STRA,R0 EPASS           ;  to KEL1 -- a call here would cost the depth)
        BCTA,UN KWALK
KEL1:   LODA,R0 *KPRXN          ; the new one, drawn
        STRA,R0 ERX
        LODA,R0 *KPRYN
        STRA,R0 ERY
        LODI,R0 1
        STRA,R0 EPASS
; KWALK: the ellipse ERX, ERY a row at a time (y 0 to ry; x its half width:
; F = rx^2 (ry^2 - y^2) - x^2 ry^2 >= 0 at the widest x): EPASS 0 keeps
; each row's x in TMPW, 1 draws them (KROWS). Calls five down.
KWALK:  LODA,R3 ERY             ; 2B = 2 ry^2
        LODA,R0 KSQL,R3
        STRA,R0 E2BL
        LODA,R0 KSQH,R3
        STRA,R0 E2BH
        LODA,R3 ERX             ; dR = A = rx^2 (F's drop to row 1); 2A
        LODA,R0 KSQL,R3
        STRA,R0 EDRL
        STRA,R0 E2AL
        LODA,R0 KSQH,R3
        STRA,R0 EDRH
        STRA,R0 E2AH
        BSTA,UN KADA            ; 2A, 2B doubled
        LODA,R0 E2BL
        ADDA,R0 E2BL
        STRA,R0 E2BL
        LODA,R0 E2BH
        PPSL    PSLWC
        ADDA,R0 E2BH
        STRA,R0 E2BH
        CPSL    PSLWC|PSLC
        EORZ    R0              ; F = 0, y = 0, x = rx
        STRA,R0 EFL
        STRA,R0 EFH
        STRA,R0 EY
        STRA,R0 EDLL            ; dL = B (2 rx - 1): 2B rx times, less B
        STRA,R0 EDLH
        LODA,R1 ERX
        STRA,R1 EX
        BCTR,EQ KW2
KW1:    BSTA,UN KADB
        BDRR,R1 KW1
KW2:    PPSL    PSLWC|PSLC
        LODA,R3 ERY
        LODA,R0 EDLL
        SUBA,R0 KSQL,R3
        STRA,R0 EDLL
        LODA,R0 EDLH
        SUBA,R0 KSQH,R3
        STRA,R0 EDLH
        CPSL    PSLWC|PSLC
        LODA,R0 EPASS           ; the old one's: TMPW's stream
        BCFR,EQ KW3
        LODI,R1 FRRAMP2+((TMPW-$2900)>>8)
        LODI,R3 (TMPW-$2900)&$FF
        BSTA,UN RAMSET
KW3:    LODA,R0 EPASS           ; the row
        BCFR,EQ KW4
        LODA,R2 EX
        LODA,R0 *PRDAT,R2       ; (the stream: TMPW[y] = x)
        BCTR,UN KW5
KW4:    BSTA,UN KROWS
KW5:    LODA,R0 EY              ; the last row?
        COMA,R0 ERY
        BCTR,LT KW6
        LODA,R0 EPASS
        BCFA,EQ KW9
        BSTA,UN RAMPEND         ; the old one's done: the new one
        BCTA,UN KEL1
KW6:    ADDI,R0 1
        STRA,R0 EY
        PPSL    PSLWC|PSLC      ; F -= dR
        LODA,R0 EFL
        SUBA,R0 EDRL
        STRA,R0 EFL
        LODA,R0 EFH
        SUBA,R0 EDRH
        STRA,R0 EFH
        CPSL    PSLWC|PSLC      ; dR += 2A
        PPSL    PSLWC
        LODA,R0 EDRL
        ADDA,R0 E2AL
        STRA,R0 EDRL
        LODA,R0 EDRH
        ADDA,R0 E2AH
        STRA,R0 EDRH
        CPSL    PSLWC
KW7:    LODA,R0 EFH             ; F < 0: x in a step (F += dL, dL -= 2B)
        TMI,R0  $80
        BCFA,EQ KW3
        LODA,R0 EX
        BCTA,EQ KW3             ; (x 0: as narrow as it goes)
        SUBI,R0 1
        STRA,R0 EX
        CPSL    PSLC
        PPSL    PSLWC
        LODA,R0 EFL
        ADDA,R0 EDLL
        STRA,R0 EFL
        LODA,R0 EFH
        ADDA,R0 EDLH
        STRA,R0 EFH
        CPSL    PSLWC
        PPSL    PSLWC|PSLC
        LODA,R0 EDLL
        SUBA,R0 E2BL
        STRA,R0 EDLL
        LODA,R0 EDLH
        SUBA,R0 E2BH
        STRA,R0 EDLH
        CPSL    PSLWC|PSLC
        BCTA,UN KW7
KW9:    RETC,UN
; KADB: dL += 2B. KADA: 2A = A + A. Leaves. Clobber R0.
KADB:   CPSL    PSLC
        PPSL    PSLWC
        LODA,R0 EDLL
        ADDA,R0 E2BL
        STRA,R0 EDLL
        LODA,R0 EDLH
        ADDA,R0 E2BH
        STRA,R0 EDLH
        CPSL    PSLWC
        RETC,UN
KADA:   CPSL    PSLC
        LODA,R0 E2AL
        ADDA,R0 E2AL
        STRA,R0 E2AL
        PPSL    PSLWC
        LODA,R0 E2AH
        ADDA,R0 E2AH
        STRA,R0 E2AH
        CPSL    PSLWC
        CPSL    PSLC
        RETC,UN
KPRXN:  DWBE    ERXN
KPRYN:  DWBE    ERYN

; KROWS: the rows ECY - EY and ECY + EY (once at y 0) of the ellipse: from
; -EX to EX about ECX, less the old one's (TMPW[EY], for EY <= ERYO): two
; spans then. Calls three down. Clobbers R0-R3.
KROWS:  LODA,R0 ECY
        SUBA,R0 EY
KRWR:   STRA,R0 BY
        LODA,R0 ERYO            ; the old one has this row?
        COMI,R0 $FF
        BCTR,EQ KRW2
        COMA,R0 EY
        BCTR,LT KRW2
        BSTA,UN KRWO            ; its half width: the spans outside it
        COMA,R0 EX
        BCFR,LT KRWN
        LODA,R0 ECX             ; the left: cx - x .. cx - wo - 1
        SUBA,R0 EX
        STRA,R0 SX0
        BSTA,UN KRWO
        EORI,R0 $FF             ; (- wo - 1)
        ADDA,R0 ECX
        STRA,R0 SX1
        BSTA,UN KSPAN
        BSTR,UN KRWO            ; the right: cx + wo + 1 ..
        ADDA,R0 ECX
        ADDI,R0 1
        STRA,R0 SX0
        BCTR,UN KRW3
KRW2:   LODA,R0 ECX             ; the whole row
        SUBA,R0 EX
        STRA,R0 SX0
KRW3:   LODA,R0 ECX             ;  .. cx + x
        ADDA,R0 EX
        STRA,R0 SX1
        BSTA,UN KSPAN
KRWN:   LODA,R0 EY              ; the other row (none at y 0)
        RETC,EQ
        LODA,R0 ECY
        SUBA,R0 EY
        COMA,R0 BY
        RETC,LT                 ; (unsigned: any difference)
        RETC,GT
        LODA,R0 ECY
        ADDA,R0 EY
        BCTA,UN KRWR
; KRWO: R0 = the old one's half width at row EY. Leaf. Clobbers R3.
KRWO:   LODA,R3 EY
        LODA,R0 *KPTMPW,R3
        RETC,UN

