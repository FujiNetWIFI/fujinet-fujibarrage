' ===========================================================================
' main.bas -- FujiBarrage for the Intellivision and the FujiNet cartridge:
' artillery for two to ten tanks, as src/main.c.
'
' The lobby's appkeys say who we are and where to play (appkey.bas); else
' we ask, and list the rooms. In a room, the server's state says which
' screen to show: the waiting room, the shop, the field (the defence
' window, aiming, a shot), a round's end, the game's.
'
' There is no C compiler for the CP1610, so this is the client again in
' IntyBASIC 1.5 (src/intv/Makefile builds it) with its hot paths in
' CP1610 assembly; after FujiBlast's Intellivision client. Memory:
' memory.bas; the field: fld.asm, cell.asm, play.bas; the script: ply.asm.
' ===========================================================================

    OPTION MAP 2
    OPTION EXPLICIT ON
    ASM CFGVAR "name" = "FujiBarrage"
    ASM CFGVAR "short_name" = "FujiBarrage"
    ASM CFGVAR "year" = 2026

    GOTO boot

    INCLUDE "memory.bas"
    INCLUDE "proto.bas"
    INCLUDE "opts.bas"
    ' MAP 2's windows (the Makefile cuts them to what checkcfg.py allows):
    '   5 $4810-$4FFF  the art, the field's tables
    '   0 $5000-$6FFF  the tunes, the link, input, sound, the main loop
    '   1 $A000-$B7FF  the menus, the rooms, what stands out (turn.bas)
    '   4 $7100-$7FFF  the test scenes (TEST=1 only: the ECS's ROM, so never on a console)
    '   2 $C040-$DFFF  the field and the shot
    '   3 $F000-$F7FF  the catalog; IntyBASIC's own (its epilogue)
    SEGMENT 5
    INCLUDE "gfx.bas"
    INCLUDE "config.bas"
    ASM INCLUDE "fldtab.asm"
    SEGMENT 0
    INCLUDE "music.bas"
    INCLUDE "fujinet.bas"
    INCLUDE "nstrans.bas"
    INCLUDE "net.bas"
    INCLUDE "isr.bas"
    INCLUDE "input.bas"
    INCLUDE "sound.bas"
    INCLUDE "appkey.bas"
    ASM INCLUDE "netasm.asm"
    ASM INCLUDE "io.asm"
    SEGMENT 3
    INCLUDE "cat.bas"
    ASM INCLUDE "util.asm"
    SEGMENT 2
    INCLUDE "screen.bas"
    INCLUDE "play.bas"
    ASM INCLUDE "fld.asm"
    ASM INCLUDE "cell.asm"
    ASM INCLUDE "mob.asm"
    ASM INCLUDE "trk.asm"
    ASM INCLUDE "ops.asm"
    ASM INCLUDE "ply.asm"
    SEGMENT 5
    INCLUDE "items.bas"
    SEGMENT 1
    ASM INCLUDE "sfx.asm"
    INCLUDE "menus.bas"
    INCLUDE "room.bas"
    INCLUDE "turn.bas"
    ' the test scenes and their fixture: SEGMENT 4 ($7100-$7FFF, the ECS's
    ' ROM: free in jzIntv's -s0 runs) in a TEST=1 build, else 0 (stubs)
    INCLUDE "testseg.bas"
    INCLUDE "fixture.bas"
    INCLUDE "test.bas"
    SEGMENT 0

    CONST JOIN_WAIT = 900       ' frames for the first state before giving up
    CONST S_NONE    = 0
    CONST S_WAIT    = 1
    CONST S_SHOP    = 2
    CONST S_FIELD   = 3
    CONST S_RESULTS = 4
    CONST S_OVER    = 5
    DIM shown, shown_seq, want
    DIM #pr_t

screen_for: PROCEDURE
    c = rb(ST + OFF_PHASE)
    IF c = PH_SHOP THEN
        want = S_SHOP
    ELSEIF c = PH_DEFENSE OR c = PH_AIM OR c = PH_SHOT THEN
        want = S_FIELD
    ELSEIF c = PH_ROUNDEND THEN
        want = S_RESULTS
    ELSEIF c = PH_GAMEOVER THEN
        want = S_OVER                   ' (its own: it follows a round's end)
    ELSE
        want = S_WAIT
    END IF
END

' room_upper: SRVTBL in capitals at STXT (the GROM has no small letters here).
room_upper: PROCEDURE
    FOR i = 0 TO ROOM_LEN
        c = rb(SRVTBL + i)
        IF c >= 97 AND c <= 122 THEN c = c - 32
        POKE STXT + i, c
        IF c = 0 THEN EXIT FOR
    NEXT i
END

' play_room: play in SRVTBL. pr_ret = 1: come back to it (the link
' dropped); 0: we left, or it would not have us.
play_room: PROCEDURE
    GOSUB appkey_save_room
    GOSUB gfx_begin
    PRINT AT 86 COLOR CW_YELLOW, "JOINING"
    GOSUB room_upper
    #a = STXT : n = 20 : r = 6 : #cw = CW_WHITE : GOSUB gfx_ccentre
    GOSUB net_open
    IF link = 0 THEN GOSUB msg_host : pr_ret = 0 : RETURN
    GOSUB net_hello
    shown = S_NONE : shown_seq = 0 : #pr_t = 0 : my_seat = NONE : cat_ok = 0
    DO
        GOSUB frame_wait
        #seen = #jiffy : #frame_count = #seen   ' (the clock, as tick has it: the turn's
                                        ' banner and reminder, the aim's sends read it)
pt_a:                                   ' (a pass, for intvsim's prof: pt_a-pt_b)
        GOSUB input_take
        IF ev_key = KEY_MUTE THEN GOSUB snd_toggle_mute
        dec_budget = DECODE_MOST
        GOSUB net_poll
pt_n:
        IF net_r = NET_ERROR THEN
            GOSUB snd_stop
            GOSUB net_close
            GOSUB gfx_begin
            PRINT AT 81 COLOR CW_YELLOW, "THE SERVER SAID NO"
            IF net_err = ERR_VERSION THEN
                PRINT AT 121 COLOR CW_WHITE, "UPDATE FUJIBARRAGE"
            ELSEIF net_err = ERR_CAPS THEN
                PRINT AT 120 COLOR CW_WHITE, "NOT SERVED HERE"
            ELSE
                PRINT AT 124 COLOR CW_WHITE, "NO SUCH ROOM"
            END IF
            msg_t = 0 : GOSUB message_wait
            pr_ret = 0 : RETURN
        END IF
        IF net_r = NET_LOST THEN
            GOSUB snd_stop
            GOSUB gfx_begin
            PRINT AT 82 COLOR CW_YELLOW, "CONNECTION LOST"
            PRINT AT 124 COLOR CW_WHITE, "TRYING AGAIN"
            msg_t = 120 : GOSUB message_wait
            pr_ret = 1 : RETURN
        END IF
        IF ev_key = KEY_ESC AND (shown <> S_FIELD OR menu = M_OFF) THEN
            GOSUB snd_stop
            GOSUB net_leave
            GOSUB net_close
            pr_ret = 0 : RETURN
        END IF
        IF net_heard = 0 THEN
            #pr_t = #pr_t + 1
            IF #pr_t > JOIN_WAIT THEN
                GOSUB net_close
                GOSUB msg_host
                pr_ret = 0 : RETURN
            END IF
        ELSE
            IF net_r = NET_STATE THEN
                my_seat = rb(ST + OFF_MY_SEAT)
                GOSUB cat_check
                GOSUB screen_for
                IF want <> shown OR (want = S_FIELD AND rb(ST + OFF_SEQ) <> shown_seq) THEN
                    shown = want
                    shown_seq = rb(ST + OFF_SEQ)
                    IF want = S_WAIT THEN
                        GOSUB wait_enter
                    ELSEIF want = S_SHOP THEN
                        GOSUB shop_enter
                    ELSEIF want = S_FIELD THEN
                        GOSUB play_enter
                    ELSE
                        res_final = (want = S_OVER) AND 1
                        GOSUB results_enter
                    END IF
                ELSEIF shown = S_WAIT THEN
                    GOSUB wait_state
                ELSEIF shown = S_SHOP THEN
                    GOSUB shop_state
                ELSEIF shown = S_FIELD THEN
                    GOSUB play_state
                END IF
            END IF
            IF net_r = NET_SCRIPT THEN
                IF shown <> S_FIELD THEN shown = S_FIELD : shown_seq = rb(ST + OFF_SEQ) : GOSUB play_enter
                GOSUB play_script
                IF pr_ret = 2 THEN pr_ret = 0 : RETURN    ' (left mid-shot)
            END IF
            IF shown = S_WAIT THEN
                GOSUB wait_frame
            ELSEIF shown = S_SHOP THEN
                GOSUB shop_frame
            ELSEIF shown = S_FIELD THEN
                GOSUB play_frame
                IF pr_ret = 2 THEN pr_ret = 0 : RETURN
            ELSE
                GOSUB results_frame
            END IF
        END IF
pt_b:
    LOOP WHILE 1
END

' ---------------------------------------------------------------------------
boot:
    CALL FLD_CLEAR              ' (the cartridge's RAM: what a reset or CONFIG left)
    POKE TEST_HOOK, 0
    WAIT
    ON FRAME GOSUB frame_isr
    MODE 1
    BORDER 0
    CLS
    SOUND 4, 1, $38
    GOSUB gram_fixed_in
    my_seat = NONE
    ready = 1
    GOSUB test_check
    GOSUB fn_wait_mailbox
    IF fn_ok = 0 THEN
        PRINT AT 82 COLOR CW_YELLOW, "NO FUJINET FOUND"
        PRINT AT 120 COLOR CW_WHITE, "IS THE CARTRIDGE IN?"
        DO
            WAIT
        LOOP WHILE 1
    END IF
    GOSUB appkey_load
    GOSUB title
    IF rb(MYNAME) = 0 THEN
        GOSUB name_entry
        GOSUB appkey_save_name
    END IF
main_loop:
    IF rb(SRVTBL) = 0 THEN
        GOSUB room_list
        IF v = 0 THEN GOTO main_loop
    END IF
    GOSUB play_room
    IF pr_ret = 0 THEN GOSUB appkey_clear_room
    GOTO main_loop
