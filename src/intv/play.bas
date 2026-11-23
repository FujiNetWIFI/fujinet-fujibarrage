' ===========================================================================
' play.bas -- the field (src/play.c's): the tanks as the STATE has them,
' the HUD's two rows, aiming on our turn, and each shot's SCRIPT played.
'
' The field's cells are fld.asm's and cell.asm's (RENDER draws what
' changed, a few cells a frame). The tanks are MOBs: a body 7 x 5 and its
' barrel (five angles, mirrored past 90), behind the cards' foreground so
' dirt hides a buried tank. There are 8 MOBs and up to 10 tanks: with
' more than 8 alive, the least important share the last MOBs, a frame
' each (they blink at 30 Hz); ours, the shooter's and the turn's never do.
'
' HUD: row 0 the seat's name, angle, power and energy on its colour; row
' 1 its weapon and how many, the wind and the timer, or a message.
' ===========================================================================

    CONST RENDER_N = 12         ' cells drawn a frame at most (out of a shot)
    DIM fsky, my_turn, aim_a, weapon, held_n, sent_a, last_send, guide, target
    DIM hud_s, fire_k, mob_n, d, talk_left, whistle
    DIM #aim_p, #sent_p
    CONST M_OFF    = 0          ' the items line (items.bas): shut, the items, a target, driving
    CONST M_ITEM   = 1
    CONST M_TARGET = 2
    CONST M_DRIVE  = 3
    DIM menu, item, drive_n

    DEF FN pl(x, f) = USR PLB(x, f)
    DEF FN pl16(x, f) = USR PLW(x, f)
    DEF FN inv(x) = rb(ST + OFF_INV + (x))

' the tanks as drawn (TANKS, cartridge RAM: mob.asm and ply.asm read them too)
    CONST TK_ON = TANKS         ' 10 each: alive and shown,
    CONST TK_X  = TANKS + 10    ' x (its bottom centre),
    CONST TK_Y  = TANKS + 20    ' y,
    CONST TK_A  = TANKS + 30    ' the barrel's angle,
    CONST TK_E  = TANKS + 40    ' energy,
    CONST TK_CH = TANKS + 50    ' falling on its parachute
    CONST TK_MX = TANKS + 60    ' its MOB's X, Y and A (mob.asm's)
    CONST TK_MY = TANKS + 70
    CONST TK_MA = TANKS + 80
    CONST MB_ORD = TANKS + 90   ' 10: the seats in order of importance (mob.asm's)
    CONST TK_FT = TANKS + 100   ' a fall: frames left, y (8.8), its step a frame, where it ends
    CONST TK_FY = TANKS + 110
    CONST TK_FD = TANKS + 120
    CONST TK_FE = TANKS + 130
    CONST TK_BURN = TANKS + 140 ' frames of burning (a death), of a shield's flash
    CONST TK_FL = TANKS + 150

' ---- the field opened ------------------------------------------------------

' field_open: this round's sky and dirt colours, then the field on the
' screen anew (FLD_OPEN: every cell drawn again over the next frames).
field_open: PROCEDURE
    fsky = rb(ST + OFF_SKY)
    IF fsky >= N_SKIES THEN fsky = 0
    c = dirt_col(rb(ST + OFF_PALETTE) AND 7)
    POKE FV_DIRTFG, c
    POKE FV_DIRTBG, fgbg(0, c)
    POKE FV_DIRTW, $800 + G_SOLID * 8 + c
    IF c = C_WHITE THEN POKE FV_FIREALT, C_RED ELSE POKE FV_FIREALT, C_WHITE
    fire_k = 0
    POKE FV_FIREFG, fire_fg(0)
    i = 0
    FOR r = 0 TO 9
        v = sky_col(fsky * 10 + r)
        #w = fgbg(0, v)
        FOR k = 0 TO 19
            #v = #w
            IF fsky = SKY_STARS AND r < STAR_ROWS THEN
                IF (k * 37 + r * 101 + k * r * 13) % 7 < 2 THEN
                    #v = $800 + (G_STAR + 1 - ((k + r) AND 1)) * 8 + fgbg(C_WHITE, v)
                END IF
            END IF
            POKE SKYW + i, #v
            i = i + 1
        NEXT k
    NEXT r
    CALL FLD_OPEN
END

' field_loading: a whole field is coming (TERRAIN base FF): nothing to do
' here, the planes aren't the screen (a text screen stays as it is).
field_loading: PROCEDURE
END

' field_draw: a frame's drawing: cells, the fire's colour every 4 frames,
' cells the pool had no card for, the tanks.
field_draw: PROCEDURE
    CALL RENDER(RENDER_N)
    IF (#frame_count AND 3) = 0 THEN
        fire_k = fire_k + 1
        IF fire_k >= N_FIRE THEN fire_k = 0
        CALL FIRE_CYCLE(fire_fg(fire_k))
        CALL RETRY
    END IF
    GOSUB tanks_draw
END

' ---- the tanks ---------------------------------------------------------------

' tanks_from_state: the tanks as the STATE has them
tanks_from_state: PROCEDURE
    FOR s = 0 TO SEATS - 1
        POKE TK_ON + s, 0
        IF pl(s, P_KIND) <> KIND_EMPTY THEN
            IF pl(s, P_FLAGS) AND PF_ALIVE THEN POKE TK_ON + s, 1
        END IF
        #v = pl16(s, P_X)
        IF #v > 159 THEN #v = 159
        POKE TK_X + s, #v
        POKE TK_Y + s, pl(s, P_Y)
        POKE TK_A + s, pl(s, P_ANGLE)
        POKE TK_E + s, pl(s, P_ENERGY)
        POKE TK_CH + s, 0
    NEXT s
    IF my_turn THEN POKE TK_A + my_seat, aim_a
    POKE MB_DIRTY, 1
END

' tanks_draw: the MOBs for this frame (mob.asm)
tanks_draw: PROCEDURE
    CALL TANKS_MOB
END

' ---- the HUD ---------------------------------------------------------------------

' hud: both rows (the turn's seat, or ours)
hud: PROCEDURE
    hud_s = rb(ST + OFF_TURN)
    IF hud_s >= SEATS THEN
        IF my_seat < SEATS THEN hud_s = my_seat ELSE hud_s = 0
    END IF
    s = hud_s
    GOSUB defending
    IF v THEN
        GOSUB def_row0                  ' (the defence window: what to do)
    ELSE
        r = 0 : v = seat_col(s) : GOSUB gfx_bar
        #cw = fgbg(seat_txt(s), seat_col(s))
        #p = 0 : n = NAME_LEN : GOSUB gfx_name
        IF my_turn AND s = my_seat THEN #nv = aim_a ELSE #nv = pl(s, P_ANGLE)
        #p = 10 : GOSUB gfx_numr
        IF my_turn AND s = my_seat THEN #nv = #aim_p ELSE #nv = pl16(s, P_POWER)
        #p = 15 : GOSUB gfx_numr
        #nv = PEEK(TK_E + s)
        #p = 19 : GOSUB gfx_numr
    END IF
    IF talk_left THEN RETURN            ' (row 1: a phrase)
    IF menu THEN GOSUB items_line : RETURN
    GOSUB defending
    IF v THEN
        r = 1 : GOSUB gfx_clear_row
        PRINT AT 20 COLOR CW_YELLOW, "R:ITEMS  ENTER:DONE"
        RETURN
    END IF
    IF PEEK(BAN_ON) = 1 THEN GOSUB banner_row1 : RETURN
    r = 1 : GOSUB gfx_clear_row
    IF my_turn AND s = my_seat THEN i = weapon ELSE i = pl(s, P_WEAPON)
    IF i >= N_ITEMS THEN i = 0
    n = 10 : GOSUB cat_item_name
    #a = STXT : #p = 20 : #cw = CW_WHITE : GOSUB gfx_cstr
    IF s = my_seat AND i <> I_MINI_MISSILE THEN
        #nv = inv(i) : #p = 31 : GOSUB gfx_numr
    END IF
    #v = rb(ST + OFF_WIND) + rb(ST + OFF_WIND + 1) * 256
    IF #v AND $8000 THEN
        #BACKTAB(33) = (60 - 32) * 8 + CW_WHITE       ' <
        #nv = -#v
    ELSE
        IF #v THEN #BACKTAB(33) = (62 - 32) * 8 + CW_WHITE    ' >
        #nv = #v
    END IF
    #p = 35 : GOSUB gfx_numr
    #nv = rb(ST + OFF_TIMER) : #p = 39 : #cw = CW_YELLOW : GOSUB gfx_numr
END

' ---- entering the field, and the STATE changing ----------------------------

play_enter: PROCEDURE
    GOSUB gfx_begin
    GOSUB gram_fixed_in
    in_script = 0
    my_turn = 0
    POKE BAN_ON, 0 : POKE DEF_OPEN, 0 : POKE MB_ME, 0
    menu = 0 : guide = I_NONE : target = NO_SEAT
    GOSUB field_open
    GOSUB tanks_from_state
    GOSUB turn_check                    ' (a round may open on our turn)
    GOSUB hud
END

is_my_turn: PROCEDURE
    v = 0
    IF my_seat >= SEATS OR rb(ST + OFF_PHASE) <> PH_AIM THEN RETURN
    IF (pl(my_seat, P_FLAGS) AND PF_ALIVE) = 0 THEN RETURN
    t = rb(ST + OFF_TURN)
    IF t = my_seat THEN v = 1 : RETURN
    IF t = NO_SEAT AND (pl(my_seat, P_FLAGS) AND PF_DONE) = 0 THEN v = 1
END

play_state: PROCEDURE
    IF in_script THEN RETURN
    GOSUB turn_check
    GOSUB tanks_from_state
    net_terrain = 0
    GOSUB hud
END

' ---- aiming ---------------------------------------------------------------

next_weapon: PROCEDURE
    i = weapon
    DO
        i = i + 1
        IF i >= N_WEAPONS THEN i = 0
        IF i = I_MINI_MISSILE OR inv(i) <> 0 OR i = weapon THEN EXIT DO
    LOOP WHILE 1
    weapon = i
    sid = UI_SELECT : GOSUB snd_cue
    POKE RAWB + 3, weapon
    na_act = ACT_WEAPON : na_n = 1 : GOSUB net_act
END

play_frame: PROCEDURE
    IF net_terrain THEN GOSUB tanks_from_state : net_terrain = 0
    GOSUB field_draw
    GOSUB turn_frame
    GOSUB defending
    IF my_turn = 0 AND v = 0 THEN
        IF menu THEN GOSUB items_close
        RETURN
    END IF
    IF v AND ev_key = KEY_RETURN THEN   ' the defence window: done
        menu = M_OFF
        na_act = ACT_DONE : na_n = 0 : GOSUB net_act
        sid = UI_GO : GOSUB snd_cue
        GOSUB hud
        RETURN
    END IF
    IF menu THEN GOSUB items_frame : RETURN
    IF ev_btn AND BTN_ITEMS THEN GOSUB items_open : RETURN
    IF v THEN RETURN                    ' (the defence window: items only)
    d = inp_dir
    IF d THEN
        IF held_n < 255 THEN held_n = held_n + 1
    ELSE
        held_n = 0
    END IF
    c = 1
    IF held_n > 15 THEN c = 2
    IF held_n > 40 THEN c = 4
    IF d AND (held_n = 1 OR held_n > 15 OR (held_n AND 3) = 0) THEN
        IF (d AND IN_LEFT) AND aim_a < 180 THEN
            IF aim_a + c > 180 THEN aim_a = 180 ELSE aim_a = aim_a + c
        END IF
        IF (d AND IN_RIGHT) AND aim_a > 0 THEN
            IF aim_a < c THEN aim_a = 0 ELSE aim_a = aim_a - c
        END IF
        #v = PEEK(TK_E + my_seat) * 10
        IF (d AND IN_UP) AND #aim_p < #v THEN #aim_p = #aim_p + c * 5
        IF (d AND IN_DOWN) AND #aim_p > 0 THEN
            IF #aim_p < c * 5 THEN #aim_p = 0 ELSE #aim_p = #aim_p - c * 5
        END IF
        IF #aim_p > #v THEN #aim_p = #v
        POKE TK_A + my_seat, aim_a
        POKE MB_DIRTY, 1
        GOSUB hud
    END IF
    IF ev_btn AND BTN_NEXT THEN GOSUB next_weapon : GOSUB hud
    IF ((#frame_count - last_send) AND 255) >= 8 AND (aim_a <> sent_a OR #aim_p <> #sent_p) THEN
        POKE RAWB + 3, aim_a
        POKE RAWB + 4, #aim_p AND 255
        POKE RAWB + 5, #aim_p / 256
        na_act = ACT_AIM : na_n = 3 : GOSUB net_act
        sent_a = aim_a : #sent_p = #aim_p
        last_send = #frame_count
    END IF
    IF ev_btn AND BTN_FIRE THEN
        POKE RAWB + 3, aim_a
        POKE RAWB + 4, #aim_p AND 255
        POKE RAWB + 5, #aim_p / 256
        POKE RAWB + 6, weapon
        POKE RAWB + 7, guide
        POKE RAWB + 8, target
        na_act = ACT_FIRE : na_n = 6 : GOSUB net_act
        guide = I_NONE : target = NO_SEAT
        my_turn = 0 : POKE MB_ME, 0
        GOSUB hud
    END IF
END

' ---- the script ------------------------------------------------------------------

    DIM ps_k
' play_script: the segment in SCRIPT played to its end (ply.asm), then D.
' Frames are the clock: a pass that took too long is caught up (4 steps
' at most), the drawing only once. The link stays up meanwhile (a G frame
' in the middle: the server gave up on us; the field comes again whole).
' The script's last segment: everything stops.
play_script: PROCEDURE
    in_script = 1
    my_turn = 0 : POKE MB_ME, 0 : POKE BAN_ON, 0
    net_skipped = 0
    IF seg_no = 0 THEN CALL PLY_RESET : talk_left = 0
    CALL PLY_START
    GOSUB hud
    DO
        GOSUB frame_wait
pp_a:
        FOR ps_k = 1 TO owed
            GOSUB jiffy_step
            CALL PLY_STEP
            IF PEEK(PL_DONE) THEN EXIT FOR
        NEXT ps_k
        GOSUB ply_events
        CALL PLY_DRAW
        GOSUB field_draw
        GOSUB input_take
        IF ev_key = KEY_MUTE THEN GOSUB snd_toggle_mute
        IF offline = 0 THEN
            dec_budget = DECODE_PLAY
            GOSUB net_poll
        END IF
        IF net_skipped OR net_r = NET_LOST OR net_r = NET_ERROR THEN
            CALL PLY_LAST
            GOSUB ps_over
            RETURN
        END IF
pp_b:
    LOOP WHILE PEEK(PL_DONE) = 0
    CALL PLY_END
    IF seg_flags AND S_LAST_SEG THEN
        CALL PLY_LAST
        GOSUB ps_over
    END IF
    IF offline = 0 THEN GOSUB net_done
END

' ps_over: the script's end: still, quiet, the tanks from the state
ps_over: PROCEDURE
    in_script = 0
    talk_left = 0
    POKE PL_SHAKE, 0
    POKE PL_FLASH, 0
    POKE SHK_X, 0 : POKE SHK_Y, 0
    BORDER 0
    SOUND 0, 1, 0 : whistle = 0
    GOSUB field_draw
    net_terrain = 1
    GOSUB hud
END

' ply_events: what the script's ops left for us: sounds, the HUD, a phrase,
' the shaking and the flash
ply_events: PROCEDURE
    v = PEEK(PL_EV)
    IF v THEN
        POKE PL_EV, 0
        IF v AND EV_SND THEN
            n = PEEK(PL_NSND)
            FOR i = 1 TO n
                #w = PEEK(PL_SND + i - 1)
                sid = #w AND 255 : sn_size = #w / 256 : GOSUB snd_cue
            NEXT i
            POKE PL_NSND, 0
        END IF
        IF v AND EV_TALK THEN GOSUB talk_show
        IF v AND EV_HUD THEN GOSUB hud
    END IF
    IF talk_left THEN
        IF talk_left <= owed THEN talk_left = 0 : GOSUB hud ELSE talk_left = talk_left - owed : GOSUB talk_tick
    END IF
    c = PEEK(PL_FALL)                   ' a shell falling: the whistle, lower as it drops
    IF c AND (snd_off AND 1) = 0 THEN
        SOUND 0, 120 + c * 4, 4 : whistle = 1
    ELSEIF whistle THEN
        SOUND 0, 1, 0 : whistle = 0
    END IF
    c = PEEK(PL_SHAKE)                  ' shaking: the screen a pixel or two about
    IF c THEN
        IF c <= owed THEN c = 0 ELSE c = c - owed
        POKE PL_SHAKE, c
        IF c THEN POKE SHK_X, (#frame_count AND 1) * 2 : POKE SHK_Y, (#frame_count AND 2) / 2 ELSE POKE SHK_X, 0 : POKE SHK_Y, 0
    END IF
    c = PEEK(PL_FLASH)                  ' a flash: the border lit
    IF c THEN
        IF c <= owed THEN c = 0 ELSE c = c - owed
        POKE PL_FLASH, c
        IF c THEN BORDER PEEK(PL_FLASHC) AND 15 ELSE BORDER 0
    END IF
END


