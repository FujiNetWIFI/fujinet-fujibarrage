; p_room.inc -- the waiting room (src/room.c): who's here, the host starts;
; the settings (the divide key), the host changes them, everyone can look.
; A text screen (B_SEATS): row 0 the heading, rows 1-10 the seats (each
; row's band gives colour 1 the seat's colour: a strip at the left), rows
; 11-12 what next. WSEAT draws a seat's row for the results too (p_shop).

        JP      WENTER          ; E_ENTER
        JP      WSTATE          ; E_STATE
        JP      WFRAME          ; E_TICK

; the room's own (GAMEV: the field's, never at the same time)
WSET    EQU     GAMEV           ; the settings shown (else the seats)
WSEL    EQU     GAMEV+1         ;  the one chosen (in WKS)
WPAGE   EQU     GAMEV+2         ;  their page
WTOP    EQU     GAMEV+3         ;  the first shown
WN      EQU     GAMEV+4         ;  how many on the page
WKS     EQU     GAMEV+5         ;  the page's settings (16 at most)
WROWS   EQU     10              ; settings shown at once (rows 1-10)

WENTER: XOR     A
        LD      (WSET),A
        LD      (WPAGE),A
WDRAW:  XOR     A
        LD      (WSEL),A
        LD      (WTOP),A
        LD      HL,B_SEATS
        LD      A,(WSET)
        OR      A
        JR      Z,WD0
        LD      HL,B_SETS       ; (the settings: labels yellow, values white)
WD0:    CALL    TEXTSCR
        LD      A,(WSET)
        OR      A
        JR      NZ,WD2
        TXT     SWROOM,0,1,3*4  ; the seats: "WAITING ROOM: id"
        LD      HL,SRVTBL
        LD      DE,0*256+16
        LD      C,2*4
        CALL    TXTAT
        JR      WD5
WD2:    TXT     SSETS,0,1,3*4   ; the settings: their page's
        LD      A,(WPAGE)
        FAR     PG_CAT,CPNAME
        LD      HL,WORK
        LD      DE,0*256+11
        LD      C,2*4
        CALL    TXTAT
        XOR     A               ; the page's settings
        LD      (WN),A
        LD      B,0
WD3:    PUSH    BC
        LD      A,B
        FAR     PG_CAT,CSINFO
        LD      HL,WPAGE
        CP      (HL)
        POP     BC
        JR      NZ,WD4
        LD      A,(WN)
        CP      16
        JR      NC,WD4
        LD      E,A
        LD      D,0
        LD      HL,WKS
        ADD     HL,DE
        LD      (HL),B
        INC     A
        LD      (WN),A
WD4:    INC     B
        LD      A,B
        CP      CNSETS
        JR      C,WD3
WD5:    CALL    ISHOST          ; the hint
        LD      HL,SWAIT
        JR      NZ,WD6
        LD      HL,SHSEAT
        LD      A,(ST+OFF_ROOM_FLAGS)   ; (a room for people only: FIRE needs two)
        AND     ROOM_PEOPLE
        JR      Z,WD6
        LD      HL,SHPPL
WD6:    LD      A,(WSET)
        OR      A
        JR      Z,WD7
        LD      HL,SHSET
        CALL    ISHOST
        JR      Z,WD7
        LD      HL,SWSET
WD7:    LD      DE,11*TXH*256+1
        LD      C,3*4
        CALL    TXTAT
        FAR     PG_CAT,CCHECK   ; a server whose catalog isn't ours
        OR      A
        JR      NZ,WSTATE
        TXT     SWUPD,12*TXH,1,1*4
; WSTATE: a new state: the seats' rows again, or the settings'.
WSTATE: LD      A,(WSET)
        OR      A
        JR      NZ,WST2
        XOR     A
WST1:   PUSH    AF
        LD      B,A
        INC     B
        CALL    WSEAT
        POP     AF
        INC     A
        CP      SEATS
        JR      C,WST1
        RET
WST2:   XOR     A
WST3:   PUSH    AF
        CALL    WSLINE
        POP     AF
        INC     A
        CP      WROWS
        JR      C,WST3
        RET

; ISHOST: Z if we are the host.
ISHOST: LD      A,(ST+OFF_HOST)
        LD      HL,MYSEAT
        CP      (HL)
        RET

; WSEAT: seat A's row, at row B (room.c seat_line): its strip, its name, a
; bot's tier or a person's link, HOST, a game's wins. Clobbers all but
; IX, IY.
WSEAT:  LD      (T0),A
        LD      A,B             ; T1: its line
        ADD     A,A
        LD      D,A
        ADD     A,A
        ADD     A,D
        LD      (T1),A
        LD      A,(T0)          ; (each field written whole over the last: no
        CALL    SEATP           ;  clearing, no flicker)
        LD      DE,P_KIND
        ADD     HL,DE
        LD      A,(HL)
        OR      A
        JR      NZ,WS1
        LD      A,(T1)          ; empty: no strip, "- OPEN -" over the row
        LD      D,A
        LD      E,0
        LD      BC,1*256+TXH
        XOR     A
        CALL    FILLB
        LD      HL,SWOPEN
        LD      A,(T1)
        LD      D,A
        LD      E,1
        LD      C,2*4
        LD      B,BYTEPL-1
        JP      TXTN
WS1:    LD      A,(T1)          ; the strip: colour 1, the seat's
        LD      D,A
        LD      E,0
        LD      BC,1*256+TXH
        LD      A,55H
        CALL    FILLB
        LD      A,(T0)          ; its name: in its seat's colour (1), white
        CALL    SNAME           ; where that is too dark to read (the strip
        LD      HL,SNINK        ; beside it is the colour there; TODO
        LD      A,(T0)          ; 2026-11-23)
        LD      C,A
        LD      B,0
        ADD     HL,BC
        LD      C,(HL)
        LD      HL,WORK
        LD      A,(T1)
        LD      D,A
        LD      E,1
        LD      B,10
        CALL    TXTN
        LD      A,(T0)
        CALL    SEATP
        PUSH    HL
        POP     IX
        LD      A,(IX+P_KIND)
        CP      KIND_BOT
        JR      NZ,WS3
        LD      HL,SBOT         ; a bot: its tier
        LD      A,(T1)
        LD      D,A
        LD      E,11
        LD      C,2*4
        LD      B,4
        CALL    TXTN
        LD      A,(IX+P_TIER)
        FAR     PG_CAT,CTNAME
        LD      HL,WORK
        LD      A,(T1)
        LD      D,A
        LD      E,15
        LD      C,2*4
        LD      B,15
        CALL    TXTN
        JR      WS4
WS3:    LD      A,(IX+P_FLAGS)  ; a person: the link's down?
        AND     PF_CONN
        LD      HL,SLINKD
        JR      Z,WS3A
        LD      HL,SWBLANK
WS3A:   LD      A,(T1)
        LD      D,A
        LD      E,11
        LD      C,2*4
        LD      B,19
        CALL    TXTN
WS4:    LD      A,(ST+OFF_PHASE)        ; a game on: its wins
        CP      PH_WAIT
        JR      Z,WS5
        LD      HL,SWINS
        LD      A,(T1)
        LD      D,A
        LD      E,30
        LD      C,3*4
        LD      B,5
        CALL    TXTN
        LD      A,(T1)
        LD      D,A
        LD      E,35
        LD      B,3
        LD      C,3*4
        LD      A,(IX+P_WINS)
        JP      NUMAT
WS5:    LD      A,(ST+OFF_HOST)         ; waiting: the host
        LD      HL,T0
        CP      (HL)
        LD      HL,SHOSTW
        JR      Z,WS6
        LD      HL,SWBLANK
WS6:    LD      A,(T1)
        LD      D,A
        LD      E,30
        LD      C,3*4
        LD      B,9
        JP      TXTN

; WSLINE: the settings' row A (1 + A on the screen): the one at WTOP + A,
; its label and value, a cursor if chosen. Clobbers all but IX, IY.
WSLINE: LD      (T0),A
        INC     A               ; T1: its line
        ADD     A,A
        LD      D,A
        ADD     A,A
        ADD     A,D
        LD      (T1),A
        LD      D,A
        LD      E,0
        LD      BC,BYTEPL*256+TXH
        XOR     A
        CALL    FILLB
        LD      A,(WTOP)
        LD      HL,T0
        ADD     A,(HL)
        LD      (T2),A          ; T2: its index in WKS
        LD      HL,WN
        CP      (HL)
        RET     NC              ; (none)
        LD      HL,WSEL         ; the cursor
        CP      (HL)
        JR      NZ,WSL1
        LD      A,(T1)
        LD      D,A
        LD      E,0
        LD      A,'>'
        LD      C,1*4
        CALL    CHRAT
WSL1:   LD      A,(T2)          ; its key
        LD      E,A
        LD      D,0
        LD      HL,WKS
        ADD     HL,DE
        LD      A,(HL)
        LD      (T3),A
        FAR     PG_CAT,CSLABEL
        LD      HL,WORK
        LD      A,(T1)
        LD      D,A
        LD      E,1
        LD      C,3*4
        LD      B,24
        CALL    TXTN
        LD      A,(T3)          ; its value
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_SETTINGS
        ADD     HL,DE
        LD      B,(HL)
        LD      A,(T3)
        FAR     PG_CAT,CSVAL
        LD      HL,WORK
        LD      A,(T1)
        LD      D,A
        LD      E,26
        LD      C,2*4
        LD      B,14
        JP      TXTN

; WFRAME: a frame of the room: the divide key the settings and back; on
; them, + the next page, up and down choose, the host's left and right
; change; FIRE (the host) starts.
WFRAME: LD      A,(EVKEY)
        CP      K_DIV
        JR      NZ,WF1
        LD      A,(WSET)
        XOR     1
        LD      (WSET),A
        SND     UI_TICK
        JP      WDRAW
WF1:    LD      A,(WSET)
        OR      A
        JP      Z,WF9
        LD      A,(EVKEY)
        CP      K_PLUS
        JR      NZ,WF2
        LD      A,(WPAGE)
        INC     A
        CP      CNPAGES
        JR      C,WF1A
        XOR     A
WF1A:   LD      (WPAGE),A
        SND     UI_TICK
        JP      WDRAW
WF2:    CALL    REP
        LD      (T0),A
        LD      A,(WSEL)
        LD      C,A             ; C: the old choice
        LD      A,(T0)
        AND     D_UP
        JR      Z,WF3
        LD      A,C
        OR      A
        JR      Z,WF3
        DEC     A
        LD      (WSEL),A
WF3:    LD      A,(T0)
        AND     D_DOWN
        JR      Z,WF4
        LD      A,(WSEL)
        INC     A
        LD      HL,WN
        CP      (HL)
        JR      NC,WF4
        LD      (WSEL),A
WF4:    LD      A,(WSEL)        ; moved: scrolled, or its two rows
        CP      C
        JR      Z,WF6
        PUSH    BC
        SND     UI_TICK
        POP     BC
        LD      A,(WSEL)
        LD      HL,WTOP
        CP      (HL)
        JR      C,WF5S          ; above the top
        SUB     (HL)
        CP      WROWS
        JR      C,WF5           ; still shown
        LD      A,(WSEL)
        SUB     WROWS-1
        LD      (WTOP),A
        JR      WF5T
WF5S:   LD      (WTOP),A
WF5T:   CALL    WSTATE
        JR      WF6
WF5:    LD      A,C             ; the old row, the new
        LD      HL,WTOP
        SUB     (HL)
        CALL    WSLINE
        LD      A,(WSEL)
        LD      HL,WTOP
        SUB     (HL)
        CALL    WSLINE
WF6:    CALL    ISHOST          ; the host: left and right change it
        JR      NZ,WF9
        LD      A,(T0)
        AND     D_LEFT|D_RIGHT
        JR      Z,WF9
        LD      A,(WN)
        OR      A
        JR      Z,WF9
        LD      A,(WSEL)        ; its key, its value, its count
        LD      E,A
        LD      D,0
        LD      HL,WKS
        ADD     HL,DE
        LD      A,(HL)
        LD      (AARGS),A
        FAR     PG_CAT,CSINFO
        LD      A,(AARGS)
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_SETTINGS
        ADD     HL,DE
        LD      A,(HL)
        LD      B,A             ; B the value, C the count
        LD      A,(T0)
        AND     D_LEFT
        JR      Z,WF7
        LD      A,B             ; one less, round
        OR      A
        JR      NZ,WF6A
        LD      A,C
WF6A:   DEC     A
        JR      WF8
WF7:    LD      A,B             ; one more, round
        INC     A
        CP      C
        JR      C,WF8
        XOR     A
WF8:    LD      (AARGS+1),A
        LD      B,ACT_SET
        LD      C,2
        CALL    ACTQ
        SND     UI_TICK
WF9:    LD      A,(EVBTN)       ; FIRE: the host starts
        OR      A
        RET     Z
        CALL    ISHOST
        RET     NZ
        LD      B,ACT_START
        LD      C,0
        CALL    ACTQ
        SND     UI_GO
        RET

SWROOM: DB      "WAITING ROOM:",0
SSETS:  DB      "SETTINGS:",0
; a seat's name's ink: 1 (its colour: B_SEATS's R1) but where that is dark
; (luminance 2-3: seats 0, 4, 8), 3 (white)
SNINK:  DB      3*4,1*4,1*4,1*4,3*4,1*4,1*4,1*4,3*4,1*4
SWOPEN: DB      " - OPEN -",0
SWBLANK: DB     0
SBOT:   DB      "BOT",0
SLINKD: DB      "(LINK DOWN)",0
SWINS:  DB      "WINS",0
SHOSTW: DB      "HOST",0
SHSEAT: DB      "FIRE STARTS (BOTS FILL)    /: SETTINGS",0
SHPPL:  DB      "FIRE STARTS (2+ PEOPLE)    /: SETTINGS",0
SWAIT:  DB      "WAITING FOR THE HOST    /: SETTINGS",0
SHSET:  DB      "/: SEATS  +: PAGE  <>: CHANGE",0
SWSET:  DB      "/: SEATS  +: PAGE",0
SWUPD: DB     "THE SERVER IS NEWER: UPDATE FUJIBARRAGE",0
