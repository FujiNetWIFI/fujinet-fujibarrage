; p_room.inc -- bank BK_ROOM: the waiting room and the settings
; (src/room.c). Entered only through these (equ.inc E_*).
        FJMP    wait_enter              ; E_WENTER
        FJMP    wait_state              ; E_WSTATE
        FJMP    wait_pass               ; E_WPASS
        FJMP    title_scene             ; E_TSCENE

; The 12-line font, 16 columns by 10 rows:
;   seats     row 0 "ROOM" and its id; rows 1-5 the ten seats, two a row:
;             the mark, the name (6), '*' the host, '?' a person's link
;             down; row 7 "7 SETTINGS"; row 8 "5 START" (the host) or
;             "WAITING"; row 9 "0 LEAVE  9 SOUND"
;   settings  row 0 the page's name; rows 1-8 its settings, the label (9)
;             and the value (6, right-aligned); the host's cursor inverted; row 9 the
;             keys: 4 6 change it (the host), 2 8 choose, 1 3 the page
; 7 shows one or the other; 5 starts (the host). The settings' state is in
; the player's ellipse bytes (the field's, not shown now): WSET.. below.
WSET    EQU     EX              ; the settings shown
WPAGE   EQU     EY              ;  the page
WSEL    EQU     ERX             ;  the host's cursor (the page's nth)
WTOP    EQU     ERY             ;  the first shown
WN      EQU     EH              ;  the page's settings (WKS: the cart's RAM)
WROW    EQU     ESTEP           ;  a row being drawn

; wait_enter: the seats, afresh.
        RFIT    27
wait_enter:
        SETV    WSET, 0
        SETV    WPAGE, 0
        SETV    WSEL, 0
        SETV    WTOP, 0
        FJMP    wt_draw

; wt_draw: the screen whole: the seats, or the settings.
        RFIT    139
wt_draw:
        OPN     TOP_CLS
        GETV    WSET
        BZ      wtd1
        FJMP    ws_draw
wtd1:   FCALL   hl_clear                ; ROOM id
        LDR     R8, s_room
        LDI     0
        FCALL   ln_str
        LDR     R8, SRVTBL
        LDI     5
        FCALL   ln_str
        LDI     0
        FCALL   ln_row
        FCALL   hl_clear                ; the keys
        LDR     R8, s_7set
        LDI     0
        FCALL   ln_str
        LDI     7
        FCALL   ln_row
        FCALL   hl_clear
        LDR     R8, s_0leave
        LDI     0
        FCALL   ln_str
        LDI     9
        FCALL   ln_row
        FJMP    wait_state
s_room: DB      "ROOM", 0
s_7set: DB      "7 SETTINGS", 0
s_0leave: DB    "0 LEAVE  9 SOUND", 0
s_5start: DB    "5 START", 0
s_waiting: DB   "WAITING", 0

; wait_state: what a commit changed: the seats (rows 1-5, row 8), or the
;   settings' values.
        RFIT    110
wait_state:
        GETV    WSET
        BZ      wst1
        FJMP    ws_rows
wst1:   SETV    T5, 0                   ; the seats, two a row
wst2:   FCALL   hl_clear
        SETV    T3, 0
        GETV    T5
        FCALL   wt_seat
        SETV    T3, 8
        GETV    T5
        ADI     1
        FCALL   wt_seat
        GETV    T5
        SHR
        ADI     1
        FCALL   ln_row
        FCALL   in_poll                 ; (a long draw: no press lost)
        GETV    T5
        ADI     2
        STR     RA
        XRI     SEATS
        BNZ     wst2
        FCALL   hl_clear                ; row 8: the host starts, or we wait
        LDR     R8, s_waiting
        GETS    OFF_HOST
        STR     R2
        GETV    MYSEAT
        XOR
        BNZ     wst3
        LDR     R8, s_5start
wst3:   LDI     0
        FCALL   ln_str
        LDI     8
        FJMP    ln_row

; wt_seat: seat D at HLINE's column T3: its mark and name (6), '*' the
;   host, '?' a person's link down; " -" empty.
        RFIT    112
wt_seat:
        PUTV    T6
        FCALL   seat_rec                ; its kind
        GLO     RB
        ADI     P_KIND
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDN     RB
        BNZ     wsa1
        LDR     R8, s_open
        GETV    T3
        FJMP    ln_str
wsa1:   LDI     6
        PLO     RC
        GETV    T6
        FCALL   ln_seat
        GETV    T3                      ; the mark after the name: column + 7
        ADI     LO(HLINE+7)
        PLO     R9
        LDI     HI(HLINE)
        PHI     R9
        GETS    OFF_HOST                ; the host
        STR     R2
        GETV    T6
        XOR
        BNZ     wsa2
        LDI     '*'
        STR     R9
        FRET
wsa2:   GETV    T6                      ; a person, its link down
        FCALL   seat_rec
        GLO     RB
        ADI     P_KIND
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDN     RB
        XRI     KIND_HUMAN
        BNZ     wsa9
        GLO     RB
        ADI     P_FLAGS-P_KIND
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDN     RB
        ANI     PF_CONN
        BNZ     wsa9
        LDI     '?'
        STR     R9
wsa9:   FRET
s_open: DB      " -", 0

; ---- the settings ---------------------------------------------------------------
; ws_draw: the page's name, its settings (WKS), the keys.
        RFIT    197
ws_draw:
        FCALL   cat_check
        BNZ     wsd0
        FRET
wsd0:   POKEAT  WKS                     ; this page's settings into WKS
        SETV    WN, 0
        SETV    T5, 0
wsd1:   GETV    T5
        FCALL   cat_setting
        LDN     R8                      ; (its page)
        STR     R2
        GETV    WPAGE
        XOR
        BNZ     wsd2
        GETV    WN
        SMI     16
        BDF     wsd2
        INCV    WN
        GETV    T5
        PKD
wsd2:   INCV    T5
        XRI     N_SETTINGS
        BNZ     wsd1
        PKEND
        FCALL   hl_clear                ; row 0: the page's name
        GETV    WPAGE
        FCALL   cat_page
        LDI     16
        PLO     RC
        LDI     0
        FCALL   ln_lp
        LDI     0
        FCALL   ln_row
        FCALL   hl_clear                ; row 9: the keys
        LDR     R8, s_setk
        GETS    OFF_HOST
        STR     R2
        GETV    MYSEAT
        XOR
        BZ      wsd3
        LDR     R8, s_setv
wsd3:   LDI     0
        FCALL   ln_str
        LDI     9
        FCALL   ln_row
        FJMP    ws_rows
s_setk: DB      "46SET 28 13PAGE", 0
s_setv: DB      "13 PAGE 7 BACK", 0

; ws_rows: rows 1-8: the settings from WTOP (label 8, value 8), the
;   host's cursor inverted. (cat_value spends T0-T7: WROW the row.)
        RFIT    95
ws_rows:
        SETV    WROW, 0
wsr1:   FCALL   hl_clear
        GETV    WROW                    ; the setting: WKS[WTOP + row], if any
        STR     R2
        GETV    WTOP
        ADD
        PUTV    T6
        STR     R2
        GETV    WN
        SM
        BZ      wsr3
        BNF     wsr3
        GETV    T6
        ADI     LO(WKS)
        PLO     RB
        LDI     HI(WKS)
        ADCI    0
        PHI     RB
        LDN     RB
        PUTV    T4
        FCALL   cat_setting             ; its label: 9
        INC     R8
        INC     R8
        LDI     9
        PLO     RC
        LDI     0
        FCALL   ln_lp
        GETV    T4                      ; its value (the state's): 8
        ADI     LO(ST+OFF_SETTINGS)
        PLO     RB
        LDI     HI(ST+OFF_SETTINGS)
        ADCI    0
        PHI     RB
        LDN     RB
        PUTV    T5
        FCALL   cat_value               ; (right-aligned: 6 at most)
        FCALL   ln_val6
wsr3:   FJMP    wsr4

        RFIT    69
wsr4:   GETV    WROW
        ADI     1
        FCALL   ln_row
        GETS    OFF_HOST                ; the host's cursor
        STR     R2
        GETV    MYSEAT
        XOR
        BNZ     wsr5
        GETV    WTOP                    ; on this row: WSEL - WTOP = row
        STR     R2
        GETV    WSEL
        SM
        STR     R2
        GETV    WROW
        XOR
        BNZ     wsr5
        GETV    WROW
        ADI     1
        FCALL   ln_inv
wsr5:   FCALL   in_poll                 ; (a long draw: no press lost)
        INCV    WROW
        XRI     8
        BNZ     wsr6
        FRET
wsr6:   FJMP    wsr1

; ---- the keys ----------------------------------------------------------------------
; wait_pass: 7 the seats or the settings; 5 starts (the host); with the
;   settings: 1 3 the page, 2 8 the cursor, 4 6 the value (the host).
        RFIT    172
wait_pass:
        FCALL   key_take
        PUTV    T0
        XRI     7
        BNZ     wp1
        GETV    WSET
        XRI     1
        STR     RA
        SETV    WSEL, 0
        SETV    WTOP, 0
        SND     UI_TICK
        FJMP    wt_draw
wp1:    GETV    T0
        XRI     5
        BNZ     wp2
        GETS    OFF_HOST
        STR     R2
        GETV    MYSEAT
        XOR
        BNZ     wp9
        SND     UI_GO
        SETV    ANARG, 0
        LDI     ACT_START
        FJMP    act_q
wp2:    GETV    T0                      ; 9: the sound off or on
        XRI     9
        BNZ     wp2b
        FJMP    snd_mute
wp2b:   GETV    WSET
        BZ      wp9
        GETV    WN
        BZ      wp9
        GETV    T0
        XRI     1
        BNZ     wp3
        GETV    WPAGE                   ; 1: the page before
        BNZ     wp2a
        LDI     3
wp2a:   SMI     1
        BR      wp3a
wp3:    GETV    T0
        XRI     3
        BNZ     wp4
        GETV    WPAGE                   ; 3: the next
        ADI     1
        SMI     3
        BDF     wp3a
        ADI     3
wp3a:   PUTV    WPAGE
        SND     UI_TICK
        SETV    WSEL, 0
        SETV    WTOP, 0
        FJMP    wt_draw
wp4:    FJMP    wp_cur
wp9:    FRET

; wp_cur: T0: 2 8 the cursor (the list scrolls), 4 6 the value (the host).
        RFIT    189
wp_cur: GETV    T0
        XRI     2
        BNZ     wc2
        GETV    WSEL                    ; 2: up
        BZ      wc9
        SMI     1
        PUTV    WSEL
        STR     R2
        GETV    WTOP
        SM
        BNF     wc8
        GETV    WSEL
        PUTV    WTOP
        BR      wc8
wc2:    GETV    T0
        XRI     8
        BNZ     wc4
        GETV    WSEL                    ; 8: down
        ADI     1
        STR     R2
        GETV    WN
        SM
        BZ      wc9
        BNF     wc9
        LDN     R2
        PUTV    WSEL
        GETV    WTOP                    ; past the 8 shown: the list on
        STR     R2
        GETV    WSEL
        SM
        SMI     8
        BNF     wc8
        GETV    WSEL
        SMI     7
        PUTV    WTOP
wc8:    SND     UI_TICK
        FJMP    ws_rows
wc4:    GETS    OFF_HOST                ; 4 6: the value (the host)
        STR     R2
        GETV    MYSEAT
        XOR
        BNZ     wc9
        GETV    T0
        XRI     4
        BZ      wc5
        GETV    T0
        XRI     6
        BNZ     wc9
wc5:    GETV    WSEL                    ; k = WKS[WSEL], v = its value
        ADI     LO(WKS)
        PLO     RB
        LDI     HI(WKS)
        ADCI    0
        PHI     RB
        LDN     RB
        PUTV    AARGS
        ADI     LO(ST+OFF_SETTINGS)
        PLO     RB
        LDI     HI(ST+OFF_SETTINGS)
        ADCI    0
        PHI     RB
        LDN     RB
        PUTV    AARGS+1
        GETV    AARGS                   ; n values
        FCALL   cat_count
        PUTV    T1
        FJMP    wc6
wc9:    FRET

        RFIT    74
wc6:    GETV    T0
        XRI     4
        BNZ     wc7
        GETV    AARGS+1                 ; 4: the one before (round)
        BNZ     wc6a
        GETV    T1
wc6a:   SMI     1
        BR      wc7a
wc7:    GETV    AARGS+1                 ; 6: the next (round)
        ADI     1
        PUTV    T2
        STR     R2
        GETV    T1
        SM
        BZ      wc7z
        BNF     wc7z
        GETV    T2
        BR      wc7a
wc7z:   LDI     0
wc7a:   PUTV    AARGS+1
        SND     UI_TICK
        SETV    ANARG, 2
        LDI     ACT_SET
        FJMP    act_q

; ---- the title (menus.c title) -----------------------------------------------------
; A field scene (tools/s2/mktitle_s2.py: title.inc): the hills put into the
; model and drawn, the shell's dotted arc and its burst, two tanks with
; their marks; FUJIBARRAGE and FOR FUJINET over the sky; PRESS 5 under
; the field. 5 goes on (the model emptied, the sprites hidden).
        RFIT    146
title_scene:
        OPN     TOP_CLS
        SETV    VIEWF, 0                ; a night sky: as drawn
        FCALL   view_set
        LDR     R8, TTOPS               ; the hills
        LDI     FW
        PHI     R9
        LDI     0
        PLO     R9
        FCALL   fld_puts
        FCALL   fld_all
        SETV    EMODE, TM_SET           ; the arc
        LDR     R8, TTARC
tts1:   LDA     R8
        XRI     0FFH
        BZ      tts2
        XRI     0FFH
        PLO     R9
        LDA     R8
        PHI     R9
        FCALL   dot_r9
        BR      tts1
tts2:   SETV    EX, TTBX                ; the burst
        SETV    EY, TTBY
        SETV    ERX, TTBRX
        SETV    ERY, TTBRY
        SETV    EMODE, FIRE_MODE
        FCALL   ell_draw
        FCALL   tk_reset                ; the tanks
        LDI     NONE
        FCALL   tk_blinkset
        LDI     TT0A
        FCALL   ang_step
        PHI     RC
        LDI     TT0Y
        PLO     RC
        LDI     TT0X
        PHI     R9
        LDI     1
        PLO     R9
        LDI     0
        FCALL   tk_set
        LDI     TT1A
        FCALL   ang_step
        PHI     RC
        LDI     TT1Y
        PLO     RC
        LDI     TT1X
        PHI     R9
        LDI     1
        PLO     R9
        FCALL   tk_set1
        FJMP    tts3

        RFIT    231
tts3:   SETV    TXM, TM_SET             ; the words
        LDR     R8, s_ttl
        LDI     TXROW(1)
        PHI     R9
        FCALL   txt_ctr
        SETV    TXM, TM_SET+TM_SHORT
        LDR     R8, s_tfn
        LDI     26
        PHI     R9
        FCALL   txt_ctr
        LDR     R8, s_tp5
        LDI     HUD_Y1+3
        PHI     R9
        FCALL   txt_ctr
        SETV    KEV, NONE
        SND     MUS_TITLE
tts4:   FCALL   frame                   ; until 5 (the tanks' images go
        FCALL   in_poll                 ;  meanwhile)
        FCALL   snd_tick
        FCALL   tk_pass
        FCALL   key_take
        XRI     5
        BNZ     tts4
        SND     UI_GO
        OPN     TOP_SPRHIDE
        FCALL   tk_reset
        FJMP    fld_clear
tk_set1:
        LDI     1
        FJMP    tk_set
s_ttl:  DB      "FUJIBARRAGE", 0
s_tfn:  DB      "FOR FUJINET", 0
s_tp5:  DB      "PRESS 5", 0
        INCLUDE "title.inc"
