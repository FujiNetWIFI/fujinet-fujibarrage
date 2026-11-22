; p_ply.inc -- the script player (src/atari/ply.s, trk.s; docs/PROTOCOL.md
; "Playing a segment"): each frame every track, blast, path, flow, beam
; and settle moves a step, then the ops due run. Paths, flows and beams are
; the OPS page's (FAR); the ops about tanks, talk, damage and sound go to
; the PLAY page (FAR COP), which reads their arguments at PLYPC.
;
; The segment is in SCRIPT (SEGLEN bytes); PLAYSEG plays it to its end,
; frames the clock (at most 8 steps a pass when behind, drawn once).

        JP      PLAYSEG         ; E_ENTER: the segment, played
        JP      PLYRESET        ; E_STATE: a new shot: nothing moving
        JP      PLYSTOP         ; E_TICK: the script's end: every track gone

NBLAST  EQU     6
NSETTLE EQU     8
DIRTBALL EQU    80H             ; an OP_DIRT, kept with the blasts
BLSZ    EQU     10              ; a blast: kind, n, tot (0: off), rx, ry, dn, x16, y16
TKSZ    EQU     21              ; a track (below)
STSZ    EQU     5               ; a settle: left, n (0: off), x16, at (script offset)

; a track's bytes (IX): its style and flags, X (3), Y (3), VX (3), VY (3),
; AX (2), AY (2), n (2), the head drawn: x (FF none), y. TK_STY: b0-3 the
; style, b4 on, b5-6 the pixel under the head.
TK_STY  EQU     0
TK_X    EQU     1
TK_Y    EQU     4
TK_VX   EQU     7
TK_VY   EQU     10
TK_AX   EQU     13
TK_AY   EQU     15
TK_N    EQU     17
TK_HX   EQU     19
TK_HY   EQU     20
TK_ONB  EQU     4               ; (the bit)

; ---- a new shot, its end -------------------------------------------------------------
PLYRESET:
        LD      HL,TRACKS       ; nothing moving: tracks, blasts, settles
        LD      DE,TRACKS+1
        LD      BC,16*TKSZ+NBLAST*BLSZ+NSETTLE*STSZ-1
        LD      (HL),0
        LDIR
        XOR     A
        LD      (PLYWAIT),A
        LD      (PLYWAIT+1),A
        LD      IX,TRACKS       ; no head drawn
        LD      B,16
PR1:    LD      (IX+TK_HX),0FFH
        LD      DE,TKSZ
        ADD     IX,DE
        DJNZ    PR1
        FAR     PG_OPS,OPSRESET
        RET

; PLYSTOP: the script's end: every track gone (their heads off).
PLYSTOP: LD     IX,TRACKS
        LD      B,16
YPS1:    PUSH    BC
        CALL    TROFF
        RES     TK_ONB,(IX+TK_STY)
        LD      DE,TKSZ
        ADD     IX,DE
        POP     BC
        DJNZ    YPS1
        RET

; ---- a segment ---------------------------------------------------------------------
; PLAYSEG: SCRIPT's segment played to its end (play.c play_segment): a step
; each frame (up to 8 a pass when behind), then what moved drawn; at its
; end the settles land and the paths and flows finish (the OPS page).
PLAYSEG: LD     A,(SEGNO)       ; a new shot
        OR      A
        CALL    Z,PLYRESET
        XOR     A
        LD      (PLYPC),A
        LD      (PLYDONE),A
        LD      HL,(FRAMES)
        LD      (PLAST),HL
PG1:    LD      A,(PLYDONE)
        OR      A
        JR      NZ,PG9
PG2:    LD      HL,(PLAST)      ; the frames since the last pass (1-8)
        LD      DE,(FRAMES)
        EX      DE,HL
        OR      A
        SBC     HL,DE
        LD      A,H
        OR      L
        JR      NZ,PG3
        HALT
        JR      PG2
PG3:    LD      A,H
        OR      A
        JR      NZ,PG3A
        LD      A,L
        CP      9
        JR      C,PG4
PG3A:   LD      A,8
PG4:    LD      B,A
        LD      L,A
        LD      H,0
        LD      DE,(PLAST)
        ADD     HL,DE
        LD      (PLAST),HL
PG5:    PUSH    BC
        CALL    PLYSTEP
        POP     BC
        LD      A,(PLYDONE)
        OR      A
        JR      NZ,PG6
        DJNZ    PG5
PG6:    CALL    PLYDRAW         ; drawn once a pass
        LD      A,(RECOL)       ; the tanks' colours if they moved (here, not
        OR      A               ;  from PDRAWN: FAR calls nested are the
        JR      Z,PG6A          ;  stack's deepest)
        XOR     A
        LD      (RECOL),A
        FAR     PG_OPS,SOLVE
PG6A:   FAR     PG_PLAY,PDRAWN  ;  (the HUD)
        CALL    WHISTLE
        JR      PG1
PG9:    CALL    PLYEND
        XOR     A               ; (the whistle stops at a cut)
        LD      (WHIST),A
        LD      A,(SEGFLG)      ; the script's last: everything stops
        AND     S_LAST_SEG
        RET     Z
        CALL    PLYSTOP
        FAR     PG_OPS,OPSBEAMSOFF
        FAR     PG_PLAY,PSCREND
        RET

; WHISTLE: the lowest falling shell's whistle (FALL: its line | 1; play.c
; draw's snd_whistle), lower as it drops: A's period 35 + 9/8 line, as the
; INTV's 932 Hz down to 330; none while someone talks.
WHISTLE: LD     A,(TALKLEFT)
        OR      A
        LD      A,0
        JR      NZ,WH1
        LD      A,(FALL)
        OR      A
        JR      Z,WH1
        LD      B,A
        RRCA
        RRCA
        RRCA
        AND     1FH
        ADD     A,B
        ADD     A,35
WH1:    LD      (WHIST),A
        RET

; PLYSTEP: a frame (ply.s ply_step): the step of what moves (not on the
; first: only its ops), then the ops due.
PLYSTEP: LD     A,(PLYPC)
        OR      A
        CALL    NZ,ANIM
        LD      HL,(PLYWAIT)
        LD      A,H
        OR      L
        JR      Z,PST1
        DEC     HL
        LD      (PLYWAIT),HL
PST1:   LD      HL,(PLYWAIT)
        LD      A,H
        OR      L
        RET     NZ
        LD      A,(PLYDONE)
        OR      A
        RET     NZ
        LD      A,(PLYPC)       ; pc < the segment's length?
        LD      HL,SEGLEN
        CP      (HL)
        RET     NC
        CALL    OP
        JR      PST1

; ADV: PLYPC += A. Preserves BC, DE.
ADV:    LD      HL,PLYPC
        ADD     A,(HL)
        LD      (HL),A
        RET

; PCARGS: HL = SCRIPT + PLYPC (an op's arguments). Preserves BC, DE.
PCARGS: LD      A,(PLYPC)
        LD      HL,SCRIPT
        ADD     A,L
        LD      L,A
        RET     NC
        INC     H
        RET

; ---- the ops ------------------------------------------------------------------------
OP:     CALL    PCARGS
        LD      A,(HL)          ; the op
        PUSH    AF
        LD      A,1
        CALL    ADV
        CALL    PCARGS          ; HL: its arguments
        POP     AF
        CP      OP_WAIT
        JR      Z,OWAIT
        CP      OP_END
        JR      Z,OEND
        CP      OP_ARC
        JR      Z,OARC
        CP      OP_STOP
        JR      Z,OSTOP
        CP      OP_PATH
        JR      Z,OPATH
        CP      OP_FLOW
        JR      Z,OFLOW
        CP      OP_BEAM
        JR      Z,OBEAM
        CP      OP_EXPLODE
        JP      Z,OBLAST
        CP      OP_DIRT
        JP      Z,OBLAST
        CP      OP_SETTLE
        JP      Z,OSETTLE
        CP      OP_WIND
        JR      Z,OWIND
        CP      OP_SHAKE
        JR      Z,OSHAKE
        CP      OP_TALK         ; 30H-3AH: the PLAY page's
        JR      C,OEND
        CP      OP_FIRE+1
        JR      NC,OEND
        FAR     PG_PLAY,COP
        RET
OEND:   LD      A,1             ; OP_END, or not ours: the segment is done
        LD      (PLYDONE),A
        RET
OWAIT:  LD      A,(HL)          ; n (0: 256)
        LD      (PLYWAIT),A
        OR      A
        LD      A,0
        JR      NZ,OW1
        INC     A
OW1:    LD      (PLYWAIT+1),A
        LD      A,1
        JR      ADV
OARC:   CALL    TRKLOAD
        LD      A,18
        JR      ADV
OSTOP:  LD      A,(HL)
        CALL    TRKSTOP
        LD      A,1
        JP      ADV
OPATH:  FAR     PG_OPS,OPSPATH  ; A: the bytes it took
        JP      ADV
OFLOW:  FAR     PG_OPS,OPSFLOW
        JP      ADV
OBEAM:  FAR     PG_OPS,OPSBEAM
        JP      ADV
OWIND:  LD      A,(HL)
        LD      (ST+OFF_WIND),A
        INC     HL
        LD      A,(HL)
        LD      (ST+OFF_WIND+1),A
        LD      A,2
        JP      ADV
OSHAKE: LD      A,(HL)
        LD      (SHAKE),A
        LD      A,1
        JP      ADV

; EXPLODE: x16, y16, rx, ry, kind, frames; DIRT: the same without kind.
; The first free blast (or the last).
OBLAST: PUSH    AF
        PUSH    HL
        LD      IX,BLASTS
        LD      B,NBLAST-1
YOB1:    LD      A,(IX+2)        ; (tot 0: free)
        OR      A
        JR      Z,YOB2
        LD      DE,BLSZ
        ADD     IX,DE
        DJNZ    YOB1
YOB2:    POP     HL
        LD      A,(HL)          ; x16, y16
        LD      (IX+6),A
        INC     HL
        LD      A,(HL)
        LD      (IX+7),A
        INC     HL
        LD      A,(HL)
        LD      (IX+8),A
        INC     HL
        LD      A,(HL)
        LD      (IX+9),A
        INC     HL
        LD      A,(HL)          ; rx, ry
        LD      (IX+3),A
        INC     HL
        LD      A,(HL)
        LD      (IX+4),A
        INC     HL
        LD      C,6             ; (the bytes so far)
        POP     AF
        CP      OP_DIRT
        LD      A,DIRTBALL
        JR      Z,OB3
        LD      A,(HL)          ; kind
        INC     HL
        INC     C
OB3:    LD      (IX+0),A
        CP      EX_NUKE         ; a nuke shakes the screen
        JR      NZ,OB4
        LD      A,30
        LD      (SHAKE),A
OB4:    LD      A,(HL)          ; frames (0: 1)
        OR      A
        JR      NZ,OB5
        INC     A
OB5:    LD      (IX+2),A
        XOR     A
        LD      (IX+1),A        ; n
        LD      (IX+5),A        ; dn
        LD      A,C
        INC     A
        JP      ADV

; SETTLE: x16, n, frames, n column entries: kept (landing after frames),
; or written now (frames 0). Full: the oldest lands now.
OSETTLE: LD     IX,SETTLES
        LD      B,NSETTLE
OS1:    LD      A,(IX+1)        ; (n 0: free)
        OR      A
        JR      Z,OS2
        LD      DE,STSZ
        ADD     IX,DE
        DJNZ    OS1
        LD      IX,SETTLES      ; full: the oldest lands now
        PUSH    HL
        CALL    SETTLE
        POP     HL
OS2:    LD      A,(HL)          ; x16
        LD      (IX+2),A
        INC     HL
        LD      A,(HL)
        LD      (IX+3),A
        INC     HL
        LD      A,(HL)          ; n
        LD      (IX+1),A
        INC     HL
        LD      A,(HL)          ; frames
        LD      (IX+0),A
        LD      A,4
        CALL    ADV
        LD      A,(PLYPC)       ; its columns start here
        LD      (IX+4),A
        LD      B,(IX+1)        ; past them
        LD      A,B
        OR      A
        JR      Z,OS5           ; (no columns: nothing to keep)
OS3:    PUSH    BC
        CALL    PCARGS
        CALL    ELEN
        CALL    ADV
        POP     BC
        DJNZ    OS3
OS5:    LD      A,(IX+0)        ; frames 0: now
        OR      A
        RET     NZ
        JP      SETTLE

; ELEN: A = the length of the column entry at HL (1, or 1 + 2 a span).
ELEN:   LD      A,(HL)
        CP      0F1H
        LD      A,1
        RET     C
        LD      A,(HL)
        SUB     0F0H
        ADD     A,A
        INC     A
        RET

; SETTLE: settle IX's columns written (fld_col each), and it is off. Four
; solid columns in one byte go together (YGROUP); the rest one at a time.
SETTLE: LD      A,(IX+1)
        OR      A
        RET     Z
        LD      (STN),A
        LD      A,(IX+4)        ; its entries in SCRIPT
        LD      HL,SCRIPT
        ADD     A,L
        LD      L,A
        JR      NC,YSE1
        INC     H
YSE1:   LD      E,(IX+2)        ; DE: its x
        LD      D,(IX+3)
        PUSH    IX
YSE2:   LD      A,D             ; on the field?
        OR      A
        JR      NZ,YSE3
        LD      A,E
        CP      FIELD_W
        JR      NC,YSE3
        AND     3               ; a byte's first column, 4 left, all solid:
        JR      NZ,YSE2A        ;  the four at once
        LD      A,(STN)
        CP      4
        JR      C,YSE2A
        PUSH    HL
        LD      B,4
YSE2B:  LD      A,(HL)
        CP      0F1H
        JR      NC,YSE2C
        INC     HL
        DJNZ    YSE2B
YSE2C:  POP     HL              ; (B 0: all four solid)
        LD      A,B
        OR      A
        JR      NZ,YSE2A
        PUSH    HL
        PUSH    DE
        LD      B,E
        CALL    YGROUP
        POP     DE
        POP     HL
        INC     HL
        INC     HL
        INC     HL
        INC     HL
        INC     DE
        INC     DE
        INC     DE
        INC     DE
        LD      A,(STN)
        SUB     4
        LD      (STN),A
        JR      NZ,YSE2
        JR      YSE5
YSE2A:  PUSH    HL              ; a column: written; past its entry
        PUSH    DE
        LD      B,E
        PUSH    HL
        POP     IX
        CALL    PY_COLW
        POP     DE
        POP     HL
YSE3:   CALL    ELEN            ; (off the field: only past it)
        ADD     A,L
        LD      L,A
        JR      NC,YSE4
        INC     H
YSE4:   INC     DE
        LD      A,(STN)
        DEC     A
        LD      (STN),A
        JR      NZ,YSE2
YSE5:   POP     IX
        LD      (IX+1),0
        RET

; YGROUP: columns B to B+3 (one byte; B a multiple of 4) from the four
; solid entries at HL (00-F0): each sky above its top, dirt from it down,
; written a byte a row, the byte changing only at a top.
YGTOP   EQU     WORK+24         ; the tops left (FF: passed)
YGCUR   EQU     WORK+28         ; the byte now
YGROW   EQU     WORK+29         ; the row it starts at
YGROUP: LD      DE,YGTOP
        LD      C,4
YG1:    LD      A,(HL)          ; (F0, or past the field: sky all the way)
        CP      FIELD_H
        JR      C,YG2
        LD      A,FIELD_H
YG2:    LD      (DE),A
        INC     HL
        INC     DE
        DEC     C
        JR      NZ,YG1
        LD      C,0
        CALL    PY_ADR          ; HL: the byte at the top
        LD      A,PX_SKY*55H
        LD      (YGCUR),A
        XOR     A
        LD      (YGROW),A
YG3:    LD      DE,YGTOP        ; the next top (FIELD_H: none before the bottom)
        LD      B,4
        LD      C,FIELD_H
YG4:    LD      A,(DE)
        CP      C
        JR      NC,YG5
        LD      C,A
YG5:    INC     DE
        DJNZ    YG4
        LD      A,(YGROW)       ; rows to it: the byte now
        LD      B,A
        LD      A,C
        SUB     B
        JR      Z,YG7
        LD      B,A
        LD      A,(YGCUR)
        LD      DE,BYTEPL
YG6:    LD      (HL),A
        ADD     HL,DE
        DJNZ    YG6
YG7:    LD      A,C
        CP      FIELD_H
        RET     NC
        LD      (YGROW),A
        LD      DE,YGTOP        ; each column whose top it is: dirt from here
        LD      B,0C0H          ;  (its pixel's bits, from the left)
YG8:    LD      A,(DE)
        CP      C
        JR      NZ,YG9
        LD      A,0FFH
        LD      (DE),A
        LD      A,(YGCUR)       ; sky 01 ^ 11 = dirt 10
        XOR     B
        LD      (YGCUR),A
YG9:    INC     DE
        SRL     B
        SRL     B
        JR      NZ,YG8
        JR      YG3
; ---- a frame's step --------------------------------------------------------------
; ANIM: the tracks a step; each blast grows (its last frame: the crater, or
; the dirt); the paths, flows and beams (OPS); the settles count down; the
; fire's colour runs while anything burns; the talk's chatter.
ANIM:   CALL    TRKSTEP
        XOR     A
        LD      (PLFIRE),A
        LD      IX,BLASTS
        LD      B,NBLAST
AN1:    LD      A,(IX+2)
        OR      A
        JR      Z,AN4
        INC     (IX+1)          ; n++
        LD      A,(IX+1)
        CP      (IX+2)
        JR      NC,AN2
        LD      A,1             ; still growing: fire
        LD      (PLFIRE),A
        JR      AN4
AN2:    PUSH    BC              ; done: the crater (or the dirt) now
        LD      A,(IX+3)
        LD      C,(IX+4)
        CALL    ELARGS
        LD      A,(IX+0)
        CP      DIRTBALL
        LD      B,PX_DIRT
        LD      C,1
        JR      Z,AN3
        CP      EX_PLASMA
        LD      B,PX_SKY
        LD      C,2
        JR      Z,AN3
        CP      EX_PUFF
        JR      Z,AN3
        LD      C,0
AN3:    LD      A,B
        LD      (ELV),A
        LD      A,C
        LD      (ELMODE),A
        PUSH    IX
        CALL    PY_ELLIPSE
        POP     IX
        LD      (IX+2),0
        POP     BC
AN4:    LD      DE,BLSZ
        ADD     IX,DE
        DJNZ    AN1
        FAR     PG_OPS,OPSSTEP  ; A: a fire flow burns
        LD      HL,PLFIRE
        OR      (HL)
        LD      (HL),A
        LD      IX,SETTLES      ; the settles count down
        LD      B,NSETTLE
AN5:    LD      A,(IX+1)
        OR      A
        JR      Z,AN6
        DEC     (IX+0)
        JR      NZ,AN6
        PUSH    BC
        CALL    SETTLE
        POP     BC
AN6:    LD      DE,STSZ
        ADD     IX,DE
        DJNZ    AN5
        DI                      ; the fire's colour runs while anything burns
        LD      A,(FLAGS)
        AND     0FFH-FL_FIRE
        LD      B,A
        LD      A,(PLFIRE)
        OR      A
        LD      A,B
        JR      Z,AN7
        OR      FL_FIRE
AN7:    LD      (FLAGS),A
        EI
        FAR     PG_PLAY,CANIM
        RET

; ELARGS: the ellipse at blast IX, radii A, C (0 is 1; ry at most 200).
ELARGS: OR      A
        JR      NZ,EA1
        INC     A
EA1:    LD      (ELRX),A
        LD      A,C
        OR      A
        JR      NZ,EA2
        INC     A
EA2:    CP      201
        JR      C,EA3
        LD      A,200
EA3:    LD      (ELRY),A
        LD      L,(IX+6)
        LD      H,(IX+7)
        LD      (ELX),HL
        LD      L,(IX+8)
        LD      H,(IX+9)
        LD      (ELY),HL
        RET

; ---- the blasts drawn at their size now (ply.s ply_draw) --------------------------
PLYDRAW: LD     IX,BLASTS
        LD      B,NBLAST
PD1:    LD      A,(IX+2)
        OR      A
        JR      Z,PD3
        LD      A,(IX+1)
        CP      (IX+5)
        JR      Z,PD3
        LD      (IX+5),A
        PUSH    BC
        LD      A,(IX+3)        ; rx n / total, ry n / total
        CALL    SCALE
        PUSH    AF
        LD      A,(IX+4)
        CALL    SCALE
        LD      C,A
        POP     AF
        CALL    ELARGS
        LD      A,(IX+0)
        CP      DIRTBALL
        LD      B,PX_DIRT
        LD      C,1
        JR      Z,PD2
        LD      B,PX_FX         ; plasma, a puff: over the air only
        CP      EX_PLASMA
        JR      Z,PD2
        CP      EX_PUFF
        JR      Z,PD2
        LD      C,0             ; a crater's fireball eats the dirt
PD2:    LD      A,B
        LD      (ELV),A
        LD      A,C
        LD      (ELMODE),A
        PUSH    IX
        CALL    PY_ELLIPSE
        POP     IX
        POP     BC
PD3:    LD      DE,BLSZ
        ADD     IX,DE
        DJNZ    PD1
        RET

; SCALE: A = A * n / total of blast IX (at most A: n <= total). Preserves IX.
SCALE:  LD      E,A             ; HL = A * n
        LD      D,0
        LD      A,(IX+1)
        LD      HL,0
        LD      B,8
SC1:    ADD     HL,HL
        RLA
        JR      NC,SC2
        ADD     HL,DE
SC2:    DJNZ    SC1
        LD      E,(IX+2)        ; / total
        LD      D,0
        LD      A,0FFH
SC3:    INC     A
        OR      A
        SBC     HL,DE
        JR      NC,SC3
        RET

; ---- a segment's end ------------------------------------------------------------------
; PLYEND: the segment's bytes are going: settles land, paths and flows
; finish (OPS).
PLYEND: FAR     PG_PLAY,TFLUSH  ; (a talk's letters: now, the bytes go)
        LD      IX,SETTLES
        LD      B,NSETTLE
YPE1:    PUSH    BC
        CALL    SETTLE
        POP     BC
        LD      DE,STSZ
        ADD     IX,DE
        DJNZ    YPE1
        LD      A,(SEGFLG)      ; (the last: every flow ends now)
        AND     S_LAST_SEG
        FAR     PG_OPS,OPSSEGEND
        RET

; ---- the tracks (src/atari/trk.s) -----------------------------------------------------
; TRKIX: IX = track A's (0-15). Preserves BC, HL.
TRKIX:  AND     0FH
        PUSH    HL
        PUSH    DE
        LD      L,A             ; 21 A
        LD      H,0
        LD      D,H
        LD      E,L
        ADD     HL,HL           ; 2A
        ADD     HL,HL           ; 4A
        PUSH    HL
        ADD     HL,HL           ; 8A
        ADD     HL,HL           ; 16A
        ADD     HL,DE           ; 17A
        POP     DE
        ADD     HL,DE           ; 21A
        LD      DE,TRACKS
        ADD     HL,DE
        PUSH    HL
        POP     IX
        POP     DE
        POP     HL
        RET

; TRKSTOP: track A ends now.
TRKSTOP: CALL   TRKIX
        CALL    TROFF
        RES     TK_ONB,(IX+TK_STY)
        RET

; TRKLOAD: an OP_ARC's arguments at HL (slot, style, x16, y16, vx24, vy24,
; ax16, ay16, n16) start a track (X = x16 << 8: 12.12).
TRKLOAD: LD     A,(HL)
        CALL    TRKIX
        PUSH    HL
        CALL    TROFF           ; its old head away
        POP     HL
        INC     HL
        LD      A,(HL)          ; (the style; the flags: off)
        AND     15
        LD      (IX+TK_STY),A
        INC     HL
        LD      (IX+TK_X),0
        LD      A,(HL)
        LD      (IX+TK_X+1),A
        INC     HL
        LD      A,(HL)
        LD      (IX+TK_X+2),A
        INC     HL
        LD      (IX+TK_Y),0
        LD      A,(HL)
        LD      (IX+TK_Y+1),A
        INC     HL
        LD      A,(HL)
        LD      (IX+TK_Y+2),A
        INC     HL
        PUSH    IX              ; VX, VY, AX, AY, n: 12 bytes as they come
        POP     DE
        PUSH    HL
        LD      HL,TK_VX
        ADD     HL,DE
        EX      DE,HL
        POP     HL
        LD      BC,12
        LDIR
        SET     TK_ONB,(IX+TK_STY)
        JP      TRON

; TRKSTEP: every live track one step (trk.s trk_step): its head off, then
;       X += VX;  Y += VY;  VX += AX;  VY += AY
; its head on; the lowest falling shell's line (FALL: the whistle).
TRKSTEP: XOR    A
        LD      (FALL),A
        LD      IX,TRACKS+15*TKSZ
        LD      B,16
YTS1:    BIT     TK_ONB,(IX+TK_STY)
        JP      Z,YTS9
        PUSH    BC
        CALL    TROFF
        LD      A,(IX+TK_N)     ; no steps left: it ends
        OR      (IX+TK_N+1)
        JR      NZ,YTS2
        RES     TK_ONB,(IX+TK_STY)
        POP     BC
        JP      YTS9
YTS2:    LD      L,(IX+TK_N)
        LD      H,(IX+TK_N+1)
        DEC     HL
        LD      (IX+TK_N),L
        LD      (IX+TK_N+1),H
        LD      A,(IX+TK_X)     ; X += VX
        ADD     A,(IX+TK_VX)
        LD      (IX+TK_X),A
        LD      A,(IX+TK_X+1)
        ADC     A,(IX+TK_VX+1)
        LD      (IX+TK_X+1),A
        LD      A,(IX+TK_X+2)
        ADC     A,(IX+TK_VX+2)
        LD      (IX+TK_X+2),A
        LD      A,(IX+TK_Y)     ; Y += VY
        ADD     A,(IX+TK_VY)
        LD      (IX+TK_Y),A
        LD      A,(IX+TK_Y+1)
        ADC     A,(IX+TK_VY+1)
        LD      (IX+TK_Y+1),A
        LD      A,(IX+TK_Y+2)
        ADC     A,(IX+TK_VY+2)
        LD      (IX+TK_Y+2),A
        LD      C,0             ; VX += AX (sign-extended)
        BIT     7,(IX+TK_AX+1)
        JR      Z,YTS3
        DEC     C
YTS3:    LD      A,(IX+TK_VX)
        ADD     A,(IX+TK_AX)
        LD      (IX+TK_VX),A
        LD      A,(IX+TK_VX+1)
        ADC     A,(IX+TK_AX+1)
        LD      (IX+TK_VX+1),A
        LD      A,(IX+TK_VX+2)
        ADC     A,C
        LD      (IX+TK_VX+2),A
        LD      C,0             ; VY += AY
        BIT     7,(IX+TK_AY+1)
        JR      Z,YTS4
        DEC     C
YTS4:    LD      A,(IX+TK_VY)
        ADD     A,(IX+TK_AY)
        LD      (IX+TK_VY),A
        LD      A,(IX+TK_VY+1)
        ADC     A,(IX+TK_AY+1)
        LD      (IX+TK_VY+1),A
        LD      A,(IX+TK_VY+2)
        ADC     A,C
        LD      (IX+TK_VY+2),A
        CALL    TRON
        BIT     7,(IX+TK_VY+2)  ; falling (VY >= 0), on the field's lines,
        JR      NZ,YTS8          ;  not a digger: the lowest is the whistle's
        LD      A,(IX+TK_STY)
        AND     15
        CP      ST_DIGGER
        JR      Z,YTS8
        CALL    YTKPY            ; A: its line (C: past them)
        JR      C,YTS8
        LD      HL,FALL
        CP      (HL)
        JR      C,YTS8
        OR      1
        LD      (HL),A
YTS8:    POP     BC
YTS9:    LD      DE,-TKSZ
        ADD     IX,DE
        DEC     B
        JP      NZ,YTS1
        RET

; YTKPX: A = the head's x, (X >> 12) (carry: off the field's columns).
; YTKPY: its line (carry: off the field's lines). Clobber HL.
YTKPX:   LD      L,(IX+TK_X+1)
        LD      H,(IX+TK_X+2)
        JR      YTKP
YTKPY:   LD      L,(IX+TK_Y+1)
        LD      H,(IX+TK_Y+2)
YTKP:    SRA     H               ; (H:L) >> 4, arithmetic
        RR      L
        SRA     H
        RR      L
        SRA     H
        RR      L
        SRA     H
        RR      L
        LD      A,H
        OR      A
        SCF
        RET     NZ
        LD      A,L
        OR      A               ; (no carry)
        RET

; TRON: track IX's head on, at (X >> 12, Y >> 12): fire over sky (a smoke
; trail's over fire too), the pixel under it kept.
TRON:   CALL    YTKPX
        RET     C
        CP      FIELD_W
        RET     NC
        LD      B,A
        CALL    YTKPY
        RET     C
        CP      FIELD_H
        RET     NC
        LD      C,A
        CALL    PY_GET          ; over sky; a trail over fire too (refview's
        CP      PX_SKY          ;  drawHead: never over dirt or a tank)
        JR      Z,TRN1
        CP      PX_FX
        RET     NZ
        LD      E,A
        LD      A,(IX+TK_STY)
        AND     15
        CP      ST_SMOKE
        RET     NZ
        LD      A,E
TRN1:   RRCA                    ; its pixel under, in b5-6 of the flags
        RRCA
        RRCA
        LD      E,A
        LD      A,(IX+TK_STY)
        AND     9FH
        OR      E
        LD      (IX+TK_STY),A
        LD      (IX+TK_HX),B
        LD      (IX+TK_HY),C
        LD      E,PX_FX
        JP      PY_PLOT

; TROFF: track IX's head off: its pixel back (unless a trail, or drawn over:
; only if it is still fire).
TROFF:  LD      A,(IX+TK_HX)
        CP      0FFH
        RET     Z
        LD      B,A
        LD      C,(IX+TK_HY)
        LD      (IX+TK_HX),0FFH
        LD      A,(IX+TK_STY)
        AND     15
        CP      ST_SMOKE
        RET     Z
        CALL    PY_GET
        CP      PX_FX
        RET     NZ
        LD      A,(IX+TK_STY)   ; its pixel under
        RLCA
        RLCA
        RLCA
        AND     3
        LD      E,A
        JP      PY_PLOT
