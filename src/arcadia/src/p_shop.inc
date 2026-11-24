; p_shop.inc -- the shop, a round's end, the game's (src/room.c): 13-row
; text screens (SHOWN says which).
; The shop (16 columns):
;   row 0   SHOP    ROUND nn         row 1  CASH   nnnnnnnnn
;   rows 2-9  the list: '>' the chosen, its name (names.inc SN9, 9 cells),
;           its price (5): what the arms level allows but the mini missile
;   row 10  OWN nn BUNDLE nn  (the chosen)    row 11  FIRE BUY  < SELL
;   row 12  ENTER: DONE / NOT ENOUGH CASH / DONE: WAITING, the timer (14-15)
; The disc's up and down choose (the list scrolls), FIRE buys a bundle (if
; the cash is there), a LEFT push sells one, Enter is done: the STATE's
; cash and stock answer each (E_STATE). Watching, or done: nothing.
; The results (round or game): row 0 which, rows 1-2 the winner, rows 3-12
; the seats with their wins. The items' prices are the build's catalog
; (catitem.inc), shown only when the server's is the same.

        BCTA,UN SENTER          ; E_ENTER: the shop, or the results (SHOWN)
        BCTA,UN SSTATE          ; E_STATE
        BCTA,UN SFRAME          ; E_TICK

SHROWS  EQU     8               ; the list's rows (2-9)

SENTER: LODA,R0 SHOWN
        COMI,R0 S_SHOP
        BCFA,EQ RENTER
        ZBSR    *ZTEXT
        SND     MUS_SHOP
        EORZ    R0
        STRA,R0 SHSEL
        STRA,R0 SHTOP
        STRA,R0 SHN
        STRA,R0 SHMSG
        STRA,R0 REPN
        BSTA,UN CATOURS         ; the list: what the arms level allows (and
        BCFR,EQ SE3             ;  our catalog's)
        LODI,R1 I_MINI_MISSILE+1
SE1:    LODA,R0 CIARM,R1
        COMA,R0 ST+OFF_SETTINGS+SET_ARMS
        BCTR,GT SE2
        LODA,R3 SHN
        LODZ    R1
        STRA,R0 SHIDS,R3
        ADDI,R3 1
        STRA,R3 SHN
SE2:    ADDI,R1 1
        COMI,R1 N_ITEMS
        BCFR,EQ SE1
SE3:    DCOL    LABI            ; the labels
        DAT     0,0
        TXTAT   SSHOP
        DAT     1,0
        TXTAT   SCASH
        DAT     10,0
        TXTAT   SOWN
        DAT     10,7
        TXTAT   SBUND
        DAT     11,0
        TXTAT   SHINT
        BSTA,UN SHALL
; SSTATE: a new state: the round, the cash, the chosen's stock, row 12.
SSTATE: LODA,R0 SHOWN
        COMI,R0 S_SHOP
        BCFA,EQ RSTATE
        DCOL    TXTI
        DAT     0,14            ; the round to come
        LODA,R0 ST+OFF_ROUND
        ADDI,R0 1
        LODI,R1 2
        ZBSR    *ZDNUM
        LODI,R3 3               ; the cash (32 bits)
SS1:    LODA,R0 ST+OFF_CASH,R3
        STRA,R0 N32,R3
        BDRR,R3 SS1
        LODA,R0 ST+OFF_CASH
        STRA,R0 N32
        DAT     1,4
        LODI,R0 0
        LODI,R1 12
        BSTA,UN NUMX
        BSTA,UN SFOOT
; SROW12: row 12: the message, or what next; the timer.
SROW12: DAT     12,0
        LODA,R0 SHMSG
        BCTR,EQ SR1
        DCOL    WARNI
        LODI,R0 SPOOR>>8
        LODI,R1 SPOOR&$FF
        BCTR,UN SR3
SR1:    LODA,R3 MYSEAT          ; watching, done, or what next
        COMI,R3 SEATS
        BCTR,LT SR2
        DCOL    TXTI
        LODI,R0 SWATCHS>>8
        LODI,R1 SWATCHS&$FF
        BCTR,UN SR3
SR2:    LODA,R0 SEAT25,R3
        STRZ    R3
        LODA,R0 ST+OFF_SEATS+P_FLAGS,R3
        TMI,R0  PF_DONE
        BCFR,EQ SR2A
        DCOL    TXTI
        LODI,R0 SDONE>>8
        LODI,R1 SDONE&$FF
        BCTR,UN SR3
SR2A:   DCOL    LABI
        LODI,R0 SENTERD>>8
        LODI,R1 SENTERD&$FF
SR3:    LODI,R3 13
        BSTA,UN TXTPAD
        DCOL    TXTI
        LODA,R0 ST+OFF_TIMER
        LODI,R1 2
        ZBSR    *ZDNUM
        RETC,UN

; SFOOT: row 10's numbers: the chosen's stock and bundle (none: blank).
SFOOT:  DCOL    TXTI
        DAT     10,4
        LODA,R0 SHN
        BCTR,EQ SF0
        LODA,R3 SHSEL
        LODA,R0 SHIDS,R3
        STRZ    R3
        LODA,R0 ST+OFF_INV,R3
        LODI,R1 2
        ZBSR    *ZDNUM
        DAT     10,14
        LODA,R3 SHSEL
        LODA,R0 SHIDS,R3
        STRZ    R3
        LODA,R0 CIBUN,R3
        LODI,R1 2
        ZBSR    *ZDNUM
        RETC,UN
SF0:    RETC,UN

; SHCUR: glyph R0 in the list's row R1's cursor cell (rows past the list:
; none). Leaf. Clobbers R0, R1, R3, T0.
SHCUR:  COMI,R1 SHROWS
        RETC,GT
        RETC,EQ
        STRA,R0 T0
        LODZ    R1
        ADDI,R0 2
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R3
        LODA,R0 T0
        STRA,R0 SCR,R3
        RETC,UN

; SHALL: the list's rows.
SHALL:  EORZ    R0
SHA1:   STRA,R0 WI
        BSTR,UN SHLINE
        LODA,R0 WI
        ADDI,R0 1
        COMI,R0 SHROWS
        BCFR,EQ SHA1
        RETC,UN

; SHLINE: the list's row WI (row 2 + WI): entry SHTOP + WI, '>' if chosen,
; its name, its price (past the list: blank). Calls three down. Clobbers
; R0-R3, T0-T3, N32, DIG.
SHLINE: LODA,R1 WI
        ADDI,R1 2
        LODI,R2 $FF
        ZBSR    *ZDPOS
        LODA,R0 SHTOP
        ADDA,R0 WI
        STRA,R0 T0              ; T0: its place in the list
        COMA,R0 SHN
        BCTR,LT SHL1
        LODI,R0 GSPACE
        LODI,R3 16
        ZBSR    *ZDFILL
        RETC,UN
SHL1:   LODI,R0 GSPACE          ; the cursor
        LODA,R1 T0
        COMA,R1 SHSEL
        BCFR,EQ SHL2
        LODI,R0 GCURSOR|USI
SHL2:   ZBSR    *ZDGLY
        LODA,R3 T0
        LODA,R0 SHIDS,R3
        STRA,R0 T1              ; T1: the item
        DCOL    TXTI
        BSTA,UN SN9AT           ; its name (9)
        EORZ    R0
        ZBSR    *ZDPUTC
        LODA,R3 T1              ; its price (5)
        LODA,R0 CIPRL,R3
        STRZ    R1
        LODA,R0 CIPRH,R3
        BSTA,UN N16
        LODI,R0 0
        LODI,R1 5
        BCTA,UN NUMX            ; tail call

; SN9AT: item T1's short name (names.inc SN9: 9 cells) at the next cells,
; in DCOLOR. Calls one down. Clobbers R0, R1, R3; advances R2.
SN9AT:  LODA,R0 T1
        LODI,R1 0               ; R1: SN9's first part, or SN9B's
        COMI,R0 28
        BCTR,LT SN1
        SUBI,R0 28
        LODI,R1 1
SN1:    STRZ    R3              ; R3: 9 x it
        ADDZ    R0
        ADDZ    R0
        ADDZ    R0
        ADDZ    R3
        STRZ    R3
        LODI,R0 9
        STRA,R0 T2
SN2:    COMI,R1 0
        BCFR,EQ SN3
        LODA,R0 *PSN9A,R3
        BCTR,UN SN4
SN3:    LODA,R0 *PSN9B,R3
SN4:    ZBSR    *ZDPUTC
        ADDI,R3 1
        LODA,R0 T2
        SUBI,R0 1
        STRA,R0 T2
        BCFR,EQ SN2
        RETC,UN
PSN9A:  DWBE    SN9
PSN9B:  DWBE    SN9B

; ---- a frame of the shop -------------------------------------------------------------
SFRAME: LODA,R0 SHOWN
        COMI,R0 S_SHOP
        RETC,LT
        RETC,GT
        LODA,R3 MYSEAT          ; ours to shop: seated, not done
        COMI,R3 SEATS
        RETC,GT
        RETC,EQ
        LODA,R0 SEAT25,R3
        STRZ    R3
        LODA,R0 ST+OFF_SEATS+P_FLAGS,R3
        ANDI,R0 PF_DONE
        RETC,GT
        LODA,R0 EVKEY           ; Enter: done
        COMI,R0 K_ENT
        BCFR,EQ SFR0
        LODI,R1 ACT_DONE
        LODI,R2 0
        ZBSR    *ZACTQ
        SND     UI_GO
        RETC,UN
SFR0:   LODA,R0 SHN
        RETC,EQ
        BSTA,UN REP             ; the disc
        STRA,R0 WMV
        LODA,R0 SHSEL
        STRA,R0 WOLD
        LODA,R0 WMV
        TMI,R0  D_UP
        BCFR,EQ SFR1
        LODA,R0 SHSEL
        BCTR,EQ SFR1
        SUBI,R0 1
        STRA,R0 SHSEL
SFR1:   LODA,R0 WMV
        TMI,R0  D_DOWN
        BCFR,EQ SFR2
        LODA,R0 SHSEL
        ADDI,R0 1
        COMA,R0 SHN
        BCFR,LT SFR2
        STRA,R0 SHSEL
SFR2:   LODA,R0 SHSEL           ; moved (a step: REP's): the cursor moves,
        COMA,R0 WOLD            ;  or the rows move a row on the screen and
        BCTA,EQ SFR6            ;  the one that came is drawn (the 2026-11-23
        SND     UI_TICK         ;  TODO: a scroll costs shop time)
        LODA,R0 WOLD            ; the old row's cursor off (if still shown)
        SUBA,R0 SHTOP
        STRZ    R1
        LODI,R0 GSPACE
        BSTA,UN SHCUR
        LODA,R0 SHSEL
        COMA,R0 SHTOP
        BCTR,LT SFR3            ; above the top: the rows down
        SUBA,R0 SHTOP
        COMI,R0 SHROWS
        BCTR,LT SFR4            ; still shown: its cursor
        LODA,R0 SHTOP           ; below: the rows up, the last drawn
        ADDI,R0 1
        STRA,R0 SHTOP
        LODI,R3 $FF
SFR2A:  LODA,R0 SCR+48,R3,+
        STRA,R0 SCR+32,R3
        COMI,R3 (SHROWS-1)*16-1
        BCFR,EQ SFR2A
        LODI,R0 SHROWS-1
        STRA,R0 WI
        BSTA,UN SHLINE
        BCTR,UN SFR5
SFR3:   STRA,R0 SHTOP           ; the rows down, the first drawn
        LODI,R3 (SHROWS-1)*16
SFR3A:  LODA,R0 SCR+32-1,R3
        STRA,R0 SCR+48-1,R3
        BDRR,R3 SFR3A
        EORZ    R0
        STRA,R0 WI
        BSTA,UN SHLINE
        BCTR,UN SFR5
SFR4:   STRZ    R1              ; the new row's cursor on
        LODI,R0 GCURSOR|USI
        BSTA,UN SHCUR
SFR5:   BSTA,UN SFOOT
        LODA,R0 SHMSG           ; (NOT ENOUGH CASH goes with a move)
        BCTA,EQ SFR6
        EORZ    R0
        STRA,R0 SHMSG
        BSTA,UN SROW12
SFR6:   LODA,R3 SHSEL           ; the chosen: AARGS = it, 1
        LODA,R0 SHIDS,R3
        STRA,R0 T1
        LODA,R0 EVBTN           ; FIRE: buy, if the cash is there
        BCTA,EQ SFR8
        LODA,R0 ST+OFF_CASH+3   ; (32 bits: past 65535, plenty)
        IORA,R0 ST+OFF_CASH+2
        BCFR,EQ SFR7
        LODA,R3 T1
        LODA,R0 ST+OFF_CASH+1
        COMA,R0 CIPRH,R3
        BCTR,LT SFR6P
        BCTR,GT SFR7
        LODA,R0 ST+OFF_CASH
        COMA,R0 CIPRL,R3
        BCFR,LT SFR7
SFR6P:  SND     UI_ERROR
        LODI,R0 1
        STRA,R0 SHMSG
        BSTA,UN SROW12
        BCTR,UN SFR8
SFR7:   LODI,R1 ACT_BUY
        BSTR,UN SACT
        SND     UI_GO
        LODA,R0 SHMSG
        BCTR,EQ SFR8
        EORZ    R0
        STRA,R0 SHMSG
        BSTA,UN SROW12
SFR8:   LODA,R0 PRVDIR          ; LEFT pushed (not held: no selling by holding)
        EORI,R0 $FF
        ANDA,R0 INDIR
        TMI,R0  D_LEFT
        RETC,LT
        LODA,R3 T1              ; and we have one
        LODA,R0 ST+OFF_INV,R3
        RETC,EQ
        LODI,R1 ACT_SELL
        BSTR,UN SACT
        SND     UI_SELECT
        RETC,UN
; SACT: action R1 (ACT_BUY, ACT_SELL) on item T1, a bundle. Calls two
; down. Clobbers R0-R3.
SACT:   STRA,R1 T0
        CPTR    AARGS
        LODA,R0 T1
        BSTA,UN RAMPUT1
        LODI,R0 1
        BSTA,UN RAMPUT1
        BSTA,UN RAMPEND
        LODA,R1 T0
        LODI,R2 2
        ZBSR    *ZACTQ
        RETC,UN

; ---- a round's end, the game's ----------------------------------------------------------
; RENTER: row 0 which; rows 1-2 who won (its name in its colour, then WINS
; THE ROUND / THE GAME!); rows 3-12 the seats that are filled, each with
; its wins.
RENTER: ZBSR    *ZTEXT
        LODI,R0 MUS_WIN         ; (the round's tune, or the game's)
        LODA,R1 SHOWN
        COMI,R1 S_OVER
        BCFR,EQ RE0
        LODI,R0 MUS_OVER
RE0:    ZBSR    *ZSNDQ
        DCOL    LABI
        LODI,R0 SRES>>8
        LODI,R1 SRES&$FF
        LODA,R2 SHOWN
        COMI,R2 S_OVER
        BCFR,EQ RE1
        LODI,R0 SOVER>>8
        LODI,R1 SOVER&$FF
RE1:    LODI,R3 0
        ZBSR    *ZCENTR
        LODA,R3 ST+OFF_RESULT   ; who won
        COMI,R3 SEATS
        BCFR,LT RSTATE
        DAT     1,4
        BSTA,UN SNAMEI
        ZBSR    *ZNAMEAT
        DCOL    TXTI
        LODI,R0 SWROUND>>8
        LODI,R1 SWROUND&$FF
        LODA,R2 SHOWN
        COMI,R2 S_OVER
        BCFR,EQ RE2
        LODI,R0 SWGAME>>8
        LODI,R1 SWGAME&$FF
RE2:    LODI,R3 2
        ZBSR    *ZCENTR
; RSTATE: the seats with their wins (rows 3 on).
RSTATE: LODI,R0 3               ; WOLD: the row, WI: the seat
        STRA,R0 WOLD
        EORZ    R0
RS1:    STRA,R0 WI
        STRZ    R3
        LODA,R0 SEAT25,R3
        STRZ    R3
        LODA,R0 ST+OFF_SEATS+P_KIND,R3
        BCTA,EQ RS9
        LODA,R1 WOLD
        LODI,R2 $FF
        ZBSR    *ZDPOS
        LODI,R0 GSPACE          ; ours: the cursor
        LODA,R3 WI
        COMA,R3 MYSEAT
        BCFR,EQ RS2
        LODI,R0 GCURSOR|USI
RS2:    ZBSR    *ZDGLY
        LODA,R3 WI
        BSTA,UN SNAMEI
        ZBSR    *ZNAMEAT
        EORZ    R0
        ZBSR    *ZDGLY
        DCOL    LABI
        TXTAT   SWIN
        DCOL    TXTI
        LODA,R3 WI
        LODA,R0 SEAT25,R3
        STRZ    R3
        LODA,R0 ST+OFF_SEATS+P_WINS,R3
        LODI,R1 3
        ZBSR    *ZDNUM
        LODA,R0 WOLD
        ADDI,R0 1
        STRA,R0 WOLD
RS9:    LODA,R0 WI
        ADDI,R0 1
        COMI,R0 SEATS
        BCFA,EQ RS1
        RETC,UN

; CATOURS: CC EQ if the state's catalog is the one in ROM (p_room.inc's).
; Leaf. Keeps R0-R2.
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

        INCLUDE "src/numx.inc"
        INCLUDE "src/txt.inc"

SSHOP:  DB      "SHOP    ROUND",$FF
SCASH:  DB      "CASH",$FF
SOWN:   DB      "OWN",$FF
SBUND:  DB      "BUNDLE",$FF
SHINT:  DB      "FIRE BUY  < SELL",$FF
SPOOR:  DB      "NO CASH",$FF
SDONE:  DB      "DONE: WAITING",$FF
SENTERD: DB     "ENTER: DONE",$FF
SWATCHS: DB     "WATCHING",$FF
SRES:   DB      "ROUND OVER",$FF
SOVER:  DB      "GAME OVER",$FF
SWROUND: DB     "WINS THE ROUND",$FF
SWGAME: DB      "WINS THE GAME!",$FF
SWIN:   DB      "WIN",$FF

        INCLUDE "catitem.inc"
