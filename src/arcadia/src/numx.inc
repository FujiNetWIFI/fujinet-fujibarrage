; numx.inc -- a number of up to 32 bits in decimal, a string right-aligned
; (p_shop.inc, p_sets.inc include it: a page can't call another page's code).

; NUMX: N32 (32-bit, little-endian, in TZ) in decimal, right-aligned in R1
; cells, R0 decimals (0-2: a '.' before the last R0 digits, at least one
; digit before it: cat.c cat_value's 0.05), in DCOLOR at the next cells
; (DPUTC: R2). N32 is used up. Calls two down. Clobbers R0, R1, R3, T0-T3,
; N32, DIG; advances R2.
NUMX:   STRA,R0 T0              ; T0: the decimals, T1: the width
        STRA,R1 T1
        STRA,R2 T2              ; (the display's cell)
        LODI,R3 0               ; R3: 10^k's offset in NXPOW (from 10^9)
NX1:    EORZ    R0              ; T3: the digit
        STRA,R0 T3
NX2:    LODA,R0 N32+3           ; N32 >= 10^k? (from the top byte)
        COMA,R0 NXPOW+3,R3
        BCTA,LT NX5
        BCTR,GT NX3
        LODA,R0 N32+2
        COMA,R0 NXPOW+2,R3
        BCTA,LT NX5
        BCTR,GT NX3
        LODA,R0 N32+1
        COMA,R0 NXPOW+1,R3
        BCTA,LT NX5
        BCTR,GT NX3
        LODA,R0 N32
        COMA,R0 NXPOW,R3
        BCTA,LT NX5
NX3:    PPSL    PSLWC|PSLC      ; N32 -= 10^k: the digit one more
        LODA,R0 N32
        SUBA,R0 NXPOW,R3
        STRA,R0 N32
        LODA,R0 N32+1
        SUBA,R0 NXPOW+1,R3
        STRA,R0 N32+1
        LODA,R0 N32+2
        SUBA,R0 NXPOW+2,R3
        STRA,R0 N32+2
        LODA,R0 N32+3
        SUBA,R0 NXPOW+3,R3
        STRA,R0 N32+3
        CPSL    PSLWC|PSLC
        LODA,R0 T3
        ADDI,R0 1
        STRA,R0 T3
        BCTA,UN NX2
NX5:    LODZ    R3              ; DIG[k] = it
        RRR,R0
        RRR,R0
        ANDI,R0 $3F
        STRZ    R1
        LODA,R0 T3
        STRA,R0 DIG,R1
        ADDI,R3 4
        COMI,R3 40
        BCFA,EQ NX1
        LODI,R0 9               ; the first digit shown: the first not 0, at
        SUBA,R0 T0              ;  most 9 - the decimals (T3)
        STRA,R0 T3
        LODI,R3 0
NX6:    COMA,R3 T3
        BCFR,LT NX7
        LODA,R0 DIG,R3
        BCFR,EQ NX7
        ADDI,R3 1
        BCTR,UN NX6
NX7:    LODI,R0 10              ; its characters: 10 - f, and the point
        SUBZ    R3
        LODA,R1 T0
        BCTR,EQ NX8
        ADDI,R0 1
NX8:    STRZ    R1              ; the spaces before: the width - them
        LODA,R0 T1
        SUBZ    R1
        STRZ    R1
        LODA,R2 T2
NX9:    COMI,R1 0
        BCTR,EQ NX11
        BCTR,LT NX11
        EORZ    R0
        ZBSR    *ZDPUTC
        SUBI,R1 1
        BCTR,UN NX9
NX11:   LODA,R0 T0              ; the digits (the point before the last T0)
        BCTR,EQ NX12
        LODI,R0 10
        SUBA,R0 T0
        COMZ    R3
        BCFR,EQ NX12
        LODI,R0 GDOT
        ZBSR    *ZDPUTC
NX12:   LODA,R0 DIG,R3
        IORI,R0 GDIGIT
        ZBSR    *ZDPUTC
        ADDI,R3 1
        COMI,R3 10
        BCFR,EQ NX11
        RETC,UN
; 10^9 .. 10^0, 32-bit little-endian
NXPOW:  DB      $00,$CA,$9A,$3B, $00,$E1,$F5,$05, $80,$96,$98,$00, $40,$42,$0F,$00
        DB      $A0,$86,$01,$00, $10,$27,$00,$00, $E8,$03,$00,$00, $64,$00,$00,$00
        DB      $0A,$00,$00,$00, $01,$00,$00,$00

; N16: N32 = R0:R1 (the high byte, the low). Leaf.
N16:    STRA,R0 N32+1
        STRA,R1 N32
        EORZ    R0
        STRA,R0 N32+2
        STRA,R0 N32+3
        RETC,UN

; RTXT: the glyph string at R0:R1 ($FF ends it) right-aligned in R3
; cells (spaces before it), in DCOLOR at the next cells. Calls two down.
; Clobbers R0, R1, R3; advances R2.
RTXT:   STRA,R0 DSTRH
        STRA,R1 DSTRL
        LODI,R1 $FF             ; R1: its length
RTX1:   LODA,R0 *DSTRH,R1,+
        COMI,R0 $FF
        BCFR,EQ RTX1
        LODZ    R3              ; R3: the spaces before
        SUBZ    R1
        STRZ    R3
RTX2:   COMI,R3 0
        BCTR,EQ RTX3
        BCTR,LT RTX3
        EORZ    R0
        ZBSR    *ZDPUTC
        SUBI,R3 1
        BCTR,UN RTX2
RTX3:   BCTA,UN DTXTS           ; tail call

