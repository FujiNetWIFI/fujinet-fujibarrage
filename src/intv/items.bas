' ===========================================================================
' items.bas -- the items line (src/play.c's), on HUD row 1: R opens it on
' our turn or in the defence window. The disc's left and right pick an item
' we have; fire uses it. A guidance item asks for its target next (the disc
' picks a tank, which flashes; fire takes it: the next shot steers for it);
' fuel drives (the disc held left or right, fire stops). R or CLEAR closes
' it. The Defense Computer works by itself: it isn't listed.
' ===========================================================================

' (menu, item, drive_n and the M_* are play.bas's: it uses them first)

' defending: v = 1 in the defence window, ours and not done
defending: PROCEDURE
    v = 0
    IF rb(ST + OFF_PHASE) <> PH_DEFENSE OR my_seat >= SEATS THEN RETURN
    IF (pl(my_seat, P_FLAGS) AND (PF_DEFENSE + PF_DONE)) = PF_DEFENSE THEN v = 1
END

' next_item: item = the next owned one from item, a step d (1, or 255 back);
' I_NONE if none
next_item: PROCEDURE
    FOR c = 1 TO N_ITEMS - N_WEAPONS
        item = item + d
        IF item < N_WEAPONS OR item >= N_ITEMS THEN
            IF d = 1 THEN item = N_WEAPONS ELSE item = N_ITEMS - 1
        END IF
        IF inv(item) AND item <> I_DEFENSE_COMPUTER THEN RETURN
    NEXT c
    item = I_NONE
END

' next_target: target = the next tank alive, not ours, a step d
next_target: PROCEDURE
    FOR c = 1 TO SEATS
        target = target + d
        IF target >= SEATS THEN
            IF d = 1 THEN target = 0 ELSE target = SEATS - 1
        END IF
        IF target <> my_seat AND PEEK(TK_ON + target) THEN RETURN
    NEXT c
    target = NO_SEAT
END

' items_line: row 1 as the line has it
items_line: PROCEDURE
    r = 1 : GOSUB gfx_clear_row
    IF menu = M_ITEM THEN
        i = item : n = 12 : GOSUB cat_item_name
        #a = STXT : #p = 20 : #cw = CW_YELLOW : GOSUB gfx_cstr
        #BACKTAB(33) = (88 - 32) * 8 + CW_WHITE          ' X
        #nv = inv(item) : #p = 34 : #cw = CW_WHITE : GOSUB gfx_num
        IF item = I_PARACHUTE OR item = I_IMPACT_FUSE THEN
            IF item = I_PARACHUTE THEN c = PF_CHUTE ELSE c = PF_FUSE
            IF pl(my_seat, P_FLAGS) AND c THEN PRINT AT 37 COLOR CW_GREEN, "ON" ELSE PRINT AT 37 COLOR CW_RED, "OFF"
        END IF
    ELSEIF menu = M_TARGET THEN
        PRINT AT 20 COLOR CW_YELLOW, "TARGET"
        IF target < SEATS THEN s = target : #p = 27 : n = NAME_LEN : #cw = CW_WHITE : GOSUB gfx_name
    ELSE
        PRINT AT 20 COLOR CW_YELLOW, "DRIVE <>  FUEL"
        #nv = inv(I_FUEL) : #p = 35 : #cw = CW_WHITE : GOSUB gfx_num
    END IF
END

' items_close: the line shut, row 1 the HUD's again
items_close: PROCEDURE
    menu = M_OFF
    GOSUB hud
END

' items_open: R pressed: the line, at the first item we have
items_open: PROCEDURE
    item = N_ITEMS - 1 : d = 1 : GOSUB next_item
    IF item = I_NONE THEN sid = UI_ERROR : GOSUB snd_cue : RETURN
    menu = M_ITEM
    prev_dir = inp_dir
    GOSUB items_line
END

' items_frame: a frame of the line (it has the disc and the buttons)
items_frame: PROCEDURE
    IF (ev_btn AND BTN_ITEMS) OR ev_key = KEY_ESC THEN GOSUB items_close : RETURN
    v = IN_LEFT : GOSUB pressed : t = v
    v = IN_RIGHT : GOSUB pressed
    d = 0
    IF t THEN d = 255
    IF v THEN d = 1
    IF menu = M_ITEM THEN
        IF d THEN GOSUB next_item : sid = UI_TICK : GOSUB snd_cue : GOSUB items_line
        IF ev_btn AND BTN_FIRE THEN
            sid = UI_SELECT : GOSUB snd_cue
            GOSUB defending
            IF item <= I_AUTOPILOT AND v = 0 THEN
                guide = item
                target = NO_SEAT : d = 1 : GOSUB next_target
                menu = M_TARGET
            ELSEIF item = I_FUEL AND v = 0 THEN
                menu = M_DRIVE
            ELSE
                POKE RAWB + 3, item
                na_act = ACT_USE : na_n = 1 : GOSUB net_act
                IF item <> I_PARACHUTE AND item <> I_IMPACT_FUSE THEN GOSUB items_close : RETURN
            END IF
            GOSUB items_line
        END IF
    ELSEIF menu = M_TARGET THEN
        IF d THEN GOSUB next_target : sid = UI_TICK : GOSUB snd_cue : GOSUB items_line
        IF target < SEATS THEN POKE TK_FL + target, 2      ' (it flashes)
        IF ev_btn AND BTN_FIRE THEN sid = UI_SELECT : GOSUB snd_cue : GOSUB items_close
    ELSE
        IF inp_dir AND (IN_LEFT + IN_RIGHT) THEN
            IF (drive_n AND 7) = 0 THEN
                IF inp_dir AND IN_RIGHT THEN POKE RAWB + 3, 1 ELSE POKE RAWB + 3, 0
                na_act = ACT_DRIVE : na_n = 1 : GOSUB net_act
            END IF
            drive_n = drive_n + 1
        ELSE
            drive_n = 0
        END IF
        IF ev_btn AND BTN_FIRE THEN GOSUB items_close
    END IF
    prev_dir = inp_dir
END
