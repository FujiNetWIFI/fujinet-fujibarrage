' ===========================================================================
' menus.bas -- the screens before a room, as src/menus.c: the title, the
' name (picked a letter at a time with the disc: the Intellivision has no
' keyboard; FujiBlast's picker), the rooms.
' ===========================================================================

    DIM mt

' title: the logo, two tanks, the march (T_TITLE), until fire or ENTER.
title: PROCEDURE
    GOSUB gfx_begin
    GOSUB gram_logo_in
    FOR i = 0 TO N_LOGO_CELLS - 1
        #a = logo_cell(i * 3)
        #BACKTAB(#a) = $0800 + (G_LOGO + logo_cell(i * 3 + 1)) * 8 + fgbg(logo_cell(i * 3 + 2), C_BLACK)
    NEXT i
    PRINT AT 162 COLOR CW_TAN, "ARTILLERY FOR TEN"
    PRINT AT 205 COLOR CW_WHITE, "PRESS FIRE"
    FOR i = 220 TO 239                  ' the ground
        #BACKTAB(i) = $0800 + G_LEVEL * 8 + fgbg(C_DKGREEN, C_BLACK)
    NEXT i
    #v = T_TITLE1 : #w = T_TITLE2 : GOSUB snd_tune
    mt = 0
    DO
        GOSUB tick
        mt = mt + 1
        ' the two tanks, their barrels swinging
        k = (mt / 32) AND 3
        IF k = 3 THEN k = 1
        SPRITE 0, 30 + 8 + MOB_VIS, 86 + 8 + MOB_ZY2, MOB_GRAM + (G_TANK + 2 + k) * 8 + seat_mob(0)
        SPRITE 1, 122 + 8 + MOB_VIS, 86 + 8 + MOB_ZY2 + MOB_FLIP, MOB_GRAM + (G_TANK + 2 + k) * 8 + seat_mob(3)
        IF ev_btn OR ev_key = KEY_RETURN THEN EXIT DO
    LOOP WHILE 1
    GOSUB snd_tune_off
    GOSUB mob_hide_all
END

' name_entry: MYNAME, 2-8 letters and digits. The disc's up and down pick
' the letter, right or fire takes it, left or CLEAR takes one back; a
' keypad digit is taken at once; ENTER ends it.
    DIM ne_n, ne_c, ne_rep
name_entry: PROCEDURE
    GOSUB gfx_begin
    PRINT AT 41 COLOR CW_YELLOW, "WHAT IS YOUR NAME?"
    PRINT AT 162 COLOR CW_TAN, "DISC UP/DOWN: PICK"
    PRINT AT 182 COLOR CW_TAN, "RIGHT/FIRE: TAKE"
    PRINT AT 200 COLOR CW_TAN, "LEFT/CLEAR: BACK"
    PRINT AT 222 COLOR CW_TAN, "ENTER: DONE"
    ne_n = 0 : ne_c = 65 : ne_rep = 0
    POKE MYNAME, 0
    prev_dir = 0
    DO
        r = 5 : GOSUB gfx_clear_row
        #a = MYNAME : n = NAME_LEN : #p = 106 : #cw = CW_WHITE : GOSUB gfx_cstr
        IF ne_n < NAME_LEN THEN
            IF #frame_count AND 16 THEN #BACKTAB(106 + ne_n) = (ne_c - 32) * 8 + CW_YELLOW ELSE #BACKTAB(106 + ne_n) = (45 - 32) * 8 + CW_YELLOW
        END IF
        GOSUB tick
        v = 0
        IF inp_dir AND (IN_UP + IN_DOWN) THEN
            IF (prev_dir AND (IN_UP + IN_DOWN)) = 0 THEN
                v = 1 : ne_rep = 18
            ELSE
                ne_rep = ne_rep - 1
                IF ne_rep = 0 THEN v = 1 : ne_rep = 5
            END IF
        END IF
        IF v THEN
            IF inp_dir AND IN_UP THEN GOSUB ne_next ELSE GOSUB ne_prev
            sid = UI_TICK : GOSUB snd_cue
        END IF
        v = IN_RIGHT : GOSUB pressed
        IF (v OR (ev_btn AND BTN_FIRE)) AND ne_n < NAME_LEN THEN
            POKE MYNAME + ne_n, ne_c : ne_n = ne_n + 1 : POKE MYNAME + ne_n, 0
            sid = UI_SELECT : GOSUB snd_cue
        END IF
        IF ev_key >= 48 AND ev_key <= 57 AND ne_n < NAME_LEN THEN
            POKE MYNAME + ne_n, ev_key : ne_n = ne_n + 1 : POKE MYNAME + ne_n, 0
            sid = UI_SELECT : GOSUB snd_cue
        END IF
        v = IN_LEFT : GOSUB pressed
        IF (v OR ev_key = KEY_ESC) AND ne_n > 0 THEN
            ne_n = ne_n - 1 : POKE MYNAME + ne_n, 0
        END IF
        prev_dir = inp_dir
        IF ev_key = KEY_RETURN AND ne_n >= 2 THEN EXIT DO
    LOOP WHILE 1
END

' the letters run A-Z then 0-9
ne_next: PROCEDURE
    ne_c = ne_c + 1
    IF ne_c = 91 THEN ne_c = 48
    IF ne_c = 58 THEN ne_c = 65
END
ne_prev: PROCEDURE
    ne_c = ne_c - 1
    IF ne_c = 64 THEN ne_c = 57
    IF ne_c = 47 THEN ne_c = 90
END

' room_list: the rooms on the server, one picked into SRVTBL (v = 1), or
' v = 0 (no answer). A room: its name, people / seats, waiting or the round.
    DIM rl_sel
room_list: PROCEDURE
    GOSUB gfx_begin
    PRINT AT 85 COLOR CW_YELLOW, "CONNECTING"
    #a = SRVHOST : n = 20 : r = 6 : #cw = CW_WHITE : GOSUB gfx_ccentre
    GOSUB net_rooms
    IF room_n = 0 THEN
        GOSUB net_close
        GOSUB msg_host
        v = 0
        RETURN
    END IF
    GOSUB gfx_begin
    r = 0 : v = C_BLUE : GOSUB gfx_bar
    PRINT AT 0 COLOR CB_YELLOW, "PICK A ROOM"
    FOR i = 0 TO room_n - 1
        #a = ROOMS + i * ROOM_REC + 8
        #p = (2 + i) * 20 + 1 : n = 12 : #cw = CW_WHITE : GOSUB gfx_cstrn
        #a = ROOMS + i * ROOM_REC
        #p = (2 + i) * 20 + 14
        #nv = rb(#a + 20) : #cw = CW_YELLOW : GOSUB gfx_num
        IF rb(#a + 22) = PH_WAIT THEN
            #BACKTAB((2 + i) * 20 + 17) = (87 - 32) * 8 + CW_TAN      ' W
        ELSE
            #BACKTAB((2 + i) * 20 + 17) = (82 - 32) * 8 + CW_RED      ' R(ound)
            #p = (2 + i) * 20 + 18 : #nv = rb(#a + 23) : #cw = CW_RED : GOSUB gfx_num
        END IF
    NEXT i
    r = 11 : v = C_BLUE : GOSUB gfx_bar
    PRINT AT 220 COLOR CB_WHITE, "DISC:PICK FIRE:JOIN"
    rl_sel = 0
    prev_dir = 0
    DO
        FOR i = 0 TO room_n - 1
            IF i = rl_sel THEN #BACKTAB((2 + i) * 20) = (62 - 32) * 8 + CW_YELLOW ELSE #BACKTAB((2 + i) * 20) = 0
        NEXT i
        GOSUB tick
        GOSUB net_poll
        IF net_r = NET_LOST THEN v = 0 : RETURN
        v = IN_UP : GOSUB pressed
        IF v AND rl_sel > 0 THEN rl_sel = rl_sel - 1 : sid = UI_TICK : GOSUB snd_cue
        v = IN_DOWN : GOSUB pressed
        IF v AND rl_sel + 1 < room_n THEN rl_sel = rl_sel + 1 : sid = UI_TICK : GOSUB snd_cue
        prev_dir = inp_dir
        IF (ev_btn AND BTN_FIRE) OR ev_key = KEY_RETURN THEN EXIT DO
    LOOP WHILE 1
    sid = UI_GO : GOSUB snd_cue
    FOR i = 0 TO ROOM_LEN - 1
        c = rb(ROOMS + rl_sel * ROOM_REC + i)
        IF c >= 65 AND c <= 90 THEN c = c + 32
        POKE SRVTBL + i, c
    NEXT i
    POKE SRVTBL + ROOM_LEN, 0
    v = 1
END
