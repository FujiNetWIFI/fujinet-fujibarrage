; p_test.inc -- the rigs' offline scenes (TEST builds). At the start TESTHK
; is 3C C3 for 20 frames; tools/s2sim's `test N ARG` pokes TESTHK+1 = N + 1,
; +2 = ARG, then $5A, and we answer $A5:
;   1  M0 spike: the field (a whole render, ten more, a 30-column settle)
;   2  (M0's six sprite tracks: gone; M3 has the player's)
;   3  M0 spike: a nuke's fireball by columns in six stages, its crater
;   4  Checkpoint 1's art: FIXBUF's field and tanks, three fireballs, a
;      shell, the HUD; ARG b0 day (inverted), b1-3 the dirt's pattern
;      (ARTPAT), b4 a crust
;   5  the screens on a canned STATE and catalog (FIXBUF: ST at +0, the
;      catalog's length at +510, the catalog at +512; tools/s2/mkstate_s2.py):
;      the room's loop with the link quiet; TESTEV (poked) a commit
;   6  the sounds: ARG < MUS_TITLE: each from ARG to the last cue, one
;      every 180 jiffies; else that tune alone; then quiet for ever
;      (S2SIM_WAV records them: tools/s2/sndwav_s2.py cuts them)
; Each scene writes RE.0 at its steps into TMARK and ends spinning.

; t_check: D = the scene asked for (0: none).
        RFIT    52
t_check:
        SETV    TESTHK, 3CH
        SETV    TESTHK+1, 0C3H
        LDI     20
        PLO     R9
qtc1:   GETV    TESTHK
        XRI     5AH
        BZ      qtc2
        FCALL   frame
        DEC     R9
        GLO     R9
        BNZ     qtc1
        SETV    TESTHK, 0
        LDI     0
        FRET
qtc2:   SETV    TESTHK, 0A5H
        GETV    TESTHK+1
        SMI     1
        FRET

MARK    MACRO   k               ; TMARK+k = the frame count now
        GLO     RE
        PUT9    TMARK+(k)
        ENDM

        RFIT    63
t_scene:
        PUT9    TSCN
        LDI     BK_FIX                  ; (FIXBUF's bank, the scenes through)
        FCALL   bksel
        GET9    TSCN
        SMI     1
        BNZ     tsc2
        FJMP    t_field                 ; 1
tsc2:   SMI     2
        BNZ     tsc4
        FJMP    t_play                  ; 3
tsc4:   SMI     1
        BNZ     tsc5
        FJMP    t_art                   ; 4 (2: the art too)
tsc5:   SMI     1
        BNZ     tsc6a
        FJMP    t_screens               ; 5
tsc6a:  FJMP    t_sounds                ; 6

t_end:  BR      t_end

; ---- scene 1: the field ---------------------------------------------------------
        RFIT    253
t_field:
        FCALL   frame
        MARK    0
        LDR     R8, TERR1               ; the model: 64 columns
        LDI     0
        PLO     R9
        LDI     FW
        PHI     R9
        FCALL   fld_puts
        MARK    1
        FCALL   fld_all                 ; the whole field
        MARK    2
        LDI     10                  ; ten more
        PUT9    TK
tfl1:    FCALL   fld_all
        GET9    TK
        SMI     1
        PUT9    TK
        BNZ     tfl1
        MARK    3
        LDR     R8, TERR2               ; a settle: 30 columns from x 17
        LDI     17
        PLO     R9
        LDI     30
        PHI     R9
        FCALL   fld_puts
        MARK    4
        LDI     64
        FCALL   fld_flush
        MARK    5
        LDI     10                  ; ten more 30-column redraws
        PUT9    TK
tfl2:    LDR     R8, TERR2
        LDI     17
        PLO     R9
        LDI     30
        PHI     R9
        FCALL   fld_puts
        LDI     64
        FCALL   fld_flush
        GET9    TK
        SMI     1
        PUT9    TK
        BNZ     tfl2
        MARK    6
        FJMP    t_end

; ---- scene 3: FIXBUF's shot, played as in a room ---------------------------------
; The field and the tanks from FIXBUF, each segment copied into SCRIPT and
; played through the player to its OP_END; then every dirty column drawn,
; TMARK+0..1 the jiffies it took (JIFFY's), +2..3 the script's frames
; (PNOW), +15 $A5: done (tools/s2sim/refcheck.py).
        RFIT    135
t_fire:
t_play: LDR     R8, FIXBUF+35           ; the model: FIXBUF's terrain
        LDI     0
        PLO     R9
        LDI     FW
        PHI     R9
        FCALL   fld_puts
        FCALL   fld_all
        LDI     0                       ; the tanks
        PUT9    TK
tpl1:   GET9    TK
        ADI     LO(FIXBUF+3)
        PLO     R8
        LDI     HI(FIXBUF+3)
        PHI     R8
        LDN     R8                      ; x
        PHI     R9
        GLO     R8
        ADI     10
        PLO     R8
        LDN     R8                      ; y
        PLO     RC
        GLO     R8
        ADI     10
        PLO     R8
        LDN     R8                      ; angle
        FCALL   ang_step
        PHI     RC
        LDI     1
        PLO     R9
        GET9    TK
        FCALL   tk_set
        GET9    TK
        ADI     1
        PUT9    TK
        XRI     SEATS
        BNZ     tpl1
tpl2:   FCALL   tk_pass
        GETV    TKDRT
        BNZ     tpl2
        GETV    TKDRT+1
        BNZ     tpl2
        FJMP    tpl3

        RFIT    89
tpl3:   FCALL   frame                   ; the clock from here (RE: the ISR's
        GLO     RE                      ;  frames, not JIFFY: frame caps a
        PUT9    TMARK                   ;  pass's jiffies at 8)
        GHI     RE
        PUT9    TMARK+1
tpl3a:  LDR     R8, FIXBUF+33           ; past the terrain: the first segment
        LDA     R8
        PLO     R9
        LDN     R8
        PHI     R9
        INC     R8
        GLO     R9
        STR     R2
        GLO     R8
        ADD
        PLO     R8
        GHI     R9
        STR     R2
        GHI     R8
        ADC
        PHI     R8
        GLO     R8                      ; T? the segment's place: FIXSEG
        PUT9    TMARK+4
        GHI     R8
        PUT9    TMARK+5
        SETV    SEGNO, 0
        FJMP    tpl4

        RFIT    188
tpl4:   GET9    TMARK+4                 ; the next segment: its length (LE),
        PLO     R8                      ;  then its bytes into SCRIPT
        GET9    TMARK+5
        PHI     R8
        LDA     R8
        PUTV    SEGLEN
        PLO     R9
        LDA     R8
        PUTV    SEGLEN+1
        PHI     R9
        POKEAT  SCRIPT
tpl5:   GLO     R9
        BNZ     tpl5a
        GHI     R9
        BZ      tpl6
tpl5a:  DEC     R9
        LDA     R8
        PKD
        BR      tpl5
tpl6:   PKEND
        GLO     R8
        PUT9    TMARK+4
        GHI     R8
        PUT9    TMARK+5
        GETV    SEGNO                   ; the last: S_LAST_SEG
        ADI     1
        STR     R2
        LDI     HI(FIXBUF+1)
        PHI     RB
        LDI     LO(FIXBUF+1)
        PLO     RB
        LDN     RB
        SM
        BZ      tpl6a
        LDI     0
        BR      tpl7
tpl6a:  LDI     S_LAST_SEG
tpl7:   PUTV    SEGFLG
        FCALL   ply_start
tpl8:   FCALL   frame
        FCALL   ply_pass
        GETV    PLAYING
        BNZ     tpl8
        INCV    SEGNO                   ; the next, if any
        STR     R2
        LDI     HI(FIXBUF+1)
        PHI     RB
        LDI     LO(FIXBUF+1)
        PLO     RB
        LDN     RB
        SM
        BNZ     tpl4
        FJMP    tpl9

        RFIT    115
tpl9:   GLO     RE                      ; the time it took, the frames
        STR     R2
        GET9    TMARK
        SD
        PUT9    TMARK
        GHI     RE
        STR     R2
        GET9    TMARK+1
        SDB
        PUT9    TMARK+1
        GETV    PNOW
        PUT9    TMARK+2
        GETV    PNOW+1
        PUT9    TMARK+3
tpl10:  LDI     8                       ; every column drawn
        FCALL   fld_flush
        BNZ     tpl10
        LDI     0A5H
        PUT9    TMARK+15
        FJMP    t_end

; ---- the scenes' data -----------------------------------------------------------------
        DFIT    256
TERR1:  DB      66, 69, 71, 73, 74, 75, 75, 74, 73, 72, 71, 71, 70, 70, 70, 70
        DB      71, 72, 73, 74, 75, 76, 76, 76, 75, 74, 72, 69, 66, 62, 59, 55
        DB      50, 47, 43, 40, 38, 37, 36, 36, 37, 38, 40, 42, 44, 47, 49, 51
        DB      53, 54, 55, 55, 56, 56, 55, 55, 55, 55, 56, 57, 58, 60, 63, 65
; a settle's 30 columns: dirt lower, a few with a cave (2 spans)
TERR2:  DB      80, 81, 82, 0F2H, 40, 60, 70, 108, 83, 84, 85, 0F2H, 45, 62, 75, 108
        DB      86, 86, 86, 86, 85, 84, 83, 82, 0F1H, 50, 108, 80, 79, 78, 77, 76
        DB      75, 74, 73, 72, 71, 70, 69, 68

; ---- scene 4: the art ------------------------------------------------------------------
        RFIT    197
t_art:  GET9    TARG                    ; the dirt: its pattern, a crust
        SHR
        ANI     7
        ADI     LO(ARTPAT)
        PLO     R8
        LDI     HI(ARTPAT)
        PHI     R8
        LDN     R8
        PUTV    DIRTM
        GET9    TARG
        SHR
        SHR
        SHR
        SHR
        ANI     1
        PUTV    DCRUST
        GET9    TARG                    ; b5: the tank row (flat, 10 abreast)
        ANI     20H
        BZ      tart0
        FJMP    t_row
tart0:  LDR     R8, FIXBUF+35           ; FIXBUF's terrain
        LDI     0
        PLO     R9
        LDI     FW
        PHI     R9
        FCALL   fld_puts
        FCALL   fld_all
        LDI     0                       ; the tanks: FIXBUF's x, y; angles 15 s
        PUT9    TK
tart1:  GET9    TK
        ADI     LO(FIXBUF+3)
        PLO     R8
        LDI     HI(FIXBUF+3)
        PHI     R8
        LDN     R8                      ; x
        PHI     R9
        GLO     R8
        ADI     10
        PLO     R8
        LDN     R8                      ; y
        PLO     RC
        LDI     1
        PLO     R9
        GET9    TK                      ; the step: the seat's (0-9)
        ADI     1
        PHI     RC
        GET9    TK
        FCALL   tk_set
        GET9    TK
        ADI     1
        PUT9    TK
        XRI     SEATS
        BNZ     tart1
        FJMP    tart2

        RFIT    177
tart2:    FCALL   tk_pass                 ; (two images a pass)
        GETV    TKDRT
        BNZ     tart2
        GETV    TKDRT+1
        BNZ     tart2
        LDI     12                      ; three fireballs in the sky (y 22):
        PLO     R9                      ;  CHECK, DOT, HATCH; one on the ground
        LDI     22
        PHI     R9
        LDI     TM_SET+TP_CHECK
        FCALL   t_ball
        LDI     32
        PLO     R9
        LDI     22
        PHI     R9
        LDI     TM_SET+TP_DOT
        FCALL   t_ball
        LDI     52
        PLO     R9
        LDI     22
        PHI     R9
        LDI     TM_SET+TP_HATCH
        FCALL   t_ball
        LDI     20
        PLO     R9
        LDI     0
        PHI     R9
        LDI     TM_SET+TP_CHECK
        FCALL   t_ball
        PARS                            ; a shell: image 20 (mask 21), sprite 10
        P       20
        P       3
        P       4
        P       0
        P       40H
        P       40H
        P       0
        GOP     TOP_IMAGE
        PARS
        P       21
        P       3
        P       4
        P       0E0H
        P       0E0H
        P       0E0H
        P       0E0H
        GOP     TOP_IMAGE
        PARS
        P       10
        P       20
        P       22
        P       30
        P       TM_MASK
        GOP     TOP_SPRITE
        FJMP    tart3

        RFIT    105
tart3:  SETV    TXM, TM_SET+TM_SHORT    ; the HUD
        LDR     R8, s_arth1
        LDI     0
        PLO     R9
        LDI     HUD_Y1
        PHI     R9
        FCALL   txt_at16
        LDR     R8, s_arth2
        LDI     0
        PLO     R9
        LDI     HUD_Y2
        PHI     R9
        FCALL   txt_at16
        GET9    TARG                    ; day: inverted
        ANI     1
        BZ      tart9
        PARS
        P       0
        P       0
        P       TV_INVERT
        GOP     TOP_VIEW
tart9:    FJMP    t_end
s_arth1: DB     GL_MARK, "SPARK A 74P 550", 0
s_arth2: DB     "MINI MIS 12 <17", 0

; t_ball: a fireball (rx 3, ry 8) at x R9.0, y R9.1 (0: on the ground
;   there), mode D.
        RFIT    112
t_ball: PUT9    TMARK+8                 ; (the mode)
        GHI     R9
        BNZ     tbl1
        GLO     R9                      ; the ground: TOPS[x]
        ADI     LO(TOPS)
        PLO     RB
        LDI     HI(TOPS)
        PHI     RB
        LDN     RB
tbl1:   PUT9    TMARK+9
        GLO     R9                      ; from x - 3
        SMI     3
        PLO     R9
        LDR     R8, BALLH
tb1:    LDA     R8
        BZ      tb2
        PHI     RC                      ; h
        PARS
        GLO     R9
        PD
        GET9    TMARK+9                 ; y - h
        STR     R2
        GHI     RC
        SD
        PD
        P       1
        GHI     RC
        SHL
        ADI     1
        PD
        GET9    TMARK+8
        PD
        GOP     TOP_RECT
        INC     R9
        BR      tb1
tb2:    FRET

        PFIT    16
ARTPAT: DB      TM_SET+TP_SOLID, TM_SET+TP_DOT, TM_SET+TP_SPARSE, TM_SET+TP_HATCH
        DB      TM_SET+TP_CHECK, TM_SET+TP_DASH, TM_SET+TP_VDASH, TM_SET+TP_SOLID
BALLH:  DB      4, 7, 8, 8, 8, 7, 4, 0

; t_row: flat ground at line 80, the ten tanks abreast (x 4 + 6 s), each
;   at angle 20 s, the HUD; then the scene's end (day too).
        RFIT    200
t_row:  LDR     R8, ROW80
        LDI     0
        PLO     R9
        LDI     FW
        PHI     R9
        FCALL   fld_puts
        FCALL   fld_all
        LDI     0
        PUT9    TK
trw2:   GET9    TK                      ; x 4 + 6 s, y 80, step (20 s) / 15
        SHL
        STR     R2
        SHL
        ADD
        ADI     4
        PHI     R9
        LDI     80
        PLO     RC
        GET9    TK
        SHL
        SHL
        STR     R2
        SHL
        SHL
        ADD
        FCALL   ang_step
        PHI     RC
        LDI     1
        PLO     R9
        GET9    TK
        FCALL   tk_set
        GET9    TK
        ADI     1
        PUT9    TK
        XRI     SEATS
        BNZ     trw2
trw3:   FCALL   tk_pass
        GETV    TKDRT
        BNZ     trw3
        GETV    TKDRT+1
        BNZ     trw3
        FJMP    tart3
ROW80:  DB      80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80
        DB      80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80
        DB      80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80
        DB      80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80

; ---- scene 5: the screens on a canned state -------------------------------------
        RFIT    203
t_screens:
        POKEAT  ST                      ; the state
        LDR     R8, FIXBUF
        LDR     R9, STATE_SIZE
tsc6:   LDA     R8
        PKD
        DEC     R9
        GLO     R9
        BNZ     tsc6
        GHI     R9
        BNZ     tsc6
        PKEND
        LDR     R8, FIXBUF+510          ; the catalog
        LDA     R8
        PUTV    CATN
        PLO     R9
        LDN     R8
        PUTV    CATN+1
        PHI     R9
        SETV    CATOK, 0
        POKEAT  CATBUF
        LDR     R8, FIXBUF+512
tsc7:   LDA     R8
        PKD
        DEC     R9
        GLO     R9
        BNZ     tsc7
        GHI     R9
        BNZ     tsc7
        PKEND
        GETS    OFF_MY_SEAT             ; who we are; the link heard, quiet
        PUTV    MYSEAT
        SETV    NLINK, NL_LINK+NL_JOINED+NL_HEARD
        GETV    FLAGS
        ORI     FL_TEST
        STR     RA
        SETV    TESTEV, EV_STATE
        LDR     R8, s_troom
        LDR     R9, SRVTBL
        FCALL   strcpy
        FJMP    room2
s_troom: DB     "test", 0

; strcpy: the string at R8 to R9 (its NUL too).
        RFIT    6
strcpy: LDA     R8
        STR     R9
        INC     R9
        BNZ     strcpy
        FRET

; ---- scene 6: the sounds -----------------------------------------------------------
        RFIT    117
t_sounds:
        OPN     TOP_CLS
        GET9    TARG
        PUTV    MRC                     ; the id
        SMI     MUS_TITLE
        BDF     tsn4
tsn1:   GETV    MRC
        FCALL   snd_play
        LDI     180
        PUT9    TK
tsn2:   FCALL   frame
        FCALL   snd_tick
        GET9    TK
        SMI     1
        PUT9    TK
        BNZ     tsn2
        INCV    MRC
        XRI     MUS_TITLE
        BNZ     tsn1
tsn3:   FCALL   frame
        FCALL   snd_tick
        BR      tsn3
tsn4:   GETV    MRC
        FCALL   snd_play
        BR      tsn3
