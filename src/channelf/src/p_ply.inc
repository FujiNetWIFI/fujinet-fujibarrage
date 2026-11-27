; ---------------------------------------------------------------------------
; p_ply.inc -- the shot player (docs/PROTOCOL.md "Playing a segment"), run
; from RAM (PLYRAM: START copies the page there), so it is resident: it
; calls fixed code and any page directly.
;
; A port of the 6502's (src/6502/ply.s, trk.s, ops.s; the Atari's fld.s for
; the field's ops) op for op, on the Channel F's field (disp.inc: FLD is
; the screen while the field shows, so every pixel set below is drawn as it
; is set, and only if it changes). Each frame every track, blast, path,
; flow, beam and settle moves a step, then the ops due run; a client behind
; its clock runs up to 8 steps a pass (the cart's tick) and draws once.
;
; The 6502's arrays of a field per slot are records here (a slot's bytes
; together, DC0 walking them); coordinates are 16-bit where the script's
; are, off the field meaning nothing is drawn (a get there is sky).
;
; Blasts are drawn a column at a time (the field is column-major): column dx
; covers rows ey +- D, D = ery * q(|dx| / erx), q a quarter circle; a blast
; that grows draws only its ring (the rows outside its last size). The shape
; is the client's own (the server checks the field after the settles).
;
; The ops about tanks move them (tank.inc); talk, damage and shield are the
; HUD's messages (hud.inc: event lines queued, drawn 16 jiffies apart and
; not while behind), a sound is played (SND_BOOM by its size), the flash and
; the shake are the field's palettes (PFX, FXTICK).
; ---------------------------------------------------------------------------

PS_SMOKE  EQU 2                 ; track styles (ST_*: the low nibble)
PS_DIGGER EQU 4
DIRTBALL  EQU 80H               ; a blast record's kind: an OP_DIRT

; ===========================================================================
; PLAYSEG -- the segment in SCRIPT (SEGLEN bytes; SEGNO, SEGFLG) played to
;   its end; at the shot's last segment, what still moves stops.
; ===========================================================================
PLAYSEG: ENTER
        LIS 1                   ; a shot under way (to its last segment:
        STB INSCR               ;  PSCREND)
        LDB SEGNO               ; a new shot: nothing moving
        NI 0FFH
        BNZ PS1
        PI PRESET
PS1:    DCI SEGLEN              ; PP = SCRIPT, PEND = SCRIPT + SEGLEN
        LM
        LR 0,A
        LM
        LR 1,A
        DCI SCRIPT
        LR Q,DC
        DCI PP
        LR A,QU
        ST
        LR A,QL
        ST
        LR A,QL                 ; (PEND right after PP: big-endian)
        AS 0
        LR 2,A
        LR A,QU
        LNK                     ; (SCRIPT's high byte + 1 at most: no carry)
        AS 1
        ST
        LR A,2
        ST
        CLR
        STB PDONE
        STB PWAIT
        STB PWAIT+1
        PI TICKRD               ; the clock from now
        LR A,0
        STB PLAST
        LR A,1
        STB PLAST+1
PS2:    PI PBEHIND              ; steps owed (1-8): wait for one at least
        LDB PSTEPS
        NI 0FFH
        BZ PS2
        STB PDT                 ; (the pass's jiffies)
PS3:    PI PSTEP
        LDB PDONE
        NI 0FFH
        BNZ PS4
        DECB PSTEPS
        BNZ PS3
        PI PDRAW
        PI SNDTICKP
        LDB TALKN               ; (the effects only if any: ~2K clocks a
        DCI EVPEND              ;  pass otherwise; a message held to the
        OM                      ;  script's end, MSGJ 1, needs none)
        DCI FLASHN
        OM
        DCI PSHAKE
        OM
        BNZ PS3A
        LDB MSGJ
        CI 1
        BC PS2
PS3A:   PI PFX
        BR PS2
PS4:    PI PDRAW
        PI MFLUSH               ; a talk's letters all (SCRIPT's bytes go),
        PI EVDRAW               ;  the last event line
        PI PENDSEG              ; settles land, paths end, flows all in
        LDB SEGFLG
        NI S_LAST_SEG
        BZ PS9
        PI PSCREND
PS9:    LEAVE

; PBEHIND -- PSTEPS = the ticks since PLAST (8 at most); PLAST += them:
;   the rest is owed to the next pass (the client catches up).
PBEHIND: ENTER
        PI TICKRD               ; r1:r0 now
        DCI PLAST
        LM
        COM                     ; now - PLAST (low bytes: never 256 owed)
        INC
        AS 0
        CI 8
        BC PB1
        LIS 8
PB1:    STB PSTEPS
        LR 3,A
        DCI PLAST               ; PLAST += them
        LM
        AS 3
        LR 4,A
        LM
        LNK
        DCI PLAST+1
        ST
        DCI PLAST
        LR A,4
        ST
        LEAVE

; SNDTICKP -- the sound's tick for the steps run (sound.inc reads DT).
SNDTICKP: ENTER
        LDB PDT
        STB DT
        PI SNDTICK
        LEAVE

; PFX -- a pass's other effects: a talk's next letter, the queued event
;   line (16 jiffies after the last, and not while behind: the field
;   first), a message's time, the flash's and the shake's.
PFX:    ENTER
        LDB TALKN
        NI 0FFH
        BZ PFX0
        PI MSTEP
PFX0:   LDB EVPEND
        NI 0FFH
        BZ PFX2
        LDB EVJ                 ; an event line: 16 jiffies since the last
        LR 1,A
        LDB PDT
        COM
        INC
        AS 1
        BC PFX1
        CLR
PFX1:   STB EVJ
        NI 0FFH
        BNZ PFX2
        LDB PDT                 ; (behind: at the segment's end)
        CI 1
        BNZ PFX2
        PI EVDRAW
PFX2:   LDB MSGJ                ; a message's time (not over in a script:
        CI 1                    ;  the HUD is drawn after it)
        BC PFX3
        LDB PDT
        LR 0,A
        LIS 1
        LR 1,A
        PI MTICK
PFX3:   LDB FLASHN
        DCI PSHAKE
        OM
        BZ PFX4
        PI FXTICK
PFX4:   LEAVE

; EVDRAW -- the queued event line (if one is): drawn.
EVDRAW: ENTER
        LDB EVPEND
        NI 0FFH
        BZ EVD9
        CLR
        STB EVPEND
        LI 16
        STB EVJ
        DCI EVARG
        LR Q,DC
        LDB EVKIND
        CI OP_DAMAGE
        BNZ EVD1
        PI MDMG
        LEAVE
EVD1:   PI MSHLD
EVD9:   LEAVE

; FXFLASH -- OP_FLASH for r0 frames: the field's rows lit (each row's
;   palette XOR 2: black and light grey, light blue and light green swap).
FXFLASH: ENTER
        LR A,0
        NI 0FFH
        BNZ FXF1
        LIS 1
FXF1:   LR 1,A
        LDB FLASHN
        NI 0FFH
        LR A,1
        STB FLASHN
        BNZ FXF9                ; (lit already: longer)
        LIS FY0
        LR 0,A
        LI FY0+FH-1
        LR 1,A
        LIS 2
        LR 2,A
        PI PALX
FXF9:   LEAVE

; FXTICK -- a pass of the flash (its frames out: the sky again) and the
;   shake (PSHAKE frames: the sky's rows over the land flicker, each row's
;   palette XOR 1 every 4 jiffies; out: the sky again).
FXTICK: ENTER
        LDB FLASHN
        NI 0FFH
        BZ FXT2
        LR 1,A
        LDB PDT
        COM
        INC
        AS 1
        BNC FXT1
        BZ FXT1
        STB FLASHN
        LEAVE                   ; (no shake while it is lit)
FXT1:   PI FXEND
        LEAVE
FXT2:   LDB PSHAKE
        NI 0FFH
        BNZ FXT3
        LEAVE
FXT3:   LR 1,A
        LDB PDT
        COM
        INC
        AS 1
        BNC FXT1
        BZ FXT1
        STB PSHAKE
        LDB PDT                 ; every 4 jiffies a flicker
        LR 1,A
        LDB SHKJ
        AS 1
        STB SHKJ
        CI 3
        BNC FXT4
        LEAVE
FXT4:   CLR
        STB SHKJ
        PI SKHOR                ; r1 = the row over the land
        LR A,1
        NI 0FFH
        BNZ FXT5
        LEAVE
FXT5:   AI FY0-1
        LR 1,A
        LIS FY0
        LR 0,A
        LIS 1
        LR 2,A
        PI PALX
        LEAVE

; FXEND -- the flash and the shake over: the sky as it is.
FXEND:  ENTER
        CLR
        STB FLASHN
        STB PSHAKE
        STB SHKJ
        PI SKYSET
        LEAVE

; PRESET -- a new shot: nothing moving.
PRESET: ENTER
        DCI PREC0
        LI (PRECEND-PREC0+255)>>8
        LR 1,A
        LI (PRECEND-PREC0)&0FFH
        LR 0,A
PRS1:   CLR
        ST
        DS 0
        BNZ PRS1
        DS 1
        BNZ PRS1
        CLR
        STB PFALL
        LEAVE

; PSCREND -- the script's end: tracks stop, beams go (src/play.c script_end).
PSCREND: ENTER
        CLR
        STB INSCR
        STB PI0
PSE1:   LDB PI0
        LR 0,A
        PI TOFF                 ; its head off, the track ended
        LDB PI0
        LR 0,A
        PI TREC
        CLR
        ST
        LDB PI0
        INC
        STB PI0
        CI NTRK
        BNZ PSE1
        CLR
        LR 0,A
        PI BOFF
        LIS 1
        LR 0,A
        PI BOFF
        LDB FLASHN              ; a flash, a shake: over
        DCI PSHAKE
        OM
        BZ PSE2
        PI FXEND
PSE2:   LEAVE

; ===========================================================================
; a frame: every track, blast, path, flow, beam and settle a step (not on
; the segment's first frame: PP at SCRIPT), then the ops due
; ===========================================================================
PSTEP:  ENTER
        DCI PP                  ; the first frame only runs ops
        LM
        CI SCRIPT>>8
        BNZ PST1
        LM
        CI SCRIPT&0FFH
        BZ PST2
PST1:   PI ANIM
PST2:   DCI PWAIT               ; waiting: one less (16-bit)
        LM
        LR 0,A
        LM
        LR 1,A
        AS 0
        BZ PST3
        LR A,0
        AI 0FFH
        LR 0,A
        LR A,1
        LNK
        AI 0FFH
        DCI PWAIT+1
        ST
        DCI PWAIT
        LR A,0
        ST
PST3:   DCI PWAIT               ; ops until a wait or the end
        LM
        LR 0,A
        LM
        AS 0
        BNZ PSTX
        LDB PDONE
        NI 0FFH
        BNZ PSTX
        DCI PP                  ; PP < PEND? (PEND + ~PP carries: PEND > PP)
        LM
        COM
        LR 5,A                  ; ~PP hi (before: COM clears the carry)
        LM
        COM
        LR 4,A                  ; ~PP lo
        LM
        LR 6,A                  ; PEND hi
        LM
        AS 4
        ADC3 5,6
        BC PST4
        LIS 1                   ; PP at the end: done
        STB PDONE
PSTX:   LEAVE
PST4:   PI OP
        BR PST3

; ===========================================================================
; the ops
; ===========================================================================
; OP -- the op at PP; PP past it and its arguments.
OP:     ENTER
        DCI PP
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LM                      ; the op; Q its arguments
        LR 8,A
        LR Q,DC
        LR A,8
        CI OP_WAIT
        BNZ OP1
        LM                      ; WAIT n (0: 256)
        LR 0,A
        LR Q,DC
        CLR
        LR 1,A
        LR A,0
        NI 0FFH
        BNZ OPW1
        LIS 1
        LR 1,A
OPW1:   DCI PWAIT
        LR A,0
        ST
        LR A,1
        ST
        JMP OPNEXT
OP1:    CI OP_ARC
        BNZ OP2
        PI TLOAD
        JMP OPNEXT
OP2:    CI OP_STOP
        BNZ OP3
        LM                      ; slot
        NI NTRK-1
        LR 0,A
        LR Q,DC
        STB PI0
        PI QSAVE                ; (TOFF spends Q)
        LDB PI0
        LR 0,A
        PI TOFF
        LDB PI0
        LR 0,A
        PI TREC
        CLR
        ST
        PI QLOAD
        JMP OPNEXT
OP3:    CI OP_PATH
        BNZ OP4
        PI OPATH
        JMP OPNEXT
OP4:    CI OP_FLOW
        BNZ OP5
        PI OFLOW
        JMP OPNEXT
OP5:    CI OP_BEAM
        BNZ OP6
        PI OBEAM
        JMP OPNEXT
OP6:    CI OP_EXPLODE
        BZ OP6A
        CI OP_DIRT
        BNZ OP7
OP6A:   PI OBLAST
        JMP OPNEXT
OP7:    CI OP_SETTLE
        BNZ OP8
        PI OSETTLE
        JMP OPNEXT
OP8:    CI OP_WIND
        BNZ OP9
        LM
        LR 0,A
        LM
        LR 1,A
        LR Q,DC
        DCI ST+OFF_WIND
        LR A,0
        ST
        LR A,1
        ST
        JMP OPNEXT
OP9:    CI OP_SHAKE
        BNZ OP10
        LM
        LR Q,DC
        STB PSHAKE
        JMP OPNEXT
OP10:   CI OP_TALK-1            ; $30-$3A: the tanks', the rest read past
        BC OPEND
        CI OP_FIRE
        BC OPC
OPEND:  LIS 1                   ; OP_END, or not ours: the segment is done
        STB PDONE
        LEAVE
OPC:    PI OPCOP
OPNEXT: DCI PP                  ; PP = Q (past the arguments)
        LR A,QU
        ST
        LR A,QL
        ST
        LEAVE

; OPCOP -- op r8 ($30-$3A), its arguments at Q (src/play.c c_op); Q past
;   them.
OPCOP:  ENTER
        LR DC,Q
        LR A,8
        CI OP_TALK              ; seat, len, text: the HUD's talk
        BNZ OC1
        PI QSAVE
        PI MTALK
        PI QLOAD
        LR DC,Q
        LM
        LM
        ADCU
        JMP OCX
OC1:    CI OP_SOUND             ; id, size: played (SND_BOOM: SND_BOOM0-3
        BNZ OC2                 ;  by its size)
        LM
        LR 0,A
        LM
        LR 1,A
        LR Q,DC
        LR A,0
        CI SND_BOOM
        BNZ OC1A
        LR A,1
        NI 3
        AI SND_BOOM0
        LR 0,A
OC1A:   PI QSAVE                ; (SNDPLAY spends Q)
        PI SNDPLAY
        PI QLOAD
        JMP OCQ
OC2:    CI OP_SHIELD            ; seat, event, hp: an event line (queued:
        BZ OC2A                 ;  PFX draws the latest, 16 jiffies apart)
        CI OP_DAMAGE            ; seat, amount, energy left: one too
        BNZ OC3
OC2A:   LR A,8
        STB EVKIND
        LIS 1
        STB EVPEND
        LR DC,Q
        XDC
        DCI EVARG
        XDC
        LIS 3
        LR 0,A
OC2B:   LM
        XDC
        ST
        XDC
        DS 0
        BNZ OC2B
        JMP OCX
OC3:    CI OP_FALL              ; seat, x16, y, frames, chute
        BNZ OC4
        PI OTANKXY
        LM
        LM
        JMP OCX
OC4:    CI OP_DEATH             ; seat, effect
        BNZ OC5
        LM
        LR 0,A
        LM
        LR Q,DC
        LR A,0
        CI SEATS-1
        BNC OCQ
        DCI TKON
        ADC
        CLR
        ST
        PI QSAVE                ; (TKSHOW spends Q)
        PI TKSHOW
        PI QLOAD
        JMP OCQ
OC5:    CI OP_TANK              ; seat, x16, y, angle
        BNZ OC6
        PI OTANKXY
        LM
        LR 1,A
        LR Q,DC
        LDB TKS2
        CI SEATS-1
        BNC OCQ
        LR 0,A
        DCI TKA
        ADC
        LR A,1
        ST
        PI QSAVE
        PI TKSHOW
        PI QLOAD
        JMP OCQ
OC6:    CI OP_FLASH             ; colour, frames: the field's rows lit
        BNZ OC7
        LM
        LM
        LR Q,DC
        LR 0,A
        PI QSAVE
        PI FXFLASH
        PI QLOAD
        JMP OCQ
OC7:    CI OP_FIRE              ; seat, angle, power16, weapon
        BNZ OCX
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LM
        LM
        LR Q,DC
        LR A,0
        CI SEATS-1
        BNC OCQ
        DCI TKA
        ADC
        LR A,1
        ST
        PI QSAVE
        PI TKSHOW
        PI QLOAD
        JMP OCQ
OCX:    LR Q,DC
OCQ:    LEAVE

; OTANKXY -- seat, x16, y at DC0: the seat's wanted tank there (TKS2 the
;   seat); OP_FALL's (r8) shown now, OP_TANK's after its angle.  DC0 past.
OTANKXY: ENTER
        LM
        LR 0,A
        LM
        LR 1,A                  ; x (its low byte: the field is < 256)
        LM
        LM
        LR 2,A                  ; y
        LR Q,DC
        LR A,0
        STB TKS2
        CI SEATS-1
        BNC OTX
        DCI TKX
        ADC
        LR A,1
        ST
        LR A,0
        DCI TKY
        ADC
        LR A,2
        ST
        LR A,0
        DCI TKON
        ADC
        LIS 1
        ST
        LR A,8
        CI OP_FALL
        BNZ OTX
        PI QSAVE
        LDB TKS2
        LR 0,A
        PI TKSHOW
        PI QLOAD
OTX:    LR DC,Q
        LEAVE

; ===========================================================================
; blasts: EXPLODE x16, y16, rx, ry, kind, frames; DIRT the same, no kind.
; A record: 0 on, 1 kind, 2 n, 3 tot, 4 rx, 5 ry, 6 dn, 7 xl, 8 xh, 9 yl,
; 10 yh, 11 drx, 12 dry (the radii it was last drawn at: 0 none).
; ===========================================================================
; OBLAST -- op r8's arguments at Q into the first free blast (or the last);
;   Q past them.
OBLAST: ENTER
        CLR                     ; the first free one
        LR 4,A
OB1:    LR A,4
        LR 0,A
        PI BREC
        LM
        NI 0FFH
        BZ OB2
        LR A,4
        INC
        LR 4,A
        CI NBLAST-1
        BNZ OB1
OB2:    LR A,4
        STB PI0
        LR DC,Q                 ; the arguments
        LM
        LR 0,A                  ; xl
        LM
        LR 1,A                  ; xh
        LM
        LR 2,A                  ; yl
        LM
        LR 3,A                  ; yh
        LM
        LR 5,A                  ; rx
        LM
        LR 6,A                  ; ry
        LR A,8
        CI OP_DIRT
        LI DIRTBALL
        BZ OB3
        LM                      ; kind
OB3:    LR 7,A
        LM                      ; frames (0: 1)
        NI 0FFH
        BNZ OB4
        LIS 1
OB4:    LR 4,A
        LR Q,DC                 ; (past them)
        LR A,7                  ; a nuke shakes the screen
        CI EX_NUKE
        BNZ OB5
        LI 30
        STB PSHAKE
OB5:    LDB PI0
        LR 8,A
        LR A,8
        SL 4
        DCI BLS
        ADCU                    ; its record: on, kind, n, tot, rx, ry, dn,
        LIS 1                   ;  xl, xh, yl, yh, drx, dry
        ST
        LR A,7
        ST
        CLR
        ST
        LR A,4
        ST
        LR A,5
        ST
        LR A,6
        ST
        CLR
        ST
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        CLR
        ST
        ST
        LEAVE

; BREC -- DC0 = blast r0's record (16 bytes).  Leaf.
BREC:   LR A,0
        SL 4
        DCI BLS
        ADCU
        POP

; ===========================================================================
; settles: SETTLE x16, n, frames, n column entries.
; A record: 0 on, 1 left, 2 n, 3 xl, 4 xh, 5 ph, 6 pl (its entries).
; ===========================================================================
; OSETTLE -- the arguments at Q into a free settle (full: settle 0 lands
;   now); frames 0: it lands now.  Q past its entries.
OSETTLE: ENTER
        CLR
        LR 4,A
OS1:    LR A,4
        LR 0,A
        PI SREC
        LM
        NI 0FFH
        BZ OS2
        LR A,4
        INC
        LR 4,A
        CI NSETTLE
        BNZ OS1
        PI QSAVE                ; full: settle 0 lands now
        CLR
        STB PI0
        PI SLAND
        PI QLOAD
        CLR
        LR 4,A
OS2:    LR A,4
        STB PI0
        LR DC,Q                 ; its arguments
        LM
        LR 0,A                  ; xl
        LM
        LR 1,A                  ; xh
        LM
        LR 2,A                  ; n
        LM
        LR 3,A                  ; frames
        LR Q,DC                 ; (its entries start here)
        LR A,4
        SL 1
        SL 1
        SL 1
        DCI SETS
        ADC                     ; its record: on, left, n, xl, xh, ph, pl
        LIS 1
        ST
        LR A,3
        ST
        LR A,2
        ST
        LR A,0
        ST
        LR A,1
        ST
        LR A,QU
        ST
        LR A,QL
        ST
        LR DC,Q                 ; past its entries
        LR A,2
        LR 5,A
        NI 0FFH
        BZ OS4
OS3:    PI ELEN
        DS 5
        BNZ OS3
OS4:    LR Q,DC
        LR A,3                  ; frames 0: it lands now
        NI 0FFH
        BNZ OS9
        PI QSAVE
        PI SLAND
        PI QLOAD
OS9:    LEAVE

; QSAVE, QLOAD -- Q kept in QSV (over a call that spends it).  Leaves.
QSAVE:  LR A,QU
        STB QSV
        LR A,QL
        STB QSV+1
        POP
QLOAD:  DCI QSV
        LM
        LR QU,A
        LM
        LR QL,A
        POP

; ELEN -- DC0 past the column entry there (1, or 1 + 2n).  Leaf.
ELEN:   LM
        CI T_SPANS
        BC ELN1                 ; (00-F0: one byte)
        AI 010H                 ; F0 + n: 2n more
        SL 1
        ADC
ELN1:   POP

; SREC -- DC0 = settle r0's record (8 bytes).  Leaf.
SREC:   LR A,0
        SL 1
        SL 1
        SL 1
        DCI SETS
        ADC
        POP

; SLAND -- settle PI0's columns written (fld.inc FCOLENT; columns off the
;   field skipped); it is off.
SLAND:  ENTER
        LDB PI0
        LR 0,A
        PI SREC
        LM
        NI 0FFH
        BNZ SL0
        LEAVE
SL0:    LM                      ; (left)
        LM
        STB PT+4                ; n (STB: DC0 moved; again from the record)
        LDB PI0
        LR 0,A
        PI SREC
        LI 3
        ADC
        LM
        LR 5,A                  ; xl
        LM
        LR 1,A                  ; xh
        LM
        LR QU,A                 ; its entries
        LM
        LR QL,A
        LR A,1
        STB PT+5
        LR DC,Q
        XDC                     ; DC1: the entries
SL1:    LDB PT+4
        NI 0FFH
        BZ SL9
        LDB PT+5                ; on the field (x 0 .. FW-1)?
        NI 0FFH
        BNZ SL2
        LR A,5
        CI FW-1
        BNC SL2
        PI FCOLENT              ; (keeps r5; DC1 past the entry)
        BR SL3
SL2:    XDC                     ; off it: past its entry
        PI ELEN
        XDC
SL3:    LR A,5                  ; x++ (16-bit)
        INC
        LR 5,A
        BNZ SL4
        LDB PT+5
        INC
        STB PT+5
SL4:    LDB PT+4
        AI 0FFH
        STB PT+4
        BR SL1
SL9:    LDB PI0
        LR 0,A
        PI SREC
        CLR
        ST
        LEAVE

; ===========================================================================
; a frame's step (ply.s anim)
; ===========================================================================
ANIM:   ENTER
        PI TSTEPALL
        CLR                     ; the blasts: a frame on; at their end the
        STB PI0                 ;  crater (or the dirt) now
AN1:    LDB PI0
        LR 0,A
        PI BREC
        LM
        NI 0FFH
        BZ AN9
        LM                      ; (kind)
        LM                      ; n + 1, against tot
        INC
        LR 1,A
        LM
        LR 2,A
        LI 0FEH                 ; (DC0 back on n)
        ADC
        LR A,1
        ST
        LR A,1                  ; n >= tot: its end (tot + ~n = tot - n - 1:
        COM                     ;  C when tot > n)
        AS 2
        BC AN9
        PI BEND
AN9:    LDB PI0
        INC
        STB PI0
        CI NBLAST
        BNZ AN1
        PI PATHSTEP             ; paths, flows, beams
        PI FLOWSTEP
        PI BEAMSTEP
        CLR                     ; the settles: a frame on
        STB PI0
AN10:   LDB PI0
        LR 0,A
        PI SREC
        LM
        NI 0FFH
        BZ AN12
        LM                      ; left - 1
        AI 0FFH
        LR 1,A
        LI 0FFH
        ADC
        LR A,1
        ST
        LR A,1
        NI 0FFH
        BNZ AN12
        PI SLAND
AN12:   LDB PI0
        INC
        STB PI0
        CI NSETTLE
        BNZ AN10
        LEAVE

; BEND -- blast PI0 at its end: the whole ellipse (a crater all to sky,
;   plasma and a puff their fire back to sky, the dirt ball as dirt); off.
BEND:   ENTER
        LDB PI0
        LR 0,A
        PI BREC
        LM                      ; (on)
        LM
        LR 7,A                  ; kind
        LM
        LM
        LM
        STB EL+4                ; erx (STB: DC0 again)
        LDB PI0
        LR 0,A
        PI BREC
        LIS 5
        ADC
        LM
        STB EL+5                ; ery
        LDB PI0
        LR 0,A
        PI BREC
        LIS 7
        ADC
        PI BCENTRE
        LI 0FFH                 ; whole
        STB EL+6
        LR A,7
        CI DIRTBALL
        BNZ BE1
        LIS 1                   ; the dirt: sky becomes dirt
        LR 0,A
        LI PX_DIRT
        BR BE3
BE1:    CI EX_PLASMA
        BZ BE2
        CI EX_PUFF
        BZ BE2
        CLR                     ; a crater: all of it sky
        LR 0,A
        LI PX_SKY
        BR BE3
BE2:    LIS 2                   ; plasma, a puff: their fire back to sky
        LR 0,A
        LI PX_SKY
BE3:    STB EL+9                ; the value
        LR A,0
        STB EL+8                ; the mode
        PI ELLIPSE
        LDB PI0
        LR 0,A
        PI BREC
        CLR
        ST
        LEAVE

; BCENTRE -- EL+0..3 = the centre (xl, xh, yl, yh at DC0).  Leaf.
BCENTRE: LM
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A
        LM
        LR 3,A
        DCI EL
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        POP

; ===========================================================================
; the blasts drawn at their size now (ply.s ply_draw): each that grew, its
; ring (the rows outside the radii it was last drawn at)
; ===========================================================================
PDRAW:  ENTER
        CLR
        STB PI0
PD1:    LDB PI0
        LR 0,A
        PI BREC
        LM
        NI 0FFH
        BNZ PD2
        JMP PD9
PD2:    LM
        STB PT+3                ; kind
        LDB PI0
        LR 0,A
        PI BREC
        LIS 2
        ADC
        LM
        LR 1,A                  ; n
        LM
        LR 2,A                  ; tot
        LM
        LR 5,A                  ; rx
        LM
        LR 6,A                  ; ry
        LM                      ; dn: drawn at this n already?
        XS 1
        BNZ PD3
        JMP PD9
PD3:    LI 0FFH                 ; dn = n
        ADC
        LR A,1
        ST
        LR A,1                  ; rx n / tot, ry n / tot
        STB PT
        LR A,2
        STB PT+1
        LR A,6
        STB PT+2
        LR A,5
        LR 3,A
        PI SCALE
        STB EL+4
        LDB PT+2
        LR 3,A
        PI SCALE
        STB EL+5
        LDB PI0                 ; its centre; the radii it was drawn at, and
        LR 0,A                  ;  these now
        PI BREC
        LIS 7
        ADC
        PI BCENTRE
        LDB PI0
        LR 0,A
        PI BREC
        LI 11
        ADC
        LM
        LR 1,A                  ; drx (0: nothing drawn yet: whole)
        LM
        LR 2,A                  ; dry
        LI 0FEH
        ADC
        LDB EL+4                ; (LDB: DC0 again)
        LR 3,A
        LDB EL+5
        LR 4,A
        LDB PI0
        LR 0,A
        PI BREC
        LI 11
        ADC
        LR A,3
        ST
        LR A,4
        ST
        LR A,1
        NI 0FFH
        LI 0FFH
        BZ PD4
        LR A,1
PD4:    STB EL+6
        LR A,2
        STB EL+7
        LDB PT+3                ; the dirt: sky becomes dirt
        CI DIRTBALL
        BNZ PD5
        LIS 1
        LR 0,A
        LI PX_DIRT
        BR PD7
PD5:    CI EX_PLASMA            ; plasma, a puff: over the air only
        BZ PD6
        CI EX_PUFF
        BZ PD6
        CLR                     ; a crater's fireball eats the dirt
        LR 0,A
        LI PX_FX
        BR PD7
PD6:    LIS 1
        LR 0,A
        LI PX_FX
PD7:    STB EL+9
        LR A,0
        STB EL+8
        PI ELLIPSE
PD9:    LDB PI0
        INC
        STB PI0
        CI NBLAST
        BZ PDX
        JMP PD1
PDX:    LEAVE

; SCALE -- A = r3 * n / tot (n, tot at PT, PT+1; at most r3: n <= tot);
;   0 is 1.  Leaf.  Clobbers A, r0-r4, r2.
SCALE:  DCI PT
        LM
        LR 2,A                  ; n: its bits from the top
        CLR
        LR 0,A
        LR 1,A
        LIS 8
        LR 4,A
SC1:    LR A,0                  ; r1:r0 <<= 1 (< 32768 before: no carry out)
        AS 0
        LR 0,A
        LR A,1
        LNK
        AS 1
        LR 1,A
        LR A,2                  ; n's top bit: + r3
        AS 2
        LR 2,A
        BNC SC2
        LR A,0
        AS 3
        LR 0,A
        LR A,1
        LNK
        LR 1,A
SC2:    DS 4
        BNZ SC1
        DCI PT+1                ; / tot: r0 the quotient (< 256: n <= tot)
        LM
        LR 2,A
        LIS 8
        LR 4,A
SCD1:    LR A,0                  ; r1:r0 <<= 1, the 9th bit in C
        AS 0
        LR 0,A
        LR A,1
        LNK                     ; (r1 < tot <= 255: + C can't carry)
        AS 1
        LR 1,A
        BC SDSUB                ; (256 or more: at least tot)
        LR A,2                  ; r1 >= tot?
        COM
        INC
        AS 1
        BNC SCD2
        LR 1,A
        BR SDQ
SDSUB:  LR A,2
        COM
        INC
        AS 1
        LR 1,A
SDQ:    LR A,0
        INC
        LR 0,A
SCD2:    DS 4
        BNZ SCD1
        LR A,0
        NI 0FFH
        BNZ SDX
        LIS 1
SDX:    POP

; ===========================================================================
; ELLIPSE -- the ellipse at (EL+0,1 x; EL+2,3 y), radii EL+4, EL+5 (0 is 1):
;   each pixel by mode EL+8 (0: all, 1: sky, 2: fire become value EL+9;
;   mode 2's value is sky); EL+6 (FF: none) and EL+7 the radii of the one
;   inside it already drawn: only the rows outside it.
;   A column dx's half-height is ery * q32[j] / 64, j = |dx| * 32 / erx
;   (rounded, stepped without dividing: the Atari's rows, turned).
; ===========================================================================
ELX     EQU EL                  ; centre x (LE)
ELY     EQU EL+2                ; centre y (LE)
ELRX    EQU EL+4
ELRY    EQU EL+5
ELPX    EQU EL+6                ; the inner radii (FF: none)
ELPY    EQU EL+7
ELMODE  EQU EL+8
ELV     EQU EL+9
ELDX    EQU EL+10               ; |dx|
ELJ     EQU EL+11               ; j for erx, and its remainder
ELJR    EQU EL+12
ELK     EQU EL+13               ; j for the inner one, and its remainder
ELKR    EQU EL+14
ELD     EQU EL+15               ; the column's half-height
ELDO    EQU PT+6                ; the inner one's (FF: none)
ELC     EQU PT+7                ; a column's x (LE: PT+7, PT+8)
ELT     EQU PT+9                ; a run's top, bottom (LE: PT+9..12)

ELLIPSE: ENTER
        LDB ELRX                ; (0 is 1)
        NI 0FFH
        BNZ EL1
        LIS 1
        STB ELRX
EL1:    LDB ELRY
        NI 0FFH
        BNZ EL2
        LIS 1
        STB ELRY
EL2:    CLR
        STB ELDX
        STB ELJ
        STB ELK
        LDB ELRX                ; the remainders start at r / 2 (rounding)
        SR 1
        STB ELJR
        LDB ELPX
        SR 1
        STB ELKR
ELCOL:  LDB ELJ                 ; D = (ery q32[j] + 32) >> 6
        LR 0,A
        LDB ELRY
        LR 3,A
        PI QMUL
        STB ELD
        LI 0FFH                 ; the inner one's (none past its radius)
        STB ELDO
        LDB ELPX
        CI 0FFH
        BZ ELC1
        LR 1,A
        LDB ELDX
        COM                     ; |dx| > prx: none (prx + ~dx: C if prx > dx-1)
        INC
        AS 1
        BNC ELC1
        LDB ELK
        LR 0,A
        LDB ELPY
        LR 3,A
        PI QMUL
        STB ELDO
ELC1:   LDB ELDX                ; the column ex + |dx|
        LR 0,A
        LDB ELX
        AS 0
        STB ELC
        LDB ELX+1
        LNK
        STB ELC+1
        PI ECOL
        LDB ELDX                ; and ex - |dx| (not twice at 0)
        NI 0FFH
        BZ ELC2
        LR 0,A
        COM
        INC
        LR 0,A                  ; (-|dx|: its high byte FF)
        LDB ELX
        AS 0
        STB ELC
        LDB ELX+1
        LNK
        AI 0FFH
        STB ELC+1
        PI ECOL
ELC2:   LDB ELDX                ; the next |dx|, to erx
        LR 0,A
        LDB ELRX
        XS 0
        BZ ELX9
        LDB ELDX
        INC
        STB ELDX
        DCI ELJ                 ; j: rem += 32; while rem >= erx: rem -= erx, j++
        LM
        LR 1,A
        LM
        AI 32
        LR 2,A
        LDB ELRX
        LR 3,A
ELJ1:   LR A,3                  ; rem >= erx? (rem + ~erx + 1)
        COM
        INC
        AS 2
        BNC ELJ2
        LR 2,A
        LR A,1
        CI 31
        BNC ELJ1                ; (j at 32: stays)
        INC
        LR 1,A
        BR ELJ1
ELJ2:   DCI ELJ
        LR A,1
        ST
        LR A,2
        ST
        LDB ELPX                ; and the inner one's, if it has one
        CI 0FFH
        BZ ELC3
        LR 3,A
        DCI ELK
        LM
        LR 1,A
        LM
        AI 32
        LR 2,A
ELK1:   LR A,3
        COM
        INC
        AS 2
        BNC ELK2
        LR 2,A
        LR A,1
        CI 31
        BNC ELK1
        INC
        LR 1,A
        BR ELK1
ELK2:   DCI ELK
        LR A,1
        ST
        LR A,2
        ST
ELC3:   JMP ELCOL
ELX9:   LEAVE

; QMUL -- A = (r3 * q32[r0] + 32) >> 6 (a half-height).  Leaf.
;   Clobbers A, r0-r4.
QMUL:   LR A,0
        DCI Q32
        ADC
        LM
        LR 2,A                  ; q (0-64): its bits from the top
        CLR
        LR 0,A
        LR 1,A
        LIS 8
        LR 4,A
QM1:    LR A,0
        AS 0
        LR 0,A
        LR A,1
        LNK
        AS 1
        LR 1,A
        LR A,2
        AS 2
        LR 2,A
        BNC QM2
        LR A,0
        AS 3
        LR 0,A
        LR A,1
        LNK
        LR 1,A
QM2:    DS 4
        BNZ QM1
        LR A,0                  ; + 32, >> 6: (r1 << 2) + ((r0 + 32) >> 6)
        AI 32
        LR 0,A
        LR A,1
        LNK
        SL 1
        SL 1
        LR 1,A
        LR A,0
        SR 4
        SR 1
        SR 1
        AS 1
        POP
; a quarter circle: Q32[i] = 64 sqrt(1 - (i/32)^2) (the Atari's q32)
Q32:    DB 64, 64, 64, 63, 63, 62, 62, 61, 60, 59, 57, 56, 54, 53, 51, 49
        DB 46, 44, 41, 38, 35, 32, 28, 24, 20, 15, 10, 5, 0, 0, 0, 0, 0

; ECOL -- the ellipse's column ELC (16-bit): rows ey - D .. ey + D, or
;   with an inner one (ELDO not FF) only ey - D .. ey - DO - 1 and
;   ey + DO + 1 .. ey + D.  Off the field: nothing.
ECOL:   ENTER
        LDB ELC+1
        NI 0FFH
        BZ ECO1
        LEAVE
ECO1:   LDB ELC
        CI FW-1
        BC ECO2
        LEAVE
ECO2:   LDB ELDO
        CI 0FFH
        BNZ ECO3
        LDB ELD                 ; whole: ey - D .. ey + D
        LR 0,A
        LR 1,A
        PI ERUN
        LEAVE
ECO3:   LDB ELD                 ; above: ey - D .. ey - DO - 1
        LR 0,A
        LDB ELDO
        INC
        COM
        INC
        LR 1,A                  ; (-(DO + 1): from -D to -(DO+1))
        PI ERUNA
        LDB ELDO                ; below: ey + DO + 1 .. ey + D
        INC
        COM
        INC
        LR 0,A                  ; (-(DO+1) as "from" below: ERUNB)
        LDB ELD
        LR 1,A
        PI ERUNB
        LEAVE

; ERUN -- rows ey - r0 .. ey + r1 of column ELC (r0, r1 0-255).
ERUN:   ENTER
        LDB ELY                 ; top = ey - r0
        LR 2,A
        LDB ELY+1
        LR 3,A
        LR A,0
        COM
        INC
        LR 4,A                  ; (-r0)
        LR A,0
        NI 0FFH
        LI 0                    ; (r0 0: its high byte 0, else FF)
        BZ ERU1
        LI 0FFH
ERU1:   LR 5,A
        LR A,2
        AS 4
        STB ELT
        LR A,3
        LNK
        AS 5
        STB ELT+1
        LR A,2                  ; bottom = ey + r1
        AS 1
        STB ELT+2
        LR A,3
        LNK
        STB ELT+3
        PI EWRITE
        LEAVE

; ERUNA -- rows ey - r0 .. ey + r1 (r1 negative: -1 .. -255 as FF..01).
ERUNA:  ENTER
        LDB ELY
        LR 2,A
        LDB ELY+1
        LR 3,A
        LR A,0                  ; top = ey - r0
        COM
        INC
        LR 4,A
        LR A,0
        NI 0FFH
        LI 0
        BZ ERA1
        LI 0FFH
ERA1:   LR 5,A
        LR A,2
        AS 4
        STB ELT
        LR A,3
        LNK
        AS 5
        STB ELT+1
        LR A,2                  ; bottom = ey + r1 (r1 < 0: high byte FF)
        AS 1
        STB ELT+2
        LR A,3
        LNK
        AI 0FFH
        STB ELT+3
        PI EWRITE
        LEAVE

; ERUNB -- rows ey + r0 .. ey + r1, r0 = -(DO + 1) (so the top is
;   ey + DO + 1: minus r0) and r1 = D.
ERUNB:  ENTER
        LDB ELY
        LR 2,A
        LDB ELY+1
        LR 3,A
        LR A,0                  ; top = ey - r0 (= ey + DO + 1)
        COM
        INC
        LR 4,A                  ; (DO + 1: 1-256, as a byte 1..0)
        LR A,2
        AS 4
        STB ELT
        LR A,3
        LNK
        STB ELT+1
        LR A,4                  ; (DO + 1 = 256: its high byte 1)
        NI 0FFH
        BNZ ERB1
        LDB ELT+1
        INC
        STB ELT+1
ERB1:   LR A,2                  ; bottom = ey + r1
        AS 1
        STB ELT+2
        LR A,3
        LNK
        STB ELT+3
        PI EWRITE
        LEAVE

; EWRITE -- rows ELT .. ELT+2 (16-bit, signed) of column ELC, clipped to
;   the field: the mode's kernel (FCOLW: all; FCOLSW: from -> to).
EWRITE: ENTER
        LDB ELT+3               ; bottom < 0: nothing
        NI 0FFH
        BM EWX
        BZ EW1
        LI FH-1                 ; past the field: its last row
        STB ELT+2
EW1:    LDB ELT+2
        CI FH-1
        BC EW2
        LI FH-1
        STB ELT+2
EW2:    LDB ELT+1               ; top < 0: 0; past the field: nothing
        NI 0FFH
        BZ EW3
        BP EWX
        CLR
        STB ELT
EW3:    LDB ELT                 ; top > bottom: nothing (top + ~bottom carries)
        LR 6,A
        LDB ELT+2
        LR 7,A
        COM
        AS 6
        BC EWX
        LR A,6                  ; count = bottom - top + 1
        COM
        INC
        AS 7
        INC
        LR 7,A
        LDB ELC
        LR 5,A
        LDB ELMODE
        NI 0FFH
        BNZ EW4
        LDB ELV                 ; all: FCOLW
        LR 1,A
        PI FCOLW
        LEAVE
EW4:    CI 1                    ; sky becomes the value
        BNZ EW5
        LI PX_SKY
        LR 8,A
        LDB ELV
        LR 1,A
        PI FCOLSW
        LEAVE
EW5:    LI PX_FX                ; fire becomes sky
        LR 8,A
        LI PX_SKY
        LR 1,A
        PI FCOLSW
EWX:    LEAVE

; ===========================================================================
; pixels at 16-bit (x, y): r5 x, r2 its high byte; r6 y, r3 its high byte.
; Off the field: a get is sky, a set does nothing (the Atari's fld.s).
; ===========================================================================
; PGET -- A = the pixel.  Leaf.  Clobbers A, H, DC0.
PGET:   LR A,2
        NI 0FFH
        BNZ PGS
        LR A,3
        NI 0FFH
        BNZ PGS
        LR A,5
        CI FW-1
        BNC PGS
        LR A,6
        CI FH-1
        BNC PGS
        JMP FGET                ; (its POP returns for us)
PGS:    LI PX_SKY
        POP

; PPLOT -- the pixel = r1.  Leaf.  Clobbers A, r4, H, DC0.
PPLOT:  LR A,2
        NI 0FFH
        BNZ PPX
        LR A,3
        NI 0FFH
        BNZ PPX
        LR A,5
        CI FW-1
        BNC PPX
        LR A,6
        CI FH-1
        BNC PPX
        JMP FSET
PPX:    POP

; PSWAP -- the pixel = r1 if it is r8.  Leaf.  Clobbers A, r4, H, DC0.
PSWAP:  LR A,2
        NI 0FFH
        BNZ PSX
        LR A,3
        NI 0FFH
        BNZ PSX
        LR A,5
        CI FW-1
        BNC PSX
        LR A,6
        CI FH-1
        BNC PSX
        LR A,5                  ; (FADDR inline: a leaf can't call one)
        SR 1
        SR 1
        AI FLD>>8
        LR 10,A
        LR A,5
        SL 4
        SL 1
        SL 1
        AS 6
        LR 11,A
        LR DC,H
        LM
        XS 8
        BNZ PSX
        LR A,1                  ; (it is r8: r1 differs unless r1 = r8)
        XS 8
        BZ PSX
        JMP FSETW
PSX:    POP

; ===========================================================================
; tracks (trk.s): OP_ARC slot, style, x16, y16, vx24, vy24, ax16, ay16, n16.
; A record (32): 0 on, 1 style, 2-4 X (lo, mid, hi), 5-7 Y, 8-10 VX,
; 11-13 VY, 14-15 AX, 16-17 AY, 18-19 n, 20-21 px, 22-23 py (LE), 24 under,
; 25 drawn.  X, Y 12.12; each step X += VX, Y += VY, VX += AX, VY += AY
; (AX, AY sign-extended); the head at (X >> 12, Y >> 12), FX over sky (a
; smoke trail's over fire too, and it stays).
; ===========================================================================
; TREC -- DC0 = track r0's record.  Leaf.
TREC:   LR A,0
        SL 4
        SL 1
        DCI TRK
        ADCU
        POP

; TLOAD -- the OP_ARC at Q starts its track; Q past its 18 bytes.
TLOAD:  ENTER
        PI QSAVE                ; (TOFF, TON spend Q)
        LR DC,Q
        LM
        NI NTRK-1
        STB PI0
        LR 0,A
        PI TOFF                 ; its old head away
        PI QLOAD
        LDB PI0
        LR 0,A
        PI TREC
        XDC                     ; DC1: the record
        LR DC,Q
        LM                      ; (slot)
        LIS 1                   ; on
        XDC
        ST
        XDC
        LM                      ; style
        XDC
        ST
        CLR                     ; X = x16 << 8
        ST
        XDC
        LM
        XDC
        ST
        XDC
        LM
        XDC
        ST
        CLR                     ; Y = y16 << 8
        ST
        XDC
        LM
        XDC
        ST
        XDC
        LM
        XDC
        ST
        LI 12                   ; VX, VY, AX, AY, n: as they come
        LR 4,A
TLD1:    XDC
        LM
        XDC
        ST
        DS 4
        BNZ TLD1
        CLR                     ; px, py, under, drawn: 0
        ST
        ST
        ST
        ST
        ST
        ST
        XDC
        LR Q,DC
        PI QSAVE
        LDB PI0
        LR 0,A
        PI TON
        PI QLOAD
        LEAVE

; TSTEPALL -- every live track a step; PFALL the lowest falling shell's
;   line | 1 (the whistle), or 0.
TSTEPALL: ENTER
        CLR
        STB PFALL
        STB PI0
TSA1:    LDB PI0
        LR 0,A
        PI TREC
        LM
        NI 0FFH
        BZ TS9
        LDB PI0
        LR 0,A
        PI TSTEP1
TS9:    LDB PI0
        INC
        STB PI0
        CI NTRK
        BNZ TSA1
        LEAVE

; TSTEP1 -- track r0 a step: its head off, n-- (none left: it ends), the
;   recurrence, its head on.
TSTEP1: ENTER
        LR A,0
        STB PT+10
        PI TOFF
        LDB PT+10
        LR 0,A
        PI TREC
        LI 18                   ; n: none left, it ends
        ADC
        LM
        LR 0,A
        LM
        LR 1,A
        LR A,0
        NI 0FFH
        BNZ TSA2
        LR A,1
        NI 0FFH
        BNZ TSA2
        LDB PT+10
        LR 0,A
        PI TREC
        CLR
        ST
        LEAVE
TSA2:    LR A,0                  ; n-- (16-bit)
        AI 0FFH
        LR 0,A
        LR A,1
        LNK
        AI 0FFH
        LR 1,A
        LI 0FEH
        ADC
        LR A,0
        ST
        LR A,1
        ST
        LDB PT+10
        LR 0,A
        PI TREC
        LR Q,DC
        PI TMOVE
        LDB PT+10
        LR 0,A
        PI TON
        LEAVE

; TMOVE -- the record at Q: X += VX, Y += VY, VX += AX, VY += AY.  Leaf.
;   Clobbers A, r0-r11, DCs.
TMOVE:  LR DC,Q                 ; X (2-4), Y (5-7)
        LIS 2
        ADC
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A
        LM
        LR 3,A
        LM
        LR 4,A
        LM
        LR 5,A
        LM                      ; + VX (8-10)
        AS 0
        LR 0,A
        ADCM 1
        LR 1,A
        ADCM 2
        LR 2,A
        LM                      ; + VY (11-13)
        AS 3
        LR 3,A
        ADCM 4
        LR 4,A
        ADCM 5
        LR 5,A
        LR DC,Q                 ; X, Y back
        LIS 2
        ADC
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        LR A,4
        ST
        LR A,5
        ST
        LM                      ; VX (8-10), VY (11-13), AX (14-15), AY (16-17)
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A
        LM
        LR 3,A
        LM
        LR 4,A
        LM
        LR 5,A
        LM
        LR 6,A                  ; AX lo
        LM
        LR 7,A                  ; AX hi
        LM
        LR 8,A                  ; AY lo
        LM
        LR 10,A                 ; AY hi
        LR A,7                  ; r11 = AX's sign: 0 or FF
        AS 7
        CLR
        LNK
        COM
        INC
        LR 11,A
        LR A,0                  ; VX += AX
        AS 6
        LR 0,A
        ADC3 1,7
        LR 1,A
        ADC3 2,11
        LR 2,A
        LR A,10                 ; r11 = AY's sign
        AS 10
        CLR
        LNK
        COM
        INC
        LR 11,A
        LR A,3                  ; VY += AY
        AS 8
        LR 3,A
        ADC3 4,10
        LR 4,A
        ADC3 5,11
        LR 5,A
        LR DC,Q                 ; VX, VY back
        LI 8
        ADC
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        LR A,4
        ST
        LR A,5
        ST
        POP

; TON -- track r0's head on, at (X >> 12, Y >> 12) (arithmetic): drawn FX
;   over sky (a smoke trail's over fire too), the pixel under it kept.
;   The whistle's line (PFALL) from a falling shell's.
TON:    ENTER
        LR A,0
        STB PT+11
        PI TREC
        LR Q,DC
        LIS 3                   ; px = (Xhi:Xmid) >> 4
        ADC
        LM
        LR 0,A                  ; mid
        LM
        LR 1,A                  ; hi
        LM                      ; Y's lo (5): skip
        LM
        LR 2,A                  ; Y mid
        LM
        LR 3,A                  ; Y hi
        LR A,1                  ; pxl = (hi << 4) | (mid >> 4)
        SL 4
        LR 4,A
        LR A,0
        SR 4
        AS 4
        LR 5,A                  ; x lo
        LR A,1                  ; pxh = hi >> 4, the sign in
        SR 4
        LR 4,A
        LR A,1
        NI 80H
        BZ TO1
        LR A,4
        OI 0F0H
        LR 4,A
TO1:    LR A,3                  ; pyl, pyh the same
        SL 4
        LR 6,A
        LR A,2
        SR 4
        AS 6
        LR 6,A                  ; y lo
        LR A,3
        SR 4
        LR 7,A
        LR A,3
        NI 80H
        BZ TO2
        LR A,7
        OI 0F0H
        LR 7,A
TO2:    LR DC,Q                 ; px, py (20-23)
        LI 20
        ADC
        LR A,5
        ST
        LR A,4
        ST
        LR A,6
        ST
        LR A,7
        ST
        LR A,4                  ; (r2, r3: the high bytes)
        LR 2,A
        LR A,7
        LR 3,A
        PI PGET
        LR 1,A                  ; the pixel under it
        CI PX_SKY
        BZ TO4
        CI PX_FX                ; a trail's over fire too (never over dirt
        BNZ TO6                 ;  or a tank)
        LR DC,Q
        LIS 1
        ADC
        LM
        NI 15
        CI PS_SMOKE
        BNZ TO6
TO4:    LR DC,Q                 ; under = it, drawn = 1
        LI 24
        ADC
        LR A,1
        ST
        LIS 1
        ST
        LI PX_FX
        LR 1,A
        PI PPLOT
TO6:    LR DC,Q                 ; falling (VY > 0), on the field, not a
        LIS 13                  ;  digger: the lowest is the whistle's
        ADC
        LM                      ; VY hi
        NI 80H
        BNZ TO9
        LR A,3
        NI 0FFH
        BNZ TO9
        LR DC,Q
        LIS 1
        ADC
        LM
        NI 15
        CI PS_DIGGER
        BZ TO9
        LR A,6                  ; py >= PFALL: PFALL = py | 1
        LR 0,A
        LDB PFALL
        COM
        AS 0                    ; (py + ~PFALL: C when py > PFALL - 1)
        BNC TO9
        LR A,0
        OI 1
        STB PFALL
TO9:    LEAVE

; TOFF -- track r0's head off: its pixel back (unless a trail's, or it is
;   no longer fire), not drawn.
TOFF:   ENTER
        PI TREC
        LR Q,DC
        LI 25
        ADC
        LM                      ; drawn?
        NI 0FFH
        BZ TOFX
        LI 0FFH
        ADC
        CLR
        ST
        LR DC,Q                 ; a trail's stays
        LIS 1
        ADC
        LM
        NI 15
        CI PS_SMOKE
        BZ TOFX
        LR DC,Q                 ; its place (20-23) and what was under it (24)
        LI 20
        ADC
        LM
        LR 5,A
        LM
        LR 2,A
        LM
        LR 6,A
        LM
        LR 3,A
        LM
        LR 1,A                  ; under
        LI PX_FX                ; still fire: back to what it was
        LR 8,A
        PI PSWAP
TOFX:   LEAVE

; ===========================================================================
; paths (ops.s): PATH slot, style, x16, y16, n, n nibble steps (dx << 4 |
; dy & 15).  A record (16): 0 on, 1 sty, 2 x, 3 y, 4 n, 5 dr, 6 ph, 7 pl.
; x, y bytes: one that wraps past an edge is off the field (as the Atari's).
; ===========================================================================
PREC:   LR A,0                  ; DC0 = path r0's record.  Leaf.
        SL 4
        DCI PATHS
        ADC
        POP

; OPATH -- the path at Q; Q past it (7 + n).
OPATH:  ENTER
        LR DC,Q
        LM
        NI NPATH-1
        STB PI0
        PI QSAVE                ; (PEND1 spends Q)
        LDB PI0
        LR 0,A
        PI PEND1                ; that slot's path ends first
        PI QLOAD
        LR DC,Q
        LM                      ; (slot)
        LM
        LR 1,A                  ; style
        LM
        LR 2,A                  ; x lo
        LM
        LM
        LR 3,A                  ; y lo
        LM
        LM
        LR 4,A                  ; n
        LR Q,DC                 ; (its steps)
        LDB PI0
        LR 0,A
        PI PREC
        LIS 1                   ; on, sty, x, y, n, dr 0, steps
        ST
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        LR A,4
        ST
        CLR
        ST
        LR A,QU
        ST
        LR A,QL
        ST
        LDB PI0
        LR 0,A
        PI PHEAD
        LDB PI0                 ; past its steps (n again: PHEAD spends r4)
        LR 0,A
        PI PREC
        LIS 4
        ADC
        LM
        LR DC,Q
        ADCU
        LR Q,DC
        LEAVE

; PHEAD -- path r0's head drawn: FX over air (a digger's over dirt too).
PHEAD:  ENTER
        LR A,0
        STB PT+12
        PI PREC
        LM                      ; (on)
        LM
        STB PT+13               ; sty
        LDB PT+12
        LR 0,A
        PI PREC
        LIS 2
        ADC
        LM
        LR 5,A                  ; x
        LM
        LR 6,A                  ; y
        CLR
        LR 2,A
        LR 3,A
        PI PGET
        CI PX_SKY
        BZ PH1
        CI PX_DIRT
        BNZ PH9
        LDB PT+13
        CI PS_DIGGER
        BNZ PH9
PH1:    LI PX_FX
        LR 1,A
        PI PPLOT
        LDB PT+12               ; dr = 1
        LR 0,A
        PI PREC
        LIS 5
        ADC
        LIS 1
        ST
PH9:    LEAVE

; PLEAVE -- path r0's head moves on: its fire back to air (a digger has
;   bored: air behind it).
PLEAVE: ENTER
        PI PREC
        LR Q,DC
        LIS 5
        ADC
        LM
        NI 0FFH
        BZ PLX
        LI 0FFH
        ADC
        CLR
        ST
        LR DC,Q
        LIS 2
        ADC
        LM
        LR 5,A
        LM
        LR 6,A
        CLR
        LR 2,A
        LR 3,A
        LI PX_FX
        LR 8,A
        LI PX_SKY
        LR 1,A
        PI PSWAP
PLX:    LEAVE

; PEND1 -- path r0 stops.
PEND1:  ENTER
        LR A,0
        STB PT+12
        PI PREC
        LM
        NI 0FFH
        BZ PE9
        LDB PT+12
        LR 0,A
        PI PLEAVE
        LDB PT+12
        LR 0,A
        PI PREC
        CLR
        ST
PE9:    LEAVE

; PATHSTEP -- each path a step (n left: its head moves; none: it ends).
PATHSTEP: ENTER
        CLR
        STB PT+11
PA1:    LDB PT+11
        LR 0,A
        PI PREC
        LM
        NI 0FFH
        BZ PA9
        LIS 3                   ; n
        ADC
        LM
        NI 0FFH
        BNZ PA2
        LDB PT+11
        LR 0,A
        PI PEND1
        BR PA9
PA2:    LDB PT+11
        LR 0,A
        PI PLEAVE
        LDB PT+11               ; n--, the step, steps++
        LR 0,A
        PI PREC
        LR Q,DC
        LIS 4
        ADC
        LM
        AI 0FFH
        LR 1,A
        LI 0FFH
        ADC
        LR A,1
        ST
        LR DC,Q
        LIS 6
        ADC
        LM
        LR QU,A                 ; (the record in H while Q walks the steps)
        LM
        LR QL,A
        LR H,DC
        LR DC,Q
        LM                      ; the step: dx << 4 | dy & 15
        LR 1,A
        LR Q,DC
        LR DC,H
        LI 0FEH
        ADC
        LR A,QU
        ST
        LR A,QL
        ST
        LI 0F8H                 ; (back to the record's start)
        ADC
        LR H,DC
        LR A,1                  ; x += dx (arithmetic >> 4)
        SR 4
        LR 2,A
        LR A,1
        NI 80H
        BZ PA3
        LR A,2
        OI 0F0H
        LR 2,A
PA3:    LR A,1                  ; y += dy (the low nibble, signed)
        SL 4
        LR 3,A
        SR 4
        LR 4,A
        LR A,3
        NI 80H
        BZ PA4
        LR A,4
        OI 0F0H
        LR 4,A
PA4:    LR DC,H
        LIS 2
        ADC
        LM
        AS 2
        LR 2,A
        LM
        AS 4
        LR 4,A
        LR DC,H
        LIS 2
        ADC
        LR A,2
        ST
        LR A,4
        ST
        LDB PT+11
        LR 0,A
        PI PHEAD
PA9:    LDB PT+11
        INC
        STB PT+11
        CI NPATH
        BZ PA10
        JMP PA1
PA10:   LEAVE

; ===========================================================================
; flows (ops.s): FLOW kind, frames, hold, n, n cells (x16, y8).  A record
; (16): 0 on, 1 kind, 2 frm, 3 f, 4 hold, 5 n, 6 sh, 7 accl, 8 acch, 9 ph,
; 10 pl.  Its cells appear evenly over the frames (fire over air, mud as
; dirt over air and fire); a fire goes out hold frames after its last.
; ===========================================================================
FREC:   LR A,0                  ; DC0 = flow r0's record.  Leaf.
        SL 4
        DCI FLOWS
        ADC
        POP

; OFLOW -- the flow at Q into a free slot (full: the last ends now); Q past
;   it (4 + 3n).
OFLOW:  ENTER
        CLR
        LR 4,A
OF1:    LR A,4
        LR 0,A
        PI FREC
        LM
        NI 0FFH
        BZ OF2
        LR A,4
        INC
        LR 4,A
        CI NFLOW
        BNZ OF1
        LIS NFLOW-1             ; full: the last ends now
        LR 4,A
        LR 0,A
        PI QSAVE
        LR A,4
        STB PT+11
        LR 0,A
        PI FEND
        PI QLOAD
        LDB PT+11
        LR 4,A
OF2:    LR A,4
        STB PT+11
        LR DC,Q
        LM
        LR 1,A                  ; kind
        LM
        NI 0FFH                 ; frames (0: 1)
        BNZ OF3
        LIS 1
OF3:    LR 2,A
        LM
        LR 3,A                  ; hold
        LM
        LR 5,A                  ; n
        LR Q,DC                 ; its cells
        LR A,4
        LR 0,A
        PI FREC                 ; on, kind, frm, f, hold, n, sh, accl, acch, ph, pl
        LIS 1
        ST
        LR A,1
        ST
        LR A,2
        ST
        CLR
        ST
        LR A,3
        ST
        LR A,5
        ST
        CLR
        ST
        ST
        ST
        LR A,QU
        ST
        LR A,QL
        ST
        LR DC,Q                 ; past them: 3n
        LR A,5
        ADCU
        LR A,5
        ADCU
        LR A,5
        ADCU
        LR Q,DC
        LEAVE

; FCELLS -- flow PT+12's cells PT+14 .. PT+15 - 1: each that is r8 becomes
;   r1; mud (PT+13 1): fire becomes r1 too.
FCELLS: ENTER
        LR A,8
        STB EL                  ; (from, to: kept over the swaps)
        LR A,1
        STB EL+1
FCLL1:   LDB PT+14
        LR 0,A
        LDB PT+15
        XS 0
        BZ FCLX
        LDB PT+12               ; the cell: ph:pl + 3 i
        LR 0,A
        PI FREC
        LI 9
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LDB PT+14
        LR 0,A
        LR DC,Q
        ADCU
        LR A,0
        ADCU
        LR A,0
        ADCU
        LM
        LR 5,A                  ; x lo
        LM
        LR 2,A                  ; x hi
        LM
        LR 6,A                  ; y
        CLR
        LR 3,A
        LDB EL
        LR 8,A
        LDB EL+1
        LR 1,A
        PI PSWAP
        LDB PT+13               ; mud: fire too
        NI 0FFH
        BZ FCL2
        LI PX_FX
        LR 8,A
        PI PSWAP
FCL2:   LDB PT+14
        INC
        STB PT+14
        BR FCLL1
FCLX:   LEAVE

; FSHOW1 -- flow PT+12's first r0 cells in (fire over the air; mud as dirt).
FSHOW1: ENTER
        LR A,0
        STB PT+15               ; (to)
        LDB PT+12
        LR 0,A
        PI FREC
        LIS 6
        ADC
        LM                      ; sh: more than it shows?
        LR 1,A
        LDB PT+15
        COM
        AS 1                    ; (sh + ~to: C when sh >= to... sh > to - 1)
        BC FSH9
        LR A,1
        STB PT+14               ; from sh
        LDB PT+12               ; sh = to
        LR 0,A
        PI FREC
        LIS 6
        ADC
        LDB PT+15
        LR 1,A
        LDB PT+12
        LR 0,A
        PI FREC
        LIS 6
        ADC
        LR A,1
        ST
        LDB PT+12               ; kind: mud as dirt
        LR 0,A
        PI FREC
        LM
        LM
        CI FLOW_MUD
        LIS 1
        BZ FSW1
        CLR
FSW1:   STB PT+13
        LI PX_SKY
        LR 8,A
        LDB PT+13
        NI 0FFH
        LI PX_FX
        BZ FSW2
        LI PX_DIRT
FSW2:   LR 1,A
        PI FCELLS
FSH9:   LEAVE

; FEND -- flow r0 all in, then a fire goes out; off.
FEND:   ENTER
        LR A,0
        STB PT+12
        PI FREC
        LIS 5
        ADC
        LM
        LR 0,A                  ; n: all in
        PI FSHOW1
        LDB PT+12
        LR 0,A
        PI FREC
        LM
        LM
        CI FLOW_MUD
        BZ FEN1
        CLR                     ; a fire out: its cells' fire back to air
        STB PT+14
        STB PT+13
        LDB PT+12
        LR 0,A
        PI FREC
        LIS 5
        ADC
        LM
        STB PT+15
        LI PX_FX
        LR 8,A
        LI PX_SKY
        LR 1,A
        PI FCELLS
FEN1:   LDB PT+12
        LR 0,A
        PI FREC
        CLR
        ST
        LEAVE

; FLOWSTEP -- each flow a frame: its cells so far by the frames, then its
;   hold; all of it shown and the hold over: it ends.
FLOWSTEP: ENTER
        CLR
        STB PT+11
FW1:    LDB PT+11
        LR 0,A
        STB PT+12
        PI FREC
        LM
        NI 0FFH
        BNZ FW2
        JMP FW9
FW2:    LDB PT+12
        LR 0,A
        PI FREC
        LIS 2
        ADC
        LM
        LR 2,A                  ; frm
        LM
        LR 3,A                  ; f
        LR A,2                  ; f < frm: a frame more
        COM
        AS 3                    ; (f + ~frm: C when f >= frm)
        BC FW5
        LDB PT+12               ; f++, acc += n, cells while acc >= frm
        LR 0,A
        PI FREC
        LIS 3
        ADC
        LR A,3
        INC
        ST
        LM                      ; (hold)
        LM
        LR 4,A                  ; n
        LM
        LR 5,A                  ; sh
        LM
        LR 6,A                  ; accl
        LM
        LR 7,A                  ; acch
        LR A,6
        AS 4
        LR 6,A
        LR A,7
        LNK
        LR 7,A
FW3:    LR A,2                  ; acc >= frm: acc -= frm, a cell more
        COM
        INC
        AS 6
        LR 0,A
        LR A,7
        LNK
        AI 0FFH
        BNC FW4                 ; (a borrow: acc < frm)
        LR 7,A
        LR A,0
        LR 6,A
        LR A,5
        INC
        LR 5,A
        BR FW3
FW4:    LDB PT+12               ; acc back
        LR 0,A
        PI FREC
        LIS 7
        ADC
        LR A,6
        ST
        LR A,7
        ST
        LR A,5
        LR 0,A
        PI FSHOW1
        BR FW6
FW5:    LDB PT+12               ; the hold: one less
        LR 0,A
        PI FREC
        LIS 4
        ADC
        LM
        NI 0FFH
        BZ FW6
        AI 0FFH
        LR 1,A
        LI 0FFH
        ADC
        LR A,1
        ST
FW6:    LDB PT+12               ; f >= frm and no hold: it ends
        LR 0,A
        PI FREC
        LIS 2
        ADC
        LM
        LR 2,A
        LM
        LR 3,A
        LM
        LR 4,A                  ; hold
        LR A,2
        COM
        AS 3
        BNC FW9
        LR A,4
        NI 0FFH
        BNZ FW9
        LDB PT+12
        LR 0,A
        PI FEND
FW9:    LDB PT+11
        INC
        STB PT+11
        CI NFLOW
        BZ FWX
        JMP FW1
FWX:    LEAVE

; ===========================================================================
; beams (ops.s): BEAM x0 16, y0 16, x1 16, y1 16, kind, frames.  A record
; (8): 0 left, 1 x0, 2 y0, 3 x1, 4 y1 (low bytes: a wrap is off the field).
; Bresenham as the reference view: on, fire over air and dirt; off, fire
; back to air.
; ===========================================================================
BRC:    LR A,0                  ; DC0 = beam r0's record.  Leaf.
        SL 1
        SL 1
        SL 1
        DCI BEAMS
        ADC
        POP

; OBEAM -- the beam at Q (into slot 0, or 1 if 0 is on: that one off
;   first); Q past its 10 bytes.
OBEAM:  ENTER
        CLR
        LR 4,A
        DCI BEAMS
        LM
        NI 0FFH
        BZ OBE1
        LIS 1
        LR 4,A
OBE1:   LR A,4
        STB PT+11
        LR 0,A
        PI BOFF
        LR DC,Q
        LM
        LR 1,A                  ; x0
        LM
        LM
        LR 2,A                  ; y0
        LM
        LM
        LR 3,A                  ; x1
        LM
        LM
        LR 4,A                  ; y1
        LM
        LM                      ; (kind) frames (0: 1)
        LM
        NI 0FFH
        BNZ OBE2
        LIS 1
OBE2:   LR 5,A
        LR Q,DC
        LDB PT+11
        LR 0,A
        PI BRC
        LR A,5
        ST
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        LR A,4
        ST
        LIS 1
        STB PT+12               ; on
        LDB PT+11
        LR 0,A
        PI BLINE
        LEAVE

; BOFF -- beam r0's pixels back to air; it is gone.
BOFF:   ENTER
        LR A,0
        STB PT+11
        PI BRC
        LM
        NI 0FFH
        BZ BOX
        LI 0FFH
        ADC
        CLR
        ST
        STB PT+12               ; off
        LDB PT+11
        LR 0,A
        PI BLINE
BOX:    LEAVE

; BEAMSTEP -- each beam a frame; at its last, off.
BEAMSTEP: ENTER
        CLR
        STB PT+10
BMS1:    LDB PT+10
        LR 0,A
        PI BRC
        LM
        NI 0FFH
        BZ BMS2
        AI 0FFH
        LR 1,A
        LI 0FFH
        ADC
        LR A,1
        ST
        LR A,1
        NI 0FFH
        BNZ BMS2
        LIS 1                   ; (BOFF wants it live)
        LR 1,A
        LI 0FFH
        ADC
        LR A,1
        ST
        LDB PT+10
        LR 0,A
        PI BOFF
BMS2:    LDB PT+10
        INC
        STB PT+10
        CI NBEAM
        BNZ BMS1
        LEAVE

; BLINE -- beam r0's line, on (PT+12 1) or off.
BLX0    EQU EL                  ; the line: x0, y0, x1, y1, |dx|, |dy|, sx,
BLY0    EQU EL+1                ;  sy, err (LE), e2 (LE)
BLX1    EQU EL+2
BLY1    EQU EL+3
BLDX    EQU EL+4
BLDY    EQU EL+5
BLSX    EQU EL+6
BLSY    EQU EL+7
BLERR   EQU EL+8
BLE2    EQU EL+10
BLNDX   EQU EL+12               ; -dx, -dy (16-bit)
BLNDY   EQU EL+14

; NEG16 -- r1:r0 = -r0 (16-bit).  Leaf.
NEG16:  LR A,0
        NI 0FFH
        BNZ NG1
        LR 1,A                  ; (0: 0)
        POP
NG1:    LR A,0
        COM
        INC
        LR 0,A
        LI 0FFH
        LR 1,A
        POP

BLINE:  ENTER
        PI BRC
        LM                      ; (left)
        LM
        LR 1,A
        LM
        LR 2,A
        LM
        LR 3,A
        LM
        LR 4,A
        DCI BLX0
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        LR A,4
        ST
        LR A,3                  ; |dx|, its sign: x0 > x1? (x0 + ~x1 carries)
        COM
        AS 1
        BC BLN1
        LR A,1                  ; x1 - x0
        COM
        INC
        AS 3
        STB BLDX
        LIS 1
        BR BLN2
BLN1:   LR A,3                  ; x0 - x1
        COM
        INC
        AS 1
        STB BLDX
        LI 0FFH
BLN2:   STB BLSX
        LR A,4                  ; |dy|, its sign
        COM
        AS 2
        BC BLN3
        LR A,2
        COM
        INC
        AS 4
        STB BLDY
        LIS 1
        BR BLN4
BLN3:   LR A,4
        COM
        INC
        AS 2
        STB BLDY
        LI 0FFH
BLN4:   STB BLSY
        LDB BLDX                ; -dx, -dy (16-bit: 0 stays 0)
        LR 0,A
        PI NEG16
        DCI BLNDX
        LR A,0
        ST
        LR A,1
        ST
        LDB BLDY
        LR 0,A
        PI NEG16
        DCI BLNDY
        LR A,0
        ST
        LR A,1
        ST
        LDB BLDX                ; err = dx - dy (= dx + -dy)
        LR 2,A
        DCI BLNDY
        LM
        AS 2
        LR 2,A
        LM
        LNK
        DCI BLERR+1
        ST
        DCI BLERR
        LR A,2
        ST
BLL:    PI BPLOT
        DCI BLX0                ; the end?
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        XS 0
        BNZ BLGO
        LM
        XS 1
        BNZ BLGO
        LEAVE
BLGO:   DCI BLERR               ; e2 = 2 err
        LM
        LR 0,A
        LM
        LR 1,A
        LR A,0
        AS 0
        LR 2,A
        LR A,1
        LNK
        AS 1
        LR 3,A
        DCI BLE2
        LR A,2
        ST
        LR A,3
        ST
        LDB BLDY                ; e2 > -dy (e2 + dy > 0): x moves
        AS 2
        LR 4,A
        LR A,3
        LNK
        BM BLY
        LR 5,A
        NI 0FFH
        BNZ BLXM
        LR A,4
        NI 0FFH
        BZ BLY
BLXM:   DCI BLNDY               ; err -= dy (+= -dy)
        LM
        LR 4,A
        LM
        LR 5,A
        LDB BLERR
        AS 4
        STB BLERR
        LDB BLERR+1
        LNK
        AS 5
        STB BLERR+1
        LDB BLSX
        LR 4,A
        LDB BLX0
        AS 4
        STB BLX0
BLY:    DCI BLE2                ; e2 < dx: y moves (e2 + -dx < 0)
        LM
        LR 2,A
        LM
        LR 3,A
        DCI BLNDX
        LM
        AS 2
        LM
        LNK
        AS 3
        BP BLL0
        LDB BLDX                ; err += dx
        LR 4,A
        LDB BLERR
        AS 4
        STB BLERR
        LDB BLERR+1
        LNK
        STB BLERR+1
        LDB BLSY
        LR 4,A
        LDB BLY0
        AS 4
        STB BLY0
BLL0:   JMP BLL

; BPLOT -- (BLX0, BLY0): on, fire over air and dirt; off, fire back to air.
BPLOT:  ENTER
        LDB BLX0
        LR 5,A
        LDB BLY0
        LR 6,A
        CLR
        LR 2,A
        LR 3,A
        LDB PT+12
        NI 0FFH
        BZ BPO1
        LI PX_FX
        LR 1,A
        LI PX_SKY
        LR 8,A
        PI PSWAP
        LI PX_DIRT
        LR 8,A
        LI PX_FX
        LR 1,A
        PI PSWAP
        LEAVE
BPO1:   LI PX_FX
        LR 8,A
        LI PX_SKY
        LR 1,A
        PI PSWAP
        LEAVE

; ===========================================================================
; a segment's end (ply.s ply_end): settles land, paths stop, flows run out
; ===========================================================================
PENDSEG: ENTER
        CLR
        STB PI0
PN1:    PI SLAND
        LDB PI0
        INC
        STB PI0
        CI NSETTLE
        BNZ PN1
        CLR
        STB PT+11
PN2:    LDB PT+11
        LR 0,A
        PI PEND1
        LDB PT+11
        INC
        STB PT+11
        CI NPATH
        BNZ PN2
        CLR
        STB PT+11
PN3:    LDB PT+11
        LR 0,A
        PI FREC
        LM
        NI 0FFH
        BZ PN4
        LDB PT+11
        LR 0,A
        PI FEND
PN4:    LDB PT+11
        INC
        STB PT+11
        CI NFLOW
        BNZ PN3
        LEAVE
