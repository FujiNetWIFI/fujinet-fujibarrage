' ===========================================================================
' turn.bas -- what stands out (play.bas's, in segment 1: segment 2 is full):
' our turn (the cue, YOUR TURN! flashing, our tank blinking, the cue again
' when idle), the defence window (its items line opened by itself, its
' prompt), and a tank's talk scrolling through HUD row 1.
' ===========================================================================

' ---- your turn, the defence window: what stands out ----------------------

' def_row0: row 0 in the defence window: SET UP DEFENCES and how, by turns
' (32 jiffies each), and the seconds left
def_row0: PROCEDURE
    r = 0 : GOSUB gfx_clear_row
    IF #frame_count AND 32 THEN
        PRINT AT 0 COLOR CW_YELLOW, "R:ITEMS ENTER:DONE"
    ELSE
        PRINT AT 0 COLOR CW_WHITE, "SET UP DEFENCES"
    END IF
    #nv = rb(ST + OFF_TIMER) : #p = 19 : #cw = CW_YELLOW : GOSUB gfx_numr
END

' banner_row1: YOUR TURN! on row 1, white and yellow by turns (16 jiffies)
banner_row1: PROCEDURE
    r = 1 : GOSUB gfx_clear_row
    IF #frame_count AND 16 THEN
        PRINT AT 25 COLOR CW_YELLOW, "YOUR TURN!"
    ELSE
        PRINT AT 25 COLOR CW_WHITE, "YOUR TURN!"
    END IF
END

' turn_check: our turn begins: the aim as the server has it, the cue, the
' banner, our tank blinking (mob.asm's MB_ME)
turn_check: PROCEDURE
    GOSUB is_my_turn
    IF v AND my_turn = 0 THEN
        aim_a = pl(my_seat, P_ANGLE)
        #aim_p = pl16(my_seat, P_POWER)
        weapon = pl(my_seat, P_WEAPON)
        sent_a = aim_a : #sent_p = #aim_p
        sid = UI_TURN : GOSUB snd_cue
        POKE TURN_AT, #frame_count : POKE IDLE_AT, #frame_count
        POKE BAN_ON, 1
    END IF
    my_turn = v
    IF my_turn THEN POKE MB_ME, my_seat + 1 ELSE POKE MB_ME, 0
END

' turn_frame: a frame of our turn and the defence window: the banner's and
' the prompt's flashing, the defence window's items line opened by itself,
' the cue again after 10 seconds idle
turn_frame: PROCEDURE
    IF PEEK(BAN_ON) = 1 THEN
        #v = #frame_count - PEEK(TURN_AT)
        IF my_turn = 0 OR #v >= 180 THEN
            POKE BAN_ON, 0
            IF talk_left = 0 AND menu = 0 THEN GOSUB hud
        ELSEIF (#frame_count AND 15) = 0 AND talk_left = 0 AND menu = 0 THEN
            GOSUB banner_row1
        END IF
    END IF
    GOSUB defending
    IF v THEN
        IF PEEK(DEF_OPEN) = 0 THEN
            POKE DEF_OPEN, 1
            sid = UI_TURN : GOSUB snd_cue
            item = N_ITEMS - 1 : d = 1 : GOSUB next_item
            IF item <> I_NONE THEN menu = M_ITEM : prev_dir = inp_dir
            GOSUB hud
        ELSEIF (#frame_count AND 31) = 0 THEN
            GOSUB def_row0
        END IF
    ELSE
        POKE DEF_OPEN, 0
    END IF
    IF my_turn THEN
        IF inp_dir OR ev_btn OR ev_key THEN
            POKE IDLE_AT, #frame_count
        ELSE
            #v = #frame_count - PEEK(IDLE_AT)
            IF #v >= 600 THEN
                POKE IDLE_AT, #frame_count
                sid = UI_TURN : GOSUB snd_cue
            END IF
        END IF
    END IF
END

' talk_show: the phrase in HUD row 1: "NAME: phrase", its first 20
' characters at once; then, if it is longer, it scrolls a character every 4
' jiffies until its end is in the last cell, and stays a second (talk_tick)
talk_show: PROCEDURE
    s = PEEK(PL_TALK_S)
    IF PEEK(TK_ON + s) = 0 THEN RETURN
    sid = UI_TALK : GOSUB snd_cue
    #a = ST + OFF_SEATS + s * SEAT_SIZE + P_NAME : n = NAME_LEN : GOSUB gfx_clen
    POKE TALK_S, s : POKE TALK_L, n : POKE TALK_K, 0
    #a = PEEK(PL_TALK_P) : n = PEEK(PL_TALK_N)
    IF n > 40 THEN n = 40
    k = 0
    WHILE k < n
        POKE TALK_TXT + k, rb(#a + k)
        k = k + 1
    WEND
    POKE TALK_N, n
    c = PEEK(TALK_L) + 2 + n
    IF c > 20 THEN talk_left = 30 + (c - 20) * 4 + 60 ELSE talk_left = 120
    GOSUB talk_draw
END

' talk_tick: the scroll, from what is left of the phrase's time
talk_tick: PROCEDURE
    c = PEEK(TALK_L) + 2 + PEEK(TALK_N)
    IF c <= 20 THEN RETURN
    c = c - 20                          ' (its steps)
    #v = 90 + c * 4 - talk_left         ' (jiffies since it came)
    IF #v <= 30 THEN RETURN
    #v = (#v - 30) / 4
    IF #v > c THEN #v = c
    IF #v <> PEEK(TALK_K) THEN POKE TALK_K, #v : GOSUB talk_draw
END

' talk_draw: row 1 from character TALK_K of "NAME: phrase"
talk_draw: PROCEDURE
    n = PEEK(TALK_L)
    t = s : s = PEEK(TALK_S) : GOSUB seat_cw : #v = #cw : s = t    ' (the name: its seat's colour)
    #a = ST + OFF_SEATS + PEEK(TALK_S) * SEAT_SIZE + P_NAME
    FOR k = 0 TO 19
        c = PEEK(TALK_K) + k
        #cw = #v
        IF c < n THEN
            t = rb(#a + c)
        ELSEIF c = n THEN
            t = 58                      ' :
        ELSE
            t = 32
            #cw = CW_WHITE
            IF c >= n + 2 THEN
                i = c - n - 2
                IF i < PEEK(TALK_N) THEN t = PEEK(TALK_TXT + i)
            END IF
        END IF
        IF t >= 96 THEN t = t - 32
        IF t < 32 THEN t = 32
        #BACKTAB(20 + k) = (t - 32) * 8 + #cw
    NEXT k
END
