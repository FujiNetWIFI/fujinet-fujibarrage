; p_play.inc -- the field (src/play.c): entered (the screen in block
; graphics, all of the terrain asked for), a new state (the tanks that
; moved, our turn, the HUD), a frame (the tanks again after terrain, the
; HUD, aiming: the disc's left and right the angle, up and down the power,
; Enter the next weapon, FIRE), the vblank's (the sprites).
;
; The screen (equ.inc, look.inc; tools/arc/mock_arc.py's Mock.build, look
; B): rows 0-23 the field, 48x48 blocks; the decoder draws the terrain's
; columns into it as they come (p_net.inc PTERR, fld.inc). The tanks: the
; 4 sprites show our own, the turn's, and the next seats to fire after it
; (every tank with 4 or fewer on the field), in their seats' colours, the
; barrel at the angle (tools/arc/mktank_arc.py's pictures, UDC $38-$3B);
; the rest are 2x1 HOT blocks on the field, their cells kept (TBC) to be
; put back (TKOFF, fixed B). The terrain's columns and the tanks are drawn
; by p_fld.inc (its own page: the decoder's and M3's too). Rows 24-25 the
; HUD, the items line, the defence window, YOUR TURN! and the messages:
; p_hud.inc's (a frame's E_UI says whether the disc and keys are aiming).
; Our tank (a sprite) blinks yellow while we aim, a guidance target too.
;
; The shot's segments are the PLY page's (p_ply.inc): main.inc plays each
; there; the field after the shot is what the script made it.

        BCTA,UN PENTER          ; E_ENTER
        BCTA,UN PSTATE          ; E_STATE
        BCTA,UN PTICK           ; E_TICK
        BCTA,UN PVBL            ; E_VBL: the sprites, in the vblank

GLT     EQU     $3C             ; the field's user glyphs: '<' '>' (UDC 4, 5)
GGT     EQU     $3D

; ---- entered --------------------------------------------------------------------------
; PENTER: the field's screen: all sky, the HUD's rows text; the mode (block
; graphics, 26 rows, multicolour, centred); no tank drawn; all of the
; terrain asked for (a text screen had the field's RAM, and the terrain
; that came meanwhile was dropped: RESYNC with tv FF); then the state.
PENTER: LODI,R0 CSKY            ; rows 0-12
        LODI,R3 13*16
PE1:    STRA,R0 SCR-1,R3
        BDRR,R3 PE1
        LODI,R0 GTEXT           ; the HUD's rows: text, blank
        LODI,R3 32
PE2:    STRA,R0 SCR2+(HUDROW-13)*16-1,R3
        BDRR,R3 PE2
        LODI,R0 CSKY            ; rows 13-23
        LODI,R3 (HUDROW-13)*16
PE3:    STRA,R0 SCR2-1,R3
        BDRR,R3 PE3
        LODI,R3 32              ; '<' '>' '!' '?'
PE4:    LODA,R0 ARROWS-1,R3
        STRA,R0 UDC+32-1,R3
        BDRR,R3 PE4
        LODI,R0 FGFXMOD
        STRA,R0 GFXMOD
        LODI,R0 FVPOS
        STRA,R0 VPOS
        LODI,R0 FBGCOL
        CPUT    BGCB
        LODI,R0 FBGCOL
        STRA,R0 BGCOL
        LODA,R0 FLAGS           ; (no script under way on a new field)
        IORI,R0 FL_FIELD
        ANDI,R0 ~FL_INSCR&$FF
        CPUT    FLAGS
        LODI,R0 $80             ; multicolour (the sound keeps it: PITW)
        STRA,R0 PITCH
        LODI,R0 $FF             ; all of the terrain again
        CPUT    NTV
        LODA,R0 RHOLD
        IORI,R0 $80
        STRA,R0 RHOLD
        EORZ    R0              ; no tank drawn, no 3-kind cell kept
        CPUT    NTERR
        EORZ    R0
        CPUT    C3N
        EORZ    R0
        CPUT    TBN
        CPTR    TKX
        LODI,R3 SEATS
PE5:    LODI,R0 $FF
        BSTA,UN RAMPUT1
        BDRR,R3 PE5
        BSTA,UN RAMPEND
        CPTR    TALKT           ; no message, the field's UI shut
        LODI,R3 4
PE5A:   EORZ    R0
        BSTA,UN RAMPUT1
        BDRR,R3 PE5A
        BSTA,UN RAMPEND
        FARPG   PG_HUD,E_UIOFF
        EORZ    R0
        STRA,R0 MYTURN
        STRA,R0 HELDN
        LODI,R0 NONE
        LODI,R3 4
PE6:    STRA,R0 SPRSEL-1,R3
        BDRR,R3 PE6
; PSTATE: a new state: the tanks as it has them, our turn, the HUD.
PSTATE: FARPG   PG_FLD,E_TANKS  ; (p_fld.inc TKSTATE)
        BSTA,UN ISTURN
        COMA,R0 MYTURN
        BCTR,EQ PS1
        STRA,R0 MYTURN
        COMI,R0 0
        BCTR,EQ PS1
        BSTA,UN AIMINIT         ; our turn begins: the UI fresh, YOUR TURN!,
        FARPG   PG_HUD,E_UIOFF  ;  the cue (a message cut short)
        LODI,R0 BANNERJ
        CPUT    TURNJ
        EORZ    R0
        CPUT    TALKT
        SND     UI_TURN
PS1:    LODA,R0 FDIRTY
        IORI,R0 FD_HUD|FD_SPR
        STRA,R0 FDIRTY
        RETC,UN

ARROWS: DB      $00,$04,$08,$10,$20,$10,$08,$04 ; <
        DB      $00,$20,$10,$08,$04,$08,$10,$20 ; >
        DB      $00,$08,$08,$08,$08,$08,$00,$08 ; ! (talk)
        DB      $00,$1C,$22,$02,$04,$08,$00,$08 ; ?

; ISTURN: R0 = 1 if we aim now: the phase AIM, our seat alive, the turn
; ours (or everyone's and we're not done). Leaf. Clobbers R0, R3.
ISTURN: LODA,R0 ST+OFF_PHASE
        COMI,R0 PH_AIM
        BCFR,EQ IST0
        LODA,R3 MYSEAT
        COMI,R3 SEATS
        BCFR,LT IST0
        LODA,R0 SEAT25,R3
        STRZ    R3
        LODA,R0 ST+OFF_SEATS+P_FLAGS,R3
        TMI,R0  PF_ALIVE
        BCFR,EQ IST0
        LODA,R3 ST+OFF_TURN
        COMA,R3 MYSEAT
        BCTR,EQ IST1
        COMI,R3 NO_SEAT
        BCFR,EQ IST0
        TMI,R0  PF_DONE
        BCTR,EQ IST0
IST1:   LODI,R0 1
        RETC,UN
IST0:   EORZ    R0
        RETC,UN

; AIMINIT: our aim from the state (the server's: where we left it). Leaf.
; Clobbers R0, R3.
AIMINIT: LODA,R3 MYSEAT
        LODA,R0 SEAT25,R3
        STRZ    R3
        LODA,R0 ST+OFF_SEATS+P_ANGLE,R3
        STRA,R0 AIMA
        LODA,R0 ST+OFF_SEATS+P_POWER,R3
        STRA,R0 AIMPL
        LODA,R0 ST+OFF_SEATS+P_POWER+1,R3
        STRA,R0 AIMPH
        LODA,R0 ST+OFF_SEATS+P_WEAPON,R3
        STRA,R0 WEAPON
        LODA,R0 FRML
        STRA,R0 SENTT
        EORZ    R0
        STRA,R0 HELDN
        RETC,UN

; ---- the sprites (the vblank's) ---------------------------------------------------------
; PVBL: if they changed, the 4 sprites: each its seat's tank (SPRSEL) at
; TKX/TKY, its seat's colour, its picture at its angle (ours, aiming:
; AIMA); or off. Sprite n: Y = 256 - its top line (FTOP + 4y - 8), X = its
; left pixel + 43 (TANKX: the tank's middle on its column). The positions
; and colours first, the pictures after (the vblank is short: run late, a
; barrel is a frame behind, never a tank). Calls one down. Clobbers R0-R3,
; T0-T2.
PVBL:   LODA,R0 FDIRTY
        TMI,R0  FD_SPR
        RETC,LT
        ANDI,R0 ~FD_SPR&$FF
        STRA,R0 FDIRTY
        LODI,R3 3*2             ; R3: sprite n's registers (2n)
PV1:    LODZ    R3
        RRR,R0
        STRA,R0 T0
        LODA,R1 T0
        LODA,R0 SPRSEL,R1       ; its seat
        COMI,R0 SEATS
        BCTR,LT PV2
        EORZ    R0              ; off
        STRA,R0 SPRPOS,R3
        STRA,R0 SPRPOS+1,R3
        BCTR,UN PV3
PV2:    STRZ    R1
        LODA,R0 TKY,R1          ; Y = 256 - (FTOP + 4y - 8)
        ANDI,R0 $7F
        RRL,R0
        RRL,R0
        STRZ    R2
        LODI,R0 256-FTOP+8
        SUBZ    R2
        STRA,R0 SPRPOS,R3
        LODA,R0 TKX,R1
        STRZ    R1
        LODA,R0 TANKX,R1
        STRA,R0 SPRPOS+1,R3
PV3:    SUBI,R3 2
        BCFR,LT PV1
        LODI,R0 NONE            ; T1, T2: the seats blinking yellow now
        STRA,R0 T1
        STRA,R0 T2
        LODA,R0 FRML
        ANDI,R0 16
        BCTR,EQ PV3B
        LODA,R0 MYTURN          ; (only while we aim: AIMV is the shot's
        BCTR,EQ PV3B            ;  BEAMS while one plays)
        LODA,R0 MYSEAT
        STRA,R0 T1
        LODA,R0 *PIMENU
        COMI,R0 M_TARGET
        BCFR,EQ PV3B
        LODA,R0 *PTARGT
        STRA,R0 T2
PV3B:   LODI,R3 0               ; the colours
        BSTA,UN SPRCOL
        STRA,R0 SPRC01
        LODI,R3 2
        BSTA,UN SPRCOL
        STRA,R0 SPRC23
        LODI,R0 3               ; the pictures (T0: the sprite)
PV4:    STRA,R0 T0
        STRZ    R1
        LODA,R0 SPRSEL,R1
        COMI,R0 SEATS
        BCFA,LT PV9
        STRZ    R1              ; its angle: ours while we aim, as aimed
        LODA,R0 TKA,R1
        STRA,R0 T2
        LODA,R0 MYTURN
        BCTR,EQ PV5
        LODZ    R1
        COMA,R0 MYSEAT
        BCFR,EQ PV5
        LODA,R0 AIMA
        STRA,R0 T2
PV5:    LODA,R1 T2              ; R2: its picture's rows (TANKI, 180 at most)
        COMI,R1 181
        BCTR,LT PV6
        LODI,R1 180
PV6:    LODA,R0 TANKI,R1
        STRZ    R2
        LODA,R0 T0              ; R3: sprite n's UDC (8n)
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R3
        LODI,R1 6
PV8:    LODA,R0 TANKP,R2
        STRA,R0 UDC,R3
        ADDI,R2 1
        ADDI,R3 1
        BDRR,R1 PV8
        LODI,R0 $7E             ; the body
        STRA,R0 UDC,R3
        STRA,R0 UDC+1,R3
PV9:    LODA,R0 T0
        SUBI,R0 1
        BCFA,LT PV4
        RETC,UN
; SPRCOL: sprites R3 and R3 + 1's colour byte. Calls one down. Clobbers
; R0-R2.
SPRCOL: BSTR,UN SEATCL
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R2
        ADDI,R3 1
        BSTR,UN SEATCL
        IORZ    R2
        IORI,R0 $C0
        RETC,UN
; SEATCL: R0 = the colour of sprite R3's seat (0 if none): seat mod 5's;
; yellow (the 16 frames on of 32) for ours while we aim and a guidance
; target being chosen (PVBL's T1, T2). Leaf. Clobbers R1.
SEATCL: LODA,R0 SPRSEL,R3
        COMI,R0 SEATS
        BCTR,LT SCL0
        EORZ    R0
        RETC,UN
SCL0:   COMA,R0 T1
        BCTR,EQ SCLY
        COMA,R0 T2
        BCFR,EQ SCL1
SCLY:   LODI,R0 CYELLOW
        RETC,UN
SCL1:   COMI,R0 5
        BCTR,LT SCL2
        SUBI,R0 5
SCL2:   STRZ    R1
        LODA,R0 SEATC,R1
        RETC,UN
SEATC:  DB      CWHITE,CCYAN,CGREEN,CMAGEN,CBLUE

        INCLUDE "tank.inc"      ; TANKI, TANKP, TANKX (tools/arc/mktank_arc.py)

; ---- a frame -----------------------------------------------------------------------------
; PTICK: the block tanks again if terrain came (its columns took them
; off), the HUD if asked, and on our turn the disc and keys aim.
PTICK:  LODA,R0 NTERR
        BCTR,EQ PT1
        EORZ    R0
        CPUT    NTERR
        FARPG   PG_FLD,E_TKDRAW
PT1:    FARPG   PG_HUD,E_UI     ; the HUD, the items line, the defence window
        LODA,R0 FDIRTY          ; a guidance target: the sprites again
        TMI,R0  FD_SEL
        BCFR,EQ PT2
        ANDI,R0 ~FD_SEL&$FF
        STRA,R0 FDIRTY
        FARPG   PG_FLD,E_TKSEL
        RETC,UN
PT2:    TMI,R2  UF_AIM          ; (R2: E_UI's; the items line has the keys)
        RETC,LT
        LODA,R0 MYTURN
        RETC,EQ
; AIM: our turn: the disc (a step, more the longer it's held: 1, 2 past 15
; jiffies, 5 past 45): left and right the angle (left turns the barrel
; left: up), up and down the power (5 a step); Enter the next weapon we
; have; FIRE fires; our aim to the server every 8 frames while it isn't
; what the state has (ACT_AIM: the others see the barrel move).
AIM:    LODA,R0 INDIR
        BCFR,EQ AM1
        STRA,R0 HELDN
        BCTA,UN AM6
AM1:    LODA,R0 HELDN           ; (jiffies: PAL's 50 frames as long)
        ADDA,R0 DT
        TPSL    PSLC
        BCFR,EQ AM1A
        LODI,R0 255
AM1A:   STRA,R0 HELDN           ; R1: the step; held 16 jiffies or more, the
        LODI,R1 1               ;  step a jiffy (times DT: a long frame, or
        COMI,R0 16              ;  PAL's, moves as far)
        BCTR,LT AM2
        LODI,R1 2
        COMI,R0 46
        BCTR,LT AM1B
        LODI,R1 5
AM1B:   LODA,R2 DT
        EORZ    R0
AM1C:   ADDZ    R1
        BDRR,R2 AM1C
        STRZ    R1
AM2:    LODA,R0 INDIR
        TMI,R0  D_LEFT
        BCFR,EQ AM3
        LODA,R0 AIMA
        ADDZ    R1
        COMI,R0 181
        BCTR,LT AM2A
        LODI,R0 180
AM2A:   STRA,R0 AIMA
AM3:    LODA,R0 INDIR
        TMI,R0  D_RIGHT
        BCFR,EQ AM4
        LODA,R0 AIMA
        COMZ    R1
        BCFR,LT AM3A
        LODZ    R1
AM3A:   SUBZ    R1
        STRA,R0 AIMA
AM4:    LODZ    R1              ; R1: the power's step (5 x)
        RRL,R0
        RRL,R0
        ADDZ    R1
        STRZ    R1
        LODA,R0 INDIR
        TMI,R0  D_UP
        BCFR,EQ AM4B
        LODA,R0 AIMPL           ; up: + the step, at most 1000
        ADDZ    R1
        STRA,R0 AIMPL
        TPSL    PSLC
        BCFR,EQ AM4A
        LODA,R0 AIMPH
        ADDI,R0 1
        STRA,R0 AIMPH
AM4A:   LODA,R0 AIMPH
        COMI,R0 1000>>8
        BCTR,LT AM5
        BCTR,GT AM4M
        LODA,R0 AIMPL
        COMI,R0 1000&$FF
        BCFR,GT AM5
AM4M:   LODI,R0 1000>>8
        STRA,R0 AIMPH
        LODI,R0 1000&$FF
        STRA,R0 AIMPL
        BCTR,UN AM5
AM4B:   LODA,R0 INDIR
        TMI,R0  D_DOWN
        BCFR,EQ AM5
        LODA,R0 AIMPL           ; down: - the step, at least 0
        COMZ    R1
        BCFR,LT AM4D
        LODA,R0 AIMPH           ; (a borrow)
        BCFR,EQ AM4C
        STRA,R0 AIMPL           ;  (none to borrow: 0)
        BCTR,UN AM5
AM4C:   SUBI,R0 1
        STRA,R0 AIMPH
        LODA,R0 AIMPL
AM4D:   SUBZ    R1
        STRA,R0 AIMPL
AM5:    LODA,R0 FDIRTY          ; (the HUD, our barrel)
        IORI,R0 FD_HUD|FD_SPR
        STRA,R0 FDIRTY
AM6:    LODA,R0 EVKEY           ; Enter: the next weapon
        COMI,R0 K_ENT
        BCFR,EQ AM7
        BSTA,UN NEXTW
AM7:    LODA,R0 EVBTN           ; FIRE
        BCFA,EQ PFIRE
        LODA,R0 FRML            ; our aim: every 8 frames, if the state's isn't
        SUBA,R0 SENTT
        COMI,R0 8
        RETC,LT
        LODA,R3 MYSEAT
        LODA,R0 SEAT25,R3
        STRZ    R3
        LODA,R0 ST+OFF_SEATS+P_ANGLE,R3
        COMA,R0 AIMA
        BCFR,EQ AM8
        LODA,R0 ST+OFF_SEATS+P_POWER,R3
        COMA,R0 AIMPL
        BCFR,EQ AM8
        LODA,R0 ST+OFF_SEATS+P_POWER+1,R3
        COMA,R0 AIMPH
        RETC,EQ
AM8:    LODA,R0 FRML
        STRA,R0 SENTT
        BSTR,UN AARGAP          ; angle, power16
        BSTA,UN RAMPEND
        LODI,R1 ACT_AIM
        LODI,R2 3
        ZBSR    *ZACTQ
        RETC,UN
; AARGAP: AARGS: our angle and power (LE), the stream left open. Calls one
; down. Clobbers R0-R3.
AARGAP: CPTR    AARGS
        LODA,R0 AIMA
        BSTA,UN RAMPUT1
        LODA,R0 AIMPL
        BSTA,UN RAMPUT1
        LODA,R0 AIMPH
        BCTA,UN RAMPUT1         ; tail call
; PFIRE: angle, power16, weapon, the guidance and its target (the items
; line's: none unless chosen); no more aiming; our tank's barrel stays
; where we fired.
PFIRE:  BSTR,UN AARGAP
        LODA,R0 WEAPON
        BSTA,UN RAMPUT1
        LODA,R0 *PGUIDE         ; the items line's guidance and target
        BSTA,UN RAMPUT1
        LODA,R0 *PTARGT
        BSTA,UN RAMPUT1
        BSTA,UN RAMPEND
        LODI,R0 I_NONE          ; (spent)
        CPUT    GUIDE
        LODI,R0 NO_SEAT
        CPUT    TARGET
        LODI,R1 ACT_FIRE
        LODI,R2 6
        ZBSR    *ZACTQ
        SND     UI_GO
        EORZ    R0
        STRA,R0 MYTURN
        LODA,R0 AIMA
        LODA,R3 MYSEAT
        CPUTR   TKA
        LODA,R0 FDIRTY
        IORI,R0 FD_HUD|FD_SPR
        STRA,R0 FDIRTY
        RETC,UN
; NEXTW: the next weapon we have (the mini missile always), ACT_WEAPON.
; Calls two down. Clobbers R0-R3.
NEXTW:  LODA,R0 WEAPON
        LODI,R1 N_WEAPONS
NW1:    ADDI,R0 1
        COMI,R0 N_WEAPONS
        BCTR,LT NW2
        EORZ    R0
NW2:    BCTR,EQ NW3             ; (I_MINI_MISSILE)
        STRZ    R3
        LODA,R0 ST+OFF_INV,R3
        BCFR,EQ NW2A
        LODZ    R3
        BDRR,R1 NW1
        RETC,UN
NW2A:   LODZ    R3
NW3:    STRA,R0 WEAPON
        CPUT    AARGS
        LODA,R0 FDIRTY
        IORI,R0 FD_HUD
        STRA,R0 FDIRTY
        SND     UI_SELECT
        LODI,R1 ACT_WEAPON
        LODI,R2 1
        ZBSR    *ZACTQ
        RETC,UN
        IF      I_MINI_MISSILE <> 0
        ERROR   "NEXTW: the mini missile is weapon 0"
        ENDIF

; RAM-H, through pointer words
PGUIDE: DWBE    GUIDE
PTARGT: DWBE    TARGET
