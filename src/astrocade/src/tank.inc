; tank.inc -- a tank in the field (src/atari/field.c sprite()): its body
; (7x5, its bottom centre at (x, y)) and its barrel (4 pixels from the
; turret at its angle), as tank pixels; erased by turning its tank pixels
; back to sky. Needs fld.inc (FP_PLOT, FP_GET) and the page's FP_BARREL.

TBODY:  DB      1CH,3EH,7FH,7FH,2AH     ; b6 the left column

; TANKD: tank A drawn (TKX, TKY, TKA). TANKE: erased. Clobber all but IX, IY.
TANKD:  LD      E,PX_TANK
        JR      TK0
TANKE:  LD      E,0FFH          ; (erase: tank pixels back to sky)
TK0:    LD      C,A
        LD      B,0
        LD      A,E
        LD      (TKDV),A
        LD      HL,TKX
        ADD     HL,BC
        LD      A,(HL)
        LD      (TKDX),A
        LD      HL,TKY
        ADD     HL,BC
        LD      A,(HL)
        LD      (TKDY),A
        LD      HL,TKA
        ADD     HL,BC
        LD      A,(HL)
        LD      (TKDA),A
        LD      HL,TBODY        ; the body: 5 rows from y - 5
        LD      A,(TKDY)
        SUB     TANK_H
        LD      C,A
TK1:    LD      A,(HL)
        INC     HL
        PUSH    HL
        LD      D,A
        LD      A,(TKDX)
        SUB     3
        LD      B,A
        LD      E,7
TK2:    BIT     6,D
        CALL    NZ,TKPX
        SLA     D
        INC     B
        DEC     E
        JR      NZ,TK2
        POP     HL
        INC     C
        LD      A,(TKDY)
        CP      C
        JR      NZ,TK1
        LD      A,(TKDA)        ; the barrel: i = a/5 (a <= 90), (180 - a)/5
        LD      C,A
        CP      91
        JR      C,TK3
        LD      A,180
        SUB     C
TK3:    LD      B,0
TK4:    SUB     5
        JR      C,TK5
        INC     B
        JR      TK4
TK5:    LD      L,B             ; FP_BARREL + 8i
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      DE,FP_BARREL
        ADD     HL,DE
        LD      E,4
TK6:    LD      A,(TKDA)
        CP      91
        LD      A,(TKDX)
        JR      C,TK7
        SUB     (HL)            ; past 90: to the left
        JR      TK8
TK7:    ADD     A,(HL)
TK8:    LD      B,A
        INC     HL
        LD      A,(TKDY)
        SUB     TANK_H
        SUB     (HL)
        LD      C,A
        INC     HL
        PUSH    HL
        PUSH    DE
        LD      D,0
        CALL    TKPX
        POP     DE
        POP     HL
        DEC     E
        JR      NZ,TK6
        RET
; TKPX: the pixel (B, C): drawn (TKDV a value), or erased (TKDV 0FFH: a tank
; pixel back to sky). Preserves BC, DE, HL.
TKPX:   PUSH    DE
        PUSH    HL
        LD      A,(TKDV)
        CP      0FFH
        JR      Z,TKP1
        LD      E,A
        CALL    FP_PLOT
        JR      TKP9
TKP1:   CALL    FP_GET
        CP      PX_TANK
        JR      NZ,TKP9
        LD      E,PX_SKY
        CALL    FP_PLOT
TKP9:   POP     HL
        POP     DE
        RET
