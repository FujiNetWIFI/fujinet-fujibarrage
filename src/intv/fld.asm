;; ===========================================================================
;; fld.asm -- the field's pixels (src/atari/fld.s's, for the Intellivision).
;; Two planes of 160 x 80 bits in cartridge RAM: the dirt (DIRT) and the
;; fire (FXP: fire, shells, trails), 800 words apart, each laid out as the
;; GRAM is: a card cell 4 words, two rows a word (the even row in the low
;; byte), the left pixel bit 7. A pixel is sky (neither), dirt or fire,
;; never both (fire over dirt takes it): server/internal/view's PxSky,
;; PxDirt, PxFx. The tanks are MOBs, never in the planes.
;;
;; The ops (FV_OP, memory.bas's PO_*), a pixel or a span at a time:
;;   PO_SKY 0    sky                    PO_AIRFX 3  fire where there's no dirt
;;   PO_DIRT 1   dirt                   PO_CLRFX 4  fire goes out (to sky)
;;   PO_FX 2     fire
;; A word that changes queues its cell to be drawn again (DIRTY; cell.asm
;; draws them). Each cell knows (CELL) whether it is all sky or all dirt
;; while it isn't queued: a TERRAIN or SETTLE column skips those it
;; wouldn't change.
;;
;; From IntyBASIC: CALL / USR, arguments in R0-R3, the result in R0. CALL
;; is JSR R5: a routine that calls another saves R5 first.
;; ===========================================================================

;; DIRTY: cell R1 to be drawn again (queued once: its class unknown till
;; then). Keeps R0, R2, R3.
DIRTY:  PROC
        PSHR    R5
        PSHR    R0
        MOVR    R1, R4
        ADDI    #const_CELL, R4
        MVI@    R4, R0
        MOVR    R0, R5
        ANDI    #const_CF_DIRTY, R5
        BNEQ    @@x                     ; queued already
        ANDI    #const_CF_CARD + const_CF_FIRE + const_CF_APPROX, R0
        XORI    #const_CF_DIRTY, R0
        DECR    R4
        MVO@    R0, R4
        MVI     const_FV_DQT, R0
        MOVR    R0, R4
        ADDI    #const_DIRTYQ, R4
        MVO@    R1, R4
        INCR    R0
        CMPI    #208, R0
        BNEQ    @@w
        CLRR    R0
@@w:    MVO     R0, const_FV_DQT
@@x:    PULR    R0
        PULR    PC
        ENDP

;; OPW: the word at R0 (in DIRT; its fire 800 words on) by FV_OP, the
;; pixels in mask R3; R2 its cell. Writes and queues the cell only if a
;; pixel changes. Keeps R0, R2, R3.
OPW:    PROC
        PSHR    R5
        MOVR    R0, R4
        MVI@    R4, R1                  ; D
        ADDI    #const_FXP - const_DIRT - 1, R4
        MVI@    R4, R5                  ; F
        MVI     const_FV_OP, R4
        DECR    R4
        BMI     @@sky
        BEQ     @@dirt
        DECR    R4
        BEQ     @@fx
        DECR    R4
        BEQ     @@air
        MOVR    R3, R4                  ; fire out: F & ~M
        COMR    R4
        ANDR    R4, R5
        B       @@put
@@sky:  MOVR    R3, R4                  ; D & ~M, F & ~M
        COMR    R4
        ANDR    R4, R1
        ANDR    R4, R5
        B       @@put
@@dirt: MOVR    R3, R4                  ; D | M, F & ~M
        COMR    R4
        ANDR    R4, R1
        XORR    R3, R1
        ANDR    R4, R5
        B       @@put
@@fx:   MOVR    R3, R4                  ; D & ~M, F | M
        COMR    R4
        ANDR    R4, R1
        ANDR    R4, R5
        XORR    R3, R5
        B       @@put
@@air:  MOVR    R1, R4                  ; F | (M & ~D)
        COMR    R4
        ANDR    R3, R4
        COMR    R5
        ANDR    R5, R4                  ; (only the bits not fire yet)
        COMR    R5
        XORR    R4, R5
@@put:  MOVR    R0, R4
        CMP@    R4, R1
        BNEQ    @@chg
        ADDI    #const_FXP - const_DIRT - 1, R4
        CMP@    R4, R5
        BEQ     @@x
@@chg:  MOVR    R0, R4
        MVO@    R1, R4
        ADDI    #const_FXP - const_DIRT - 1, R4
        MVO@    R5, R4
        MOVR    R2, R1
        CALL    DIRTY
@@x:    PULR    PC
        ENDP

;; PXAT: pixel (R0, R1) (in the field): R0 its word in DIRT, R3 its bit
;; there, R2 its cell. R1 not kept.
PXAT:   PROC
        MOVR    R0, R3
        ANDI    #7, R3
        ADDI    #fld_bit, R3
        MOVR    R3, R4
        MVI@    R4, R3
        SLR     R0, 2
        SLR     R0, 1                   ; the cell's column
        MOVR    R1, R4
        ADDI    #fld_ycell, R4
        MVI@    R4, R2
        ADDR    R0, R2
        SLL     R0, 2
        MOVR    R1, R4
        ADDI    #fld_yofs, R4
        ADD@    R4, R0
        ADDI    #const_DIRT, R0
        SARC    R1, 1                   ; an odd row: the high byte
        BNC     @@x
        SWAP    R3
@@x:    JR      R5
        ENDP

;; PIXOP(x, y, op): pixel (x, y) by op. USR: what it was (0 sky, 1 dirt,
;; 2 fire); off the field: 0, and nothing done.
PIXOP:  PROC
        PSHR    R5
        MVO     R2, const_FV_OP
        CMPI    #160, R0
        BC      @@off                   ; (negative too: unsigned)
        CMPI    #80, R1
        BC      @@off
        CALL    PXAT
        MOVR    R0, R4
        MVI@    R4, R1
        ANDR    R3, R1
        BNEQ    @@d
        ADDI    #const_FXP - const_DIRT - 1, R4
        MVI@    R4, R1
        ANDR    R3, R1
        BEQ     @@s
        MVII    #2, R1
        B       @@op
@@d:    MVII    #1, R1
        B       @@op
@@s:    CLRR    R1
@@op:   PSHR    R1
        CALL    OPW
        PULR    R0
        PULR    PC
@@off:  CLRR    R0
        PULR    PC
        ENDP

;; PIXGET(x, y): USR what pixel (x, y) is (0 sky, 1 dirt, 2 fire; off the
;; field 0).
PIXGET: PROC
        PSHR    R5
        CMPI    #160, R0
        BC      @@s
        CMPI    #80, R1
        BC      @@s
        CALL    PXAT
        MOVR    R0, R4
        MVI@    R4, R1
        ANDR    R3, R1
        BNEQ    @@d
        ADDI    #const_FXP - const_DIRT - 1, R4
        MVI@    R4, R1
        ANDR    R3, R1
        BEQ     @@s
        MVII    #2, R0
        PULR    PC
@@d:    MVII    #1, R0
        PULR    PC
@@s:    CLRR    R0
        PULR    PC
        ENDP

;; SPAN(y, x0, x1, op): row y from x0 to x1 (both in; signed, clipped to the
;; field) by op.
SPAN:   PROC
        PSHR    R5
        MVO     R3, const_FV_OP
        CMPI    #80, R0
        BC      @@x                     ; (y < 0 too)
        TSTR    R1
        BPL     @@a
        CLRR    R1
@@a:    CMPI    #159, R2
        BLE     @@b
        MVII    #159, R2
@@b:    CMPR    R1, R2                  ; x1 - x0 < 0: nothing there
        BLT     @@x
        MOVR    R2, R3                  ; the last cell's mask
        ANDI    #7, R3
        ADDI    #fld_rmask, R3
        MOVR    R3, R4
        MVI@    R4, R3
        MVO     R3, const_SP_RM
        SLR     R2, 2
        SLR     R2, 1
        PSHR    R2                      ; the last cell's column
        MOVR    R1, R3                  ; the first cell's mask
        ANDI    #7, R3
        ADDI    #fld_lmask, R3
        MOVR    R3, R4
        MVI@    R4, R3
        SLR     R1, 2
        SLR     R1, 1                   ; the first cell's column
        PULR    R2
        SUBR    R1, R2
        MVO     R2, const_SP_N          ; cells after the first
        MOVR    R0, R4
        ADDI    #fld_ycell, R4
        MVI@    R4, R2
        ADDR    R1, R2                  ; the first cell
        SLL     R1, 2
        MOVR    R0, R4
        ADDI    #fld_yofs, R4
        ADD@    R4, R1
        ADDI    #const_DIRT, R1         ; its word
        SARC    R0, 1
        BNC     @@ev
        SWAP    R3                      ; an odd row: the high bytes
        MVI     const_SP_RM, R0
        SWAP    R0
        MVO     R0, const_SP_RM
        MVII    #$FF00, R0
        B       @@go
@@ev:   MVII    #$00FF, R0
@@go:   MVO     R0, const_SP_FULL
        MOVR    R1, R0
        MVI     const_SP_N, R1
        TSTR    R1
        BNEQ    @@many
        AND     const_SP_RM, R3         ; one cell: both ends in it
        CALL    OPW
        B       @@x
@@many: CALL    OPW
@@mid:  ADDI    #4, R0
        INCR    R2
        MVI     const_SP_N, R1
        DECR    R1
        MVO     R1, const_SP_N
        BEQ     @@last
        MVI     const_SP_FULL, R3
        CALL    OPW
        B       @@mid
@@last: MVI     const_SP_RM, R3
        CALL    OPW
@@x:    PULR    PC
        ENDP

;; HW: R0 = (R0 * R1 + 32) >> 6 (an ellipse row's half width: radius times
;; q32[i]). R1, R2 not kept.
HW:     PROC
        CLRR    R2
@@m:    SARC    R1, 1
        BNC     @@n
        ADDR    R0, R2
@@n:    SLL     R0, 1
        TSTR    R1
        BNEQ    @@m
        ADDI    #32, R2
        SLR     R2, 2
        SLR     R2, 2
        SLR     R2, 2
        MOVR    R2, R0
        JR      R5
        ENDP

;; ELLIPSE: the ellipse at (EL_X, EL_Y), radii EL_RX, EL_RY (1 at least),
;; by FV_OP; with EL_IRX not 0, less the ellipse of radii EL_IRX, EL_IRY
;; inside it (a blast that grew: its ring only). fld.s's rows: row dy is
;; rx * q32[i] / 64 each side, i = dy * 32 / ry rounded (stepped without
;; dividing).
ELLIPSE: PROC
        PSHR    R5
        CLRR    R0
        MVO     R0, const_EL_DY
        MVO     R0, const_EL_I
        MVO     R0, const_EL_I2
        MVI     const_EL_RY, R0
        SLR     R0, 1
        MVO     R0, const_EL_REM
        MVI     const_EL_IRY, R0
        SLR     R0, 1
        MVO     R0, const_EL_REM2
@@row:  MVI     const_EL_I, R1
        ADDI    #fld_q32, R1
        MOVR    R1, R4
        MVI@    R4, R1
        MVI     const_EL_RX, R0
        CALL    HW
        MVO     R0, const_EL_HW
        MVII    #$FFFF, R0              ; the inner's (-1: none on this row)
        MVI     const_EL_IRX, R1
        TSTR    R1
        BEQ     @@ni
        MVI     const_EL_DY, R2
        CMP     const_EL_IRY, R2
        BGT     @@ni
        MVI     const_EL_I2, R1
        ADDI    #fld_q32, R1
        MOVR    R1, R4
        MVI@    R4, R1
        MVI     const_EL_IRX, R0
        CALL    HW
@@ni:   MVO     R0, const_EL_IHW
        MVI     const_EL_Y, R0          ; the row above the centre,
        SUB     const_EL_DY, R0
        CALL    EROW
        MVI     const_EL_DY, R0         ; and below
        TSTR    R0
        BEQ     @@nx
        ADD     const_EL_Y, R0
        CALL    EROW
@@nx:   MVI     const_EL_DY, R0
        CMP     const_EL_RY, R0
        BC      @@x                     ; the last row done
        INCR    R0
        MVO     R0, const_EL_DY
        MVI     const_EL_REM, R0        ; i: dy * 32 / ry, rounded
        ADDI    #32, R0
        MVI     const_EL_I, R1
@@i:    CMP     const_EL_RY, R0
        BNC     @@id
        SUB     const_EL_RY, R0
        CMPI    #32, R1
        BC      @@i
        INCR    R1
        B       @@i
@@id:   MVO     R0, const_EL_REM
        MVO     R1, const_EL_I
        MVI     const_EL_IRY, R2        ; the inner's i
        TSTR    R2
        BEQ     @@row
        MVI     const_EL_REM2, R0
        ADDI    #32, R0
        MVI     const_EL_I2, R1
@@i2:   CMPR    R2, R0
        BNC     @@i2d
        SUBR    R2, R0
        CMPI    #32, R1
        BC      @@i2
        INCR    R1
        B       @@i2
@@i2d:  MVO     R0, const_EL_REM2
        MVO     R1, const_EL_I2
        B       @@row
@@x:    PULR    PC
        ENDP

;; EROW: row R0 of the ellipse: EL_X - EL_HW to EL_X + EL_HW, less EL_X -
;; EL_IHW to EL_X + EL_IHW when EL_IHW isn't -1.
EROW:   PROC
        PSHR    R5
        MVO     R0, const_EL_ROW
        MVI     const_EL_IHW, R1
        TSTR    R1
        BMI     @@full
        MVI     const_EL_X, R1          ; the left part
        SUB     const_EL_HW, R1
        MVI     const_EL_X, R2
        SUB     const_EL_IHW, R2
        DECR    R2
        MVI     const_FV_OP, R3
        CALL    SPAN
        MVI     const_EL_ROW, R0        ; the right
        MVI     const_EL_X, R1
        ADD     const_EL_IHW, R1
        INCR    R1
        MVI     const_EL_X, R2
        ADD     const_EL_HW, R2
        MVI     const_FV_OP, R3
        CALL    SPAN
        PULR    PC
@@full: MVI     const_EL_X, R1
        SUB     const_EL_HW, R1
        MVI     const_EL_X, R2
        ADD     const_EL_HW, R2
        MVI     const_FV_OP, R3
        CALL    SPAN
        PULR    PC
        ENDP

;; ELL(x, y, rx, ry): the whole ellipse by FV_OP (no inner), from BASIC.
ELL:    PROC
        MVO     R0, const_EL_X
        MVO     R1, const_EL_Y
        MVO     R2, const_EL_RX
        MVO     R3, const_EL_RY
        CLRR    R0
        MVO     R0, const_EL_IRX
        MVO     R0, const_EL_IRY
        B       ELLIPSE                 ; (it returns to our caller)
        ENDP

;; CRUN(top, end): rows top to end - 1 are dirt in COLUMN's image (COLB).
;; The spans of an entry don't overlap, so their bits are added.
CRUN:   PROC
        PSHR    R5
        CMPI    #81, R1
        BNC     @@ok
        MVII    #80, R1
@@ok:   CMPR    R1, R0
        BC      @@x                     ; top >= end: none
@@c:    MOVR    R0, R2                  ; the cell row
        SLR     R2, 2
        SLR     R2, 1
        MOVR    R2, R3
        SLL     R3, 2
        SLL     R3, 1
        ADDI    #8, R3                  ; the next one's top row
        MOVR    R1, R5                  ; rows of it to the end
        SUBR    R3, R5
        BPL     @@all
        ADDI    #8, R5
        B       @@m
@@all:  MVII    #8, R5
@@m:    ADDI    #fld_to, R5
        MOVR    R5, R4
        MVI@    R4, R5                  ; rows before the end
        MOVR    R0, R4
        ANDI    #7, R4
        ADDI    #fld_from, R4
        AND@    R4, R5                  ; and from the top
        ADDI    #const_COLB, R2
        MOVR    R2, R4
        XOR@    R4, R5
        DECR    R4
        MVO@    R5, R4
        MOVR    R3, R0                  ; on from the next one's top
        CMPR    R1, R0
        BNC     @@c
@@x:    PULR    PC
        ENDP

;; COLUMN(x, p): column x becomes the TERRAIN entry at p (a byte a word):
;; dirt and sky, no fire. USR: the entry's length.
COLUMN: PROC
        PSHR    R5
        MVO     R0, const_CO_X
        CLRR    R2                      ; the image: no dirt
        MVII    #const_COLB, R4
        MVII    #10, R3
@@z:    MVO@    R2, R4
        DECR    R3
        BNEQ    @@z
        MOVR    R1, R4
        MVI@    R4, R0
        ANDI    #255, R0
        CMPI    #$F0, R0
        BEQ     @@e1                    ; no dirt
        BC      @@spans
        MVII    #80, R1                 ; dirt from row R0 down
        CALL    CRUN
@@e1:   MVII    #1, R0
        PSHR    R0
        B       @@put
@@spans:
        SUBI    #$F0, R0                ; n spans: top, end each
        MOVR    R0, R2
        SLL     R0, 1
        INCR    R0
        PSHR    R0                      ; the length, 1 + 2n
@@sp:   MVI@    R4, R0
        MVI@    R4, R1
        ANDI    #255, R0
        ANDI    #255, R1
        PSHR    R4
        PSHR    R2
        CALL    CRUN
        PULR    R2
        PULR    R4
        DECR    R2
        BNEQ    @@sp
@@put:  MVI     const_CO_X, R0          ; the column's bits, spread
        MOVR    R0, R1
        ANDI    #7, R1
        SLL     R1, 2
        ADDI    #fld_exp, R1
        MVO     R1, const_CO_EXP
        MOVR    R1, R4                  ; both rows' bits, and not them
        ADDI    #3, R4
        MVI@    R4, R3
        MVO     R3, const_CO_MM
        COMR    R3
        MVO     R3, const_CO_NM
        SLR     R0, 2
        SLR     R0, 1
        MVO     R0, const_CO_CELL
        SLL     R0, 2
        ADDI    #const_DIRT, R0
        MVO     R0, const_CO_W
        CLRR    R0
        MVO     R0, const_CO_CY
@@cell: MVI     const_CO_CY, R1         ; this cell row's bits
        ADDI    #const_COLB, R1
        MOVR    R1, R4
        MVI@    R4, R1
        MVI     const_CO_CELL, R4       ; all sky, or all dirt, and staying so: skip it
        ADDI    #const_CELL, R4
        MVI@    R4, R2
        ANDI    #const_CF_CLASS + const_CF_DIRTY, R2
        CMPI    #const_CL_SKY, R2
        BNEQ    @@nsk
        TSTR    R1
        BEQ     @@next
        B       @@wr
@@nsk:  CMPI    #const_CL_DIRT, R2
        BNEQ    @@wr
        CMPI    #255, R1
        BEQ     @@next
@@wr:   MVI     const_CO_W, R0
        MOVR    R0, R4
        MVI     const_CO_MM, R3         ; R3 the column's bits (both rows of a word)
        TSTR    R1
        BEQ     @@sky
        CMPI    #255, R1
        BEQ     @@dirt
        CLRR    R2                      ; some of each: a word at a time
        MVO     R2, const_CO_CHG
        MVI     const_CO_NM, R3
@@wd:   MOVR    R1, R4                  ; R0 the word, R1 the bits left (2 a word)
        ANDI    #3, R4
        ADD     const_CO_EXP, R4
        MVI@    R4, R5                  ; the new bits
        MOVR    R0, R4
        MVI@    R4, R2                  ; D
        ANDR    R3, R2
        XORR    R5, R2
        DECR    R4
        CMP@    R4, R2
        BEQ     @@sd
        DECR    R4
        MVO@    R2, R4
        MVO     R4, const_CO_CHG        ; (not 0: an address)
@@sd:   INCR    R0
        SLR     R1, 2
        MOVR    R0, R4
        ANDI    #3, R4
        BNEQ    @@wd
        MVI     const_CO_CHG, R1
        B       @@fx
@@sky:  CLRR    R1                      ; all sky: those bits off (R1: changed)
        MVII    #4, R5
@@s1:   MVI@    R4, R2
        MOVR    R2, R0
        ANDR    R3, R0
        BEQ     @@s2
        XORR    R0, R2
        DECR    R4
        MVO@    R2, R4
        MOVR    R4, R1
@@s2:   DECR    R5
        BNEQ    @@s1
        B       @@fx
@@dirt: CLRR    R1                      ; all dirt: those bits on
        MVII    #4, R5
@@d1:   MVI@    R4, R2
        MOVR    R2, R0
        ANDR    R3, R0
        XORR    R3, R0                  ; the bits not on yet
        BEQ     @@d2
        XORR    R0, R2
        DECR    R4
        MVO@    R2, R4
        MOVR    R4, R1
@@d2:   DECR    R5
        BNEQ    @@d1
@@fx:   MVI     const_CO_W, R4          ; any fire in the column here? (rarely)
        ADDI    #const_FXP - const_DIRT, R4
        MVI     const_CO_MM, R3
        MVII    #4, R5
@@f1:   MVI@    R4, R2
        ANDR    R3, R2
        BEQ     @@f2
        DECR    R4                      ; off with it
        MVI@    R4, R0
        XORR    R2, R0
        DECR    R4
        MVO@    R0, R4
        MOVR    R4, R1
@@f2:   DECR    R5
        BNEQ    @@f1
        TSTR    R1
        BEQ     @@next
        MVI     const_CO_CELL, R1
        CALL    DIRTY
@@next: MVI     const_CO_W, R0          ; the cell row below
        ADDI    #80, R0
        MVO     R0, const_CO_W
        MVI     const_CO_CELL, R0
        ADDI    #20, R0
        MVO     R0, const_CO_CELL
        MVI     const_CO_CY, R0
        INCR    R0
        MVO     R0, const_CO_CY
        CMPI    #10, R0
        BNEQ    @@cell
        PULR    R0
        PULR    PC
        ENDP

;; COLS(x, n, p): columns x to x + n - 1 become the n TERRAIN entries at p
;; (a byte a word): dirt and sky, no fire. USR: the entries' length. A cell
;; column (8 columns) at a time: a column whose entry is one run to the
;; bottom (or none, the usual) is built with the others a cell at a time;
;; a column with spans goes through COLUMN. Columns past the field are
;; skipped (their entries counted).
COLS:   PROC
        PSHR    R5
        MVO     R2, const_CS_P0
        MVO     R2, const_CS_P
        MVO     R0, const_CS_X
        ADDR    R0, R1
        MVO     R1, const_CS_END
@@grp:  MVI     const_CS_X, R0
        CMP     const_CS_END, R0
        BC      @@done
        MOVR    R0, R1                  ; this cell column's last + 1, or the end
        ANDI    #$FFF8, R1
        ADDI    #8, R1
        CMP     const_CS_END, R1
        BNC     @@ge
        MVI     const_CS_END, R1
@@ge:   MVO     R1, const_CS_GE
        CLRR    R0
        MVO     R0, const_CS_CM
        MVO     R0, const_CS_SLOW
        MVO     R0, const_CS_TMAX
        MVII    #80, R0
        MVO     R0, const_CS_TMIN
@@ent:  MVI     const_CS_X, R0          ; its entries
        CMP     const_CS_GE, R0
        BC      @@built
        MVI     const_CS_P, R4
        MVI@    R4, R1
        ANDI    #255, R1
        CMPI    #$F0, R1
        BC      @@nr
@@run:  MVO     R4, const_CS_P          ; one run from R1 down
        CMPI    #160, R0
        BC      @@nx                    ; (past the field)
        MOVR    R0, R2
        ANDI    #7, R2
        MOVR    R2, R3
        ADDI    #const_CS_TOP, R3
        MVO@    R1, R3
        ADDI    #fld_bit, R2
        MVI@    R2, R2
        ADD     const_CS_CM, R2
        MVO     R2, const_CS_CM
        CMP     const_CS_TMIN, R1
        BGE     @@t1
        MVO     R1, const_CS_TMIN
@@t1:   CMP     const_CS_TMAX, R1
        BLE     @@nx
        MVO     R1, const_CS_TMAX
        B       @@nx
@@nr:   BNEQ    @@sp
        MVII    #80, R1                 ; no dirt: a run from the bottom
        B       @@run
@@sp:   DECR    R4                      ; spans: COLUMN's, after
        CMPI    #160, R0
        BC      @@sk
        MOVR    R0, R2
        ANDI    #7, R2
        MOVR    R2, R3
        ADDI    #const_CS_SLOWP, R3
        MVO@    R4, R3
        ADDI    #fld_bit, R2
        MVI@    R2, R2
        ADD     const_CS_SLOW, R2
        MVO     R2, const_CS_SLOW
@@sk:   SUBI    #$F0, R1
        SLL     R1, 1
        INCR    R1
        ADDR    R1, R4
        MVO     R4, const_CS_P
@@nx:   MVI     const_CS_X, R0
        INCR    R0
        MVO     R0, const_CS_X
        B       @@ent
@@built:
        MVI     const_CS_CM, R0
        TSTR    R0
        BEQ     @@slow
        MOVR    R0, R1                  ; CM in a word, and not it
        SWAP    R1
        ADDR    R0, R1
        MVO     R1, const_CS_CMW
        COMR    R1
        MVO     R1, const_CS_NCMW
        MVI     const_CS_GE, R0         ; the cell column's top cell
        DECR    R0
        SLR     R0, 2
        SLR     R0, 1
        MVO     R0, const_CS_CELL
        SLL     R0, 2
        ADDI    #const_DIRT, R0
        MVO     R0, const_CS_W
        CLRR    R0
        MVO     R0, const_CS_BASE
@@cell: MVI     const_CS_BASE, R0
        MOVR    R0, R1
        ADDI    #8, R1
        CMP     const_CS_TMIN, R1       ; every top below it: sky
        BLE     @@csky
        CMP     const_CS_TMAX, R0       ; every top above it: dirt
        BGE     @@cdirt
        CLRR    R1                      ; some of each: each row's columns
        MVII    #const_CS_ROW, R4
        MVO@    R1, R4
        MVO@    R1, R4
        MVO@    R1, R4
        MVO@    R1, R4
        MVO@    R1, R4
        MVO@    R1, R4
        MVO@    R1, R4
        MVO@    R1, R4
        MVII    #const_CS_TOP, R4       ; a column's bit goes in the row its dirt starts
        MVII    #fld_bit, R5
        MVI     const_CS_CM, R3
@@col:  MVI@    R5, R2                  ; its bit
        MVI@    R4, R1                  ; its top
        MOVR    R3, R0
        ANDR    R2, R0
        BEQ     @@cn
        SUB     const_CS_BASE, R1
        BPL     @@c0
        CLRR    R1
@@c0:   CMPI    #8, R1
        BGE     @@cn
        ADDI    #const_CS_ROW, R1
        ADD@    R1, R2
        MVO@    R2, R1
@@cn:   CMPI    #fld_bit + 8, R5
        BNEQ    @@col
        MVII    #const_CS_ROW, R4       ; then each row has all those above it
        MVII    #const_CS_NW, R5
        CLRR    R0
        MVII    #4, R3
@@pr:   ADD@    R4, R0                  ; the even row
        MOVR    R0, R1
        ADD@    R4, R0                  ; the odd
        MOVR    R0, R2
        SWAP    R2
        ADDR    R2, R1
        MVO@    R1, R5
        DECR    R3
        BNEQ    @@pr
        B       @@merge
@@csky: CLRR    R1
        B       @@cu
@@cdirt:
        MVI     const_CS_CMW, R1
@@cu:   MVII    #const_CS_NW, R4
        MVO@    R1, R4
        MVO@    R1, R4
        MVO@    R1, R4
        MVO@    R1, R4
@@merge:                                ; D: the columns' bits replaced; F: off there
        CLRR    R0
        MVO     R0, const_CS_CHG
        MVI     const_CS_W, R4
        MVII    #const_CS_NW, R5
        MVI     const_CS_NCMW, R3
        MVII    #4, R1
@@mw:   MVI@    R4, R2
        MOVR    R2, R0
        ANDR    R3, R2
        ADD@    R5, R2
        CMPR    R2, R0
        BEQ     @@m1
        DECR    R4
        MVO@    R2, R4
        MVO     R4, const_CS_CHG
@@m1:   DECR    R1
        BNEQ    @@mw
        ADDI    #const_FXP - const_DIRT - 4, R4
        MVI     const_CS_CMW, R3
        MVII    #4, R1
@@mf:   MVI@    R4, R2
        MOVR    R2, R0
        ANDR    R3, R0
        BEQ     @@m2
        XORR    R0, R2
        DECR    R4
        MVO@    R2, R4
        MVO     R4, const_CS_CHG
@@m2:   DECR    R1
        BNEQ    @@mf
        MVI     const_CS_CHG, R0
        TSTR    R0
        BEQ     @@cnx
        MVI     const_CS_CELL, R1
        CALL    DIRTY
@@cnx:  MVI     const_CS_W, R0          ; the cell below
        ADDI    #80, R0
        MVO     R0, const_CS_W
        MVI     const_CS_CELL, R0
        ADDI    #20, R0
        MVO     R0, const_CS_CELL
        MVI     const_CS_BASE, R0
        ADDI    #8, R0
        MVO     R0, const_CS_BASE
        CMPI    #80, R0
        BNEQ    @@cell
@@slow: MVI     const_CS_SLOW, R0       ; the columns with spans
        TSTR    R0
        BEQ     @@grp
        CLRR    R1
@@sl:   MVI     const_CS_SLOW, R0
        MOVR    R1, R4
        ADDI    #fld_bit, R4
        AND@    R4, R0
        BEQ     @@sn
        PSHR    R1
        MOVR    R1, R4
        ADDI    #const_CS_SLOWP, R4
        MVI     const_CS_GE, R0         ; its x
        DECR    R0
        ANDI    #$FFF8, R0
        ADDR    R1, R0
        MVI@    R4, R1
        CALL    COLUMN
        PULR    R1
@@sn:   INCR    R1
        CMPI    #8, R1
        BNEQ    @@sl
        B       @@grp
@@done: MVI     const_CS_P, R0
        SUB     const_CS_P0, R0
        PULR    PC
        ENDP

;; TERRAIN(n): a G frame (NBUF: type, base, tv, flags, x16, entries; n its
;; length) into the planes: its entries counted, then COLS.
TERRAIN: PROC
        PSHR    R5
        ADDI    #const_NBUF, R0
        MOVR    R0, R3                  ; the frame's end
        MVII    #const_NBUF + 6, R4
        CLRR    R1                      ; entries
@@c:    CMPR    R3, R4
        BC      @@go
        MVI@    R4, R0
        ANDI    #255, R0
        INCR    R1
        CMPI    #$F1, R0
        BNC     @@c
        SUBI    #$F0, R0
        SLL     R0, 1
        ADDR    R0, R4
        B       @@c
@@go:   MVI     const_NBUF + 5, R0      ; x
        ANDI    #255, R0
        SWAP    R0
        ADD     const_NBUF + 4, R0
        MVII    #const_NBUF + 6, R2
        CALL    COLS
        PULR    PC
        ENDP

;; FLD_CLEAR (boot's first): all of the cartridge's RAM 0 but the test
;; hook (a reset leaves it as it was, and on a console CONFIG ran in it
;; first: the frame handler's queues and the sound would run on what it
;; left); then no dirt, no fire, every cell all sky and not queued (TERRAIN
;; and SETTLE columns write the field from then on).
FLD_CLEAR: PROC
        CLRR    R0
        MVII    #const_ST, R4
        MVII    #(const_TEST_HOOK - const_ST) / 4, R1
@@z:    MVO@    R0, R4
        MVO@    R0, R4
        MVO@    R0, R4
        MVO@    R0, R4
        DECR    R1
        BNEQ    @@z
        MVII    #const_CELL, R4
        MVII    #const_CL_SKY, R0
        MVII    #200, R1
@@c:    MVO@    R0, R4
        DECR    R1
        BNEQ    @@c
        CLRR    R0
        MVO     R0, const_FV_DQH
        MVO     R0, const_FV_DQT
        JR      R5
        ENDP
