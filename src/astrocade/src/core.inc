; core.inc -- start-up, the raster, paging, the frame, input. Resident.
; FujiBlast's (src/astrocade/src/core.inc), with the field's raster.
;
; Interrupts are ON: IM 2, I = 4FH (refresh cycles land in screen RAM, never
; on the cart's hotspot pages), the vector word in RAM. The handlers own the
; shadow registers (EXX, EX AF,AF'): nothing else may use them. They never
; touch MAGIC or XPAND. The frame interrupt selects the sound's page with a
; raw read, so the mailbox's register pairs run with DI (fujilib.inc).

; ---- start-up --------------------------------------------------------------------
; RESET restarts here (RSTENT) but never resets the cart: the page, the
; socket and ACKSEQ are as they were. So everything is set up afresh.
PRGSTR: DI
        LD      SP,STACK
        LD      HL,STKLOW       ; the stack's cells: A5H (a rig counts the ones
        LD      DE,STKLOW+1     ;  left at its bottom: the high-water mark)
        LD      BC,STACK-STKLOW-1
        LD      (HL),0A5H
        LDIR
        LD      HL,NORMEM       ; the picture and every variable: zero (the BIOS's
        LD      DE,NORMEM+1     ;  menu leaves its text in the variables)
        LD      BC,STKLOW-NORMEM-1
        LD      (HL),0
        LDIR
        CALL    SNDOFF          ; the chip silent (SNDSH is zero)
        LD      A,0FFH
        LD      (MYSEAT),A
        LD      (KRAW),A
        LD      (SNDQ),A
        LD      (SNDQ2),A
        LD      HL,B_PLAIN      ; a text screen: white on black
        CALL    TEXTSCR
        CALL    INTON
        LD      A,(FNMAGF)      ; the cart: 'F','N', protocol 2 or later
        CP      'F'
        JR      NZ,STBAD
        LD      A,(FNMAGN)
        CP      'N'
        JR      NZ,STBAD
        LD      A,(FNPVER)
        CP      2
        JP      C,STBAD
        LD      A,(FLAGS)       ; the sound plays from the frame interrupt now
        OR      FL_SNDON
        LD      (FLAGS),A
        XOR     A               ; the main loop: page 0's (mapped, CURPG 0)
        LD      (CURPG),A
        LD      A,(FNBKSEL+PG_BOOT)
        LD      HL,MAININIT
        JP      (HL)
; STBAD: no FujiNet cart, or one too old for APPBANK images: say so, stop.
STBAD:  LD      HL,SNEEDFW
        LD      DE,5*TXH*256+12
        LD      C,3*4
        CALL    TXTAT
        LD      HL,SNEEDF2
        LD      DE,6*TXH*256+12
        LD      C,3*4
        CALL    TXTAT
STHALT: HALT
        JR      STHALT
SNEEDFW: DB     "NEEDS A FUJINET",0
SNEEDF2: DB     "CART, PROTO 2",0

; ---- the raster ------------------------------------------------------------------
; INTON: IM 2 through VECTOR; the frame interrupt starts the chain.
INTON:  LD      HL,VBISR
        LD      (VECTOR),HL
        LD      A,4FH           ; I: refresh cycles stay in screen RAM
        LD      I,A
        IM      2
        LD      A,VECTOR AND 0FFH
        OUT     (INFBK),A
        LD      A,VBLLN
        OUT     (INLIN),A
        LD      A,08H           ; the screen's interrupt on, held until taken
        OUT     (INMOD),A
        EI
        RET

; BANDISR: a field band's colours: HORCB (its split), L0 and R0 (its two
; tanks' colours), L1 = R1 (its sky). Timed: the five writes must land in
; the band's first scanline (it comes a scanline early: BANDOFS), so the
; entry's values wait in the shadow registers (E HORCB, B L0, C R0, D sky),
; loaded by the last interrupt; the next INLIN goes first, then BPAD NOPs.
BANDISR: EX     AF,AF'
        EXX
        LD      A,(NXTLN)       ; the next band's interrupt, NBL lines on
        ADD     A,2*NBL
        LD      (NXTLN),A
        OUT     (INLIN),A
        DS      BPAD            ; NOPs
        LD      A,E
        OUT     (HORCB),A
        LD      A,B
        OUT     (COL0L),A
        LD      A,C
        OUT     (COL0R),A
        LD      A,D
        OUT     (COL1L),A
        OUT     (COL1R),A
        LD      A,(NXTLN)       ; the frame's interrupt next? (at or past
        CP      VBLLN           ;  VBLLN: a shake starts the bands 2 lines on)
        JR      C,BI1
        LD      HL,VBISR
        LD      (VECTOR),HL
BI1:    LD      HL,(BANDP)      ; the next band's entry, for the next interrupt
        LD      E,(HL)
        INC     L
        LD      B,(HL)
        INC     L
        LD      C,(HL)
        INC     L
        LD      (BANDP),HL
        LD      HL,(SKYP)
        LD      D,(HL)
        INC     HL
        LD      (SKYP),HL
        EXX
        EX      AF,AF'
        EI
        RETI

FLSKY:  DB      07H,07H,07H,07H,07H,07H,07H,07H ; a flash's sky: white bands
        DB      07H,07H,07H,07H,07H,07H,07H,07H

; VBISR: the frame interrupt, under the field and its bars (or a text
; screen). On the field: the border below and the next frame's HUD in one
; HORCB (b6-7 colour 2: the ground's; b0-5 the HUD's split), the HUD's
; colours, the frame's dirt and fire, the bands' table (BANDA), the
; first band's entry waiting in the shadow registers. On a text screen:
; TEXTISR's table. Then the frame counted, and the sound's jiffy.
VBISR:  EX      AF,AF'
        EXX
        LD      HL,(FRAMES)
        INC     HL
        LD      (FRAMES),HL
        LD      A,(FLAGS)
        AND     FL_FIELD
        JP      Z,VBTXT
        LD      A,80H+20        ; the HUD: bytes 0-19 left, 20-39 right
        OUT     (HORCB),A
        LD      HL,HUDC
        LD      A,(HL)
        OUT     (COL0L),A
        INC     HL
        LD      A,(HL)
        OUT     (COL1L),A
        INC     HL
        LD      A,(HL)
        OUT     (COL0R),A
        INC     HL
        LD      A,(HL)
        OUT     (COL1R),A
        LD      A,(FLASHN)      ; a flash: the dirt lit, the fire white
        OR      A
        JR      Z,VBD
        LD      A,(DIRTC)
        OR      6
        OUT     (COL2L),A
        OUT     (COL2R),A
        LD      A,07H
        JR      VB1
VBD:    LD      A,(DIRTC)       ; the dirt, the fire
        OUT     (COL2L),A
        OUT     (COL2R),A
        LD      A,(FLAGS)
        AND     FL_FIRE
        LD      A,(FIREC)
        JR      Z,VB1
        LD      A,(FRAMES)      ; live fire: its cycle, a step every 4 frames
        RRCA
        RRCA
        AND     7
        LD      HL,FIRECYC
        ADD     A,L
        LD      L,A
        JR      NC,VB0
        INC     H
VB0:    LD      A,(HL)
VB1:    OUT     (COL3L),A
        OUT     (COL3R),A
        LD      HL,BANDA        ; the bands (one table: two, alternating each
                                ;  frame, strobed near the ground on a TV)
        LD      E,(HL)          ; the first band's entry, waiting
        INC     L
        LD      B,(HL)
        INC     L
        LD      C,(HL)
        INC     L
        LD      (BANDP),HL
        LD      HL,(SKYTB)      ; the sky (a flash: white, its frames counted)
        LD      A,(FLASHN)
        OR      A
        JR      Z,VB3
        DEC     A
        LD      (FLASHN),A
        LD      HL,FLSKY
VB3:    LD      D,(HL)
        INC     HL
        LD      (SKYP),HL
        LD      A,2*FROW-BANDOFS        ; the first band's interrupt
        OUT     (INLIN),A
        LD      A,(SHAKE)       ; a shake: the first band 2 lines taller, every
        OR      A               ;  band after it 2 lines down, 2 frames in 4
        JR      Z,VB4           ;  (the picture itself can't move)
        DEC     A
        LD      (SHAKE),A
        AND     2
        LD      A,2*FROW-BANDOFS+4
        JR      NZ,VB5
VB4:    LD      A,2*FROW-BANDOFS
VB5:    LD      (NXTLN),A
        LD      HL,BANDISR
        LD      (VECTOR),HL
        JR      VBSND
VBTXT:  LD      HL,(TXPAL)      ; a text screen: its kept colours (L0, L2, L3,
        LD      A,(HL)          ;  R0), then its band table's first entry
        OUT     (COL0L),A
        INC     HL
        LD      A,(HL)
        OUT     (COL2L),A
        INC     HL
        LD      A,(HL)
        OUT     (COL3L),A
        INC     HL
        LD      A,(HL)
        OUT     (COL0R),A
        LD      HL,(BANDTB)
        LD      A,(HL)
        INC     HL
        LD      BC,4*256+COLBX
        OUT     (HORCB),A
        OUTI
        OUTI
        OUTI
        LD      A,(HL)
        INC     HL
        OUT     (COL1L),A
        LD      A,(HL)
        INC     HL
        LD      (BANDP),HL
        OUT     (INLIN),A
        CP      TVBLLN
        JR      Z,VBSND         ; one band only: the frame again
        LD      HL,TXTISR
        LD      (VECTOR),HL
; the sound's jiffy (the sound page's E_TICK), in the shadow registers; IX
; kept. The interrupt may land anywhere, so the sound's page is mapped by
; the raw select (not SELPG: CURPG is the program's page and stays) and the
; interrupted page selected again after. Then the first band's entry is
; loaded again (the tick takes the shadow registers).
VBSND:  LD      A,(FLAGS)
        AND     FL_SNDON
        JR      Z,VB9
        PUSH    IX
        LD      A,(FNBKSEL+PG_SND)
        CALL    E_TICK
        LD      A,(CURPG)
        ADD     A,80H
        LD      E,A
        LD      D,FNRSELP
        LD      A,(DE)
        POP     IX
        LD      HL,(BANDP)      ; the first band's entry again (E, B, C, D:
        DEC     L               ;  the tick had them; nothing pushed, as this
        LD      C,(HL)          ;  runs on top of the program's deepest. A
        DEC     L               ;  text screen's interrupt keeps its own.)
        LD      B,(HL)
        DEC     L
        LD      E,(HL)
        LD      HL,(SKYP)
        DEC     HL
        LD      D,(HL)
VB9:    EXX
        EX      AF,AF'
        EI
        RETI

; TXTISR: a text screen's band (FujiBlast's BANDISR): HORCB, R3, R2, R1,
; L1, then the next band's scanline (TVBLLN after the last). Timed as the
; field's: its five writes must land in the band's first scanline; it
; reads its entry first, so it pads less (TBPAD, FujiBlast's measure).
TXTISR: PUSH    AF
        PUSH    BC
        PUSH    HL
        LD      HL,(BANDP)
        LD      A,(HL)          ; HORCB
        INC     HL
        LD      BC,4*256+COLBX  ; OUTI: registers 3, 2, 1 (B after its DEC)
        DS      TBPAD           ; NOPs
        OUT     (HORCB),A
        OUTI                    ; R3
        OUTI                    ; R2
        OUTI                    ; R1
        LD      A,(HL)
        INC     HL
        OUT     (COL1L),A       ; L1
        LD      A,(HL)          ; the next interrupt's scanline
        INC     HL
        LD      (BANDP),HL
        OUT     (INLIN),A
        CP      TVBLLN
        JR      NZ,TI1
        LD      HL,VBISR        ; that was the last band: the frame next
        LD      (VECTOR),HL
TI1:    POP     HL
        POP     BC
        POP     AF
        EI
        RETI

; FIELDSCR: the field screen's raster from the next frame: LINES lines,
; the bands (BANDA, SKYTB, HUDC, DIRTC, FIREC set). Clobbers A.
FIELDSCR: LD    A,VERBLV
        OUT     (VERBL),A
        DI
        LD      A,(FLAGS)
        OR      FL_FIELD
        LD      (FLAGS),A
        EI
        RET

; TEXTSCR: a text screen: band table HL (resident), the plain kept colours
; (P_TEXT), TLINES lines, all cleared. TEXTBLANK: the same, all black (the
; field arriving over a text screen). Clobber A, BC, DE, HL.
TEXTBLANK: LD   HL,B_BLANK
TEXTSCR: DI
        LD      (BANDTB),HL
        LD      HL,P_TEXT
        LD      (TXPAL),HL
        LD      A,(FLAGS)
        AND     0FFH-FL_FIELD
        OR      FL_WIPED        ; (the field's RAM goes, its version with it)
        LD      (FLAGS),A
        EI
        LD      A,TVERBL
        OUT     (VERBL),A
        LD      HL,SCR
        LD      DE,SCR+1
        LD      BC,TLINES*BYTEPL-1
        LD      (HL),0
        LDIR
        RET

; ---- paging ----------------------------------------------------------------------
; SELPG: map page A at 2000H (one read; the next read sees it). Preserves
; BC, DE, HL. Resident code only.
SELPG:  LD      (CURPG),A
        PUSH    HL
        ADD     A,80H
        LD      L,A
        LD      H,FNRSELP
        LD      A,(HL)
        POP     HL
        RET

; FARCALL: call into a page; the page and address follow the CALL:
;       CALL    FARCALL
;       DB      page
;       DW      routine
; A, BC, DE, HL go in and come back as the routine left them (and F). The
; caller's page is mapped again on return. IX and IY are the routine's to
; keep. (M.U.L.E.'s.)
FARCALL: LD     (FCA),A
        LD      (FCHL),HL
        POP     HL
        LD      A,(HL)          ; page
        INC     HL
        PUSH    DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      (FCTGT),DE
        POP     DE
        PUSH    HL              ; the real return
        LD      H,A
        LD      A,(CURPG)
        PUSH    AF              ; the caller's page
        LD      A,H
        CALL    SELPG
        LD      HL,(FCHL)
        LD      A,(FCA)
        CALL    FCJMP
FCBACK: LD      (FCHL),HL
        POP     HL              ; H = the caller's page
        PUSH    AF
        LD      A,H
        CALL    SELPG
        POP     AF
        LD      HL,(FCHL)
        RET
FCJMP:  PUSH    HL
        LD      HL,(FCTGT)
        EX      (SP),HL
        RET

; PGCALL: entry HL of page A (E_*), the caller's page back after. BC, DE
; go in; A, F, BC, DE, HL come back as the entry left them.
PGCALL: LD      (FCTGT),HL
        LD      H,A
        LD      A,(CURPG)
        PUSH    AF
        LD      A,H
        CALL    SELPG
        CALL    FCJMP
        JR      FCBACK

; ---- the frame ----------------------------------------------------------------------
; FRAME: the next frame: wait for the frame interrupt to count one (HALT:
; every band's interrupt wakes it), DT, the controls; MR turns the sound
; off and on. Clobbers A, BC, DE, HL.
FRAME:  LD      HL,(LASTF)
FR1:    LD      DE,(FRAMES)
        LD      A,E
        CP      L
        JR      NZ,FR2
        HALT
        JR      FR1
FR2:    LD      (LASTF),DE
        EX      DE,HL           ; frames since the last
        OR      A
        SBC     HL,DE
        LD      A,H
        OR      A
        JR      NZ,FR3
        LD      A,L
        CP      9
        JR      C,FR4
FR3:    LD      A,8
FR4:    LD      (DT),A
        LD      A,(INDIR)
        LD      (PRVDIR),A
        CALL    INPOLL
        LD      A,(EVKEY)
        CP      K_MUTE
        RET     NZ
        LD      A,K_NONE        ; (never the game's)
        LD      (EVKEY),A
        LD      A,(FLAGS)
        XOR     FL_MUTE
        LD      (FLAGS),A
        RET

; ---- input --------------------------------------------------------------------------
; INPOLL: this frame's controls. INDIR: directions held (the handle's own
; bits, diagonals too). INBTN: the trigger held, EVBTN: it went down.
; EVKEY: a key that went down (K_*, digits). Clobbers A, BC, DE, HL.
INPOLL: IN      A,(SW0)
        LD      C,A
        AND     0FH
        LD      (INDIR),A
        LD      A,C
        AND     10H
        LD      HL,INBTN
        JR      Z,IP1
        LD      A,(HL)          ; down: an event if it was up
        XOR     1
        LD      (EVBTN),A
        LD      (HL),1
        JR      IP2
IP1:    LD      (EVBTN),A
        LD      (HL),A
IP2:    CALL    KEYRAW          ; A = the key held, K_NONE none
        LD      HL,KRAW
        LD      C,(HL)
        LD      (HL),A
        CP      C
        JR      NZ,IP3
        LD      A,K_NONE        ; held: no event
IP3:    LD      (EVKEY),A
        RET

; KEYRAW: A = the key held on the keypad now (KPTAB), K_NONE if none: the
; first found. Clobbers A, BC, DE, HL.
KEYRAW: LD      HL,KPTAB
        LD      C,KEY0
KRC:    IN      A,(C)
        LD      D,A
        LD      E,6
KRB:    SRL     D
        JR      NC,KRB2
        LD      A,(HL)
        CP      K_NONE
        RET     NZ
KRB2:   INC     HL
        DEC     E
        JR      NZ,KRB
        INC     C
        LD      A,C
        CP      KEY3+1
        JR      NZ,KRC
        LD      A,K_NONE
        RET
; per column 14H..17H, bit 0 first (MAME's astrohome.cpp)
KPTAB:  DB      K_PCT,K_DIV,K_MUL,K_MINUS,K_PLUS,K_ENT          ; % / x - + =
        DB      K_DN,K_CH,9,6,3,K_DOT                           ; v CH 9 6 3 .
        DB      K_UP,K_MS,8,5,2,0                               ; ^ MS 8 5 2 0
        DB      K_CLR,K_MUTE,7,4,1,K_BACK                       ; C MR 7 4 1 CE

; ---- odds and ends --------------------------------------------------------------------
; RAND: A = a random byte: a 16-bit Galois LFSR, seeded from the clock
; (M.U.L.E.'s). Preserves everything else.
RAND:   PUSH    HL
        LD      HL,(RNDS)
        LD      A,H
        OR      L
        JR      NZ,RND1
        LD      HL,(FRAMES)
        SET     0,L
RND1:   LD      A,8
RND2:   SRL     H
        RR      L
        JR      NC,RND3
        PUSH    AF
        LD      A,H
        XOR     0B4H
        LD      H,A
        POP     AF
RND3:   DEC     A
        JR      NZ,RND2
        LD      (RNDS),HL
        LD      A,L
        POP     HL
        RET

; ACTQ: an action to send: B its kind (ACT_*), C its args' count (0-6, in
; AARGS); the next seq (never 0). One waits at a time: a second replaces it.
; Clobbers A.
ACTQ:   LD      A,B
        LD      (AK),A
        LD      A,C
        LD      (ANARG),A
        LD      A,(TXF)
        OR      TX_ACT
        LD      (TXF),A
        LD      A,(ASEQ)
        INC     A
        JR      NZ,ACQ1
        INC     A
ACQ1:   LD      (ASEQ),A
        RET

; ---- the state's seats --------------------------------------------------------------
; SEATP: HL = seat A's record in ST (OFF_SEATS + 25 A). Preserves BC, DE.
SEATP:  PUSH    DE
        PUSH    BC
        LD      L,A
        LD      H,0
        LD      D,H
        LD      E,L             ; A
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL           ; 8A
        LD      B,H
        LD      C,L
        ADD     HL,HL           ; 16A
        ADD     HL,BC           ; 24A
        ADD     HL,DE           ; 25A
        LD      DE,ST+OFF_SEATS
        ADD     HL,DE
        POP     BC
        POP     DE
        RET
; SNAME: seat A's name into WORK (NUL-ended). Preserves IX, IY.
SNAME:  CALL    SEATP
        LD      DE,WORK
        LD      BC,NAME_LEN
        LDIR
        XOR     A
        LD      (DE),A
        RET

; REP: A = the directions moved this frame: a push, then every 4 frames once
; held 16 (room.c rep()). Clobbers HL, DE.
REP:    LD      A,(PRVDIR)
        CPL
        LD      HL,INDIR
        AND     (HL)            ; pushed now
        JR      NZ,RP1
        LD      A,(HL)          ; held: due again?
        OR      A
        RET     Z
        LD      HL,(FRAMES)
        LD      DE,(RPNEXT)
        OR      A
        SBC     HL,DE
        BIT     7,H
        LD      A,0
        RET     NZ
        LD      HL,(FRAMES)
        LD      DE,4
        ADD     HL,DE
        LD      (RPNEXT),HL
        LD      A,(INDIR)
        RET
RP1:    LD      HL,(FRAMES)
        LD      DE,16
        ADD     HL,DE
        LD      (RPNEXT),HL
        RET
