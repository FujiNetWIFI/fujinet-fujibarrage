; sound.inc -- the sounds on the Studio II's one fixed tone (FujiBlast's):
; each a rhythm of (jiffies on, jiffies off) pairs, the ISR keeping Q on
; while RD.0 counts down. A sound cuts the one playing only if its
; priority is as high (SNDPRI: talk 0, ticks 1, tunes 2, the rest 3);
; snd_tick runs once a pass (the main loop, the title's).
; The ids: SND_* 0-15 (proto.inc: OP_SOUND's), SND_BOOM0-3 (a boom's size),
; UI_*, MUS_* (snd.inc: tools/s2/mksnd_s2.py).

; snd_play: sound D, unless muted or a higher one plays. Clobbers R8, RD.1.
        RFIT    86
snd_play:
        PHI     RD                      ; (the id: no call before it's used)
        GETV    FLAGS
        ANI     FL_MUTE
        BNZ     sp9
        GETV    SNDPH                   ; one playing: its priority
        BZ      sp1
        GETV    SNDID
        ADI     LO(SNDPRI)
        PLO     R8
        LDI     HI(SNDPRI)
        ADCI    0
        PHI     R8
        LDN     R8
        STR     R2
        GHI     RD                      ; the new one's: as high?
        ADI     LO(SNDPRI)
        PLO     R8
        LDI     HI(SNDPRI)
        ADCI    0
        PHI     R8
        LDN     R8
        SM
        BNF     sp9
sp1:    GHI     RD
        PUTV    SNDID
        SHL
        ADI     LO(SNDTAB)
        PLO     R8
        LDI     HI(SNDTAB)
        ADCI    0
        PHI     R8
        LDA     R8                      ; (DW: high byte first)
        PUTV    SNDPH
        LDN     R8
        PUTV    SNDPL
        GETV    JIFFY
        PUTV    SNDT
        FJMP    snd_tick
sp9:    FRET

; snd_tick: every pass: the next pair when its time comes. Clobbers R8.
        RFIT    71
snd_tick:
        GETV    SNDPH
        BZ      stk9
        GETV    SNDT                    ; JIFFY - SNDT, signed: not yet?
        STR     R2
        GETV    JIFFY
        SM
        ANI     80H
        BNZ     stk9
        GETV    SNDPH
        PHI     R8
        GETV    SNDPL
        PLO     R8
        LDA     R8                      ; on
        BZ      stk8
        PLO     RD                      ; the ISR's beep counter
        STR     R2
        LDA     R8                      ; + off
        ADD
        STR     R2
        GETV    JIFFY
        ADD
        PUTV    SNDT
        GLO     R8
        PUTV    SNDPL
        GHI     R8
        PUTV    SNDPH
stk9:   FRET
stk8:   SETV    SNDPH, 0
        FRET

; snd_mute: the sound off (the beep stopped) or on (a tick says so).
        RFIT    24
snd_mute:
        GETV    FLAGS
        XRI     FL_MUTE
        STR     RA
        ANI     FL_MUTE
        BZ      smu1
        SETV    SNDPH, 0
        FRET
smu1:   SND     UI_SELECT
        FRET
