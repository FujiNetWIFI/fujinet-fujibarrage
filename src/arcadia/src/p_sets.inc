; p_sets.inc -- the waiting room's settings view (p_room.inc FARPGs it:
; its catalog data fills a page of its own): row 0 "SETTINGS:" and the
; page's name; rows 1-10 five of the page's settings, two rows each (the
; label, cyan; its value right-aligned, white); rows 11-12 the hints. The
; names and numbers are the build's catalog (catset.inc, tools/arc/
; mkcat_arc.py's): a setting's record (CSREC: page, type, count, decimals,
; min16, step16), its label, its choices' names. No far calls from here
; (PGFAR is one level): fixed code and its own only.

        BCTA,UN SDRAW           ; E_SDRAW: the view (after TEXTSCR)
        BCTA,UN SALL            ; E_SALL: its slots again (a new state)
        BCTA,UN WSLINE          ; E_SLINE: slot WI
        BCTA,UN SCHG            ; E_SCHG: the chosen one's next value (WMV)

WROWS   EQU     5               ; (p_room.inc's)

SDRAW:  DAT     0,0             ; the settings: "SETTINGS: page"
        DCOL    LABI
        TXTAT   SSETS
        DCOL    TXTI
        LODA,R0 WPAGE
        ADDZ    R0
        STRZ    R3
        LODA,R0 CPNAM+1,R3
        STRZ    R1
        LODA,R0 CPNAM,R3
        ZBSR    *ZDTXT
        EORZ    R0              ; WKS: the page's settings (T0 the key)
        STRA,R0 WN
        STRA,R0 T0
WD3:    LODA,R0 T0
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R3
        LODA,R0 CSREC,R3        ; (its page)
        COMA,R0 WPAGE
        BCFR,EQ WD4
        LODA,R3 WN
        COMI,R3 16
        BCTR,EQ WD4
        LODA,R0 T0
        STRA,R0 WKS,R3
        ADDI,R3 1
        STRA,R3 WN
WD4:    LODA,R0 T0
        ADDI,R0 1
        STRA,R0 T0
        COMI,R0 CNSETS
        BCFR,EQ WD3
        DAT     11,0            ; the hints
        DCOL    LABI
        TXTAT   SPAGEH
        DAT     12,0
        BSTA,UN ISHOST
        BCFR,EQ WD4A
        LODI,R0 SCHGH>>8
        LODI,R1 SCHGH&$FF
        BCTR,UN WD5
WD4A:   LODI,R0 SSEATH>>8
        LODI,R1 SSEATH&$FF
WD5:    ZBSR    *ZDTXT
; SALL: every slot.
SALL:   EORZ    R0
SAL1:   STRA,R0 WI
        BSTA,UN WSLINE
        LODA,R0 WI
        ADDI,R0 1
        COMI,R0 WROWS
        BCFR,EQ SAL1
        RETC,UN

; WSLINE: the settings' slot WI (rows 1 + 2 WI and 2 + 2 WI): setting
; WTOP + WI of the page, '>' if chosen, its label; its value. Calls three
; down. Clobbers R0-R3, T0-T3, N32, DIG.
WSLINE: LODA,R0 WI
        ADDZ    R0
        ADDI,R0 1
        STRZ    R1
        LODI,R2 $FF
        ZBSR    *ZDPOS
        LODA,R0 WTOP
        ADDA,R0 WI
        STRA,R0 T0              ; T0: its place in WKS
        COMA,R0 WN
        BCTR,LT WSL1
        LODI,R0 GSPACE          ; none: both rows blank
        LODI,R3 16
        ZBSR    *ZDFILL
        LODI,R0 GSPACE
        LODI,R3 16
        ZBSR    *ZDFILL
        RETC,UN
WSL1:   LODI,R0 GSPACE          ; the cursor
        LODA,R1 T0
        COMA,R1 WSEL
        BCFR,EQ WSL2
        LODI,R0 GCURSOR|USI
WSL2:   ZBSR    *ZDGLY
        LODA,R3 T0              ; T1: the setting
        LODA,R0 WKS,R3
        STRA,R0 T1
        DCOL    LABI
        LODA,R0 T1
        ADDZ    R0
        STRZ    R3
        LODA,R0 CSLAB+1,R3
        STRZ    R1
        LODA,R0 CSLAB,R3
        LODI,R3 15
        BSTA,UN TXTPAD
        LODA,R0 WI              ; the value: the next row, cells 1-15
        ADDZ    R0
        ADDI,R0 2
        STRZ    R1
        ZBSR    *ZDPOS
        LODI,R2 0
; SVALUE: setting T1's value (the state's) right-aligned in 15 cells: a
; choice's name, or a number (min + value x step, its decimals). Calls
; two down. Clobbers R0-R3, T0-T3, N32, DIG.
SVALUE: DCOL    TXTI
        LODA,R3 T1
        LODA,R0 ST+OFF_SETTINGS,R3
        STRA,R0 T2              ; T2: the value
        LODA,R0 T1
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R3              ; R3: its record
        LODA,R0 CSREC+1,R3
        BCFA,EQ SVNUM
        LODA,R0 T1              ; a choice: past T2 names
        ADDZ    R0
        STRZ    R3
        LODA,R0 CSCH,R3
        STRA,R0 DSTRH
        LODA,R0 CSCH+1,R3
        STRA,R0 DSTRL
SVC1:   LODA,R0 T2
        BCTR,EQ SVC3
        LODI,R3 $FF
SVC2:   LODA,R0 *DSTRH,R3,+
        COMI,R0 $FF
        BCFR,EQ SVC2
        LODZ    R3
        ADDI,R0 1
        ADDA,R0 DSTRL
        STRA,R0 DSTRL
        TPSL    PSLC
        BCFR,EQ SVC2A
        LODA,R0 DSTRH
        ADDI,R0 1
        STRA,R0 DSTRH
SVC2A:  LODA,R0 T2
        SUBI,R0 1
        STRA,R0 T2
        BCTR,UN SVC1
SVC3:   LODA,R0 DSTRH
        LODA,R1 DSTRL
        LODI,R3 15
        BCTA,UN RTXT            ; tail call
SVNUM:  LODA,R0 CSREC+4,R3      ; a number: N32 = min
        STRA,R0 N32
        LODA,R0 CSREC+5,R3
        STRA,R0 N32+1
        EORZ    R0
        STRA,R0 N32+2
        STRA,R0 N32+3
SVN1:   LODA,R0 T2              ;  + T2 steps (24 bits: the cash)
        BCTR,EQ SVN2
        SUBI,R0 1
        STRA,R0 T2
        PPSL    PSLWC
        CPSL    PSLC
        LODA,R0 N32
        ADDA,R0 CSREC+6,R3
        STRA,R0 N32
        LODA,R0 N32+1
        ADDA,R0 CSREC+7,R3
        STRA,R0 N32+1
        LODA,R0 N32+2
        ADDI,R0 0
        STRA,R0 N32+2
        CPSL    PSLWC
        BCTR,UN SVN1
SVN2:   LODA,R0 CSREC+3,R3      ; its decimals
        LODI,R1 15
        BCTA,UN NUMX            ; tail call

; SCHG: the chosen setting's value one less (WMV's D_LEFT) or one more,
; round: T1 its key, T2 the value. Leaf.
SCHG:   LODA,R3 WSEL            ; T1: its key, T2: its value, T3: its count
        LODA,R0 WKS,R3
        STRA,R0 T1
        STRZ    R3
        LODA,R0 ST+OFF_SETTINGS,R3
        STRA,R0 T2
        LODA,R0 T1
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R3
        LODA,R0 CSREC+2,R3
        STRA,R0 T3
        LODA,R0 WMV
        TMI,R0  D_LEFT
        BCFR,EQ SCH2
        LODA,R0 T2              ; one less, round
        BCFR,EQ SCH1
        LODA,R0 T3
SCH1:   SUBI,R0 1
        STRA,R0 T2
        RETC,UN
SCH2:   LODA,R0 T2              ; one more, round
        ADDI,R0 1
        COMA,R0 T3
        BCTR,LT SCH3
        EORZ    R0
SCH3:   STRA,R0 T2
        RETC,UN

; ISHOST: CC EQ if we hold a seat and are the host (p_room.inc's). Leaf.
; Clobbers R0.
ISHOST: LODA,R0 MYSEAT
        COMI,R0 SEATS
        BCTR,LT ISH1
        RETC,UN                 ; (not EQ)
ISH1:   COMA,R0 ST+OFF_HOST
        RETC,UN

        INCLUDE "src/numx.inc"
        INCLUDE "src/txt.inc"

SSETS:  DB      "SETTINGS: ",$FF
SPAGEH: DB      "ENTER: NEXT PAGE",$FF
SSEATH: DB      "SELECT: SEATS   ",$FF
SCHGH:  DB      "<>: CHANGE IT   ",$FF

        INCLUDE "catset.inc"
