;; ===========================================================================
;; cell.asm -- the field on the screen: each of its 200 cells (BACKTAB rows
;; 2-11) drawn from the planes (fld.asm) in foreground/background mode.
;;
;; A cell with no fire shows its dirt: none, the row's sky (the GROM's
;; blank, or a star on the stars sky: SKYW); all, the solid card in the
;; dirt's colour; some, its pattern in the dirt's colour on the row's sky.
;; A cell with fire shows the fire's pattern in the fire's colour, on
;; whichever of sky or dirt covers more of the rest.
;;
;; A pattern neither empty nor full is a card from the pool, GRAM cards
;; G_POOL on (N_POOL of them): the same pattern, the same card, found by
;; its hash (64 buckets, chained), counted by the cells showing it. A card
;; no cell shows keeps its pattern till another needs it (the clock finds
;; one), so a pattern that comes back costs nothing. With the pool full a
;; cell gets the nearest level card, and is drawn again when a card comes
;; free (RETRY).
;;
;; Nothing here touches the GRAM or BACKTAB: the cards to load and the
;; words to write go into GRAMQ, in order, and the frame handler does them
;; (GRAM_FLUSH, while the GRAM is open), so a cell never shows a card
;; before it is loaded and a card is never loaded while a cell still shows
;; it. A GRAMQ entry: an address (a card's in the GRAM, $3800 on, or a
;; BACKTAB word's) and what goes there (the card's pattern in PPAT, or the
;; word).
;; ===========================================================================

;; GQPUT: GRAMQ gets (R0, R1). R2 not kept.
GQPUT:  PROC
        MVI     const_FV_GQT, R2
        MOVR    R2, R4
        ADDI    #const_GRAMQ, R4
        MVO@    R0, R4
        MVO@    R1, R4
        ADDI    #2, R2
        CMPI    #192, R2
        BNEQ    @@w
        CLRR    R2
@@w:    MVO     R2, const_FV_GQT        ; (after the entry: the handler may take it now)
        JR      R5
        ENDP

;; GRAM_FLUSH: the frame handler's (the Makefile calls it from IntyBASIC's
;; own, right after its DEFINE uploads, while the GRAM is still open).
;; GRAMQ in order, a card costing 4 of the budget and a word 1, till the
;; budget is spent or the queue is.
GRAM_FLUSH: PROC
        PSHR    R5
        MVI     var_READY, R0           ; (not before boot has cleared the
        TSTR    R0                      ; cartridge's RAM)
        BNEQ    @@go
        MVO     R0, $30
        MVO     R0, $31
        PULR    PC
@@go:   MVI     const_SHK_X, R0         ; (a shake: the picture's delays, in the
        MVO     R0, $30                 ; vertical blank as the STIC wants)
        MVI     const_SHK_Y, R0
        MVO     R0, $31
        MVI     const_FV_GQH, R3
        MVII    #64, R2
@@top:  CMP     const_FV_GQT, R3
        BEQ     @@out
        MOVR    R3, R4
        ADDI    #const_GRAMQ, R4
        MVI@    R4, R5                  ; where
        MVI@    R4, R0                  ; what
        CMPI    #$3800, R5
        BC      @@card
        MVO@    R0, R5                  ; a BACKTAB word
        DECR    R2
        B       @@adv
@@card: SUBI    #4, R2
        BMI     @@out                   ; not in this frame's window: the next
        MOVR    R0, R4
        MVI@    R4, R1
        MVO@    R1, R5
        SWAP    R1
        MVO@    R1, R5
        MVI@    R4, R1
        MVO@    R1, R5
        SWAP    R1
        MVO@    R1, R5
        MVI@    R4, R1
        MVO@    R1, R5
        SWAP    R1
        MVO@    R1, R5
        MVI@    R4, R1
        MVO@    R1, R5
        SWAP    R1
        MVO@    R1, R5
        SUBI    #const_PPAT, R0         ; loaded: no longer waiting
        SLR     R0, 2
        ADDI    #const_PPEND, R0
        MOVR    R0, R4
        CLRR    R1
        MVO@    R1, R4
@@adv:  ADDI    #2, R3
        CMPI    #192, R3
        BNEQ    @@w
        CLRR    R3
@@w:    TSTR    R2
        BGT     @@top
@@out:  MVO     R3, const_FV_GQH
        PULR    PC
        ENDP

;; HBUCK: R0 = the bucket of hash R0. R1 not kept.
HBUCK:  PROC
        MOVR    R0, R1
        SWAP    R1
        XORR    R1, R0
        MOVR    R0, R1
        SLR     R1, 2
        SLR     R1, 2
        XORR    R1, R0
        ANDI    #63, R0
        JR      R5
        ENDP

;; UNLINK: card RV_K out of the bucket its hash puts it in.
UNLINK: PROC
        PSHR    R5
        MVI     const_RV_K, R1
        MOVR    R1, R4
        ADDI    #const_PHASH, R4
        MVI@    R4, R0
        CALL    HBUCK
        ADDI    #const_PBUCK, R0
        MOVR    R0, R3                  ; the link pointing at it: the bucket's, first
        MVI     const_RV_K, R2
        INCR    R2                      ; (what a link to it holds)
@@w:    MOVR    R3, R4
        MVI@    R4, R0
        ANDI    #$7FFF, R0
        BEQ     @@out                   ; (not there)
        CMPR    R2, R0
        BEQ     @@hit
        DECR    R0
        ADDI    #const_PNEXT, R0
        MOVR    R0, R3
        B       @@w
@@hit:  MVI     const_RV_K, R4          ; that link gets its next
        ADDI    #const_PNEXT, R4
        MVI@    R4, R1
        ANDI    #$7FFF, R1
        MOVR    R3, R4
        MVI@    R4, R0
        ANDI    #$8000, R0              ; (a card's link keeps its flag, a bucket's has none)
        XORR    R1, R0
        DECR    R4
        MVO@    R0, R4
@@out:  CLRR    R0                      ; in no bucket now
        MVI     const_RV_K, R4
        ADDI    #const_PNEXT, R4
        MVO@    R0, R4
        PULR    PC
        ENDP

;; POOL_GET: a card for the pattern at RV_P0: R0 = the card (G_POOL on,
;; counted), or 0 with the pool full.
POOL_GET: PROC
        PSHR    R5
        MVI     const_RV_P0, R0         ; the hash
        MVI     const_RV_P1, R1
        SWAP    R1
        ADDR    R1, R0
        XOR     const_RV_P2, R0
        MVI     const_RV_P3, R1
        SWAP    R1
        ADDR    R1, R0
        MVO     R0, const_RV_H
        CALL    HBUCK
        MVO     R0, const_RV_B
        ADDI    #const_PBUCK, R0
        MOVR    R0, R4
        MVI@    R4, R1                  ; its chain's first + 1
@@ch:   ANDI    #$7FFF, R1
        BEQ     @@new
        DECR    R1                      ; card R1 (from 0)
        MOVR    R1, R4
        ADDI    #const_PHASH, R4
        MVI@    R4, R0
        CMP     const_RV_H, R0
        BNEQ    @@nx
        MOVR    R1, R2                  ; the pattern itself
        SLL     R2, 2
        ADDI    #const_PPAT, R2
        MOVR    R2, R4
        MVI@    R4, R0
        CMP     const_RV_P0, R0
        BNEQ    @@nx
        MVI@    R4, R0
        CMP     const_RV_P1, R0
        BNEQ    @@nx
        MVI@    R4, R0
        CMP     const_RV_P2, R0
        BNEQ    @@nx
        MVI@    R4, R0
        CMP     const_RV_P3, R0
        BNEQ    @@nx
        MOVR    R1, R4                  ; it: one more cell shows it
        ADDI    #const_PREF, R4
        MVI@    R4, R0
        INCR    R0
        DECR    R4
        MVO@    R0, R4
        MOVR    R1, R0
        ADDI    #const_G_POOL, R0
        PULR    PC
@@nx:   MOVR    R1, R4
        ADDI    #const_PNEXT, R4
        MVI@    R4, R1
        B       @@ch
@@new:  MVI     const_FV_CLOCK, R1      ; a card no cell shows, not waiting to load
        MVII    #const_N_POOL, R2
@@f:    INCR    R1
        CMPI    #const_N_POOL, R1
        BNC     @@f1
        CLRR    R1
@@f1:   MOVR    R1, R4
        ADDI    #const_PREF, R4
        MVI@    R4, R0
        TSTR    R0
        BNEQ    @@fn
        ADDI    #const_PPEND - const_PREF - 1, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@got
@@fn:   DECR    R2
        BNEQ    @@f
        CLRR    R0                      ; the pool is full
        PULR    PC
@@got:  MVO     R1, const_FV_CLOCK
        MVO     R1, const_RV_K
        MOVR    R1, R4
        ADDI    #const_PNEXT, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@link
        CALL    UNLINK                  ; out of its old pattern's bucket
@@link: MVI     const_RV_B, R0          ; first in the new one's
        ADDI    #const_PBUCK, R0
        MOVR    R0, R4
        MVI@    R4, R2
        XORI    #$8000, R2
        MVI     const_RV_K, R1
        MOVR    R1, R4
        ADDI    #const_PNEXT, R4
        MVO@    R2, R4
        MOVR    R1, R2
        INCR    R2
        MOVR    R0, R4
        MVO@    R2, R4
        MOVR    R1, R4                  ; its hash, one cell, waiting to load
        ADDI    #const_PHASH, R4
        MVI     const_RV_H, R0
        MVO@    R0, R4
        MOVR    R1, R4
        ADDI    #const_PREF, R4
        MVII    #1, R0
        MVO@    R0, R4
        MOVR    R1, R4
        ADDI    #const_PPEND, R4
        MVO@    R0, R4
        MOVR    R1, R2                  ; its pattern
        SLL     R2, 2
        ADDI    #const_PPAT, R2
        MOVR    R2, R4
        MVI     const_RV_P0, R0
        MVO@    R0, R4
        MVI     const_RV_P1, R0
        MVO@    R0, R4
        MVI     const_RV_P2, R0
        MVO@    R0, R4
        MVI     const_RV_P3, R0
        MVO@    R0, R4
        ADDI    #const_G_POOL, R1       ; loaded at the next frame: GRAM $3800 + card * 8
        MOVR    R1, R0
        SLL     R0, 2
        SLL     R0, 1
        ADDI    #$3800, R0
        PSHR    R1
        MOVR    R2, R1
        CALL    GQPUT
        PULR    R0
        PULR    PC
        ENDP

;; POOL_PUT: card R0 (any card) one cell fewer.
POOL_PUT: PROC
        SUBI    #const_G_POOL, R0
        BMI     @@x                     ; not the pool's
        ADDI    #const_PREF, R0
        MOVR    R0, R4
        MVI@    R4, R1
        DECR    R4
        DECR    R1
        MVO@    R1, R4
        BNEQ    @@x
        MVI     const_FV_NAPPROX, R1    ; free: the cells that wanted one try again
        TSTR    R1
        BEQ     @@x
        MVO     R1, const_FV_RETRY
@@x:    JR      R5
        ENDP

;; POP4: R0 = the set bits in the 4 words at R4. R1-R3 not kept.
POP4:   PROC
        CLRR    R0
        MVII    #4, R3
@@l:    MVI@    R4, R1
        MOVR    R1, R2
        ANDI    #255, R1
        ADDI    #fld_pop, R1
        MVI@    R1, R1
        ADDR    R1, R0
        SWAP    R2
        ANDI    #255, R2
        ADDI    #fld_pop, R2
        MVI@    R2, R2
        ADDR    R2, R0
        DECR    R3
        BNEQ    @@l
        JR      R5
        ENDP

;; LOAD4: the 4 words at R4 to R1 on; R2 = their AND, R3 = 1 if any is
;; not 0. R0 not kept.
LOAD4:  PROC
        MVII    #$FFFF, R2
        CLRR    R3
        MVI@    R4, R0
        MVO@    R0, R1
        INCR    R1
        ANDR    R0, R2
        TSTR    R0
        BEQ     @@1
        MVII    #1, R3
@@1:    MVI@    R4, R0
        MVO@    R0, R1
        INCR    R1
        ANDR    R0, R2
        TSTR    R0
        BEQ     @@2
        MVII    #1, R3
@@2:    MVI@    R4, R0
        MVO@    R0, R1
        INCR    R1
        ANDR    R0, R2
        TSTR    R0
        BEQ     @@3
        MVII    #1, R3
@@3:    MVI@    R4, R0
        MVO@    R0, R1
        ANDR    R0, R2
        TSTR    R0
        BEQ     @@4
        MVII    #1, R3
@@4:    JR      R5
        ENDP

;; COPYP: RV_P0..3 = the 4 words at R4. R0 not kept.
COPYP:  PROC
        MVI@    R4, R0
        MVO     R0, const_RV_P0
        MVI@    R4, R0
        MVO     R0, const_RV_P1
        MVI@    R4, R0
        MVO     R0, const_RV_P2
        MVI@    R4, R0
        MVO     R0, const_RV_P3
        JR      R5
        ENDP

;; LEVEL: R0 = the level card nearest the pattern at RV_P0 (by its pixels).
LEVEL:  PROC
        PSHR    R5
        MVII    #const_RV_P0, R4
        CALL    POP4
        MVII    #const_G_LEVEL, R1
        CMPI    #25, R0
        BNC     @@x
        INCR    R1
        CMPI    #41, R0
        BNC     @@x
        INCR    R1
@@x:    MOVR    R1, R0
        PULR    PC
        ENDP

;; SKYBG: R1 = the background bits of cell RV_IDX's sky.
SKYBG:  PROC
        MVI     const_RV_IDX, R4
        ADDI    #const_SKYW, R4
        MVI@    R4, R1
        ANDI    #$3600, R1
        JR      R5
        ENDP

;; RCELL: cell RV_IDX drawn: RV_W its word, RV_CELL its CELL entry, then
;; committed (COMMIT).
RCELL:  PROC
        PSHR    R5
        MVI     const_RV_IDX, R0        ; its fire
        SLL     R0, 2
        MVO     R0, const_RV_W          ; (its words' offset, for now)
        MOVR    R0, R4
        ADDI    #const_FXP, R4
        MVII    #const_RV_F0, R1
        CALL    LOAD4
        MVO     R2, const_RV_FAND
        MVO     R3, const_RV_FANY
        MVI     const_RV_W, R4          ; its dirt
        ADDI    #const_DIRT, R4
        MVII    #const_RV_D0, R1
        CALL    LOAD4
        MVI     const_RV_FANY, R0
        TSTR    R0
        BNEQ    @@fire
        TSTR    R3
        BEQ     @@sky
        CMPI    #$FFFF, R2
        BEQ     @@dirt
        MVII    #const_RV_D0, R4        ; some dirt: its pattern on the sky
        CALL    COPYP
        CALL    POOL_GET
        CLRR    R2                      ; (the CELL entry: the card, nothing else)
        TSTR    R0
        BNEQ    @@dc
        CALL    LEVEL
        MVII    #const_CF_APPROX, R2
        B       @@dw
@@dc:   MOVR    R0, R2
@@dw:   MVO     R2, const_RV_CELL
        SLL     R0, 2
        SLL     R0, 1
        ADDI    #$800, R0
        ADD     const_FV_DIRTFG, R0
        CALL    SKYBG
        ADDR    R1, R0
        MVO     R0, const_RV_W
        B       COMMIT
@@sky:  MVI     const_RV_IDX, R4
        ADDI    #const_SKYW, R4
        MVI@    R4, R0
        MVO     R0, const_RV_W
        MVII    #const_CL_SKY, R0
        MVO     R0, const_RV_CELL
        B       COMMIT
@@dirt: MVI     const_FV_DIRTW, R0
        MVO     R0, const_RV_W
        MVII    #const_CL_DIRT, R0
        MVO     R0, const_RV_CELL
        B       COMMIT
@@fire: CALL    SKYBG                   ; the background: the sky, unless dirt covers more of the rest
        TSTR    R3
        BEQ     @@fb
        PSHR    R1
        MVII    #const_RV_D0, R4
        CALL    POP4
        PSHR    R0
        MVII    #const_RV_F0, R4
        CALL    POP4
        PULR    R1
        SLL     R1, 1
        ADDR    R0, R1                  ; 2 dirt + fire > 64: dirt
        PULR    R0
        CMPI    #65, R1
        BNC     @@fs
        MVI     const_FV_DIRTBG, R1
        MVI     const_FV_FIREFG, R0     ; (on dirt of its own colour: the other)
        CMP     const_FV_DIRTFG, R0
        BNEQ    @@fg
        MVI     const_FV_FIREALT, R0
        B       @@fg
@@fs:   MOVR    R0, R1
@@fb:   MVI     const_FV_FIREFG, R0
@@fg:   ADDR    R0, R1
        PSHR    R1                      ; its colours
        MVI     const_RV_FAND, R2       ; all fire: the solid card
        CMPI    #$FFFF, R2
        BNEQ    @@fp
        MVII    #const_CF_FIRE, R2
        MVO     R2, const_RV_CELL
        CLRR    R0
        B       @@fw
@@fp:   MVII    #const_RV_F0, R4
        CALL    COPYP
        CALL    POOL_GET
        MVII    #const_CF_FIRE, R2
        TSTR    R0
        BNEQ    @@fc
        CALL    LEVEL
        MVII    #const_CF_FIRE + const_CF_APPROX, R2
        B       @@fe
@@fc:   ADDR    R0, R2
@@fe:   MVO     R2, const_RV_CELL
@@fw:   SLL     R0, 2
        SLL     R0, 1
        ADDI    #$800, R0
        PULR    R1
        ADDR    R1, R0
        MVO     R0, const_RV_W
        B       COMMIT
        ENDP

;; COMMIT: (RCELL's tail, its R5 on the stack) cell RV_IDX becomes RV_CELL
;; and RV_W: the counts of fire and level cells kept, its old card let go
;; (after the new one is counted: the same card stays), its word queued if
;; it changed.
COMMIT: PROC
        MVI     const_RV_IDX, R4
        ADDI    #const_CELL, R4
        MVI@    R4, R0                  ; the old entry
        MVI     const_RV_CELL, R1
        DECR    R4
        MVO@    R1, R4
        MOVR    R0, R2                  ; what changed
        XORR    R1, R2
        MOVR    R2, R3
        ANDI    #const_CF_FIRE, R3
        BEQ     @@nf
        MVI     const_FV_NFIRE, R3
        INCR    R3
        MOVR    R1, R4
        ANDI    #const_CF_FIRE, R4
        BNEQ    @@f1
        SUBI    #2, R3
@@f1:   MVO     R3, const_FV_NFIRE
@@nf:   ANDI    #const_CF_APPROX, R2
        BEQ     @@na
        MVI     const_FV_NAPPROX, R3
        INCR    R3
        ANDI    #const_CF_APPROX, R1
        BNEQ    @@a1
        SUBI    #2, R3
@@a1:   MVO     R3, const_FV_NAPPROX
@@na:   ANDI    #const_CF_CARD, R0
        CALL    POOL_PUT
        MVI     const_RV_IDX, R4
        ADDI    #const_CELLW, R4
        MVI@    R4, R1
        MVI     const_RV_W, R0
        CMPR    R0, R1
        BEQ     @@x
        DECR    R4
        MVO@    R0, R4
        MOVR    R0, R1
        MVI     const_RV_IDX, R0
        ADDI    #$200 + 40, R0
        CALL    GQPUT
@@x:    PULR    PC
        ENDP

;; RENDER(n): up to n queued cells drawn. Stops early with the GRAM queue
;; near full (the frame handler empties it a frame at a time).
RENDER: PROC
        PSHR    R5
        MVO     R0, const_RV_N
@@next: MVI     const_RV_N, R0
        TSTR    R0
        BEQ     @@x
        DECR    R0
        MVO     R0, const_RV_N
        MVI     const_FV_DQH, R1
        CMP     const_FV_DQT, R1
        BEQ     @@x                     ; none queued
        MVI     const_FV_GQH, R0        ; room for a card and a word, and to spare
        SUB     const_FV_GQT, R0
        BGT     @@r
        ADDI    #192, R0
@@r:    CMPI    #8, R0
        BLT     @@x
        MOVR    R1, R4
        ADDI    #const_DIRTYQ, R4
        MVI@    R4, R2
        INCR    R1
        CMPI    #208, R1
        BNEQ    @@q
        CLRR    R1
@@q:    MVO     R1, const_FV_DQH
        MVO     R2, const_RV_IDX
        CALL    RCELL
        B       @@next
@@x:    PULR    PC
        ENDP

;; FLD_OPEN: the field on the screen anew (SKYW, FV_DIRTW, FV_DIRTFG,
;; FV_DIRTBG, FV_FIREFG, FV_FIREALT set): GRAMQ and the pool empty, every
;; cell its sky for now (BACKTAB too) and queued to be drawn.
FLD_OPEN: PROC
        PSHR    R5
        DIS
        CLRR    R0
        MVO     R0, const_FV_GQH
        MVO     R0, const_FV_GQT
        EIS
        MVII    #const_PREF, R4         ; the pool: PREF to PPEND's end
        MVII    #const_PPEND + 44 - const_PREF, R1
@@p:    MVO@    R0, R4
        DECR    R1
        BNEQ    @@p
        MVO     R0, const_FV_CLOCK
        MVO     R0, const_FV_NFIRE
        MVO     R0, const_FV_NAPPROX
        MVO     R0, const_FV_RETRY
        MVO     R0, const_FV_DQH
        MVII    #200, R1
        MVO     R1, const_FV_DQT
        MVII    #const_SKYW, R4
        MVII    #const_CELLW, R5
        MVII    #$200 + 40, R3
        CLRR    R2                      ; the cell
@@c:    MVI@    R4, R0
        MVO@    R0, R5
        MVO@    R0, R3
        INCR    R3
        MOVR    R2, R1
        ADDI    #const_DIRTYQ, R1
        MVO@    R2, R1
        ADDI    #const_CELL - const_DIRTYQ, R1
        MVII    #const_CF_DIRTY, R0
        MVO@    R0, R1
        INCR    R2
        CMPI    #200, R2
        BNEQ    @@c
        PULR    PC
        ENDP

;; FLD_LEAVE: GRAMQ emptied (a text screen comes: no field cell may write
;; over it).
FLD_LEAVE: PROC
        DIS
        MVI     const_FV_GQT, R0
        MVO     R0, const_FV_GQH
        EIS
        JR      R5
        ENDP

;; FIRE_CYCLE(fg): the fire's colour now fg: the cells with fire shown in
;; it at once (their BACKTAB words' foreground, the frame handler kept out
;; while each is changed).
FIRE_CYCLE: PROC
        MVO     R0, const_FV_FIREFG
        MVI     const_FV_NFIRE, R1
        TSTR    R1
        BEQ     @@x
        MOVR    R0, R3
        MVII    #const_CELL, R4
        MVII    #$200 + 40, R2
@@c:    MVI@    R4, R0
        ANDI    #const_CF_FIRE, R0
        BEQ     @@n
        DIS
        MVI@    R2, R1
        ANDI    #$FFF8, R1
        ADDR    R3, R1
        MVO@    R1, R2
        EIS
@@n:    INCR    R2
        CMPI    #$200 + 240, R2
        BNEQ    @@c
@@x:    JR      R5
        ENDP

;; RETRY: with a pool card free, the cells drawn with level cards queued
;; again.
RETRY:  PROC
        PSHR    R5
        MVI     const_FV_RETRY, R0
        TSTR    R0
        BEQ     @@x
        CLRR    R0
        MVO     R0, const_FV_RETRY
        CLRR    R1
@@c:    MOVR    R1, R4
        ADDI    #const_CELL, R4
        MVI@    R4, R0
        ANDI    #const_CF_APPROX + const_CF_DIRTY, R0
        CMPI    #const_CF_APPROX, R0
        BNEQ    @@n
        PSHR    R1
        CALL    DIRTY
        PULR    R1
@@n:    INCR    R1
        CMPI    #200, R1
        BNEQ    @@c
@@x:    PULR    PC
        ENDP
