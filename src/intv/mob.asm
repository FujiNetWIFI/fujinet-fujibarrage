;; ===========================================================================
;; mob.asm -- the tanks as MOBs (play.bas's tanks_draw, in assembly). From
;; the tank tables (TK_*, cartridge RAM) into IntyBASIC's MOB shadow
;; (_mobs: 8 X words, 8 Y, 8 A), which its frame handler copies to the STIC.
;;
;; A tank: its body and barrel in one card (G_TANK + the nearest of 0, 22,
;; 45, 68, 90 degrees; mirrored past 90, a pixel to the left), in its
;; seat's colour, behind the cards' foreground (dirt hides a buried tank),
;; a field pixel a MOB pixel; falling on its parachute, the chute pair
;; (G_CHUTE, 16 rows). Its card's rows 3-7 are the body: the MOB's top is
;; 8 rows above the tank's y, and the field starts at screen row 2.
;;
;; Seats in order of importance: ours, the turn's, the rest. With n > 8
;; alive, the first 16 - n keep a MOB each and the rest share the others,
;; the even ones on even frames (they blink at 30 Hz).
;;
;; The words and the order are worked out only when a tank or the turn
;; changes (MB_DIRTY: play.bas sets it); a frame copies them.
;; ===========================================================================

;; TANKS_MOB: this frame's MOBs.
TANKS_MOB: PROC
        PSHR    R5
        CALL    FX_TICK
        MVI     const_MB_DIRTY, R0
        TSTR    R0
        BEQ     @@go
        CALL    TANKS_PREP
@@go:   MVII    #const_MB_ORD, R4       ; the seats that keep one
        CLRR    R3                      ; the MOB
        MVI     const_MB_KEEP, R2
        TSTR    R2
        BEQ     @@sh
@@k:    MVI@    R4, R1
        CALL    @@put
        INCR    R3
        DECR    R2
        BNEQ    @@k
@@sh:   MVI     const_MB_CNT, R2        ; the rest, those of this frame's parity
        SUB     const_MB_KEEP, R2
        BEQ     @@off
        MVI     _frame, R0
        ANDI    #1, R0
        BEQ     @@s
        MVI@    R4, R1                  ; (odd frames: from the second)
        DECR    R2
        BEQ     @@off
@@s:    MVI@    R4, R1
        CMPI    #8, R3
        BC      @@off
        CALL    @@put
        INCR    R3
        DECR    R2
        BEQ     @@off
        MVI@    R4, R1                  ; (the other parity's)
        DECR    R2
        BNEQ    @@s
@@off:  MVII    #_mobs, R4              ; the MOBs left: off
        ADDR    R3, R4
        CLRR    R1
@@o:    CMPI    #8, R3
        BC      @@x
        MVO@    R1, R4
        INCR    R3
        B       @@o
@@x:    PULR    PC

;; @@put: seat R1's words on MOB R3, in its effect's colours: burning (a
;; death: the fire's colours, cycling), flashing (a shield: white every
;; other two frames). R2-R4 kept.
@@put:  PSHR    R5
        PSHR    R4
        PSHR    R2
        MOVR    R1, R0
        ADDI    #const_TK_MX, R0
        MOVR    R0, R4
        MVII    #_mobs, R5
        ADDR    R3, R5
        MVI@    R4, R0                  ; X
        MVO@    R0, R5
        ADDI    #7, R5
        ADDI    #9, R4
        MVI@    R4, R0                  ; Y
        MVO@    R0, R5
        ADDI    #7, R5
        ADDI    #9, R4
        MVI@    R4, R0                  ; A
        MOVR    R1, R2
        ADDI    #const_TK_BURN, R2
        MVI@    R2, R2
        TSTR    R2
        BEQ     @@nb
        ANDI    #$EFF8, R0              ; (the colour: bits 0-2 and 12)
        MVI     _frame, R2
        SLR     R2, 2
        ANDI    #3, R2
        ADDI    #@@burn, R2
        MVI@    R2, R2
        ADDR    R2, R0
        B       @@pa
@@nb:   MOVR    R1, R2
        ADDI    #const_TK_FL, R2
        MVI@    R2, R2
        TSTR    R2
        BEQ     @@pa
        MVI     _frame, R2
        ANDI    #2, R2
        BEQ     @@pa
        ANDI    #$EFF8, R0
        ADDI    #7, R0                  ; white
@@pa:   MVO@    R0, R5
        PULR    R2
        PULR    R4
        PULR    PC
@@burn: DECLE   2, $1002, 6, $1002      ; red, orange, yellow, orange
        ENDP

;; FX_TICK: a frame of the tanks' effects: burning and flashing wind down (a
;; burnt-out tank leaves the order)
FX_TICK: PROC
        MVII    #const_TK_BURN, R4
        MVII    #20, R1                 ; (TK_FL follows TK_BURN)
@@l:    MVI@    R4, R0
        TSTR    R0
        BEQ     @@n
        DECR    R4
        DECR    R0
        MVO@    R0, R4
        BNEQ    @@n
        CMPI    #10, R1                 ; a burn over (the first 10): the order again
        BLE     @@n
        MVII    #1, R0
        MVO     R0, const_MB_DIRTY
@@n:    DECR    R1
        BNEQ    @@l
        JR      R5
        ENDP

;; TANKS_PREP: each seat's MOB words (TK_MX, TK_MY, TK_MA) and the seats in
;; order (MB_ORD: MB_CNT alive, the first MB_KEEP with a MOB each).
TANKS_PREP: PROC
        PSHR    R5
        CLRR    R0
        MVO     R0, const_MB_DIRTY
        MVO     R0, const_MB_CNT
        MVI     var_MY_SEAT, R1         ; ours
        CALL    @@alive
        TSTR    R1
        BEQ     @@t
        MVI     var_MY_SEAT, R1
        CALL    @@ord
@@t:    MVI     const_ST + const_OFF_TURN, R1   ; the turn's
        ANDI    #255, R1
        MVO     R1, const_MB_T
        CMP     var_MY_SEAT, R1
        BEQ     @@rest
        CALL    @@alive
        TSTR    R1
        BEQ     @@rest
        MVI     const_MB_T, R1
        CALL    @@ord
@@rest: CLRR    R1                      ; the rest, in seat order
@@r:    CMP     var_MY_SEAT, R1
        BEQ     @@rn
        CMP     const_MB_T, R1
        BEQ     @@rn
        PSHR    R1
        CALL    @@alive
        MOVR    R1, R0
        PULR    R1
        TSTR    R0
        BEQ     @@rn
        CALL    @@ord
@@rn:   INCR    R1
        CMPI    #const_SEATS, R1
        BNEQ    @@r
        MVI     const_MB_CNT, R2        ; kept: all, or 16 - n
        MOVR    R2, R0
        CMPI    #9, R2
        BLT     @@k
        MVII    #16, R0
        SUBR    R2, R0
@@k:    MVO     R0, const_MB_KEEP
        CLRR    R1                      ; every seat's words
@@w:    PSHR    R1
        CALL    @@words
        PULR    R1
        INCR    R1
        CMPI    #const_SEATS, R1
        BNEQ    @@w
        PULR    PC

;; @@alive: R1 = not 0 if seat R1 is on the field, or burning (JR sets
;; the flags: the caller tests R1)
@@alive:
        CMPI    #const_SEATS, R1
        BC      @@no
        ADDI    #const_TK_ON, R1
        MOVR    R1, R4
        MVI@    R4, R1
        ADDI    #const_TK_BURN - const_TK_ON - 1, R4
        ADD@    R4, R1
        JR      R5
@@no:   CLRR    R1
        JR      R5

;; @@ord: seat R1 next in the order. R1 kept.
@@ord:  MVI     const_MB_CNT, R0
        MOVR    R0, R4
        ADDI    #const_MB_ORD, R4
        MVO@    R1, R4
        INCR    R0
        MVO     R0, const_MB_CNT
        JR      R5

;; @@words: seat R1's X, Y and A
@@words:
        PSHR    R5
        MOVR    R1, R4                  ; its barrel: mirrored past 90
        ADDI    #const_TK_A, R4
        MVI@    R4, R0
        CLRR    R2                      ; (Y's mirror bit)
        CMPI    #91, R0
        BNC     @@a
        NEGR    R0
        ADDI    #180, R0
        MVII    #$400, R2
@@a:    MVII    #const_G_TANK, R3       ; the nearest of 0, 22, 45, 68, 90
        CMPI    #12, R0
        BNC     @@c
        INCR    R3
        CMPI    #34, R0
        BNC     @@c
        INCR    R3
        CMPI    #57, R0
        BNC     @@c
        INCR    R3
        CMPI    #79, R0
        BNC     @@c
        INCR    R3
@@c:    MOVR    R1, R4                  ; Y: the tank's y + 16, normal size
        ADDI    #const_TK_Y, R4
        MVI@    R4, R0
        ADDI    #16 + $100, R0
        ADDR    R2, R0
        MOVR    R1, R4                  ; on its parachute: the pair, 8 higher
        ADDI    #const_TK_CH, R4
        MVI@    R4, R5
        TSTR    R5
        BEQ     @@nc
        SUBI    #8 - $80, R0
        MVII    #const_G_CHUTE, R3
@@nc:   MOVR    R1, R4
        ADDI    #const_TK_MY, R4
        MVO@    R0, R4
        SLL     R3, 2                   ; A: the card, GRAM, behind, the colour
        SLL     R3, 1
        ADDI    #$800 + $2000, R3
        MOVR    R1, R4
        ADDI    #label_SEAT_MOB, R4
        ADD@    R4, R3
        MOVR    R1, R4
        ADDI    #const_TK_MA, R4
        MVO@    R3, R4
        MOVR    R1, R4                  ; X: the tank's x + 5 (mirrored: + 4), shown
        ADDI    #const_TK_X, R4
        MVI@    R4, R0
        ADDI    #5 + $200, R0
        TSTR    R2
        BEQ     @@nf
        DECR    R0
@@nf:   MOVR    R1, R4
        ADDI    #const_TK_MX, R4
        MVO@    R0, R4
        PULR    PC
        ENDP
