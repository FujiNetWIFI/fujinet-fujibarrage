; ---------------------------------------------------------------------------
; p_hud.inc -- page HUD: the field's screen (src/play.c): the field shown in
; the round's sky, the tanks, the two text rows under it, aiming and firing.
;
; HUD row 1: the turn's (or our) name, A angle, P power, E energy.
; HUD row 2: its weapon, our count of it (X n), the wind (< or > and its
; strength), the turn's timer.
; Aiming (our turn): the stick left/right turns the barrel (held: faster),
; up/down the power (at most energy x 10); twist picks the weapon before or
; after (one we own); push fires. ACT_AIM goes every 8 jiffies while the aim
; differs from what was sent (the others see the barrel move). The turn's
; tank blinks while it aims (8 jiffies off in 32). YOUR TURN!, the items
; line (pull), guidance's target arrow, fuel's driving, the defence window
; (it opens the line on the defences alone; START is done) and the idle
; cue are here; the rows and the messages are hud.inc's.
;
; Entries: E_ENTER (the field onto the screen), E_STATE (a new state),
; E_TICK (each pass).
; ---------------------------------------------------------------------------


HENTER: ENTER
        CLR                     ; the text screen's rows blank
        LR 0,A
        LI TROWS-1
        LR 1,A
        PI TXCLR
        PI SKYSET
        LDB VMODE               ; the field onto the screen
        CI VM_FIELD
        BZ HEN1
        PI FSHOW
HEN1:   CLR
        STB MYTURN
        STB BLINKT
        STB MENU                ; the field's UI fresh
        STB FLK
        STB INSCR
        STB DEFOPN
        LI 0FFH
        STB GUIDE
        STB TARGET
        PI CATCHK
        PI TKSTATE
        PI TURNCHK
        PI HDRAW
        LEAVE

HSTATE: ENTER
        PI TKSTATE
        PI TURNCHK
        PI HDRAW
        LEAVE

; ---- each pass ----------------------------------------------------------------
HTICK:  ENTER
        LDB DT                  ; the turn's tank's blink
        LR 0,A
        LDB BLINKT
        LR 1,A
        AS 0
        STB BLINKT
        XS 1
        NI BLINKOFF             ; (its off window entered or left: shown)
        BZ HTK0
        PI TKSTATE
HTK0:   LDB DT                  ; a message's time (held while a shot's
        LR 0,A                  ;  segments come); the flashes
        LDB INSCR
        LR 1,A
        PI MTICK
        PI FLTICK
        PI DEFING               ; the defence window opens: its items line
        LR A,0                  ;  at once (the defences alone)
        STB HDEF
        NI 0FFH
        BNZ HTD1
        CLR
        STB DEFOPN
        BR HTD2
HTD1:   LDB DEFOPN
        NI 0FFH
        BNZ HTD2
        LIS 1
        STB DEFOPN
        SND UI_TURN
        LIS FL_DEFENCE
        STB FLK
        PI IFIRST
        LR A,0
        STB ITEM
        CI I_NONE
        BZ HTD1A
        LIS M_ITEM
        STB MENU
        LDB INPDIR
        STB DPREV
HTD1A:  PI HDRAW
HTD2:   LDB MYTURN              ; our turn: the first input ends YOUR TURN!,
        NI 0FFH                 ;  idle a while: the cue again
        BZ HTD3
        PI IDLECHK
HTD3:   LDB MYTURN              ; neither ours nor the window: the items
        DCI HDEF                ;  line shut
        OM
        BNZ HTD4
        LDB MENU
        NI 0FFH
        BZ HTD3X
        PI ISHUT
        PI HROW2
HTD3X:  LEAVE
HTD4:   LDB HDEF                ; the defence window: START is done
        NI 0FFH
        BZ HTD5
        LDB EVKEY
        CI KEY_END
        BNZ HTD5
        PI ISHUT
        LIS ACT_DONE
        LR 1,A
        CLR
        LR 2,A
        PI ACTQ
        SND UI_GO
        PI HDRAW
        LEAVE
HTD5:   LDB EVKEY               ; pull: the items line open or shut
        CI KEY_BACK
        LIS 1
        BZ HTD5A
        CLR
HTD5A:  LR 0,A
        LDB MENU
        NI 0FFH
        BZ HTD6
        PI IFRAME
        LEAVE
HTD6:   LR A,0
        NI 0FFH
        BZ HTD7
        PI IOPEN
        LEAVE
HTD7:   LDB HDEF                ; the window: the items alone
        NI 0FFH
        BZ HTK2
        LEAVE
HTK2:   PI AIMSTICK
        PI AIMKEYS
        PI AIMSEND
        LDB EVBTN               ; push: fire
        NI 0FFH
        BNZ HTK3
        LEAVE
HTK3:   DCI AARGS               ; ACT_FIRE angle, power, weapon, guide, target
        LDB AIMA
        DCI AARGS
        ST
        LDB AIMP
        DCI AARGS+1
        ST
        LDB AIMP+1
        DCI AARGS+2
        ST
        LDB AIMW
        DCI AARGS+3
        ST
        LDB GUIDE               ; guidance (I_NONE) and its target (NO_SEAT)
        DCI AARGS+4
        ST
        LDB TARGET
        DCI AARGS+5
        ST
        LI I_NONE
        STB GUIDE
        LI NO_SEAT
        STB TARGET
        LIS ACT_FIRE
        LR 1,A
        LIS 6
        LR 2,A
        PI ACTQ
        SND UI_GO
        CLR
        STB MYTURN
        PI HDRAW
        LEAVE

; AIMSTICK -- the stick: left/right the angle, up/down the power; a step
;   when first held, then every 4 passes, then each pass, 1 then 2 then 4
;   (5x for the power) the longer it is held.
AIMSTICK: ENTER
        LDB INPDIR
        NI 0FFH
        BNZ AST1
        CLR
        STB HELDN
        LEAVE
AST1:   LDB HELDN               ; (to 255)
        INC
        BZ AST2
        STB HELDN
AST2:   LDB HELDN               ; a step now?
        CI 1
        BZ AST3
        CI 15
        BNC AST3
        NI 3
        BZ AST3
        LEAVE
AST3:   LDB HELDN               ; its size
        LR 1,A
        LIS 1
        LR 2,A
        LR A,1
        CI 15
        BC AST4
        LIS 2
        LR 2,A
        LR A,1
        CI 40
        BC AST4
        LIS 4
        LR 2,A
AST4:   LDB INPDIR
        LR 3,A
        NI IN_LEFT              ; left: up to 180
        BZ AST5
        LDB AIMA
        AS 2
        CI 180
        BC AST4A
        LI 180
AST4A:  STB AIMA
AST5:   LR A,3
        NI IN_RIGHT             ; right: down to 0
        BZ AST6
        LR A,2
        COM
        INC
        LR 4,A
        LDB AIMA
        AS 4
        BC AST5A                ; (no borrow)
        CLR
AST5A:  STB AIMA
AST6:   LR A,2                  ; the power's step: 5 x
        SL 1
        SL 1
        AS 2
        LR 2,A
        LDB MYSEAT              ; its most: energy x 10 (r5:r4)
        LR 0,A
        PI SEATAT
        LI P_ENERGY
        ADC
        LM
        LR 4,A
        CLR
        LR 5,A
        LR A,4                  ; x 10 = x 8 + x 2
        SL 1
        LR 6,A                  ; (x 2: 200 at most)
        LR A,4
        SR 4
        SR 1
        LR 5,A                  ; (x 8's high byte: e >> 5)
        LR A,4
        SL 1
        SL 1
        SL 1
        AS 6
        LR 4,A
        LR A,5
        LNK
        LR 5,A
        LR A,3
        NI IN_UP
        BZ AST7
        LDB AIMP                ; up: + step, at most r5:r4
        AS 2
        STB AIMP
        LDB AIMP+1
        LNK
        STB AIMP+1
        PI AIMCLIP
AST7:   LR A,3
        NI IN_DOWN
        BZ AST8
        LR A,2                  ; down: - step, at least 0
        COM
        INC
        LR 6,A
        LDB AIMP
        AS 6
        STB AIMP
        LDB AIMP+1
        LNK
        AI 0FFH
        STB AIMP+1
        BC AST8                 ; (no borrow out of the high byte)
        CLR
        STB AIMP
        STB AIMP+1
AST8:   LDB MYSEAT              ; our barrel, and the numbers
        LR 0,A
        LDB AIMA
        LR 1,A
        LR A,0
        DCI TKA
        ADC
        LR A,1
        ST
        PI TKSHOW
        PI HROW1
        LEAVE

; AIMCLIP -- AIMP at most r5:r4.  Leaf.  Clobbers A, r6-r8, DC0.
AIMCLIP: DCI AIMP
        LM
        LR 7,A                  ; lo
        LM
        LR 6,A                  ; hi
        LR A,5                  ; AIMP + ~max carries when AIMP > max
        COM                     ; (~hi first: COM clears the carry)
        LR 8,A
        LR A,4
        COM
        AS 7
        ADC3 8,6
        BNC AC9
        DCI AIMP
        LR A,4
        ST
        LR A,5
        ST
AC9:    POP

; AIMKEYS -- twist: the weapon before (counterclockwise) or after that we
;   own (the Mini Missile always); ACT_WEAPON.
AIMKEYS: ENTER
        LDB EVKEY
        CI KEY_TWL
        LI 0FFH
        BZ AK1
        LDB EVKEY
        CI KEY_TWR
        BZ AK0
        LEAVE
AK0:    LIS 1
AK1:    STB T2                  ; the step: 1 or -1
        LDB AIMW
        LR 6,A
        LI N_WEAPONS
        LR 7,A                  ; (tries)
AK2:    LDB T2
        AS 6
        LR 6,A
        CI N_WEAPONS-1          ; wrapped: 255 -> the last, N_WEAPONS -> 0
        BC AK3
        LR A,6
        CI 0FFH
        LI N_WEAPONS-1
        BZ AK2A
        CLR
AK2A:   LR 6,A
AK3:    LR A,6
        NI 0FFH
        BZ AK4                  ; the Mini Missile
        DCI ST+OFF_INV
        ADC
        LM
        NI 0FFH
        BNZ AK4
        DS 7
        BNZ AK2
        LEAVE
AK4:    LR A,6
        STB AIMW
        STB AARGS
        LIS ACT_WEAPON
        LR 1,A
        LIS 1
        LR 2,A
        PI ACTQ
        SND UI_SELECT
        PI HROW2
        LEAVE

; AIMSEND -- ACT_AIM (angle, power) every 8 jiffies while it differs from
;   what was sent.
AIMSEND: ENTER
        LDB SENTT               ; 8 jiffies since?
        COM
        INC
        LR 0,A
        LDB JIFFY
        AS 0
        CI 7
        BNC AS1
        LEAVE
AS1:    DCI AIMA                ; (AIMA, AIMP: the same as SENTA, SENTP?)
        LM
        LR 1,A
        LM
        LR 2,A
        LM
        LR 3,A
        DCI SENTA
        LM
        XS 1
        BNZ AS2
        LM
        XS 2
        BNZ AS2
        LM
        XS 3
        BNZ AS2
        LEAVE
AS2:    DCI SENTA
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        DCI AARGS
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        LDB JIFFY
        STB SENTT
        LIS ACT_AIM
        LR 1,A
        LIS 3
        LR 2,A
        PI ACTQ
        LEAVE

; ---- the turn --------------------------------------------------------------------
; TURNCHK -- our turn begins (src/play.c turn_check): the aim as the server
;   has it, the cue; MYTURN.
TURNCHK: ENTER
        PI ISMINE
        LR A,0
        NI 0FFH
        BNZ TC1
        CLR
        STB MYTURN
        LEAVE
TC1:    LDB MYTURN
        NI 0FFH
        BZ TC2
        LEAVE
TC2:    LDB MYSEAT
        LR 0,A
        PI SEATAT
        LR Q,DC
        LI P_ANGLE
        ADC
        LM
        STB AIMA
        STB SENTA
        LR DC,Q
        LI P_POWER
        ADC
        LM
        LR 1,A
        LM
        LR 2,A
        LR A,1
        STB AIMP
        STB SENTP
        LR A,2
        STB AIMP+1
        STB SENTP+1
        LR DC,Q
        LI P_WEAPON
        ADC
        LM
        STB AIMW
        LDB JIFFY
        STB SENTT
        LIS 1
        STB MYTURN
        CLR
        STB HELDN
        STB IDLEJ
        STB IDLEJ+1
        SND UI_TURN
        LDB MENU                ; YOUR TURN! (not over the items line)
        NI 0FFH
        BNZ TC3
        LIS FL_BANNER
        STB FLK
        LI BANJ
        STB FLJ
TC3:    LEAVE

; ISMINE -- r0 = 1 if we aim now (src/play.c is_my_turn).  Leaf.
ISMINE: CLR
        LR 0,A
        LDB MYSEAT
        CI SEATS-1
        BNC ISM9
        LDB ST+OFF_PHASE
        CI PH_AIM
        BNZ ISM9
        LDB MYSEAT              ; alive
        LR 1,A
        SL 1
        SL 1
        SL 1
        LR 2,A
        SL 1
        AS 2
        AS 1
        DCI ST+OFF_SEATS+P_FLAGS
        ADCU
        LM
        LR 2,A
        NI PF_ALIVE
        BZ ISM9
        LDB ST+OFF_TURN
        XS 1
        BZ ISM1
        LDB ST+OFF_TURN         ; everyone aims, and we aren't done
        CI NO_SEAT
        BNZ ISM9
        LR A,2
        NI PF_DONE
        BNZ ISM9
ISM1:   LIS 1
        LR 0,A
ISM9:   POP

; ---- the flashes: YOUR TURN!, SET UP DEFENCES (row 1's palette) ------------------
; FLTICK -- a pass: the banner's time (over: not our turn, BANJ out, the
;   items line open), the window's (over: not defending); lit 16 jiffies in
;   32 (light grey), black the rest; a message keeps its band.
FLTICK: ENTER
        LDB FLK
        NI 0FFH
        BNZ FT1
        LEAVE
FT1:    CI FL_BANNER
        BNZ FT3
        LDB MYTURN
        NI 0FFH
        BZ FT2
        LDB MENU
        NI 0FFH
        BNZ FT2
        LDB FLJ
        LR 1,A
        LDB DT
        COM
        INC
        AS 1
        BNC FT2
        BZ FT2
        STB FLJ
        BR FT4
FT2:    PI FLOFF
        LEAVE
FT3:    PI DEFING
        LR A,0
        NI 0FFH
        BZ FT2
FT4:    LDB MSGJ
        NI 0FFH
        BZ FT5
        LEAVE
FT5:    LDB JIFFY
        NI 16
        BZ FT6
        LIS MSGPAL
FT6:    LR 2,A
        LI HUDY0
        LR 0,A
        LI HUDY0+5
        LR 1,A
        PI PALSET               ; (only a change is written)
        LEAVE

; FLOFF -- the flash over: row 1 black and as the HUD has it.
FLOFF:  ENTER
        CLR
        STB FLK
        LDB MSGJ
        NI 0FFH
        BNZ FLO1
        LI HUDY0
        LR 0,A
        LI HUDY0+5
        LR 1,A
        CLR
        LR 2,A
        PI PALSET
        PI HROW1
FLO1:   LEAVE

; IDLECHK -- our turn: an input ends YOUR TURN! and starts the idle clock
;   again; REMINDJ jiffies idle: the cue again.
REMINDJ EQU 600
IDLECHK: ENTER
        LDB INPDIR
        DCI EVBTN
        OM
        DCI EVKEY
        OM
        BZ IDC2
        CLR
        STB IDLEJ
        STB IDLEJ+1
        LDB FLK
        CI FL_BANNER
        BNZ IDC1
        PI FLOFF
IDC1:   LEAVE
IDC2:   LDB DT                  ; IDLEJ += DT (16 bits)
        LR 1,A
        DCI IDLEJ
        LM
        AS 1
        LR 2,A
        LM
        LNK
        LR 3,A
        DCI IDLEJ
        LR A,2
        ST
        LR A,3
        ST
        CI (REMINDJ>>8)-1       ; REMINDJ at least?
        BC IDC9
        CI REMINDJ>>8
        BNZ IDC3
        LR A,2
        CI (REMINDJ&0FFH)-1
        BC IDC9
IDC3:   CLR
        STB IDLEJ
        STB IDLEJ+1
        SND UI_TURN
IDC9:   LEAVE

; ---- the items line (src/play.c items_frame) --------------------------------------
; Pull opens it on our turn (the defence window opens it by itself): the
; stick's left and right pick an item we have, push uses it (ACT_USE).
; Guidance asks for a target next (left and right; an arrow over it; push
; takes it: the next ACT_FIRE carries both); fuel drives (held left or
; right: ACT_DRIVE every 8 passes; push stops). Pull shuts it. The Defense
; Computer works by itself: not listed.

; IFIRST -- r0 = the first item we have (I_NONE: none).
IFIRST: ENTER
        LI N_ITEMS-1
        STB NXI
        LIS 1
        STB NXD
        PI NEXTI
        LEAVE

; IOPEN -- the items line open on its first item (none: the error cue).
IOPEN:  ENTER
        PI IFIRST
        LR A,0
        CI I_NONE
        BNZ IOP1
        SND UI_ERROR
        LEAVE
IOP1:   STB ITEM
        LIS M_ITEM
        STB MENU
        LDB INPDIR
        STB DPREV
        SND UI_SELECT
        PI HROW2
        LEAVE

; ISHUT -- the items line shut (its arrow off).
ISHUT:  ENTER
        CLR
        LR 0,A
        PI MARK
        CLR
        STB MENU
        LEAVE

; IFRAME -- a pass of the open line; r0 = 1: shut it.
IFRAME: ENTER
        LR A,0
        STB T2
        LDB DPREV               ; the stick's new presses: a step
        COM
        LR 1,A
        LDB INPDIR
        STB DPREV
        NS 1
        LR 2,A
        CLR
        STB NXD
        STB IREDR
        LR A,2
        NI IN_LEFT
        BZ IFR1
        LI 0FFH
        STB NXD
IFR1:   LR A,2
        NI IN_RIGHT
        BZ IFR2
        LIS 1
        STB NXD
IFR2:   LDB T2
        NI 0FFH
        BZ IFR3
        PI ISHUT
        JMP IFRX
IFR3:   PI DEFING
        LR A,0
        STB T2                  ; (now: defending)
        LDB MENU
        CI M_ITEM
        BZ IFR3A
        JMP IFR6
IFR3A:  LDB NXD                 ; an item: a step
        NI 0FFH
        BZ IFR4
        LDB ITEM
        STB NXI
        PI NEXTI
        LR A,0
        STB ITEM
        SND UI_TICK
        LIS 1
        STB IREDR
IFR4:   LDB EVBTN               ; push: used
        NI 0FFH
        BNZ IFR4A
        JMP IFR9
IFR4A:  SND UI_SELECT
        LDB ITEM
        CI I_NONE
        BNZ IFR4B
        JMP IFRX
IFR4B:  CI I_AUTOPILOT          ; guidance: its target next
        BNC IFR5
        LDB T2
        NI 0FFH
        BNZ IFR5A
        LDB ITEM
        STB GUIDE
        LDB TARGET
        STB NXT
        LIS 1
        STB NXD
        PI NEXTT
        LR A,0
        STB TARGET
        LIS M_TARGET
        STB MENU
        LIS 1
        LR 0,A
        PI MARK
        JMP IFRX
IFR5:   CI I_FUEL               ; fuel: driving
        BNZ IFR5A
        LDB T2
        NI 0FFH
        BNZ IFR5A
        LIS M_DRIVE
        STB MENU
        CLR
        STB DRIVN
        JMP IFRX
IFR5A:  LDB ITEM                ; the rest: used now (the parachute and the
        STB AARGS               ;  impact fuse switch: the line stays; the
        LIS ACT_USE             ;  window: more to set up)
        LR 1,A
        LIS 1
        LR 2,A
        PI ACTQ
        LDB ITEM
        CI I_PARACHUTE
        BZ IFRY
        CI I_IMPACT_FUSE
        BZ IFRY
        LDB T2
        NI 0FFH
        BNZ IFRY
        CLR
        STB MENU
IFRY:   JMP IFRX
IFR6:   LDB MENU                ; (JMP spent A)
        CI M_TARGET
        BNZ IFR8
        LDB NXD                 ; a target: a step
        NI 0FFH
        BZ IFR7
        CLR
        LR 0,A
        PI MARK
        LDB TARGET
        STB NXT
        PI NEXTT
        LR A,0
        STB TARGET
        LIS 1
        LR 0,A
        PI MARK
        SND UI_TICK
        LIS 1
        STB IREDR
IFR7:   LDB EVBTN               ; push: taken
        NI 0FFH
        BZ IFR9
        PI ISHUT
        SND UI_SELECT
        BR IFRX
IFR8:   LDB DPREV               ; driving: ACT_DRIVE each 8 passes held
        NI IN_LEFT+IN_RIGHT
        BNZ IFR8A
        CLR
        STB DRIVN
        BR IFR8B
IFR8A:  LDB DRIVN
        LR 3,A
        INC
        STB DRIVN
        LR A,3
        NI 7
        BNZ IFR8B
        LDB DPREV
        NI IN_RIGHT
        BZ IFR8C
        LIS 1
IFR8C:  STB AARGS
        LIS ACT_DRIVE
        LR 1,A
        LIS 1
        LR 2,A
        PI ACTQ
IFR8B:  LDB EVBTN               ; push: stop
        NI 0FFH
        BZ IFR9
        CLR
        STB MENU
IFRX:   LIS 1
        STB IREDR
IFR9:   LDB IREDR               ; (drawn again only if something changed)
        NI 0FFH
        BZ IFR10
        PI HROW2
IFR10:  LEAVE

; NEXTI -- r0 = the next item we have from NXI, a step NXD (1, FF); I_NONE
;   if none (src/play.c next_item; the window: the defences alone).
NEXTI:  ENTER
        PI DEFING
        LR A,0
        STB NXDEF
        LI N_ITEMS-N_WEAPONS
        STB NXN
NXI1:   LDB NXD
        LR 1,A
        LDB NXI
        AS 1
        STB NXI
        CI N_WEAPONS-1          ; below the items: the last
        BNC NXI2
        LI N_ITEMS-1
        STB NXI
        BR NXI3
NXI2:   CI N_ITEMS-1            ; past them: the first
        BC NXI3
        LI N_WEAPONS
        STB NXI
NXI3:   LDB NXI
        CI I_DEFENSE_COMPUTER
        BZ NXI8
        LR 3,A
        LDB NXDEF
        NI 0FFH
        BZ NXI4
        LR A,3
        CI I_PARACHUTE-1
        BC NXI8
        CI I_SUPER_DEFLECTOR
        BC NXI4
        CI I_IMPACT_FUSE
        BNZ NXI8
NXI4:   DCI ST+OFF_INV
        LR A,3
        ADC
        LM
        NI 0FFH
        BZ NXI8
        LR A,3
        LR 0,A
        LEAVE
NXI8:   DECB NXN
        BNZ NXI1
        LI I_NONE
        LR 0,A
        LEAVE

; NEXTT -- r0 = the next tank on, not ours, from NXT, a step NXD; NO_SEAT if
;   none (src/play.c next_target).
NEXTT:  ENTER
        LI SEATS
        STB NXN
NXT1:   LDB NXD
        LR 1,A
        LDB NXT
        AS 1
        CI SEATS-1
        BC NXT2
        LDB NXD                 ; past either end: the other
        NI 80H
        LI SEATS-1
        BNZ NXT2
        CLR
NXT2:   STB NXT
        LR 3,A
        LDB MYSEAT
        XS 3
        BZ NXT8
        DCI TKON
        LR A,3
        ADC
        LM
        NI 0FFH
        BZ NXT8
        LR A,3
        LR 0,A
        LEAVE
NXT8:   DECB NXN
        BNZ NXT1
        LI NO_SEAT
        LR 0,A
        LEAVE

; MARK -- the target's arrow, r0 = 1 on, 0 off: 3 fire pixels over the
;   sky only (src/play.c mark): (x - 1, y - 7), (x, y - 6), (x + 1, y - 7).
MARK:   ENTER
        LDB TARGET
        CI SEATS-1
        BNC MK9
        STB T2
        LR A,0
        NI 0FFH
        LI PX_FX
        LR 1,A
        LI PX_SKY
        LR 8,A
        BNZ MK1
        LI PX_SKY
        LR 1,A
        LI PX_FX
        LR 8,A
MK1:    CLR
        LR 2,A
        LR 3,A
        LDB T2
        DCI TKY
        ADC
        LM
        AI -(TANK_H+4)
        BNC MK9                 ; (no room over it: none)
        LR 6,A
        LDB T2
        DCI TKX
        ADC
        LM
        AI -1
        LR 5,A
        PI PSWAP
        LR A,5
        INC
        LR 5,A
        LR A,6
        INC
        LR 6,A
        PI PSWAP
        LR A,5
        INC
        LR 5,A
        LR A,6
        AI -1
        LR 6,A
        PI PSWAP
MK9:    LEAVE

