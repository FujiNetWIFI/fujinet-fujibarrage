; p_hud.inc -- our turn on the field (src/play.c turn_check, the aim):
;
;   4 6   the angle (held: faster)       2 8   the power (held: faster)
;   1 3   the weapon before, after        5    fire
;
; The aim goes to the server (ACT_AIM) every 8 jiffies while it changes;
; our tank's barrel follows it. AIMING: ours to aim; AIMA, AIMP, AWPN.
; HUDDRT: the HUD's rows to build again (hud_rows).

AIM_EVERY EQU   8               ; jiffies between ACT_AIMs

; turn_check: our turn begins (PH_AIM, our seat alive, the turn ours, or
;   everyone's and we're not done): the aim as the server has it; ends:
;   AIMING 0. The aiming seat blinks.
        RFIT    148
turn_check:
        GETV    MYSEAT
        SMI     SEATS
        BDF     tch0
        GETS    OFF_PHASE
        XRI     PH_AIM
        BNZ     tch0
        GETV    MYSEAT
        FCALL   seat_rec
        GLO     RB
        ADI     P_FLAGS
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDN     RB
        PLO     R9                      ; (our flags)
        ANI     PF_ALIVE
        BZ      tch0
        GETS    OFF_TURN                ; the turn is ours
        STR     R2
        GETV    MYSEAT
        XOR
        BZ      tch1
        GETS    OFF_TURN                ; everyone's, and we're not done
        XRI     NO_SEAT
        BNZ     tch0
        GLO     R9
        ANI     PF_DONE
        BNZ     tch0
tch1:    GETV    AIMING                  ; ours: just now?
        BNZ     tch9
        GETV    FIRED                   ; (fired: a commit from before the
        BNZ     tch9                    ;  server had it isn't a new turn)
        FJMP    tch_arm
tch0:    GETV    AIMING
        BZ      tch0b
        SETV    HUDDRT, 1
tch0b:   SETV    AIMING, 0
        SETV    FIRED, 0
        GETS    OFF_PHASE               ; another's turn: theirs blinks
        XRI     PH_AIM
        BNZ     tch0a
        GETS    OFF_TURN
        SMI     SEATS
        BDF     tch0a
        GETS    OFF_TURN
        FJMP    tk_blinkset
tch0a:   LDI     NONE
        FJMP    tk_blinkset
tch9:    FRET

; tch_arm: our turn begins: the aim as the server has it, YOUR TURN!
        RFIT    134
tch_arm:
        SND     UI_TURN
        GETV    JIFFY+1
        PUTV    IDLEAT
        SETV    AIMING, 1
        SETV    HUDDRT, 3
        SETV    BANNERT, BANNER_J       ; YOUR TURN!
        SETV    KNEW, 0                 ; (presses before it aren't the aim's)
        SETV    KNEW+1, 0
        GETV    MYSEAT                  ; the aim as the server has it
        FCALL   seat_rec
        GLO     RB
        ADI     P_ANGLE
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDA     RB
        PUTV    AIMA
        PUTV    SENTA
        LDA     RB
        PUTV    AIMP
        PUTV    SENTP
        LDN     RB
        PUTV    AIMP+1
        PUTV    SENTP+1
        GLO     RB
        ADI     P_WEAPON-P_POWER-1
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDN     RB
        PUTV    AWPN
        GLO     RE
        PUTV    AIMT
        SETV    AHT, 0
        GETV    MYSEAT
        FJMP    tk_blinkset

; aim_pass: our aim's keys, as held (KHELD) and pressed (KNEW, taken):
;   a way pressed moves the aim a step; held AIM_WAIT jiffies, a step a
;   jiffy (whatever the pass's length). 1, 3: the weapon; 5: fire. Then
;   ACT_AIM if it changed (aim_send). T2 the steps; R8 the ways active.
AIM_WAIT EQU    18
WAYS    EQU     54H             ; KHELD's bits: 2 (04), 4 (10), 6 (40); 8: KHELD+1 bit 0

        RFIT    242
aim_pass:
        GETV    AIMING
        BNZ     ap1
        FRET
ap1:    GETV    KNEW                    ; the presses, taken
        PLO     R9
        GETV    KNEW+1
        PHI     R9
        SETV    KNEW, 0
        SETV    KNEW+1, 0
        GLO     R9                      ; 5: fire
        ANI     20H
        BZ      ap2
        FJMP    aim_fire
ap2:    GLO     R9                      ; 1, 3: the weapon before, after
        ANI     02H
        BZ      ap3
        LDI     1
        FCALL   aim_weapon
ap3:    GLO     R9
        ANI     08H
        BZ      ap4
        LDI     3
        FCALL   aim_weapon
ap4:    GETV    KHELD                   ; the ways active: held or pressed
        STR     R2
        GLO     R9
        OR
        ANI     WAYS
        PLO     R8
        GETV    KHELD+1
        STR     R2
        GHI     R9
        OR
        ANI     1
        PHI     R8
        GLO     R9                      ; a way pressed now: a step
        ANI     WAYS
        STR     R2
        GHI     R9
        ANI     1
        OR
        BZ      ap5
        SETV    T2, 1
        SETV    AHT, 0
        BR      ap7
ap5:    GLO     R8                      ; held: after AIM_WAIT, DT steps
        STR     R2
        GHI     R8
        OR
        BZ      ap9
        GETV    DT
        STR     R2
        GETV    AHT
        ADD
        BNF     ap6
        LDI     0FFH
ap6:    STR     RA
        SMI     AIM_WAIT
        BNF     ap9
        GETV    DT
        PUTV    T2
ap7:    GLO     R8                      ; 4: the angle up
        ANI     10H
        BZ      ap7a
        GETV    T2
        STR     R2
        GETV    AIMA
        ADD
        SMI     181
        BNF     ap71
        LDI     0
ap71:   ADI     181
        STR     RA
ap7a:   GLO     R8                      ; 6: down
        ANI     40H
        BZ      ap7b
        GETV    T2
        STR     R2
        GETV    AIMA
        SM
        BDF     ap72
        LDI     0
ap72:   STR     RA
ap7b:   GLO     R8                      ; 2: the power up, 8: down
        ANI     04H
        BZ      ap7c
        FCALL   aim_pup
ap7c:   GHI     R8
        BZ      ap8
        FCALL   aim_pdn
ap8:    GETV    HUDDRT                  ; the first row again, the barrel
        ORI     1
        STR     RA
        GETV    MYSEAT
        PUTV    T1
        GETV    AIMA
        FCALL   ang_step
        STR     R2
        GETV    T1
        ADI     LO(TKST)
        PLO     RA
        LDN     RA
        XOR
        BZ      ap9
        LDN     R2
        STR     RA
        GETV    T1
        FCALL   tk_mark
ap9:    FJMP    aim_send

; aim_pup: the power up 5 T2, to energy x 10. Keeps R8.
        RFIT    84
aim_pup:
        GETV    T2                      ; R9 = AIMP + 5 T2
        PLO     R9
        SHL
        SHL
        STR     R2
        GLO     R9
        ADD
        STR     R2
        GETV    AIMP
        ADD
        PLO     R9
        GETV    AIMP+1
        ADCI    0
        PHI     R9
        GLO     R8                      ; (aim_pmax: RC; R8 kept for us)
        PUTV    T3
        GHI     R8
        PUTV    T4
        GLO     R9
        PLO     R8
        GHI     R9
        PHI     R8
        FCALL   aim_pmax                ; R9 = the most
        GLO     R9                      ; R8 > R9: the most
        STR     R2
        GLO     R8
        SD
        GHI     R9
        STR     R2
        GHI     R8
        SDB
        BDF     apu1
        GLO     R9
        PLO     R8
        GHI     R9
        PHI     R8
apu1:   GLO     R8
        PUTV    AIMP
        GHI     R8
        PUTV    AIMP+1
        GETV    T3
        PLO     R8
        GETV    T4
        PHI     R8
        FRET

; aim_pdn: the power down 5 T2, to 0. Keeps R8.
        RFIT    42
aim_pdn:
        GETV    T2
        PLO     R9
        SHL
        SHL
        STR     R2
        GLO     R9
        ADD
        STR     R2
        GETV    AIMP
        SM
        PLO     R9
        GETV    AIMP+1
        SMBI    0
        BDF     apd1
        LDI     0
        PLO     R9
apd1:   PUTV    AIMP+1
        GLO     R9
        PUTV    AIMP
        FRET

; aim_pmax: R9 = our energy x 10 (the most power). Keeps R8.
        RFIT    49
aim_pmax:
        GETV    MYSEAT
        FCALL   seat_rec
        GLO     RB
        ADI     P_ENERGY
        PLO     RB
        GHI     RB
        ADCI    0
        PHI     RB
        LDN     RB
        PLO     RC                      ; RC = 2e
        LDI     0
        PHI     RC
        GLO     RC
        SHL
        PLO     RC
        GHI     RC
        SHLC
        PHI     RC
        GLO     RC                      ; R9 = 8e
        SHL
        PLO     R9
        GHI     RC
        SHLC
        PHI     R9
        GLO     R9
        SHL
        PLO     R9
        GHI     R9
        SHLC
        PHI     R9
        GLO     RC                      ; + 2e
        STR     R2
        GLO     R9
        ADD
        PLO     R9
        GHI     RC
        STR     R2
        GHI     R9
        ADC
        PHI     R9
        FRET

; aim_weapon: key D (1 before, 3 after): the next weapon we have (the
;   missile always), ACT_WEAPON. T1 the direction, T2 the tries.
        RFIT    104
aim_weapon:
        PUTV    T1
        SETV    T2, N_WEAPONS
aw1:    GETV    T1                      ; the next, round the weapons
        XRI     1
        BZ      aw2
        GETV    AWPN
        ADI     1
        STR     RA
        XRI     N_WEAPONS
        BNZ     aw3
        STR     RA
        BR      aw3
aw2:    GETV    AWPN
        BNZ     aw2a
        LDI     N_WEAPONS
aw2a:   SMI     1
        STR     RA
aw3:    GETV    AWPN                    ; ours? (the missile, or we have some)
        BZ      aw5
        ADI     LO(ST+OFF_INV)
        PLO     RB
        LDI     HI(ST+OFF_INV)
        ADCI    0
        PHI     RB
        LDN     RB
        BNZ     aw5
        GETV    T2
        SMI     1
        STR     RA
        BNZ     aw1
aw5:    SND     UI_SELECT
        GETV    AWPN
        PUTV    AARGS
        SETV    ANARG, 1
        GETV    HUDDRT
        ORI     2
        STR     RA
        LDI     ACT_WEAPON
        FJMP    act_q

; aim_send: ACT_AIM (angle, power) if it changed and 8 jiffies have gone.
        RFIT    114
aim_send:
        GETV    AIMT                    ; RE.0 - AIMT >= 8
        STR     R2
        GLO     RE
        SM
        SMI     AIM_EVERY
        BNF     as9
        GETV    SENTA                   ; changed?
        STR     R2
        GETV    AIMA
        XOR
        BNZ     as1
        GETV    SENTP
        STR     R2
        GETV    AIMP
        XOR
        BNZ     as1
        GETV    SENTP+1
        STR     R2
        GETV    AIMP+1
        XOR
        BZ      as9
as1:    GETV    AIMA
        PUTV    SENTA
        PUTV    AARGS
        GETV    AIMP
        PUTV    SENTP
        PUTV    AARGS+1
        GETV    AIMP+1
        PUTV    SENTP+1
        PUTV    AARGS+2
        GLO     RE
        PUTV    AIMT
        SETV    ANARG, 3
        LDI     ACT_AIM
        FJMP    act_q
as9:    FRET

; aim_fire: ACT_FIRE (angle, power, weapon, no guidance); not ours now.
        RFIT    106
aim_fire:
        GETV    AIMA
        PUTV    AARGS
        GETV    AIMP
        PUTV    AARGS+1
        GETV    AIMP+1
        PUTV    AARGS+2
        GETV    AWPN
        PUTV    AARGS+3
        GETV    GUIDE                   ; the guidance, its target (then none)
        PUTV    AARGS+4
        GETV    TARGET
        PUTV    AARGS+5
        SETV    GUIDE, I_NONE
        SETV    TARGET, NO_SEAT
        SETV    ANARG, 6
        LDI     ACT_FIRE
        FCALL   act_q
        SETV    FIRED, 1
        SETV    AIMING, 0
        SETV    HUDDRT, 3
        LDI     NONE
        FJMP    tk_blinkset
