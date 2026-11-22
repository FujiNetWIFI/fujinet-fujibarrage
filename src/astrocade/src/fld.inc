; fld.inc -- the field's pixel primitives (src/atari/fld.s's), for a page
; that draws (a FARCALL a pixel would cost ten times the pixel). The field is
; the bitmap: FIELD_W x FIELD_H, 2 bits a pixel, 40 bytes a line, the left
; pixel in b7-6. Every label here is FP_*: build.sh makes each drawing
; page's own copy by renaming the prefix (PLY's PY_, OPS's OQ_; tables.inc
; the same), so each page draws with no page crossing.
;
; Coordinates: B = x (0-159), C = y (0-55); off the field does nothing.

; FP_ADR: HL = the byte of pixel (B, C), A = its index in the byte (0-3).
; Preserves BC, DE.
FP_ADR: PUSH    DE
        LD      L,C
        LD      H,0
        ADD     HL,HL
        LD      DE,FP_ROWT
        ADD     HL,DE
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        LD      A,B
        RRCA
        RRCA
        AND     3FH
        ADD     A,L
        LD      L,A
        JR      NC,FP_A1
        INC     H
FP_A1:  LD      A,B
        AND     3
        POP     DE
        RET

; FP_PLOT: pixel (B, C) = value E (0-3). Clobbers A, HL.
FP_PLOT: LD     A,B
        CP      FIELD_W
        RET     NC
        LD      A,C
        CP      FIELD_H
        RET     NC
        CALL    FP_ADR
        PUSH    DE
        LD      D,A             ; the pixel's index
        ADD     A,A
        ADD     A,A
        ADD     A,E             ; FP_VAL[4 * index + value]
        LD      E,A
        LD      A,D
        LD      D,0
        PUSH    HL
        LD      HL,FP_VAL
        ADD     HL,DE
        LD      E,(HL)          ; the value, in place
        LD      HL,FP_MASK
        ADD     A,L
        LD      L,A
        JR      NC,FP_P1
        INC     H
FP_P1:  LD      A,(HL)          ; the other pixels' mask
        POP     HL
        AND     (HL)
        OR      E
        LD      (HL),A
        POP     DE
        RET

; FP_GET: A = pixel (B, C)'s value (0-3); off the field: PX_SKY. Clobbers HL.
FP_GET: LD      A,B
        CP      FIELD_W
        JR      NC,FP_G9
        LD      A,C
        CP      FIELD_H
        JR      NC,FP_G9
        CALL    FP_ADR
        PUSH    BC
        LD      B,A
        LD      A,3
        SUB     B
        ADD     A,A             ; (3 - index) * 2 right shifts
        LD      B,A
        LD      A,(HL)
        JR      Z,FP_G2
FP_G1:  RRCA
        DJNZ    FP_G1
FP_G2:  AND     3
        POP     BC
        RET
FP_G9:  LD      A,PX_SKY
        RET

; FP_SWAP: pixel (B, C) = value E if it is value D (fld_swap). Clobbers A, HL.
FP_SWAP: CALL   FP_GET
        CP      D
        RET     NZ
        JP      FP_PLOT

; FP_COLW: column B from the entry at IX (TERRAIN's: 00-EF solid from that
; row down; F0 none; F0+n n spans of top, end): sky, then its dirt. IX
; moves past the entry. Clobbers A, C, DE, HL.
FP_COLW: LD     C,0
        CALL    FP_ADR          ; HL = the column's top byte, A = its index
        PUSH    HL
        LD      E,A
        LD      D,0
        LD      HL,FP_MASK
        ADD     HL,DE
        LD      C,(HL)          ; C = the mask of the column's neighbours
        LD      A,E
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      HL,FP_VAL+PX_SKY
        ADD     HL,DE
        LD      A,(HL)          ; the sky, in place
        LD      (FCSKY),A
        INC     HL              ; (PX_DIRT = PX_SKY + 1)
        LD      A,(HL)
        LD      (FCDIRT),A
        POP     HL
        LD      A,(IX+0)
        INC     IX
        CP      0F0H
        JR      NC,FP_W2
        CP      FIELD_H         ; solid from row A: sky above it, dirt below
        JR      C,FP_W0
        LD      A,FIELD_H
FP_W0:  LD      D,A
        OR      A
        JR      Z,FP_W1
        PUSH    DE
        LD      A,(FCSKY)
        CALL    FP_RUN          ; (HL moves down to row A)
        POP     DE
FP_W1:  LD      A,FIELD_H
        SUB     D
        RET     Z
        LD      D,A
        LD      A,(FCDIRT)
        JR      FP_RUN
FP_W2:  PUSH    HL              ; spans: all of it sky, then each span
        LD      A,(FCSKY)
        LD      D,FIELD_H
        CALL    FP_RUN
        POP     HL
        LD      A,(IX-1)
        SUB     0F0H            ; n spans
        RET     Z
        LD      B,A
FP_W3:  PUSH    BC
        PUSH    HL
        LD      D,(IX+0)        ; top
        LD      A,(IX+1)        ; end (exclusive)
        INC     IX
        INC     IX
        CP      FIELD_H+1
        JR      C,FP_W4
        LD      A,FIELD_H
FP_W4:  SUB     D
        JR      Z,FP_W5
        JR      C,FP_W5
        PUSH    AF
        CALL    FP_ROW
        POP     AF
        LD      D,A
        LD      A,(FCDIRT)
        CALL    FP_RUN
FP_W5:  POP     HL
        POP     BC
        DJNZ    FP_W3
        RET
; FP_ROW: HL += 40 * D (the column's byte D rows down). Clobbers A, DE.
FP_ROW: LD      A,D
        OR      A
        RET     Z
        LD      DE,BYTEPL
FP_R1:  ADD     HL,DE
        DEC     A
        JR      NZ,FP_R1
        RET
; FP_RUN: D rows from HL down = the value A (in place; C the neighbours' mask).
; Clobbers A, D, E, HL.
FP_RUN: LD      E,A
FP_U1:  LD      A,(HL)
        AND     C
        OR      E
        LD      (HL),A
        LD      A,L
        ADD     A,BYTEPL
        LD      L,A
        JR      NC,FP_U2
        INC     H
FP_U2:  DEC     D
        JR      NZ,FP_U1
        RET

; a pixel's other pixels (by its index), and a value in its place
FP_MASK: DB     3FH,0CFH,0F3H,0FCH
FP_VAL: DB      00H,40H,80H,0C0H        ; index 0: values 0-3 in b7-6
        DB      00H,10H,20H,30H
        DB      00H,04H,08H,0CH
        DB      00H,01H,02H,03H
