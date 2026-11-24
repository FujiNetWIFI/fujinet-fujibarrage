; p_ply.inc -- the shot player (server/internal/view/view.go, the reference
; client, op for op; docs/PROTOCOL.md "Playing a segment"; the Astrocade's
; src/astrocade/src/p_ply.inc in 2650 form): SCRIPT's segment (SEGLEN
; bytes) played to its end, a frame at a time -- every track, blast, path,
; flow, beam and settle moves a step, then the ops due run -- up to 8 steps
; a pass when behind (the cart's tick: DT), the blasts drawn once a pass.
; At its end (View.EndSegment) the settles land and the paths and flows
; finish (the bytes they read are going). Paths, flows and beams are the
; OPS page's (FARPG); the field's blocks are blk.inc's; the tanks as drawn
; are p_fld.inc's (TKX, TKY, TKA: OP_TANK, OP_FALL, OP_DEATH, OP_FIRE move
; them, the block tanks drawn again after the pass: E_TKSEL).
;
; Depth: the main loop's GOPAGE runs PLAYSEG with one return address on
; the stack; the ops run inline (flows, not calls) and the ellipse (KELL,
; six deep) is called only from PLAYSEG's own level.

        BCTA,UN PLAYSEG         ; E_ENTER: the segment in SCRIPT, played

DIRTBALL EQU    $80             ; an OP_DIRT, kept with the blasts
NCATCH  EQU     8               ; steps a pass at most
PLAGMAX EQU     180             ; a shot's timeline that far behind: given up

; RAM-H, through pointer words (page 0 code)
PPWAITL: DWBE   PWAITL
PPWAITH: DWBE   PWAITH
PPDONE: DWBE    PDONE
PPLAST: DWBE    PLAST
PPLAST1: DWBE   PLAST+1
PCTKL:  DWBE    FNTICKL         ; the cart's 60 Hz tick (read HI, LO, HI)
PCTKH:  DWBE    FNTICKH
PPSTEPS: DWBE   PSTEPS
PPBLK:  DWBE    PBLK
PFLASHN: DWBE   FLASHN
PSHAKEN: DWBE   SHAKEN
PTLOCK: DWBE    TLOCK
PLPA0:  DWBE    PATHS           ; the paths', beams', flows' on bytes
PLPA1:  DWBE    PATHS+PASZ
PLPA2:  DWBE    PATHS+2*PASZ
PLPA3:  DWBE    PATHS+3*PASZ
PLBE0:  DWBE    BEAMS
PLBE1:  DWBE    BEAMS+BESZ
PLFL0:  DWBE    FLOWS
PLFL1:  DWBE    FLOWS+FLSZ
PLFL2:  DWBE    FLOWS+2*FLSZ
PLFL3:  DWBE    FLOWS+3*FLSZ
PSTEPC: DWBE    STEPC
PSTEPC1: DWBE   STEPC+1
PTALKT: DWBE    TALKT
PTALKN: DWBE    TALKN
; a blast's bytes (R3 its offset, 8 b)
BP0:    DWBE    BLASTS          ; kind ($80: dirt)
BP1:    DWBE    BLASTS+1        ; n: its frames so far
BP2:    DWBE    BLASTS+2        ; tot: its frames (0: free)
BP3:    DWBE    BLASTS+3        ; rx, ry
BP4:    DWBE    BLASTS+4
BP5:    DWBE    BLASTS+5        ; dn: n as drawn
BP6:    DWBE    BLASTS+6        ; x, y (signed, -32..95)
BP7:    DWBE    BLASTS+7
; a settle's (R3 its offset, 5 s)
SP0:    DWBE    SETTLES         ; frames left
SP1:    DWBE    SETTLES+1       ; columns (0: free)
SP2:    DWBE    SETTLES+2       ; x16
SP3:    DWBE    SETTLES+3
SP4:    DWBE    SETTLES+4       ; its entries' pc

; ---- a segment ------------------------------------------------------------------
; PLAYSEG: SCRIPT's segment played to its OP_END: a pass a frame (FRAME),
; its steps (DT: the frames since the last, at most 8), the blasts drawn,
; the tanks and the HUD if they changed; then the segment's end. The
; decoder's bytes (PLYSCR) are the player's meanwhile: zeroed after (the
; decoder idle).
PLAYSEG: LODA,R0 SEGNO          ; a new shot: nothing moving; the HUD again
        BCFR,EQ PS1             ;  (no aim on it now; a message left over
        BSTA,UN PLYRESET        ;  gone: PLYRESET zeroed TALKT)
        LODA,R0 FDIRTY
        IORI,R0 FD_HUD
        STRA,R0 FDIRTY
PS1:    EORZ    R0
        STRA,R0 PPC
        STRA,R0 PFLG
        STRA,R0 MYTURN
        CPUT    PDONE
        LODA,R0 FDIRTY
        IORI,R0 FD_SPR
        STRA,R0 FDIRTY
        LODA,R0 SEGNO           ; the shot's timeline: from now at its first
        BCTR,EQ PSY1            ;  segment; on from the last one's after (its
        BSTA,UN PBEHIND         ;  lag and the link's wait caught up, as the
        COMI,R0 PLAGMAX         ;  frames are the clock), unless that's far
        BCTR,LT PSLOOP          ;  behind
PSY1:   BSTA,UN PTICK16
        STRA,R2 PTMP
        LODZ    R1
        CPUT    PLAST+1
        LODA,R0 PTMP
        CPUT    PLAST
PSLOOP: LODA,R0 *PPDONE         ; a pass
        BCFA,EQ PSEND
PSL0:   BSTA,UN PBEHIND         ; the frames the timeline is behind the tick:
        COMI,R0 0               ;  none, the next frame first (the blank: the
        BCFR,EQ PSL1            ;  sound, the controls)
        ZBSR    *ZFRAME
        BCTR,UN PSL0
PSL1:   COMI,R0 NCATCH+1        ; at most 8 steps a pass (the test scene's
        BCTR,LT PSL2            ;  lockstep: 1)
        LODI,R0 NCATCH
PSL2:   LODA,R1 *PTLOCK
        BCTR,EQ PSL3
        LODI,R0 1
PSL3:   STRZ    R1              ; PF_BEHIND: a whole catch-up's steps
        LODA,R0 PFLG
        ANDI,R0 ~PF_BEHIND&$FF
        COMI,R1 NCATCH
        BCFR,EQ PSL3A
        IORI,R0 PF_BEHIND
PSL3A:  STRA,R0 PFLG
        LODZ    R1
        CPUT    PSTEPS
        LODA,R0 *PPSTEPS        ; the timeline on by them
        ADDA,R0 *PPLAST
        CPUT    PLAST
        TPSL    PSLC
        BCFR,EQ PSL4
        LODA,R0 *PPLAST1
        ADDI,R0 1
        CPUT    PLAST+1
PSL4:   FARPG   PG_PLAY,E_VBL   ; the sprites (in the blank if it waited)
; ---- a step (view.go Step): what moves, then the ops due --------------------------
PSSTEP: FARPG   PG_TRK,E_TSTEP  ; the tracks
        LODI,R3 0               ; the blasts: each a frame on (R3 the free
PSB0:   LODA,R0 *BP2,R3         ;  ones' offset; a live one's in PBLK)
        BCFR,EQ PSB1
        ADDI,R3 BLSZ
        COMI,R3 NBLAST*BLSZ
        BCTR,LT PSB0
        BCTA,UN PSB10
PSB1:   STRZ    R2              ; (R2: tot)
        LODZ    R3
        CPUT    PBLK
        LODA,R3 *PPBLK
        LODA,R0 *BP1,R3         ; n += 1
        ADDI,R0 1
        CPUTR   BLASTS+1
        LODA,R3 *PPBLK
        LODA,R0 *BP1,R3
        COMA,R0 *BP2,R3
        BCTA,LT PSB9            ; (growing: drawn at the pass's end)
        FARPG   PG_BLS,E_BGROW  ; its last: grown to its size, then its end
        FARPG   PG_BLS,E_BEND   ;  (a crater: air; a flash: its fire out; dirt:
        EORZ    R0              ;  none); gone
        LODA,R3 *PPBLK
        CPUTR   BLASTS+2
PSB9:   LODA,R3 *PPBLK
        ADDI,R3 BLSZ
        COMI,R3 NBLAST*BLSZ
        BCTA,LT PSB0
PSB10:
        LODA,R0 *PLPA0          ; the paths, beams (none live: no far call)
        IORA,R0 *PLPA1
        IORA,R0 *PLPA2
        IORA,R0 *PLPA3
        IORA,R0 *PLBE0
        IORA,R0 *PLBE1
        BCTR,EQ PSO1
        FARPG   PG_OPS,E_OSTEP
PSO1:   LODA,R0 *PLFL0          ; the flows
        IORA,R0 *PLFL1
        IORA,R0 *PLFL2
        IORA,R0 *PLFL3
        BCTR,EQ PSO2
        FARPG   PG_FLW,E_FSTEP
PSO2:
        BCTA,UN STLSTEP         ; the settles (a flow: the depth)
PSTL9:
        BSTA,UN FXSTEP          ; the flash, the shake
        LODA,R0 *PPWAITL        ; a wait: one frame of it
        IORA,R0 *PPWAITH
        BCTR,EQ PSOPS
        LODA,R0 *PPWAITL
        SUBI,R0 1
        CPUT    PWAITL
        LODA,R0 *PPWAITL
        COMI,R0 $FF
        BCFR,EQ PSOPS
        LODA,R0 *PPWAITH
        SUBI,R0 1
        CPUT    PWAITH
PSOPS:  LODA,R0 *PPWAITL        ; the ops due: until a wait or the end
        IORA,R0 *PPWAITH
        BCFR,EQ PSS9
        LODA,R0 *PPDONE
        BCFR,EQ PSS9
        LODA,R0 PPC             ; (past the segment's bytes: its end)
        COMA,R0 SEGLEN
        BCFA,LT OPEND
        BCTA,UN OPDO            ; an op (it comes back to PSOPS)
PSS9:   LODA,R0 *PPDONE         ; the step's done: another?
        BCFR,EQ PSDRAW
        LODA,R0 *PPSTEPS
        SUBI,R0 1
        CPUT    PSTEPS
        LODA,R0 *PPSTEPS
        BCFA,EQ PSSTEP
; ---- the pass's end: the blasts at their size, the tanks, the HUD ---------------
PSDRAW: LODA,R0 PFLG            ; far behind: the fireballs' growth not drawn
        TMI,R0  PF_BEHIND       ;  (it's fire: their ends draw what's left
        BCTA,EQ PSD11           ;  and cut the craters)
        LODI,R3 0               ; (R3 as PSB0's)
PSD0:   LODA,R0 *BP2,R3
        BCTR,EQ PSD9
        LODA,R0 *BP1,R3         ; (dirt grows each frame too: view.go's)
        COMA,R0 *BP5,R3
        BCTR,EQ PSD9
        LODZ    R3
        CPUT    PBLK
        FARPG   PG_BLS,E_BGROW
        LODA,R3 *PPBLK          ; drawn to n
        LODA,R0 *BP1,R3
        CPUTR   BLASTS+5
        LODA,R3 *PPBLK
PSD9:   ADDI,R3 BLSZ
        COMI,R3 NBLAST*BLSZ
        BCTR,LT PSD0
PSD11:  BSTA,UN PSTK            ; the block tanks again
        LODA,R0 *PTALKN         ; the HUD's message: a letter a pass (a
        BCFR,EQ PSD11B          ;  catch-up pass types one); its time in
        LODA,R0 *PTALKT         ;  eighths of a second (TALKT: when the
        BCTR,EQ PSD11A          ;  clock passes one)
        LODA,R0 FRML
        ANDI,R0 7
        COMA,R0 DT
        BCFR,LT PSD11A
PSD11B: FARPG   PG_MSG,E_MSTEP
PSD11A:
        LODA,R0 FDIRTY          ; the HUD, if asked (OP_WIND, a message's end;
        TMI,R0  FD_HUD          ;  p_hud.inc: none while a message shows)
        BCFR,EQ PSD12
        FARPG   PG_HUD,E_HUD
PSD12:
        LODA,R0 *PTLOCK         ; the test scene's lockstep: the step's
        BCTA,EQ PSLOOP          ;  count, then a frame with it as it is
        LODA,R0 *PSTEPC         ;  (the rig compares the field then)
        ADDI,R0 1
        CPUT    STEPC
        LODA,R0 *PSTEPC
        BCFR,EQ PSD10
        LODA,R0 *PSTEPC1
        ADDI,R0 1
        CPUT    STEPC+1
PSD10:  ZBSR    *ZFRAME
        BCTA,UN PSLOOP
; PTICK16: R1:R2 = the cart's tick (HI, LO, HI: the bytes change together).
; Leaf. Clobbers R0.
PTICK16: LODA,R1 *PCTKH
        LODA,R2 *PCTKL
        LODA,R0 *PCTKH
        COMZ    R1
        BCFR,EQ PTICK16
        RETC,UN
; PBEHIND: R0 = the ticks since the timeline's (PLAST), 255 at most. Calls
; one down. Clobbers R0-R2.
PBEHIND: BSTR,UN PTICK16
        PPSL    PSLWC|PSLC
        LODZ    R2
        SUBA,R0 *PPLAST
        STRZ    R2
        LODZ    R1
        SUBA,R0 *PPLAST1
        CPSL    PSLWC|PSLC
        BCFR,EQ PBH1
        LODZ    R2
        RETC,UN
PBH1:   TMI,R0  $80             ; (ahead: none)
        BCTR,EQ PBH2
        LODI,R0 255
        RETC,UN
PBH2:   EORZ    R0
        RETC,UN

; PSTK: the block tanks drawn again if they were taken off (or moved).
; (No HUD in a segment: the field's page draws it with a far call of its
; own -- PGFAR is one level only -- so OP_WIND's FD_HUD waits for the main
; loop's E_TICK after the segment.)
PSTK:   LODA,R0 PFLG
        TMI,R0  PF_TK
        RETC,LT
        ANDI,R0 ~PF_TK&$FF
        STRA,R0 PFLG
        FARPG   PG_FLD,E_TKSEL
        RETC,UN

; ---- the segment's end (View.EndSegment) -----------------------------------------
PSEND:  LODI,R0 0               ; the settles land
        CPUT    PBLK
PSE1:   LODA,R3 *PPBLK
        LODA,R0 *SP1,R3
        BCTR,EQ PSE2
        FARPG   PG_SET,E_SAPPLY
PSE2:   LODA,R0 *PPBLK
        ADDI,R0 STSZ
        CPUT    PBLK
        LODA,R0 *PPBLK
        COMI,R0 NSETTLE*STSZ
        BCTR,LT PSE1
        FARPG   PG_OPS,E_OSEGEND        ; the paths stop, the flows run out
        FARPG   PG_FLW,E_FSEGEND
        BSTA,UN PSTK
        FARPG   PG_MSG,E_MFLUSH ; the talk's letters (SCRIPT's bytes go)
        LODA,R0 SEGFLG          ; a script under way (not its last segment):
        TMI,R0  S_LAST_SEG      ;  its next is awaited (NSEGW)
        BCTR,EQ PSE5
        LODA,R0 FLAGS
        IORI,R0 FL_INSCR
        BCTR,UN PSE6
PSE5:   FARPG   PG_HUD,E_UIOFF  ; the script's end: AIMV the field's UI again
        LODA,R0 FLAGS
        ANDI,R0 ~FL_INSCR&$FF
PSE6:   CPUT    FLAGS
        LODA,R0 NSEGS           ; (the rigs': segments played)
        ADDI,R0 1
        CPUT    NSEGS
        EORZ    R0              ; the decoder's bytes back: idle
        LODI,R3 PLYSCRN
PSE3:   STRA,R0 PLYSCR-1,R3
        BDRR,R3 PSE3
        RETC,UN

; PLYRESET: a new shot: nothing moving (SHOTRES to SHOTEND zeroed, a
; stream), no flash or shake left. Calls one down. Clobbers R0-R3.
PLYRESET: LODI,R1 FRRAMP2+((SHOTRES-$2900)>>8)
        LODI,R3 (SHOTRES-$2900)&$FF
        BSTA,UN RAMSET
        LODI,R2 0
        LODI,R3 SHOTEND-SHOTRES-256
PR1:    LODA,R0 *PRDAT,R2
        BDRR,R3 PR1
        LODI,R3 0
PR2:    LODA,R0 *PRDAT,R2
        BDRR,R3 PR2
        BSTA,UN RAMPEND
        IF      SHOTEND-SHOTRES <= 256
        ERROR   "PLYRESET: the shot's state is 256 bytes or less now"
        ENDIF
        LODI,R0 FBGCOL
        CPUT    BGCB
        EORZ    R0
        STRA,R0 SHAKEB
        RETC,UN

; ---- the ops -------------------------------------------------------------------------
; OPDO: the op at PPC, its arguments after it (ARGS: R3 = PPC, then
; *PSCRIPT,R3,+ reads the next). Each runs inline and goes back to PSOPS
; (OPNEXT: PPC on past its R0 bytes).
OPDO:   LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3
        ADDI,R3 1
        STRA,R3 PPC
        COMI,R0 OP_WAIT
        BCTA,EQ OPWAIT
        COMI,R0 OP_ARC
        BCTA,EQ OPARCF
        COMI,R0 OP_PATH
        BCTA,EQ OPPATH
        COMI,R0 OP_STOP
        BCTA,EQ OPSTOPF
        COMI,R0 OP_EXPLODE
        BCTA,EQ OPEXPL
        COMI,R0 OP_DIRT
        BCTA,EQ OPEXPL
        COMI,R0 OP_BEAM
        BCTA,EQ OPBEAM
        COMI,R0 OP_FLOW
        BCTA,EQ OPFLOW
        COMI,R0 OP_SETTLE
        BCTA,EQ OPSETL
        COMI,R0 OP_TALK
        BCTA,EQ OPTALK
        COMI,R0 OP_SOUND
        BCTA,EQ OPSND
        COMI,R0 OP_SHIELD
        BCTA,EQ OPSHLD
        COMI,R0 OP_DAMAGE
        BCTA,EQ OPDMG
        COMI,R0 OP_FALL
        BCTA,EQ OPFALL
        COMI,R0 OP_DEATH
        BCTA,EQ OPDEATH
        COMI,R0 OP_TANK
        BCTA,EQ OPTANK
        COMI,R0 OP_WIND
        BCTA,EQ OPWIND
        COMI,R0 OP_SHAKE
        BCTA,EQ OPSHAKE
        COMI,R0 OP_FLASH
        BCTA,EQ OPFLASH
        COMI,R0 OP_FIRE
        BCTA,EQ OPFIRE
; OP_END (or one this client doesn't know, OP_TUNNEL: the segment ends)
OPEND:  LODI,R0 1
        CPUT    PDONE
        BCTA,UN PSOPS
; OPNEXT: PPC += R0, the next op.
OPNEXT: ADDA,R0 PPC
        STRA,R0 PPC
        BCTA,UN PSOPS
; OP_SHIELD seat, event, hp; OP_DAMAGE seat, amount, energy left: the
; HUD's event line (p_hud.inc).
OPSHLD: FARPG   PG_MSG,E_OSHLD
        BCTR,UN OPSK3
OPDMG:  FARPG   PG_MSG,E_ODMG
OPSK3:  LODI,R0 3
        BCTR,UN OPNEXT

; OP_WAIT n: n frames (0: 256).
OPWAIT: LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3
        CPUT    PWAITL
        LODA,R0 *PPWAITL
        BCTR,EQ OPW1
        EORZ    R0
        BCTR,UN OPW2
OPW1:   LODI,R0 1
OPW2:   CPUT    PWAITH
        LODI,R0 1
        BCTA,UN OPNEXT

; OP_TALK seat, len, text: over the HUD's rows (p_hud.inc).
OPTALK: FARPG   PG_MSG,E_OTALK
        LODA,R3 PPC
        ADDI,R3 1
        LODA,R0 *PSCRIPT,R3
        ADDI,R0 2
        BCTA,UN OPNEXT

; OP_SOUND id, size: played (SND_BOOM: SND_BOOM0-3 by its size).
OPSND:  LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3
        COMI,R0 SND_BOOM
        BCFR,EQ OPSB
        ADDI,R3 1
        LODA,R0 *PSCRIPT,R3
        ANDI,R0 3
        ADDI,R0 SND_BOOM0
OPSB:   ZBSR    *ZSNDQ
        LODI,R0 2
        BCTA,UN OPNEXT

; OP_WIND wind16: the state's (the HUD's).
OPWIND: CPTR    ST+OFF_WIND
        LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3
        BSTA,UN RAMPUT1
        LODA,R3 PPC
        ADDI,R3 1
        LODA,R0 *PSCRIPT,R3
        BSTA,UN RAMPUT1
        BSTA,UN RAMPEND
        LODA,R0 FDIRTY
        IORI,R0 FD_HUD
        STRA,R0 FDIRTY
        LODI,R0 2
        BCTA,UN OPNEXT

; OP_SHAKE frames.
OPSHAKE: LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3
        CPUT    SHAKEN
        LODI,R0 1
        BCTA,UN OPNEXT

; OP_FLASH colour, frames: the sky white for its frames.
OPFLASH: LODA,R3 PPC
        ADDI,R3 1
        LODA,R0 *PSCRIPT,R3
        CPUT    FLASHN
        LODA,R0 *PFLASHN
        BCTR,EQ OPF1
        LODI,R0 FBGFLASH
        CPUT    BGCB
OPF1:   LODI,R0 2
        BCTA,UN OPNEXT
FBGFLASH EQU    $80|(FHOT<<3)|CWHITE

; OP_FIRE seat, angle, power16, weapon: the barrel at the angle.
OPFIRE: LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3
        COMI,R0 SEATS
        BCFR,LT OPFI1
        STRA,R0 PTMP
        LODA,R3 PPC
        ADDI,R3 1
        LODA,R0 *PSCRIPT,R3
        LODA,R3 PTMP
        CPUTR   TKA
        LODA,R0 FDIRTY
        IORI,R0 FD_SPR
        STRA,R0 FDIRTY
OPFI1:  LODI,R0 5
        BCTA,UN OPNEXT

; OP_DEATH seat, effect: gone (its blast comes as OP_EXPLODE).
OPDEATH: LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3
        COMI,R0 SEATS
        BCFR,LT OPD1
        STRA,R0 PTMP
        BSTA,UN TKCHG
        LODI,R0 $FF
        LODA,R3 PTMP
        CPUTR   TKX
OPD1:   LODI,R0 2
        BCTA,UN OPNEXT

; OP_TANK seat, x16, y, angle: placed. OP_FALL seat, x16, y, frames, chute:
; it goes there (at once: view.go's).
OPTANK: LODI,R0 5
        BCTR,UN OPT0
OPFALL: LODI,R0 6
OPT0:   STRA,R0 EX              ; (the op's length)
        LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3
        COMI,R0 SEATS
        BCFA,LT OPT9
        STRA,R0 PTMP
        BSTA,UN TKCHG
        LODA,R3 PPC             ; x: on the field, else none
        LODA,R0 *PSCRIPT,R3,+
        STRA,R0 EY
        LODA,R0 *PSCRIPT,R3,+
        BCFR,EQ OPT1
        LODA,R0 EY
        COMI,R0 FIELD_W
        BCTR,LT OPT2
OPT1:   LODI,R0 $FF
OPT2:   LODA,R3 PTMP
        CPUTR   TKX
        LODA,R3 PPC             ; y
        ADDI,R3 3
        LODA,R0 *PSCRIPT,R3
        LODA,R3 PTMP
        CPUTR   TKY
        LODA,R0 EX              ; OP_TANK's angle too
        COMI,R0 5
        BCFR,EQ OPT9
        LODA,R3 PPC
        ADDI,R3 4
        LODA,R0 *PSCRIPT,R3
        LODA,R3 PTMP
        CPUTR   TKA
OPT9:   LODA,R0 EX
        BCTA,UN OPNEXT

; TKCHG: a tank is to move: the block tanks off first (they come back
; after the pass: PF_TK), the sprites again. Calls three down.
TKCHG:  LODA,R0 TBN
        BCTR,EQ TKC1
        BSTA,UN TKOFF
TKC1:   LODA,R0 PFLG
        IORI,R0 PF_TK
        STRA,R0 PFLG
        LODA,R0 FDIRTY
        IORI,R0 FD_SPR
        STRA,R0 FDIRTY
        RETC,UN

; OP_ARC, OP_STOP: the TRK page's.
OPARCF: FARPG   PG_TRK,E_TLOAD
        LODI,R0 18
        BCTA,UN OPNEXT
OPSTOPF: FARPG  PG_TRK,E_TSTOP
        LODI,R0 1
        BCTA,UN OPNEXT

; OP_PATH, OP_BEAM, OP_FLOW: the OPS page's (their bytes taken: R2).
OPPATH: FARPG   PG_OPS,E_OPATH
        BCTR,UN OPFAR
OPBEAM: FARPG   PG_OPS,E_OBEAM
        BCTR,UN OPFAR
OPFLOW: FARPG   PG_FLW,E_OFLOW
OPFAR:  LODZ    R2
        BCTA,UN OPNEXT

; ---- blasts (OP_EXPLODE, OP_DIRT) ------------------------------------------
; OP_EXPLODE x16, y16, rx, ry, kind, frames; OP_DIRT the same without its
; kind: the first free blast (none: dropped, as view.go's 16 full would).
OPEXPL: STRA,R0 EX              ; (the op: OP_DIRT has no kind)
        LODI,R3 0
OE1:    LODA,R0 *BP2,R3
        BCTR,EQ OE2
        ADDI,R3 BLSZ
        COMI,R3 NBLAST*BLSZ
        BCTR,LT OE1
        BCTA,UN OE9
OE2:    LODI,R1 FRRAMP2+((BLASTS-$2900)>>8)
        ADDI,R3 (BLASTS-$2900)&$FF
        BSTA,UN RAMSET
        LODA,R3 PPC             ; kind (dirt: DIRTBALL)
        ADDI,R3 6
        LODA,R0 *PSCRIPT,R3
        STRA,R0 EY
        LODA,R0 EX
        COMI,R0 OP_DIRT
        BCFR,EQ OE3
        LODI,R0 DIRTBALL
        STRA,R0 EY
        LODA,R0 *PSCRIPT,R3     ; (its frames: byte 6)
        BCTR,UN OE4
OE3:    ADDI,R3 1
        LODA,R0 *PSCRIPT,R3     ; frames (0: 1)
OE4:    BCFR,EQ OE5
        LODI,R0 1
OE5:    STRA,R0 PTMP
        LODA,R0 EY
        BSTA,UN TW
        LODI,R0 0               ; n
        BSTA,UN TW
        LODA,R0 PTMP            ; tot
        BSTA,UN TW
        LODA,R3 PPC             ; rx, ry: 24 at most
        ADDI,R3 4
        LODA,R0 *PSCRIPT,R3
        BSTR,UN RCLAMP
        BSTA,UN TW
        LODA,R3 PPC
        ADDI,R3 5
        LODA,R0 *PSCRIPT,R3
        BSTR,UN RCLAMP
        BSTA,UN TW
        LODI,R0 0               ; dn
        BSTA,UN TW
        LODA,R3 PPC             ; x, y: -32..95
        BSTR,UN XCLAMP
        BSTA,UN TW
        LODA,R3 PPC
        ADDI,R3 2
        BSTR,UN XCLAMP
        BSTA,UN TW
        BSTA,UN RAMPEND
OE9:    LODI,R0 8
        LODA,R1 EX
        COMI,R1 OP_DIRT
        BCFA,EQ OPNEXT
        LODI,R0 7
        BCTA,UN OPNEXT
; RCLAMP: R0 at most EMAXR. Leaf.
RCLAMP: COMI,R0 EMAXR+1
        RETC,LT
        LODI,R0 EMAXR
        RETC,UN
; XCLAMP: R0 = the 16-bit coordinate at SCRIPT + R3 as a byte, -32..95
; (an ellipse of 24 at most reaches the field from no further). Leaf.
XCLAMP: LODA,R0 *PSCRIPT,R3,+   ; (the high byte)
        BCTR,EQ XC1
        BCTR,LT XC3
XC2:    LODI,R0 95
        RETC,UN
XC1:    LODA,R0 *PSCRIPT,R3,-   ; 0-255: at most 95
        COMI,R0 96
        RETC,LT
        BCTR,UN XC2
XC3:    COMI,R0 $FF             ; negative: -1..-32 kept
        BCFR,EQ XC4
        LODA,R0 *PSCRIPT,R3,-
        COMI,R0 $E0
        RETC,GT
        RETC,EQ
XC4:    LODI,R0 $E0
        RETC,UN

; ---- settles (OP_SETTLE) -------------------------------------------------------
; OP_SETTLE x16, n, frames, n column entries: written now (frames 0) or
; kept, landing after frames (or at the segment's end). Full: the first
; lands now and its place is taken.
OPSETL: LODA,R3 PPC
        ADDI,R3 2
        LODA,R0 *PSCRIPT,R3     ; n: none, nothing to do
        STRA,R0 ERX
        BCTA,EQ OPS5
        LODA,R0 *PSCRIPT,R3,+   ; frames 0: now
        BCFR,EQ OPS1
        FARPG   PG_SET,E_SNOW
        BCTA,UN OPS5
OPS1:   LODI,R3 0               ; kept: the first free (full: the first lands)
OPS2:   LODA,R0 *SP1,R3
        BCTR,EQ OPS3
        ADDI,R3 STSZ
        COMI,R3 NSETTLE*STSZ
        BCTR,LT OPS2
        LODI,R0 0
        CPUT    PBLK
        FARPG   PG_SET,E_SAPPLY
        LODA,R3 PPC             ; (n again: SAPPLY's)
        ADDI,R3 2
        LODA,R0 *PSCRIPT,R3
        STRA,R0 ERX
        LODI,R3 0
OPS3:   LODI,R1 FRRAMP2+((SETTLES-$2900)>>8)
        ADDI,R3 (SETTLES-$2900)&$FF
        BSTA,UN RAMSET
        LODA,R3 PPC             ; left: frames
        ADDI,R3 3
        LODA,R0 *PSCRIPT,R3
        BSTA,UN TW
        LODA,R0 ERX             ; n
        BSTA,UN TW
        LODA,R3 PPC             ; x16
        LODA,R0 *PSCRIPT,R3
        BSTA,UN TW
        LODA,R0 *PSCRIPT,R3,+
        LODA,R0 *PSCRIPT,R3
        BSTA,UN TW
        LODA,R0 PPC             ; its entries' pc
        ADDI,R0 4
        BSTA,UN TW
        BSTA,UN RAMPEND
OPS5:   LODA,R3 PPC             ; past its entries (n again: the far pages'
        ADDI,R3 2               ;  SAPPLY counts ERX down)
        LODA,R0 *PSCRIPT,R3
        STRZ    R1
        LODA,R0 PPC
        ADDI,R0 4
        STRA,R0 EY
        LODZ    R1
        BCTR,EQ OPS7
OPS6:   LODA,R3 EY
        BSTR,UN ELEN
        ADDA,R0 EY
        STRA,R0 EY
        BDRR,R1 OPS6
OPS7:   LODA,R0 EY
        STRA,R0 PPC
        EORZ    R0
        BCTA,UN OPNEXT
; ELEN: R0 = the length of the column entry at SCRIPT + R3 (1, or 1 + 2n:
; n spans). Leaf.
ELEN:   LODA,R0 *PSCRIPT,R3
        COMI,R0 T_SPANS+1
        BCTR,LT ELN1
        SUBI,R0 T_SPANS
        ADDZ    R0
        ADDI,R0 1
        RETC,UN
ELN1:   LODI,R0 1
        RETC,UN

; STLSTEP: the kept settles count down; at 0 they land. A flow from PSSTEP
; and back (PSTL9).
STLSTEP: LODI,R3 0              ; (R3 the free ones' offset; a kept one's
STS0:   LODA,R0 *SP1,R3         ;  in PBLK)
        BCTR,EQ STS3
        LODZ    R3
        CPUT    PBLK
        LODA,R3 *PPBLK
        LODA,R0 *SP0,R3
        SUBI,R0 1
        CPUTR   SETTLES
        LODA,R3 *PPBLK
        LODA,R0 *SP0,R3
        BCFR,EQ STS2
        FARPG   PG_SET,E_SAPPLY
STS2:   LODA,R3 *PPBLK
STS3:   ADDI,R3 STSZ
        COMI,R3 NSETTLE*STSZ
        BCTR,LT STS0
        BCTA,UN PSTL9

; TW: R0 into the stream. Leaf. Clobbers R2.
TW:     STRZ    R2
        LODA,R0 *PRDAT,R2
        RETC,UN

; ---- the flash and the shake ------------------------------------------------------
; FXSTEP: a frame of each: the flash over, the sky back; the shake's shift
; (2 pixels, every other 2 frames) in VOLUME's b5-7 (the vblank's notes OR
; it in; written here when none plays). Calls one down.
FXSTEP: LODA,R0 *PFLASHN
        BCTR,EQ FX2
        SUBI,R0 1
        CPUT    FLASHN
        LODA,R0 *PFLASHN
        BCFR,EQ FX2
        LODI,R0 FBGCOL
        CPUT    BGCB
FX2:    LODA,R0 *PSHAKEN
        BCFR,EQ FX3
        LODA,R0 SHAKEB          ; (none: no shift)
        RETC,EQ
        EORZ    R0
        BCTR,UN FX4
FX3:    SUBI,R0 1
        CPUT    SHAKEN
        LODA,R0 *PSHAKEN
        ANDI,R0 2
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
FX4:    STRA,R0 SHAKEB
        LODA,R0 SNDPH           ; (a note plays: its next writes it)
        RETC,GT
        RETC,LT
        LODA,R0 SHAKEB
        STRA,R0 VOLUME
        RETC,UN


