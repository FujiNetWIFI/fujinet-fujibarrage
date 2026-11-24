; p_room.inc -- the waiting room (src/room.c): who's here, the host starts;
; the settings (Select or keypad 1), the host changes them (the disc's left
; and right: ACT_SET), everyone can look. A 13-row text screen:
;   seats     row 0 "ROOM id"; rows 1-10 the seats: '>' ours, the name in
;             its seat's colour, then HOST, a bot's tier (6 cells: the
;             catalog's, cut), LOST (a person's link down); row 11 the
;             settings' hint (a newer server's catalog: UPDATE BARRAGE);
;             row 12 what next (FIRE starts: the host)
;   settings  row 0 "SETTINGS:" and the page's name; rows 1-10 five of the
;             page's settings, two rows each (the label, cyan; its value
;             right-aligned, white); the disc's up and down choose (the
;             list scrolls), Enter the next page; rows 11-12 the hints
; The settings are drawn by their own page (p_sets.inc: the catalog's
; names and numbers fill it), shown only when the server's catalog is the
; build's (CATOURS). Every field is written whole over the last: no
; flicker. The state's in TZ (ram.inc: W*), the shared helpers txt.inc's.

        BCTA,UN WENTER          ; E_ENTER
        BCTA,UN WSTATE          ; E_STATE
        BCTA,UN WFRAME          ; E_TICK

WROWS   EQU     5               ; settings shown at once (two rows each)

WENTER: EORZ    R0
        STRA,R0 WSET
        STRA,R0 WPAGE
        STRA,R0 REPN
; WDRAW: the screen afresh: the seats or the settings (the first chosen).
WDRAW:  EORZ    R0
        STRA,R0 WSEL
        STRA,R0 WTOP
        ZBSR    *ZTEXT
        LODA,R0 WSET
        BCFA,EQ WD2
        DAT     0,0             ; the seats: "ROOM id"
        DCOL    LABI
        TXTAT   SROOM
        DCOL    TXTI
        LODI,R3 $FF
WE1:    LODA,R0 SRVTBL,R3,+
        BCTR,EQ WE2
        ZBSR    *ZA2G
        ZBSR    *ZDPUTC
        COMI,R3 ROOM_LEN-1
        BCFR,EQ WE1
WE2:    DAT     11,0            ; the settings' hint, or a newer server's
        BSTA,UN CATOURS
        BCTR,EQ WE3
        DCOL    WARNI
        TXTAT   SUPD
        BCTA,UN WSTATE
WE3:    DCOL    LABI
        TXTAT   SSETH
        BCTA,UN WSTATE
WD2:    FARPG   PG_SETS,E_SDRAW ; the settings: their own page's
        RETC,UN
; WSTATE: a new state: the seats' rows and what next, or the settings'.
WSTATE: LODA,R0 WSET
        BCFA,EQ WST2
        EORZ    R0
WST1:   STRA,R0 WI
        BSTA,UN WSEAT
        LODA,R0 WI
        ADDI,R0 1
        COMI,R0 SEATS
        BCFR,EQ WST1
        DAT     12,0            ; what next
        DCOL    TXTI
        LODI,R0 SWATCH>>8
        LODI,R1 SWATCH&$FF
        LODA,R2 MYSEAT
        COMI,R2 NONE
        BCTR,EQ WS12
        LODI,R0 SWAITH>>8
        LODI,R1 SWAITH&$FF
        COMA,R2 ST+OFF_HOST
        BCFR,EQ WS12
        DCOL    USI
        LODI,R0 SSTART>>8
        LODI,R1 SSTART&$FF
        LODA,R2 ST+OFF_ROOM_FLAGS       ; (a room for people only: FIRE needs two)
        ANDI,R2 ROOM_PEOPLE
        BCTR,EQ WS12
        LODI,R0 SPPL>>8
        LODI,R1 SPPL&$FF
WS12:   LODI,R2 $FF
        LODI,R3 15
        BCTA,UN TXTPAD          ; tail call
WST2:   FARPG   PG_SETS,E_SALL
        RETC,UN

; WSEAT: seat WI's row (row WI + 1). Calls three down. Clobbers R0-R3, T0-T3.
WSEAT:  LODA,R1 WI
        ADDI,R1 1
        LODI,R2 $FF
        ZBSR    *ZDPOS
        LODA,R3 WI
        LODA,R0 SEAT25,R3
        STRA,R0 T2              ; T2: its offset in the seats
        STRZ    R3
        LODA,R0 ST+OFF_SEATS+P_KIND,R3
        BCFR,EQ WSE1
        DCOL    LABI            ; empty
        TXTAT   SOPEN
        RETC,UN
WSE1:   LODI,R0 GSPACE          ; ours: the cursor
        LODA,R3 WI
        COMA,R3 MYSEAT
        BCFR,EQ WSE2
        LODI,R0 GCURSOR|USI
WSE2:   ZBSR    *ZDGLY
        LODA,R3 WI              ; the name, in its colour
        BSTA,UN SNAMEI
        ZBSR    *ZNAMEAT
        EORZ    R0
        ZBSR    *ZDGLY
        LODA,R0 ST+OFF_HOST     ; then HOST, the tier, LOST, or nothing
        COMA,R0 WI
        BCFR,EQ WSE3
        DCOL    WARNI
        TXTAT   SHOST
        RETC,UN
WSE3:   LODA,R3 T2
        LODA,R0 ST+OFF_SEATS+P_KIND,R3
        COMI,R0 KIND_BOT
        BCFR,EQ WSE5
        LODA,R0 ST+OFF_SEATS+P_TIER,R3  ; CTN6 + 6 x the tier
        COMI,R0 CNTIERS
        BCTR,LT WSE4
        EORZ    R0
WSE4:   ADDZ    R0
        STRZ    R1
        ADDZ    R0
        ADDZ    R1
        STRZ    R3
        DCOL    TXTI
        LODI,R1 6
WSE4A:  LODA,R0 CTN6,R3
        ZBSR    *ZDPUTC
        ADDI,R3 1
        BDRR,R1 WSE4A
        RETC,UN
WSE5:   LODA,R0 ST+OFF_SEATS+P_FLAGS,R3
        TMI,R0  PF_CONN
        BCTR,EQ WSE6
        DCOL    WARNI
        TXTAT   SLOST
        RETC,UN
WSE6:   TXTAT   SBLANK6
        RETC,UN

; ISHOST: CC EQ if we hold a seat and are the host. Leaf. Clobbers R0.
ISHOST: LODA,R0 MYSEAT
        COMI,R0 SEATS
        BCTR,LT ISH1
        RETC,UN                 ; (not EQ)
ISH1:   COMA,R0 ST+OFF_HOST
        RETC,UN

; CATOURS: CC EQ if the state's catalog (OFF_CAT_LEN, OFF_CAT_SUM) is the
; one in ROM (catequ.inc's CATLEN, CATS1, CATS2), else a server whose
; names and prices aren't these (UPDATE). Leaf. Keeps R0-R2.
CATOURS: LODA,R3 ST+OFF_CAT_LEN
        COMI,R3 CATLEN&$FF
        RETC,LT
        RETC,GT
        LODA,R3 ST+OFF_CAT_LEN+1
        COMI,R3 CATLEN>>8
        RETC,LT
        RETC,GT
        LODA,R3 ST+OFF_CAT_SUM
        COMI,R3 CATS1
        RETC,LT
        RETC,GT
        LODA,R3 ST+OFF_CAT_SUM+1
        COMI,R3 CATS2
        RETC,UN

; WFRAME: a frame of the room: Select (or keypad 1) the settings and back
; (with our catalog); on them, Enter the next page, the disc's up and
; down choose, the host's left and right change it; FIRE (the host) starts.
WFRAME: LODA,R0 EVKEY
        COMI,R0 K_SEL
        BCTR,EQ WF0
        COMI,R0 K_ITEMS
        BCFR,EQ WF1
WF0:    BSTA,UN CATOURS
        BCFR,EQ WF1
        LODA,R0 WSET
        EORI,R0 1
        STRA,R0 WSET
        SND     UI_TICK
        BCTA,UN WDRAW
WF1:    LODA,R0 WSET
        BCTA,EQ WF9
        LODA,R0 EVKEY           ; Enter: the next page
        COMI,R0 K_ENT
        BCFR,EQ WF2
        LODA,R0 WPAGE
        ADDI,R0 1
        COMI,R0 CNPAGES
        BCTR,LT WF1A
        EORZ    R0
WF1A:   STRA,R0 WPAGE
        SND     UI_TICK
        BCTA,UN WDRAW
WF2:    BSTA,UN REP             ; the disc
        STRA,R0 WMV
        LODA,R0 WSEL
        STRA,R0 WOLD
        LODA,R0 WMV
        TMI,R0  D_UP
        BCFR,EQ WF3
        LODA,R0 WSEL
        BCTR,EQ WF3
        SUBI,R0 1
        STRA,R0 WSEL
WF3:    LODA,R0 WMV
        TMI,R0  D_DOWN
        BCFR,EQ WF4
        LODA,R0 WSEL
        ADDI,R0 1
        COMA,R0 WN
        BCFR,LT WF4
        STRA,R0 WSEL
WF4:    LODA,R0 WSEL            ; moved: scrolled, or its two slots
        COMA,R0 WOLD
        BCTA,EQ WF6
        SND     UI_TICK
        LODA,R0 WSEL
        COMA,R0 WTOP
        BCTR,LT WF5S            ; above the top
        SUBA,R0 WTOP
        COMI,R0 WROWS
        BCTR,LT WF5             ; still shown
        LODA,R0 WSEL
        SUBI,R0 WROWS-1
WF5S:   STRA,R0 WTOP
        FARPG   PG_SETS,E_SALL
        BCTR,UN WF6
WF5:    LODA,R0 WOLD
        SUBA,R0 WTOP
        STRA,R0 WI
        FARPG   PG_SETS,E_SLINE
        LODA,R0 WSEL
        SUBA,R0 WTOP
        STRA,R0 WI
        FARPG   PG_SETS,E_SLINE
WF6:    BSTA,UN ISHOST          ; the host: left and right change it
        BCFA,EQ WF9
        LODA,R0 WMV
        ANDI,R0 D_LEFT|D_RIGHT
        BCTA,EQ WF9
        LODA,R0 WN
        BCTA,EQ WF9
        FARPG   PG_SETS,E_SCHG  ; T1: its key, T2: its value changed
        LODA,R0 T2
WF8:    STRA,R0 T2
        CPTR    AARGS
        LODA,R0 T1
        BSTA,UN RAMPUT1
        LODA,R0 T2
        BSTA,UN RAMPUT1
        BSTA,UN RAMPEND
        LODI,R1 ACT_SET
        LODI,R2 2
        ZBSR    *ZACTQ
        SND     UI_TICK
WF9:    LODA,R0 EVBTN           ; FIRE: the host starts
        RETC,EQ
        BSTA,UN ISHOST
        RETC,LT
        RETC,GT
        LODI,R1 ACT_START
        LODI,R2 0
        ZBSR    *ZACTQ
        SND     UI_GO
        RETC,UN

        INCLUDE "src/txt.inc"
        INCLUDE "cattier.inc"

SROOM:  DB      "ROOM ",$FF
SUPD:   DB      " UPDATE BARRAGE ",$FF
SSETH:  DB      "SELECT: SETTINGS",$FF
SOPEN:  DB      " - OPEN -       ",$FF
SHOST:  DB      "HOST  ",$FF
SLOST:  DB      "LOST  ",$FF
SBLANK6: DB     "      ",$FF
SWATCH: DB      "WATCHING",$FF
SWAITH: DB      "WAITING FOR HOST",$FF
SSTART: DB      "FIRE: START",$FF
SPPL:   DB      "FIRE: 2+ PEOPLE",$FF
