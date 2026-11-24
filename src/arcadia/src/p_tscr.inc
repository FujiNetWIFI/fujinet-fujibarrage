; p_tscr.inc -- the screens' offline test scenes (M4; fixed B's TSCRN runs
; them; tools/arcsim/scenes/t*.txt drive and snap them): a canned STATE
; (tools/arc/mkstate_arc.py: state.inc), the scene's phase, the screen:
;    4  a round's results          5  the game's end
;    6  the waiting room (we host: the settings, changing them)
;    7  the field, our turn: the items line, a guidance target, FIRE
;    8  the field, a shot's segment: talk (typed, scrolled), wind, damage,
;       a shield (the HUD's messages, then the HUD again)
;    9  the field, the defence window (its line opens by itself)
;   10  the shop, its timer ticking
; The field's scenes have the shot fixture's terrain and tanks (p_test.inc
; E_TFIX, as scene 3). Offline (FL_TEST): actions go nowhere (the rig
; peeks AARGS for what an action would have said).

        BCTA,UN TSET            ; E_TSET: ST, the phase; R0 the screen
        BCTA,UN TTICK           ; E_TTICK: a frame; R0 1 = a new state
        BCTA,UN TFLD            ; (6) the field's scenes, after E_TFIX

TSCENE  EQU     STEPC+1         ; the scene (TZ's hook dies with the field:
                                ;  the test scene's RAM-H byte)
PTSCN:  DWBE    TSCENE

; TSET: offline; the scene kept; ST from TSTATE; our seat; the phase.
TSET:   LODA,R0 FLAGS
        IORI,R0 FL_TEST
        CPUT    FLAGS
        LODA,R0 TESTHK+2
        CPUT    TSCENE
        CPTR    ST              ; ST: 378 bytes, a stream
        LODI,R3 0
TSE1:   LODA,R0 TSTATE,R3
        BSTA,UN RAMPUT1
        ADDI,R3 1
        BCFR,EQ TSE1
TSE2:   LODA,R0 TSTATE+256,R3
        BSTA,UN RAMPUT1
        ADDI,R3 1
        COMI,R3 STATE_SIZE-256
        BCFR,EQ TSE2
        BSTA,UN RAMPEND
        LODA,R0 ST+OFF_MY_SEAT
        CPUT    MYSEAT
        BSTR,UN TIDX
        LODA,R0 TSPH,R3
        CPUT    ST+OFF_PHASE
        BSTR,UN TIDX
        LODA,R0 TSSCR,R3
        RETC,UN
; TIDX: R3 = the scene's index in the tables (4 on; past them: 4's).
; Leaf. Clobbers R0.
TIDX:   LODA,R0 *PTSCN
        SUBI,R0 4
        COMI,R0 TSN
        BCTR,LT TIX1
        EORZ    R0
TIX1:   STRZ    R3
        RETC,UN
TSPH:   DB      PH_ROUNDEND,PH_GAMEOVER,PH_WAIT,PH_AIM,PH_SHOT,PH_DEFENSE,PH_SHOP
TSSCR:  DB      S_RESULTS,S_OVER,S_WAIT,S_FIELD,S_FIELD,S_FIELD,S_SHOP
TSN     EQU     7

; TTICK: the timer a second down every 64 frames (45 again at 0): a new
; state.
TTICK:  LODA,R0 FRML
        ANDI,R0 63
        COMA,R0 DT
        BCFR,LT TTI0
        LODA,R0 ST+OFF_TIMER
        SUBI,R0 1
        BCFR,LT TTI1
        LODI,R0 45
TTI1:   CPUT    ST+OFF_TIMER
        LODI,R0 1
        RETC,UN
TTI0:   EORZ    R0
        RETC,UN

; TFLD: the field's scenes on the fixture's seats (p_test.inc TFIX made
; every seat a bot at its tank): ours (seat 0) a person, alive, linked (the
; defence window: PF_DEFENSE too), the scene's phase, our turn; scene 8 its
; segment into SCRIPT. TMARK: 1 if fixed B's loop is to play it.
TFLD:   BSTR,UN TIDX
        LODA,R0 TSPH,R3
        CPUT    ST+OFF_PHASE
        EORZ    R0
        CPUT    ST+OFF_TURN
        EORZ    R0
        CPUT    MYSEAT
        LODI,R0 KIND_HUMAN
        CPUT    ST+OFF_SEATS+P_KIND
        LODI,R0 PF_ALIVE|PF_CONN
        STRA,R0 T0
        LODA,R0 *PTSCN
        COMI,R0 9
        BCFR,EQ TFL1
        LODI,R0 PF_ALIVE|PF_CONN|PF_DEFENSE
        STRA,R0 T0
TFL1:   LODA,R0 T0
        CPUT    ST+OFF_SEATS+P_FLAGS
        EORZ    R0
        CPUT    TMARK
        LODA,R0 *PTSCN
        COMI,R0 8
        RETC,LT
        RETC,GT
        CPTR    SCRIPT          ; scene 8: its segment
        LODI,R3 0
TFL2:   LODA,R0 TSEG8,R3
        BSTA,UN RAMPUT1
        ADDI,R3 1
        COMI,R3 TSEG8N
        BCFR,EQ TFL2
        BSTA,UN RAMPEND
        LODI,R0 TSEG8N
        STRA,R0 SEGLEN
        EORZ    R0
        CPUT    SEGNO
        LODI,R0 S_LAST_SEG
        CPUT    SEGFLG
        LODI,R0 1
        CPUT    TMARK
        RETC,UN

; scene 8's segment: a talk long enough to scroll (with marks the field
; lacks), the wind turning, a hit, a shield, a short talk, its time out
        ASCII
TSEG8:  DB      OP_TALK,1,33,"I'M OKAY! ... NO I'M NOT. REALLY?"
        DB      OP_WAIT,60
        DB      OP_WIND,250&$FF,250>>8
        DB      OP_WAIT,20
        DB      OP_DAMAGE,2,30,70
        DB      OP_WAIT,60
        DB      OP_SHIELD,1,SH_HIT,55
        DB      OP_WAIT,60
        DB      OP_TALK,5,9,"MOM? 100%"
        DB      OP_WAIT,30
        DB      OP_END
TSEG8N  EQU     $-TSEG8
        GLYPHS

        INCLUDE "state.inc"
