; ell.inc -- an ellipse on the field (src/atari/fld.s fld_ellipse): the
; blasts' fireballs, their craters, the dirt balls. Ours to draw (the
; settles that follow write the true columns), the Atari's shape: row dy
; is (erx * FP_Q32[i] + 32) >> 6 to each side, i = dy * 32 / ery. Its
; modes: 0 every pixel EV; 1 sky becomes EV; 2 fire becomes sky. FP_*
; (each drawing page's copy, as fld.inc).
;
; In: EX, EY (16-bit, signed: the field's pixels), ERX, ERY (radii, 1-200),
; EV, EMODE (ram.inc). Clobbers all but IX, IY.

FP_ELLIPSE:
        LD      A,(ELV)         ; the mode's byte patterns: 0 every pixel ELPE;
        LD      E,A             ;  1 sky becomes ELPE; 2 fire becomes sky
        LD      D,0
        LD      HL,FP_PAT
        ADD     HL,DE
        LD      A,(ELMODE)
        CP      2
        LD      D,PX_SKY*55H
        LD      E,(HL)
        JR      C,FP_EP
        LD      D,PX_FX*55H
        LD      E,PX_SKY*55H
FP_EP:  LD      A,D
        LD      (ELPD),A
        LD      A,E
        LD      (ELPE),A
        LD      A,(ELRY)
        OR      A
        JR      NZ,FP_E1
        INC     A
        LD      (ELRY),A
FP_E1:  SRL     A
        LD      (ELREM),A        ; ery / 2: the rounding
        XOR     A
        LD      (ELDY),A
        LD      (ELI),A
FP_EROW: LD     A,(ELI)          ; hw = (erx * q32[i] + 32) >> 6
        LD      E,A
        LD      D,0
        LD      HL,FP_Q32
        ADD     HL,DE
        LD      E,(HL)
        LD      A,(ELRX)
        LD      HL,0
        LD      B,8
FP_EM:  ADD     HL,HL           ; HL = A * E
        RLA
        JR      NC,FP_EM2
        ADD     HL,DE
FP_EM2: DJNZ    FP_EM
        LD      DE,32
        ADD     HL,DE
        LD      B,6
FP_ED:  SRL     H
        RR      L
        DJNZ    FP_ED
        LD      (ELHW),HL
        LD      HL,(ELY)         ; the row above: ey - dy
        LD      A,(ELDY)
        LD      E,A
        LD      D,0
        OR      A
        SBC     HL,DE
        CALL    FP_ESPAN
        LD      A,(ELDY)         ; and below: ey + dy
        OR      A
        JR      Z,FP_ENX
        LD      E,A
        LD      D,0
        LD      HL,(ELY)
        ADD     HL,DE
        CALL    FP_ESPAN
FP_ENX: LD      A,(ELRY)         ; dy >= ery: done
        LD      B,A
        LD      A,(ELDY)
        CP      B
        RET     NC
        INC     A
        LD      (ELDY),A
        LD      A,(ELI)         ; i += (rem + 32) / ery (i at most 32)
        LD      E,A
        LD      A,(ELREM)
        ADD     A,32
FP_EI:  CP      B
        JR      C,FP_EJ
        SUB     B
        INC     E
        JR      FP_EI
FP_EJ:  LD      (ELREM),A
        LD      A,E
        CP      33
        JR      C,FP_EK
        LD      A,32
FP_EK:  LD      (ELI),A
        JP      FP_EROW

; FP_ESPAN: line HL (16-bit), EX - EHW to EX + EHW, clipped to the field,
; by the mode, a byte (4 pixels) at a time: the end bytes masked. A swap
; finds its pixels in a byte at once: t = b ^ ELPD has 00 in each pair that
; matched; ~(t | t >> 1) & 55H marks their low bits. Clobbers all but IX, IY.
FP_ESPAN:
        LD      A,H             ; on the field's lines?
        OR      A
        RET     NZ
        LD      A,L
        CP      FIELD_H
        RET     NC
        LD      C,A             ; C: the line
        LD      HL,(ELX)         ; x0 = max(ex - hw, 0)
        LD      DE,(ELHW)
        OR      A
        SBC     HL,DE
        BIT     7,H
        JR      Z,FP_S1
        LD      HL,0
FP_S1:  LD      A,H
        OR      A
        RET     NZ              ; (all right of the field)
        LD      A,L
        CP      FIELD_W
        RET     NC
        LD      B,A             ; B: x0
        LD      HL,(ELX)         ; x1 = min(ex + hw, 159)
        ADD     HL,DE
        BIT     7,H
        RET     NZ              ; (all left of the field)
        LD      A,H
        OR      A
        LD      A,FIELD_W-1
        JR      NZ,FP_S2
        LD      A,L
        CP      FIELD_W
        JR      C,FP_S2
        LD      A,FIELD_W-1
FP_S2:  CP      B
        RET     C               ; (x1 < x0)
        LD      D,A             ; D: x1
; FP_HSPAN: pixels B to D (B <= D, on the field) of line C, by the mode
; (ELMODE, ELPD, ELPE as FP_ELLIPSE sets them). Clobbers all but IX, IY.
FP_HSPAN:
        CALL    FP_ADR          ; HL: x0's byte, A: its index
        PUSH    HL
        LD      HL,FP_LM        ; the first byte's mask: x0's pixel on
        CALL    FP_TAB
        LD      (ELLM),A
        LD      A,D             ; the last's: up to x1's
        AND     3
        LD      HL,FP_RM
        CALL    FP_TAB
        LD      (ELRM),A
        POP     HL
        LD      A,D             ; B: the bytes after the first
        RRCA
        RRCA
        AND     3FH
        LD      E,A
        LD      A,B
        RRCA
        RRCA
        AND     3FH
        LD      D,A
        LD      A,E
        SUB     D
        LD      B,A
        LD      A,(ELPD)
        LD      D,A
        LD      A,(ELPE)
        LD      E,A
        LD      A,(ELMODE)
        OR      A
        JR      NZ,FP_SW
        LD      A,B             ; 0: every pixel
        OR      A
        JR      NZ,FP_Z1
        CALL    FP_BOTH
        JR      FP_ZB
FP_Z1:  LD      A,(ELLM)
        LD      C,A
        CALL    FP_ZB
        INC     HL
        DEC     B
        JR      Z,FP_Z3
FP_Z2:  LD      (HL),E
        INC     HL
        DJNZ    FP_Z2
FP_Z3:  LD      A,(ELRM)
        LD      C,A
FP_ZB:  LD      A,(HL)          ; byte = b ^ ((b ^ E) & C)
        XOR     E
        AND     C
        XOR     (HL)
        LD      (HL),A
        RET
FP_SW:  LD      A,B             ; 1, 2: a swap
        OR      A
        JR      NZ,FP_V1
        CALL    FP_BOTH
        JR      FP_VB
FP_V1:  LD      A,(ELLM)
        LD      C,A
        PUSH    BC
        CALL    FP_VB
        POP     BC
        INC     HL
        DEC     B
        JR      Z,FP_V3
FP_V2:  LD      A,(HL)          ; a whole byte (C the scratch)
        XOR     D
        LD      C,A
        RRCA
        OR      C
        CPL
        AND     55H
        LD      C,A
        ADD     A,A
        OR      C
        LD      C,A
        LD      A,(HL)
        XOR     E
        AND     C
        XOR     (HL)
        LD      (HL),A
        INC     HL
        DJNZ    FP_V2
FP_V3:  LD      A,(ELRM)
        LD      C,A
FP_VB:  LD      A,(HL)          ; a byte's pixels that are D's, under mask C,
        XOR     D               ;  become E's (B the scratch)
        LD      B,A
        RRCA
        OR      B
        CPL
        AND     55H
        LD      B,A
        ADD     A,A
        OR      B
        AND     C
        LD      B,A
        LD      A,(HL)
        XOR     E
        AND     B
        XOR     (HL)
        LD      (HL),A
        RET
; FP_BOTH: C = both ends' masks (a span in one byte).
FP_BOTH: LD     A,(ELLM)
        LD      C,A
        LD      A,(ELRM)
        AND     C
        LD      C,A
        RET
; FP_TAB: A = (HL + A). Preserves BC, DE.
FP_TAB: ADD     A,L
        LD      L,A
        JR      NC,FP_T1
        INC     H
FP_T1:  LD      A,(HL)
        RET
FP_LM:  DB      0FFH,3FH,0FH,03H        ; a byte's pixels from index i on
FP_RM:  DB      0C0H,0F0H,0FCH,0FFH     ;  up to index i
FP_PAT: DB      00H,55H,0AAH,0FFH       ; a value in all 4 pixels
