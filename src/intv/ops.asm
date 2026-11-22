;; ===========================================================================
;; ops.asm -- the script's paths (OP_PATH), flows (OP_FLOW) and beams
;; (OP_BEAM) on the planes (src/atari/ops.s's; server/internal/view's
;; pixel rules). Paths and flows read their bytes where they lie in
;; SCRIPT; at a segment's end they finish (OPS_SEG_END).
;;
;;   a path's head    a roller over sky, a digger over sky or dirt: fire;
;;                    moving on, its fire back to sky (a digger bores)
;;   a flow's cells   fire over the air (sky), mud as dirt over air or fire;
;;                    a fire's cells back to sky at its end
;;   a beam           fire over everything on its line; at its end, sky
;;
;; OPS_PATH, OPS_FLOW, OPS_BEAM: the op's arguments at R4; R4 past them.
;; ===========================================================================

;; @@xy: R0, R1 = path R3's x, y (R3: its slot)
PA_XY:  PROC
        MOVR    R3, R4
        ADDI    #const_PA_X, R4
        MVI@    R4, R0
        ADDI    #const_PA_Y - const_PA_X - 1, R4
        MVI@    R4, R1
        JR      R5
        ENDP

;; PHEAD: path R3's head drawn (a roller over sky, a digger over sky or
;; dirt). R3 kept.
PHEAD:  PROC
        PSHR    R5
        MOVR    R3, R4
        ADDI    #const_PA_DR, R4
        CLRR    R0
        MVO@    R0, R4
        CALL    PA_XY
        PSHR    R3
        CALL    PIXGET
        PULR    R3
        TSTR    R0
        BEQ     @@draw
        CMPI    #1, R0
        BNEQ    @@x
        MOVR    R3, R4
        ADDI    #const_PA_ST, R4
        MVI@    R4, R0
        CMPI    #const_ST_DIGGER, R0
        BNEQ    @@x
@@draw: CALL    PA_XY
        PSHR    R3
        MVII    #const_PO_FX, R2
        CALL    PIXOP
        PULR    R3
        MOVR    R3, R4
        ADDI    #const_PA_DR, R4
        MVII    #1, R0
        MVO@    R0, R4
@@x:    PULR    PC
        ENDP

;; PLEAVE: path R3's head moves on: its fire back to sky. R3 kept.
PLEAVE: PROC
        PSHR    R5
        MOVR    R3, R4
        ADDI    #const_PA_DR, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@x
        CLRR    R0
        DECR    R4
        MVO@    R0, R4
        CALL    PA_XY
        PSHR    R3
        MVII    #const_PO_CLRFX, R2
        CALL    PIXOP
        PULR    R3
@@x:    PULR    PC
        ENDP

;; PEND: path R3 stops. R3 kept.
PEND:   PROC
        PSHR    R5
        MOVR    R3, R4
        ADDI    #const_PA_ON, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@x
        CALL    PLEAVE
        MOVR    R3, R4
        ADDI    #const_PA_ON, R4
        CLRR    R0
        MVO@    R0, R4
@@x:    PULR    PC
        ENDP

;; OPS_PATH: slot (0-3), style, x16, y16, n, n steps (a byte each: dx in
;; the high nibble, dy in the low, signed)
OPS_PATH: PROC
        PSHR    R5
        MVI@    R4, R3
        ANDI    #const_NPATH - 1, R3
        PSHR    R4
        CALL    PEND
        PULR    R4
        MOVR    R3, R5                  ; (R5 as a pointer: no calls till PHEAD)
        ADDI    #const_PA_ST, R5
        MVI@    R4, R0
        MVO@    R0, R5
        MVI@    R4, R0                  ; x
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        ADDI    #const_PA_X - const_PA_ST - 1, R5
        MVO@    R0, R5
        MVI@    R4, R0                  ; y
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        ADDI    #const_PA_Y - const_PA_X - 1, R5
        MVO@    R0, R5
        MVI@    R4, R0                  ; n
        ADDI    #const_PA_N - const_PA_Y - 1, R5
        MVO@    R0, R5
        ADDI    #const_PA_P - const_PA_N - 1, R5
        MVO@    R4, R5                  ; its steps start here
        ADDR    R0, R4                  ; past them
        MOVR    R3, R5
        ADDI    #const_PA_ON, R5
        MVII    #1, R0
        MVO@    R0, R5
        PSHR    R4
        CALL    PHEAD
        PULR    R4
        PULR    PC
        ENDP

;; PSTEP: every path a step
PSTEP:  PROC
        PSHR    R5
        CLRR    R3
@@l:    MOVR    R3, R4
        ADDI    #const_PA_ON, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@n
        MOVR    R3, R4
        ADDI    #const_PA_N, R4
        MVI@    R4, R0
        TSTR    R0
        BNEQ    @@mv
        CALL    PEND
        B       @@n
@@mv:   DECR    R0
        DECR    R4
        MVO@    R0, R4
        CALL    PLEAVE
        MOVR    R3, R4                  ; its next step byte
        ADDI    #const_PA_P, R4
        MVI@    R4, R1
        MOVR    R1, R2
        INCR    R2
        DECR    R4
        MVO@    R2, R4
        MVI@    R1, R1
        MOVR    R1, R0                  ; dx: the high nibble, signed
        SLR     R0, 2
        SLR     R0, 2
        CMPI    #8, R0
        BNC     @@dx
        SUBI    #16, R0
@@dx:   MOVR    R3, R4
        ADDI    #const_PA_X, R4
        ADD@    R4, R0
        DECR    R4
        MVO@    R0, R4
        ANDI    #15, R1                 ; dy: the low
        CMPI    #8, R1
        BNC     @@dy
        SUBI    #16, R1
@@dy:   MOVR    R3, R4
        ADDI    #const_PA_Y, R4
        ADD@    R4, R1
        DECR    R4
        MVO@    R1, R4
        CALL    PHEAD
@@n:    INCR    R3
        CMPI    #const_NPATH, R3
        BNEQ    @@l
        PULR    PC
        ENDP

;; FCELLS: flow R3's cells PL_I to PL_J - 1 by op PL_K
FCELLS: PROC
        PSHR    R5
@@l:    MVI     const_PL_I, R0
        CMP     const_PL_J, R0
        BC      @@x
        MOVR    R0, R1                  ; 3 bytes a cell: x16, y8
        SLL     R1, 1
        ADDR    R0, R1
        MOVR    R3, R4
        ADDI    #const_FL_P, R4
        ADD@    R4, R1
        MOVR    R1, R4
        MVI@    R4, R0
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        MVI@    R4, R1
        MVI     const_PL_K, R2
        PSHR    R3
        CALL    PIXOP
        PULR    R3
        MVI     const_PL_I, R0
        INCR    R0
        MVO     R0, const_PL_I
        B       @@l
@@x:    PULR    PC
        ENDP

;; FSHOW: flow R3's first R0 cells in (fire over the air, mud as dirt)
FSHOW:  PROC
        PSHR    R5
        MOVR    R3, R4
        ADDI    #const_FL_SH, R4
        CMP@    R4, R0                  ; no more than shown already: nothing
        BLE     @@x
        DECR    R4
        MVI@    R4, R1
        DECR    R4
        MVO@    R0, R4
        MVO     R1, const_PL_I
        MVO     R0, const_PL_J
        MVII    #const_PO_AIRFX, R0
        MOVR    R3, R4
        ADDI    #const_FL_KIND, R4
        MVI@    R4, R1
        CMPI    #const_FLOW_MUD, R1
        BNEQ    @@f
        MVII    #const_PO_DIRT, R0
@@f:    MVO     R0, const_PL_K
        CALL    FCELLS
@@x:    PULR    PC
        ENDP

;; FEND: flow R3 all in; a fire's cells back to sky; it is gone
FEND:   PROC
        PSHR    R5
        MOVR    R3, R4
        ADDI    #const_FL_N, R4
        MVI@    R4, R0
        CALL    FSHOW
        MOVR    R3, R4
        ADDI    #const_FL_KIND, R4
        MVI@    R4, R0
        CMPI    #const_FLOW_MUD, R0
        BEQ     @@off
        CLRR    R0
        MVO     R0, const_PL_I
        MOVR    R3, R4
        ADDI    #const_FL_N, R4
        MVI@    R4, R0
        MVO     R0, const_PL_J
        MVII    #const_PO_CLRFX, R0
        MVO     R0, const_PL_K
        CALL    FCELLS
@@off:  MOVR    R3, R4
        ADDI    #const_FL_ON, R4
        CLRR    R0
        MVO@    R0, R4
        PULR    PC
        ENDP

;; OPS_FLOW: kind, frames, hold, n, n cells (x16, y8)
OPS_FLOW: PROC
        PSHR    R5
        CLRR    R3                      ; a free one (none: the last ends now)
@@f:    MOVR    R3, R0
        ADDI    #const_FL_ON, R0
        MOVR    R0, R1
        MVI@    R1, R0
        TSTR    R0
        BEQ     @@got
        INCR    R3
        CMPI    #const_NFLOW, R3
        BNEQ    @@f
        DECR    R3
        PSHR    R4
        CALL    FEND
        PULR    R4
@@got:  MOVR    R3, R5                  ; (no calls from here)
        ADDI    #const_FL_KIND, R5
        MVI@    R4, R0
        MVO@    R0, R5
        MVI@    R4, R0                  ; frames (0: 1)
        TSTR    R0
        BNEQ    @@fr
        INCR    R0
@@fr:   ADDI    #const_FL_FRM - const_FL_KIND - 1, R5
        MVO@    R0, R5
        CLRR    R0                      ; none so far
        ADDI    #const_FL_F - const_FL_FRM - 1, R5
        MVO@    R0, R5
        MVI@    R4, R0                  ; hold
        ADDI    #const_FL_HOLD - const_FL_F - 1, R5
        MVO@    R0, R5
        MVI@    R4, R0                  ; n
        ADDI    #const_FL_N - const_FL_HOLD - 1, R5
        MVO@    R0, R5
        CLRR    R1
        ADDI    #const_FL_SH - const_FL_N - 1, R5
        MVO@    R1, R5
        ADDI    #const_FL_ACC - const_FL_SH - 1, R5
        MVO@    R1, R5
        ADDI    #const_FL_P - const_FL_ACC - 1, R5
        MVO@    R4, R5                  ; its cells start here
        MOVR    R0, R1                  ; past them: 3 a cell
        SLL     R1, 1
        ADDR    R0, R1
        ADDR    R1, R4
        MOVR    R3, R5
        ADDI    #const_FL_ON, R5
        MVII    #1, R0
        MVO@    R0, R5
        PULR    PC
        ENDP

;; FSTEP: every flow a frame: n x frames so far / frames of its cells in,
;; then held, then (a fire) out
FSTEP:  PROC
        PSHR    R5
        CLRR    R3
@@l:    MOVR    R3, R4
        ADDI    #const_FL_ON, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@n
        MOVR    R3, R4                  ; f < frames: a frame more of it
        ADDI    #const_FL_F, R4
        MVI@    R4, R0
        MOVR    R3, R4
        ADDI    #const_FL_FRM, R4
        CMP@    R4, R0
        BC      @@hold
        INCR    R0
        MOVR    R3, R4
        ADDI    #const_FL_F, R4
        MVO@    R0, R4
        MOVR    R3, R4                  ; acc += n; a cell more each frames of it
        ADDI    #const_FL_ACC, R4
        MVI@    R4, R0
        MOVR    R3, R4
        ADDI    #const_FL_N, R4
        ADD@    R4, R0
        MOVR    R3, R4
        ADDI    #const_FL_SH, R4
        MVI@    R4, R1
        MOVR    R3, R4
        ADDI    #const_FL_FRM, R4
        MVI@    R4, R2
@@sub:  CMPR    R2, R0
        BNC     @@sh
        SUBR    R2, R0
        INCR    R1
        B       @@sub
@@sh:   MOVR    R3, R4
        ADDI    #const_FL_ACC, R4
        MVO@    R0, R4
        MOVR    R1, R0
        CALL    FSHOW
        B       @@end
@@hold: MOVR    R3, R4
        ADDI    #const_FL_HOLD, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@end
        DECR    R0
        DECR    R4
        MVO@    R0, R4
@@end:  MOVR    R3, R4                  ; all in and held: done
        ADDI    #const_FL_F, R4
        MVI@    R4, R0
        MOVR    R3, R4
        ADDI    #const_FL_FRM, R4
        CMP@    R4, R0
        BNC     @@n
        MOVR    R3, R4
        ADDI    #const_FL_HOLD, R4
        MVI@    R4, R0
        TSTR    R0
        BNEQ    @@n
        CALL    FEND
@@n:    INCR    R3
        CMPI    #const_NFLOW, R3
        BNEQ    @@l
        PULR    PC
        ENDP

;; LINE: (LN_X0, LN_Y0) to (LN_X1, LN_Y1) by op LN_OP, Bresenham as the
;; reference view draws it (4096 pixels at most).
LINE:   PROC
        PSHR    R5
        MVII    #1, R2                  ; dx, sx
        MVI     const_LN_X1, R0
        SUB     const_LN_X0, R0
        BPL     @@px
        NEGR    R0
        MVII    #$FFFF, R2
@@px:   MVO     R0, const_LN_DX
        MVO     R2, const_LN_SX
        MVII    #1, R2                  ; dy, sy
        MVI     const_LN_Y1, R0
        SUB     const_LN_Y0, R0
        BPL     @@py
        NEGR    R0
        MVII    #$FFFF, R2
@@py:   MVO     R0, const_LN_DY
        MVO     R2, const_LN_SY
        MVI     const_LN_DX, R0         ; err = dx - dy
        SUB     const_LN_DY, R0
        MVO     R0, const_LN_ERR
        MVII    #4096, R0
        MVO     R0, const_LN_CNT
@@l:    MVI     const_LN_X0, R0
        MVI     const_LN_Y0, R1
        MVI     const_LN_OP, R2
        CALL    PIXOP
        MVI     const_LN_X0, R0         ; the end?
        CMP     const_LN_X1, R0
        BNEQ    @@go
        MVI     const_LN_Y0, R0
        CMP     const_LN_Y1, R0
        BEQ     @@x
@@go:   MVI     const_LN_CNT, R0
        DECR    R0
        MVO     R0, const_LN_CNT
        BEQ     @@x
        MVI     const_LN_ERR, R1        ; e2 = 2 err
        SLL     R1, 1
        MOVR    R1, R0                  ; e2 > -dy: x moves
        ADD     const_LN_DY, R0
        BLE     @@ny
        MVI     const_LN_ERR, R0
        SUB     const_LN_DY, R0
        MVO     R0, const_LN_ERR
        MVI     const_LN_X0, R0
        ADD     const_LN_SX, R0
        MVO     R0, const_LN_X0
@@ny:   CMP     const_LN_DX, R1         ; e2 < dx: y moves
        BGE     @@l
        MVI     const_LN_ERR, R0
        ADD     const_LN_DX, R0
        MVO     R0, const_LN_ERR
        MVI     const_LN_Y0, R0
        ADD     const_LN_SY, R0
        MVO     R0, const_LN_Y0
        B       @@l
@@x:    PULR    PC
        ENDP

;; BLINE: beam R3's line by op R0
BLINE:  PROC
        PSHR    R5
        MVO     R0, const_LN_OP
        MOVR    R3, R4
        ADDI    #const_BE_X0, R4
        MVI@    R4, R0
        MVO     R0, const_LN_X0
        ADDI    #const_BE_Y0 - const_BE_X0 - 1, R4
        MVI@    R4, R0
        MVO     R0, const_LN_Y0
        ADDI    #const_BE_X1 - const_BE_Y0 - 1, R4
        MVI@    R4, R0
        MVO     R0, const_LN_X1
        ADDI    #const_BE_Y1 - const_BE_X1 - 1, R4
        MVI@    R4, R0
        MVO     R0, const_LN_Y1
        PSHR    R3
        CALL    LINE
        PULR    R3
        PULR    PC
        ENDP

;; BOFF: beam R3's fire back to sky, and it is gone
BOFF:   PROC
        PSHR    R5
        MOVR    R3, R4
        ADDI    #const_BE_LEFT, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@x
        CLRR    R0
        DECR    R4
        MVO@    R0, R4
        MVII    #const_PO_CLRFX, R0
        CALL    BLINE
@@x:    PULR    PC
        ENDP

;; OPS_BEAM: x0 16, y0 16, x1 16, y1 16, kind, frames
OPS_BEAM: PROC
        PSHR    R5
        CLRR    R3                      ; the first, unless it is on
        MVI     const_BE_LEFT, R0
        TSTR    R0
        BEQ     @@s
        INCR    R3
@@s:    PSHR    R4
        CALL    BOFF
        PULR    R4
        MOVR    R3, R5                  ; (no calls till BLINE)
        ADDI    #const_BE_X0, R5
        MVI@    R4, R0
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        MVO@    R0, R5
        ADDI    #const_BE_Y0 - const_BE_X0 - 1, R5
        MVI@    R4, R0
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        MVO@    R0, R5
        ADDI    #const_BE_X1 - const_BE_Y0 - 1, R5
        MVI@    R4, R0
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        MVO@    R0, R5
        ADDI    #const_BE_Y1 - const_BE_X1 - 1, R5
        MVI@    R4, R0
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        MVO@    R0, R5
        MVI@    R4, R0                  ; (kind)
        MVI@    R4, R0                  ; frames (0: 1)
        TSTR    R0
        BNEQ    @@f
        INCR    R0
@@f:    MOVR    R3, R5
        ADDI    #const_BE_LEFT, R5
        MVO@    R0, R5
        PSHR    R4
        MVII    #const_PO_FX, R0
        CALL    BLINE
        PULR    R4
        PULR    PC
        ENDP

;; BSTEP: every beam a frame; at its last, gone
BSTEP:  PROC
        PSHR    R5
        CLRR    R3
@@l:    MOVR    R3, R4
        ADDI    #const_BE_LEFT, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@n
        DECR    R4
        DECR    R0
        MVO@    R0, R4
        BNEQ    @@n
        INCR    R0                      ; (BOFF wants it live)
        MVO@    R0, R4
        CALL    BOFF
@@n:    INCR    R3
        CMPI    #const_NBEAM, R3
        BNEQ    @@l
        PULR    PC
        ENDP

;; OPS_STEP: a frame of the paths, flows and beams
OPS_STEP: PROC
        PSHR    R5
        CALL    PSTEP
        CALL    FSTEP
        CALL    BSTEP
        PULR    PC
        ENDP

;; OPS_SEG_END: the segment's bytes are going: paths stop, flows run out
OPS_SEG_END: PROC
        PSHR    R5
        CLRR    R3
@@p:    CALL    PEND
        INCR    R3
        CMPI    #const_NPATH, R3
        BNEQ    @@p
        CLRR    R3
@@f:    MOVR    R3, R4
        ADDI    #const_FL_ON, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@n
        CALL    FEND
@@n:    INCR    R3
        CMPI    #const_NFLOW, R3
        BNEQ    @@f
        PULR    PC
        ENDP

;; OPS_BEAMS_OFF: the beams gone (the script's end)
OPS_BEAMS_OFF: PROC
        PSHR    R5
        CLRR    R3
@@l:    CALL    BOFF
        INCR    R3
        CMPI    #const_NBEAM, R3
        BNEQ    @@l
        PULR    PC
        ENDP
