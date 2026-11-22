' ===========================================================================
' screen.bas -- the text screens' helpers (scr.h's), after FujiBlast's
' draw.bas and screens.bas: 20x12 cards in foreground/background mode,
' the GROM's capitals (a card is (ASCII - 32) * 8 plus its colours:
' foreground 0-7, any background), a row's background its own (a bar);
' and the waits every screen uses.
' ===========================================================================

    CONST MOB_VIS  = $200       ' X: shown
    CONST MOB_ZX   = $400       ' X: twice as wide
    CONST MOB_ZY2  = $100       ' Y: a row two scanlines (a field pixel)
    CONST MOB_YRES = $80        ' Y: 16 rows (two cards)
    CONST MOB_FLIP = $400       ' Y: mirrored left to right
    CONST MOB_BEHIND = $2000    ' A: behind the cards' foreground (dirt hides a tank)
    CONST MOB_GRAM = $800       ' A: a GRAM card

    ' text colours on black (a GROM letter's card is (ASCII - 32) * 8 plus these)
    CONST CW_WHITE  = fgbg(C_WHITE, C_BLACK)
    CONST CW_YELLOW = fgbg(C_YELLOW, C_BLACK)
    CONST CW_TAN    = fgbg(C_TAN, C_BLACK)
    CONST CW_RED    = fgbg(C_RED, C_BLACK)
    CONST CW_GREEN  = fgbg(C_GREEN, C_BLACK)
    ' on the blue bars
    CONST CB_WHITE  = fgbg(C_WHITE, C_BLUE)
    CONST CB_YELLOW = fgbg(C_YELLOW, C_BLUE)
    CONST CB_TAN    = fgbg(C_TAN, C_BLUE)

    DIM #p, #cw                 ' where text goes, and its colour word
    DIM #nv                     ' gfx_num's number

' gfx_begin: a new screen: black, no MOBs.
gfx_begin: PROCEDURE
    CALL FLD_LEAVE                      ' (no field cell may land on it)
    GOSUB mob_hide_all
    CLS
END

' gfx_bar: row r all background colour v.
gfx_bar: PROCEDURE
    #w = fgbg(0, v)
    FOR i = 0 TO 19
        #BACKTAB(r * 20 + i) = #w
    NEXT i
END

' gfx_clear_row: row r black.
gfx_clear_row: PROCEDURE
    FOR i = 0 TO 19
        #BACKTAB(r * 20 + i) = 0
    NEXT i
END

' gfx_cstr: the 0-ended text at #a (cartridge RAM), n characters at most,
' at #p in colour #cw, in capitals (the GROM's are all it has in this mode).
gfx_cstr: PROCEDURE
    k = 0
    WHILE k < n
        c = rb(#a + k)
        IF c = 0 THEN EXIT WHILE
        IF c >= 96 THEN c = c - 32
        IF c < 32 THEN c = 32
        #BACKTAB(#p + k) = (c - 32) * 8 + #cw
        k = k + 1
    WEND
END

' gfx_cstrn: the same, but exactly n characters (the rest blank).
gfx_cstrn: PROCEDURE
    k = 0
    c = 1
    WHILE k < n
        IF c THEN c = rb(#a + k)
        IF c THEN
            t = c
            IF t >= 96 THEN t = t - 32
            IF t < 32 THEN t = 32
        ELSE
            t = 32
        END IF
        #BACKTAB(#p + k) = (t - 32) * 8 + #cw
        k = k + 1
    WEND
END

' gfx_clen: n = the length of the 0-ended text at #a (n at most).
gfx_clen: PROCEDURE
    k = 0
    WHILE k < n
        IF rb(#a + k) = 0 THEN EXIT WHILE
        k = k + 1
    WEND
    n = k
END

' gfx_ccentre: the text at #a (n at most) centred on row r, colour #cw.
gfx_ccentre: PROCEDURE
    GOSUB gfx_clen
    #p = r * 20 + (20 - n) / 2
    GOSUB gfx_cstr
END

' gfx_num: #nv in decimal at #p (left to right), colour #cw; n = its digits.
gfx_num: PROCEDURE
    n = 0
    k = 0                               ' a digit put yet
    FOR j = 0 TO 4
        #w = pow10(j)
        c = 0
        WHILE #nv >= #w
            #nv = #nv - #w : c = c + 1
        WEND
        IF c OR k OR j = 4 THEN
            #BACKTAB(#p + n) = (16 + c) * 8 + #cw
            n = n + 1
            k = 1
        END IF
    NEXT j
END

' gfx_numr: #nv right-aligned to end at #p (the last digit there).
gfx_numr: PROCEDURE
    #a = #nv
    j = 1
    WHILE #a >= 10
        #a = #a / 10 : j = j + 1
    WEND
    #p = #p - j + 1
    GOSUB gfx_num
END

' gfx_name: seat s's name (ST) at #p, colour #cw, padded to n.
gfx_name: PROCEDURE
    #a = ST + OFF_SEATS + s * SEAT_SIZE + P_NAME
    IF n > NAME_LEN THEN n = NAME_LEN
    GOSUB gfx_cstrn
END

' ---- the waits -------------------------------------------------------------------

' tick: a frame of a screen: wait, the presses, 0 mutes.
tick: PROCEDURE
    GOSUB frame_wait
    #seen = #jiffy
    #frame_count = #seen
    GOSUB input_take
    IF ev_key = KEY_MUTE THEN GOSUB snd_toggle_mute
END

' pressed: v = 1 if the disc went bit v (IN_*) this frame (not held).
pressed: PROCEDURE
    IF (inp_dir AND v) <> 0 AND (prev_dir AND v) = 0 THEN v = 1 ELSE v = 0
END

' rep: v = 1 if disc bit v went this frame, or has been held long enough
' to repeat (rep_n frames held so far: the caller zeroes it when let go).
    DIM rep_n
rep: PROCEDURE
    IF (inp_dir AND v) = 0 THEN v = 0 : RETURN
    IF (prev_dir AND v) = 0 THEN rep_n = 0 : v = 1 : RETURN
    rep_n = rep_n + 1
    IF rep_n >= 18 AND (rep_n AND 3) = 0 THEN v = 1 ELSE v = 0
END

' message_wait: the screen as the caller drew it, for msg_t frames, or
' (msg_t 0) until fire or ENTER, with PRESS FIRE on row 10.
    DIM msg_t
message_wait: PROCEDURE
    IF msg_t = 0 THEN PRINT AT 205 COLOR CW_WHITE, "PRESS FIRE"
    DO
        GOSUB tick
        IF msg_t THEN
            msg_t = msg_t - 1
            IF msg_t = 0 THEN EXIT DO
        ELSE
            IF ev_btn OR ev_key = KEY_RETURN THEN EXIT DO
        END IF
    LOOP WHILE 1
END

' msg_host: "NO ANSWER FROM" and the host, until fire.
msg_host: PROCEDURE
    GOSUB gfx_begin
    PRINT AT 83 COLOR CW_YELLOW, "NO ANSWER FROM"
    #a = SRVHOST : n = 20 : r = 6 : #cw = CW_WHITE : GOSUB gfx_ccentre
    msg_t = 0 : GOSUB message_wait
END

' ---- the MOBs ------------------------------------------------------------------------

mob_hide_all: PROCEDURE
    FOR i = 0 TO 7
        SPRITE i, 0, 0, 0
    NEXT i
END

' ---- the GRAM: the fixed cards; the logo over the pool's on the title -------------
    DIM gram_set                ' 1 the fixed cards are in, 2 and the logo
gram_fixed_in: PROCEDURE
    IF gram_set THEN RETURN
    DEFINE 0, N_FIXED, gram_fixed
    WAIT
    gram_set = 1
END

gram_logo_in: PROCEDURE
    GOSUB gram_fixed_in
    DEFINE G_LOGO, 16, gram_logo
    WAIT
    DEFINE G_LOGO + 16, N_LOGO - 16, VARPTR gram_logo(64)
    WAIT
    gram_set = 2
END
