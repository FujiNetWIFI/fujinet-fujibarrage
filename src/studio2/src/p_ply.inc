; p_ply.inc -- the shot player (docs/PROTOCOL.md "Playing a segment";
; server/internal/view view.go is the reference, op for op).
;
; A segment whole in SCRIPT is played a pass at a time (ply_pass): the
; pass's jiffies (DT, PLY_CATCHUP at most) are that many frames; each one
; (ply_frame) everything moves a step (pf_*: view.go animate), then the ops
; due run (ply_ops). The picture is drawn once a pass (ply_draw), held so it
; shows whole.
;
; THE FIELD stays exact through the model: only OP_SETTLE writes it (at its
; landing frame, or at the segment's end), as TERRAIN does (fld.inc). The
; rest is the picture's: fireballs in the check pattern grown in stages,
; craters cut, dirt balls and mud, beams, paths' heads, fire; the settles
; that follow draw the truth, and what no settle follows (a plasma or puff
; fireball) gives its columns back to the model (fld_flush).
;
; TRACKS are the cart's sprites 10-15 (the server keeps six for us): the
; 24-bit ARC in console RAM (PTRK), the rest of each in the cart's RAM
; (PTREC). Records in the cart's RAM hold absolute frames (PNOW's) for their
; ends, so they change only when made and when they end.

PLY_CATCHUP EQU 8               ; frames a pass at most
SPR_TRK EQU     10              ; track k: sprite 10 + k
IM_SHELL EQU    20              ; the shell's image (21 its halo)
FIRE_MODE EQU   TM_SET+TP_CHECK ; a fireball (Checkpoint 1)

; the records in the cart's RAM (ram.inc: PTREC, PBREC, PSREC, PPREC,
; PFREC, PEREC, in that order, to PRECEND):
TR_ON   EQU     0               ; track: on, style, AX (LE), AY (LE), the
TR_STY  EQU     1               ;  frame it goes (LE)
TR_AX   EQU     2
TR_AY   EQU     4
TR_END  EQU     6
TR_SH   EQU     3               ; (8 bytes)
BL_ON   EQU     0               ; blast: on (BLK_*), x, y (signed bytes: the
BL_X    EQU     1               ;  field's), rx, ry, kind, total frames, the
BL_Y    EQU     2               ;  frame it began (LE)
BL_RX   EQU     3
BL_RY   EQU     4
BL_KIND EQU     5
BL_TOT  EQU     6
BL_T0   EQU     7
BL_END  EQU     9               ; (LE: T0 + total)
BL_SH   EQU     4               ; (16 bytes)
BLK_BLAST EQU   1
BLK_DIRT EQU    2               ; an OP_DIRT: dirt fills the air
SE_ON   EQU     0               ; settle: on, x, n, its entries (SCRIPT's
SE_X    EQU     1               ;  address, big-endian), the frame it lands
SE_N    EQU     2
SE_AT   EQU     3
SE_LAND EQU     5               ; (LE)
SE_SH   EQU     3
PA_ON   EQU     0               ; path: on, style, its steps (address), n
PA_STY  EQU     1
PA_AT   EQU     2
PA_N    EQU     4
PA_SH   EQU     3
FL_ON   EQU     0               ; flow: on, kind, frames, hold, n, its cells
FL_KIND EQU     1               ;  (address), the frame it began (LE)
FL_FR   EQU     2
FL_HOLD EQU     3
FL_N    EQU     4
FL_AT   EQU     5
FL_T0   EQU     7
FL_SH   EQU     4
BE_ON   EQU     0               ; beam: on, x0 y0 x1 y1 (signed bytes), the
BE_X0   EQU     1               ;  frame it goes (LE)
BE_Y0   EQU     2
BE_X1   EQU     3
BE_Y1   EQU     4
BE_END  EQU     5
BE_SH   EQU     3

; RECOF: RB = record D of the table at base, 2^sh bytes each; D = its first
; byte (on). Keeps R8, R9, RC.
RECOF   MACRO   base, sh
        REPT    sh
        SHL
        ENDM
        ADI     LO(base)
        PLO     RB
        LDI     HI(base)
        ADCI    0
        PHI     RB
        LDN     RB
        ENDM

; ARGS / ARGSEND: R8 = the script's pointer (PPC); PPC = R8.
ARGS    MACRO
        GETV    PPC
        PLO     R8
        GETV    PPC+1
        PHI     R8
        ENDM
ARGSEND MACRO
        GLO     R8
        PUTV    PPC
        GHI     R8
        PUTV    PPC+1
        ENDM
; SKIPN n: the op's n argument bytes passed over.
SKIPN   MACRO   n
        GETV    PPC
        ADI     n
        STR     RA
        GETV    PPC+1
        ADCI    0
        STR     RA
        ENDM

; ---------------------------------------------------------------------------
; ply_reset -- a new shot: every record off, the frame 0, no head shown,
;   no stage drawn, the shell's images; the shells hidden.
; ---------------------------------------------------------------------------
        RFIT    225
ply_reset:
        POKEAT  PTREC                   ; every record off (they lie in order)
        LDR     R9, PRECEND-PTREC
prs1:   LDI     0
        PKD
        DEC     R9
        GLO     R9
        BNZ     prs1
        GHI     R9
        BNZ     prs1
        PKEND
        SETV    PNOW, 0
        SETV    PNOW+1, 0
        LDR     R8, PCT                 ; no records on
        LDI     6
        PLO     R9
prs1a:  LDI     0
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     prs1a
        LDR     R8, PHNX                ; heads not wanted, nor shown
        LDI     4*NTRK
        PLO     R9
prs2:   LDI     NONE
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     prs2
        LDR     R8, PBSTG               ; no stage drawn, no cell shown
        LDI     NBLAST+NFLOW
        PLO     R9
prs3:   LDI     0
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     prs3
        PARS                            ; the shell: a dot two lines tall in
        P       IM_SHELL                ;  a one-dot halo
        P       3
        P       4
        P       0
        P       40H
        P       40H
        P       0
        GOP     TOP_IMAGE
        PARS
        P       IM_SHELL+1
        P       3
        P       4
        P       0E0H
        P       0E0H
        P       0E0H
        P       0E0H
        GOP     TOP_IMAGE
        PARS                            ; the shells hidden (sprites 10-15)
        P       0
        P       0FCH
        GOP     TOP_SPRHIDE
        FRET

; ---------------------------------------------------------------------------
; ply_start -- the segment in SCRIPT starts (a new shot at segment 0): its
;   ops due now.
; ---------------------------------------------------------------------------
        RFIT    52
ply_start:
        GETV    SEGNO
        BNZ     pst1
        FCALL   ply_reset
pst1:   LDI     LO(SCRIPT)
        PUTV    PPC
        LDI     HI(SCRIPT)
        PUTV    PPC+1
        GETV    PNOW
        PUTV    PWAIT
        GETV    PNOW+1
        PUTV    PWAIT+1
        SETV    PLAYING, 1
        FRET

; ---------------------------------------------------------------------------
; ply_pass -- a pass of the segment: DT frames (PLY_CATCHUP at most); then
;   the picture, held. At OP_END: ply_end (PLAYING is 0).
; ---------------------------------------------------------------------------
        RFIT    114
ply_pass:
        GETV    DT
        SMI     PLY_CATCHUP+1
        BNF     pps1
        LDI     PLY_CATCHUP
        BR      pps2
pps1:   GETV    DT
pps2:   PUTV    PSTEPS
pps3:   FCALL   ply_frame
        GETV    PLAYING
        BZ      pps9
        GETV    PSTEPS
        SMI     1
        STR     RA
        BNZ     pps3
pps9:   HOLD1
        FCALL   ply_draw
        HOLD0
        GETV    HUDDRT                  ; a message set: drawn once a pass
        ANI     MSGDRT
        BZ      pps9b
        GETV    HUDDRT
        ANI     0FFH-MSGDRT
        STR     RA
        FCALL   msg_draw
pps9b:  GETV    HMSGT                   ; the HUD's message: its time
        BZ      pps9a
        FCALL   msg_tick
pps9a:  GETV    PLAYING
        BNZ     pps10
        FJMP    ply_end
pps10:  FRET

; ---------------------------------------------------------------------------
; ply_frame -- frame PNOW: everything moves a step (not the shot's first
;   frame: view.go), then the ops due run (to an OP_WAIT or OP_END); PNOW
;   on.
; ---------------------------------------------------------------------------
        RFIT    126
ply_frame:
        GETV    PNOW
        BNZ     pfr0
        GETV    PNOW+1
        BZ      pfr1
pfr0:   GETV    PSHAKE                  ; the shake's and flash's frames
        BZ      pfr0a
        SMI     1
        STR     RA
pfr0a:  GETV    PFLASH
        BZ      pfr0b
        SMI     1
        STR     RA
pfr0b:  GETV    PCT                     ; (a table with none on: skipped)
        BZ      pfr0c
        FCALL   pf_tracks
pfr0c:  GETV    PCB
        BZ      pfr0d
        FCALL   pf_blasts
pfr0d:  GETV    PCP
        BZ      pfr0e
        FCALL   pf_paths
pfr0e:  GETV    PCF
        BZ      pfr0f
        FCALL   pf_flows
pfr0f:  GETV    PCE
        BZ      pfr0g
        FCALL   pf_beams
pfr0g:  GETV    PCS
        BZ      pfr1
        FCALL   pf_settles
pfr1:   GETV    PWAIT                   ; the ops due: PNOW >= PWAIT
        STR     R2
        GETV    PNOW
        SM
        GETV    PWAIT+1
        STR     R2
        GETV    PNOW+1
        SMB
        BNF     pfr2
        FCALL   ply_ops
pfr2:   INCV    PNOW
        BNZ     pfr3
        INCV    PNOW+1
pfr3:   FRET

; ---------------------------------------------------------------------------
; ply_ops -- the ops at PPC, one after another, until an OP_WAIT or OP_END
;   (POPSTOP) or an op we don't know (the segment ends). Each op's handler
;   is ICALLed from OPTAB with PPC past the op's byte; it moves PPC past its
;   arguments.
; ---------------------------------------------------------------------------
        RFIT    71
ply_ops:
        SETV    POPSTOP, 0
pop1:   ARGS
        LDA     R8                      ; the op
        PLO     RC
        ARGSEND
        GLO     RC
        SMI     OP_FIRE+1
        BNF     pop2
        LDI     LO(op_end)              ; (unknown: the end)
        PLO     R9
        LDI     HI(op_end)
        PHI     R9
        BR      pop3
pop2:   GLO     RC
        SHL
        ADI     LO(OPTAB)
        PLO     R9
        LDI     HI(OPTAB)
        ADCI    0
        PHI     R9
        LDA     R9
        PHI     RC
        LDN     R9
        PLO     R9
        GHI     RC
        PHI     R9
pop3:   ICALL
        GETV    POPSTOP
        BZ      pop1
        FRET

; the ops' handlers, by op ($00-$3A): op_skip0 for none
        PFIT    2*(OP_FIRE+1)
OPTAB:  DW      op_end, op_wait, op_skip0, op_skip0, op_skip0, op_skip0, op_skip0, op_skip0
        DW      op_skip0, op_skip0, op_skip0, op_skip0, op_skip0, op_skip0, op_skip0, op_skip0
        DW      op_arc, op_path, op_stop, op_skip0, op_skip0, op_skip0, op_skip0, op_skip0
        DW      op_skip0, op_skip0, op_skip0, op_skip0, op_skip0, op_skip0, op_skip0, op_skip0
        DW      op_explode, op_dirt, op_tunnel, op_beam, op_flow, op_settle, op_skip0, op_skip0
        DW      op_skip0, op_skip0, op_skip0, op_skip0, op_skip0, op_skip0, op_skip0, op_skip0
        DW      op_talk, op_sound, op_shield, op_damage, op_fall, op_death, op_tank, op_wind
        DW      op_shake, op_flash, op_fire

; ---- the ops that end a frame's ops ------------------------------------------------
        RFIT    81
op_end: SETV    PLAYING, 0
        SETV    POPSTOP, 1
op_skip0:
        FRET
op_wait:
        ARGS                            ; n frames (0: 256): PWAIT = PNOW + n
        LDA     R8
        PLO     RC
        ARGSEND
        GLO     RC
        STR     R2
        GETV    PNOW
        ADD
        PUTV    PWAIT
        GETV    PNOW+1
        ADCI    0
        PUTV    PWAIT+1
        GLO     RC
        BNZ     opw1
        INCV    PWAIT+1                 ; (0: 256)
opw1:   SETV    POPSTOP, 1
        FRET

; ---- the ops a port shows its own way (M4, M5: the HUD, the sound) ------------------
        RFIT    139
op_talk:
        ARGS                            ; seat, len, text: msg.inc
        LDA     R8
        PUTV    T0
        LDA     R8
        PUTV    T1
        GLO     R8
        PUTV    T2
        GHI     R8
        PUTV    T3
        GETV    T1
        STR     R2
        GLO     R8
        ADD
        PLO     R8
        GHI     R8
        ADCI    0
        PHI     R8
        ARGSEND
        FJMP    msg_talk
op_sound:
        ARGS                            ; id, size: a boom's size its own
        LDA     R8
        PUTV    T0
        LDA     R8
        PUTV    T1
        ARGSEND
        GETV    T0
        XRI     SND_BOOM
        BNZ     osn1
        GETV    T1
        ANI     3
        ADI     SND_BOOM0
        BR      osn2
osn1:   GETV    T0
        SMI     16                      ; (past the SND_*: none)
        BDF     osn9
        GETV    T0
osn2:   FJMP    snd_play
osn9:   FRET
        RFIT    90
op_shield:
        ARGS                            ; seat, event, hp: msg.inc
        LDA     R8
        PUTV    T0
        LDA     R8
        PUTV    T1
        LDA     R8
        PUTV    T2
        ARGSEND
        FJMP    msg_shield
op_damage:
        ARGS                            ; seat, amount, energy left
        LDA     R8
        PUTV    T0
        INC     R8
        LDA     R8
        PUTV    T1
        ARGSEND
        FJMP    msg_hit
        RFIT    95
op_wind:
        SKIPN   2
        FRET
op_tunnel:
        SKIPN   5
        FRET
op_shake:
        ARGS                            ; frames: the picture shaken
        LDA     R8
        PUTV    PSHAKE
        ARGSEND
        FRET
op_flash:
        ARGS                            ; colour, frames: inverted
        INC     R8
        LDA     R8
        PUTV    PFLASH
        ARGSEND
        FRET

; ---- the tanks' ops -----------------------------------------------------------------
; op_tank: seat, x16, y, angle: placed. op_fall: seat, x16, y, frames, chute:
; there (at once, as view.go). op_death: seat, effect: gone. op_fire: seat,
; angle, power16, weapon: its barrel.
        RFIT    113
op_tank:
        ARGS
        LDA     R8                      ; seat
        PUTV    T4
        LDA     R8                      ; x
        PHI     R9
        INC     R8
        LDA     R8                      ; y
        PLO     RC
        LDA     R8                      ; angle: its step
        FCALL   ang_step
        PHI     RC
        ARGSEND
        LDI     1
        PLO     R9
        FJMP    opt_set
op_fall:
        ARGS
        LDA     R8                      ; seat
        PUTV    T4
        LDA     R8                      ; x
        PHI     R9
        INC     R8
        LDA     R8                      ; y
        PLO     RC
        INC     R8                      ; (frames, chute)
        INC     R8
        ARGSEND
        GETV    T4                      ; the step it has
        ADI     LO(TKST)
        PLO     RA
        LDN     RA
        PHI     RC
        LDI     1
        PLO     R9
opt_set:
        GETV    T4                      ; (a seat we have)
        SMI     SEATS
        BDF     opt9
        GETV    T4
        FJMP    tk_set
opt9:   FRET

        RFIT    170
op_death:
        ARGS
        LDA     R8
        PUTV    T4
        INC     R8
        ARGSEND
        GETV    T4
        SMI     SEATS
        BDF     opd9
        GETV    T4                      ; off: as it was, but off
        ADI     LO(TKX)
        PLO     RA
        LDN     RA
        PHI     R9
        GETV    T4
        ADI     LO(TKY)
        PLO     RA
        LDN     RA
        PLO     RC
        GETV    T4
        ADI     LO(TKST)
        PLO     RA
        LDN     RA
        PHI     RC
        LDI     0
        PLO     R9
        GETV    T4
        FJMP    tk_set
opd9:   FRET
op_fire:
        ARGS
        LDA     R8                      ; seat
        PUTV    T4
        LDA     R8                      ; angle
        PUTV    T5
        INC     R8                      ; (power16, weapon)
        INC     R8
        INC     R8
        ARGSEND
        GETV    T4
        SMI     SEATS
        BDF     opf9
        GETV    T4
        ADI     LO(TKON)
        PLO     RA
        LDN     RA
        PLO     R9
        GETV    T4
        ADI     LO(TKX)
        PLO     RA
        LDN     RA
        PHI     R9
        GETV    T4
        ADI     LO(TKY)
        PLO     RA
        LDN     RA
        PLO     RC
        GETV    T5
        FCALL   ang_step
        PHI     RC
        GETV    T4
        FJMP    tk_set
opf9:   FRET

; ---------------------------------------------------------------------------
; TRACKS. PTRK + 12 k: X3 Y3 VX3 VY3 (LE: 12.12 position, velocity);
;   PTREC + 8 k: on, style, AX, AY, end. PHNX/PHNY + k: the head as it is
;   now (NONE: off the picture, or none); PHX/PHY: as its sprite shows it.
; ---------------------------------------------------------------------------
; op_arc: slot, style, x16, y16, vx24, vy24, ax16, ay16, n16: a track
;   (re)starts; its head shown at its start.
        RFIT    228
op_arc: ARGS
        LDA     R8                      ; the slot (6 of them)
        ANI     15
        PUTV    T0
        SMI     NTRK
        BNF     oa1
        GETV    PPC                     ; (not ours: passed over)
        ADI     17
        STR     RA
        GETV    PPC+1
        ADCI    0
        STR     RA
        FRET
oa1:    LDA     R8                      ; style
        PUTV    T1
        GETV    T0                      ; R9: its ARC (PTRK + 12 k)
        SHL
        SHL
        STR     R2
        SHL
        ADD
        ADI     LO(PTRK)
        PLO     R9
        LDI     HI(PTRK)
        PHI     R9
        LDI     0                       ; X = x16 << 8, Y = y16 << 8
        STR     R9
        INC     R9
        LDA     R8
        STR     R9
        INC     R9
        LDA     R8
        STR     R9
        INC     R9
        LDI     0
        STR     R9
        INC     R9
        LDA     R8
        STR     R9
        INC     R9
        LDA     R8
        STR     R9
        INC     R9
        LDI     6                       ; VX, VY: as they come
        PLO     RC
oa2:    LDA     R8
        STR     R9
        INC     R9
        DEC     RC
        GLO     RC
        BNZ     oa2
        GLO     R8                      ; (AX, AY, n from here: RC)
        PLO     RC
        GHI     R8
        PHI     RC
        GLO     R8                      ; PPC past the arguments
        ADI     6
        PUTV    PPC
        GHI     R8
        ADCI    0
        PUTV    PPC+1
        GETV    T0                      ; its record, one stream: on, style,
        SHL                             ;  AX, AY, end (PNOW + n + 1)
        SHL
        SHL
        ADI     LO(PTREC)
        PLO     R9
        LDI     HI(PTREC)
        ADCI    0
        PHI     R9
        LDN     R9                      ; (off till now: one more on)
        BNZ     oa2a
        INCV    PCT
oa2a:   POKER   R9
        LDI     1
        PKD
        GETV    T1
        PKD
        LDA     RC
        PKD
        LDA     RC
        PKD
        LDA     RC
        PKD
        LDA     RC
        PKD
        LDA     RC                      ; n + 1 + PNOW
        PLO     R9
        LDN     RC
        PHI     R9
        INC     R9
        GETV    PNOW
        STR     R2
        GLO     R9
        ADD
        PKD
        GETV    PNOW+1
        STR     R2
        GHI     R9
        ADC
        PKD
        PKEND
        GETV    T0                      ; its head now
        FJMP    trk_head

; trk_head: track D's head (X >> 12, Y >> 12) into PHNX/PHNY (NONE: off
;   the picture: x -8..63, y -24..119, as signed bytes).
        RFIT    99
trk_head:
        PUTV    T2
        SHL                             ; R8: its X's high byte (PTRK + 12 k + 2)
        SHL
        STR     R2
        SHL
        ADD
        ADI     LO(PTRK+2)
        PLO     R8
        LDI     HI(PTRK)
        PHI     R8
        LDN     R8                      ; x: in a signed byte?
        ADI     8
        ANI     0F0H
        BNZ     thd8
        LDN     R8
        SHL
        SHL
        SHL
        SHL
        STR     R2
        DEC     R8
        LDN     R8
        SHR
        SHR
        SHR
        SHR
        OR
        PLO     R9
        ADI     8                       ; (-8..63)
        SMI     72
        BDF     thd8
        INC     R8                      ; y (Y's high byte: +3 from X's)
        INC     R8
        INC     R8
        INC     R8
        LDN     R8
        ADI     8
        ANI     0F0H
        BNZ     thd8
        LDN     R8
        SHL
        SHL
        SHL
        SHL
        STR     R2
        DEC     R8
        LDN     R8
        SHR
        SHR
        SHR
        SHR
        OR
        PHI     R9
        ADI     24                      ; (-24..119)
        SMI     144
        BDF     thd8
        BR      thd9
thd8:   LDI     NONE
        PLO     R9
thd9:   GETV    T2                      ; PHNX[k], PHNY[k]
        ADI     LO(PHNX)
        PLO     R8
        LDI     HI(PHNX)
        PHI     R8
        GLO     R9
        STR     R8
        GLO     R8
        ADI     NTRK
        PLO     R8
        GHI     R9
        STR     R8
        FRET

; op_stop: slot: the track ends now.
        RFIT    49
op_stop:
        ARGS
        LDA     R8
        ANI     15
        PUTV    T0
        ARGSEND
        GETV    T0
        SMI     NTRK
        BDF     ops9
        GETV    T0
        FJMP    trk_off
ops9:   FRET

; trk_off: track D off: its record off, its head none.
        RFIT    84
trk_off:
        PUTV    T2
        SHL
        SHL
        SHL
        ADI     LO(PTREC)
        PLO     R9
        LDI     HI(PTREC)
        ADCI    0
        PHI     R9
        LDN     R9                      ; (on till now: one fewer)
        BZ      tof1
        VAR     PCT
        LDN     RA
        SMI     1
        STR     RA
tof1:   POKER   R9
        LDI     0
        PKD
        PKEND
        GETV    T2
        ADI     LO(PHNX)
        PLO     R8
        LDI     HI(PHNX)
        PHI     R8
        LDI     NONE
        STR     R8
        FRET

; ---------------------------------------------------------------------------
; pf_tracks -- each track on a step: at its end it goes; else the ARC
;   steps and its head is where it is. A smoke trail lights the sky the
;   head goes over. T0 the track.
; ---------------------------------------------------------------------------
        RFIT    127
pf_tracks:
        SETV    T0, 0
pft1:   GETV    T0
        RECOF   PTREC, TR_SH            ; on?
        BZ      pft8
        GLO     RB                      ; its end: PNOW >= end, it goes
        ADI     TR_END
        PLO     RB
        GETV    PNOW
        STR     R2
        LDA     RB
        SD
        GETV    PNOW+1
        STR     R2
        LDN     RB
        SDB
        BNF     pft3                    ; (PNOW >= the end: it goes)
        GETV    T0
        FCALL   trk_off
        BR      pft8
pft3:   GETV    T0                      ; R8: its ARC, RB: its AX
        SHL
        SHL
        STR     R2
        SHL
        ADD
        ADI     LO(PTRK)
        PLO     R8
        LDI     HI(PTRK)
        PHI     R8
        GLO     RB
        SMI     TR_END+1-TR_AX
        PLO     RB
        LCALL   trk_step
        GETV    T0                      ; a smoke trail: its head now, the
        RECOF   PTREC+TR_STY, TR_SH     ;  sky under it lit (the others'
        ANI     15                      ;  heads: ply_draw's)
        XRI     ST_SMOKE
        BNZ     pft8
        GETV    T0
        FCALL   trk_head
        GETV    T0
        FCALL   trk_trail
pft8:   INCV    T0
        XRI     NTRK
        BNZ     pft1
        FRET

; trk_step -- LEAF: the ARC at R8 (X3 Y3 VX3 VY3, LE, console RAM) a step
;   (X += VX, Y += VY, VX += AX, VY += AY: 24-bit, A sign-extended); A from
;   RB (AX, AY: LE, the cart's RAM, read through R9). Clobbers R8, R9, D.
        RFIT    92
trk_step:
        GLO     R8                      ; X += VX, Y += VY: R(X) on VX
        ADI     6
        PLO     R9
        GHI     R8
        ADCI    0
        PHI     R9
        SEX     R9
        LDN     R8
        ADD
        STR     R8
        INC     R8
        INC     R9
        LDN     R8
        ADC
        STR     R8
        INC     R8
        INC     R9
        LDN     R8
        ADC
        STR     R8
        INC     R8
        INC     R9
        LDN     R8
        ADD
        STR     R8
        INC     R8
        INC     R9
        LDN     R8
        ADC
        STR     R8
        INC     R8
        INC     R9
        LDN     R8
        ADC
        STR     R8
        INC     R8                      ; R8: VX; R(X) on AX (the cart's)
        GLO     RB
        PLO     R9
        GHI     RB
        PHI     R9
        LDN     R8                      ; VX += AX, its sign carried up
        ADD
        STR     R8
        INC     R8
        INC     R9
        LDN     R8
        ADC
        STR     R8
        INC     R8
        LDN     R9                      ; (ANI keeps DF)
        ANI     80H
        BZ      tst1
        LDN     R8
        ADCI    0FFH
        BR      tst2
tst1:   LDN     R8
        ADCI    0
tst2:   STR     R8
        INC     R8
        INC     R9
        LDN     R8                      ; VY += AY
        ADD
        STR     R8
        INC     R8
        INC     R9
        LDN     R8
        ADC
        STR     R8
        INC     R8
        LDN     R9
        ANI     80H
        BZ      tst3
        LDN     R8
        ADCI    0FFH
        BR      tst4
tst3:   LDN     R8
        ADCI    0
tst4:   STR     R8
        SEX     R2
        SEP     R3

; trk_trail: track D's head lights the sky under it (a smoke trail stays
;   till the round's end): over sky in the model, a dot set.
        RFIT    64
trk_trail:
        ADI     LO(PHNX)
        PLO     R8
        LDI     HI(PHNX)
        PHI     R8
        LDN     R8                      ; x: on the field (0-63)?
        SMI     FW
        BDF     ttr9
        LDN     R8
        PLO     R9
        GLO     R8
        ADI     NTRK
        PLO     R8
        LDN     R8                      ; y: on the field?
        PHI     R9
        SMI     FH
        BDF     ttr9
        LCALL   fld_dirt
        BDF     ttr9
        PARS
        GLO     R9
        PD
        GHI     R9
        PD
        P       1
        P       1
        P       TM_SET
        GOP     TOP_RECT
ttr9:   FRET

; ---------------------------------------------------------------------------
; BLASTS (and dirt balls). PBREC + 16 k: on (BLK_*), x, y, rx, ry, kind,
;   total, the frame it began. PBSTG + k: the stage drawn (of 4).
; ---------------------------------------------------------------------------
; op_explode: x16, y16, rx, ry, kind, frames. op_dirt: x16, y16, rx, ry,
;   frames. Into a free record (none: dropped, as the Atari's).
        RFIT    89
op_explode:
        LDI     BLK_BLAST
        BR      oex1
op_dirt:
        LDI     BLK_DIRT
oex1:   PUTV    T1
        SETV    T0, 0                   ; a free record
oex2:   GETV    T0
        RECOF   PBREC, BL_SH
        BZ      oex3
        INCV    T0
        XRI     NBLAST
        BNZ     oex2
        GETV    T1                      ; (none: its arguments passed over)
        XRI     BLK_BLAST
        BZ      oex2a
        SKIPN   7
        FRET
oex2a:  SKIPN   8
        FRET
oex3:   FJMP    oex4

        RFIT    164
oex4:   ARGS
        POKER   RB                      ; its record, one stream
        GETV    T1
        PKD
        LDA     R8                      ; x (its byte: the field's)
        PKD
        INC     R8
        LDA     R8                      ; y
        PKD
        INC     R8
        LDA     R8                      ; rx, ry
        PKD
        LDA     R8
        PKD
        GETV    T1                      ; kind (a dirt ball: none)
        XRI     BLK_BLAST
        BZ      oex5
        LDI     0
        BR      oex6
oex5:   LDA     R8
oex6:   PKD
        LDA     R8                      ; total: at least 1
        BNZ     oex7
        LDI     1
oex7:   PUTV    T2
        PKD
        GETV    PNOW                    ; it began now, ends at now + total
        PKD
        GETV    PNOW+1
        PKD
        GETV    T2
        STR     R2
        GETV    PNOW
        ADD
        PKD
        GETV    PNOW+1
        ADCI    0
        PKD
        PKEND
        INCV    PCB
        ARGSEND
        GETV    T0                      ; nothing drawn yet
        ADI     LO(PBSTG)
        PLO     R8
        LDI     HI(PBSTG)
        PHI     R8
        LDI     0
        STR     R8
        FRET

; pf_blasts: each one's last frame: a crater cut (a plasma or puff gives its
;   columns back to the model), off. (Its growing is ply_draw's.)
        RFIT    188
pf_blasts:
        SETV    T0, 0
pfb1:   GETV    T0
        RECOF   PBREC, BL_SH
        BZ      pfb8
        GLO     RB                      ; PNOW >= its end: the end
        ADI     BL_END
        PLO     RB
        GETV    PNOW
        STR     R2
        LDA     RB
        SD
        GETV    PNOW+1
        STR     R2
        LDN     RB
        SDB
        BNF     pfb8
        GETV    T0                      ; the end: its ellipse whole
        RECOF   PBREC, BL_SH
        XRI     BLK_BLAST
        BNZ     pfb7                    ; (a dirt ball: its settle follows)
        FCALL   bl_ell
        GETV    T0
        RECOF   PBREC+BL_KIND, BL_SH
        XRI     EX_PLASMA
        BZ      pfb5
        LDN     RB
        XRI     EX_PUFF
        BZ      pfb5
        SETV    EMODE, TM_CLEAR         ; a crater: cut
        FCALL   ell_draw
        BR      pfb7
pfb5:   FCALL   ell_dirty               ; a flash: the columns given back
pfb7:   GETV    T0                      ; off
        RECOF   PBREC, BL_SH
        POKER   RB
        LDI     0
        PKD
        PKEND
        VAR     PCB
        LDN     RA
        SMI     1
        STR     RA
pfb8:   INCV    T0
        XRI     NBLAST
        BNZ     pfb1
        FRET

; bl_n: blast T0's frames so far (PNOW - its start, 255 at most) in D; T2
;   its total.
        RFIT    46
bl_n:   GETV    T0
        RECOF   PBREC+BL_TOT, BL_SH
        PUTV    T2
        INC     RB
        GETV    PNOW
        STR     R2
        LDA     RB
        SD
        PLO     R9
        GETV    PNOW+1
        STR     R2
        LDN     RB
        SDB
        BZ      bln1
        LDI     0FFH
        FRET
bln1:   GLO     R9
        FRET

; bl_ell: blast T0's whole ellipse into EX, EY, ERX, ERY.
        RFIT    46
bl_ell: GETV    T0
        RECOF   PBREC+BL_X, BL_SH
        PUTV    EX
        INC     RB
        LDA     RB
        PUTV    EY
        LDA     RB
        PUTV    ERX
        LDN     RB
        PUTV    ERY
        FRET

; ell_dirty: the columns EX - ERX .. EX + ERX (on the field) dirty: drawn
;   again from the model.
        RFIT    36
ell_dirty:
        GETV    ERX                     ; R9.0 x, R9.1 how many
        STR     R2
        GETV    EX
        SM
        PLO     R9
        GETV    ERX
        SHL
        ADI     1
        PHI     R9
eld1:   GLO     R9
        SMI     FW
        BDF     eld2
        GLO     R9
        FCALL   fld_mark
eld2:   INC     R9
        GHI     R9
        SMI     1
        PHI     R9
        BNZ     eld1
        FRET

; ---------------------------------------------------------------------------
; ell_draw -- the ellipse EX, EY (signed bytes), ERX, ERY drawn in mode
;   EMODE as vertical runs, one RECT a column: column dx's half height is
;   ERY QT[32 dx / ERX] / 32 (the client's own drawing: a dot off at
;   most); never past the field (lines 0 to FH - 1). EDX, EACC.
; ---------------------------------------------------------------------------
        RFIT    140
ell_draw:
        GETV    ERX                     ; rx: 1-63
        BNZ     eld0
        SETV    ERX, 1
eld0:   GETV    ERX
        SMI     64
        BNF     edw0
        SETV    ERX, 63
edw0:   GETV    ERX                     ; the step: 8192 / rx (8.8 of 32nds)
        SHL
        ADI     LO(RC8)
        PLO     R8
        LDI     HI(RC8)
        ADCI    0
        PHI     R8
        LDA     R8
        PUTV    ESTEP
        LDN     R8
        PUTV    ESTEP+1
        SETV    EDX, 0
        SETV    EACC, 0
        SETV    EACC+1, 0
edw1:   GETV    EACC+1                  ; i = acc >> 8 (32 at most)
        SMI     33
        BNF     edw2
        LDI     32
        BR      edw3
edw2:   GETV    EACC+1
edw3:   ADI     LO(QT)
        PLO     R8
        LDI     HI(QT)
        ADCI    0
        PHI     R8
        LDN     R8                      ; h = ry QT[i] / 32
        PLO     R9                      ;  (R9.0 q, RC the product, R8 ry)
        GETV    ERY
        PLO     R8
        LDI     0
        PHI     R8
        PLO     RC
        PHI     RC
        LDI     6
        PHI     R9
edw4:   GLO     R9
        SHR
        PLO     R9
        BNF     edw5
        GLO     R8
        STR     R2
        GLO     RC
        ADD
        PLO     RC
        GHI     R8
        STR     R2
        GHI     RC
        ADC
        PHI     RC
edw5:   GLO     R8
        SHL
        PLO     R8
        GHI     R8
        SHLC
        PHI     R8
        GHI     R9
        SMI     1
        PHI     R9
        BNZ     edw4
        FJMP    edw6

        RFIT    87
edw6:   GHI     RC                      ; / 32: (hi << 3) | (lo >> 5)
        SHL
        SHL
        SHL
        STR     R2
        GLO     RC
        SHR
        SHR
        SHR
        SHR
        SHR
        OR
        PUTV    EH
        GETV    EDX                     ; the column EX + dx
        STR     R2
        GETV    EX
        ADD
        FCALL   ell_col
        GETV    EDX                     ; and EX - dx (dx > 0)
        BZ      edw7
        STR     R2
        GETV    EX
        SM
        FCALL   ell_col
edw7:   GETV    ESTEP                   ; on: acc += step, dx up to rx
        STR     R2
        GETV    EACC
        ADD
        STR     RA
        GETV    ESTEP+1
        STR     R2
        GETV    EACC+1
        ADC
        STR     RA
        INCV    EDX
        STR     R2
        GETV    ERX
        SM
        BNF     edw9
        FJMP    edw1
edw9:   FRET

; ell_col: column D (0-63, else nothing) from EY - EH to EY + EH, cut to
;   the field, in mode EMODE.
        RFIT    114
ell_col:
        PLO     R9                      ; (x)
        SMI     FW
        BDF     ecl9
        GETV    EY                      ; top = EY - EH (16-bit: EY signed)
        PLO     R8
        ANI     80H
        BZ      ecl1
        LDI     0FFH
ecl1:   PHI     R8                      ; R8 = EY
        GETV    EH
        STR     R2
        GLO     R8
        SM
        PLO     RC
        GHI     R8
        SMBI    0
        PHI     RC                      ; RC = top
        ANI     80H                     ; (above the field: from 0)
        BZ      ecl2
        LDI     0
        PLO     RC
        PHI     RC
ecl2:   GETV    EH                      ; R8 = EY + EH, the bottom
        STR     R2
        GLO     R8
        ADD
        PLO     R8
        GHI     R8
        ADCI    0
        PHI     R8
        ANI     80H                     ; (all above the field: nothing)
        BNZ     ecl9
        GHI     R8                      ; (past the field: FH - 1)
        BNZ     ecl3
        GLO     R8
        SMI     FH
        BNF     ecl4
ecl3:   LDI     FH-1
        PLO     R8
ecl4:   GHI     RC                      ; top past the field: nothing
        BNZ     ecl9
        GLO     RC
        SMI     FH
        BDF     ecl9
        GLO     RC                      ; lines: bottom - top + 1
        STR     R2
        GLO     R8
        SM
        BNF     ecl9
        ADI     1
        PHI     R8
        PARS
        GLO     R9
        PD
        GLO     RC
        PD
        P       1
        GHI     R8
        PD
        GETV    EMODE
        PD
        GOP     TOP_RECT
ecl9:   FRET

        PFIT    33
QT:     DB      32, 32, 32, 32, 32, 32, 31, 31, 31, 31, 30, 30, 30, 29, 29, 28
        DB      28, 27, 26, 26, 25, 24, 23, 22, 21, 20, 19, 17, 15, 14, 11, 8, 0
        PFIT    128
RC8:    DB      0, 0, 0, 32, 0, 16, 171, 10, 0, 8, 102, 6, 85, 5, 146, 4
        DB      0, 4, 142, 3, 51, 3, 233, 2, 171, 2, 118, 2, 73, 2, 34, 2
        DB      0, 2, 226, 1, 199, 1, 175, 1, 154, 1, 134, 1, 116, 1, 100, 1
        DB      85, 1, 72, 1, 59, 1, 47, 1, 37, 1, 26, 1, 17, 1, 8, 1
        DB      0, 1, 248, 0, 241, 0, 234, 0, 228, 0, 221, 0, 216, 0, 210, 0
        DB      205, 0, 200, 0, 195, 0, 191, 0, 186, 0, 182, 0, 178, 0, 174, 0
        DB      171, 0, 167, 0, 164, 0, 161, 0, 158, 0, 155, 0, 152, 0, 149, 0
        DB      146, 0, 144, 0, 141, 0, 139, 0, 137, 0, 134, 0, 132, 0, 130, 0

; ---------------------------------------------------------------------------
; SETTLES. op_settle: x16, n, frames, n column entries: frames 0 written at
;   once (the model, the columns dirty); else kept (PSREC + 8 k: on, x, n,
;   the entries' address, the frame it lands) and written then, or at the
;   segment's end. (No room to keep it: written now.)
; ---------------------------------------------------------------------------
        RFIT    248
op_settle:
        ARGS
        LDA     R8                      ; x (the field's: 0-63)
        PUTV    T1
        INC     R8
        LDA     R8                      ; n
        PUTV    T2
        LDA     R8                      ; frames
        PUTV    T3
        GLO     R8                      ; T4:T5 the entries
        PUTV    T5
        GHI     R8
        PUTV    T4
        GETV    T2                      ; PPC past them (1 or 1 + 2n each)
        PLO     R9
ost1:   GLO     R9
        BZ      ost3
        DEC     R9
        LDA     R8
        SMI     E_NONE+1
        BNF     ost1
        ADI     1
        SHL
        STR     R2
        GLO     R8
        ADD
        PLO     R8
        GHI     R8
        ADCI    0
        PHI     R8
        BR      ost1
ost3:   ARGSEND
        GETV    T3                      ; frames 0: now
        BZ      ost8
        SETV    T0, 0                   ; else a free record
ost4:   GETV    T0
        RECOF   PSREC, SE_SH
        BZ      ost5
        INCV    T0
        XRI     NSETTLE
        BNZ     ost4
        BR      ost8                    ; (none free: now)
ost5:   POKER   RB
        LDI     1
        PKD
        GETV    T1
        PKD
        GETV    T2
        PKD
        GETV    T4
        PKD
        GETV    T5
        PKD
        GETV    T3                      ; it lands at PNOW + frames
        STR     R2
        GETV    PNOW
        ADD
        PKD
        GETV    PNOW+1
        ADCI    0
        PKD
        PKEND
        INCV    PCS
        FRET
ost8:   GETV    T4                      ; written: the model, dirty
        PHI     R8
        GETV    T5
        PLO     R8
        GETV    T1
        PLO     R9
        GETV    T2
        PHI     R9
        FJMP    se_put

; se_put: R9.1 entries from R8 into the model from column R9.0, cut to the
;   field (x 0-63, x + n <= 64: the rest passed over).
        RFIT    21
se_put: GLO     R9
        SMI     FW
        BDF     sep9
        GHI     R9                      ; x + n past the field: n cut
        STR     R2
        GLO     R9
        ADD
        SDI     FW
        BDF     sep1
        GLO     R9
        SDI     FW
        PHI     R9
sep1:   FJMP    fld_puts
sep9:   FRET

; pf_settles: each one whose frame has come lands. T0 the settle.
        RFIT    63
pf_settles:
        SETV    T0, 0
pfs1:   GETV    T0
        RECOF   PSREC, SE_SH
        BZ      pfs8
        GLO     RB                      ; PNOW >= its landing?
        ADI     SE_LAND
        PLO     RB
        GETV    PNOW
        STR     R2
        LDA     RB
        SD
        GETV    PNOW+1
        STR     R2
        LDN     RB
        SDB
        BNF     pfs8
        GETV    T0
        FCALL   se_land
pfs8:   INCV    T0
        XRI     NSETTLE
        BNZ     pfs1
        FRET

; se_land: settle D lands (the model, dirty), off.
        RFIT    80
se_land:
        PUTV    T6
        RECOF   PSREC, SE_SH
        POKER   RB                      ; off (the record read after: kept)
        LDI     0
        PKD
        PKEND
        VAR     PCS
        LDN     RA
        SMI     1
        STR     RA
        INC     RB
        LDA     RB                      ; x
        PLO     R9
        LDA     RB                      ; n
        PHI     R9
        LDA     RB                      ; the entries
        PHI     R8
        LDN     RB
        PLO     R8
        FJMP    se_put

; ---------------------------------------------------------------------------
; PATHS. op_path: slot (0-3), style, x16, y16, n, steps: a head a dot a
;   frame (PPREC + 8 k: on, style, the steps' address, n; PAX, PAY, PAI the
;   step it is at, PAD drawn). A roller's head shows over sky only and is
;   put back as it goes; a digger's over anything, and leaves air.
; ---------------------------------------------------------------------------
        RFIT    211
op_path:
        ARGS
        LDA     R8
        ANI     3
        PUTV    T0
        FCALL   pa_end                  ; (a live one ends first)
        ARGS
        INC     R8
        LDA     R8                      ; style
        PUTV    T1
        LDA     R8                      ; x
        PUTV    T2
        INC     R8
        LDA     R8                      ; y
        PUTV    T3
        INC     R8
        LDA     R8                      ; n
        PUTV    T4
        INCV    PCP
        GETV    T0                      ; its record: on, style, the steps, n
        RECOF   PPREC, PA_SH
        POKER   RB
        LDI     1
        PKD
        GETV    T1
        PKD
        GHI     R8
        PKD
        GLO     R8
        PKD
        GETV    T4
        PKD
        PKEND
        GETV    T4                      ; PPC past the steps
        STR     R2
        GLO     R8
        ADD
        PLO     R8
        GHI     R8
        ADCI    0
        PHI     R8
        ARGSEND
        GETV    T0                      ; PAX, PAY; step 0
        ADI     LO(PAX)
        PLO     R8
        LDI     HI(PAX)
        PHI     R8
        GETV    T2
        STR     R8
        GLO     R8
        ADI     NPATH
        PLO     R8
        GETV    T3
        STR     R8
        GLO     R8
        ADI     NPATH
        PLO     R8
        LDI     0
        STR     R8
        GETV    T0
        FJMP    pa_head

; pa_xy: path D's R8 on its PAX (PAY at +NPATH, PAI +2 NPATH, PAD +3);
;   R9 = its x (.0), y (.1). T7 the path.
        RFIT    25
pa_xy:  PUTV    T7
        ADI     LO(PAX)
        PLO     R8
        LDI     HI(PAX)
        PHI     R8
        LDN     R8
        PLO     R9
        GLO     R8
        ADI     NPATH
        PLO     R8
        LDN     R8
        PHI     R9
        GLO     R8
        SMI     NPATH
        PLO     R8
        FRET

; pa_head: path D's head drawn where it is: a roller over sky only, a
;   digger anywhere (in dirt: a hole); PAD whether it was.
        RFIT    87
pa_head:
        FCALL   pa_xy
        GETV    T7
        RECOF   PPREC+PA_STY, PA_SH
        PUTV    T6
        GLO     R9                      ; on the field?
        SMI     FW
        BDF     pah8
        GHI     R9
        SMI     FH
        BDF     pah8
        LCALL   fld_dirt                ; (keeps R9)
        BDF     pah2
        LDI     TM_SET                  ; over sky: set
        BR      pah4
pah2:   GETV    T6                      ; in dirt: a digger's hole
        XRI     ST_DIGGER
        BNZ     pah8
        LDI     TM_CLEAR
pah4:   PUTV    EMODE
        FCALL   dot_r9
        LDI     1
        BR      pah9
pah8:   LDI     0
pah9:   PHI     RD
        GETV    T7                      ; PAD = drawn
        ADI     LO(PAD)
        PLO     R8
        LDI     HI(PAD)
        PHI     R8
        GHI     RD
        STR     R8
        FRET

; dot_r9: a dot at (R9.0, R9.1) in mode EMODE. Keeps R9.
        RFIT    34
dot_r9: PARS
        GLO     R9
        PD
        GHI     R9
        PD
        P       1
        P       1
        GETV    EMODE
        PD
        GOP     TOP_RECT
        FRET

; pa_leave: path T7's head goes from where it is: a digger leaves air
;   (cleared); a roller's dot put back as the model has it (sky: cleared).
        RFIT    59
pa_leave:
        GETV    T7
        ADI     LO(PAD)
        PLO     R8
        LDI     HI(PAD)
        PHI     R8
        LDN     R8
        BZ      pal9
        LDI     0
        STR     R8
        GETV    T7
        FCALL   pa_xy
        GETV    T6
        XRI     ST_DIGGER
        BZ      pal2
        LCALL   fld_dirt                ; a roller: the model's dot
        BDF     pal3
pal2:   SETV    EMODE, TM_CLEAR
        FJMP    dot_r9
pal3:   SETV    EMODE, TM_SET
        FJMP    dot_r9
pal9:   FRET

; pa_end: path T0 ends (its head gone), off.
        RFIT    120
pa_end: GETV    T0
        RECOF   PPREC, PA_SH
        BZ      pae9
        GETV    T0
        RECOF   PPREC+PA_STY, PA_SH
        PUTV    T6
        GETV    T0
        PUTV    T7
        FCALL   pa_leave
        GETV    T0
        RECOF   PPREC, PA_SH
        POKER   RB
        LDI     0
        PKD
        PKEND
        VAR     PCP
        LDN     RA
        SMI     1
        STR     RA
pae9:   FRET

; pf_paths: each path on a step: its head leaves, moves by its next step
;   (dx << 4 | dy, signed nibbles), shows; no steps left: it ends.
        RFIT    205
pf_paths:
        SETV    T0, 0
pfp1:   GETV    T0
        RECOF   PPREC, PA_SH
        BZ      pfp8
        GETV    T0                      ; its step (PAI) against n
        ADI     LO(PAI)
        PLO     R8
        LDI     HI(PAI)
        PHI     R8
        LDN     R8
        PLO     RC
        STR     R2
        GETV    T0
        RECOF   PPREC+PA_N, PA_SH
        SM
        BNZ     pfp2
        FCALL   pa_end                  ; (none left: it ends)
        BR      pfp8
pfp2:   GETV    T0
        RECOF   PPREC+PA_STY, PA_SH
        PUTV    T6
        INC     RB                      ; the step: steps[PAI]
        LDA     RB
        PHI     R9
        LDN     RB
        PLO     R9
        GLO     RC
        STR     R2
        GLO     R9
        ADD
        PLO     R9
        GHI     R9
        ADCI    0
        PHI     R9
        LDN     R9
        PUTV    T5
        GETV    T0                      ; PAI on
        ADI     LO(PAI)
        PLO     R8
        LDI     HI(PAI)
        PHI     R8
        LDN     R8
        ADI     1
        STR     R8
        GETV    T0
        PUTV    T7
        FCALL   pa_leave
        GETV    T0                      ; x += dx, y += dy
        FCALL   pa_xy
        GETV    T5
        SHR
        SHR
        SHR
        SHR
        ANI     0FH
        SMI     8                       ; (signed: -8..7)
        BDF     pfp3
        ADI     8
        BR      pfp4
pfp3:   ADI     0F8H
pfp4:   STR     R2
        LDN     R8
        ADD
        STR     R8
        GETV    T5
        ANI     0FH
        SMI     8
        BDF     pfp5
        ADI     8
        BR      pfp6
pfp5:   ADI     0F8H
pfp6:   STR     R2
        GLO     R8
        ADI     NPATH
        PLO     R8
        LDN     R8
        ADD
        STR     R8
        GETV    T0
        FCALL   pa_head
pfp8:   INCV    T0
        XRI     NPATH
        BNZ     pfp1
        FRET

; ---------------------------------------------------------------------------
; FLOWS. op_flow: kind, frames, hold, n, cells (x16, y8) x n: the cells
;   appear evenly over frames (fire over sky, mud as dirt); hold frames
;   after, fire goes back to sky. PFREC + 16 k: on, kind, frames, hold, n,
;   the cells' address, the frame it began; PFSHN + k: cells shown.
; ---------------------------------------------------------------------------
        RFIT    203
op_flow:
        SETV    T0, 0                   ; a free record
ofl1:   GETV    T0
        RECOF   PFREC, FL_SH
        BZ      ofl2
        INCV    T0
        XRI     NFLOW
        BNZ     ofl1
        FJMP    ofl_skip
ofl2:   INCV    PCF
        ARGS
        POKER   RB                      ; on, kind, frames (1 at least), hold, n
        LDI     1
        PKD
        LDA     R8
        PKD
        LDA     R8
        BNZ     ofl3
        LDI     1
ofl3:   PKD
        LDA     R8
        PKD
        LDA     R8
        PUTV    T1
        PKD
        GHI     R8                      ; the cells' address
        PKD
        GLO     R8
        PKD
        GETV    PNOW                    ; it began now
        PKD
        GETV    PNOW+1
        PKD
        PKEND
        GETV    T1                      ; PPC past the cells (3 n, 16-bit)
        PLO     R9
        LDI     0
        PHI     R9
        GLO     R9
        SHL
        PLO     R9
        GHI     R9
        SHLC
        PHI     R9
        GETV    T1
        STR     R2
        GLO     R9
        ADD
        PLO     R9
        GHI     R9
        ADCI    0
        PHI     R9
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
        ARGSEND
        GETV    T0                      ; none shown yet
        ADI     LO(PFSHN)
        PLO     R8
        LDI     HI(PFSHN)
        PHI     R8
        LDI     0
        STR     R8
        FRET

; ofl_skip: an op_flow with no free record: passed over (4 + 3 n bytes).
        RFIT    63
ofl_skip:
        ARGS                            ; (none: passed over: 4 + 3 n)
        INC     R8
        INC     R8
        INC     R8
        LDA     R8
        PUTV    T1
        PLO     R9
        LDI     0
        PHI     R9
        GLO     R9
        SHL
        PLO     R9
        GHI     R9
        SHLC
        PHI     R9
        GETV    T1
        STR     R2
        GLO     R9
        ADD
        STR     R2
        GLO     R8
        ADD
        PLO     R8
        GHI     R9
        ADCI    0
        STR     R2
        GHI     R8
        ADD
        PHI     R8
        ARGSEND
        FRET

; pf_flows: each flow on: f = frames since it began; up to its frames,
;   n f / frames cells shown; at frames + hold, all shown and fire out.
        RFIT    153
pf_flows:
        SETV    T0, 0
pff1:   GETV    T0
        RECOF   PFREC, FL_SH
        BZ      pff8
        GLO     RB                      ; f = PNOW - its start (255 at most)
        ADI     FL_T0
        PLO     RB
        GETV    PNOW
        STR     R2
        LDA     RB
        SD
        PLO     R9
        GETV    PNOW+1
        STR     R2
        LDN     RB
        SDB
        BZ      pff2
        LDI     0FFH
        PLO     R9
pff2:   GLO     R9
        PUTV    T1                      ; T1 f
        GETV    T0
        RECOF   PFREC+FL_FR, FL_SH
        PUTV    T2                      ; T2 frames
        INC     RB
        LDA     RB
        PUTV    T3                      ; T3 hold
        LDN     RB
        PUTV    T4                      ; T4 n
        GETV    T2                      ; f >= frames + hold: the end
        STR     R2
        GETV    T3
        ADD
        BDF     pff3                    ; (past 255: not yet)
        STR     R2
        GETV    T1
        SM
        BNF     pff3
        GETV    T0
        FCALL   fl_end
        BR      pff8
pff3:   GETV    T2                      ; f <= frames: n f / frames shown
        STR     R2
        GETV    T1
        SD
        BNF     pff8
        FCALL   fl_target               ; D = n f / frames
        FCALL   fl_show
pff8:   INCV    T0
        XRI     NFLOW
        BNZ     pff1
        FRET

; fl_target: D = T4 T1 / T2 (cells n, f, frames: n f < 65536): the
;   product, then 16 shifts and trial subtractions.
        RFIT    79
fl_target:
        GETV    T4                      ; RC = n f
        PLO     R8
        LDI     0
        PHI     R8
        PLO     RC
        PHI     RC
        GETV    T1
        PLO     R9
        LDI     8
        PHI     R9
flt1:   GLO     R9
        SHR
        PLO     R9
        BNF     flt2
        GLO     R8
        STR     R2
        GLO     RC
        ADD
        PLO     RC
        GHI     R8
        STR     R2
        GHI     RC
        ADC
        PHI     RC
flt2:   GLO     R8
        SHL
        PLO     R8
        GHI     R8
        SHLC
        PHI     R8
        GHI     R9
        SMI     1
        PHI     R9
        BNZ     flt1
        LDI     0                       ; RC / frames: R8 the remainder
        PLO     R8
        LDI     16
        PHI     R9
flt3:   GLO     RC                      ; (RC << 1, its top into R8)
        SHL
        PLO     RC
        GHI     RC
        SHLC
        PHI     RC
        GLO     R8
        SHLC
        PLO     R8
        GETV    T2                      ; R8 >= frames: take it, the bit
        STR     R2
        GLO     R8
        SM
        BNF     flt4
        PLO     R8
        INC     RC
flt4:   GHI     R9
        SMI     1
        PHI     R9
        BNZ     flt3
        GLO     RC                      ; (n at most: 255)
        FRET

; fl_show: flow T0's cells shown up to D (fire over sky; mud as dirt).
        RFIT    110
fl_show:
        PUTV    T5
        GETV    T0
        ADI     LO(PFSHN)
        PLO     R8
        LDI     HI(PFSHN)
        PHI     R8
        LDN     R8
        PUTV    T6                      ; T6 shown
        GETV    T5                      ; target
        STR     R2
        GLO     R8                      ; (PFSHN = target, now)
        PLO     RC
        GHI     R8
        PHI     RC
        LDN     R2
        STR     RC
fls1:   GETV    T6                      ; each cell from shown to target
        STR     R2
        GETV    T5
        SM
        BZ      fls9
        BNF     fls9
        FCALL   fl_cell                 ; R9 = cell T6
        INCV    T6
        GLO     R9                      ; on the field?
        SMI     FW
        BDF     fls1
        GHI     R9
        SMI     FH
        BDF     fls1
        GETV    T0
        RECOF   PFREC+FL_KIND, FL_SH
        XRI     FLOW_MUD
        BZ      fls3
        LCALL   fld_dirt                ; fire: over sky only
        BDF     fls1
fls3:   SETV    EMODE, TM_SET
        FCALL   dot_r9
        BR      fls1
fls9:   FRET

; fl_cell: R9 = flow T0's cell T6 (x .0, y .1: the bytes).
        RFIT    63
fl_cell:
        GETV    T0
        RECOF   PFREC+FL_AT, FL_SH
        PHI     R8
        INC     RB
        LDN     RB
        PLO     R8
        GETV    T6                      ; + 3 T6 (16-bit)
        PLO     R9
        LDI     0
        PHI     R9
        GLO     R9
        SHL
        PLO     R9
        GHI     R9
        SHLC
        PHI     R9
        GETV    T6
        STR     R2
        GLO     R9
        ADD
        PLO     R9
        GHI     R9
        ADCI    0
        PHI     R9
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
        LDA     R8                      ; x16's low byte, y8
        PLO     R9
        INC     R8
        LDN     R8
        PHI     R9
        FRET

; fl_end: flow D all shown, then (fire) back to sky where the model is,
;   off.
        RFIT    182
fl_end: PUTV    T0
        RECOF   PFREC+FL_N, FL_SH
        FCALL   fl_show
        GETV    T0
        RECOF   PFREC+FL_KIND, FL_SH
        XRI     FLOW_MUD
        BZ      fle8
        SETV    T6, 0                   ; fire: each cell out
fle1:   GETV    T0
        RECOF   PFREC+FL_N, FL_SH
        STR     R2
        GETV    T6
        SM
        BDF     fle8
        FCALL   fl_cell
        INCV    T6
        GLO     R9
        SMI     FW
        BDF     fle1
        GHI     R9
        SMI     FH
        BDF     fle1
        LCALL   fld_dirt
        BDF     fle1
        SETV    EMODE, TM_CLEAR
        FCALL   dot_r9
        BR      fle1
fle8:   GETV    T0
        RECOF   PFREC, FL_SH
        POKER   RB
        LDI     0
        PKD
        PKEND
        VAR     PCF
        LDN     RA
        SMI     1
        STR     RA
        FRET

; ---------------------------------------------------------------------------
; BEAMS. op_beam: x0 16, y0 16, x1 16, y1 16, kind, frames: a line lit
;   (XOR: seen on sky and dirt) for frames; then cleared (it burned what
;   it crossed: the settle after it is the truth). PEREC + 8 k.
; ---------------------------------------------------------------------------
        RFIT    175
op_beam:
        SETV    T0, 0
obm1:   GETV    T0
        RECOF   PEREC, BE_SH
        BZ      obm2
        INCV    T0
        XRI     NBEAM
        BNZ     obm1
        SKIPN   10                      ; (none free: passed over)
        FRET
obm2:   INCV    PCE
        ARGS
        POKER   RB                      ; on, x0, y0, x1, y1, its end
        LDI     1
        PKD
        LDA     R8
        PKD
        INC     R8
        LDA     R8
        PKD
        INC     R8
        LDA     R8
        PKD
        INC     R8
        LDA     R8
        PKD
        INC     R8
        INC     R8                      ; (kind)
        LDA     R8                      ; frames: 1 at least
        BNZ     obm3
        LDI     1
obm3:   STR     R2
        GETV    PNOW
        ADD
        PKD
        GETV    PNOW+1
        ADCI    0
        PKD
        PKEND
        ARGSEND
        SETV    EMODE, TM_XOR
        GETV    T0
        FJMP    be_line

; pf_beams: each beam whose frame has come: its line cleared, off.
        RFIT    135
pf_beams:
        SETV    T0, 0
pfm1:   GETV    T0
        RECOF   PEREC, BE_SH
        BZ      pfm8
        GLO     RB
        ADI     BE_END
        PLO     RB
        GETV    PNOW
        STR     R2
        LDA     RB
        SD
        GETV    PNOW+1
        STR     R2
        LDN     RB
        SDB
        BNF     pfm8
        SETV    EMODE, TM_CLEAR
        GETV    T0
        FCALL   be_line
        GETV    T0
        RECOF   PEREC, BE_SH
        POKER   RB
        LDI     0
        PKD
        PKEND
        VAR     PCE
        LDN     RA
        SMI     1
        STR     RA
pfm8:   INCV    T0
        XRI     NBEAM
        BNZ     pfm1
        FRET

; be_line: beam D's line (Bresenham, view.go's), a dot at a time in mode
;   EMODE (on the field only). R9 the dot; BLX1/BLY1 the end, BLSX/BLSY the
;   steps, BLDX/BLDY the sizes, BLERR (16-bit) the error.
        RFIT    145
be_line:
        RECOF   PEREC+BE_X0, BE_SH
        PLO     R9
        INC     RB
        LDA     RB
        PHI     R9
        LDA     RB
        PUTV    BLX1
        LDN     RB
        PUTV    BLY1
        LDI     1                       ; dx = |x1 - x0|, sx
        PUTV    BLSX
        GLO     R9
        STR     R2
        GETV    BLX1
        SM
        PUTV    BLDX
        ANI     80H
        BZ      bel1
        GETV    BLDX
        SDI     0
        PUTV    BLDX
        SETV    BLSX, 0FFH
bel1:   LDI     1                       ; dy = |y1 - y0|, sy
        PUTV    BLSY
        GHI     R9
        STR     R2
        GETV    BLY1
        SM
        PUTV    BLDY
        ANI     80H
        BZ      bel2
        GETV    BLDY
        SDI     0
        PUTV    BLDY
        SETV    BLSY, 0FFH
bel2:   GETV    BLDY                    ; err = dx - dy (16-bit)
        STR     R2
        GETV    BLDX
        SM
        PUTV    BLERR
        LDI     0
        SMBI    0
        PUTV    BLERR+1
        FJMP    bel3

        RFIT    135
bel3:   GLO     R9                      ; the dot (on the field)
        SMI     FW
        BDF     bel4
        GHI     R9
        SMI     FH
        BDF     bel4
        FCALL   dot_r9
bel4:   GLO     R9                      ; the end?
        STR     R2
        GETV    BLX1
        XOR
        BNZ     bel5
        GHI     R9
        STR     R2
        GETV    BLY1
        XOR
        BNZ     bel5
        FRET
bel5:   GETV    BLERR                   ; e2 = 2 err (RC)
        SHL
        PLO     RC
        GETV    BLERR+1
        SHLC
        PHI     RC
        GETV    BLDY                    ; e2 > -dy: err -= dy, x += sx
        STR     R2                      ;  (e2 + dy > 0)
        GLO     RC
        ADD
        PLO     R8
        GHI     RC
        ADCI    0
        PHI     R8
        ANI     80H
        BNZ     bel6
        GLO     R8
        BNZ     bel5a
        GHI     R8
        BZ      bel6
bel5a:  GETV    BLDY
        STR     R2
        GETV    BLERR
        SM
        STR     RA
        GETV    BLERR+1
        SMBI    0
        STR     RA
        GETV    BLSX
        STR     R2
        GLO     R9
        ADD
        PLO     R9
bel6:   GETV    BLDX                    ; e2 < dx: err += dx, y += sy
        STR     R2                      ;  (e2 - dx < 0)
        GLO     RC
        SM
        GHI     RC
        SMBI    0
        ANI     80H
        BZ      bel7
        GETV    BLDX
        STR     R2
        GETV    BLERR
        ADD
        STR     RA
        GETV    BLERR+1
        ADCI    0
        STR     RA
        GETV    BLSY
        STR     R2
        GHI     R9
        ADD
        PHI     R9
bel7:   FJMP    bel3

; ---------------------------------------------------------------------------
; ply_draw -- the pass's picture: the shells where their heads are, each
;   fireball's or dirt ball's next stage, a few dirty columns, the tanks.
; ---------------------------------------------------------------------------
        RFIT    184
ply_draw:
        SETV    T0, 0                   ; the heads: wanted (PHNX/PHNY) as
pdr1:   GETV    T0                      ;  shown (PHX/PHY)? (an on track's
        RECOF   PTREC, TR_SH            ;  head found here, once a pass)
        BZ      pdr1a
        GETV    T0
        FCALL   trk_head
pdr1a:  GETV    T0
        ADI     LO(PHNX)
        PLO     R8
        LDI     HI(PHNX)
        PHI     R8
        LDN     R8
        PLO     R9
        GLO     R8
        ADI     NTRK
        PLO     R8
        LDN     R8
        PHI     R9
        GLO     R8                      ; R8: PHX[k] (PHX: PHNX + 2 NTRK)
        ADI     NTRK
        PLO     R8
        GLO     R9
        STR     R2
        LDN     R8
        XOR
        BNZ     pdr2
        GLO     R8
        ADI     NTRK
        PLO     R8
        GHI     R9
        STR     R2
        LDN     R8
        XOR
        BZ      pdr8
        GLO     R8
        SMI     NTRK
        PLO     R8
pdr2:   GLO     R9                      ; shown as wanted
        STR     R8
        GLO     R8
        ADI     NTRK
        PLO     R8
        GHI     R9
        STR     R8
        GLO     R9
        XRI     NONE
        BZ      pdr4
        PARS                            ; SPRITE 10 + k, the shell, MASK
        GETV    T0
        ADI     SPR_TRK
        PD
        P       IM_SHELL
        GLO     R9
        SMI     1
        PD
        GHI     R9
        SMI     1
        PD
        P       TM_MASK
        GOP     TOP_SPRITE
        BR      pdr8
pdr4:   PARS                            ; hidden
        GETV    T0
        ADI     SPR_TRK-8
        ADI     LO(BITS)
        PLO     RB
        LDI     HI(BITS)
        PHI     RB
        P       0
        LDN     RB
        PD
        GOP     TOP_SPRHIDE
pdr8:   INCV    T0
        XRI     NTRK
        BNZ     pdr1
        FCALL   pd_blasts
        FCALL   pd_view
        LDI     2
        FCALL   fld_flush
        FJMP    tk_pass

; pd_blasts: each blast on: its stage (of 4: 4 n / total) drawn if it has
;   grown: the ellipse at stage / 4 of its size, fire's pattern (a dirt
;   ball: solid).
        RFIT    178
pd_blasts:
        SETV    T0, 0
pdb1:   GETV    T0
        RECOF   PBREC, BL_SH
        BZ      pdb8
        FCALL   bl_n                    ; n (T2 total)
        SHL                             ; 4 n / total, 4 at most
        BDF     pdb3
        SHL
        BDF     pdb3
        PLO     R9
        LDI     0
        PHI     R9
pdb2:   GETV    T2
        STR     R2
        GLO     R9
        SM
        BNF     pdb4
        PLO     R9
        GHI     R9
        ADI     1
        PHI     R9
        SMI     4
        BNF     pdb2
pdb3:   LDI     4
        PHI     R9
pdb4:   GHI     R9                      ; grown past the stage drawn?
        BZ      pdb8
        PUTV    T3
        GETV    T0
        ADI     LO(PBSTG)
        PLO     R8
        LDI     HI(PBSTG)
        PHI     R8
        LDN     R8
        STR     R2
        GETV    T3
        SM
        BZ      pdb8
        BNF     pdb8
        GETV    T3
        STR     R8
        FCALL   bl_ell                  ; its size at stage / 4
        GETV    T3
        PLO     R9
        GETV    ERX
        FCALL   stage_of
        PUTV    ERX
        GETV    ERY
        FCALL   stage_of
        PUTV    ERY
        GETV    T0
        RECOF   PBREC, BL_SH
        XRI     BLK_BLAST
        BZ      pdb5
        SETV    EMODE, TM_SET
        BR      pdb6
pdb5:   SETV    EMODE, FIRE_MODE
pdb6:   FCALL   ell_draw
pdb8:   INCV    T0
        XRI     NBLAST
        BNZ     pdb1
        FRET

; stage_of: D r at stage R9.0 (1-4) of 4: r / 4, r / 2, 3 r / 4, r; 1 at
;   least. Keeps R9.
        RFIT    36
stage_of:
        PLO     RC
        GLO     R9
        XRI     4
        BZ      sto9
        GLO     R9
        XRI     2
        BNZ     sto1
        GLO     RC
        SHR
        BR      sto8
sto1:   GLO     RC
        SHR
        SHR
        STR     R2
        GLO     R9
        XRI     1
        BZ      sto2
        GLO     RC                      ; 3: r - r / 4
        SM
        BR      sto8
sto2:   LDN     R2
sto8:   BNZ     sto9a
        LDI     1
sto9a:  FRET
sto9:   GLO     RC
        FRET

; ---------------------------------------------------------------------------
; ply_end -- the segment's end: settles land, paths stop, flows run out
;   (view.go EndSegment); at the script's last segment, the shells go and
;   beams are cleared.
; ---------------------------------------------------------------------------
        RFIT    117
ply_end:
        SETV    T0, 0
ple1:   GETV    T0                      ; the settles
        RECOF   PSREC, SE_SH
        BZ      ple2
        GETV    T0
        FCALL   se_land
ple2:   INCV    T0
        XRI     NSETTLE
        BNZ     ple1
        SETV    T0, 0
ple3:   FCALL   pa_end                  ; the paths
        INCV    T0
        XRI     NPATH
        BNZ     ple3
        SETV    T0, 0
ple4:   GETV    T0                      ; the flows
        RECOF   PFREC, FL_SH
        BZ      ple5
        GETV    T0
        FCALL   fl_end
ple5:   INCV    T0
        XRI     NFLOW
        BNZ     ple4
        GETV    SEGFLG                  ; the script's end?
        ANI     S_LAST_SEG
        BZ      ple9
        FJMP    ply_script_end
ple9:   FRET

; ply_script_end: the shells off, the beams cleared and off.
        RFIT    206
ply_script_end:
        SETV    T0, 0
pse1:   GETV    T0
        RECOF   PTREC, TR_SH
        BZ      pse2
        GETV    T0
        FCALL   trk_off
pse2:   INCV    T0
        XRI     NTRK
        BNZ     pse1
        SETV    T0, 0
pse3:   GETV    T0
        RECOF   PEREC, BE_SH
        BZ      pse4
        SETV    EMODE, TM_CLEAR
        GETV    T0
        FCALL   be_line
        GETV    T0
        RECOF   PEREC, BE_SH
        POKER   RB
        LDI     0
        PKD
        PKEND
        VAR     PCE
        LDN     RA
        SMI     1
        STR     RA
pse4:   INCV    T0
        XRI     NBEAM
        BNZ     pse3
        SETV    PSHAKE, 0               ; the picture still, as the sky has it
        SETV    PFLASH, 0
        HOLD1
        FCALL   ply_draw                ; (the shells hidden)
        HOLD0
        FRET

; pd_view: the shake (the picture a line down, then up, 4 frames each) and
;   the flash (inverted 4 frames in 8) on the sky's VIEW; sent if changed.
        RFIT    103
pd_view:
        LDI     0                       ; dy
        PLO     R9
        GETV    PSHAKE
        BZ      pdv1
        GETV    PNOW
        ANI     4
        BZ      pdv0
        LDI     1
        BR      pdv0a
pdv0:   LDI     0FFH
pdv0a:  PLO     R9
pdv1:   GETV    VIEWF                   ; flags
        PHI     R9
        GETV    PFLASH
        BZ      pdv2
        GETV    PNOW
        ANI     4
        BZ      pdv2
        GHI     R9
        XRI     TV_INVERT
        PHI     R9
pdv2:   GLO     R9                      ; as shown?
        STR     R2
        GETV    VIEWD
        XOR
        BNZ     pdv3
        GHI     R9
        STR     R2
        GETV    VIEWD+1
        XOR
        BZ      pdv9
pdv3:   GLO     R9
        PUTV    VIEWD
        GHI     R9
        PUTV    VIEWD+1
        PARS
        P       0
        GLO     R9
        PD
        GHI     R9
        PD
        GOP     TOP_VIEW
pdv9:   FRET
