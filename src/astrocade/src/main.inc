; main.inc -- src/main.c: the start, the rooms, a room. Page 0's (the boot
; page): every other page is reached by PGCALL, which maps page 0 back.
; FujiBlast's (src/astrocade/src/main.inc).
;
; The lobby's appkeys say who we are and where to play; else we ask, and
; list the rooms. In a room each frame runs the link's step (p_net), then
; the screen the state's phase wants: the waiting room (p_room), the shop,
; a round's end and the game's (p_shop), the field (p_play: entered on a
; new state, run each frame, and a segment played when one is whole).

; ---- the start ----------------------------------------------------------------
MAININIT:
        GOPAGE  PG_BOOT,E_ENTER         ; the rigs' test hook, the appkeys
        OR      A
        JR      Z,MI1
        GOPAGE  PG_TEST,E_ENTER         ; an offline scene (it never comes back)
MI1:    GOPAGE  PG_MENU,E_ENTER         ; the title, and a name if none
        OR      A
        JR      Z,TOP
        GOPAGE  PG_BOOT,E_SAVENM        ; (a new one: kept)

; ---- the rooms, when no room is known (menus.c room_list) -----------------------
TOP:    LD      A,(SRVTBL)
        OR      A
        JR      NZ,ROOM
        GOPAGE  PG_BOOT,E_CONN          ; "CONNECTING", the socket: A = 1 open
        OR      A
        JR      Z,TOPNA
        GOPAGE  PG_NET,E_INIT
        GOPAGE  PG_MENU,E_STATE         ; the list asked for
TOPL:   CALL    FRAME
        GOPAGE  PG_NET,E_ENTER          ; (the link's step)
        LD      A,(NEVT)
        AND     EV_LOST
        JR      NZ,TOPNA
        GOPAGE  PG_MENU,E_TICK          ; A: 0 on, 1 picked, 2 no list
        OR      A
        JR      Z,TOPL
        DEC     A
        JR      Z,ROOM
TOPNA:  LD      A,M_NOANS               ; (no room to forget)
        JP      TFAIL

; ---- in a room (main.c play_room) ------------------------------------------------
ROOM:   GOPAGE  PG_BOOT,E_JOIN          ; the room kept, "JOINING", the socket
        OR      A
        JP      Z,TNOANS
        GOPAGE  PG_NET,E_HELLO          ; the link's counters, the state zeroed, HELLO
ROOM2:  XOR     A                       ; (the test scenes come in here)
        LD      (SHOWN),A
MAINLP: CALL    FRAME
        GOPAGE  PG_NET,E_ENTER          ; the link's step: NEVT
        LD      A,(FLAGS)
        AND     FL_TEST
        JR      Z,ML0
        GOPAGE  PG_TEST,E_TICK          ; a scene's own
ML0:    LD      A,(NEVT)
        BIT     1,A                     ; EV_ERROR
        JP      NZ,TERR
        BIT     2,A                     ; EV_LOST
        JP      NZ,TLOST
        BIT     3,A                     ; EV_NOANS (no state in JOIN_WAIT frames)
        JP      NZ,TNOANS
        LD      A,(EVKEY)
        CP      K_CLR
        JR      NZ,ML1
        LD      A,(SHOWN)               ; C: leave (on the field the items
        CP      S_FIELD                 ;  line takes it first: shut, by
        JP      NZ,TLEAVE               ;  p_play's frame)
        LD      A,(IMENU)
        OR      A
        JP      Z,TLEAVE
ML1:    LD      A,(NLINK)               ; no state yet: nothing to show
        AND     NL_HEARD
        JR      Z,MAINLP
        LD      A,(NEVT)                ; a segment whole: on the field, played
        AND     EV_SCRIPT
        JR      Z,ML2
        LD      A,(SHOWN)
        CP      S_FIELD
        JR      Z,ML1A
        LD      A,S_FIELD
        CALL    ENTERS
ML1A:   FAR     PG_OPS,TMOFF            ; (our turn's arrow off the field first)
        LD      A,1                     ; the segment played (the PLY page),
        LD      (INSCR),A               ;  then our D. A shot's states are the
        XOR     A                       ;  seats before it: none is taken until
        LD      (MYTURN),A              ;  its last segment ends (INSCR). Here,
        GOPAGE  PG_PLY,E_ENTER          ;  not in the PLAY page: a page call
        GOPAGE  PG_NET,E_DONE           ;  less on the stack under every shot
        JR      MAINLP
ML2:    LD      A,(NEVT)                ; a screen is picked only at a commit (ST is
        AND     EV_STATE                ;  patched as frames come: half a state between)
        JR      Z,ML7
        LD      A,(ST+OFF_PHASE)        ; the screen it wants
        CP      PH_GAMEOVER+1
        JR      C,ML4
        LD      A,PH_WAIT
ML4:    LD      C,A
        LD      B,0
        LD      HL,SCRFOR
        ADD     HL,BC
        LD      A,(HL)
        LD      HL,SHOWN
        CP      (HL)
        JR      NZ,ML5
        CP      S_FIELD                 ; the field again: a new round's
        JR      NZ,ML6
        LD      A,(ST+OFF_SEQ)
        LD      HL,SHSEQ
        CP      (HL)
        JR      Z,ML6
        LD      A,S_FIELD
ML5:    CALL    ENTERS                  ; another screen: drawn afresh
        JR      ML7
ML6:    LD      HL,E_STATE              ; the same one: the new state on it
        LD      DE,SCRPG
        CALL    SCRCALL
ML7:    LD      HL,E_TICK               ; and its frame
        LD      DE,SCRTK
        CALL    SCRCALL
        JP      MAINLP

; ENTERS: screen A shown afresh (SHOWN, SHSEQ), its page's E_ENTER.
ENTERS: LD      (SHOWN),A
        LD      A,(ST+OFF_SEQ)
        LD      (SHSEQ),A
        LD      HL,E_ENTER
        LD      DE,SCRPG
; SCRCALL: entry HL of the page table DE names for the screen SHOWN.
SCRCALL: LD     A,(SHOWN)
        ADD     A,E
        LD      E,A
        JR      NC,SCC1
        INC     D
SCC1:   LD      A,(DE)
        JP      PGCALL

; by phase: the screen; by screen: the pages that draw it and run it
SCRFOR: DB      S_WAIT,S_SHOP,S_FIELD,S_FIELD,S_FIELD,S_RESULTS,S_OVER
SCRPG:  DB      PG_BOOT,PG_ROOM,PG_SHOP,PG_PLAY,PG_SHOP,PG_SHOP
SCRTK:  DB      PG_BOOT,PG_ROOM,PG_SHOP,PG_PLAY,PG_SHOP,PG_SHOP ; (S_NONE: the
                                ;  boot's E_STATE and E_TICK, a return)

; the server said no
TERR:   LD      A,(NERR)
        LD      B,M_VERSION+80H
        CP      ERR_VERSION
        JR      Z,TER1
        LD      B,M_CAPS+80H
        CP      ERR_CAPS
        JR      Z,TER1
        LD      B,M_NOROOM+80H
TER1:   LD      A,B
        JR      TFAIL
; the link went: say so, then join again (the room kept)
TLOST:  LD      A,M_LOST
        JR      TFAIL
; C: give the seat up, BYE (a few frames to get it out), close
TLEAVE: LD      A,(NLINK)
        AND     NL_JOINED
        JR      Z,TLV1
        LD      B,ACT_LEAVE
        LD      C,0
        CALL    ACTQ
TLV1:   LD      A,(TXF)
        OR      TX_BYE
        LD      (TXF),A
        LD      A,30
        LD      (RCOUNT),A
TLV2:   CALL    FRAME
        GOPAGE  PG_NET,E_ENTER
        LD      A,(TXF)
        OR      A
        JR      NZ,TLV3
        LD      A,(NKIND)
        OR      A
        JR      Z,TLV4
TLV3:   LD      HL,RCOUNT
        DEC     (HL)
        JR      NZ,TLV2
TLV4:   LD      A,M_QUIET+80H
        JR      TFAIL
; no answer: give the room up
TNOANS: LD      A,M_NOANS+80H
; TFAIL: the socket closed, message A (b7: the room forgotten too), then
; the rooms again.
TFAIL:  LD      (MSGID),A
        CALL    SNDOFF
        GOPAGE  PG_BOOT,E_FAIL
        JP      TOP
