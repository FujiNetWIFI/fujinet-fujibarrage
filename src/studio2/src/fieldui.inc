; fieldui.inc -- the field's own lines over the HUD (src/play.c): the items
; line, the defence window, YOUR TURN!, and the shot's messages (talk, a
; hit, a shield).
;
; THE ITEMS LINE (7 on our turn or in the defence window; row 2): 4 6
; choose an item we have, 5 uses it: guidance (Heat Seeker..Autopilot)
; asks for a target next (4 6, its tank blinks; 5 takes it: our ACT_FIRE
; carries both), fuel drives (4 6 held: ACT_DRIVE every 8 jiffies; 5
; stops), the rest ACT_USE (the parachute and the fuse stay on the line;
; the defence window keeps it open). 7 shuts it. The defence window opens
; it by itself on the defences alone; 9 is done (ACT_DONE).
; YOUR TURN! flashes on row 1 for BANNER_J jiffies or until a key.
; A MESSAGE (HMSGT jiffies) holds both rows: a talk (the speaker, then its
; words: 32 at once, the rest after half its time), a hit, a shield.

; (M_*, MK_*, BANNER_J, TALK_J, EVENT_J: equ.inc)

; defending: D = 1 if the defence window is ours (PH_DEFENSE, our seat
;   alive and not done).
        RFIT    47
defending:
        GETS    OFF_PHASE
        XRI     PH_DEFENSE
        BNZ     dfn0
        GETV    MYSEAT
        SMI     SEATS
        BDF     dfn0
        GETV    MYSEAT
        FCALL   seat_rec
        GLO     RB
        ADI     P_FLAGS
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDN     RB
        ANI     PF_ALIVE+PF_DONE
        XRI     PF_ALIVE
        BNZ     dfn0
        LDI     1
        FRET
dfn0:   LDI     0
        FRET

; next_item: D = the item after (T0 = 1) or before (T0 = FF) item D that we
;   have and may use now (not the Defense Computer; the defence window:
;   the defences alone); I_NONE if none. T1 the item, T2 the tries.
        RFIT    117
next_item:
        PUTV    T1
        SETV    T2, N_ITEMS-N_WEAPONS
nit1:   GETV    T0                      ; on, round the items
        STR     R2
        GETV    T1
        ADD
        PUTV    T1
        SMI     N_WEAPONS
        BDF     nit2
        LDI     N_ITEMS-1               ; (below the items: the last)
        BR      nit3
nit2:   GETV    T1
        SMI     N_ITEMS
        BNF     nit4
        LDI     N_WEAPONS               ; (past them: the first)
nit3:   PUTV    T1
nit4:   GETV    T1                      ; ours?
        ADI     LO(ST+OFF_INV)
        PLO     RB
        LDI     HI(ST+OFF_INV)
        ADCI    0
        PHI     RB
        LDN     RB
        BZ      nit8
        GETV    T1
        XRI     I_DEFENSE_COMPUTER
        BZ      nit8
        FCALL   defending               ; the window: the defences alone
        BZ      nit9
        GETV    T1
        XRI     I_IMPACT_FUSE
        BZ      nit9
        GETV    T1
        SMI     I_PARACHUTE
        BNF     nit8
        SMI     I_SUPER_DEFLECTOR-I_PARACHUTE+1
        BNF     nit9
nit8:   GETV    T2
        SMI     1
        STR     RA
        BNZ     nit1
        LDI     I_NONE
        FRET
nit9:   GETV    T1
        FRET

; next_target: D = the seat after (T0 1) or before (T0 FF) seat D with a
;   tank on, not ours; NO_SEAT if none. T1 the seat, T2 the tries.
        RFIT    91
next_target:
        PUTV    T1
        SETV    T2, SEATS
ntg1:   GETV    T0
        STR     R2
        GETV    T1
        ADD
        PUTV    T1
        SMI     SEATS
        BNF     ntg2
        GETV    T0                      ; (round)
        ANI     80H
        BZ      ntg1a
        LDI     SEATS-1
        BR      ntg1b
ntg1a:  LDI     0
ntg1b:  PUTV    T1
ntg2:   GETV    T1
        STR     R2
        GETV    MYSEAT
        XOR
        BZ      ntg3
        GETV    T1
        ADI     LO(TKON)
        PLO     RA
        LDN     RA
        BZ      ntg3
        GETV    T1
        FRET
ntg3:   GETV    T2
        SMI     1
        STR     RA
        BNZ     ntg1
        LDI     NO_SEAT
        FRET

; ---------------------------------------------------------------------------
; fx_pass -- the field's pass before the aim's: the defence window opening,
;   the banner's time, the message's time; then, ours to act (our turn or
;   the defence window), the items line's keys. D = 1 if they took the
;   keys (the aim's pass is skipped).
; ---------------------------------------------------------------------------
        RFIT    232
fx_pass:
        FCALL   msg_tick                ; (first: it spends T0-T7)
        FCALL   fx_idle
        FCALL   defending               ; the defence window: it opens once
        PUTV    T6
        BNZ     fxp1
        SETV    DEFOPN, 0
        BR      fxp2
fxp1:   GETV    DEFOPN
        BNZ     fxp2
        SETV    DEFOPN, 1
        SND     UI_TURN
        SETV    T0, 1
        LDI     N_ITEMS-1
        FCALL   next_item
        PUTV    ITEM
        XRI     I_NONE
        BZ      fxp1a
        SETV    MENU, M_ITEM
fxp1a:  SETV    HUDDRT, 3
fxp2:   GETV    BANNERT                 ; YOUR TURN!: its time
        BZ      fxp3
        GETV    DT
        STR     R2
        GETV    BANNERT
        SM
        BDF     fxp2a
        LDI     0
fxp2a:  STR     RA
        BNZ     fxp3
        SETV    HUDDRT, 3
fxp3:   GETV    T6                      ; ours to act?
        BNZ     fxp4
        GETV    AIMING
        BNZ     fxp4
        GETV    MENU                    ; (not: the line shut)
        BZ      fxp9
        SETV    MENU, M_OFF
        SETV    HUDDRT, 3
fxp9:   LDI     0
        FRET
fxp4:   GETV    KNEW                    ; a key: the banner gone
        BZ      fxp5
        GETV    BANNERT
        BZ      fxp5
        SETV    BANNERT, 0
        SETV    HUDDRT, 3
fxp5:   GETV    T6                      ; the defence window: 9 done
        BZ      fxp6
        GETV    KNEW+1
        ANI     02H
        BZ      fxp6
        SETV    KNEW+1, 0
        SETV    MENU, M_OFF
        SETV    ANARG, 0
        LDI     ACT_DONE
        FCALL   act_q
        SND     UI_GO
        SETV    HUDDRT, 3
        LDI     1
        FRET
fxp6:   FCALL   items_keys              ; the line had the keys: spent
        BZ      fxp7
        SETV    KNEW, 0
        SETV    KNEW+1, 0
        LDI     1
fxp7:   FRET

; fx_idle: our turn, no key held for 3 x 256 jiffies (~13 s): the cue
;   again (play.c REMIND_J).
        RFIT    48
fx_idle:
        GETV    AIMING
        BZ      fxi9
        GETV    KHELD
        BNZ     fxi1
        GETV    KHELD+1
        BNZ     fxi1
        GETV    IDLEAT
        STR     R2
        GETV    JIFFY+1
        SM
        SMI     3
        BNF     fxi9
        SND     UI_TURN
fxi1:   GETV    JIFFY+1
        PUTV    IDLEAT
fxi9:   FRET

; items_keys: 7 opens or shuts the line; open, its keys. D = 1 if it has
;   them (open), else 0.
        RFIT    113
items_keys:
        GETV    KNEW                    ; 7
        ANI     80H
        BZ      ik2
        GETV    KNEW
        ANI     7FH
        STR     RA
        GETV    MENU
        BZ      ik1
        SETV    MENU, M_OFF             ; shut (the guidance stays: play.c)
        FCALL   ik_blink0
        SETV    HUDDRT, 3
        LDI     1
        FRET
ik1:    SETV    T0, 1                   ; open, on the first we have
        LDI     N_ITEMS-1
        FCALL   next_item
        PUTV    ITEM
        XRI     I_NONE
        BZ      ik1e
        SETV    MENU, M_ITEM
        SETV    HUDDRT, 2
        LDI     1
        FRET
ik1e:   SND     UI_ERROR                ; (none to use)
        LDI     1
        FRET
ik2:    GETV    MENU
        BZ      ik9
        XRI     M_ITEM
        BNZ     ik3
        FJMP    ik_item
ik3:    GETV    MENU
        XRI     M_TARGET
        BNZ     ik4
        FJMP    ik_target
ik4:    FJMP    ik_drive
ik9:    LDI     0
        FRET

; ik_blink0: the turn's tank blinks again (ours), no target's.
        RFIT    18
ik_blink0:
        GETV    AIMING
        BZ      ikb1
        GETV    MYSEAT
        FJMP    tk_blinkset
ikb1:   LDI     NONE
        FJMP    tk_blinkset

; ik_item: 4 6 the item before or after, 5 uses it.
        RFIT    201
ik_item:
        FCALL   ik_step                 ; 4 6: T0 = FF or 1 (0: none)
        BZ      iki1
        GETV    ITEM
        FCALL   next_item
        XRI     I_NONE
        BZ      iki1
        XRI     I_NONE
        PUTV    ITEM
        SETV    HUDDRT, 2
        SND     UI_TICK
iki1:   GETV    KNEW                    ; 5
        ANI     20H
        BZ      iki9
        GETV    KNEW
        ANI     0DFH
        STR     RA
        SETV    HUDDRT, 2
        SND     UI_SELECT
        FCALL   defending
        BNZ     iki5
        GETV    ITEM                    ; guidance: a target next
        SMI     I_AUTOPILOT+1
        BDF     iki3
        GETV    ITEM
        PUTV    GUIDE
        SETV    T0, 1
        GETV    TARGET
        FCALL   next_target
        PUTV    TARGET
        XRI     NO_SEAT
        BZ      iki9
        SETV    MENU, M_TARGET
        GETV    TARGET
        FCALL   tk_blinkset
        BR      iki9
iki3:   GETV    ITEM                    ; fuel: driving
        XRI     I_FUEL
        BNZ     iki5
        SETV    MENU, M_DRIVE
        SETV    DRIVN, 0
        BR      iki9
iki5:   GETV    ITEM                    ; the rest: used
        PUTV    AARGS
        SETV    ANARG, 1
        LDI     ACT_USE
        FCALL   act_q
        FCALL   defending               ; (the window, the chute, the fuse: open)
        BNZ     iki9
        GETV    ITEM
        XRI     I_PARACHUTE
        BZ      iki9
        GETV    ITEM
        XRI     I_IMPACT_FUSE
        BZ      iki9
        SETV    MENU, M_OFF
        SETV    HUDDRT, 3
iki9:   LDI     1
        FRET

; ik_step: T0 = FF for 4 pressed, 1 for 6, else 0 (D the same); taken.
        RFIT    43
ik_step:
        GETV    KNEW
        ANI     50H
        BZ      iks0
        ANI     10H
        BZ      iks1
        GETV    KNEW
        ANI     0EFH
        STR     RA
        LDI     0FFH
        BR      iks2
iks1:   GETV    KNEW
        ANI     0BFH
        STR     RA
        LDI     1
        BR      iks2
iks0:   LDI     0
iks2:   PUTV    T0
        FRET

; ik_target: 4 6 the tank before or after (it blinks), 5 takes it.
        RFIT    76
ik_target:
        FCALL   ik_step
        BZ      ikt1
        GETV    TARGET
        FCALL   next_target
        XRI     NO_SEAT
        BZ      ikt1
        XRI     NO_SEAT
        PUTV    TARGET
        FCALL   tk_blinkset
        SETV    HUDDRT, 2
        SND     UI_TICK
ikt1:   GETV    KNEW
        ANI     20H
        BZ      ikt9
        GETV    KNEW
        ANI     0DFH
        STR     RA
        SETV    MENU, M_OFF             ; taken: our fire carries it
        SND     UI_SELECT
        FCALL   ik_blink0
        SETV    HUDDRT, 3
ikt9:   LDI     1
        FRET

; ik_drive: 4 6 held drive (ACT_DRIVE 0 left, 1 right, every 8 jiffies);
;   5 stops.
        RFIT    110
ik_drive:
        GETV    KNEW
        ANI     20H
        BZ      ikd1
        GETV    KNEW
        ANI     0DFH
        STR     RA
        SETV    MENU, M_OFF
        SETV    HUDDRT, 3
        LDI     1
        FRET
ikd1:   GETV    KHELD                   ; held: every 8 jiffies
        ANI     50H
        BNZ     ikd2
        SETV    DRIVN, 0
        LDI     1
        FRET
ikd2:   GETV    DRIVN
        BNZ     ikd3
        GETV    KHELD
        ANI     40H                     ; (6: right)
        BZ      ikd2a
        LDI     1
ikd2a:  PUTV    AARGS
        SETV    ANARG, 1
        LDI     ACT_DRIVE
        FCALL   act_q
        SETV    DRIVN, 8
        SETV    HUDDRT, 2
ikd3:   GETV    DT
        STR     R2
        GETV    DRIVN
        SM
        BDF     ikd4
        LDI     0
ikd4:   STR     RA
        LDI     1
        FRET

; ---------------------------------------------------------------------------
; The HUD's rows held (hud_rows): row 1 by a message, the defence window
; or YOUR TURN!; row 2 by a talk or the items line.
; ---------------------------------------------------------------------------
; row1_held, row2_held: D = 1 if held.
        RFIT    41
row1_held:
        GETV    HMSGT
        BNZ     rh1
        GETV    BANNERT
        BNZ     rh1
        FJMP    defending
rh1:    LDI     1
        FRET
row2_held:
        GETV    MENU
        BNZ     rh1
        GETV    HMSGT
        BZ      rh2
        GETV    MSGK
        XRI     MK_TALK
        BZ      rh1
rh2:    LDI     0
        FRET

; hud_line1: row 1 held: the message, DEFENCES, or YOUR TURN!
        RFIT    64
hud_line1:
        GETV    HMSGT
        BZ      hl11
        FJMP    msg_draw
hl11:   FCALL   hl_clear
        LDR     R8, s_yourt
        FCALL   defending
        BZ      hl12
        LDR     R8, s_def
hl12:   LDI     0
        FCALL   ln_str
        LDI     0
        FJMP    hl_draw
s_def:  DB      "DEFENCES  9 DONE", 0
s_yourt: DB     "   YOUR TURN!", 0

; hud_line2: row 2 held: the talk, or the items line: the item (11) and how
;   many; TARGET and its tank; DRIVE and the fuel.
        RFIT    145
hud_line2:
        GETV    MENU
        BNZ     hl21
        FJMP    msg_draw
hl21:   FCALL   hl_clear
        GETV    MENU
        XRI     M_TARGET
        BNZ     hl22
        LDR     R8, s_target            ; TARGET mark name
        LDI     0
        FCALL   ln_str
        SETV    T3, 7
        LDI     8
        PLO     RC
        GETV    TARGET
        FCALL   ln_seat
        BR      hl28
hl22:   GETV    MENU
        XRI     M_DRIVE
        BNZ     hl23
        LDR     R8, s_drive             ; DRIVE 46 FUEL nn
        LDI     0
        FCALL   ln_str
        LDI     I_FUEL
        BR      hl24
hl23:   FCALL   cat_check               ; the item: its name (11), how many
        BZ      hl23a
        GETV    ITEM
        FCALL   cat_item
        GLO     R8
        ADI     6
        PLO     R8
        GHI     R8
        ADCI    0
        PHI     R8
        LDI     11
        PLO     RC
        LDI     0
        FCALL   ln_lp
hl23a:  GETV    ITEM
hl24:   ADI     LO(ST+OFF_INV)          ; how many: 2 places at the end
        PLO     RB
        LDI     HI(ST+OFF_INV)
        ADCI    0
        PHI     RB
        LDN     RB
        PLO     R8
        LDI     0
        PHI     R8
        LDI     LO(HLINE+14)
        PLO     R9
        LDI     2
        FCALL   hl_num
hl28:   LDI     1
        FJMP    hl_draw
s_target: DB    "TARGET", 0
s_drive: DB     "DRIVE 46 FUEL", 0
