; p_shop.inc -- bank BK_SHOP: the shop, a round's end, the game's
; (src/room.c). Entered only through these (equ.inc E_*).
        FJMP    shop_enter              ; E_SENTER
        FJMP    shop_state              ; E_SSTATE
        FJMP    shop_pass               ; E_SPASS
        FJMP    res_enter               ; E_RENTER
        FJMP    res_state               ; E_RSTATE
        FJMP    res_pass                ; E_RPASS

; The shop, the 12-line font, 16 by 10:
;   row 0  SHOP    ROUND nn          row 1  CASH  nnnnnnnnnnn
;   rows 2-7  six of the list: '>' the chosen, its name (9), its price (6):
;          what the arms level allows, but the mini missile
;   row 8  OWN nn  X nn  tt  (the chosen: owned, the bundle; the timer)
;   row 9  5BUY 4SELL 9DONE / DONE: WAITING / NOT ENOUGH CASH
; 2 and 8 choose (the list scrolls), 5 buys a bundle (if the cash is
; there), 4 sells one, 9 is done: the STATE's cash and stock answer each.
; Its state is in the player's ellipse bytes (not in use off the field).
SHN     EQU     EX              ; the list's items (SHIDS: the cart's RAM)
SHSEL   EQU     EY              ;  the chosen
SHTOP   EQU     ERX             ;  the first shown
SHMSG   EQU     ERY             ;  jiffies NOT ENOUGH CASH stays (0: none)
SHROWS  EQU     6

        RFIT    141
shop_enter:
        OPN     TOP_CLS
        SND     MUS_SHOP
        SETV    SHN, 0
        SETV    SHSEL, 0
        SETV    SHTOP, 0
        SETV    SHMSG, 0
        FCALL   cat_check
        BZ      she3
        POKEAT  SHIDS                   ; the list: items 1-47 the arms allow
        SETV    T5, 1
she1:   GETV    T5
        FCALL   cat_item
        INC     R8
        LDN     R8                      ; its arms level
        STR     R2
        GETS    OFF_SETTINGS+SET_ARMS
        SM
        BNF     she2
        GETV    T5
        PKD
        INCV    SHN
she2:   INCV    T5
        XRI     N_ITEMS
        BNZ     she1
        PKEND
she3:   FCALL   sh_rows
        FJMP    shop_state

; shop_state: the round, the cash, the chosen's line, the status.
        RFIT    132
shop_state:
        FCALL   hl_clear                ; SHOP    ROUND nn
        LDR     R8, s_shop
        LDI     0
        FCALL   ln_str
        LDR     R8, s_round
        LDI     8
        FCALL   ln_str
        GETS    OFF_ROUND
        ADI     1
        PLO     R8
        LDI     0
        PHI     R8
        LDI     LO(HLINE+14)
        PLO     R9
        LDI     2
        FCALL   hl_num
        LDI     0
        FCALL   ln_row
        FCALL   hl_clear                ; CASH nnnnnnnnnnn
        LDR     R8, s_cash
        LDI     0
        FCALL   ln_str
        LDR     R8, ST+OFF_CASH         ; (num32 spends it: a copy)
        LDR     R9, T0
        LDI     4
        PLO     RC
sst1:   LDA     R8
        STR     R9
        INC     R9
        DEC     RC
        GLO     RC
        BNZ     sst1
        LDR     R8, T0
        LDR     R9, VALBUF
        FCALL   num32
        FCALL   sh_right                ; right-aligned in 11
        LDI     1
        FCALL   ln_row
        FCALL   sh_own
        FJMP    sh_status
s_shop: DB      "SHOP", 0
s_round: DB     "ROUND", 0
s_cash: DB      "CASH", 0

; sh_right: VALBUF right-aligned at HLINE's end (column 15 its last).
        RFIT    32
sh_right:
        LDR     R8, VALBUF
        LDI     0
        PLO     RC
shr1:   LDA     R8
        BZ      shr2
        INC     RC
        BR      shr1
shr2:   GLO     RC
        SDI     16
        BNF     shr9
        PLO     RC
        LDR     R8, VALBUF
        GLO     RC
        FJMP    ln_str
shr9:   FRET

; sh_rows: rows 2-7 from SHTOP.
        RFIT    34
sh_rows:
        SETV    T1, 0                   ; (sh_line spends T2-T7)
shw1:   GETV    T1
        STR     R2
        GETV    SHTOP
        ADD
        FCALL   sh_line
        FCALL   in_poll                 ; (a long draw: no press lost)
        INCV    T1
        XRI     SHROWS
        BNZ     shw1
        FRET

; sh_line: list entry D on its row (if shown): '>' the chosen, its name
;   (9), its price (6). T2 the entry (hl_num spends T3-T7).
        RFIT    150
sh_line:
        PUTV    T2
        STR     R2                      ; shown? SHTOP <= D < SHTOP + 6
        GETV    SHTOP
        SD
        BNF     shl9
        SMI     SHROWS
        BDF     shl9
        FCALL   hl_clear
        GETV    T2                      ; an item (past the list: blank)
        STR     R2
        GETV    SHN
        SM
        BZ      shl8
        BNF     shl8
        GETV    T2
        ADI     LO(SHIDS)
        PLO     RB
        LDI     HI(SHIDS)
        ADCI    0
        PHI     RB
        LDN     RB
        FCALL   cat_item                ; its name
        GLO     R8
        PUTV    T3
        GHI     R8
        PUTV    T4
        GLO     R8
        ADI     6
        PLO     R8
        GHI     R8
        ADCI    0
        PHI     R8
        LDI     9
        PLO     RC
        LDI     1
        FCALL   ln_lp
        GETV    T3                      ; its price (16 bits), 6 places
        ADI     3
        PLO     RB
        GETV    T4
        ADCI    0
        PHI     RB
        LDA     RB
        PLO     R8
        LDN     RB
        PHI     R8
        LDI     LO(HLINE+10)
        PLO     R9
        LDI     6
        FCALL   hl_num
        GETV    T2                      ; the chosen: '>'
        STR     R2
        GETV    SHSEL
        XOR
        BNZ     shl8
        LDI     '>'
        PUT9    HLINE
shl8:   GETV    T2                      ; its row: 2 + D - SHTOP
        STR     R2
        GETV    SHTOP
        SD
        ADI     2
        FJMP    ln_row
shl9:   FRET

; sh_own: row 8: the chosen's stock and bundle, the timer.
        RFIT    129
sh_own: FCALL   hl_clear
        GETV    SHN
        BZ      sho8
        GETV    SHSEL
        ADI     LO(SHIDS)
        PLO     RB
        LDI     HI(SHIDS)
        ADCI    0
        PHI     RB
        LDN     RB
        PUTV    T2                      ; (hl_num spends T3-T7)
        LDR     R8, s_own
        LDI     0
        FCALL   ln_str
        GETV    T2                      ; owned
        ADI     LO(ST+OFF_INV)
        PLO     RB
        LDI     HI(ST+OFF_INV)
        ADCI    0
        PHI     RB
        LDN     RB
        PLO     R8
        LDI     0
        PHI     R8
        LDI     LO(HLINE+4)
        PLO     R9
        LDI     2
        FCALL   hl_num
        LDI     'X'
        PUT9    HLINE+8
        GETV    T2                      ; its bundle
        FCALL   cat_item
        INC     R8
        INC     R8
        LDN     R8
        PLO     R8
        LDI     0
        PHI     R8
        LDI     LO(HLINE+9)
        PLO     R9
        LDI     2
        FCALL   hl_num
sho8:   GETS    OFF_TIMER               ; the timer
        PLO     R8
        LDI     0
        PHI     R8
        LDI     LO(HLINE+14)
        PLO     R9
        LDI     2
        FCALL   hl_num
        LDI     8
        FJMP    ln_row
s_own:  DB      "OWN", 0

; sh_status: row 9: NOT ENOUGH CASH (a while), DONE: WAITING, or the keys.
        RFIT    106
sh_status:
        FCALL   hl_clear
        LDR     R8, s_nocash
        GETV    SHMSG
        BNZ     sts1
        LDR     R8, s_shopk
        GETV    MYSEAT
        SMI     SEATS
        BDF     sts1
        GETV    MYSEAT
        FCALL   seat_rec
        GLO     RB
        ADI     P_FLAGS
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDN     RB
        ANI     PF_DONE
        BZ      sts1
        LDR     R8, s_doneW
sts1:   LDI     0
        FCALL   ln_str
        LDI     9
        FJMP    ln_row
s_shopk: DB     "5BUY 4SELL 9DONE", 0
s_nocash: DB    "NOT ENOUGH CASH", 0
s_doneW: DB     "DONE: WAITING", 0

; ---- the keys ----------------------------------------------------------------------
; shop_pass: 2 8 choose, 5 buy, 4 sell, 9 done (ours, not done yet); the
;   message's time.
        RFIT    112
shop_pass:
        GETV    SHMSG                   ; NOT ENOUGH CASH: its time
        BZ      shp0
        GETV    DT
        STR     R2
        GETV    SHMSG
        SM
        BDF     shp0a
        LDI     0
shp0a:  STR     RA
        BNZ     shp0
        FCALL   sh_status
shp0:   GETV    MYSEAT                  ; ours, not done
        SMI     SEATS
        BDF     shp9
        GETV    MYSEAT
        FCALL   seat_rec
        GLO     RB
        ADI     P_FLAGS
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDN     RB
        ANI     PF_DONE
        BNZ     shp9
        FCALL   key_take
        PUTV    T0
        XRI     9
        BNZ     shp1
        SND     UI_GO
        SETV    ANARG, 0                ; 9: done
        LDI     ACT_DONE
        FJMP    act_q
shp1:   GETV    SHN
        BZ      shp9
        GETV    T0
        XRI     2
        BZ      shp2
        GETV    T0
        XRI     8
        BZ      shp2
        FJMP    sh_buysell
shp2:   FJMP    sh_move
shp9:   FRET

; sh_move: T0 2 or 8: the cursor (the list scrolls a row).
        RFIT    139
sh_move:
        GETV    SHSEL
        PUTV    T7                      ; (the old)
        GETV    T0
        XRI     2
        BNZ     smv2
        GETV    SHSEL                   ; up
        BZ      smv9
        SMI     1
        PUTV    SHSEL
        STR     R2
        GETV    SHTOP
        SM
        BNF     smv8
        GETV    SHSEL
        PUTV    SHTOP
        FJMP    smv_all
smv2:   GETV    SHSEL                   ; down
        ADI     1
        STR     R2
        GETV    SHN
        SM
        BZ      smv9
        BNF     smv9
        LDN     R2
        PUTV    SHSEL
        GETV    SHTOP                   ; past the six shown: on a row
        STR     R2
        GETV    SHSEL
        SM
        SMI     SHROWS
        BNF     smv8
        GETV    SHSEL
        SMI     SHROWS-1
        PUTV    SHTOP
        FJMP    smv_all
smv8:   SND     UI_TICK
        GETV    T7                      ; the old line, the new
        FCALL   sh_line
        GETV    SHSEL
        FCALL   sh_line
        FJMP    sh_own
smv9:   FRET
smv_all:
        SND     UI_TICK
        FCALL   sh_rows
        FJMP    sh_own

; sh_buysell: T0 5: a bundle bought (the cash there), 4: one sold.
        RFIT    130
sh_buysell:
        GETV    SHSEL                   ; the chosen item
        ADI     LO(SHIDS)
        PLO     RB
        LDI     HI(SHIDS)
        ADCI    0
        PHI     RB
        LDN     RB
        PUTV    AARGS
        SETV    AARGS+1, 1
        SETV    ANARG, 2
        GETV    T0
        XRI     4
        BNZ     sbs2
        GETV    AARGS                   ; 4: if we have one
        ADI     LO(ST+OFF_INV)
        PLO     RB
        LDI     HI(ST+OFF_INV)
        ADCI    0
        PHI     RB
        LDN     RB
        BZ      sbs9
        SND     UI_TICK
        LDI     ACT_SELL
        FJMP    act_q
sbs2:   GETV    T0
        XRI     5
        BNZ     sbs9
        GETV    AARGS                   ; 5: the cash >= the price (24 bits)
        FCALL   cat_item
        INC     R8
        INC     R8
        INC     R8
        LDR     RB, ST+OFF_CASH
        LDA     RB                      ; cash - price: no borrow out of 32
        STR     R2
        LDA     R8
        SD
        LDA     RB
        STR     R2
        LDA     R8
        SDB
        LDA     RB
        STR     R2
        LDN     R8
        SDB
        LDN     RB
        SMBI    0
        BNF     sbs3
        SND     UI_SELECT
        LDI     ACT_BUY
        FJMP    act_q
sbs3:   SND     UI_ERROR
        SETV    SHMSG, 90               ; NOT ENOUGH CASH
        FJMP    sh_status
sbs9:   FRET

; ---- a round's end, the game's -------------------------------------------------------
; row 0 ROUND OVER / GAME OVER; rows 2-3 the winner (mark, name) or
; NOBODY; rows 5-9 the seats, two a row: the mark, the name (5), the wins.
        RFIT    230
res_enter:
        OPN     TOP_CLS
        GETS    OFF_PHASE               ; the game's end, or a round's
        XRI     PH_GAMEOVER
        BNZ     rsn1
        LDI     MUS_OVER
        BR      rsn2
rsn1:   LDI     MUS_WIN
rsn2:   FCALL   snd_play
        FCALL   hl_clear
        LDR     R8, s_rover                     ; ROUND OVER / GAME OVER (room.c)
        GETS    OFF_PHASE
        XRI     PH_GAMEOVER
        BNZ     rse1
        LDR     R8, s_over
rse1:   LDI     3
        FCALL   ln_str
rse2:   LDI     0
        FCALL   ln_row
        FCALL   hl_clear
        LDR     R8, s_winner
        LDI     0
        FCALL   ln_str
        LDI     2
        FCALL   ln_row
        FCALL   hl_clear
        GETS    OFF_RESULT
        SMI     SEATS
        BNF     rse3
        LDR     R8, s_nobody
        LDI     2
        FCALL   ln_str
        BR      rse4
rse3:   SETV    T3, 2
        LDI     12
        PLO     RC
        GETS    OFF_RESULT
        FCALL   ln_seat
rse4:   LDI     3
        FCALL   ln_row
res_state:
        SETV    T1, 0                   ; the seats with their wins (rs_seat
rst1:   FCALL   hl_clear                ;  spends T3-T7)
        SETV    T3, 0
        GETV    T1
        FCALL   rs_seat
        SETV    T3, 8
        GETV    T1
        ADI     1
        FCALL   rs_seat
        GETV    T1
        SHR
        ADI     5
        FCALL   ln_row
        GETV    T1
        ADI     2
        STR     RA
        XRI     SEATS
        BNZ     rst1
res_pass:
        FRET
s_over: DB      "GAME OVER", 0
s_rover: DB     "ROUND OVER", 0
s_winner: DB    "WINNER", 0
s_nobody: DB    "NOBODY", 0

; rs_seat: seat D at column T3 (seated): the mark, the name (5), the wins.
        RFIT    63
rs_seat:
        PUTV    T6
        FCALL   seat_rec
        GLO     RB
        ADI     P_KIND
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDN     RB
        BZ      rss9
        LDI     5
        PLO     RC
        GETV    T6
        FCALL   ln_seat
        GETV    T6                      ; its wins, 2 places at column + 6
        FCALL   seat_rec
        GLO     RB
        ADI     P_WINS
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDN     RB
        PLO     R8
        LDI     0
        PHI     R8
        GETV    T3
        ADI     LO(HLINE+6)
        PLO     R9
        LDI     2
        FJMP    hl_num
rss9:   FRET
