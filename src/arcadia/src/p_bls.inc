; p_bls.inc -- the shot's blasts drawn (view.go's ellipses: a fireball
; growing, its crater or flash at its end; a dirt ball), a page of their
; own with the ellipse (ell.inc) and the field's blocks (blk.inc). The PLY
; page keeps the blasts (OP_EXPLODE, OP_DIRT; n each frame) and calls
; these (FARPG) for blast PBLK: E_BGROW draws it from its size drawn (dn)
; to now (n), E_BEND its end. KELL runs at the entry's own level: it is
; deep (the spans, the cells).

        BCTA,UN BGROW           ; E_BGROW
        BCTA,UN BEND            ; E_BEND

DIRTBALL EQU    $80             ; (p_ply.inc's: an OP_DIRT)
PPBLK:  DWBE    PBLK
; a blast's bytes (R3 its offset, 8 b)
BP0:    DWBE    BLASTS          ; kind ($80: dirt)
BP1:    DWBE    BLASTS+1        ; n: its frames so far
BP2:    DWBE    BLASTS+2        ; tot: its frames (0: free)
BP3:    DWBE    BLASTS+3        ; rx, ry
BP4:    DWBE    BLASTS+4
BP5:    DWBE    BLASTS+5        ; dn: n as drawn
BP6:    DWBE    BLASTS+6        ; x, y (signed, -32..95)
BP7:    DWBE    BLASTS+7

BGROW:  BSTR,UN BRSET           ; (nothing new: R0 0)
        COMI,R0 0
        RETC,EQ
        BSTA,UN KTANKE
        BCTA,UN KELL            ; (KELL's return is the far call's)
BEND:   BSTA,UN BESET
        COMI,R0 0
        RETC,EQ
        BSTA,UN KTANKE
        BCTA,UN KELL

; BRSET: blast PBLK grown from its size drawn (dn) to now (n): KELL's
; inputs (ECX, ECY; the new radii ERXN, ERYN, each max(1, r n / tot); the
; old ones ERX, ERY, ERYO, or ERYO $FF if none drawn yet; the kinds: a
; crater's fireball takes air and dirt, a flash's (plasma, a puff) air,
; dirt fills air and fire). R0 = 0 if nothing new. Calls two down.
BRSET:  LODA,R3 *PPBLK
        LODA,R0 *BP6,R3
        STRA,R0 ECX
        LODA,R0 *BP7,R3
        STRA,R0 ECY
        LODA,R0 *BP0,R3         ; the kinds
        LODI,R1 FR_SKY|FR_DIRT
        LODI,R2 PXFX
        COMI,R0 DIRTBALL
        BCFR,EQ BRS1
        LODI,R1 FR_SKY|FR_FX
        LODI,R2 PXDIRT
        BCTR,UN BRS2
BRS1:   COMI,R0 EX_PLASMA
        BCTR,EQ BRS1A
        COMI,R0 EX_PUFF
        BCFR,EQ BRS2
BRS1A:  LODI,R1 FR_SKY
BRS2:   STRA,R1 BFROM
        STRA,R2 BTO
        LODA,R0 *BP1,R3         ; the new radii
        STRA,R0 SX0
        LODA,R0 *BP3,R3
        STRZ    R1
        BSTA,UN BSCALE
        CPUT    ERXN
        LODA,R3 *PPBLK
        LODA,R0 *BP4,R3
        STRZ    R1
        BSTA,UN BSCALE
        CPUT    ERYN
        LODI,R0 $FF             ; the old ones
        STRA,R0 ERYO
        LODA,R3 *PPBLK
        LODA,R0 *BP5,R3
        BCTR,EQ BRS9
        STRA,R0 SX0
        LODA,R0 *BP3,R3
        STRZ    R1
        BSTA,UN BSCALE
        STRA,R0 ERX
        LODA,R3 *PPBLK
        LODA,R0 *BP4,R3
        STRZ    R1
        BSTA,UN BSCALE
        STRA,R0 ERY
        STRA,R0 ERYO
        LODA,R0 ERX             ; the same size: nothing new
        COMA,R0 *PERXN
        BCFR,EQ BRS9
        LODA,R0 ERY
        COMA,R0 *PERYN
        BCFR,EQ BRS9
        EORZ    R0
        RETC,UN
BRS9:   LODI,R0 1
        RETC,UN
PERXN:  DWBE    ERXN
PERYN:  DWBE    ERYN
; BSCALE: R0 = max(1, R1 * SX0 / tot) of blast PBLK (SX0 <= tot). Leaf.
; Clobbers R1-R3.
BSCALE: LODA,R3 *PPBLK
        LODA,R0 *BP2,R3
        STRA,R0 SX1             ; tot
        LODI,R2 0               ; R2: the remainder, R3: the quotient
        LODI,R3 0
        COMI,R1 0
        BCTR,EQ BSC3
BSC1:   LODZ    R2              ; r times: rem += n, past tot: q += 1
        ADDA,R0 SX0
        STRZ    R2
        TPSL    PSLC
        BCTR,EQ BSC2
        COMA,R0 SX1
        BCTR,LT BSC2A
BSC2:   SUBA,R0 SX1
        STRZ    R2
        ADDI,R3 1
BSC2A:  BDRR,R1 BSC1
BSC3:   LODZ    R3
        RETC,GT
        LODI,R0 1
        RETC,UN

; BESET: blast PBLK's end, KELL's inputs (its own radii, no old one): a
; crater takes everything to air, a flash its fire; dirt has none (R0 0).
; Leaf (and CPUT).
BESET:  LODA,R3 *PPBLK
        LODA,R0 *BP0,R3
        COMI,R0 DIRTBALL
        BCFR,EQ BES1
        EORZ    R0
        RETC,UN
BES1:   LODI,R1 FR_SKY|FR_DIRT|FR_FX
        COMI,R0 EX_PLASMA
        BCTR,EQ BES2
        COMI,R0 EX_PUFF
        BCFR,EQ BES3
BES2:   LODI,R1 FR_FX
BES3:   STRA,R1 BFROM
        LODI,R0 PXSKY
        STRA,R0 BTO
        LODA,R0 *BP6,R3
        STRA,R0 ECX
        LODA,R0 *BP7,R3
        STRA,R0 ECY
        LODI,R0 $FF
        STRA,R0 ERYO
        LODA,R0 *BP3,R3
        CPUT    ERXN
        LODA,R3 *PPBLK
        LODA,R0 *BP4,R3
        CPUT    ERYN
        LODI,R0 1
        RETC,UN

; KTANKE: KTANK for the ellipse's rows (ECY +- ERYN). Calls three down.
KTANKE: LODA,R0 ECY
        SUBA,R0 *PERYN
        STRZ    R1
        LODA,R0 ECY
        ADDA,R0 *PERYN
        STRZ    R2
        BCTA,UN KTANK           ; tail call


        INCLUDE "src/ell.inc"
        INCLUDE "src/blk.inc"
