;; ===========================================================================
;; trk.asm -- the script's tracks (src/atari/trk.s's; docs/PROTOCOL.md
;; OP_ARC): sixteen slots (TRACKS, 16 words each: memory.bas's TR_*), each
;; a moving head. Its position and velocity are 32-bit fixed point with 12
;; fraction bits (two words, the low first), its acceleration 16 bits,
;; sign-extended. Each step
;;
;;       X += VX;  Y += VY;  VX += AX;  VY += AY
;;
;; and the head is at (X >> 12, Y >> 12): the server's fitter runs the very
;; same integers (project.Arc).
;;
;; A smoke trail's head goes into the planes (fire where there's no dirt)
;; and stays, as the reference view's does. Any other head changes nothing
;; the reference view keeps (whatever meets it leaves the same pixels), so
;; it never goes into the planes: HEADS draws it over the screen, a head
;; card for each cell with heads in it (N_HEAD at most), where the field is
;; sky.
;;
;; (R5 is CALL's return: a routine that calls never keeps a pointer in it
;; across a call.)
;; ===========================================================================

;; TRK_RESET: no tracks.
TRK_RESET: PROC
        MVII    #const_TRACKS, R4
        CLRR    R0
        MVII    #16, R1
@@l:    MVO@    R0, R4
        ADDI    #15, R4
        DECR    R1
        BNEQ    @@l
        JR      R5
        ENDP

;; TRK_STOP: track R0 (0-15) ends now (a trail's pixels stay).
TRK_STOP: PROC
        ANDI    #15, R0
        SLL     R0, 2
        SLL     R0, 2
        ADDI    #const_TRACKS, R0
        MOVR    R0, R4
        CLRR    R0
        MVO@    R0, R4
        JR      R5
        ENDP

;; TRK_LOAD: an OP_ARC's arguments at R4 (slot, style, x16, y16, vx24, vy24,
;; ax16, ay16, n16) start a track; R4 past them.
TRK_LOAD: PROC
        PSHR    R5
        MVI@    R4, R0                  ; the slot: its record
        ANDI    #15, R0
        SLL     R0, 2
        SLL     R0, 2
        ADDI    #const_TRACKS, R0
        MOVR    R0, R3
        MOVR    R0, R5
        MVII    #1, R0                  ; on
        MVO@    R0, R5
        MVI@    R4, R0                  ; style
        MVO@    R0, R5
        MVI@    R4, R0                  ; X: x16 (12.4) << 8: lo = its low byte << 8,
        SWAP    R0
        MVO@    R0, R5
        MVI@    R4, R0                  ; hi = its high byte, sign-extended
        CMPI    #128, R0
        BNC     @@x1
        ADDI    #$FF00, R0
@@x1:   MVO@    R0, R5
        MVI@    R4, R0                  ; Y the same
        SWAP    R0
        MVO@    R0, R5
        MVI@    R4, R0
        CMPI    #128, R0
        BNC     @@y1
        ADDI    #$FF00, R0
@@y1:   MVO@    R0, R5
        MVI@    R4, R0                  ; VX: 24 bits, the top byte sign-extended
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        MVO@    R0, R5
        MVI@    R4, R0
        CMPI    #128, R0
        BNC     @@v1
        ADDI    #$FF00, R0
@@v1:   MVO@    R0, R5
        MVI@    R4, R0                  ; VY
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        MVO@    R0, R5
        MVI@    R4, R0
        CMPI    #128, R0
        BNC     @@v2
        ADDI    #$FF00, R0
@@v2:   MVO@    R0, R5
        MVI@    R4, R0                  ; AX
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        MVO@    R0, R5
        MVI@    R4, R0                  ; AY
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        MVO@    R0, R5
        MVI@    R4, R0                  ; steps
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        MVO@    R0, R5
        PSHR    R4
        CALL    TRK_PIX                 ; its pixel now
        PULR    R4
        PULR    PC
        ENDP

;; TRK_PIX: track R3's pixel (X >> 12, Y >> 12) into its record; a smoke
;; trail's into the planes too. R3 kept.
TRK_PIX: PROC
        PSHR    R5
        MOVR    R3, R4
        ADDI    #const_TR_XL, R4
        MVI@    R4, R0                  ; XL
        MVI@    R4, R1                  ; XH
        SWAP    R0
        ANDI    #255, R0
        SLR     R0, 2
        SLR     R0, 2
        SLL     R1, 2
        SLL     R1, 2
        ADDR    R1, R0
        MVI@    R4, R1                  ; YL
        MVI@    R4, R2                  ; YH
        SWAP    R1
        ANDI    #255, R1
        SLR     R1, 2
        SLR     R1, 2
        SLL     R2, 2
        SLL     R2, 2
        ADDR    R2, R1
        MOVR    R3, R4
        ADDI    #const_TR_PX, R4
        MVO@    R0, R4
        MVO@    R1, R4
        MOVR    R3, R4                  ; a trail: fire where there's no dirt
        ADDI    #const_TR_ST, R4
        MVI@    R4, R2
        ANDI    #15, R2
        CMPI    #const_ST_SMOKE, R2
        BNEQ    @@x
        PSHR    R3
        MVII    #const_PO_AIRFX, R2
        CALL    PIXOP
        PULR    R3
@@x:    PULR    PC
        ENDP

;; TRK_STEP: every live track a step (a track with no steps left ends).
;; PL_FALL: the lowest falling shell's row | 1 (not a digger's), or 0.
TRK_STEP: PROC
        PSHR    R5
        CLRR    R0
        MVO     R0, const_PL_FALL
        MVII    #const_TRACKS, R3
@@t:    MOVR    R3, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@n
        MOVR    R3, R4                  ; steps left?
        ADDI    #const_TR_N, R4
        MVI@    R4, R0
        TSTR    R0
        BNEQ    @@go
        MVO@    R0, R3                  ; (none: off)
        B       @@n
@@go:   DECR    R0
        DECR    R4
        MVO@    R0, R4
        MOVR    R3, R4                  ; X += VX
        ADDI    #const_TR_VXL, R4
        MVI@    R4, R0
        MVI@    R4, R1
        MOVR    R3, R4
        ADDI    #const_TR_XL, R4
        ADD@    R4, R0
        ADCR    R1
        ADD@    R4, R1
        MOVR    R3, R5
        ADDI    #const_TR_XL, R5
        MVO@    R0, R5
        MVO@    R1, R5
        MOVR    R3, R4                  ; Y += VY
        ADDI    #const_TR_VYL, R4
        MVI@    R4, R0
        MVI@    R4, R1
        MOVR    R3, R4
        ADDI    #const_TR_YL, R4
        ADD@    R4, R0
        ADCR    R1
        ADD@    R4, R1
        MVO@    R0, R5                  ; (R5: YL)
        MVO@    R1, R5
        MOVR    R3, R4                  ; VX += AX
        ADDI    #const_TR_AX, R4
        MVI@    R4, R0
        CLRR    R1
        TSTR    R0
        BPL     @@ax
        DECR    R1
@@ax:   MOVR    R3, R4
        ADDI    #const_TR_VXL, R4
        ADD@    R4, R0
        ADCR    R1
        ADD@    R4, R1
        MVO@    R0, R5                  ; (R5: VXL)
        MVO@    R1, R5
        MOVR    R3, R4                  ; VY += AY
        ADDI    #const_TR_AY, R4
        MVI@    R4, R0
        CLRR    R1
        TSTR    R0
        BPL     @@ay
        DECR    R1
@@ay:   MOVR    R3, R4
        ADDI    #const_TR_VYL, R4
        ADD@    R4, R0
        ADCR    R1
        ADD@    R4, R1
        MVO@    R0, R5                  ; (R5: VYL)
        MVO@    R1, R5
        CALL    TRK_PIX
        MOVR    R3, R4                  ; falling, on the field, not a digger: the whistle's
        ADDI    #const_TR_VYH, R4
        MVI@    R4, R0
        TSTR    R0
        BMI     @@n
        MOVR    R3, R4
        ADDI    #const_TR_ST, R4
        MVI@    R4, R0
        ANDI    #15, R0
        CMPI    #const_ST_DIGGER, R0
        BEQ     @@n
        MOVR    R3, R4
        ADDI    #const_TR_PY, R4
        MVI@    R4, R0
        CMPI    #80, R0
        BC      @@n
        CMP     const_PL_FALL, R0
        BNC     @@n
        ANDI    #$FFFE, R0              ; (| 1)
        INCR    R0
        MVO     R0, const_PL_FALL
@@n:    ADDI    #16, R3
        CMPI    #const_TRACKS + 256, R3
        BNEQ    @@t
        PULR    PC
        ENDP

;; HEADS_OFF: the cells the head cards show get their own words back.
HEADS_OFF: PROC
        PSHR    R5
@@l:    MVI     const_HD_N, R3
        TSTR    R3
        BEQ     @@x
        DECR    R3
        MVO     R3, const_HD_N
        ADDI    #const_HD_CELL, R3
        MVI@    R3, R0                  ; the cell
        MOVR    R0, R4
        ADDI    #const_CELLW, R4
        MVI@    R4, R1
        ADDI    #$200 + 40, R0
        CALL    GQPUT
        B       @@l
@@x:    PULR    PC
        ENDP

;; HEADS: this frame's heads (live tracks not trails, on the field, over
;; sky), a head card a cell: the cell's dirt and its heads, in the fire's
;; colour on the sky (a cell's dirt glows as a shell goes by).
HEADS:  PROC
        PSHR    R5
        CALL    HEADS_OFF
        MVII    #const_TRACKS, R3
@@t:    MOVR    R3, R4
        MVI@    R4, R0                  ; on,
        TSTR    R0
        BEQ     @@n
        MVI@    R4, R0                  ; not a trail,
        ANDI    #15, R0
        CMPI    #const_ST_SMOKE, R0
        BEQ     @@n
        MOVR    R3, R4                  ; on the field,
        ADDI    #const_TR_PX, R4
        MVI@    R4, R0
        MVI@    R4, R1
        CMPI    #160, R0
        BC      @@n
        CMPI    #80, R1
        BC      @@n
        PSHR    R3
        PSHR    R0
        PSHR    R1
        CALL    PIXGET                  ; over sky
        MOVR    R0, R2
        PULR    R1
        PULR    R0
        TSTR    R2
        BNEQ    @@pop
        CALL    PXAT                    ; R0 its word, R3 its bit, R2 its cell
        ANDI    #3, R0                  ; (which word of the cell: DIRT is 4-aligned)
        MVO     R0, const_PL_I
        MVO     R3, const_PL_J
        CLRR    R1                      ; a head card on this cell already?
@@f:    CMP     const_HD_N, R1
        BEQ     @@new
        MOVR    R1, R4
        ADDI    #const_HD_CELL, R4
        CMP@    R4, R2
        BEQ     @@add
        INCR    R1
        B       @@f
@@new:  CMPI    #const_N_HEAD, R1       ; no: a new one (none left: not drawn)
        BEQ     @@pop
        MOVR    R1, R4
        ADDI    #const_HD_CELL, R4
        MVO@    R2, R4
        MOVR    R1, R0
        INCR    R0
        MVO     R0, const_HD_N
        MOVR    R2, R0                  ; its pattern: the cell's dirt
        SLL     R0, 2
        ADDI    #const_DIRT, R0
        MOVR    R0, R4
        MOVR    R1, R0
        SLL     R0, 2
        ADDI    #const_HD_PAT, R0
        MOVR    R0, R5
        MVI@    R4, R0
        MVO@    R0, R5
        MVI@    R4, R0
        MVO@    R0, R5
        MVI@    R4, R0
        MVO@    R0, R5
        MVI@    R4, R0
        MVO@    R0, R5
@@add:  SLL     R1, 2                   ; the head's bit into it
        ADD     const_PL_I, R1
        ADDI    #const_HD_PAT, R1
        MVI@    R1, R0
        MVI     const_PL_J, R2
        COMR    R2
        ANDR    R2, R0
        COMR    R2
        XORR    R2, R0
        MVO@    R0, R1
@@pop:  PULR    R3
@@n:    ADDI    #16, R3
        CMPI    #const_TRACKS + 256, R3
        BNEQ    @@t
        CLRR    R3                      ; each card loaded, its cell on it
@@c:    CMP     const_HD_N, R3
        BEQ     @@x
        PSHR    R3
        MOVR    R3, R0                  ; GRAM $3800 + (G_HEAD + j) * 8, its pattern
        ADDI    #const_G_HEAD, R0
        SLL     R0, 2
        SLL     R0, 1
        ADDI    #$3800, R0
        MOVR    R3, R1
        SLL     R1, 2
        ADDI    #const_HD_PAT, R1
        CALL    GQPUT
        PULR    R3
        PSHR    R3
        MOVR    R3, R4                  ; the cell
        ADDI    #const_HD_CELL, R4
        MVI@    R4, R2
        MOVR    R3, R0                  ; its word: the card,
        ADDI    #const_G_HEAD, R0
        SLL     R0, 2
        SLL     R0, 1
        ADDI    #$800, R0
        MOVR    R2, R4                  ; the sky's background,
        ADDI    #const_SKYW, R4
        MVI@    R4, R1
        ANDI    #$3600, R1
        ADDR    R1, R0
        ADD     const_FV_FIREFG, R0     ; in the fire's colour (the cell's dirt glows with it)
@@w:    MOVR    R0, R1
        MOVR    R2, R0
        ADDI    #$200 + 40, R0
        CALL    GQPUT
        PULR    R3
        INCR    R3
        B       @@c
@@x:    PULR    PC
        ENDP
