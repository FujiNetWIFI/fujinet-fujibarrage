' ===========================================================================
' isr.bas -- the frame handler (ON FRAME GOSUB frame_isr) and the jiffy
' clock (plat.h's), after FujiBlast's. IntyBASIC allows one handler: the
' clock, the controller and the sound hang off it. The field's GRAM and its
' BACKTAB words go in earlier, in VBLANK's GRAM window (cell.asm's
' GRAM_FLUSH: the Makefile calls it from IntyBASIC's own handler, after the
' MOBs). It can run before the program's start-up (every variable is 0
' then: ready says when it may do more than count), and it must stay short
' and shallow: the stack is 24 words.
' ===========================================================================

    DIM #jiffy                  ' jiffies since boot: 60 a second (PAL counts every 5th twice)
    UNSIGNED #jiffy
    DIM pal5, ready, isr_t

frame_isr: PROCEDURE
    #jiffy = #jiffy + 1
    isr_t = 1
    IF NTSC = 0 THEN
        pal5 = pal5 + 1
        IF pal5 >= 5 THEN pal5 = 0 : #jiffy = #jiffy + 1 : isr_t = 2
    END IF
    IF ready = 0 THEN RETURN
    CALL READ_INPUT
    GOSUB snd_isr
    GOSUB mus_isr
    IF isr_t = 2 THEN GOSUB snd_isr : GOSUB mus_isr
isr_end:
END

' frame_wait: the next frame. owed: the jiffies behind (1-4; a stall of
' more than 30 isn't raced after).
    DIM #seen, #frame_count, owed
    UNSIGNED #seen, #frame_count
frame_wait: PROCEDURE
    WAIT
    #v = #jiffy - #seen
    IF #v > 30 THEN #seen = #jiffy - 1 : #v = 1
    IF #v > 4 THEN #v = 4
    owed = #v
END

' a jiffy of the logic's clock
jiffy_step: PROCEDURE
    #seen = #seen + 1
    #frame_count = #seen
END
