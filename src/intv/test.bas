' ===========================================================================
' test.bas -- the offline test scenes, for the rigs (tools/intvsim/intvsim.py
' "test SCENE ARG"), as src/test.c: the screens on a canned state, no
' network. A rig pokes $5A, $A5 into TEST_HOOK + 2 and + 3, the scene into
' + 4 and its argument into + 5 before the console starts:
'   1  the field: the fixture's terrain and tanks (gen/intv/fixture.bas,
'      from refview -fixture-bas), seat 0 ours and aiming
'   2  the waiting room
'   3  the field, then the fixture's shot played (each segment in turn);
'      tst_s 99 when it is over (tools/intvsim/refcheck.py waits for it)
'   4  a round's end (seat 3 won)       5  the game's end
'   6  every effect, three seconds apart: SND_* 0-15 (a boom at
'      each of its sizes), then the UI's
'   7  the tunes: the title's (32 s), a round won, the game over, the shop
' ===========================================================================

    DIM tst_s, tst_a
    DIM #tst_p

test_check: PROCEDURE
    IF WITH_TEST = 0 THEN RETURN
    IF rb(TEST_HOOK + 2) <> $5A OR rb(TEST_HOOK + 3) <> $A5 THEN RETURN
    tst_s = rb(TEST_HOOK + 4) : tst_a = rb(TEST_HOOK + 5)
    POKE TEST_HOOK + 2, 0
    GOSUB canned
    offline = 1
    IF tst_s = 1 THEN GOTO test_field
    IF tst_s = 3 THEN GOTO test_shot
    IF tst_s >= 6 THEN GOTO test_sound
    my_seat = 0
    IF tst_s >= 4 THEN
        POKE ST + OFF_PHASE, PH_ROUNDEND
        POKE ST + OFF_RESULT, 3
        FOR s = 0 TO SEATS - 1
            POKE ST + OFF_SEATS + s * SEAT_SIZE + P_WINS, s AND 3
            POKE ST + OFF_SEATS + s * SEAT_SIZE + P_SCORE, s * 37
        NEXT s
        res_final = tst_s - 4
        GOSUB results_enter
    ELSE
        POKE ST + OFF_PHASE, PH_WAIT
        GOSUB wait_enter
    END IF
    DO
        GOSUB tick
        IF tst_s < 4 THEN GOSUB wait_frame
    LOOP WHILE 1

test_field:
tt_a:
    GOSUB fix_terrain_in
tt_b:
    GOSUB play_enter
tt_c:
    GOSUB play_state
    DO
        GOSUB frame_wait
tf_a:
        GOSUB input_take
        GOSUB play_frame
tf_b:
    LOOP WHILE 1

test_sound:
    CLS
    IF tst_s = 7 THEN
        FOR tst_a = 0 TO 3
            #v = tst_tunes(tst_a * 2) : #w = tst_tunes(tst_a * 2 + 1) : GOSUB snd_tune
            #tst_p = tst_tlen(tst_a) : GOSUB tst_wait
            GOSUB snd_tune_off
            #tst_p = 30 : GOSUB tst_wait
        NEXT tst_a
    ELSE
        FOR tst_a = 0 TO 25
            sid = tst_a : sn_size = 0
            IF tst_a >= 16 THEN sid = SND_BOOM : sn_size = tst_a - 16 + 1
            IF tst_a >= 19 THEN sid = UI_TICK + tst_a - 19
            GOSUB snd_cue
            #tst_p = 180 : GOSUB tst_wait
        NEXT tst_a
    END IF
    tst_s = 99
    DO
        WAIT
    LOOP WHILE 1

test_shot:
    GOSUB fix_terrain_in
    GOSUB play_enter
    POKE ST + OFF_PHASE, PH_SHOT
    FOR tst_a = 1 TO 120                ' the field drawn first
        GOSUB frame_wait
        GOSUB jiffy_step
        GOSUB field_draw
    NEXT tst_a
    #tst_p = 0                          ' each segment in turn
    FOR tst_a = 0 TO FIX_NSEG - 1
        #seg_len = fix_seglen(tst_a)
        FOR #a = 0 TO #seg_len - 1
            #v = fix_seg((#tst_p + #a) / 2)
            IF (#tst_p + #a) AND 1 THEN #v = #v / 256
            POKE SCRIPT + #a, #v AND 255
        NEXT #a
        #tst_p = #tst_p + #seg_len
        seg_no = tst_a
        IF tst_a = FIX_NSEG - 1 THEN seg_flags = S_LAST_SEG ELSE seg_flags = 0
tp_a:
        GOSUB play_script
tp_b:
    NEXT tst_a
    tst_s = 99
    DO
        GOSUB frame_wait
        GOSUB jiffy_step
        GOSUB field_draw
    LOOP WHILE 1
END

' canned: a state for the scenes: ten seats (the fixture's tanks), seat 0
' ours, aiming, the fixture's sky and dirt
canned: PROCEDURE
    FOR #a = ST TO ST + STATE_SIZE - 1
        POKE #a, 0
    NEXT #a
    POKE ST + OFF_PHASE, PH_AIM
    POKE ST + OFF_ROUND, 1
    POKE ST + OFF_ROUNDS, 3
    POKE ST + OFF_HOST, 0
    POKE ST + OFF_MY_SEAT, 0
    POKE ST + OFF_TURN, 0
    POKE ST + OFF_TIMER, 30
    POKE ST + OFF_WIND, 12
    POKE ST + OFF_SKY, FIX_SKY
    POKE ST + OFF_PALETTE, FIX_PAL
    FOR s = 0 TO SEATS - 1
        #a = ST + OFF_SEATS + s * SEAT_SIZE
        FOR k = 0 TO 4
            POKE #a + P_NAME + k, tst_names(s * 5 + k)
        NEXT k
        IF s THEN POKE #a + P_KIND, KIND_BOT ELSE POKE #a + P_KIND, KIND_HUMAN
        POKE #a + P_ENERGY, 100
        POKE #a + P_ANGLE, fix_ta(s)
        POKE #a + P_POWER, 500 AND 255
        POKE #a + P_POWER + 1, 500 / 256
        POKE #a + P_X, fix_tx(s)
        POKE #a + P_Y, fix_ty(s)
        POKE #a + P_FLAGS, PF_ALIVE
    NEXT s
    POKE ST + OFF_INV + 1, 3
    my_seat = 0
END

' fix_terrain_in: the fixture's terrain (two bytes a word) into the planes
fix_terrain_in: PROCEDURE
    FOR #tst_p = 0 TO FIX_NTERR - 1
        #v = fix_terrain(#tst_p / 2)
        IF #tst_p AND 1 THEN #v = #v / 256
        POKE SCRIPT + #tst_p, #v AND 255
    NEXT #tst_p
tu_a:
    #tst_p = USR COLS(0, 160, SCRIPT)
tu_b:
END

tst_tunes:
    DATA T_TITLE1, T_TITLE2, T_WIN1, T_WIN2, T_OVER1, T_OVER2, T_SHOP1, T_SHOP2
tst_tlen:                       ' (frames: the title's once round, the others whole)
    DATA 1920, 150, 240, 60

' tst_wait: #tst_p frames
tst_wait: PROCEDURE
    FOR #a = 1 TO #tst_p
        GOSUB frame_wait
        GOSUB jiffy_step
    NEXT #a
END

tst_names:                      ' (five letters each, ASCII: a DATA string isn't)
    DATA 65, 78, 78, 0, 0, 66, 79, 76, 84, 0
    DATA 67, 79, 66, 65, 76, 71, 65, 83, 75, 69
    DATA 83, 80, 65, 82, 75, 87, 73, 68, 71, 69
    DATA 80, 73, 83, 84, 79, 75, 65, 66, 79, 79
    DATA 82, 79, 86, 69, 82, 90, 65, 80, 80, 69
