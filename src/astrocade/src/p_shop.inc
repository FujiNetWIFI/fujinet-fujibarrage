; p_shop.inc -- the shop, a round's end, the game's (src/room.c). Text
; screens. The shop (B_SHOP): rows 0-1 the heading and the cash, rows 2-10
; the list (what the arms level allows, the stick moves, FIRE buys, LEFT
; sells: the STATE's cash and stock answer each), row 11 the line, row 12
; the hints; = is done. The results (B_SEATS): row 0 who won, rows 1-10
; the seats (p_room's WSEAT).

        JP      SENTER          ; E_ENTER: the shop, or the results (SHOWN)
        JP      SSTATE          ; E_STATE
        JP      SFRAME          ; E_TICK

SHROWS  EQU     9               ; the list's rows (2-10)
SHSEL   EQU     GAMEV           ; the shop's own (GAMEV): the one chosen,
SHTOP   EQU     GAMEV+1         ;  the first shown, how many,
SHN     EQU     GAMEV+2
SHMASK  EQU     GAMEV+3         ;  which items (6 bytes: a bit each)
SHMSG   EQU     GAMEV+9         ;  the line's message shown (0 none)

SENTER: LD      A,(SHOWN)
        CP      S_SHOP
        JP      NZ,RENTER
        XOR     A
        LD      (SHSEL),A
        LD      (SHTOP),A
        LD      (SHN),A
        LD      (SHMSG),A
        LD      HL,SHMASK       ; the list: what the arms level allows
        LD      B,6
SE1:    LD      (HL),0
        INC     HL
        DJNZ    SE1
        LD      C,0
SE2:    LD      A,C
        CP      I_MINI_MISSILE
        JR      Z,SE3
        PUSH    BC
        FAR     PG_CAT,CIINFO   ; A: its arms level
        POP     BC
        LD      HL,ST+OFF_SETTINGS+SET_ARMS
        CP      (HL)
        JR      Z,SE2A
        JR      NC,SE3
SE2A:   LD      A,C
        CALL    MBIT
        OR      (HL)
        LD      (HL),A
        LD      HL,SHN
        INC     (HL)
SE3:    INC     C
        LD      A,C
        CP      N_ITEMS
        JR      C,SE2
        LD      HL,B_SHOP
        CALL    TEXTSCR
        TXT     SHHEAD,1*TXH,1,2*4
        TXT     SHHINT,12*TXH,1,3*4
        SND     MUS_SHOP
        CALL    SHALL
; SSTATE: the shop's heading, its rows (the stock), its line; the results:
; nothing new.
SSTATE: LD      A,(SHOWN)
        CP      S_SHOP
        RET     NZ
        TXT     SHTITLE,0,1,3*4 ; "SHOP  ROUND n   CASH $n"
        LD      A,(ST+OFF_ROUND)
        INC     A
        LD      B,3
        LD      DE,0*256+12
        LD      C,2*4
        CALL    NUMAT
        TXT     SHCASH,0,22,3*4
        LD      HL,ST+OFF_CASH
        LD      DE,WORK+20
        LD      BC,4
        LDIR
        LD      HL,WORK+20
        LD      DE,WORK
        CALL    NUM32
        LD      HL,WORK
        LD      DE,0*256+28
        LD      C,2*4
        LD      B,11
        CALL    TXTN
        CALL    SHOWNS          ; (the stock)
        LD      A,(SHMSG)       ; the line: done, or the timer
        OR      A
        JR      NZ,SS2
        LD      HL,SHBLANK
        LD      A,(MYSEAT)
        CP      SEATS
        JR      NC,SS1
        CALL    SEATP
        LD      DE,P_FLAGS
        ADD     HL,DE
        LD      A,(HL)
        AND     PF_DONE
        LD      HL,SHBLANK
        JR      Z,SS1
        LD      HL,SHDONE
SS1:    LD      DE,11*TXH*256+1
        LD      C,3*4
        LD      B,30
        CALL    TXTN
SS2:    LD      A,(ST+OFF_TIMER)
        LD      B,3
        LD      DE,11*TXH*256+36
        LD      C,2*4
        JP      NUMAT

; SHALL: the list's rows.
SHALL:  XOR     A
SHA1:   PUSH    AF
        CALL    SHLINE
        POP     AF
        INC     A
        CP      SHROWS
        JR      C,SHA1
        RET

; SHLINE: the list's row A (2 + A on the screen): item SHTOP + A of the
; list, its name, price, bundle, how many we have; a cursor if chosen. Each
; field written whole over the last (no clearing: no flicker).
SHLINE: CALL    SHROW
        LD      A,(T2)
        LD      HL,SHN
        CP      (HL)
        JR      C,SHL0
        LD      HL,SHBLANK      ; past the list: blank
        LD      A,(T1)
        LD      D,A
        LD      E,0
        LD      C,3*4
        LD      B,BYTEPL
        JP      TXTN
SHL0:   LD      HL,SHSEL        ; the cursor, or a space
        CP      (HL)
        LD      A,'>'
        JR      Z,SHL1
        LD      A,' '
SHL1:   PUSH    AF
        LD      A,(T1)
        LD      D,A
        LD      E,0
        POP     AF
        LD      C,1*4
        CALL    CHRAT
        LD      A,(T2)          ; the item
        CALL    NTHITEM
        LD      (T3),A
        FAR     PG_CAT,CINAME
        LD      HL,WORK
        LD      A,(T1)
        LD      D,A
        LD      E,1
        LD      C,3*4
        LD      B,19
        CALL    TXTN
        LD      A,(T3)          ; its price
        FAR     PG_CAT,CIINFO
        PUSH    BC              ; (B: its bundle)
        LD      (WORK+20),DE
        LD      L,C
        LD      H,0
        LD      (WORK+22),HL
        LD      HL,WORK+20
        LD      DE,WORK
        CALL    NUM32
        LD      HL,WORK
        LD      A,(T1)
        LD      D,A
        LD      E,20
        LD      C,3*4
        LD      B,7
        CALL    TXTN
        POP     BC              ; "X" and its bundle
        LD      A,'X'-20H
        CALL    GLYPH
        LD      A,B
        LD      B,2
        CALL    NUMN
        JR      SHOWN2
; SHROW: T0 = A (the list's row), T1 its line, T2 its place in the list.
SHROW:  LD      (T0),A
        ADD     A,2
        ADD     A,A
        LD      D,A
        ADD     A,A
        ADD     A,D
        LD      (T1),A
        LD      A,(SHTOP)
        LD      HL,T0
        ADD     A,(HL)
        LD      (T2),A
        RET
; SHOWNS: every row's count (a new state: the stock).
SHOWNS: XOR     A
SHO1:   PUSH    AF
        CALL    SHROW
        LD      A,(T2)
        LD      HL,SHN
        CP      (HL)
        JR      NC,SHO2
        CALL    NTHITEM
        LD      (T3),A
        CALL    SHOWN2
SHO2:   POP     AF
        INC     A
        CP      SHROWS
        JR      C,SHO1
        RET
; SHOWN2: item T3's count at line T1, column 33 (none: blank).
SHOWN2: LD      A,(T3)
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_INV
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        JR      NZ,SHW1
        LD      HL,SHBLANK
        LD      A,(T1)
        LD      D,A
        LD      E,33
        LD      C,1*4
        LD      B,4
        JP      TXTN
SHW1:   PUSH    AF
        LD      A,(T1)
        LD      D,A
        LD      E,33
        POP     AF
        LD      B,4
        LD      C,1*4
        JP      NUMAT

; NTHITEM: A = the list's A-th item (SHMASK's bits in order).
NTHITEM: LD     B,A
        LD      C,0
NTH1:   LD      A,C
        CALL    MBIT
        AND     (HL)
        JR      Z,NTH2
        LD      A,B
        OR      A
        LD      A,C
        RET     Z
        DEC     B
NTH2:   INC     C
        LD      A,C
        CP      N_ITEMS
        JR      C,NTH1
        XOR     A
        RET

; MBIT: HL = SHMASK's byte of item A, A = its bit. Preserves BC, DE.
MBIT:   PUSH    BC
        LD      C,A
        RRCA
        RRCA
        RRCA
        AND     1FH
        LD      HL,SHMASK
        ADD     A,L
        LD      L,A
        JR      NC,MB1
        INC     H
MB1:    LD      A,C
        AND     7
        LD      B,A
        INC     B
        LD      A,80H
MB2:    RLCA
        DJNZ    MB2
        POP     BC
        RET

; SFRAME: a frame of the shop: up and down choose (the list scrolls), FIRE
; buys one bundle (if the cash is there), a LEFT push sells one, = is done.
; Not ours (watching, or done): nothing. The results: nothing.
SFRAME: LD      A,(SHOWN)
        CP      S_SHOP
        RET     NZ
        LD      A,(MYSEAT)
        CP      SEATS
        RET     NC
        CALL    SEATP
        LD      DE,P_FLAGS
        ADD     HL,DE
        LD      A,(HL)
        AND     PF_DONE
        RET     NZ
        LD      A,(SHN)
        OR      A
        RET     Z
        LD      A,(PRVDIR)      ; LEFT pushed (not held: no selling by holding)
        CPL
        LD      HL,INDIR
        AND     (HL)
        LD      (T3),A
        CALL    REP
        LD      (T0),A
        LD      A,(SHSEL)
        LD      C,A
        LD      A,(T0)
        AND     D_UP
        JR      Z,SF1
        LD      A,C
        OR      A
        JR      Z,SF1
        DEC     A
        LD      (SHSEL),A
SF1:    LD      A,(T0)
        AND     D_DOWN
        JR      Z,SF2
        LD      A,(SHSEL)
        INC     A
        LD      HL,SHN
        CP      (HL)
        JR      NC,SF2
        LD      (SHSEL),A
SF2:    LD      A,(SHSEL)       ; moved: scrolled, or its two rows
        CP      C
        JR      Z,SF5
        PUSH    BC
        SND     UI_TICK
        POP     BC
        LD      A,(SHSEL)
        LD      HL,SHTOP
        CP      (HL)
        JR      C,SF3S
        SUB     (HL)
        CP      SHROWS
        JR      C,SF4
        LD      A,(SHSEL)
        SUB     SHROWS-1
        LD      (SHTOP),A
        JR      SF3T
SF3S:   LD      (SHTOP),A
SF3T:   CALL    SHALL
        JR      SF5
SF4:    LD      A,C
        LD      HL,SHTOP
        SUB     (HL)
        CALL    SHLINE
        LD      A,(SHSEL)
        LD      HL,SHTOP
        SUB     (HL)
        CALL    SHLINE
SF5:    LD      A,(SHSEL)       ; the item chosen: AARGS = it, 1
        CALL    NTHITEM
        LD      (AARGS),A
        LD      A,1
        LD      (AARGS+1),A
        LD      A,(EVBTN)       ; FIRE: buy, if the cash is there
        OR      A
        JR      Z,SF7
        LD      A,(AARGS)
        FAR     PG_CAT,CIINFO   ; C DE: its price (24 bits)
        LD      HL,(ST+OFF_CASH)        ; the cash (32) >= the price?
        OR      A
        SBC     HL,DE
        LD      HL,(ST+OFF_CASH+2)
        LD      B,0
        SBC     HL,BC
        JR      NC,SF6
        SND     UI_ERROR
        TXT     SHPOOR,11*TXH,1,1*4
        LD      A,1
        LD      (SHMSG),A
        JR      SF7
SF6:    XOR     A
        LD      (SHMSG),A
        LD      B,ACT_BUY
        LD      C,2
        CALL    ACTQ
        SND     UI_SELECT
SF7:    LD      A,(T3)          ; LEFT: sell one, if we have it
        AND     D_LEFT
        JR      Z,SF8
        LD      A,(AARGS)
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_INV
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        JR      Z,SF8
        LD      B,ACT_SELL
        LD      C,2
        CALL    ACTQ
        SND     UI_TICK
SF8:    LD      A,(EVKEY)       ; =: done
        CP      K_ENT
        RET     NZ
        LD      B,ACT_DONE
        LD      C,0
        CALL    ACTQ
        SND     UI_GO
        RET

; ---- a round's end, the game's ---------------------------------------------------------
RENTER: LD      A,(SHOWN)
        CP      S_OVER
        LD      A,MUS_OVER
        JR      Z,RE1
        LD      A,MUS_WIN
RE1:    CALL    SNDQUE
        LD      HL,B_SEATS
        CALL    TEXTSCR
        LD      A,(SHOWN)
        CP      S_OVER
        LD      HL,SROUND
        JR      NZ,RE2
        LD      HL,SGAME
RE2:    LD      DE,0*256+1
        LD      C,3*4
        CALL    TXTAT
        LD      A,(ST+OFF_RESULT)       ; who won
        CP      SEATS
        JR      NC,RE3
        CALL    SNAME
        LD      HL,WORK
        LD      DE,0*256+13
        LD      C,2*4
        CALL    TXTAT
        LD      A,(SHOWN)
        CP      S_OVER
        LD      HL,SWROUND
        JR      NZ,RE2A
        LD      HL,SWGAME
RE2A:   LD      DE,0*256+22
        LD      C,3*4
        CALL    TXTAT
RE3:    XOR     A               ; the seats, as in the waiting room
RE4:    PUSH    AF
        LD      B,A
        INC     B
        FAR     PG_ROOM,WSEAT
        POP     AF
        INC     A
        CP      SEATS
        JR      C,RE4
        RET

SHTITLE: DB     "SHOP  ROUND",0
SHCASH: DB      "CASH $",0
SHHEAD: DB      "ITEM              PRICE  BUNDLE OWN",0
SHHINT: DB      "FIRE BUYS  LEFT SELLS  =: DONE",0
SHDONE: DB      "DONE: WAITING FOR THE OTHERS",0
SHPOOR: DB      "NOT ENOUGH CASH               ",0
SHBLANK: DB      0
SROUND: DB      "ROUND OVER:",0
SGAME:  DB      "GAME OVER:",0
SWROUND: DB     "WINS THE ROUND",0
SWGAME: DB      "WINS THE GAME!",0
