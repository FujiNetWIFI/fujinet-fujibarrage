; hud.inc -- the field's two HUD rows (src/play.c hud), the short font, 16
; columns each; each row's gap line under it holds a bar.
;
;   108  mark, name (5), A angle (3), P power (4)      "*SPARK A 90P 550"
;   113  (the name's energy: a bar under it, 1 dot a 5)
;   114  weapon (8), count (2), wind (< or > and 3)    "MISSILE  9 <123"
;   119  (the turn's timer: a bar across, 4 dots a second, 15 at most)
;
; The HUD's seat is the turn's (ours while we aim: our aim's numbers). A
; row is built in HLINE and drawn whole when it changed (HSHAD).

HUD_SEAT EQU    HSEAT           ; (hud_rows: the seat shown; T1-T2 its
                                ;  power: num_s spends T5-T7, hl_num T3-T4)

; hud_rows: the rows HUDDRT asks for (b0 the first, b1 the second) and the
;   bars, for the seat whose turn it is.
        RFIT    93
hud_rows:
        GETS    OFF_TURN                ; the seat: the turn's, else ours, else 0
        SMI     SEATS
        BNF     hr1
        GETV    MYSEAT
        SMI     SEATS
        BNF     hr0
        LDI     0
        BR      hr2
hr0:    GETV    MYSEAT
        BR      hr2
hr1:    GETS    OFF_TURN
hr2:    PUTV    HUD_SEAT
        GETV    HUDDRT                  ; the rows asked for (b0 1, b1 2),
        ANI     1                       ;  or what holds them (fieldui.inc)
        BZ      hr3
        FCALL   row1_held
        BZ      hr2a
        FCALL   hud_line1
        BR      hr3
hr2a:   FCALL   hud_row1
hr3:    GETV    HUDDRT
        ANI     2
        BZ      hr4
        FCALL   row2_held
        BZ      hr3a
        FCALL   hud_line2
        BR      hr4
hr3a:   FCALL   hud_row2
hr4:    SETV    HUDDRT, 0
        FJMP    hud_bars

; hud_row1: mark, name, angle, power.
        RFIT    181
hud_row1:
        FCALL   hl_clear
        GETV    HUD_SEAT                ; the mark
        ADI     GL_MARK
        PUT9    HLINE
        GETV    HUD_SEAT                ; the name: 5
        FCALL   seat_rec
        LDR     R9, HLINE+1
        LDI     5
        PLO     RC
hr11:   LDA     RB
        BZ      hr12
        STR     R9
        INC     R9
        DEC     RC
        GLO     RC
        BNZ     hr11
hr12:   LDI     'A'
        PUT9    HLINE+7
        LDI     'P'
        PUT9    HLINE+11
        GETV    HUD_SEAT                ; the angle and power: our aim's, or
        STR     R2                      ;  the state's
        GETV    MYSEAT
        XOR
        BNZ     hr13
        GETV    AIMING
        BZ      hr13
        GETV    AIMA
        PLO     R8
        GETV    AIMP
        PLO     RC
        GETV    AIMP+1
        PHI     RC
        BR      hr14
hr13:   GETV    HUD_SEAT
        FCALL   seat_rec
        GLO     RB
        ADI     P_ANGLE
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDA     RB
        PLO     R8
        LDA     RB
        PLO     RC
        LDN     RB
        PHI     RC
hr14:   LDI     0
        PHI     R8
        GLO     RC
        PUTV    T1
        GHI     RC
        PUTV    T2
        LDI     HLINE+8-RAWBUF          ; the angle: 3 places
        PLO     R9
        LDI     3
        FCALL   hl_num
        GETV    T1
        PLO     R8
        GETV    T2
        PHI     R8
        LDI     HLINE+12-RAWBUF         ; the power: 4 places
        PLO     R9
        LDI     4
        FCALL   hl_num
        LDI     0
        FJMP    hl_draw

; hud_row2: weapon, count (ours), wind.
        RFIT    206
hud_row2:
        FCALL   hl_clear
        GETV    HUD_SEAT                ; the weapon: our aim's, or the state's
        STR     R2
        GETV    MYSEAT
        XOR
        BNZ     hr21
        GETV    AIMING
        BZ      hr21
        GETV    AWPN
        BR      hr22
hr21:   GETV    HUD_SEAT
        FCALL   seat_rec
        GLO     RB
        ADI     P_WEAPON
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDN     RB
hr22:   PUTV    T1
        FCALL   cat_check
        BZ      hr24
        GETV    T1                      ; its name: 8 at most
        FCALL   cat_item
        GLO     R8
        ADI     6
        PLO     R8
        GHI     R8
        ADCI    0
        PHI     R8
        LDA     R8
        PLO     RC
        SMI     9
        BNF     hr22a
        LDI     8
        PLO     RC
hr22a:  LDR     R9, HLINE
hr23:   GLO     RC
        BZ      hr24
        DEC     RC
        LDA     R8
        STR     R9
        INC     R9
        BR      hr23
hr24:   GETV    HUD_SEAT                ; the count: ours, not the missile
        STR     R2
        GETV    MYSEAT
        XOR
        BNZ     hr25
        GETV    T1
        BZ      hr25
        ADI     LO(ST+OFF_INV)
        PLO     RB
        LDI     HI(ST+OFF_INV)
        ADCI    0
        PHI     RB
        LDN     RB
        PLO     R8
        LDI     0
        PHI     R8
        LDI     HLINE+8-RAWBUF
        PLO     R9
        LDI     2
        FCALL   hl_num
hr25:   GETS    OFF_WIND                ; the wind: < or >, 3 places
        PLO     R8
        INC     RB
        LDN     RB
        PHI     R8
        ANI     80H
        BZ      hr26
        GLO     R8                      ; (negative: <, its size)
        SDI     0
        PLO     R8
        GHI     R8
        SDBI    0
        PHI     R8
        LDI     '<'
        PUT9    HLINE+12
        BR      hr27
hr26:   GLO     R8
        BNZ     hr26a
        GHI     R8
        BZ      hr27
hr26a:  LDI     '>'
        PUT9    HLINE+12
hr27:   LDI     HLINE+13-RAWBUF
        PLO     R9
        LDI     3
        FCALL   hl_num
        LDI     1
        FJMP    hl_draw

; hl_clear: HLINE all spaces (16), NUL after.
        RFIT    21
hl_clear:
        LDR     R8, HLINE
        LDI     16
        PLO     R9
hlc1:   LDI     ' '
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     hlc1
        LDI     0
        STR     R8
        FRET

; hl_num: R8 right-aligned in D places at HLINE's offset R9.0 (page $09:
;   RAWBUF + R9.0); a number too big shows its last places.
        RFIT    65
hl_num: PUTV    T4                      ; the places
        GLO     R9
        PUTV    T3
        LDI     1
        FCALL   num_s                   ; NUMB: its digits
        LDI     0                       ; how many
        PLO     RC
hn1:    LDA     R8
        BZ      hn2
        INC     RC
        BR      hn1
hn2:    DEC     R8                      ; R8 on the last digit (LDA went past
        DEC     R8                      ;  the NUL); from the right
        GETV    T3
        STR     R2
        GETV    T4
        ADD
        ADI     LO(RAWBUF)-1
        PLO     R9
        LDI     HI(RAWBUF)
        PHI     R9
hn3:    GLO     RC
        BZ      hn9
        GETV    T4
        BZ      hn9
        SMI     1
        STR     RA
        DEC     RC
        LDN     R8
        DEC     R8
        STR     R9
        DEC     R9
        BR      hn3
hn9:    FRET

; hl_draw: HLINE drawn as row D (0, 1) if it differs from its shadow.
        RFIT    133
hl_draw:
        PUTV    T3
        SHL                             ; the shadow: HSHAD + 16 row
        SHL
        SHL
        SHL
        ADI     LO(HSHAD)
        PLO     R9
        LDI     HI(HSHAD)
        PHI     R9
        LDR     R8, HLINE
        LDI     16                      ; (the shadow is the cart's RAM: read)
        PLO     RC
hd1:    LDA     R8
        STR     R2
        LDA     R9
        XOR
        BNZ     hd2
        DEC     RC
        GLO     RC
        BNZ     hd1
        BR      hd9                     ; the same: nothing
hd2:    GLO     R9                      ; another: the shadow poked whole
        SMI     1
        PLO     R9
        GLO     RC
        SDI     16
        STR     R2
        GLO     R9
        SM
        PLO     R9
        POKER   R9
        LDR     R8, HLINE
        LDI     16
        PLO     RC
hd2a:   LDA     R8
        PKD
        DEC     RC
        GLO     RC
        BNZ     hd2a
        PKEND
        SETV    TXM, TM_SET+TM_SHORT
        LDR     R8, HLINE
        LDI     0
        PLO     R9
        GETV    T3
        BZ      hd3
        LDI     HUD_Y2-HUD_Y1
hd3:    ADI     HUD_Y1
        PHI     R9
        FCALL   txt_at16
hd9:    FRET

; hud_bars: the energy under the name (line 113: 1 dot a 5 of 100, from x
;   4), the timer across (line 119: 4 dots a second, 15 s at most).
        RFIT    236
hud_bars:
        GETV    HUD_SEAT
        FCALL   seat_rec
        GLO     RB
        ADI     P_ENERGY
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDN     RB
        PHI     RC                      ; (energy / 5: 0-20)
        LDI     0
        PLO     RC
hb1:    GHI     RC
        SMI     5
        BNF     hb2
        PHI     RC
        INC     RC
        BR      hb1
hb2:    FCALL   row1_held               ; (row 1 held: no energy under it)
        BZ      hb2a
        LDI     0
        PLO     RC
hb2a:   GETS    OFF_TIMER               ; the timer: 4 a second, 60 at most
        SMI     15
        BNF     hb3
        LDI     0
hb3:    ADI     15
        SHL
        SHL
        PHI     RC
        GLO     RC                      ; the same as drawn: nothing
        STR     R2
        GETV    HBARE
        XOR
        BNZ     hb4
        GHI     RC
        STR     R2
        GETV    HBART
        XOR
        BZ      hb9
hb4:    GLO     RC
        PUTV    HBARE
        GHI     RC
        PUTV    HBART
        PARS                            ; the energy: line 113 cleared, then
        P       0                       ;  its bar
        P       HUD_Y1+5
        P       64
        P       1
        P       TM_CLEAR
        GOP     TOP_RECT
        GETV    HBARE
        BZ      hb5
        PARS
        P       4
        P       HUD_Y1+5
        GETV    HBARE
        PD
        P       1
        P       TM_SET
        GOP     TOP_RECT
hb5:    PARS                            ; the timer: line 119
        P       0
        P       HUD_Y2+5
        P       64
        P       1
        P       TM_CLEAR
        GOP     TOP_RECT
        GETV    HBART
        BZ      hb9
        PARS
        P       0
        P       HUD_Y2+5
        GETV    HBART
        PD
        P       1
        P       TM_SET+TP_DASH
        GOP     TOP_RECT
hb9:    FRET

; hud_clear: the shadows forgotten (the rows drawn whole next time).
        RFIT    65
hud_clear:
        POKEAT  HSHAD
        LDI     32
        PLO     R9
hcl1:   LDI     0
        PKD
        DEC     R9
        GLO     R9
        BNZ     hcl1
        PKEND
        SETV    HBARE, NONE
        SETV    HBART, NONE
        FRET
