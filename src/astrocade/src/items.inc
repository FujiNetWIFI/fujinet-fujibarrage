; items.inc -- the items line's frame (src/play.c's items_frame), on the OPS
; page for room (the PLAY page calls it FAR; the HUD's right half shows it:
; p_play.inc HUDR). Its state is the PLAY page's GAMEV bytes (IMENU, ITEM,
; GUIDE, TARGET, DRIVEN, PRVD; T3 defending, as PFRAME left it).

; ---- the items line (src/play.c's items_frame) ----------------------------------------
; x opens it on our turn or in the defence window: the stick's left and
; right pick an item we have, FIRE uses it. Guidance asks for its target
; next (left and right; an arrow over it; FIRE takes it: our next shot
; steers for it). Fuel drives (the stick held left or right; FIRE stops).
; x, CE or C shuts it (C otherwise leaves the room). The Defense Computer
; works by itself: not listed.
IOPEN:  LD      A,N_ITEMS-1
        LD      C,1
        CALL    NEXTI
        CP      I_NONE
        JR      NZ,IO1
        SND     UI_ERROR
        RET
IO1:    PUSH    AF
        SND     UI_SELECT
        POP     AF
IO2:    LD      (ITEM),A
        LD      A,M_ITEM
        LD      (IMENU),A
        LD      A,(INDIR)
        LD      (PRVD),A
        LD      A,1
        LD      (HUDDIRTY),A
        RET

; IDOPEN: the defence window's line, opened by itself (its cue already
; played): its first item (the defences alone: NEXTI), nothing if none
IDOPEN: LD      A,N_ITEMS-1
        LD      C,1
        CALL    NEXTI
        CP      I_NONE
        RET     Z
        JR      IO2

ICLOSE: CALL    MARKOFF
        XOR     A
        LD      (IMENU),A
        INC     A
        LD      (HUDDIRTY),A
        RET

IFRAME: LD      A,(EVKEY)       ; x, CE or C: shut
        CP      K_MUL
        JR      Z,ICLOSE
        CP      K_BACK
        JR      Z,ICLOSE
        CP      K_CLR
        JR      Z,ICLOSE
        LD      A,(PRVD)        ; C: a push's step (-1, 1; 0 none)
        CPL
        LD      B,A
        LD      A,(INDIR)
        LD      (PRVD),A
        AND     B
        LD      C,0
        BIT     2,A
        JR      Z,IF0
        DEC     C
IF0:    BIT     3,A
        JR      Z,IF0A
        INC     C
IF0A:   LD      A,1
        LD      (HUDDIRTY),A    ; (the line drawn again only if something moved)
        LD      A,(IMENU)
        CP      M_TARGET
        JR      Z,IFTGT
        CP      M_DRIVE
        JP      Z,IFDRV
        LD      A,C             ; the items: a step
        OR      A
        JR      Z,IF1
        LD      A,(ITEM)
        CALL    NEXTI
        LD      (ITEM),A
        SND     UI_TICK
        RET
IF1:    LD      A,(EVBTN)       ; FIRE: use it
        OR      A
        JR      Z,IFQ
        SND     UI_SELECT
        LD      A,(T3)
        OR      A
        JR      NZ,IF3
        LD      A,(ITEM)
        CP      I_AUTOPILOT+1   ; guidance: its target next
        JR      NC,IF2
        LD      (GUIDE),A
        LD      A,(TARGET)
        LD      C,1
        CALL    NEXTT
        LD      (TARGET),A
        LD      A,M_TARGET
        LD      (IMENU),A
        JP      MARKON
IF2:    CP      I_FUEL          ; fuel: driving
        JR      NZ,IF3
        LD      A,M_DRIVE
        LD      (IMENU),A
        XOR     A
        LD      (DRIVEN),A
        RET
IF3:    LD      A,(ITEM)        ; the rest: used now (the parachute and the
        LD      (AARGS),A       ;  impact fuse switch: the line stays)
        LD      B,ACT_USE
        LD      C,1
        CALL    ACTQ
        LD      A,(ITEM)
        CP      I_PARACHUTE
        RET     Z
        CP      I_IMPACT_FUSE
        RET     Z
        LD      A,(T3)          ; (defending: more to set up)
        OR      A
        RET     NZ
        JP      ICLOSE
IFQ:    XOR     A               ; nothing moved
        LD      (HUDDIRTY),A
        RET
IFTGT:  LD      A,C             ; the target: a step
        OR      A
        JR      Z,IFT1
        PUSH    BC
        CALL    MARKOFF
        POP     BC
        LD      A,(TARGET)
        CALL    NEXTT
        LD      (TARGET),A
        CALL    MARKON
        SND     UI_TICK
        RET
IFT1:   LD      A,(EVBTN)       ; FIRE: taken
        OR      A
        JR      Z,IFQ
        SND     UI_SELECT
        JP      ICLOSE
IFDRV:  LD      A,(INDIR)       ; driving: an ACT_DRIVE each 8 frames held
        AND     D_LEFT|D_RIGHT
        JR      NZ,IFD1
        LD      (DRIVEN),A
        JR      IFD3
IFD1:   LD      HL,DRIVEN
        LD      A,(HL)
        INC     (HL)
        AND     7
        JR      NZ,IFD3
        LD      A,(INDIR)
        AND     D_RIGHT
        JR      Z,IFD2
        LD      A,1
IFD2:   LD      (AARGS),A
        LD      B,ACT_DRIVE
        LD      C,1
        CALL    ACTQ
IFD3:   LD      A,(EVBTN)       ; FIRE: stop
        OR      A
        JR      Z,IFQ
        JP      ICLOSE

; NEXTI: A = the next item we have from item A, a step C (1, -1); I_NONE
; if none (play.c next_item).
NEXTI:  LD      B,N_ITEMS-N_WEAPONS
INX1:    ADD     A,C
        CP      N_WEAPONS
        JR      NC,INX2
        LD      A,N_ITEMS-1
INX2:    CP      N_ITEMS
        JR      C,INX3
        LD      A,N_WEAPONS
INX3:    CP      I_DEFENSE_COMPUTER
        JR      Z,INX4
        LD      E,A
        LD      A,(DEFOPN)      ; the defence window: the defences alone
        OR      A
        LD      A,E
        JR      Z,INX3A
        CP      I_PARACHUTE
        JR      C,INX4
        CP      I_SUPER_DEFLECTOR+1
        JR      C,INX3A
        CP      I_IMPACT_FUSE
        JR      NZ,INX4
INX3A:   LD      E,A
        LD      D,0
        LD      HL,ST+OFF_INV
        ADD     HL,DE
        INC     (HL)
        DEC     (HL)
        RET     NZ
INX4:    DJNZ    INX1
        LD      A,I_NONE
        RET

; NEXTT: A = the next tank on, not ours, from seat A, a step C; NO_SEAT if
; none (play.c next_target).
NEXTT:  LD      B,SEATS
ITX1:    ADD     A,C
        CP      SEATS
        JR      C,ITX2
        LD      A,0             ; (past either end: the other)
        BIT     7,C
        JR      Z,ITX2
        LD      A,SEATS-1
ITX2:    LD      E,A
        LD      HL,MYSEAT
        CP      (HL)
        JR      Z,ITX3
        LD      D,0
        LD      HL,TKON
        ADD     HL,DE
        INC     (HL)
        DEC     (HL)
        LD      A,E
        RET     NZ
ITX3:    LD      A,E
        DJNZ    ITX1
        LD      A,NO_SEAT
        RET

; TMFRAME: our turn's arrow (3 fire pixels over our tank, over the sky
; only, as the target's): blinking (16 frames on, 16 off) while we aim, off
; otherwise. TMOFF: off (a script is coming). Where it was drawn is kept:
; a tank that drives leaves nothing behind.
TMFRAME: LD     A,(MYTURN)
        OR      A
        JR      Z,TMOFF
        LD      A,(FRAMES)
        AND     16
        JR      Z,TMOFF
        LD      A,(TMK)
        OR      A
        RET     NZ
        LD      A,(MYSEAT)
        CP      SEATS
        RET     NC
        LD      C,A
        LD      B,0
        LD      HL,TKX
        ADD     HL,BC
        LD      A,(HL)
        LD      (TMKX),A
        LD      HL,TKY
        ADD     HL,BC
        LD      A,(HL)
        SUB     TANK_H+4
        LD      (TMKY),A
        LD      A,1
        LD      (TMK),A
        LD      DE,PX_SKY*256+PX_FX
        JR      TMK0
TMOFF:  LD      A,(TMK)
        OR      A
        RET     Z
        XOR     A
        LD      (TMK),A
        LD      DE,PX_FX*256+PX_SKY
TMK0:   LD      A,(TMKX)
        DEC     A               ; x - 1, y; x, y + 1; x + 1, y
        LD      B,A
        LD      A,(TMKY)
        LD      C,A
        PUSH    BC
        CALL    OQ_SWAP
        POP     BC
        INC     B
        INC     C
        PUSH    BC
        CALL    OQ_SWAP
        POP     BC
        INC     B
        DEC     C
        JP      OQ_SWAP

; MARKON, MARKOFF: an arrow (3 fire pixels) over the target, over the sky
; only (play.c mark).
MARKON: LD      DE,PX_SKY*256+PX_FX
        JR      MK0
MARKOFF: LD     DE,PX_FX*256+PX_SKY
MK0:    LD      A,(TARGET)
        CP      SEATS
        RET     NC
        LD      C,A
        LD      B,0
        LD      HL,TKX
        ADD     HL,BC
        LD      A,(HL)
        LD      HL,TKY-TKX
        ADD     HL,BC
        LD      BC,TKX
        ADD     HL,BC
        LD      C,(HL)          ; y
        DEC     A               ; x - 1, y - TANK_H - 4
        LD      B,A
        LD      A,C
        SUB     TANK_H+4
        LD      C,A
        PUSH    BC
        CALL    OQ_SWAP
        POP     BC
        INC     B               ; x, y - TANK_H - 3
        INC     C
        PUSH    BC
        CALL    OQ_SWAP
        POP     BC
        INC     B               ; x + 1, y - TANK_H - 4
        DEC     C
        JP      OQ_SWAP

