; p_set.inc -- the shot's settles land (OP_SETTLE: view.go Column, each
; column its sky then its dirt), a page of their own with the field's
; blocks (blk.inc). The PLY page keeps them; E_SAPPLY lands settle PBLK,
; E_SNOW an OP_SETTLE with frames 0 (its arguments at PPC).

        BCTA,UN SAPPLY          ; E_SAPPLY
        BCTA,UN SNOW            ; E_SNOW

PPBLK:  DWBE    PBLK
; a settle's (R3 its offset, 5 s)
SP0:    DWBE    SETTLES         ; frames left
SP1:    DWBE    SETTLES+1       ; columns (0: free)
SP2:    DWBE    SETTLES+2       ; x16
SP3:    DWBE    SETTLES+3
SP4:    DWBE    SETTLES+4       ; its entries' pc


; SNOW: the OP_SETTLE at PPC lands now (x16, n; its entries at PPC + 4).
SNOW:   LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3
        STRA,R0 ECX
        LODA,R0 *PSCRIPT,R3,+
        STRA,R0 ECY
        LODA,R0 *PSCRIPT,R3,+
        STRA,R0 ERX
        LODA,R0 PPC
        ADDI,R0 4
        STRA,R0 EY
        BCTA,UN SAPPLY1

; SAPPLY: settle PBLK lands (and is free). SAPPLY1: one from ECX:ECY (x16),
; ERX (n), EY (its entries' pc). Each column (view.go Column): sky, then
; its dirt; the block tanks off first. Calls four down. Clobbers R0-R3,
; the E* bytes.
SAPPLY: LODA,R3 *PPBLK
        LODA,R0 *SP1,R3
        STRA,R0 ERX
        LODA,R0 *SP2,R3
        STRA,R0 ECX
        LODA,R0 *SP3,R3
        STRA,R0 ECY
        LODA,R0 *SP4,R3
        STRA,R0 EY
        EORZ    R0
        CPUTR   SETTLES+1
SAPPLY1: LODA,R0 TBN            ; the block tanks off (every row)
        BCTR,EQ SA1
        BSTA,UN TKOFF
        LODA,R0 PFLG
        IORI,R0 PF_TK
        STRA,R0 PFLG
SA1:    LODA,R0 ERX
        RETC,EQ
SA2:    LODI,R0 $FF             ; ERYO: the column on the field ($FF not)
        STRA,R0 ERYO
        LODA,R0 ECY
        BCFR,EQ SA3
        LODA,R0 ECX
        COMI,R0 FIELD_W
        BCFR,LT SA3
        STRA,R0 ERYO
SA3:    LODA,R3 EY              ; the entry
        LODA,R0 *PSCRIPT,R3
        ADDI,R3 1
        STRA,R3 EY
        COMI,R0 T_EMPTY
        BCTA,LT SA4
        BCTA,EQ SA5
        SUBI,R0 T_SPANS         ; spans: EX of them, ERY the row reached
        STRA,R0 EX
        EORZ    R0
        STRA,R0 ERY
SA3A:   LODA,R0 ERY             ; sky to its top, dirt to its end
        STRA,R0 SX0
        LODA,R3 EY
        LODA,R0 *PSCRIPT,R3
        STRA,R0 SX1
        BSTA,UN CWSKY
        LODA,R3 EY
        LODA,R0 *PSCRIPT,R3
        STRA,R0 SX0
        LODA,R0 *PSCRIPT,R3,+
        STRA,R0 SX1
        STRA,R0 ERY
        ADDI,R3 1
        STRA,R3 EY
        BSTA,UN CWDIRT
        LODA,R0 EX
        SUBI,R0 1
        STRA,R0 EX
        BCFR,EQ SA3A
        LODA,R0 ERY             ; sky below the last
        BCTR,UN SA6
SA4:    STRA,R0 ERY             ; solid from a row: sky, then dirt
        STRA,R0 SX1
        EORZ    R0
        STRA,R0 SX0
        BSTR,UN CWSKY
        LODA,R0 ERY
        STRA,R0 SX0
        LODI,R0 FIELD_H
        STRA,R0 SX1
        BSTR,UN CWDIRT
        BCTR,UN SA7
SA5:    EORZ    R0              ; empty
SA6:    STRA,R0 SX0
        LODI,R0 FIELD_H
        STRA,R0 SX1
        BSTR,UN CWSKY
SA7:    LODA,R0 ECX             ; the next column
        ADDI,R0 1
        STRA,R0 ECX
        BCFR,EQ SA8
        LODA,R0 ECY
        ADDI,R0 1
        STRA,R0 ECY
SA8:    LODA,R0 ERX
        SUBI,R0 1
        STRA,R0 ERX
        BCFA,EQ SA2
        RETC,UN

; CWSKY, CWDIRT: rows SX0 to SX1 - 1 of column ERYO ($FF: off the field,
; nothing) made sky, dirt: a cell at a time (its column's blocks in the
; rows), straight on a cell without fire, through KMOD on one with it.
; Calls three down. Clobbers R0-R3, BX, BY, SX0, PTMP, BMSK.
CWSKY:  LODI,R0 PXSKY
        BCTR,UN CW0
CWDIRT: LODI,R0 PXDIRT
CW0:    STRA,R0 BTO
        LODA,R1 ERYO
        COMI,R1 $FF
        RETC,EQ
        STRA,R1 BX
        LODA,R0 SX1             ; (to 48 at most)
        COMI,R0 FIELD_H+1
        BCTR,LT CW1
        LODI,R0 FIELD_H
        STRA,R0 SX1
CW1:    LODA,R0 SX0
        COMA,R0 SX1
        RETC,GT
        RETC,EQ
        TMI,R0  1               ; a whole cell's rows from here: a run of them
        BCTR,EQ CW1A
        ADDI,R0 2
        COMA,R0 SX1
        BCTR,GT CW1A
        BCTA,UN CWRUN
CW1A:   LODA,R2 SX0             ; row R2: its cell
        LODA,R0 KROW,R2
        STRA,R0 PTMP
        ANDI,R0 1
        STRA,R0 BHALF
        LODA,R1 BX
        LODA,R0 PTMP
        ANDI,R0 $F0
        ADDA,R0 KCOL,R1
        STRA,R0 BIDX
        LODA,R0 KBIT,R1         ; R1: the column's bit in the top row
        STRZ    R1
        LODA,R0 PTMP            ; the bottom row: that only; SX0 one on
        TMI,R0  2
        BCFR,EQ CW2
        LODZ    R1
        RRL,R0
        RRL,R0
        RRL,R0
        STRA,R0 BMSK
        LODA,R0 SX0
        ADDI,R0 1
        BCTR,UN CW4
CW2:    STRA,R1 BMSK            ; the top row; the bottom too if it's in
        LODA,R0 SX0
        ADDI,R0 1
        COMA,R0 SX1
        BCTR,EQ CW4
        LODZ    R1
        RRL,R0
        RRL,R0
        RRL,R0
        IORA,R0 BMSK
        STRA,R0 BMSK
        LODA,R0 SX0
        ADDI,R0 2
CW4:    STRA,R0 SX0
        BSTA,UN KGET            ; the cell: fire in it?
        TMI,R0  $40
        BCTR,EQ CW7
        STRZ    R2
        LODA,R0 BTO
        BCTR,EQ CW5
        LODZ    R2              ; dirt: its blocks set ($00 stays; all of
        BCTA,EQ CW1             ;  them is $00)
        IORA,R0 BMSK
        COMI,R0 CDIRTP
        BCFR,EQ CW6
        EORZ    R0
        BCTR,UN CW6
CW5:    LODZ    R2              ; sky: its blocks cleared ($00 is $BF)
        BCFR,EQ CW5A
        LODI,R0 CDIRTP
CW5A:   LODA,R1 BMSK
        STRA,R0 PTMP
        LODZ    R1
        EORI,R0 $FF
        ANDA,R0 PTMP
CW6:    BSTA,UN KPUT
        BCTA,UN CW1
CW7:    LODI,R0 FR_SKY|FR_DIRT|FR_FX    ; fire: every kind becomes BTO
        STRA,R0 BFROM
        BSTA,UN KMOD
        BCTA,UN CW1

; CWRUN: the whole cells from row SX0 (even) down to SX1 (BY of them),
; each straight (CWMOD) unless it has fire (KMOD); then the rest (CW1).
CWRUN:  LODA,R1 BX              ; BMSK: the column's blocks, both rows;
        LODA,R0 KBIT,R1         ;  EPASS: the rest
        STRZ    R1
        RRL,R0
        RRL,R0
        RRL,R0
        IORZ    R1
        STRA,R0 BMSK
        EORI,R0 $FF
        STRA,R0 EPASS
        LODA,R0 SX1             ; BY: the cells (the rows' pairs)
        SUBA,R0 SX0
        ANDI,R0 $FE
        RRR,R0
        STRA,R0 BY
        LODA,R2 SX0             ; PTMP, BHALF: the first's index, half
        LODA,R0 KROW,R2
        STRZ    R2
        ANDI,R0 1
        STRA,R0 BHALF
        LODZ    R2
        ANDI,R0 $F0
        LODA,R1 BX
        ADDA,R0 KCOL,R1
        STRA,R0 PTMP
        LODA,R0 BY              ; SX0 past them
        ADDZ    R0
        ADDA,R0 SX0
        STRA,R0 SX0
        LODA,R2 BY              ; R2: the cells left, R3: the index
        LODA,R3 PTMP
        LODA,R0 BHALF
        BCFA,EQ CWS2
CWS1:   LODA,R0 SCR,R3          ; rows 0-12 (SCR)
        TMI,R0  $40
        BCTA,EQ CWH1
        BSTA,UN CWMOD
        STRA,R0 SCR,R3
CWN1:   BDRR,R2 CWN1A
        BCTA,UN CW1             ; (the run's done: the rest, one cell at most)
CWN1A:  ADDI,R3 16
        COMI,R3 13*16
        BCTR,LT CWS1
        SUBI,R3 13*16           ; (on into SCR2)
        LODI,R0 1
        STRA,R0 BHALF
CWS2:   LODA,R0 SCR2,R3         ; rows 13-23 (SCR2)
        TMI,R0  $40
        BCTA,EQ CWH2
        BSTR,UN CWMOD
        STRA,R0 SCR2,R3
CWN2:   BDRR,R2 CWN2A
        BCTA,UN CW1
CWN2A:  ADDI,R3 16
        BCTR,UN CWS2
CWH1:   BSTR,UN CWH             ; fire in it: every kind becomes BTO (KMOD)
        BCTA,UN CWN1
CWH2:   BSTR,UN CWH
        BCTA,UN CWN2
CWH:    STRA,R2 BY              ; (R2, R3 kept round it)
        STRA,R3 PTMP
        STRA,R3 BIDX
        LODI,R0 FR_SKY|FR_DIRT|FR_FX
        STRA,R0 BFROM
        BSTA,UN KMOD
        LODA,R2 BY
        LODA,R3 PTMP
        RETC,UN
; CWMOD: R0 (a cell without fire) with its blocks BMSK made BTO (EPASS:
; the others). Leaf. Clobbers R1.
CWMOD:  STRZ    R1
        LODA,R0 BTO
        BCTR,EQ CWM1
        LODZ    R1              ; dirt: set ($00 stays; all of them is $00)
        RETC,EQ
        IORA,R0 BMSK
        COMI,R0 CDIRTP
        RETC,LT
        RETC,GT
        EORZ    R0
        RETC,UN
CWM1:   LODZ    R1              ; sky: cleared ($00 is $BF)
        BCFR,EQ CWM2
        LODI,R0 CDIRTP
CWM2:   ANDA,R0 EPASS
        RETC,UN


        INCLUDE "src/blk.inc"
