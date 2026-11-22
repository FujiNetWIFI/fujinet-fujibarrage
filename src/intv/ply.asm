;; ===========================================================================
;; ply.asm -- the script player's timeline (src/atari/ply.s's;
;; docs/PROTOCOL.md "Playing a segment"; server/internal/view, op for op).
;; Each frame every track, blast, path, flow, beam and settle moves a step,
;; then the ops due run. The ops about the HUD, talk and sound are left for
;; play.bas (PL_EV, PL_SND, PL_TALK_*); the tanks' are done here (the TK_*
;; tables, mob.asm draws them).
;;
;;   PLY_RESET   a new shot: nothing moving
;;   PLY_START   a segment (SCRIPT, #seg_len) from its first op
;;   PLY_STEP    a frame (PL_DONE: OP_END reached)
;;   PLY_DRAW    the blasts at their size now, the heads (once a pass)
;;   PLY_END     the segment's bytes are going: settles land, paths stop,
;;               flows run out
;;   PLY_LAST    the script's end: tracks and beams gone, heads off
;;
;; A blast grows a ring at a time (what it added since last drawn: its
;; inside is fire already); its end is the whole ellipse.
;; ===========================================================================

;; MUL: R0 = R0 * R1 (16 bits). R1, R2 not kept.
MUL:    PROC
        CLRR    R2
@@m:    SARC    R1, 1
        BNC     @@n
        ADDR    R0, R2
@@n:    SLL     R0, 1
        TSTR    R1
        BNEQ    @@m
        MOVR    R2, R0
        JR      R5
        ENDP

;; DIV: R0 = R0 / R1 (unsigned 16 bits; R1 not 0). R1-R3 not kept.
DIV:    PROC
        CLRR    R2                      ; the remainder
        MVII    #16, R3
@@l:    SLL     R0, 1                   ; (the dividend's top bit into the remainder)
        RLC     R2, 1
        CMPR    R1, R2
        BNC     @@n
        SUBR    R1, R2
        INCR    R0
@@n:    DECR    R3
        BNEQ    @@l
        JR      R5
        ENDP

;; PLY_RESET: a new shot
PLY_RESET: PROC
        PSHR    R5
        CALL    TRK_RESET
        CLRR    R0                      ; blasts to beams: off
        MVII    #const_B_ON, R4
        MVII    #const_PL_P - const_B_ON, R1
@@z:    MVO@    R0, R4
        DECR    R1
        BNEQ    @@z
        MVO     R0, const_PL_FRAME
        MVO     R0, const_PL_FALL
        MVO     R0, const_PL_SHAKE
        MVO     R0, const_PL_FLASH
        CALL    HEADS_OFF
        PULR    PC
        ENDP

;; PLY_START: the segment in SCRIPT from its start
PLY_START: PROC
        MVII    #const_SCRIPT, R0
        MVO     R0, const_PL_P
        ADD     var_&SEG_LEN, R0
        MVO     R0, const_PL_END
        CLRR    R0
        MVO     R0, const_PL_WAIT
        MVO     R0, const_PL_DONE
        JR      R5
        ENDP

;; PLY_STEP: a frame: everything a step (not on the script's very first),
;; then the ops due
PLY_STEP: PROC
        PSHR    R5
        MVI     const_PL_DONE, R0
        TSTR    R0
        BNEQ    @@x
        MVI     const_PL_FRAME, R0
        TSTR    R0
        BNEQ    @@an
        MVI     const_PL_P, R0
        CMPI    #const_SCRIPT, R0
        BEQ     @@ops
@@an:   CALL    ANIM
@@ops:  MVI     const_PL_WAIT, R0
        TSTR    R0
        BEQ     @@run
        DECR    R0
        MVO     R0, const_PL_WAIT
@@run:  MVI     const_PL_WAIT, R0
        TSTR    R0
        BNEQ    @@fr
        MVI     const_PL_DONE, R0
        TSTR    R0
        BNEQ    @@fr
        CALL    OP
        B       @@run
@@fr:   MVI     const_PL_FRAME, R0
        INCR    R0
        MVO     R0, const_PL_FRAME
@@x:    PULR    PC
        ENDP

;; SEAT: R0 = the seat byte at R4, R4 past it; R1 = not 0 if it is a seat
SEAT:   PROC
        MVI@    R4, R0
        CLRR    R1
        CMPI    #const_SEATS, R0
        BC      @@x
        INCR    R1
@@x:    JR      R5
        ENDP

;; EVENT: PL_EV gets bits R0. R1 not kept.
EVENT:  PROC
        MVI     const_PL_EV, R1
        COMR    R0
        ANDR    R0, R1
        COMR    R0
        XORR    R0, R1
        MVO     R1, const_PL_EV
        JR      R5
        ENDP

;; OP: the op at PL_P run, PL_P past it
OP:     PROC
        PSHR    R5
        MVI     const_PL_P, R4
        CMP     const_PL_END, R4
        BC      @@end                   ; (past the segment: it is over)
        MVI@    R4, R0
        CMPI    #const_OP_WAIT, R0
        BEQ     @@wait
        TSTR    R0
        BEQ     @@end
        CMPI    #const_OP_ARC, R0
        BEQ     @@arc
        CMPI    #const_OP_STOP, R0
        BEQ     @@stop
        CMPI    #const_OP_PATH, R0
        BEQ     @@path
        CMPI    #const_OP_EXPLODE, R0
        BEQ     @@blast
        CMPI    #const_OP_DIRT, R0
        BEQ     @@blast
        CMPI    #const_OP_SETTLE, R0
        BEQ     @@settle
        CMPI    #const_OP_FLOW, R0
        BEQ     @@flow
        CMPI    #const_OP_BEAM, R0
        BEQ     @@beam
        CMPI    #const_OP_TUNNEL, R0
        BEQ     @@tunnel
        CMPI    #const_OP_TALK, R0      ; $30-$3A: the tanks', the HUD's, sound
        BNC     @@end
        CMPI    #const_OP_FIRE + 1, R0
        BC      @@end
        SUBI    #const_OP_TALK, R0
        ADDI    #@@tab, R0
        MOVR    R0, R1
        MVI@    R1, PC
@@tab:  DECLE   @@talk, @@sound, @@shield, @@damage, @@fall, @@death, @@tank
        DECLE   @@wind, @@shake, @@flash, @@fire
@@end:  MVII    #1, R0                  ; OP_END (or not one of ours): the segment is over
        MVO     R0, const_PL_DONE
        PULR    PC
@@wait: MVI@    R4, R0                  ; n frames (0: 256)
        TSTR    R0
        BNEQ    @@w
        MVII    #256, R0
@@w:    MVO     R0, const_PL_WAIT
        B       @@out
@@arc:  CALL    TRK_LOAD
        B       @@out
@@stop: MVI@    R4, R0
        PSHR    R4
        CALL    TRK_STOP
        PULR    R4
        B       @@out
@@path: CALL    OPS_PATH
        B       @@out
@@flow: CALL    OPS_FLOW
        B       @@out
@@beam: CALL    OPS_BEAM
        B       @@out
@@tunnel:
        ADDI    #5, R4
        B       @@out
@@blast:
        CALL    BLAST
        B       @@out
@@settle:
        CALL    SETTLE
        B       @@out
@@talk: CALL    SEAT                    ; seat, len, text
        MVI@    R4, R2
        TSTR    R1
        BEQ     @@tk
        MVO     R0, const_PL_TALK_S
        MVO     R2, const_PL_TALK_N
        MVO     R4, const_PL_TALK_P
        MVII    #const_EV_TALK, R0
        CALL    EVENT
@@tk:   ADDR    R2, R4
        B       @@out
@@sound:
        MVI@    R4, R0                  ; id, size
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        MVI     const_PL_NSND, R1
        CMPI    #4, R1
        BC      @@out
        MOVR    R1, R2
        ADDI    #const_PL_SND, R2
        MVO@    R0, R2
        INCR    R1
        MVO     R1, const_PL_NSND
        MVII    #const_EV_SND, R0
        CALL    EVENT
        B       @@out
@@shield:
        CALL    SEAT                    ; seat, event, hp: its MOB flashes
        ADDI    #2, R4
        TSTR    R1
        BEQ     @@out
        ADDI    #const_TK_FL, R0
        MOVR    R0, R1
        MVII    #12, R0
        MVO@    R0, R1
        B       @@out
@@damage:
        CALL    SEAT                    ; seat, amount, energy left
        INCR    R4
        MVI@    R4, R2
        TSTR    R1
        BEQ     @@out
        ADDI    #const_TK_E, R0
        MOVR    R0, R1
        MVO@    R2, R1
        MVII    #const_EV_HUD, R0
        CALL    EVENT
        B       @@out
@@fall: CALL    SEAT                    ; seat, x16, y, frames, chute
        TSTR    R1
        BNEQ    @@f1
        ADDI    #5, R4
        B       @@out
@@f1:   CALL    FALL
        B       @@out
@@death:
        CALL    SEAT                    ; seat, effect
        INCR    R4
        TSTR    R1
        BEQ     @@out
        MOVR    R0, R1
        ADDI    #const_TK_ON, R1
        CLRR    R2
        MVO@    R2, R1
        ADDI    #const_TK_BURN - const_TK_ON, R1
        MVII    #40, R2
        MVO@    R2, R1
        MVII    #1, R0
        MVO     R0, const_MB_DIRTY
        MVII    #const_EV_HUD, R0
        CALL    EVENT
        B       @@out
@@tank: CALL    SEAT                    ; seat, x16, y, angle
        TSTR    R1
        BNEQ    @@t1
        ADDI    #4, R4
        B       @@out
@@t1:   MOVR    R0, R3
        CALL    RDX                     ; x
        MOVR    R3, R1
        ADDI    #const_TK_X, R1
        MVO@    R0, R1
        MVI@    R4, R0                  ; y
        ADDI    #const_TK_Y - const_TK_X, R1
        MVO@    R0, R1
        MVI@    R4, R0                  ; angle
        ADDI    #const_TK_A - const_TK_Y, R1
        MVO@    R0, R1
        CLRR    R0                      ; (no fall, no chute)
        ADDI    #const_TK_FT - const_TK_A, R1
        MVO@    R0, R1
        ADDI    #const_TK_CH - const_TK_FT, R1
        MVO@    R0, R1
        MVII    #1, R0
        MVO     R0, const_MB_DIRTY
        B       @@out
@@wind: MVI@    R4, R0                  ; wind16 into the state
        MVO     R0, const_ST + const_OFF_WIND
        MVI@    R4, R0
        MVO     R0, const_ST + const_OFF_WIND + 1
        MVII    #const_EV_HUD, R0
        CALL    EVENT
        B       @@out
@@shake:
        MVI@    R4, R0
        MVO     R0, const_PL_SHAKE
        B       @@out
@@flash:
        MVI@    R4, R0                  ; colour, frames
        MVO     R0, const_PL_FLASHC
        MVI@    R4, R0
        MVO     R0, const_PL_FLASH
        B       @@out
@@fire: CALL    SEAT                    ; seat, angle, power16, weapon
        MVI@    R4, R2
        ADDI    #3, R4
        TSTR    R1
        BEQ     @@out
        MOVR    R0, R1
        ADDI    #const_TK_A, R1
        MVO@    R2, R1
        MVII    #1, R1
        MVO     R1, const_MB_DIRTY
        CMP     const_PL_TALK_S, R0     ; its talk ends
        BNEQ    @@out
        MVII    #const_EV_FIRE, R0
        CALL    EVENT
@@out:  MVO     R4, const_PL_P
        PULR    PC
        ENDP

;; RDX: R0 = the x16 at R4 (clamped to the field), R4 past it
RDX:    PROC
        MVI@    R4, R0
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        BPL     @@p
        CLRR    R0
@@p:    CMPI    #160, R0
        BNC     @@x
        MVII    #159, R0
@@x:    JR      R5
        ENDP

;; FALL: seat R0 falls (R4: x16, y, frames, chute): x now, y over the frames
;; (8.8 a frame), on its chute if it has one
FALL:   PROC
        PSHR    R5
        MOVR    R0, R3
        CALL    RDX
        MOVR    R3, R1
        ADDI    #const_TK_X, R1
        MVO@    R0, R1
        MVI@    R4, R0                  ; where it ends
        MOVR    R3, R1
        ADDI    #const_TK_FE, R1
        MVO@    R0, R1
        MVI@    R4, R2                  ; frames
        MVI@    R4, R0                  ; chute
        MOVR    R3, R1
        ADDI    #const_TK_CH, R1
        MVO@    R0, R1
        MOVR    R3, R1
        ADDI    #const_TK_FT, R1
        MVO@    R2, R1
        TSTR    R2
        BEQ     @@now
        PSHR    R4
        MOVR    R3, R1                  ; from where it is: y << 8, and the step
        ADDI    #const_TK_Y, R1
        MVI@    R1, R0
        MOVR    R3, R1
        ADDI    #const_TK_FE, R1
        MVI@    R1, R1
        SUBR    R0, R1                  ; down this far (up: there at once)
        BMI     @@up
        SWAP    R0
        PSHR    R3
        PSHR    R0
        MOVR    R1, R0
        SWAP    R0
        MOVR    R2, R1
        CALL    DIV
        PULR    R1
        PULR    R3
        MOVR    R3, R2
        ADDI    #const_TK_FY, R2
        MVO@    R1, R2
        ADDI    #const_TK_FD - const_TK_FY, R2
        MVO@    R0, R2
        PULR    R4
        PULR    PC
@@up:   PULR    R4
@@now:  MOVR    R3, R1                  ; there now
        ADDI    #const_TK_FE, R1
        MVI@    R1, R0
        ADDI    #const_TK_Y - const_TK_FE, R1
        MVO@    R0, R1
        CLRR    R0
        ADDI    #const_TK_FT - const_TK_Y, R1
        MVO@    R0, R1
        ADDI    #const_TK_CH - const_TK_FT, R1
        MVO@    R0, R1
        MVII    #1, R0
        MVO     R0, const_MB_DIRTY
        PULR    PC
        ENDP

;; BLAST: an OP_EXPLODE's (x16, y16, rx, ry, kind, frames) or OP_DIRT's (no
;; kind) at R4 (R0 the op): the first free blast (or the last)
BLAST:  PROC
        PSHR    R5
        MOVR    R0, R2
        CLRR    R3
@@f:    MOVR    R3, R1
        ADDI    #const_B_ON, R1
        MVI@    R1, R0
        TSTR    R0
        BEQ     @@got
        INCR    R3
        CMPI    #const_NBLAST - 1, R3
        BNEQ    @@f
@@got:  MVI@    R4, R0                  ; x
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        MOVR    R3, R1
        ADDI    #const_B_X, R1
        MVO@    R0, R1
        MVI@    R4, R0                  ; y
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        MOVR    R3, R1
        ADDI    #const_B_Y, R1
        MVO@    R0, R1
        MVI@    R4, R0                  ; rx, ry (0: 1)
        TSTR    R0
        BNEQ    @@rx
        INCR    R0
@@rx:   MOVR    R3, R1
        ADDI    #const_B_RX, R1
        MVO@    R0, R1
        MVI@    R4, R0
        TSTR    R0
        BNEQ    @@ry
        INCR    R0
@@ry:   ADDI    #const_B_RY - const_B_RX, R1
        MVO@    R0, R1
        MVII    #const_DIRTBALL, R0     ; kind
        CMPI    #const_OP_DIRT, R2
        BEQ     @@k
        MVI@    R4, R0
@@k:    ADDI    #const_B_KIND - const_B_RY, R1
        MVO@    R0, R1
        CMPI    #const_EX_NUKE, R0      ; a nuke shakes the screen
        BNEQ    @@nn
        MVII    #30, R0
        MVO     R0, const_PL_SHAKE
@@nn:   MVI@    R4, R0                  ; frames (0: 1)
        TSTR    R0
        BNEQ    @@t
        INCR    R0
@@t:    ADDI    #const_B_TOT - const_B_KIND, R1
        MVO@    R0, R1
        CLRR    R0
        ADDI    #const_B_N - const_B_TOT, R1
        MVO@    R0, R1
        ADDI    #const_B_DRX - const_B_N, R1
        MVO@    R0, R1
        ADDI    #const_B_DRY - const_B_DRX, R1
        MVO@    R0, R1
        MVII    #1, R0
        ADDI    #const_B_ON - const_B_DRY, R1
        MVO@    R0, R1
        PULR    PC
        ENDP

;; SKIPCOLS: R4 past R1 column entries
SKIPCOLS: PROC
        TSTR    R1
        BEQ     @@x
@@l:    MVI@    R4, R0
        CMPI    #const_T_SPANS + 1, R0
        BNC     @@n
        SUBI    #const_T_SPANS, R0
        SLL     R0, 1
        ADDR    R0, R4
@@n:    DECR    R1
        BNEQ    @@l
@@x:    JR      R5
        ENDP

;; SETTLE: an OP_SETTLE's x16, n, frames, n column entries at R4: now, or
;; kept for later (the 8 slots full: the first lands now)
SETTLE: PROC
        PSHR    R5
        MVI@    R4, R0                  ; x
        MVI@    R4, R1
        SWAP    R1
        ADDR    R1, R0
        MVI@    R4, R1                  ; n
        MVI@    R4, R2                  ; frames
        TSTR    R2
        BNEQ    @@keep
        MOVR    R4, R2                  ; now
        PSHR    R4
        CALL    COLS
        PULR    R4
        ADDR    R0, R4
        PULR    PC
@@keep: PSHR    R0
        PSHR    R1
        PSHR    R2
        CLRR    R3                      ; a free slot
@@f:    MOVR    R3, R0
        ADDI    #const_S_ON, R0
        MOVR    R0, R1
        MVI@    R1, R0
        TSTR    R0
        BEQ     @@got
        INCR    R3
        CMPI    #const_NSETTLE, R3
        BNEQ    @@f
        CLRR    R3                      ; full: the first lands now
        PSHR    R4
        CALL    SLAND
        PULR    R4
@@got:  PULR    R2
        PULR    R1
        PULR    R0
        MOVR    R3, R5                  ; (no calls till SKIPCOLS)
        ADDI    #const_S_ON, R5
        MVII    #1, R3
        MVO@    R3, R5
        ADDI    #const_S_LEFT - const_S_ON - 1, R5
        MVO@    R2, R5
        ADDI    #const_S_N - const_S_LEFT - 1, R5
        MVO@    R1, R5
        ADDI    #const_S_X - const_S_N - 1, R5
        MVO@    R0, R5
        ADDI    #const_S_P - const_S_X - 1, R5
        MVO@    R4, R5
        CALL    SKIPCOLS                ; past its entries
        PULR    PC
        ENDP

;; SLAND: settle R3 lands: its columns written, the slot free. R3 kept.
SLAND:  PROC
        PSHR    R5
        PSHR    R3
        MOVR    R3, R4
        ADDI    #const_S_X, R4
        MVI@    R4, R0
        ADDI    #const_S_P - const_S_X - 1, R4
        MVI@    R4, R2
        MOVR    R3, R4
        ADDI    #const_S_N, R4
        MVI@    R4, R1
        CALL    COLS
        PULR    R3
        MOVR    R3, R4
        ADDI    #const_S_ON, R4
        CLRR    R0
        MVO@    R0, R4
        PULR    PC
        ENDP

;; BOPS: R0 = blast R3's op growing (R1 = 0) or at its end (R1 = 1)
BOPS:   PROC
        MOVR    R3, R4
        ADDI    #const_B_KIND, R4
        MVI@    R4, R2
        CMPI    #const_DIRTBALL, R2     ; dirt either way
        BNEQ    @@nd
        MVII    #const_PO_DIRT, R0
        JR      R5
@@nd:   CMPI    #const_EX_PLASMA, R2    ; a flash: only the air lit, then out
        BEQ     @@fl
        CMPI    #const_EX_PUFF, R2
        BEQ     @@fl
        MVII    #const_PO_FX, R0        ; a crater: fire eats the dirt, then sky
        TSTR    R1
        BEQ     @@x
        MVII    #const_PO_SKY, R0
        JR      R5
@@fl:   MVII    #const_PO_AIRFX, R0
        TSTR    R1
        BEQ     @@x
        MVII    #const_PO_CLRFX, R0
@@x:    JR      R5
        ENDP

;; BELL: blast R3's ellipse: its centre, radii R0, R1, inner radii R2, (R2
;; not 0) EL_IRY as set; op FV_OP. R3 kept.
BELL:   PROC
        PSHR    R5
        PSHR    R3
        MVO     R0, const_EL_RX
        MVO     R1, const_EL_RY
        MVO     R2, const_EL_IRX
        MOVR    R3, R4
        ADDI    #const_B_X, R4
        MVI@    R4, R0
        MVO     R0, const_EL_X
        ADDI    #const_B_Y - const_B_X - 1, R4
        MVI@    R4, R0
        MVO     R0, const_EL_Y
        CALL    ELLIPSE
        PULR    R3
        PULR    PC
        ENDP

;; ANIM: a frame of everything moving
ANIM:   PROC
        PSHR    R5
        CALL    TRK_STEP
        CLRR    R3                      ; blasts: a frame more; at the end, the crater
@@b:    MOVR    R3, R4
        ADDI    #const_B_ON, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@nb
        ADDI    #const_B_N - const_B_ON - 1, R4
        MVI@    R4, R0
        INCR    R0
        DECR    R4
        MVO@    R0, R4
        ADDI    #const_B_TOT - const_B_N - 1, R4
        CMP@    R4, R0
        BNC     @@nb
        MVII    #1, R1                  ; done: its end over the whole of it
        CALL    BOPS
        MVO     R0, const_FV_OP
        MOVR    R3, R4
        ADDI    #const_B_RX, R4
        MVI@    R4, R0
        ADDI    #const_B_RY - const_B_RX - 1, R4
        MVI@    R4, R1
        CLRR    R2
        MVO     R2, const_EL_IRY
        CALL    BELL
        MOVR    R3, R4
        ADDI    #const_B_ON, R4
        CLRR    R0
        MVO@    R0, R4
@@nb:   INCR    R3
        CMPI    #const_NBLAST, R3
        BNEQ    @@b
        CALL    OPS_STEP
        CLRR    R3                      ; settles: a frame nearer
@@s:    MOVR    R3, R4
        ADDI    #const_S_ON, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@ns
        ADDI    #const_S_LEFT - const_S_ON - 1, R4
        MVI@    R4, R0
        DECR    R4
        DECR    R0
        MVO@    R0, R4
        BNEQ    @@ns
        CALL    SLAND
@@ns:   INCR    R3
        CMPI    #const_NSETTLE, R3
        BNEQ    @@s
        CLRR    R3                      ; falls: a frame further
@@fa:   MOVR    R3, R4
        ADDI    #const_TK_FT, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@nf
        DECR    R4
        DECR    R0
        MVO@    R0, R4
        BNEQ    @@fm
        MOVR    R3, R4                  ; landed: where it ends, the chute away
        ADDI    #const_TK_FE, R4
        MVI@    R4, R0
        ADDI    #const_TK_Y - const_TK_FE - 1, R4
        MVO@    R0, R4
        CLRR    R0
        ADDI    #const_TK_CH - const_TK_Y - 1, R4
        MVO@    R0, R4
        B       @@fd
@@fm:   MOVR    R3, R4                  ; y += its step (8.8)
        ADDI    #const_TK_FY, R4
        MVI@    R4, R0
        ADDI    #const_TK_FD - const_TK_FY - 1, R4
        ADD@    R4, R0
        ADDI    #const_TK_FY - const_TK_FD - 1, R4
        MVO@    R0, R4
        SWAP    R0
        ANDI    #255, R0
        ADDI    #const_TK_Y - const_TK_FY - 1, R4
        MVO@    R0, R4
@@fd:   MVII    #1, R0
        MVO     R0, const_MB_DIRTY
@@nf:   INCR    R3
        CMPI    #const_SEATS, R3
        BNEQ    @@fa
        PULR    PC
        ENDP

;; PLY_DRAW: each blast at its size now (rx n / total, ry n / total, 1 at
;; least): the ring it grew since last drawn; then the heads
PLY_DRAW: PROC
        PSHR    R5
        CLRR    R3
@@b:    MOVR    R3, R4
        ADDI    #const_B_ON, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@nb
        PSHR    R3
        CALL    @@size                  ; rx now
        MVO     R0, const_PL_I
        PULR    R3
        PSHR    R3
        MOVR    R3, R4
        ADDI    #const_B_RY, R4
        MVI@    R4, R0
        CALL    @@sz2                   ; ry now
        MVO     R0, const_PL_J
        PULR    R3
        MOVR    R3, R4                  ; drawn at this size already?
        ADDI    #const_B_DRX, R4
        MVI     const_PL_I, R0
        CMP@    R4, R0
        BNEQ    @@draw
        ADDI    #const_B_DRY - const_B_DRX - 1, R4
        MVI     const_PL_J, R0
        CMP@    R4, R0
        BEQ     @@nb
@@draw: CLRR    R1
        CALL    BOPS
        MVO     R0, const_FV_OP
        MOVR    R3, R4                  ; inside: last drawn (0: none)
        ADDI    #const_B_DRY, R4
        MVI@    R4, R0
        MVO     R0, const_EL_IRY
        ADDI    #const_B_DRX - const_B_DRY - 1, R4
        MVI@    R4, R2
        MVI     const_PL_I, R0
        MVI     const_PL_J, R1
        CALL    BELL
        MOVR    R3, R4
        ADDI    #const_B_DRX, R4
        MVI     const_PL_I, R0
        MVO@    R0, R4
        ADDI    #const_B_DRY - const_B_DRX - 1, R4
        MVI     const_PL_J, R0
        MVO@    R0, R4
@@nb:   INCR    R3
        CMPI    #const_NBLAST, R3
        BNEQ    @@b
        CALL    HEADS
        PULR    PC

;; @@size: R0 = blast R3's rx * n / total (1 at least); @@sz2: R0's
@@size: MOVR    R3, R4
        ADDI    #const_B_RX, R4
        MVI@    R4, R0
@@sz2:  PSHR    R5
        PSHR    R3
        MOVR    R3, R4
        ADDI    #const_B_N, R4
        MVI@    R4, R1
        CALL    MUL
        PULR    R3
        PSHR    R3
        MOVR    R3, R4
        ADDI    #const_B_TOT, R4
        MVI@    R4, R1
        CALL    DIV
        PULR    R3
        TSTR    R0
        BNEQ    @@s1
        INCR    R0
@@s1:   PULR    PC
        ENDP

;; PLY_END: the segment's bytes are going (the next segment reuses SCRIPT):
;; settles land, paths stop, flows run out
PLY_END: PROC
        PSHR    R5
        CLRR    R3
@@s:    MOVR    R3, R4
        ADDI    #const_S_ON, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@n
        CALL    SLAND
@@n:    INCR    R3
        CMPI    #const_NSETTLE, R3
        BNEQ    @@s
        CALL    OPS_SEG_END
        PULR    PC
        ENDP

;; PLY_LAST: the script's end: the tracks and beams gone, the heads off
PLY_LAST: PROC
        PSHR    R5
        CALL    TRK_RESET
        CALL    OPS_BEAMS_OFF
        CALL    HEADS_OFF
        CLRR    R0
        MVO     R0, const_PL_FALL
        PULR    PC
        ENDP
