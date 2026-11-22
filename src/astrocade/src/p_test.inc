; p_test.inc -- the rigs' offline scenes (src/test.c). At start-up a rig
; pokes TESTHK (the boot page waits 20 frames for it): 5AH in [0], the
; scene in [2], its argument in [3]; we answer A5H in [0]. Scenes:
;   3  the fixture's shot: scene 1's field on the fixture's sky, then its
;      segments played in turn; TESTHK+3 = 99 when it is over (refcheck)
;   1  the raster: the fixture's field (test/fixture.inc), its tanks, the
;      HUD, a patch of fire, the bands. [3]: b0-2 the sky (SKY_*), b4-6 the
;      dirt's palette, b3 the turn ours (else seat 1's), b5 (with b7) the
;      seats' chart, b6 a busy loop, b7 a flat field
;      with the ten tanks in a row (the shimmer's worst case).

        JP      TSTART          ; E_ENTER (never comes back)
        RET                     ; E_STATE
        NOP
        NOP
        RET                     ; E_TICK
        NOP
        NOP

TSTART: LD      A,(TESTHK+2)
        CP      3               ; 3: the fixture's shot, on its own sky
        JR      NZ,TSTA1
        LD      A,FIXSKY
        AND     7
        LD      (TESTHK+3),A
        JR      TSTA2
TSTA1:  CP      2               ; 2: a sound (TESTHK+3 its id; 0FEH a falling
        JP      Z,TSOUND        ;  shell's whistle, then its boom)
        CP      1
        JP      NZ,TSTART9
TSTA2:
; ---- scene 1: the raster -------------------------------------------------------------
        LD      HL,SCR          ; the HUD and the field: all tank pixels (0), then drawn
        LD      DE,SCR+1
        LD      BC,LINES*BYTEPL-1
        LD      (HL),0
        LDIR
        LD      A,(TESTHK+3)    ; the sky
        AND     7
        CP      7
        JR      C,TS1
        XOR     A
TS1:    LD      L,A             ; SKYTB = SKYTAB + 16 sky
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      DE,SKYTAB
        ADD     HL,DE
        LD      (SKYTB),HL
        LD      A,(TESTHK+3)    ; the dirt
        RRCA
        RRCA
        RRCA
        RRCA
        AND     7
        LD      HL,DIRTTAB
        ADD     A,L
        LD      L,A
        JR      NC,TS2
        INC     H
TS2:    LD      A,(HL)
        LD      (DIRTC),A
        LD      A,FIRESTL
        LD      (FIREC),A
        XOR     A               ; we are seat 0, the turn seat 1's (b3: ours)
        LD      (MYSEAT),A
        LD      A,(TESTHK+3)
        AND     08H
        LD      A,0
        JR      NZ,TS2A
        INC     A
TS2A:   LD      (ST+OFF_TURN),A
        LD      A,(ST+OFF_TURN) ; the HUD: the turn's seat's colour on the left
        LD      E,A
        LD      D,0
        LD      HL,SEATC
        ADD     HL,DE
        LD      A,(HL)
        LD      (HUDC),A
        LD      HL,SEATTX
        ADD     HL,DE
        LD      A,(HL)
        LD      (HUDC+1),A
        LD      A,HUDR0
        LD      (HUDC+2),A
        LD      A,HUDR1
        LD      (HUDC+3),A
        LD      A,844/25        ; the bars: power 844 of 1000, energy 100
        LD      (BARPW),A
        LD      A,40
        LD      (BARENG),A
        LD      A,(TESTHK+3)
        BIT     7,A
        JR      NZ,TSFLAT
        LD      IX,FIXTERR      ; the fixture's field
        LD      B,0
TS3:    PUSH    BC
        CALL    TCOL
        POP     BC
        INC     B
        LD      A,B
        CP      FIELD_W
        JR      C,TS3
        LD      HL,FIXTX        ; its tanks
        LD      DE,TKX
        LD      BC,SEATS
        LDIR
        LD      HL,FIXTY
        LD      DE,TKY
        LD      BC,SEATS
        LDIR
        LD      HL,FIXTA
        LD      DE,TKA
        LD      BC,SEATS
        LDIR
        JP      TS5
TSFLAT: LD      B,0             ; a flat field, row 40 down (the chart: 50)
TS4:    PUSH    BC
        LD      IX,TFLATE
        LD      A,(TESTHK+3)
        BIT     5,A
        JR      Z,TS4V
        LD      IX,TFLATC
TS4V:   CALL    TCOL
        POP     BC
        INC     B
        LD      A,B
        CP      FIELD_W
        JR      C,TS4
        LD      A,(TESTHK+3)    ; b5: the seats' chart (two staircases: each
        BIT     5,A             ;  band holds two tanks, so each shows its own)
        JR      Z,TS4Z
        LD      B,0
TS4S:   LD      E,B
        LD      D,0
        LD      A,B             ; x: 12 + 14 (s mod 5), +80 for seats 5-9
        CP      5
        LD      C,12
        JR      C,TS4T
        SUB     5
        LD      C,92
TS4T:   PUSH    AF
        LD      L,A
        ADD     A,A
        ADD     A,A
        ADD     A,A
        SUB     L               ; 7 (s mod 5)
        ADD     A,A
        ADD     A,C
        LD      HL,TKX
        ADD     HL,DE
        LD      (HL),A
        POP     AF              ; y: 13 + 8 (s mod 5): each in bands of its own
        ADD     A,A
        ADD     A,A
        ADD     A,A
        ADD     A,13
        LD      HL,TKY
        ADD     HL,DE
        LD      (HL),A
        LD      HL,TKA          ; (barrels level: 0 left, 180 right)
        ADD     HL,DE
        LD      A,B
        CP      5
        LD      A,180
        JR      NC,TS4U
        XOR     A
TS4U:   LD      (HL),A
        INC     B
        LD      A,B
        CP      SEATS
        JR      C,TS4S
        JP      TS5
TS4Z:   LD      B,0             ; ten tanks in a row: x = 8 + 16 s, y = 40,
TS4A:   LD      C,B             ;  angles 20 .. 160
        LD      HL,TKX
        LD      E,B
        LD      D,0
        ADD     HL,DE
        LD      A,B
        ADD     A,A
        ADD     A,A
        ADD     A,A
        ADD     A,A
        ADD     A,8
        LD      (HL),A
        LD      HL,TKY
        ADD     HL,DE
        LD      (HL),40
        LD      HL,TKA
        ADD     HL,DE
        LD      A,B
        ADD     A,A
        ADD     A,A
        ADD     A,A
        ADD     A,A
        ADD     A,20
        LD      (HL),A
        INC     B
        LD      A,B
        CP      SEATS
        JR      C,TS4A
TS5:    LD      HL,TKON         ; every tank on, drawn
        LD      B,SEATS
TS6:    LD      (HL),1
        INC     HL
        DJNZ    TS6
        XOR     A
TS7:    PUSH    AF
        FAR     PG_PLAY,TANKD
        POP     AF
        INC     A
        CP      SEATS
        JR      C,TS7
        LD      A,(TESTHK+2)    ; (the shot: no fire, no HUD)
        CP      3
        JR      Z,TS9Z
        LD      B,70            ; a patch of fire (its colour cycles), in the sky
TS8:    LD      C,6
TS9:    LD      E,PX_FX
        PUSH    BC
        FAR     PG_PLAY,FP_PLOT
        POP     BC
        INC     C
        LD      A,C
        CP      12
        JR      C,TS9
        INC     B
        LD      A,B
        CP      90
        JR      C,TS8
        LD      A,(FLAGS)
        OR      FL_FIRE
        LD      (FLAGS),A
        LD      HL,THUDL        ; the HUD: the left half on the seat's colour,
        LD      E,0             ;  the right on the dark strip (ink 1, ground 0);
        CALL    THUD            ;  (copied into WORK: TX5AT is the PLAY page's)
        LD      HL,THUDR
        LD      E,20
        CALL    THUD
TS9Z:   FAR     PG_OPS,SOLVE
        CALL    FIELDSCR
        LD      A,(TESTHK+2)
        CP      3
        JP      Z,TSHOT
        LD      A,(TESTHK+3)    ; b6: a busy loop (the worst instruction: 23
        BIT     6,A             ;  cycles), not FRAME's HALT: the bands' timing
        JR      Z,TSLOOP
        LD      IX,WORK
TSBUSY: INC     (IX+0)
        JR      TSBUSY
TSLOOP: LD      A,30            ; every half second the shimmer's order turns
        LD      (RCOUNT),A
TSL1:   CALL    FRAME
        LD      HL,RCOUNT
        DEC     (HL)
        JR      NZ,TSL1
        LD      A,(SROT)
        INC     A
        CP      SEATS
        JR      C,TSL2
        XOR     A
TSL2:   LD      (SROT),A
        FAR     PG_OPS,SOLVE
        JR      TSLOOP
TSTART9: JP     TSLOOP

; TSOUND: scene 2: half a second of quiet, sound TESTHK+3, then its time
; (a tune's: to its end; the title's 30 s, then off); TESTHK+3 = 99 after
; (tools/astrosim/sndwav.py cuts the WAV there).
TSOUND: LD      B,30
        CALL    TWAIT
        LD      A,(TESTHK+3)
        CP      0FEH
        JR      Z,TSW
        CALL    SNDQUE
        LD      A,(TESTHK+3)
        LD      BC,240          ; (an effect: 4 s)
        CP      MUS_TITLE
        JR      C,TSO1
        LD      BC,300          ; (a jingle: 5 s)
        JR      NZ,TSO1
        LD      BC,1860         ; (the title: once round, and a bit)
TSO1:   PUSH    BC
        LD      B,1
        CALL    TWAIT
        POP     BC
        DEC     BC
        LD      A,B
        OR      C
        JR      NZ,TSO1
        CALL    SNDOFF
TSO9:   LD      A,99
        LD      (TESTHK+3),A
TSO10:  HALT
        JR      TSO10
TSW:    LD      A,(FLAGS)       ; the whistle: the field's (A free), a shell
        OR      FL_FIELD        ;  falling from line 1 to 55 in 1.5 s, then
        LD      (FLAGS),A       ;  its boom
        LD      C,1
TSW1:   LD      A,C
        LD      B,A
        RRCA
        RRCA
        RRCA
        AND     1FH
        ADD     A,B
        ADD     A,35
        LD      (WHIST),A
        LD      B,2
        PUSH    BC
        CALL    TWAIT
        POP     BC
        INC     C
        INC     C
        LD      A,C
        CP      56
        JR      C,TSW1
        XOR     A
        LD      (WHIST),A
        LD      A,SND_BOOM0+1
        CALL    SNDQUE
        LD      BC,180
        JR      TSO1
; TWAIT: B frames.
TWAIT:  PUSH    BC
        CALL    WAIT1
        POP     BC
        DJNZ    TWAIT
        RET

; THUD: the HUD text at HL (this page's) at column E, through WORK.
THUD:   PUSH    DE
        LD      DE,WORK
        LD      BC,24
        LDIR
        POP     DE
        LD      HL,WORK
        LD      C,1*4+0
        FAR     PG_PLAY,TX5AT
        RET

; TSHOT: scene 3: the fixture's segments in turn into SCRIPT, each played
; (the PLY page); then TESTHK+3 = 99 (refcheck reads the field then).
TSHOT:  LD      A,98            ; (refcheck times the shot from here)
        LD      (TESTHK+3),A
        XOR     A
        LD      (RCOUNT),A
TSH1:   LD      A,(RCOUNT)
        CP      FIXNSEG
        JR      NC,TSH9
        LD      B,A             ; its bytes: FIXSEG + the lengths before it
        LD      HL,FIXSEG
        LD      DE,FIXSLEN
        OR      A
        JR      Z,TSH3
TSH2:   PUSH    BC
        LD      A,(DE)
        LD      C,A
        INC     DE
        LD      A,(DE)
        LD      B,A
        INC     DE
        ADD     HL,BC
        POP     BC
        DJNZ    TSH2
TSH3:   LD      A,(DE)          ; its length
        LD      (SEGLEN),A
        LD      C,A
        LD      B,0
        LD      DE,SCRIPT
        LDIR
        LD      A,(RCOUNT)
        LD      (SEGNO),A
        INC     A
        CP      FIXNSEG
        LD      A,0
        JR      NZ,TSH4
        LD      A,S_LAST_SEG
TSH4:   LD      (SEGFLG),A
        GOPAGE  PG_PLY,E_ENTER
        LD      HL,RCOUNT
        INC     (HL)
        JR      TSH1
TSH9:   LD      A,99
        LD      (TESTHK+3),A
        JP      TSLOOP

THUDL:  DB      "SPARKY 170  844 100",0
THUDR:  DB      " <17  6  MINI NUKE  3",0
; TCOL: column B from the entry at IX (this page's ROM), copied into WORK
; first (FP_COLW runs in the PLAY page, mapped where this one was: it can't
; read ours); IX moves past it.
TCOL:   PUSH    BC
        PUSH    IX
        POP     HL
        LD      DE,WORK
        LD      A,(HL)          ; its length: 1, or 1 + 2 n
        LD      C,1
        CP      0F0H
        JR      C,TC1
        SUB     0F0H
        ADD     A,A
        INC     A
        LD      C,A
TC1:    LD      B,0
        PUSH    BC
        LDIR
        POP     BC
        ADD     IX,BC
        POP     BC
        PUSH    IX
        LD      IX,WORK
        FAR     PG_PLAY,FP_COLW
        POP     IX
        RET

TFLATE: DB      40              ; a column solid from row 40
TFLATC: DB      50

        INCLUDE "fixture.inc"
