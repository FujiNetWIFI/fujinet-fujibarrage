' ===========================================================================
' sound.bas -- the PSG (sound.h's). The tunes (gen/intv/music.bas, from
' music/*.txt: the title's march, a round won, the game over, the shop)
' play ch1 on voice A and ch2 on voice C; the effects (snd_cue: what the
' script's OP_SOUND says happened, and the menus' blips) on B, and on A and
' C too while no tune plays (in play none does). The frame handler plays a
' jiffy at a time (twice on PAL's fifth frame).
'
' The music player is FujiBlast's (src/intv/sound.bas) with tunes that end.
' ===========================================================================

    CONST UI_TICK   = 64        ' (sound.h's)
    CONST UI_SELECT = 65
    CONST UI_GO     = 66
    CONST UI_ERROR  = 67
    CONST UI_TURN   = 68
    CONST UI_TALK   = 69        ' (ours: a phrase's chirp)

    DIM snd_off                 ' muted: bit 0 all, bit 1 the tunes (0 -> 2 -> 3 -> 0)
    DIM mus_na, mus_np          ' the tune's drum on A, and its noise period (MIXW reads them)

' snd_mixw: the mixer and the noise's period (sfx.asm's MIXW: the tune's
' drum on A, an effect's noise on B or C)
snd_mixw: PROCEDURE
    CALL MIXW
END

' ---------------------------------------------------------------------------
' The effects: sfx.asm's, on B (and C's low end while no tune has it).
' ---------------------------------------------------------------------------
    DIM sid, sn_size
snd_isr: PROCEDURE
    CALL SFX_ISR
END

' snd_cue: cue sid (SND_* or UI_*), size sn_size (a boom's, 0-3)
snd_cue: PROCEDURE
    IF snd_off AND 1 THEN RETURN
    CALL SFX_CUE(sid, sn_size)
    sn_size = 0
END

' ---------------------------------------------------------------------------
' The tunes: a word a step in mus_d (tools/mkmus_intv.py: a period's index
' or a noise drum, the volume, the jiffies; or a control: return, call,
' jump, the end), a channel's order a list of calls to the patterns'
' subroutines.
' ---------------------------------------------------------------------------
    CONST MUS_RET  = 0
    CONST MUS_CALL = 1
    CONST MUS_JUMP = 2
    CONST MUS_NOISE0 = 112
    DIM mus_on                  ' a channel a bit: still playing
    DIM #mp(2), #mr(2), ml(2)   ' each channel's next step, where a call returns to, jiffies left
    DIM mc, mq, mnz, mvl, mw
    DIM #mi, #mv, #mper
    UNSIGNED #mv

' mus_isr: a jiffy of the tune (the frame handler's)
mus_isr: PROCEDURE
    IF mus_on = 0 THEN RETURN
    FOR mc = 0 TO 1
        IF mus_on AND (mc + 1) THEN
            IF ml(mc) THEN ml(mc) = ml(mc) - 1
            IF ml(mc) = 0 THEN
                DO
                    #mi = #mp(mc)
                    #mv = mus_d(#mi)
                    mq = #mv / 2048             ' its jiffies (0: a control)
                    IF mq THEN EXIT DO
                    mq = #mv AND 3
                    IF mq = MUS_RET THEN
                        #mp(mc) = #mr(mc)
                    ELSEIF mq = MUS_CALL THEN
                        #mr(mc) = #mi + 2
                        #mp(mc) = mus_d(#mi + 1)
                    ELSEIF mq = MUS_JUMP THEN
                        #mp(mc) = mus_d(#mi + 1)
                    ELSE
                        EXIT DO                 ' (the end: mq 3, jiffies 0)
                    END IF
                LOOP WHILE 1
                IF (#mv / 2048) = 0 THEN
                    mus_on = mus_on AND (2 - mc)
                    IF mc = 0 THEN
                        mus_na = 0
                        SOUND 0, 1, 0
                        GOSUB snd_mixw
                    ELSE
                        SOUND 2, 1, 0
                    END IF
                ELSE
                    ml(mc) = mq
                    #mp(mc) = #mi + 1
                    mw = #mv AND 127
                    mvl = (#mv / 128) AND 15
                    mnz = 0
                    IF mw >= MUS_NOISE0 THEN mnz = mw - MUS_NOISE0 : mw = 0
                    IF snd_off THEN mvl = 0
                    IF NTSC THEN #mper = mus_nper(mw) ELSE #mper = mus_pper(mw)
                    IF mc = 0 THEN
                        IF mnz THEN
                            mus_na = 1 : mus_np = mnz
                            SOUND 0, 1, mvl
                        ELSE
                            mus_na = 0
                            SOUND 0, #mper, mvl
                        END IF
                        GOSUB snd_mixw
                    ELSE
                        SOUND 2, #mper, mvl
                    END IF
                END IF
            END IF
        END IF
    NEXT mc
END

' snd_tune: the tune whose channels start at #v, #w (T_*1, T_*2), both
' on the same jiffy
snd_tune: PROCEDURE
    ASM DIS
    #mp(0) = #v : #mp(1) = #w
    ml(0) = 0 : ml(1) = 0
    mus_on = 3
    ASM EIS
END

snd_tune_off: PROCEDURE
    IF mus_on = 0 THEN RETURN
    ASM DIS
    mus_on = 0 : mus_na = 0
    ASM EIS
    SOUND 0, 1, 0
    SOUND 2, 1, 0
    GOSUB snd_mixw
END

snd_stop: PROCEDURE
    GOSUB snd_tune_off
    CALL SFX_STOP
    SOUND 0, 1, 0
END

' all sound -> the effects (no tunes) -> none -> all
snd_toggle_mute: PROCEDURE
    IF snd_off = 0 THEN
        snd_off = 2
    ELSEIF snd_off = 2 THEN
        snd_off = 3
    ELSE
        snd_off = 0
    END IF
    IF snd_off THEN SOUND 0, 1, 0 : SOUND 2, 1, 0
    IF snd_off AND 1 THEN CALL SFX_STOP
END
