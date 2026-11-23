; solver.inc -- each field band's two tank colours and their split (BANDA),
; from the tanks as drawn (TKON, TKX, TKY) and the seats' colours (SEATC).
; A band shows at most two tanks' own colours: left of HORCB's split L0,
; right of it R0. Tanks are placed in turn -- ours (MYSEAT), the turn's
; (ST+OFF_TURN), then the rest, in an order SROT turns -- and one is shown
; only where every band it touches has room, so a tank shown is one colour
; top to bottom. One not shown wears its side's colour. (There were two
; such phases, shown on alternate frames: a shimmer that strobed near the
; ground on a real TV.) Then the bars: the power and energy bars' lengths
; (BARPW, BARENG, bytes 0-40).

SVS1    EQU     WORK            ; each band's first tank shown (NONE)
SVS2    EQU     WORK+NFB        ;  and its second
SVSH    EQU     WORK+2*NFB      ; the tanks shown this phase (10 bytes)
SVPH    EQU     WORK+2*NFB+SEATS        ; the phase
SVK     EQU     SVPH+1          ; the rest's count
SVSPL   EQU     SVPH+2          ; SPLIT's split

; SOLVE: BANDA. Clobbers all but IX, IY.
SOLVE:  XOR     A
        LD      (SVPH),A
        LD      HL,SVS1         ; no tank shown in any band
        LD      B,2*NFB+SEATS
SV1:    LD      (HL),NONE
        INC     HL
        DJNZ    SV1
        LD      A,(MYSEAT)      ; ours, the turn's
        CALL    SVTRY
        LD      A,(ST+OFF_TURN)
        CALL    SVTRY
        XOR     A               ; the rest: seat (SROT + phase + 2k) mod 10 for
SV2:    LD      (SVK),A         ;  k 0-4, then (SROT + phase + 1 + 2k) mod 10
        LD      B,A
        CP      5
        LD      A,(SVPH)
        JR      C,SV3
        INC     A
SV3:    LD      C,A
        LD      A,B
        CP      5
        JR      C,SV4
        SUB     5
SV4:    ADD     A,A
        ADD     A,C
        LD      C,A
        LD      A,(SROT)
        ADD     A,C
SV5:    CP      SEATS
        JR      C,SV6
        SUB     SEATS
        JR      SV5
SV6:    CALL    SVTRY
        LD      A,(SVK)
        INC     A
        CP      SEATS
        JR      C,SV2
        LD      HL,BANDA        ; the table
        LD      B,0             ; each band (its index)
SV8:    PUSH    BC
        PUSH    HL
        LD      C,B
        LD      B,0
        LD      HL,SVS1
        ADD     HL,BC
        LD      D,(HL)
        LD      HL,SVS2
        ADD     HL,BC
        LD      E,(HL)
        XOR     A
        LD      (SVSPL),A
        LD      A,E
        CP      NONE
        JR      NZ,SV9
        LD      E,D             ; one tank, or none: its colour both sides
        JR      SV10
SV9:    CALL    SPLIT
SV10:   POP     HL
        POP     BC
        PUSH    BC
        LD      A,B             ; (PUTENT: C the band's first line)
        ADD     A,A
        ADD     A,A
        LD      C,A
        LD      A,(SVSPL)
        CALL    PUTENT
        INC     HL
        POP     BC
        INC     B
        LD      A,B
        CP      NFB
        JR      C,SV8
; the bars: HORCB = the length (b6-7 0: the border shows colour 0, L0 left
; of the split, R0 right)
SOBARS: LD      HL,BANDA+3*NFB
        LD      A,(BARPW)
        LD      (HL),A
        INC     HL
        LD      (HL),C_POWER
        INC     HL
        LD      (HL),C_TROUGH
        INC     HL
        LD      A,(BARENG)
        LD      (HL),A
        INC     HL
        LD      (HL),C_ENERGY
        INC     HL
        LD      (HL),C_TROUGH
        RET

; SVTRY: tank A shown this phase if it is on, not shown yet, and every band
; it touches (its lines from its barrel's tip to y-1) has room. Clobbers all
; but IX, IY.
SVTRY:  CP      SEATS
        RET     NC
        LD      E,A
        LD      D,0
        LD      HL,TKON
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        RET     Z
        LD      HL,SVSH
        ADD     HL,DE
        LD      A,(HL)
        CP      NONE
        RET     NZ              ; (shown already: ours is the turn's)
        LD      HL,TKY          ; its bands: B the first, C the last
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        RET     Z
        DEC     A
        CP      FIELD_H
        JR      C,SVT1
        LD      A,FIELD_H-1
SVT1:   RRCA
        RRCA
        AND     3FH
        LD      C,A
        PUSH    BC              ; its top: y - 5 - its barrel's height
        PUSH    HL
        LD      HL,TKA
        ADD     HL,DE
        LD      A,(HL)
        CP      91
        JR      C,SVTA
        LD      B,A
        LD      A,180
        SUB     B
SVTA:   LD      B,0             ; i = a / 5
SVTB:   SUB     5
        JR      C,SVTC
        INC     B
        JR      SVTB
SVTC:   LD      L,B             ; FP_BARREL + 8i + 7: the fourth pixel's dy
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      BC,FP_BARREL+7
        ADD     HL,BC
        LD      B,(HL)
        POP     HL
        LD      A,(HL)
        SUB     TANK_H
        JR      C,SVTD
        SUB     B
        JR      NC,SVTE
SVTD:   XOR     A
SVTE:   POP     BC
SVT2:   RRCA
        RRCA
        AND     3FH
        LD      B,A
        CP      C
        JR      Z,SVT3
        RET     NC              ; (all above the field)
SVT3:   PUSH    BC              ; room in each?
        LD      HL,SVS2
        LD      A,B
        ADD     A,L
        LD      L,A
        JR      NC,SVT4
        INC     H
SVT4:   LD      A,(HL)
        CP      NONE
        JR      NZ,SVT9
        INC     HL
        LD      A,B
        INC     B
        CP      C
        JR      C,SVT4
        POP     BC
        LD      HL,SVSH         ; yes: shown; in each band, its first or second
        ADD     HL,DE
        LD      (HL),E
SVT5:   LD      HL,SVS1
        LD      A,B
        ADD     A,L
        LD      L,A
        JR      NC,SVT6
        INC     H
SVT6:   LD      A,(HL)
        CP      NONE
        JR      Z,SVT7
        LD      A,NFB           ; (taken: the second)
        ADD     A,L
        LD      L,A
        JR      NC,SVT7
        INC     H
SVT7:   LD      (HL),E
        LD      A,B
        INC     B
        CP      C
        JR      C,SVT5
        RET
SVT9:   POP     BC
        RET

; SPLIT: tanks D and E (both shown in the band): D the left one, E the right,
; SVSPL the split (bytes below it left): by the right one, or by the left
; one if that is ours or the turn's (the tanks between wear the other's
; colour, not ours); if they share a byte, D's colour both sides (D = E,
; SVSPL 0). Clobbers A, BC, HL.
SPLIT:  LD      HL,TKX
        LD      B,0
        LD      C,D
        ADD     HL,BC
        LD      A,(HL)
        LD      HL,TKX
        LD      C,E
        ADD     HL,BC
        CP      (HL)
        JR      C,SP1
        LD      A,D             ; D is right of E
        LD      D,E
        LD      E,A
SP1:    LD      HL,TKX          ; the left tank's right byte: (x + 4) / 4
        LD      C,D
        ADD     HL,BC
        LD      A,(HL)
        ADD     A,4
        RRCA
        RRCA
        AND     3FH
        LD      B,A
        LD      HL,TKX          ; the right tank's left byte: (x - 4) / 4
        LD      A,E
        ADD     A,L
        LD      L,A
        JR      NC,SP2
        INC     H
SP2:    LD      A,(HL)
        SUB     4
        JR      NC,SP3
        XOR     A
SP3:    RRCA
        RRCA
        AND     3FH
        CP      B
        JR      Z,SP4
        JR      C,SP4
        LD      (SVSPL),A
        LD      A,(MYSEAT)      ; the left one ours, or the turn's: the split
        CP      D               ;  right by it, so its colour is on it alone
        JR      Z,SP3A
        LD      A,(ST+OFF_TURN)
        CP      D
        RET     NZ
SP3A:   LD      A,B
        INC     A
        LD      (SVSPL),A
        RET
SP4:    LD      E,D             ; they share a byte
        XOR     A
        LD      (SVSPL),A
        RET

; PUTENT: the entry at HL: HORCB A (and the band's margins: b6-7, MARGIN's
; colour), L0 seat D's colour, R0 seat E's (NONE: black). HL ends on its
; last byte. Preserves BC.
PUTENT: PUSH    AF
        CALL    MARGIN
        LD      (HL),A
        POP     AF
        OR      (HL)
        LD      (HL),A
        INC     HL
        LD      A,D
        CALL    SCOL
        LD      (HL),A
        INC     HL
        LD      A,E
        CALL    SCOL
        LD      (HL),A
        RET
SCOL:   CP      SEATS
        JR      NC,SCO1
        PUSH    HL
        LD      HL,SEATC
        ADD     A,L
        LD      L,A
        JR      NC,SCO0
        INC     H
SCO0:   LD      A,(HL)
        POP     HL
        RET
SCO1:   XOR     A
        RET

; MARGIN: A = the colour the band at line C shows in the margins left and
; right of the field (HORCB b6-7): the dirt's (2) where both edges are dirt
; at the band's middle, else the sky's (1). Preserves BC, DE, HL.
MARGIN: PUSH    BC
        PUSH    DE
        PUSH    HL
        INC     C
        INC     C
        LD      B,0
        CALL    FP_GET
        LD      D,A
        LD      B,FIELD_W-1
        CALL    FP_GET
        CP      PX_DIRT
        LD      A,40H           ; (sky)
        JR      NZ,MG1
        LD      A,D
        CP      PX_DIRT
        LD      A,40H
        JR      NZ,MG1
        LD      A,80H           ; (dirt)
MG1:    POP     HL
        POP     DE
        POP     BC
        RET
