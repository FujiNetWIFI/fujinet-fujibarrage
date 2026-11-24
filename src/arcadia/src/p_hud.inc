; p_hud.inc -- the field's two HUD rows and what takes them over (src/
; play.c's hud, items line, defence window, your-turn cues and talk): its
; own page, reached by FARPG from the field's page (E_UI, E_HUD, E_UIOFF)
; and from the shot player's (E_HUD, E_UIOFF; the messages are p_msg.inc's). PGFAR is one level, so
; nothing here calls another page: the names come from fixed B (names.inc
; SN7, SN9), the rest is fixed A's. When the target's sprite must change,
; FDIRTY's FD_SEL asks the field's page to choose the sprites again.
;
; The rows (cells 1-15; cell 0 is the row's GTEXT):
;   24  name(6) A angle(3) power(" Pnnn"/"P1000")   the HUD seat's (ours
;       while we aim); YOUR TURN! flashing (BANNERJ or the first input);
;       the defence window: SET UP DEFENCES / ENTER DONE nn, flashing
;   25  weapon(7) count(2) wind(<> 3) timer(2); or the items line:
;         an item:   name(9) count(2) ON (the parachute, the fuse)
;         a target:  TARGET name(8)
;         driving:   DRIVE FUEL nnn
; A message (TALKT frames; the HUD waits): a talk (OP_TALK: the name, then
; its text typed a letter a step over both rows, scrolling up when it
; fills them; all of it at the segment's end: SCRIPT's bytes go), or an
; event line over row 24 (OP_DAMAGE: name E energy HIT; OP_SHIELD: name,
; what the shield did, its hp).
;
; The items line (Select or keypad 1 on our turn or in the defence
; window): the disc's left and right pick an item we have, FIRE uses it
; (ACT_USE); guidance asks for a target next (left and right, its tank a
; sprite blinking yellow; FIRE takes it: ACT_FIRE carries both); fuel
; drives (held left or right: ACT_DRIVE every 8 frames; FIRE stops).
; Select, 1 or Clear shuts it (Clear otherwise leaves: main.inc). The
; Defense Computer works by itself: not listed. The defence window opens
; it by itself on the defences alone; Enter is done (ACT_DONE).
; Its state: AIMV (ram.inc: the shot's BEAMS, dead while no shot is under
; way), the message's TALK* (RAM-H).

        BCTA,UN UIFRAME         ; E_UI: a frame of the field's UI (R2 UF_*)
        BCTA,UN HUDQ            ; E_HUD: the HUD's rows, if asked and no message
        BCTA,UN UIOFF           ; E_UIOFF

REMINDJ EQU     600             ; idle this long on our turn: the cue again
FBANG   EQU     $3E             ; the field's user glyphs (p_play.inc PENTER):
FLT     EQU     $3C             ;  '<' '>' (the wind), '!' '?' (talk)
FGT     EQU     $3D
HUDR24  EQU     SCR2+(HUDROW-13)*16     ; the HUD's rows in screen RAM
HUDR25  EQU     HUDR24+16

; RAM-H, through pointer words (page 0 code)
PITEM:  DWBE    ITEM
PGUIDE: DWBE    GUIDE
PTARGET: DWBE   TARGET
PDRIVEN: DWBE   DRIVEN
PTURNJ: DWBE    TURNJ
PIDLEL: DWBE    IDLEL
PIDLEH: DWBE    IDLEH
PDEFOPN: DWBE   DEFOPN
PTALKT: DWBE    TALKT
PSN7:   DWBE    SN7
PSN9A:  DWBE    SN9
PSN9B:  DWBE    SN9B

; ---- a frame ------------------------------------------------------------------------
; UIFRAME: the message's time (between a shot's segments, after one), the
; defence window opening, the turn's banner and reminder, the blinks; then
; the items line or the defence window's keys; the HUD if asked. R2 =
; UF_AIM if the disc and keys are the aim's this frame. Calls four down.
; Clobbers R0-R3, T0-T3.
UIFRAME: LODA,R0 *PTALKT        ; the message's time (eighths of a second)
        BCTR,EQ UI1
        LODA,R0 FRML
        ANDI,R0 7
        COMA,R0 DT
        BCFR,LT UI1
        LODA,R0 *PTALKT
        SUBI,R0 1
        CPUT    TALKT
        LODA,R0 *PTALKT
        BCFR,EQ UI1
        BSTA,UN DIRTYH
UI1:    LODA,R0 FLAGS           ; a shot under way (between its segments):
        TMI,R0  FL_INSCR        ;  AIMV is its BEAMS, the HUD alone
        BCTA,EQ UIX0
        BSTA,UN DEFNG           ; the defence window: opened once
        COMI,R0 0
        BCFR,EQ UI2
        LODA,R0 *PDEFOPN
        BCTR,EQ UI3
        EORZ    R0
        CPUT    DEFOPN
        BCTR,UN UI3
UI2:    LODA,R0 *PDEFOPN
        BCFR,EQ UI2B
        LODI,R0 1
        CPUT    DEFOPN
        SND     UI_TURN
        LODI,R0 N_ITEMS-1       ; its items line, on the first defence we have
        LODI,R1 1
        BSTA,UN NEXTI
        COMI,R0 I_NONE
        BCTR,EQ UI2A
        BSTA,UN IOPEN2
UI2A:   BSTA,UN DIRTYH
UI2B:   BSTA,UN FLASHQ          ; (its prompt flashes)
UI3:    LODA,R0 MYTURN          ; our turn: the banner, the reminder
        BCTA,EQ UI5
        BSTA,UN FLASHQ          ; (our tank blinks; the banner)
        BCFR,EQ UI3A
        LODA,R0 FDIRTY
        IORI,R0 FD_SPR
        STRA,R0 FDIRTY
UI3A:
        LODA,R0 INDIR           ; any input: the banner over, idle again
        IORA,R0 EVBTN
        BCFR,EQ UI3B
        LODA,R0 EVKEY
        COMI,R0 K_NONE
        BCTR,EQ UI4
UI3B:   LODA,R0 *PTURNJ
        BCTR,EQ UI3C
        EORZ    R0
        CPUT    TURNJ
        BSTA,UN DIRTYH
UI3C:   LODA,R0 *PIDLEL
        IORA,R0 *PIDLEH
        BCTA,EQ UI5
        EORZ    R0
        CPUT    IDLEL
        EORZ    R0
        CPUT    IDLEH
        BCTA,UN UI5
UI4:    LODA,R0 *PTURNJ         ; the banner's time
        BCTR,EQ UI4B
        COMA,R0 DT
        BCTR,GT UI4A
        EORZ    R0
        CPUT    TURNJ
        BSTA,UN DIRTYH
        BCTR,UN UI4B
UI4A:   SUBA,R0 DT
        CPUT    TURNJ
UI4B:   LODA,R0 *PIDLEL         ; idle: the cue again at REMINDJ
        ADDA,R0 DT
        CPUT    IDLEL
        TPSL    PSLC            ; (CPUT keeps the PSL)
        BCFR,EQ UI4C
        LODA,R0 *PIDLEH
        ADDI,R0 1
        CPUT    IDLEH
UI4C:   LODA,R0 *PIDLEH
        COMI,R0 REMINDJ>>8
        BCTR,LT UI5
        LODA,R0 *PIDLEL
        COMI,R0 REMINDJ&$FF
        BCTR,LT UI5
        EORZ    R0
        CPUT    IDLEL
        EORZ    R0
        CPUT    IDLEH
        SND     UI_TURN
UI5:    LODA,R0 *PIMENU        ; a target: its tank blinks
        COMI,R0 M_TARGET
        BCFR,EQ UI5A
        BSTA,UN FLASHQ
        BCFR,EQ UI5A
        LODA,R0 FDIRTY
        IORI,R0 FD_SPR
        STRA,R0 FDIRTY
UI5A:   BSTA,UN DEFNG           ; neither aiming nor defending: the line shut
        STRA,R0 T3              ; (T3: defending)
        LODA,R0 MYTURN
        IORA,R0 T3
        BCFR,EQ UI6
        LODA,R0 *PIMENU
        BCTA,EQ UIX0
        BSTA,UN ICLOSE
        BCTA,UN UIX0
UI6:    LODA,R0 T3              ; the defence window: Enter is done
        BCTR,EQ UI7
        LODA,R0 EVKEY
        COMI,R0 K_ENT
        BCFR,EQ UI7
        BSTA,UN ICLOSE
        LODI,R1 ACT_DONE
        LODI,R2 0
        ZBSR    *ZACTQ
        SND     UI_GO
        BCTA,UN UIX0
UI7:    LODA,R0 *PIMENU        ; the items line has the disc and the keys
        BCTR,EQ UI8
        BSTA,UN IFRAME
        BCTA,UN UIX0
UI8:    LODA,R0 EVKEY           ; Select or 1: it opens
        COMI,R0 K_SEL
        BCTR,EQ UI8A
        COMI,R0 K_ITEMS
        BCFR,EQ UI9
UI8A:   BSTA,UN IOPEN
        BCTA,UN UIX0
UI9:    LODA,R0 T3              ; defending: nothing else
        BCTA,GT UIX0
        BSTA,UN HUDQ            ; aiming
        LODI,R2 UF_AIM
        RETC,UN
UIX0:   BSTA,UN HUDQ
        LODI,R2 0
        RETC,UN

; FLASHQ: CC EQ if the flashes (16 frames on, 16 off) changed since the
; last frame (FRML mod 16 < DT); then the HUD too. Leaf. Clobbers R0.
FLASHQ: LODA,R0 FRML
        ANDI,R0 15
        COMA,R0 DT
        BCTR,LT FLQ1
        LODI,R0 1               ; (not EQ)
        RETC,UN
FLQ1:   LODA,R0 FDIRTY
        IORI,R0 FD_HUD
        STRA,R0 FDIRTY
        EORZ    R0              ; (EQ)
        RETC,UN

; DIRTYH: the HUD to be drawn again. Leaf. Clobbers R0.
DIRTYH: LODA,R0 FDIRTY
        IORI,R0 FD_HUD
        STRA,R0 FDIRTY
        RETC,UN

; DEFNG: R0 = 1 if our defence window is open (play.c defending): the
; phase DEFENSE, a seat, PF_DEFENSE and not PF_DONE. Leaf. Clobbers R0, R3.
DEFNG:  LODA,R0 ST+OFF_PHASE
        COMI,R0 PH_DEFENSE
        BCFR,EQ DFN0
        LODA,R3 MYSEAT
        COMI,R3 SEATS
        BCFR,LT DFN0
        LODA,R0 SEAT25,R3
        STRZ    R3
        LODA,R0 ST+OFF_SEATS+P_FLAGS,R3
        ANDI,R0 PF_DEFENSE|PF_DONE
        COMI,R0 PF_DEFENSE
        BCFR,EQ DFN0
        LODI,R0 1
        RETC,UN
DFN0:   EORZ    R0
        RETC,UN

; UIOFF: the field's UI as at a turn's start: the items line shut, no
; guidance, no target, no banner, nothing idle (AIMV zeroed, then GUIDE and
; TARGET none). Calls one down. Clobbers R0-R3.
UIOFF:  CPTR    AIMV
        LODI,R3 AIMVN
UIO1:   EORZ    R0
        BSTA,UN RAMPUT1
        BDRR,R3 UIO1
        BSTA,UN RAMPEND
        LODI,R0 I_NONE
        CPUT    GUIDE
        LODI,R0 NO_SEAT
        CPUT    TARGET
        RETC,UN
        IF      (GUIDE <> AIMV+2) || (TARGET <> AIMV+3)
        ERROR   "UIOFF: GUIDE, TARGET moved"
        ENDIF

; ---- the items line ----------------------------------------------------------------------
; IOPEN: open on the first item we have (none: the error beep). IOPEN2: on
; item R0. Calls two down. Clobbers R0-R3, T1-T3.
IOPEN:  LODI,R0 N_ITEMS-1
        LODI,R1 1
        BSTA,UN NEXTI
        COMI,R0 I_NONE
        BCFR,EQ IOPEN2
        SND     UI_ERROR
        RETC,UN
IOPEN2: CPUT    ITEM
        LODI,R0 M_ITEM
        CPUT    IMENU
        SND     UI_SELECT
        BCTA,UN DIRTYH
; ICLOSE: shut (a target's sprite back to the usual choice). Calls two
; down. Clobbers R0-R3.
ICLOSE: LODA,R0 *PIMENU
        COMI,R0 M_TARGET
        BCFR,EQ ICL1
        LODA,R0 FDIRTY
        IORI,R0 FD_SEL
        STRA,R0 FDIRTY
ICL1:   EORZ    R0
        CPUT    IMENU
        BCTA,UN DIRTYH

; IFRAME: a frame of the open line (play.c items_frame). Calls three down.
; Clobbers R0-R3, T0-T3.
IFRAME: LODA,R0 EVKEY           ; Select, 1 or Clear: shut
        COMI,R0 K_SEL
        BCTA,EQ ICLOSE
        COMI,R0 K_ITEMS
        BCTA,EQ ICLOSE
        COMI,R0 K_CLR
        BCTA,EQ ICLOSE
        LODA,R0 PRVDIR          ; T0: a push's step (1, $FF; 0 none)
        EORI,R0 $FF
        ANDA,R0 INDIR
        LODI,R1 0
        TMI,R0  D_LEFT
        BCFR,EQ IF0
        LODI,R1 $FF
IF0:    TMI,R0  D_RIGHT
        BCFR,EQ IF0A
        LODI,R1 1
IF0A:   STRA,R1 T0
        LODA,R0 *PIMENU
        COMI,R0 M_TARGET
        BCTA,EQ IFTGT
        COMI,R0 M_DRIVE
        BCTA,EQ IFDRV
        LODA,R1 T0              ; the items: a step
        BCTR,EQ IF1
        LODA,R0 *PITEM
        BSTA,UN NEXTI
        COMI,R0 I_NONE
        RETC,EQ
        CPUT    ITEM
        SND     UI_TICK
        BCTA,UN DIRTYH
IF1:    LODA,R0 EVBTN           ; FIRE: use it
        RETC,EQ
        SND     UI_SELECT
        BSTA,UN DEFNG
        COMI,R0 0
        BCFA,EQ IF3             ; (defending: used)
        LODA,R0 *PITEM
        COMI,R0 I_AUTOPILOT+1   ; guidance: its target next
        BCFR,LT IF2
        CPUT    GUIDE
        LODA,R0 *PTARGET
        LODI,R1 1
        BSTA,UN NEXTT
        CPUT    TARGET
        LODI,R0 M_TARGET
        CPUT    IMENU
        LODA,R0 FDIRTY
        IORI,R0 FD_SEL|FD_HUD
        STRA,R0 FDIRTY
        RETC,UN
IF2:    COMI,R0 I_FUEL          ; fuel: driving
        BCFR,EQ IF3
        LODI,R0 M_DRIVE
        CPUT    IMENU
        EORZ    R0
        CPUT    DRIVEN
        BCTA,UN DIRTYH
IF3:    LODA,R0 *PITEM          ; the rest: used now (the parachute and the
        CPUT    AARGS           ;  impact fuse switch: the line stays)
        LODI,R1 ACT_USE
        LODI,R2 1
        ZBSR    *ZACTQ
        BSTA,UN DIRTYH
        LODA,R0 *PITEM
        COMI,R0 I_PARACHUTE
        RETC,EQ
        COMI,R0 I_IMPACT_FUSE
        RETC,EQ
        BSTA,UN DEFNG           ; (defending: more to set up)
        COMI,R0 0
        RETC,GT
        BCTA,UN ICLOSE
IFTGT:  LODA,R1 T0              ; the target: a step
        BCTR,EQ IFT1
        LODA,R0 *PTARGET
        BSTA,UN NEXTT
        CPUT    TARGET
        SND     UI_TICK
        LODA,R0 FDIRTY
        IORI,R0 FD_SEL|FD_HUD
        STRA,R0 FDIRTY
        RETC,UN
IFT1:   LODA,R0 EVBTN           ; FIRE: taken (the next shot steers for it)
        RETC,EQ
        SND     UI_GO
        BCTA,UN ICLOSE
IFDRV:  LODA,R0 INDIR           ; driving: an ACT_DRIVE each 8 jiffies held
        ANDI,R0 D_LEFT|D_RIGHT
        BCFR,EQ IFD1
        LODA,R0 *PDRIVEN
        BCTR,EQ IFD3
        EORZ    R0
        CPUT    DRIVEN
        BCTR,UN IFD3
IFD1:   LODA,R0 *PDRIVEN        ; jiffies to the next (0: the push, now;
        SUBA,R0 DT              ;  DT: PAL's 50 frames drive as far)
        BCTR,GT IFD4
        ADDI,R0 8
        CPUT    DRIVEN
        LODA,R0 INDIR           ; right 1, left 0
        ANDI,R0 D_RIGHT
        BCTR,EQ IFD2
        LODI,R0 1
IFD2:   CPUT    AARGS
        LODI,R1 ACT_DRIVE
        LODI,R2 1
        ZBSR    *ZACTQ
        BSTA,UN DIRTYH          ; (the fuel left)
        BCTR,UN IFD3
IFD4:   CPUT    DRIVEN
IFD3:   LODA,R0 EVBTN           ; FIRE: stop
        RETC,EQ
        BCTA,UN ICLOSE

; NEXTI: R0 = the next item we have after item R0, a step R1 (1, $FF);
; I_NONE if none (play.c next_item: the Defense Computer works by itself;
; the defence window: the defences alone). Calls one down. Clobbers R0-R3,
; T1-T3.
NEXTI:  STRA,R0 T2              ; T2: the item, T1: the step, T3: defending
        STRA,R1 T1
        BSTA,UN DEFNG
        STRA,R0 T3
        LODI,R1 N_ITEMS-N_WEAPONS
NXI1:   LODA,R0 T2
        ADDA,R0 T1
        COMI,R0 N_WEAPONS
        BCFR,LT NXI2
        LODI,R0 N_ITEMS-1
NXI2:   COMI,R0 N_ITEMS
        BCTR,LT NXI3
        LODI,R0 N_WEAPONS
NXI3:   STRA,R0 T2
        COMI,R0 I_DEFENSE_COMPUTER
        BCTR,EQ NXI5
        LODA,R3 T3
        BCTR,EQ NXI4
        COMI,R0 I_IMPACT_FUSE
        BCTR,EQ NXI4
        COMI,R0 I_PARACHUTE
        BCTR,LT NXI5
        COMI,R0 I_SUPER_DEFLECTOR+1
        BCFR,LT NXI5
NXI4:   STRZ    R3
        LODA,R0 ST+OFF_INV,R3
        BCTR,EQ NXI5
        LODA,R0 T2
        RETC,UN
NXI5:   BDRR,R1 NXI1
        LODI,R0 I_NONE
        RETC,UN

; NEXTT: R0 = the next tank on the field, not ours, after seat R0, a step
; R1 (1, $FF); NO_SEAT if none (play.c next_target). Leaf. Clobbers R0-R3,
; T1, T2.
NEXTT:  STRA,R1 T1
        STRA,R0 T2
        LODI,R1 SEATS
NXT1:   LODA,R0 T2
        ADDA,R0 T1
        COMI,R0 SEATS
        BCTR,LT NXT2
        LODI,R0 0               ; past either end: the other
        LODA,R3 T1
        BCFR,LT NXT2
        LODI,R0 SEATS-1
NXT2:   STRA,R0 T2
        COMA,R0 MYSEAT
        BCTR,EQ NXT3
        STRZ    R3
        LODA,R0 TKX,R3
        COMI,R0 $FF
        BCTR,EQ NXT3
        LODA,R0 T2
        RETC,UN
NXT3:   BDRR,R1 NXT1
        LODI,R0 NO_SEAT
        RETC,UN

; ---- the HUD ---------------------------------------------------------------------------------
; HSEAT: R0 = the seat the HUD shows: the turn's; ours if everyone aims (or
; none's); else 0. Leaf.
HSEAT:  LODA,R0 ST+OFF_TURN
        COMI,R0 SEATS
        RETC,LT
        LODA,R0 MYSEAT
        COMI,R0 SEATS
        RETC,LT
        EORZ    R0
        RETC,UN
; HOURS: CC EQ if the HUD shows our aim (we aim, it shows our seat). Leaf.
; Clobbers R0.
HOURS:  LODA,R0 MYTURN
        COMI,R0 1
        RETC,LT
        BSTR,UN HSEAT
        COMA,R0 MYSEAT
        RETC,UN
; HANG, HWEAP: R0 = the angle, the weapon the HUD shows (ours as we aim
; it). HPOW: T2:T3 its power. Leaves. Clobber R0, R3.
HANG:   BSTR,UN HOURS
        BCFR,EQ HAN1
        LODA,R0 AIMA
        RETC,UN
HAN1:   BSTR,UN HSOFF
        LODA,R0 ST+OFF_SEATS+P_ANGLE,R3
        RETC,UN
HWEAP:  BSTR,UN HOURS
        BCFR,EQ HWE1
        LODA,R0 WEAPON
        RETC,UN
HWE1:   BSTR,UN HSOFF
        LODA,R0 ST+OFF_SEATS+P_WEAPON,R3
        RETC,UN
HPOW:   BSTR,UN HOURS
        BCFR,EQ HPO1
        LODA,R0 AIMPH
        STRA,R0 T2
        LODA,R0 AIMPL
        STRA,R0 T3
        RETC,UN
HPO1:   BSTR,UN HSOFF
        LODA,R0 ST+OFF_SEATS+P_POWER+1,R3
        STRA,R0 T2
        LODA,R0 ST+OFF_SEATS+P_POWER,R3
        STRA,R0 T3
        RETC,UN
; HSOFF: R3 = the HUD's seat's offset in the seats. Leaf. Clobbers R0.
HSOFF:  BSTA,UN HSEAT
        STRZ    R3
        LODA,R0 SEAT25,R3
        STRZ    R3
        RETC,UN

; HUDQ: the HUD, if it was asked for (FD_HUD) and no message holds its rows.
HUDQ:   LODA,R0 FDIRTY
        TMI,R0  FD_HUD
        RETC,LT
        LODA,R0 *PTALKT
        RETC,GT
        LODA,R0 FDIRTY
        ANDI,R0 ~FD_HUD&$FF
        STRA,R0 FDIRTY
; HUD: rows 24 and 25 (each field over the last: no clearing). Calls three
; down. Clobbers R0-R3, T0-T3.
HUD:    DAT     HUDROW,1
        BSTA,UN DEFNG           ; the defence window's prompt, flashing
        COMI,R0 0
        BCTR,EQ HU0
        LODA,R0 FRML
        ANDI,R0 16
        BCTR,EQ HUD1
        DCOL    HUDTI
        TXTAT   SDEF
        BCTA,UN HU6
HUD1:   DCOL    HUDI
        TXTAT   SDEFD
        DCOL    HUDTI
        LODA,R0 ST+OFF_TIMER
        LODI,R1 2
        ZBSR    *ZDNUM
        BCTA,UN HU6
HU0:    LODA,R0 *PTURNJ         ; YOUR TURN!, flashing
        BCTR,EQ HUN
        LODA,R0 FRML
        ANDI,R0 16
        BCTR,EQ HUN
        DCOL    HUDTI
        TXTAT   SYOURT
        BCTA,UN HU6
HUN:    DCOL    HUDI            ; the name (6)
        BSTA,UN HSEAT
        STRZ    R3
        BSTA,UN NAME6
        LODI,R0 'A'             ; A angle
        ZBSR    *ZDPUTC
        BSTA,UN HANG
        LODI,R1 3
        ZBSR    *ZDNUM
        BSTA,UN HPOW            ; the power
        LODA,R0 T2              ; 1000 (or more): P1000
        COMI,R0 1000>>8
        BCTR,LT HU5
        BCTR,GT HU4A
        LODA,R0 T3
        COMI,R0 1000&$FF
        BCTR,LT HU5
HU4A:   TXTAT   SP1000
        BCTR,UN HU6
HU5:    EORZ    R0              ; else " P" and 3 places
        ZBSR    *ZDPUTC
        LODI,R0 'P'
        ZBSR    *ZDPUTC
        BSTA,UN NUM3W
HU6:    DAT     HUDROW+1,1
        DCOL    HUDI
        LODA,R0 *PIMENU        ; the items line
        BCTA,EQ HUW
        COMI,R0 M_TARGET
        BCTA,EQ HUT
        COMI,R0 M_DRIVE
        BCTA,EQ HUV
        LODA,R0 *PITEM          ; an item: its name (9), how many, ON
        STRA,R0 T1
        BSTA,UN SN9AT
        EORZ    R0
        ZBSR    *ZDPUTC
        LODA,R3 T1
        LODA,R0 ST+OFF_INV,R3
        BSTA,UN CNT2
        EORZ    R0
        ZBSR    *ZDPUTC
        LODI,R1 PF_CHUTE        ; (on: the parachute, the fuse)
        LODA,R0 T1
        COMI,R0 I_PARACHUTE
        BCTR,EQ HUI1
        LODI,R1 PF_FUSE
        COMI,R0 I_IMPACT_FUSE
        BCFR,EQ HUI2
HUI1:   LODA,R3 MYSEAT
        LODA,R0 SEAT25,R3
        STRZ    R3
        LODA,R0 ST+OFF_SEATS+P_FLAGS,R3
        ANDZ    R1
        BCTR,EQ HUI2
        TXTAT   SON
        RETC,UN
HUI2:   EORZ    R0
        ZBSR    *ZDPUTC
        EORZ    R0
        ZBSR    *ZDPUTC
        RETC,UN
HUT:    TXTAT   STARGET         ; TARGET name (8)
        LODA,R3 *PTARGET
        COMI,R3 SEATS
        BCTR,LT HUT1
        LODI,R0 GSPACE
        LODI,R3 NAME_LEN
        ZBSR    *ZDFILL         ; (a blank's HUDI: written as a space)
        RETC,UN
HUT1:   ZBRR    *ZNAMEAT
HUV:    TXTAT   SDRIVE          ; DRIVE FUEL nnn
        LODA,R0 ST+OFF_INV+I_FUEL
        LODI,R1 3
        ZBSR    *ZDNUM
        EORZ    R0
        ZBRR    *ZDPUTC
HUW:    BSTA,UN HWEAP           ; the weapon (7 of its short name)
        STRA,R0 T1
        BSTA,UN SN7AT
        BSTA,UN HSEAT           ; how many (ours only; not the mini missile's)
        COMA,R0 MYSEAT
        BCFR,EQ HU8
        BSTA,UN HWEAP
        COMI,R0 I_MINI_MISSILE
        BCTR,EQ HU8
        STRZ    R3
        LODA,R0 ST+OFF_INV,R3
        BSTA,UN CNT2
        BCTR,UN HU9
HU8:    EORZ    R0
        ZBSR    *ZDPUTC
        EORZ    R0
        ZBSR    *ZDPUTC
HU9:    LODA,R0 ST+OFF_WIND+1   ; the wind: < or > (none if still), 3 places
        STRA,R0 T2
        LODA,R0 ST+OFF_WIND
        STRA,R0 T3
        LODI,R0 FGT
        LODA,R1 T2
        BCTR,GT HU10            ; (positive)
        BCTR,LT HU9A
        LODA,R1 T3
        BCFR,EQ HU10
        EORZ    R0              ; (still)
        BCTR,UN HU10
HU9A:   LODA,R0 T3              ; negative: its magnitude
        EORI,R0 $FF
        ADDI,R0 1
        STRA,R0 T3
        LODA,R0 T2
        EORI,R0 $FF
        TPSL    PSLC            ; (the low byte's carry)
        BCFR,EQ HU9B
        ADDI,R0 1
HU9B:   STRA,R0 T2
        LODI,R0 FLT
HU10:   ZBSR    *ZDPUTC
        LODA,R0 T2              ; (at most 999)
        COMI,R0 1000>>8
        BCTR,LT HU11
        BCTR,GT HU10A
        LODA,R0 T3
        COMI,R0 1000&$FF
        BCTR,LT HU11
HU10A:  LODI,R0 999>>8
        STRA,R0 T2
        LODI,R0 999&$FF
        STRA,R0 T3
HU11:   BSTA,UN NUM3W
        DCOL    HUDTI           ; the timer (2; none past 99)
        LODA,R0 ST+OFF_TIMER
        COMI,R0 100
        BCFR,LT HU12
        LODI,R1 2
        ZBSR    *ZDNUM
        RETC,UN
HU12:   EORZ    R0
        ZBSR    *ZDPUTC
        EORZ    R0
        ZBRR    *ZDPUTC

; CNT2: R0 (0-255, 99 at most) in 2 places. Calls two down. Clobbers R0,
; R1, R3.
CNT2:   COMI,R0 100
        BCTR,LT CN1
        LODI,R0 99
CN1:    LODI,R1 2
        ZBRR    *ZDNUM

; NAME6: seat R3's name in 6 cells (DCOLOR). Calls two down. Clobbers R0,
; R1, R3.
NAME6:  LODA,R0 SEAT25,R3
        STRZ    R3
        LODI,R1 6
NM61:   LODA,R0 ST+OFF_SEATS+P_NAME,R3
        ZBSR    *ZA2G
        ZBSR    *ZDPUTC
        ADDI,R3 1
        BDRR,R1 NM61
        RETC,UN

; SN7AT: weapon T1's short name (names.inc SN7: 7 cells; past the weapons:
; blank). SN9AT: item T1's (SN9: 9 cells). Calls one down. Clobbers R0,
; R1, R3, T2.
SN7AT:  LODA,R0 T1
        COMI,R0 N_WEAPONS
        BCTR,LT SN71
        LODI,R0 GSPACE
        LODI,R3 7
        ZBRR    *ZDFILL
SN71:   STRZ    R3              ; 7 x it
        ADDZ    R0
        ADDZ    R0
        ADDZ    R0
        SUBZ    R3
        STRZ    R3
        LODI,R1 7
SN72:   LODA,R0 *PSN7,R3
        ZBSR    *ZDPUTC
        ADDI,R3 1
        BDRR,R1 SN72
        RETC,UN
SN9AT:  LODA,R0 T1
        LODI,R1 0               ; R1: SN9's first part, or SN9B's
        COMI,R0 28
        BCTR,LT SN1
        SUBI,R0 28
        LODI,R1 1
SN1:    STRZ    R3              ; 9 x it
        ADDZ    R0
        ADDZ    R0
        ADDZ    R0
        ADDZ    R3
        STRZ    R3
        LODI,R0 9
        STRA,R0 T2
SN2:    COMI,R1 0
        BCFR,EQ SN3
        LODA,R0 *PSN9A,R3
        BCTR,UN SN4
SN3:    LODA,R0 *PSN9B,R3
SN4:    ZBSR    *ZDPUTC
        ADDI,R3 1
        LODA,R0 T2
        SUBI,R0 1
        STRA,R0 T2
        BCFR,EQ SN2
        RETC,UN

; NUM3W: T2:T3 (0-999) in 3 places, right-aligned, at the next cells. Calls
; two down. Clobbers R0-R3, T2, T3.
NUM3W:  LODI,R1 0               ; R1: the hundreds
NM1:    LODA,R0 T2
        BCFR,EQ NM2
        LODA,R0 T3
        COMI,R0 100
        BCTR,LT NM3
NM2:    LODA,R0 T3              ; - 100
        COMI,R0 100
        BCFR,LT NM2A
        LODA,R0 T2
        SUBI,R0 1
        STRA,R0 T2
        LODA,R0 T3
NM2A:   SUBI,R0 100
        STRA,R0 T3
        ADDI,R1 1
        BCTR,UN NM1
NM3:    LODZ    R1
        BCFR,EQ NM4
        LODA,R0 T3              ; under 100: DNUM's 3 places
        LODI,R1 3
        ZBRR    *ZDNUM
NM4:    IORI,R0 GDIGIT          ; the hundreds, then 2 places with a 0
        ZBSR    *ZDPUTC
        LODA,R0 T3
        LODI,R1 0
NM5:    COMI,R0 10
        BCTR,LT NM6
        SUBI,R0 10
        ADDI,R1 1
        BCTR,UN NM5
NM6:    STRA,R0 T3
        LODZ    R1
        IORI,R0 GDIGIT
        ZBSR    *ZDPUTC
        LODA,R0 T3
        IORI,R0 GDIGIT
        ZBRR    *ZDPUTC

SDEF:   DB      "SET UP DEFENCES",$FF
SDEFD:  DB      "ENTER DONE   ",$FF
SYOURT: DB      "  YOUR TURN",FBANG,"   ",$FF
SP1000: DB      "P1000",$FF
SON:    DB      "ON",$FF
STARGET: DB     "TARGET ",$FF
SDRIVE: DB      "DRIVE FUEL ",$FF
