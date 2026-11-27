; ---------------------------------------------------------------------------
; tank.inc -- the tanks, drawn into the field (FLD, and the screen while it
; shows the field) in blue (PX_TANK), as on the Atari and the Astrocade.
; Resident: the field's page and the player both move them.
;
; A tank is 5x3 (x its centre, y the row under it) and a barrel of 4 pixels
; from its turret (tank.inc's tables: tools/chf/mktank_chf.py); ours has a
; pennant, a pixel over the hull on the side away from the barrel. A tank
; keeps what each of its pixels covered (TKSAVE: x, y, the pixel, in the
; order drawn) and is taken off by putting those back, last first, where
; its pixel is still there: a tank on a slope leaves the slope as it was,
; and whatever wrote over a tank pixel since (a settle, a blast) stands.
;
; Each seat has a wanted tank (TKON, TKX, TKY, TKA) and the one drawn (TDON,
; TDX, TDY, TDA); TKSHOW makes the drawn one the wanted one.
; ---------------------------------------------------------------------------

        INCLUDE "tankgfx.inc"

BLINKOFF EQU 018H               ; BLINKT & this == this: the turn's tank off

; TKSHOW -- seat r0's tank as wanted: off where it is drawn, on where it is
;   wanted (if it is).
TKSHOW: ENTER
        LR A,0
        STB TKS
        DCI TDON                ; drawn: off
        ADC
        LM
        NI 0FFH
        BZ TKS1
        PI TKUNDO
        LDB TKS
        DCI TDON
        ADC
        CLR
        ST
TKS1:   LDB TKS                 ; wanted: on
        DCI TKON
        ADC
        LM
        NI 0FFH
        BZ TKSX
        CLR
        STB TKOFS
        LIS 1
        STB TKMODE
        PI TKSAVE0              ; its list from the start
        PI TKPLOT
        LDB TKS                 ; drawn = wanted
        LR 0,A
        DCI TKX
        ADC
        LM
        LR 1,A
        LR A,0
        DCI TDX
        ADC
        LR A,1
        ST
        LR A,0
        DCI TKY
        ADC
        LM
        LR 1,A
        LR A,0
        DCI TDY
        ADC
        LR A,1
        ST
        LR A,0
        DCI TKA
        ADC
        LM
        LR 1,A
        LR A,0
        DCI TDA
        ADC
        LR A,1
        ST
        LR A,0
        DCI TDON
        ADC
        LIS 1
        ST
TKSX:   LEAVE

; TKPLOT -- seat TKS's tank at TKX/TKY/TKA + TKOFS (0: the wanted one; TDX-TKX:
;   the drawn one) drawn (TKMODE 1) or taken off (0).  TD* are TK* + 40
;   (ram.inc).
TKPLOT: ENTER
        LDB TKOFS               ; its x, y, angle
        LR 0,A
        LDB TKS
        AS 0
        LR 0,A
        DCI TKX
        ADC
        LM
        STB TKPX
        LR A,0
        DCI TKY
        ADC
        LM
        STB TKPY
        LR A,0
        DCI TKA
        ADC
        LM
        LR 1,A                  ; angle / 5 (0-36): its barrel's row
        CLR
        LR 2,A
TKP0:   LR A,1
        CI 4
        BC TKP0A
        AI -5
        LR 1,A
        LR A,2
        INC
        LR 2,A
        BR TKP0
TKP0A:  LR A,1                  ; (the nearer five: round)
        CI 2
        BC TKP0B
        LR A,2
        INC
        LR 2,A
TKP0B:  LR A,2
        CI 36
        BC TKP0C
        LI 36
        LR 2,A
TKP0C:  LR A,2                  ; BARL + 8 row: 4 row kept (8 row passes 255)
        SL 1
        SL 1
        STB TKBRL
        LI TKBODYN              ; the body
        STB TKN
        CLR
        STB TKB                 ; (dy from TKPY)
        DCI TKBODY
        LR Q,DC
        PI TKPIX
        LDB TKBRL               ; the barrel: from the turret's top (y - 3)
        LR 1,A
        DCI BARL
        LR A,1
        ADCU
        LR A,1
        ADCU
        LR Q,DC
        LIS 4
        STB TKN
        LIS 1                   ; (dy up from the turret's top)
        STB TKB
        LR DC,Q
        LR Q,DC
        PI TKPIX
        LDB TKS                 ; ours: the pennant
        LR 0,A
        LDB MYSEAT
        XS 0
        BNZ TKPX9
        LDB TKOFS
        AS 0
        DCI TKA
        ADC
        LM
        CI 90
        LI PEN_L&0FFH           ; (angle <= 90: the barrel right, it left)
        BC TKP8
        LI PEN_R&0FFH
TKP8:   LR 1,A
        LIS 1
        STB TKN
        CLR
        STB TKB
        LI PEN_L>>8
        LR QU,A
        LR A,1
        LR QL,A
        PI TKPIX
TKPX9:  LEAVE
PEN_L:  DB 0FEH, 0FCH           ; the pennant: (-2, -4) or (2, -4)
PEN_R:  DB 2, 0FCH
        IF (PEN_L>>8) <> (PEN_R>>8)
        ERROR "the pennants must share a page"
        ENDIF

; TKPIX -- TKN pixels (dx, dy pairs at Q) from (TKPX, TKPY), TKB = 1: the
;   barrel's (dy up from the turret's top, TKPY - 3) else the body's (dy
;   from TKPY); drawn or taken off by TKMODE.  Off the field: nothing.
;   (Not in r7, r8: ENTER's PUSHK spends r7.)
TKPIX:  ENTER
TKX1:   LR DC,Q
        LM
        LR 2,A                  ; dx
        LM
        LR 3,A                  ; dy
        LR Q,DC
        LDB TKPX
        AS 2
        LR 5,A                  ; x
        CI FW-1
        BNC TKX9
        LDB TKB
        NI 0FFH
        BNZ TKX2
        LDB TKPY                ; the body: y + dy
        AS 3
        BR TKX3
TKX2:   LR A,3                  ; the barrel: y - 3 - dy
        COM
        INC
        LR 3,A
        LDB TKPY
        AI -3
        AS 3
TKX3:   LR 6,A
        CI FH-1
        BNC TKX9
        LDB TKMODE
        NI 0FFH
        BZ TKX4
        PI FGET                 ; drawn: what it covers kept first
        LR 1,A
        PI TKKEEP
        LI PX_TANK
        LR 1,A
        PI FSET
        BR TKX9
TKX4:   PI FGET                 ; (taken off: tank back to sky; unused now)
        CI PX_TANK
        BNZ TKX9
        LI PX_SKY
        LR 1,A
        PI FSET
TKX9:   DECB TKN
        BNZ TKX1
        LEAVE

; TKALL -- every seat's tank as wanted.
TKALL:  ENTER
        CLR
        STB TKS2
TKA1:   LDB TKS2
        LR 0,A
        PI TKSHOW
        LDB TKS2
        INC
        STB TKS2
        CI SEATS
        BNZ TKA1
        LEAVE

; TKLOST -- the drawn tanks forgotten (the field under them was written
;   whole: a TERRAIN version, a field shown afresh).  Leaf.
TKLOST: DCI TDON
        LI SEATS
        LR 0,A
TKL1:   CLR
        ST
        DS 0
        BNZ TKL1
        POP

; TKSTATE -- each seat's wanted tank from the state (PF_ALIVE, P_X, P_Y,
;   P_ANGLE; ours while we aim: AIMA), shown where it changed.
TKSTATE: ENTER
        CLR
        STB TKS2
TKT1:   LDB TKS2                ; its record: ST + OFF_SEATS + 25 s
        LR 0,A
        SL 1
        SL 1
        SL 1
        LR 1,A
        SL 1
        AS 1
        AS 0
        DCI ST+OFF_SEATS
        ADCU
        LR Q,DC
        LI P_FLAGS
        ADC
        LM
        NI PF_ALIVE
        LR 4,A                  ; on
        LR DC,Q
        LI P_X
        ADC
        LM
        LR 5,A                  ; x (its high byte: 0 on the field)
        LR DC,Q
        LI P_Y
        ADC
        LM
        LR 6,A                  ; y
        LR DC,Q
        LI P_ANGLE
        ADC
        LM
        LR 7,A                  ; angle
        LDB ST+OFF_PHASE        ; the turn's, aiming: off in its blink
        CI PH_AIM
        BNZ TKT1B
        LDB ST+OFF_TURN
        XS 0
        BNZ TKT1B
        LDB BLINKT
        NI BLINKOFF
        CI BLINKOFF
        BNZ TKT1B
        CLR
        LR 4,A
TKT1B:  LDB TKS2                ; ours, aiming: our angle
        LR 0,A
        LDB MYSEAT
        XS 0
        BNZ TKT2
        LDB MYTURN
        NI 0FFH
        BZ TKT2
        LDB AIMA
        LR 7,A
TKT2:   CLR                     ; changed?
        LR 8,A
        LR A,0
        DCI TKON
        ADC
        LM
        XS 4
        BZ TKT3
        LR A,4
        LI 0FFH
        ADC
        LR A,4
        ST
        LIS 1
        LR 8,A
TKT3:   LR A,0
        DCI TKX
        ADC
        LM
        XS 5
        BZ TKT4
        LI 0FFH
        ADC
        LR A,5
        ST
        LIS 1
        LR 8,A
TKT4:   LR A,0
        DCI TKY
        ADC
        LM
        XS 6
        BZ TKT5
        LI 0FFH
        ADC
        LR A,6
        ST
        LIS 1
        LR 8,A
TKT5:   LR A,0
        DCI TKA
        ADC
        LM
        XS 7
        BZ TKT6
        LI 0FFH
        ADC
        LR A,7
        ST
        LIS 1
        LR 8,A
TKT6:   LR A,8                  ; changed (or not drawn as wanted): shown
        NI 0FFH
        BNZ TKT7
        LR A,0
        DCI TDON
        ADC
        LM
        LR 1,A
        LR A,0
        DCI TKON
        ADC
        LM
        XS 1
        BZ TKT8
TKT7:   PI TKSHOW
TKT8:   LDB TKS2
        INC
        STB TKS2
        CI SEATS
        BZ TKTX
        JMP TKT1
TKTX:   LEAVE

; ---- what a tank covers --------------------------------------------------------
; TKSAVE0 -- seat TKS's list emptied, TKSP its start.  Leaf.
TKSAVE0: LDB TKS
        LR 0,A
        DCI TKSN
        ADC
        CLR
        ST
        LR A,0                  ; TKSP = TKSAVE + 54 s
        SL 1
        DCI TKSB
        ADC
        LM
        LR 1,A
        LM
        LR 2,A
        DCI TKSP
        LR A,1
        ST
        LR A,2
        ST
        POP

; TKKEEP -- (r5, r6) covered r1: appended to seat TKS's list (18 at most).
;   Leaf.  Clobbers A, r4, H, DC0 (not Q: TKPIX walks it).
TKKEEP: LDB TKS
        DCI TKSN
        ADC
        LM
        CI TKMAXP-1
        BC TKK1
        POP                     ; (full: not kept)
TKK1:   INC
        LR 4,A
        LI 0FFH
        ADC
        LR A,4
        ST
        DCI TKSP
        LM
        LR 10,A
        LM
        LR 11,A
        LR DC,H
        LR A,5
        ST
        LR A,6
        ST
        LR A,1
        ST
        LR H,DC
        DCI TKSP
        LR A,10
        ST
        LR A,11
        ST
        POP

; TKUNDO -- seat TKS's tank off: its list from the last, each pixel still
;   TANK back to what it covered.
TKUNDO: ENTER
        LDB TKS
        LR 0,A
        DCI TKSN
        ADC
        LM
        LR 7,A
        NI 0FFH
        BZ TKUX
        STB TKN                 ; (ENTER'd calls below spend r7: none here)
        LR A,0                  ; the last entry: TKSAVE + 54 s + 3 (n - 1)
        SL 1
        DCI TKSB
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LDB TKN
        LR 4,A
        LR DC,Q
        LR A,4
        AI 0FFH
        LR 1,A
        SL 1
        AS 1                    ; 3 (n - 1) (51 at most)
        ADC
        LR Q,DC
TKU1:   LR DC,Q
        LM
        LR 5,A
        LM
        LR 6,A
        LM
        LR 8,A                  ; what it covered
        PI FGET                 ; still the tank's?
        CI PX_TANK
        BNZ TKU2
        LR A,8
        LR 1,A
        PI FSET
TKU2:   LR DC,Q                 ; the one before
        LI -3
        ADC
        LR Q,DC
        DECB TKN
        BNZ TKU1
TKUX:   LDB TKS
        LR 0,A
        DCI TKSN
        ADC
        CLR
        ST
        LEAVE

TKMAXP  EQU 18                  ; a tank's pixels at most: 13 + 4 + 1
TKSB:   DW TKSAVE, TKSAVE+54, TKSAVE+108, TKSAVE+162, TKSAVE+216
        DW TKSAVE+270, TKSAVE+324, TKSAVE+378, TKSAVE+432, TKSAVE+486
