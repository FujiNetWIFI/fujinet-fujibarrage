; main.inc -- src/main.c: the start, the rooms, a room (fixed A). The
; Astrocade's (src/astrocade/src/main.inc) in FujiBlast's 2650 form.
;
; The lobby's appkeys say who we are and where to play; else we ask, and
; list the rooms. In a room each frame runs the link's step (p_net), then
; the screen the state's phase wants: the waiting room (p_room); the shop,
; the field, a round's end and the game's (M1: p_field's stub). A segment
; whole (CAP_HOLD: the server sends nothing more until our D) is played
; with nothing out on the link, then the D is owed (p_net E_DONE).

; ---- the start ----------------------------------------------------------------
MAININIT:
        GOPAGE  PG_BOOT,E_ENTER         ; the rigs' test hook, the appkeys
        LODA,R0 TESTHK          ; a test scene asked for: 3 the fixture's shot
        COMI,R0 $A5
        BCFR,EQ MI1
        LODA,R0 TESTHK+2
        COMI,R0 3
        BCTA,EQ TSHOT
        BCTA,GT TSCRN           ; (4 on: the screens', fixed B)
MI1:
        GOPAGE  PG_MENU,E_ENTER         ; the title, and a name if none
        COMI,R0 0
        BCTA,EQ TOP
        GOPAGE  PG_BOOT,E_SAVENM        ; (a new one: kept)

; ---- the rooms, when no room is known (menus.c room_list) -----------------------
TOP:    LODA,R0 SRVTBL
        BCFA,EQ ROOM
        GOPAGE  PG_BOOT,E_CONN          ; "CONNECTING", the socket: R0 = 1 open
        COMI,R0 0
        BCTA,EQ TOPNA
        GOPAGE  PG_NET,E_INIT
        GOPAGE  PG_MENU,E_STATE         ; the list asked for
TOPL:   BSTA,UN FRAME
        GOPAGE  PG_NET,E_ENTER          ; (the link's step)
        LODA,R0 NEVT
        TMI,R0  EV_LOST
        BCTA,EQ TOPNA
        GOPAGE  PG_MENU,E_TICK          ; R0: 0 on, 1 picked, 2 no list
        COMI,R0 0
        BCTR,EQ TOPL
        COMI,R0 1
        BCTR,EQ ROOM
TOPNA:  LODI,R2 M_NOANS         ; (no room to forget)
        BCTA,UN TFAIL

; ---- in a room (main.c play_room) ------------------------------------------------
ROOM:   GOPAGE  PG_BOOT,E_JOIN          ; the room kept, "JOINING", the socket
        COMI,R0 0
        BCTA,EQ TNOANS
        GOPAGE  PG_NET,E_HELLO          ; the link's counters, the state zeroed, HELLO
        EORZ    R0
        CPUT    SHOWN
MAINLP: BSTA,UN FRAME
        LODA,R0 SHOWN           ; the field: the sprites, in the vblank
        COMI,R0 S_FIELD
        BCFR,EQ ML0
        GOPAGE  PG_PLAY,E_VBL
ML0:    GOPAGE  PG_NET,E_ENTER          ; the link's step: NEVT
        LODA,R0 CHROW           ; the display seen (a late pass: VBSYNC)
        ANDI,R0 $0F
        COMI,R0 13
        BCTR,EQ ML00
        STRA,R0 VSEEN
ML00:   LODA,R0 NEVT
        TMI,R0  EV_ERROR
        BCTA,EQ TERR
        TMI,R0  EV_LOST
        BCTA,EQ TLOST
        TMI,R0  EV_NOANS        ; (p_net: no state in JOIN_WAIT frames)
        BCTA,EQ TNOANS
        LODA,R0 EVKEY           ; Clear: leave (on the field with the items
        COMI,R0 K_CLR           ;  line open: it shuts that, p_hud.inc)
        BCFR,EQ ML01
        LODA,R0 SHOWN
        COMI,R0 S_FIELD
        BCFA,EQ TLEAVE
        LODA,R0 FLAGS           ; (IMENU is the shot's records' during one)
        TMI,R0  FL_INSCR
        BCTA,EQ TLEAVE
        LODA,R0 *PIMENU
        BCTA,EQ TLEAVE
ML01:
        LODA,R0 NLINK           ; no state yet: nothing to show
        TMI,R0  NL_HEARD
        BCFA,EQ MAINLP
        LODA,R0 NEVT            ; a segment whole: on the field, played,
        TMI,R0  EV_SCRIPT       ;  then our D (a shot's states are the seats
        BCFR,EQ ML2             ;  before it: nothing else is picked meanwhile)
        LODA,R0 SHOWN
        COMI,R0 S_FIELD
        BCTR,EQ ML1A
        LODI,R0 S_FIELD
        BSTA,UN ENTERS
ML1A:   GOPAGE  PG_PLY,E_ENTER          ; (p_ply.inc: a page call less deep
        GOPAGE  PG_NET,E_DONE           ;  than from the field's page)
        BCTA,UN MAINLP
ML2:    LODA,R0 NEVT            ; a screen is picked only at a commit (ST is
        TMI,R0  EV_STATE        ;  patched as frames come: half a state between)
        BCFR,EQ ML7
        LODA,R2 ST+OFF_PHASE    ; the screen it wants
        COMI,R2 PH_GAMEOVER+1
        BCTR,LT ML4
        LODI,R2 PH_WAIT
ML4:    LODA,R0 SCRFOR,R2
        COMA,R0 SHOWN
        BCFR,EQ ML5
        COMI,R0 S_FIELD         ; the field again: a new round's
        BCFR,EQ ML6
        LODA,R1 ST+OFF_SEQ
        COMA,R1 SHSEQ
        BCTR,EQ ML6
ML5:    BSTR,UN ENTERS          ; another screen: drawn afresh
        BCTR,UN ML7
ML6:    LODA,R2 SHOWN           ; the same one: the new state on it
        LODA,R0 SCRPG,R2
        STRZ    R1
        LODI,R3 E_STATE
        BSTA,UN PGCALL
ML7:    LODA,R2 SHOWN           ; and its frame
        LODA,R0 SCRPG,R2
        STRZ    R1
        LODI,R3 E_TICK
        BSTA,UN PGCALL
        BCTA,UN MAINLP

; ENTERS: screen R0 shown afresh (SHOWN, SHSEQ), its page's E_ENTER.
; Calls three down. Clobbers R0-R3.
ENTERS: CPUT    SHOWN
        LODA,R0 ST+OFF_SEQ
        CPUT    SHSEQ
        LODA,R2 SHOWN
        LODA,R0 SCRPG,R2
        STRZ    R1
        LODI,R3 E_ENTER
        BCTA,UN PGCALL          ; tail call

; by phase: the screen; by screen: the page that draws it and runs it
SCRFOR: DB      S_WAIT,S_SHOP,S_FIELD,S_FIELD,S_FIELD,S_RESULTS,S_OVER
SCRPG:  DB      PG_BOOT,PG_ROOM,PG_SHOP,PG_PLAY,PG_SHOP,PG_SHOP
                                ; (S_NONE: the boot's E_STATE, E_TICK: returns)

; the server said no (the room forgotten)
TERR:   LODA,R0 NERR
        LODI,R2 M_VERSION|$80
        COMI,R0 ERR_VERSION
        BCTA,EQ TFAIL
        LODI,R2 M_CAPS|$80
        COMI,R0 ERR_CAPS
        BCTA,EQ TFAIL
        LODI,R2 M_FULL|$80
        COMI,R0 ERR_FULL
        BCTA,EQ TFAIL
        LODI,R2 M_NOROOM|$80
        BCTA,UN TFAIL
; the link went: say so, then join again (the room kept)
TLOST:  LODI,R2 M_LOST
        BCTA,UN TFAIL
; Clear: give the seat up, BYE (a few frames to get it out), close. (On a
; text screen first: RCOUNT is TZ's.)
TLEAVE: ZBSR    *ZTEXT
        LODA,R0 NLINK
        TMI,R0  NL_JOINED
        BCFR,EQ TLV1
        LODI,R1 ACT_LEAVE
        LODI,R2 0
        BSTA,UN ACTQ
TLV1:   LODA,R0 TXF
        IORI,R0 TX_BYE
        STRA,R0 TXF
        LODI,R0 30
        STRA,R0 RCOUNT
TLV2:   BSTA,UN FRAME
        GOPAGE  PG_NET,E_ENTER
        LODA,R0 TXF
        BCFR,EQ TLV3
        LODA,R0 NKIND
        BCTR,EQ TLV4
TLV3:   LODA,R0 RCOUNT
        SUBI,R0 1
        STRA,R0 RCOUNT
        BCFR,EQ TLV2
TLV4:   LODI,R2 M_QUIET|$80
        BCTR,UN TFAIL
; no answer: give the room up
TNOANS: LODI,R2 M_NOANS|$80
; TFAIL: the socket closed, message R2 (b7: the room forgotten too), then
; the rooms again.
TFAIL:  GOPAGE  PG_BOOT,E_FAIL
        BCTA,UN TOP
