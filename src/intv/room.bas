' ===========================================================================
' room.bas -- the screens in a room besides the field, as src/room.c: the
' waiting room (the host's settings on a second view), the shop, a round's
' end and the game's. Twenty columns: black lists between blue bars, a
' seat's swatch its colour, the row in hand on a blue bar with a ">".
' ===========================================================================

    DIM wt_set, wt_page, wt_sel, wt_n, wt_k     ' the settings view: on, its page, the row in hand, rows, a setting
    DIM sh_n, sh_sel, sh_top, sh_i              ' the shop: items listed, the one in hand, the first shown
    CONST SET_KS   = TRACKS         ' 16: the page's settings (TRACKS is free out of a shot)
    CONST SHOP_IDS = TRACKS + 16    ' 48: the items the arms level allows
    CONST SHOP_OWN = TRACKS + 64    ' 48: the stock of each as shown
    CONST SHOP_ROWS = 9

' seat_kind: seat s's kind (KIND_*)
    DEF FN seat_kind(x) = rb(ST + OFF_SEATS + (x) * SEAT_SIZE + P_KIND)

' hl_row: row r, the row in hand (v not 0: a blue bar and a ">", #cw the
' blue's) or not (black, #cw black's); #cx the text's colour: white
hl_row: PROCEDURE
    IF v THEN
        v = C_BLUE : GOSUB gfx_bar
        #BACKTAB(r * 20) = (62 - 32) * 8 + CB_YELLOW
        #cw = CB_WHITE
    ELSE
        GOSUB gfx_clear_row
        #cw = CW_WHITE
    END IF
END

' room_head: row 0, the room: its id and (if the list had it) its name
room_head: PROCEDURE
    r = 0 : v = C_BLUE : GOSUB gfx_bar
    #a = SRVTBL : n = ROOM_LEN : #p = 0 : #cw = CB_YELLOW : GOSUB gfx_cstr
    j = n + 1
    FOR i = 0 TO MAX_ROOMS - 1
        IF i < room_n THEN
            t = 1
            FOR k = 0 TO ROOM_LEN - 1
                c = rb(ROOMS + i * ROOM_REC + k)
                IF c >= 65 AND c <= 90 THEN c = c + 32
                IF c <> rb(SRVTBL + k) THEN t = 0
                IF c = 0 THEN EXIT FOR
            NEXT k
            IF t THEN
                #a = ROOMS + i * ROOM_REC + 8 : n = 20 - j : #p = j : #cw = CB_YELLOW : GOSUB gfx_cstr
            END IF
        END IF
    NEXT i
END

' seat_row: seat s on row r: its swatch, its name, what it is (in the
' waiting room) or its score and wins (after a round)
seat_row: PROCEDURE
    #BACKTAB(r * 20) = fgbg(0, seat_col(s))
    #BACKTAB(r * 20 + 1) = 0
    c = seat_kind(s)
    IF c = KIND_EMPTY THEN
        PRINT AT r * 20 + 2 COLOR CW_TAN, "- OPEN -          "
        RETURN
    END IF
    #p = r * 20 + 2 : n = NAME_LEN : #cw = CW_WHITE : GOSUB gfx_name
    #p = r * 20 + 10 : #BACKTAB(#p) = 0
    IF rb(ST + OFF_PHASE) <> PH_WAIT THEN
        #nv = pl16(s, P_SCORE) : #p = r * 20 + 16 : #cw = CW_TAN : GOSUB gfx_numr
        #nv = pl(s, P_WINS) : #p = r * 20 + 19 : #cw = CW_YELLOW : GOSUB gfx_numr
    ELSEIF s = rb(ST + OFF_HOST) THEN
        PRINT AT r * 20 + 11 COLOR CW_YELLOW, "HOST     "
    ELSEIF c = KIND_BOT THEN
        i = pl(s, P_TIER) : n = 9 : GOSUB cat_tier_name
        #a = STXT : n = 9 : #p = r * 20 + 11 : #cw = CW_TAN : GOSUB gfx_cstrn
    ELSE
        PRINT AT r * 20 + 11 COLOR CW_WHITE, "PLAYER   "
    END IF
END

' ---- the waiting room ------------------------------------------------------

wait_enter: PROCEDURE
    GOSUB snd_tune_off
    GOSUB gfx_begin
    wt_set = 0 : wt_page = 0 : wt_sel = 0
    GOSUB wait_draw
END

' wait_draw: the whole screen (the seats, or the settings of a page)
wait_draw: PROCEDURE
    IF wt_set = 0 THEN
        GOSUB room_head
        GOSUB wait_state
        RETURN
    END IF
    r = 0 : v = C_BLUE : GOSUB gfx_bar
    PRINT AT 0 COLOR CB_YELLOW, "SETUP"
    #BACKTAB(6) = (17 + wt_page) * 8 + CB_YELLOW
    #BACKTAB(7) = (47 - 32) * 8 + CB_YELLOW
    n = USR CATB(3) : #BACKTAB(8) = (16 + n) * 8 + CB_YELLOW
    IF cat_ok THEN
        #ca = PEEK(CI_PAGE + wt_page) : n = 10 : GOSUB cat_str
        #a = STXT : #p = 10 : #cw = CB_YELLOW : GOSUB gfx_cstr
    END IF
    wt_n = 0                            ' the page's settings
    IF cat_ok THEN
        FOR wt_k = 0 TO N_SETTINGS - 1
            IF USR CATB(PEEK(CI_SET + wt_k)) = wt_page AND wt_n < 10 THEN
                POKE SET_KS + wt_n, wt_k : wt_n = wt_n + 1
            END IF
        NEXT wt_k
    END IF
    IF wt_sel >= wt_n THEN wt_sel = 0
    FOR sh_i = 0 TO 9
        GOSUB set_line
    NEXT sh_i
    GOSUB set_foot
END

' set_line: setting row sh_i: its label, its value right to the edge
set_line: PROCEDURE
    r = 1 + sh_i : v = (sh_i = wt_sel) AND (wt_n <> 0) : GOSUB hl_row
    IF sh_i >= wt_n THEN RETURN
    wt_k = PEEK(SET_KS + sh_i)
    k = wt_k : n = 11 : GOSUB cat_label
    #a = STXT : #p = r * 20 + 1 : GOSUB gfx_cstr
    k = wt_k : v = rb(ST + OFF_SETTINGS + wt_k) : GOSUB cat_value
    IF n > 8 THEN n = 8
    #a = STXT : #p = r * 20 + 20 - n
    IF sh_i = wt_sel THEN #cw = CB_YELLOW ELSE #cw = CW_TAN
    GOSUB gfx_cstr
END

' set_foot: row 11: the host changes the row in hand; R back to the seats
set_foot: PROCEDURE
    r = 11 : v = C_BLUE : GOSUB gfx_bar
    IF my_seat = rb(ST + OFF_HOST) AND wt_n THEN
        PRINT AT 220 COLOR CB_WHITE, "<> CHANGE  L PAGE"
    ELSE
        PRINT AT 220 COLOR CB_WHITE, "L PAGE  R SEATS"
    END IF
END

wait_state: PROCEDURE
    IF wt_set THEN
        FOR sh_i = 0 TO wt_n - 1
            GOSUB set_line
        NEXT sh_i
        RETURN
    END IF
    FOR s = 0 TO SEATS - 1
        r = 1 + s : GOSUB seat_row
    NEXT s
    r = 11 : v = C_BLUE : GOSUB gfx_bar
    IF my_seat = rb(ST + OFF_HOST) THEN
        IF rb(ST + OFF_ROOM_FLAGS) = ROOM_PEOPLE THEN
            PRINT AT 220 COLOR CB_WHITE, "FIRE:START 2+ PEOPLE"
        ELSE
            PRINT AT 220 COLOR CB_WHITE, "FIRE:START R:SETUP"
        END IF
    ELSE
        PRINT AT 220 COLOR CB_WHITE, "WAITING  R:SETUP"
    END IF
END

wait_frame: PROCEDURE
    IF ev_btn AND BTN_ITEMS THEN        ' R: the seats, or the settings
        wt_set = wt_set XOR 1
        sid = UI_TICK : GOSUB snd_cue
        GOSUB gfx_begin
        GOSUB wait_draw
        RETURN
    END IF
    IF wt_set THEN
        IF ev_btn AND BTN_NEXT THEN     ' L: the next page
            wt_page = wt_page + 1
            IF wt_page >= USR CATB(3) THEN wt_page = 0
            wt_sel = 0
            sid = UI_TICK : GOSUB snd_cue
            GOSUB wait_draw
            RETURN
        END IF
        IF wt_n = 0 THEN RETURN
        v = IN_UP : GOSUB rep
        IF v AND wt_sel > 0 THEN sh_i = wt_sel : wt_sel = wt_sel - 1 : GOSUB set_line : sh_i = wt_sel : GOSUB set_line : sid = UI_TICK : GOSUB snd_cue
        v = IN_DOWN : GOSUB rep
        IF v AND wt_sel + 1 < wt_n THEN sh_i = wt_sel : wt_sel = wt_sel + 1 : GOSUB set_line : sh_i = wt_sel : GOSUB set_line : sid = UI_TICK : GOSUB snd_cue
        IF my_seat = rb(ST + OFF_HOST) THEN
            v = IN_LEFT : GOSUB pressed
            t = v
            v = IN_RIGHT : GOSUB pressed
            IF t OR v THEN              ' the value: one less or more, round
                wt_k = PEEK(SET_KS + wt_sel)
                k = wt_k : GOSUB cat_count : n = v
                c = rb(ST + OFF_SETTINGS + wt_k)
                IF t THEN
                    IF c THEN c = c - 1 ELSE c = n - 1
                ELSE
                    IF c + 1 < n THEN c = c + 1 ELSE c = 0
                END IF
                POKE RAWB + 3, wt_k : POKE RAWB + 4, c
                na_act = ACT_SET : na_n = 2 : GOSUB net_act
                sid = UI_TICK : GOSUB snd_cue
            END IF
        END IF
        prev_dir = inp_dir
    END IF
    IF (ev_btn AND BTN_FIRE) AND my_seat = rb(ST + OFF_HOST) AND my_seat <> NONE THEN
        na_act = ACT_START : na_n = 0 : GOSUB net_act
        sid = UI_GO : GOSUB snd_cue
    END IF
END

' ---- the shop ---------------------------------------------------------------

' shop_line: list entry sh_i on its row (if shown): its name, how many we have
shop_line: PROCEDURE
    IF sh_i < sh_top OR sh_i >= sh_top + SHOP_ROWS THEN RETURN
    r = 1 + sh_i - sh_top : v = (sh_i = sh_sel) : GOSUB hl_row
    IF sh_i >= sh_n THEN RETURN
    i = PEEK(SHOP_IDS + sh_i) : n = 16 : GOSUB cat_item_name
    #a = STXT : #p = r * 20 + 1 : GOSUB gfx_cstr
    i = PEEK(SHOP_IDS + sh_i)
    c = inv(i) : POKE SHOP_OWN + i, c
    IF c THEN
        #nv = c : #p = r * 20 + 19
        IF sh_i = sh_sel THEN #cw = CB_YELLOW ELSE #cw = CW_YELLOW
        GOSUB gfx_numr
    END IF
END

' shop_info: row 10, the item in hand: its price, its bundle, how many we have
shop_info: PROCEDURE
    r = 10 : GOSUB gfx_clear_row
    IF sh_n = 0 OR cat_ok = 0 THEN RETURN
    #ca = PEEK(CI_ITEM + PEEK(SHOP_IDS + sh_sel))
    #BACKTAB(200) = (36 - 32) * 8 + CW_TAN       ' $
    #cv_lo = USR CATB(#ca + 3) + USR CATB(#ca + 4) * 256    ' (a USR in another's arguments
    #cv_hi = USR CATB(#ca + 5)                              ' would clobber them)
    n = USR NUM32(#cv_lo, #cv_hi)
    #a = STXT : #p = 201 : #cw = CW_TAN : GOSUB gfx_cstr
    #p = 202 + n
    #BACKTAB(#p) = (88 - 32) * 8 + CW_TAN        ' X
    #nv = USR CATB(#ca + 2) : #p = #p + 1 : GOSUB gfx_num
    PRINT AT 211 COLOR CW_TAN, "OWN"
    #nv = inv(PEEK(SHOP_IDS + sh_sel)) : #p = 215 : GOSUB gfx_num
END

' shop_all: the list's rows
shop_all: PROCEDURE
    FOR sh_i = sh_top TO sh_top + SHOP_ROWS - 1
        GOSUB shop_line
    NEXT sh_i
    GOSUB shop_info
END

shop_enter: PROCEDURE
    GOSUB gfx_begin
    sh_n = 0 : sh_sel = 0 : sh_top = 0
    IF cat_ok THEN
        FOR sh_i = 0 TO N_ITEMS - 1
            IF sh_i <> I_MINI_MISSILE AND USR CATB(PEEK(CI_ITEM + sh_i) + 1) <= rb(ST + OFF_SETTINGS + SET_ARMS) THEN
                POKE SHOP_IDS + sh_n, sh_i : sh_n = sh_n + 1
            END IF
        NEXT sh_i
    END IF
    GOSUB shop_all
    GOSUB shop_state
    #v = T_SHOP1 : #w = T_SHOP2 : GOSUB snd_tune
END

' shop_state: the cash and the clock; the rows whose stock changed
shop_state: PROCEDURE
    r = 0 : v = C_BLUE : GOSUB gfx_bar
    PRINT AT 0 COLOR CB_YELLOW, "SHOP"
    #nv = rb(ST + OFF_ROUND) + 1 : #p = 5 : #cw = CB_YELLOW : GOSUB gfx_num
    #BACKTAB(9) = (36 - 32) * 8 + CB_YELLOW      ' $
    n = USR NUM32(rb(ST + OFF_CASH) + rb(ST + OFF_CASH + 1) * 256, rb(ST + OFF_CASH + 2) + rb(ST + OFF_CASH + 3) * 256)
    #a = STXT : #p = 10 : GOSUB gfx_cstr
    FOR sh_i = sh_top TO sh_top + SHOP_ROWS - 1
        IF sh_i < sh_n THEN
            IF PEEK(SHOP_OWN + PEEK(SHOP_IDS + sh_i)) <> inv(PEEK(SHOP_IDS + sh_i)) THEN GOSUB shop_line : GOSUB shop_info
        END IF
    NEXT sh_i
    r = 11 : v = C_BLUE : GOSUB gfx_bar
    IF my_seat < SEATS THEN
        IF pl(my_seat, P_FLAGS) AND PF_DONE THEN
            PRINT AT 220 COLOR CB_WHITE, "DONE: WAITING"
        ELSE
            PRINT AT 220 COLOR CB_WHITE, "FIRE BUY  L SELL"
        END IF
    END IF
    #nv = rb(ST + OFF_TIMER) : #p = 239 : #cw = CB_YELLOW : GOSUB gfx_numr
END

shop_frame: PROCEDURE
    IF my_seat >= SEATS OR sh_n = 0 THEN RETURN
    IF pl(my_seat, P_FLAGS) AND PF_DONE THEN RETURN
    sh_i = sh_sel
    v = IN_UP : GOSUB rep
    IF v AND sh_sel > 0 THEN sh_sel = sh_sel - 1
    v = IN_DOWN : GOSUB rep
    IF v AND sh_sel + 1 < sh_n THEN sh_sel = sh_sel + 1
    prev_dir = inp_dir
    IF sh_sel <> sh_i THEN
        sid = UI_TICK : GOSUB snd_cue
        IF sh_sel < sh_top THEN
            sh_top = sh_sel : GOSUB shop_all
        ELSEIF sh_sel >= sh_top + SHOP_ROWS THEN
            sh_top = sh_sel - SHOP_ROWS + 1 : GOSUB shop_all
        ELSE
            GOSUB shop_line
            sh_i = sh_sel : GOSUB shop_line
            GOSUB shop_info
        END IF
    END IF
    i = PEEK(SHOP_IDS + sh_sel)
    POKE RAWB + 3, i : POKE RAWB + 4, 1
    IF ev_btn AND BTN_FIRE THEN         ' (the STATE's cash says whether it went)
        na_act = ACT_BUY : na_n = 2 : GOSUB net_act
        sid = UI_SELECT : GOSUB snd_cue
    END IF
    IF (ev_btn AND BTN_NEXT) AND inv(i) <> 0 THEN
        na_act = ACT_SELL : na_n = 2 : GOSUB net_act
        sid = UI_TICK : GOSUB snd_cue
    END IF
    IF ev_key = KEY_RETURN THEN
        na_act = ACT_DONE : na_n = 0 : GOSUB net_act
        sid = UI_GO : GOSUB snd_cue
    END IF
END

' ---- a round's end, the game's ---------------------------------------------------

results_enter: PROCEDURE
    IF res_final THEN #v = T_OVER1 : #w = T_OVER2 ELSE #v = T_WIN1 : #w = T_WIN2
    GOSUB snd_tune
    GOSUB gfx_begin
    r = 0 : v = C_BLUE : GOSUB gfx_bar
    IF res_final THEN
        PRINT AT 0 COLOR CB_YELLOW, "GAME OVER"
    ELSE
        PRINT AT 0 COLOR CB_YELLOW, "ROUND"
        #nv = rb(ST + OFF_ROUND) : #p = 6 : #cw = CB_YELLOW : GOSUB gfx_num
        PRINT AT 7 + n COLOR CB_YELLOW, "OF"
        #nv = rb(ST + OFF_ROUNDS) : #p = 10 + n : GOSUB gfx_num
    END IF
    s = rb(ST + OFF_RESULT)
    IF s < SEATS THEN
        r = 1 : v = seat_col(s) : GOSUB gfx_bar
        #cw = fgbg(seat_txt(s), seat_col(s))
        #p = 20 : n = NAME_LEN : GOSUB gfx_name
        PRINT AT 29 COLOR #cw, "WINS!"
    END IF
    r = 2
    FOR s = 0 TO SEATS - 1
        IF seat_kind(s) <> KIND_EMPTY THEN GOSUB seat_row : r = r + 1
    NEXT s
END

results_frame: PROCEDURE
END
