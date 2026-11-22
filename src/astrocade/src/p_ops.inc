; p_ops.inc -- the script's paths (OP_PATH), flows (OP_FLOW) and beams
; (OP_BEAM) on the field (src/atari/ops.s; docs/PROTOCOL.md). Paths and
; flows read their bytes where they lie in SCRIPT; at a segment's end they
; finish (OPSSEGEND). Coordinates are the field's own, a byte each: one
; that wraps past an edge is simply off the field. The PLY page calls these
; FAR (their arguments at HL, in SCRIPT; each says in A how many bytes it
; took).

        RET                     ; (no entries: FAR calls by name)
        NOP
        NOP
        RET
        NOP
        NOP
        RET
        NOP
        NOP

NPATH   EQU     4
NFLOW   EQU     4
NBEAM   EQU     2
PASZ    EQU     7               ; a path: on, style, x, y, n, drawn, its next step (offset)
FLSZ    EQU     10              ; a flow: on, kind, frames, f, hold, n, shown, acc (2), cells (offset)
BESZ    EQU     5               ; a beam: frames left, x0, y0, x1, y1
FLOW_MUD EQU    1

; OFFHL: A = HL - SCRIPT (an offset in the buffer: it is under 256).
OFFHL:  LD      A,L
        SUB     SCRIPT AND 0FFH
        RET
; SCRAT: HL = SCRIPT + A. Preserves BC, DE.
SCRAT:  LD      HL,SCRIPT
        ADD     A,L
        LD      L,A
        RET     NC
        INC     H
        RET

; ---- paths ---------------------------------------------------------------------
; PHEAD: path IX's head, drawn: a roller over air, a digger over air or dirt.
PHEAD:  LD      (IX+5),0
        LD      B,(IX+2)
        LD      C,(IX+3)
        CALL    OQ_GET
        CP      PX_SKY
        JR      Z,PH1
        CP      PX_DIRT
        RET     NZ
        LD      A,(IX+1)
        CP      ST_DIGGER
        RET     NZ
PH1:    LD      E,PX_FX
        CALL    OQ_PLOT
        LD      (IX+5),1
        RET

; PLEAVE: path IX's head moves on (a digger has bored: air behind it).
PLEAVE: LD      A,(IX+5)
        OR      A
        RET     Z
        LD      (IX+5),0
        LD      B,(IX+2)
        LD      C,(IX+3)
        LD      D,PX_FX
        LD      E,PX_SKY
        JP      OQ_SWAP

; PEND: path IX stops.
PEND:   LD      A,(IX+0)
        OR      A
        RET     Z
        CALL    PLEAVE
        LD      (IX+0),0
        RET

; OPSPATH: slot, style, x16, y16, n, n steps: a path starts (one live in its
; slot ends first). A = 7 + n.
OPSPATH: LD     A,(HL)
        AND     NPATH-1
        LD      E,A             ; IX = PATHS + 7 slot
        ADD     A,A
        ADD     A,A
        ADD     A,A
        SUB     E
        LD      E,A
        LD      D,0
        LD      IX,PATHS
        ADD     IX,DE
        PUSH    HL
        CALL    PEND
        POP     HL
        INC     HL
        LD      A,(HL)          ; style
        LD      (IX+1),A
        INC     HL
        LD      A,(HL)          ; x (its low byte)
        LD      (IX+2),A
        INC     HL
        INC     HL
        LD      A,(HL)          ; y
        LD      (IX+3),A
        INC     HL
        INC     HL
        LD      A,(HL)          ; n
        LD      (IX+4),A
        INC     HL
        CALL    OFFHL           ; its steps
        LD      (IX+6),A
        LD      (IX+0),1
        CALL    PHEAD
        LD      A,(IX+4)
        ADD     A,7
        RET

; PSTEP: every live path a step: its head moves by the next byte's signed
; nibbles (dx << 4 | dy); none left: it stops.
PSTEP:  LD      IX,PATHS
        LD      B,NPATH
QPS1:    PUSH    BC
        LD      A,(IX+0)
        OR      A
        JR      Z,PS3
        LD      A,(IX+4)
        OR      A
        JR      NZ,PS2
        CALL    PEND
        JR      PS3
PS2:    CALL    PLEAVE
        DEC     (IX+4)
        LD      A,(IX+6)
        INC     (IX+6)
        CALL    SCRAT
        LD      A,(HL)
        LD      C,A
        SRA     A               ; dx: the high nibble, signed
        SRA     A
        SRA     A
        SRA     A
        ADD     A,(IX+2)
        LD      (IX+2),A
        LD      A,C             ; dy: the low nibble, signed
        ADD     A,A
        ADD     A,A
        ADD     A,A
        ADD     A,A
        SRA     A
        SRA     A
        SRA     A
        SRA     A
        ADD     A,(IX+3)
        LD      (IX+3),A
        CALL    PHEAD
PS3:    LD      DE,PASZ
        ADD     IX,DE
        POP     BC
        DJNZ    QPS1
        RET

; ---- flows ----------------------------------------------------------------------
; FCELLS: flow IX's cells OCNT to OTO-1: OFOLD becomes OFV (and with OMUD,
; fire becomes OFV too).
FCELLS: LD      A,(OCNT)
        LD      HL,OTO
        CP      (HL)
        RET     NC
        LD      C,A             ; its bytes: 3 a cell
        ADD     A,A
        ADD     A,C
        ADD     A,(IX+9)
        CALL    SCRAT
        LD      A,(HL)          ; x (its low byte; a high byte: off the field)
        LD      B,A
        INC     HL
        LD      A,(HL)
        INC     HL
        LD      C,(HL)          ; y
        OR      A
        JR      NZ,FC2
        LD      A,(OFOLD)
        LD      D,A
        LD      A,(OFV)
        LD      E,A
        CALL    OQ_SWAP
        LD      A,(OMUD)
        OR      A
        JR      Z,FC2
        LD      D,PX_FX
        LD      A,(OFV)
        LD      E,A
        CALL    OQ_SWAP
FC2:    LD      HL,OCNT
        INC     (HL)
        JR      FCELLS

; FSHOW: flow IX's first A cells in (fire over the air, mud as dirt).
FSHOW:  CP      (IX+6)
        RET     Z
        RET     C
        LD      (OTO),A
        LD      A,(IX+6)
        LD      (OCNT),A
        LD      A,(OTO)
        LD      (IX+6),A
        LD      A,PX_SKY
        LD      (OFOLD),A
        XOR     A
        LD      (OMUD),A
        LD      A,(IX+1)
        CP      FLOW_MUD
        LD      A,PX_FX
        JR      NZ,FS1
        LD      A,1
        LD      (OMUD),A
        LD      A,PX_DIRT
FS1:    LD      (OFV),A
        JR      FCELLS

; FEND: flow IX all in, then a fire goes out; it is off.
FEND:   LD      A,(IX+5)
        CALL    FSHOW
        LD      A,(IX+1)
        CP      FLOW_MUD
        JR      Z,FE1
        XOR     A
        LD      (OCNT),A
        LD      (OMUD),A
        LD      A,(IX+5)
        LD      (OTO),A
        LD      A,PX_FX
        LD      (OFOLD),A
        LD      A,PX_SKY
        LD      (OFV),A
        CALL    FCELLS
FE1:    LD      (IX+0),0
        RET

; OPSFLOW: kind, frames, hold, n, n cells (x16, y8): a flow starts (the
; first free; full, the last ends now). A = 4 + 3n.
OPSFLOW: PUSH   HL
        LD      IX,FLOWS
        LD      B,NFLOW-1
OF1:    LD      A,(IX+0)
        OR      A
        JR      Z,OF2
        LD      DE,FLSZ
        ADD     IX,DE
        DJNZ    OF1
        LD      A,(IX+0)        ; the last: live, it ends now
        OR      A
        CALL    NZ,FOFF
OF2:    POP     HL
        LD      A,(HL)          ; kind
        LD      (IX+1),A
        INC     HL
        LD      A,(HL)          ; frames (0: 1)
        OR      A
        JR      NZ,OF3
        INC     A
OF3:    LD      (IX+2),A
        INC     HL
        LD      A,(HL)          ; hold
        LD      (IX+4),A
        INC     HL
        LD      A,(HL)          ; n
        LD      (IX+5),A
        INC     HL
        CALL    OFFHL           ; its cells
        LD      (IX+9),A
        XOR     A
        LD      (IX+3),A
        LD      (IX+6),A
        LD      (IX+7),A
        LD      (IX+8),A
        LD      (IX+0),1
        LD      A,(IX+5)        ; took 4 + 3n
        LD      C,A
        ADD     A,A
        ADD     A,C
        ADD     A,4
        RET

; FSTEP: every live flow a frame: its cells come in evenly over its frames
; (acc += n; each frames of it, a cell more), then its hold, then it ends.
FSTEP:  LD      IX,FLOWS
        LD      B,NFLOW
FT1:    PUSH    BC
        LD      A,(IX+0)
        OR      A
        JR      Z,FT6
        CP      2               ; detached: it burns its frames out
        JR      NZ,FT1A
        LD      A,1
        LD      (OFIRE),A
        DEC     (IX+4)
        JR      NZ,FT6
        CALL    FBOX
        JR      FT6
FT1A:   LD      A,(IX+1)
        CP      FLOW_MUD
        JR      Z,FT2
        LD      A,1
        LD      (OFIRE),A
FT2:    LD      A,(IX+3)
        CP      (IX+2)
        JR      NC,FT4
        INC     (IX+3)
        LD      L,(IX+7)        ; acc += n
        LD      H,(IX+8)
        LD      E,(IX+5)
        LD      D,0
        ADD     HL,DE
        LD      E,(IX+2)        ; each frames of it: a cell more
        LD      C,(IX+6)
FT3:    OR      A
        SBC     HL,DE
        JR      C,FT3A
        INC     C
        JR      FT3
FT3A:   ADD     HL,DE
        LD      (IX+7),L
        LD      (IX+8),H
        LD      A,C
        CALL    FSHOW
        JR      FT5
FT4:    LD      A,(IX+4)        ; its hold
        OR      A
        JR      Z,FT5
        DEC     (IX+4)
FT5:    LD      A,(IX+3)        ; all in, the hold over: it ends
        CP      (IX+2)
        JR      C,FT6
        LD      A,(IX+4)
        OR      A
        JR      NZ,FT6
        CALL    FEND
FT6:    LD      DE,FLSZ
        ADD     IX,DE
        POP     BC
        DJNZ    FT1
        RET

; ---- beams ------------------------------------------------------------------------------
; LPLOT: (LX0, LY0): on, fire over air and dirt; off, fire back to air.
LPLOT:  LD      A,(LX0)
        LD      B,A
        LD      A,(LY0)
        LD      C,A
        LD      A,(LON)
        OR      A
        JR      Z,LP1
        LD      D,PX_SKY
        LD      E,PX_FX
        CALL    OQ_SWAP
        LD      D,PX_DIRT
        LD      E,PX_FX
        JP      OQ_SWAP
LP1:    LD      D,PX_FX
        LD      E,PX_SKY
        JP      OQ_SWAP

; BMLINE: beam IX's line (Bresenham, as the reference view draws it).
BMLINE:  LD      A,(IX+1)
        LD      (LX0),A
        LD      A,(IX+2)
        LD      (LY0),A
        LD      A,(IX+3)        ; dx = |x1 - x0|, sx
        LD      C,1
        SUB     (IX+1)
        JR      NC,BL1
        NEG
        LD      C,0FFH
BL1:    LD      (LDX),A
        LD      A,C
        LD      (LSX),A
        LD      A,(IX+4)        ; dy = |y1 - y0|, sy
        LD      C,1
        SUB     (IX+2)
        JR      NC,BL2
        NEG
        LD      C,0FFH
BL2:    LD      (LDY),A
        LD      A,C
        LD      (LSY),A
        LD      A,(LDX)         ; err = dx - dy
        LD      L,A
        LD      H,0
        LD      A,(LDY)
        LD      E,A
        LD      D,0
        OR      A
        SBC     HL,DE
        LD      (LERR),HL
BL3:    CALL    LPLOT
        LD      A,(LX0)         ; at the end?
        CP      (IX+3)
        JR      NZ,BL4
        LD      A,(LY0)
        CP      (IX+4)
        RET     Z
BL4:    LD      HL,(LERR)       ; e2 = 2 err
        ADD     HL,HL
        PUSH    HL              ; e2 > -dy: x moves
        LD      A,(LDY)
        LD      E,A
        LD      D,0
        ADD     HL,DE           ; e2 + dy > 0?
        BIT     7,H
        JR      NZ,BL5
        LD      A,H
        OR      L
        JR      Z,BL5
        LD      HL,(LERR)
        OR      A
        SBC     HL,DE
        LD      (LERR),HL
        LD      A,(LSX)
        LD      HL,LX0
        ADD     A,(HL)
        LD      (HL),A
BL5:    POP     HL              ; e2 < dx: y moves
        LD      A,(LDX)
        LD      E,A
        LD      D,0
        OR      A
        SBC     HL,DE
        BIT     7,H
        JR      Z,BL3
        LD      HL,(LERR)
        ADD     HL,DE
        LD      (LERR),HL
        LD      A,(LSY)
        LD      HL,LY0
        ADD     A,(HL)
        LD      (HL),A
        JR      BL3

; BOFF: beam IX's pixels back to air, and it is gone.
BOFF:   LD      A,(IX+0)
        OR      A
        RET     Z
        LD      (IX+0),0
        XOR     A
        LD      (LON),A
        JP      BMLINE

; OPSBEAM: x0 16, y0 16, x1 16, y1 16, kind, frames: a beam (the second slot
; if the first is live; a live one there goes first). A = 10.
OPSBEAM: LD     IX,BEAMS
        LD      A,(IX+0)
        OR      A
        JR      Z,QOB1
        LD      IX,BEAMS+BESZ
QOB1:    PUSH    HL
        CALL    BOFF
        POP     HL
        LD      A,(HL)
        LD      (IX+1),A
        INC     HL
        INC     HL
        LD      A,(HL)
        LD      (IX+2),A
        INC     HL
        INC     HL
        LD      A,(HL)
        LD      (IX+3),A
        INC     HL
        INC     HL
        LD      A,(HL)
        LD      (IX+4),A
        INC     HL
        INC     HL
        INC     HL
        LD      A,(HL)          ; frames (0: 1)
        OR      A
        JR      NZ,QOB2
        INC     A
QOB2:    LD      (IX+0),A
        LD      A,1
        LD      (LON),A
        CALL    BMLINE
        LD      A,10
        RET

; BSTEP: every live beam a frame; its last: its pixels back to air.
BSTEP:  LD      IX,BEAMS
        LD      B,NBEAM
QBS1:    PUSH    BC
        LD      A,(IX+0)
        OR      A
        JR      Z,BS2
        DEC     (IX+0)
        JR      NZ,BS2
        INC     (IX+0)          ; (BOFF wants it live)
        CALL    BOFF
BS2:    LD      DE,BESZ
        ADD     IX,DE
        POP     BC
        DJNZ    QBS1
        RET

; ---- a frame, a segment's end, a new shot ---------------------------------------------
; OPSSTEP: A = non-zero if a fire flow burns.
OPSSTEP: XOR    A
        LD      (OFIRE),A
        CALL    PSTEP
        CALL    FSTEP
        CALL    BSTEP
        LD      A,(OFIRE)
        RET

; OPSSEGEND: paths stop, flows run out (the segment's bytes are going): a
; fire flow detaches (all of it shown, burning its frames and hold out:
; FSTEP); mud is all in at once. A non-zero: the script's last segment:
; every flow ends now, the detached ones too (refview's field after it).
OPSSEGEND: LD   C,A
        LD      IX,PATHS
        LD      B,NPATH
OE1:    PUSH    BC
        CALL    PEND
        LD      DE,PASZ
        ADD     IX,DE
        POP     BC
        DJNZ    OE1
        LD      IX,FLOWS
        LD      B,NFLOW
OE2:    PUSH    BC
        LD      A,(IX+0)
        OR      A
        JR      Z,OE4
        INC     C               ; the last: off, whatever it is
        DEC     C
        JR      NZ,OE3
        CP      2               ; (detached: it goes on)
        JR      Z,OE4
        LD      A,(IX+1)
        CP      FLOW_MUD
        JR      Z,OE3
        CALL    DETACH
        JR      OE4
OE3:    CALL    FOFF
OE4:    LD      DE,FLSZ
        ADD     IX,DE
        POP     BC
        DJNZ    OE2
        RET

; FOFF: flow IX off now (live: all in, then out; detached: its box out).
FOFF:   LD      A,(IX+0)
        CP      2
        JP      NZ,FEND
; FBOX: detached flow IX's box: fire back to sky; it is off.
FBOX:   LD      A,2
        LD      (ELMODE),A
        LD      A,PX_FX*55H
        LD      (ELPD),A
        LD      A,PX_SKY*55H
        LD      (ELPE),A
        LD      A,(IX+6)
FB1:    PUSH    AF
        LD      C,A
        LD      B,(IX+5)
        LD      D,(IX+7)
        CALL    OQ_HSPAN
        POP     AF
        CP      (IX+8)
        JR      NC,FB2
        INC     A
        JR      FB1
FB2:    LD      (IX+0),0
        RET

; DETACH: live fire flow IX: all its cells in; then the box they lie in
; (x0, y0, x1, y1 in its bytes 5-8, past its cells' use) and its frames
; left (to come, and its hold: byte 4, at most 255), on 2.
DETACH: LD      L,(IX+4)        ; frames left: frames - f + hold
        LD      H,0
        LD      A,(IX+2)
        SUB     (IX+3)
        JR      C,DT0
        LD      E,A
        LD      D,0
        ADD     HL,DE
DT0:    LD      A,H
        OR      A
        LD      A,L
        JR      Z,DT0A
        LD      A,255
DT0A:   OR      A
        JR      NZ,DT0B
        INC     A
DT0B:   PUSH    AF
        LD      A,(IX+5)        ; all of it in
        CALL    FSHOW
        LD      A,0FFH          ; the box: OCNT-OFV x0, x1, y0, y1
        LD      (OCNT),A
        LD      (OFOLD),A
        XOR     A
        LD      (OTO),A
        LD      (OFV),A
        LD      A,(IX+9)
        CALL    SCRAT
        LD      B,(IX+5)
        INC     B
        JR      DT9
DT1:    LD      A,(HL)          ; x (on the field: its high byte 0), y
        INC     HL
        LD      E,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        INC     E
        DEC     E
        JR      NZ,DT9
        CP      FIELD_W
        JR      NC,DT9
        LD      E,A
        LD      A,C
        CP      FIELD_H
        JR      NC,DT9
        LD      A,(OCNT)        ; x0 = min, x1 = max
        CP      E
        JR      C,DT2
        LD      A,E
        LD      (OCNT),A
DT2:    LD      A,(OTO)
        CP      E
        JR      NC,DT3
        LD      A,E
        LD      (OTO),A
DT3:    LD      A,(OFOLD)       ; y0, y1
        CP      C
        JR      C,DT4
        LD      A,C
        LD      (OFOLD),A
DT4:    LD      A,(OFV)
        CP      C
        JR      NC,DT9
        LD      A,C
        LD      (OFV),A
DT9:    DJNZ    DT1
        POP     AF
        LD      (IX+4),A
        LD      A,(OCNT)        ; none on the field: off
        CP      0FFH
        JR      Z,DT10
        LD      (IX+5),A
        LD      A,(OFOLD)
        LD      (IX+6),A
        LD      A,(OTO)
        LD      (IX+7),A
        LD      A,(OFV)
        LD      (IX+8),A
        LD      (IX+0),2
        RET
DT10:   LD      (IX+0),0
        RET

; OPSBEAMSOFF: the beams gone (the script's end).
OPSBEAMSOFF: LD IX,BEAMS
        CALL    BOFF
        LD      IX,BEAMS+BESZ
        JP      BOFF

; OPSRESET: all off (a new shot).
OPSRESET: LD    HL,PATHS
        LD      DE,PATHS+1
        LD      BC,NPATH*PASZ+NFLOW*FLSZ+NBEAM*BESZ-1
        LD      (HL),0
        LDIR
        RET
