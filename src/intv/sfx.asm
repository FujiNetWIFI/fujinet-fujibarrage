;; ===========================================================================
;; sfx.asm -- the effects on the PSG (sound.bas's snd_cue), designed for it
;; (src/atari/sound.c says what each is). Two channels: voice B, the
;; effect; voice C, its low end (a boom's rumble, a shot's thump), only
;; while no tune has C. Booms are noise on the hardware envelope, so they
;; die away smoothly; the rest are steps.
;;
;; A step: a word of what (bits 0-11 a tone's period, 111861 / Hz; bit 12
;; noise instead, of period bits 0-4, the tone off; bit 13 on the envelope:
;; its period EP in a third word, the decay restarted: its 16 steps take
;; EP x 0.143 ms), a word of how long (bits 0-3 the volume, a step 3 dB;
;; 4-11 jiffies; 0 ends it).
;;
;; The PSG: A $1F0/$1F4/$1FB, B $1F1/$1F5/$1FC, C $1F2/$1F6/$1FD (period
;; low, high, volume); the envelope's period $1F3/$1F7, its shape $1FA; the
;; mixer $1F8, the noise's period $1F9.
;; ===========================================================================

SX_N    EQU     $1000                   ; (a step's flags)
SX_E    EQU     $2000

;; SFX_PLAY(b, c): effect b on voice B, c on voice C (0: none; C only if
;; no tune has it)
SFX_PLAY: PROC
        DIS
        TSTR    R0
        BEQ     @@c
        MVO     R0, const_SX_P0
        CLRR    R2
        MVO     R2, const_SX_N0
@@c:    TSTR    R1
        BEQ     @@x
        MVI     var_MUS_ON, R2
        ANDI    #2, R2
        BNEQ    @@x
        MVO     R1, const_SX_P1
        CLRR    R2
        MVO     R2, const_SX_N1
@@x:    EIS
        JR      R5
        ENDP

;; SFX_STOP: both quiet
SFX_STOP: PROC
        PSHR    R5
        DIS
        CLRR    R0
        MVO     R0, const_SX_P0
        MVO     R0, $1FC
        MVO     R0, const_SX_NZ
        MVI     var_MUS_ON, R1
        ANDI    #2, R1
        BNEQ    @@m
        MVO     R0, const_SX_P1
        MVO     R0, $1FD
@@m:    CALL    MIXW
        EIS
        PULR    PC
        ENDP

;; MIXW: the mixer and the noise's period: tones on; the tune's drum on A
;; (its tone off), an effect's noise on B or C (theirs off); the noise's
;; period the effects' if they have it, else the drum's
MIXW:   PROC
        MVII    #$38, R0
        MVI     var_MUS_NP, R1
        MVI     var_MUS_NA, R2
        TSTR    R2
        BEQ     @@b
        XORI    #$09, R0                ; A: noise on, tone off
@@b:    MVI     const_SX_NZ, R2
        MOVR    R2, R3
        ANDI    #1, R3
        BEQ     @@c
        XORI    #$12, R0                ; B
        MVI     const_SX_NP, R1
@@c:    ANDI    #2, R2
        BEQ     @@w
        XORI    #$24, R0                ; C
        MVI     const_SX_NP, R1
@@w:    TSTR    R1
        BNEQ    @@n
        INCR    R1
@@n:    MVO     R1, $1F9
        MVO     R0, $1F8
        JR      R5
        ENDP

;; SFX_ISR: a jiffy of both (the frame handler's). SX_P0/1: each one's
;; next step (0: quiet); SX_N0/1: the jiffies left of its step.
SFX_ISR: PROC
        PSHR    R5
        CLRR    R3                      ; channel 0 (B), then 1 (C)
@@ch:   MOVR    R3, R4
        ADDI    #const_SX_P0, R4
        MVI@    R4, R1
        TSTR    R1
        BEQ     @@nx
        MOVR    R3, R4
        ADDI    #const_SX_N0, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@step                  ; (just started)
        DECR    R4
        DECR    R0
        MVO@    R0, R4
        BNEQ    @@nx
@@step: MOVR    R1, R4
        MVI@    R4, R0                  ; what
        MVI@    R4, R2                  ; how long, how loud
        TSTR    R2
        BNEQ    @@go
        CLRR    R1                      ; the end: quiet, no noise
        MOVR    R3, R4
        ADDI    #const_SX_P0, R4
        MVO@    R1, R4
        MOVR    R3, R4
        ADDI    #$1FC, R4
        MVO@    R1, R4
        MVI     const_SX_NZ, R1
        MOVR    R3, R2
        INCR    R2
        COMR    R2
        ANDR    R2, R1
        MVO     R1, const_SX_NZ
        B       @@mix
@@go:   PSHR    R0
        MOVR    R2, R1                  ; jiffies
        SLR     R1, 2
        SLR     R1, 2
        ANDI    #255, R1
        MOVR    R3, R5
        ADDI    #const_SX_N0, R5
        MVO@    R1, R5
        ANDI    #15, R2                 ; the volume
        ANDI    #SX_E, R0
        BEQ     @@ne
        MVI@    R4, R0                  ; the envelope: its period, the decay again
        MVO     R0, $1F3
        SWAP    R0
        MVO     R0, $1F7
        CLRR    R0
        MVO     R0, $1FA
        MVII    #$30, R2
@@ne:   MOVR    R3, R5                  ; the next step
        ADDI    #const_SX_P0, R5
        MVO@    R4, R5
        MVI     var_SND_OFF, R0         ; muted
        ANDI    #1, R0
        BEQ     @@v
        CLRR    R2
@@v:    PULR    R0
        MOVR    R3, R4                  ; the volume
        ADDI    #$1FC, R4
        MVO@    R2, R4
        MVI     const_SX_NZ, R1         ; noise, or a tone
        MOVR    R3, R2
        INCR    R2                      ; (its bit)
        MOVR    R0, R4
        ANDI    #SX_N, R4
        BEQ     @@tone
        COMR    R2
        ANDR    R2, R1
        COMR    R2
        XORR    R2, R1
        MVO     R1, const_SX_NZ
        ANDI    #31, R0
        MVO     R0, const_SX_NP
        B       @@mix
@@tone: COMR    R2
        ANDR    R2, R1
        MVO     R1, const_SX_NZ
        ANDI    #$0FFF, R0
        MOVR    R3, R4
        ADDI    #$1F1, R4
        MVO@    R0, R4
        ADDI    #3, R4
        SWAP    R0
        MVO@    R0, R4
@@mix:  PSHR    R3
        CALL    MIXW
        PULR    R3
@@nx:   INCR    R3
        CMPI    #2, R3
        BNEQ    @@ch
        PULR    PC
        ENDP

;; ---- the effects (a step "what,how" as above; an
;; envelope step's third word its period: the decay takes EP x 0.143 ms)
sx_fire:  DECLE SX_N+5,$2F, SX_N+8,$2D, SX_N+12,$3B, SX_N+16,$38, SX_N+20,$45, 0,0
sx_thump: DECLE 1400,$2E, 1800,$2D, 2200,$3B, 2700,$38, 3200,$45, 0,0
sx_boom0: DECLE SX_E+SX_N+14,$1E0,3500, 0,0
sx_boom1: DECLE SX_E+SX_N+20,$360,6300, 0,0
sx_boom2: DECLE SX_E+SX_N+26,$5A0,10500, 0,0
sx_boom3: DECLE SX_E+SX_N+31,$A80,19500, 0,0
sx_crack: DECLE SX_N+3,$2F, SX_N+6,$2E, SX_E+SX_N+31,$840,15000, 0,0
sx_rumb1: DECLE 2800,$6D, 3200,$8C, 3600,$AA, 4000,$C7, 0,0
sx_rumb3: DECLE SX_E+3800,$A80,19500, 0,0
sx_napalm: DECLE SX_N+4,$2C, SX_N+18,$2B, SX_N+6,$2B, SX_N+24,$3A, SX_N+9,$39, SX_N+28,$48, SX_N+7,$47, SX_N+30,$65, 0,0
sx_laser: DECLE 60,$1F, 80,$1E, 110,$1D, 150,$1C, 200,$2B, 280,$2A, 400,$38, 560,$36, 0,0
sx_plasma: DECLE 180,$2C, 220,$2C, 180,$2B, 220,$2B, 180,$2A, 220,$29, 180,$38, 0,0
sx_dig:   DECLE SX_N+20,$3B, 1800,$3B, SX_N+22,$3A, 1900,$3A, SX_N+24,$49, 0,0
sx_roll:  DECLE 2600,$4B, 2900,$4A, 2600,$4A, 3100,$68, 0,0
sx_dirt:  DECLE SX_N+25,$6A, SX_N+27,$88, SX_N+29,$A6, 0,0
sx_shield: DECLE 300,$2D, 260,$2D, 220,$3C, 300,$49, 0,0
sx_deflect: DECLE 90,$2E, 100,$2D, 120,$3C, 140,$4A, 160,$58, 0,0
sx_chute: DECLE SX_N+14,$A6, SX_N+12,$A7, SX_N+15,$A6, 0,0
sx_death: DECLE 250,$4D, 300,$4D, 360,$4C, 430,$5C, 520,$6B, 620,$6A, 740,$89, 880,$A7, 0,0
sx_bounce: DECLE 400,$2C, 300,$2B, 350,$3A, 0,0
sx_tick:  DECLE 150,$1A, 0,0
sx_utick: DECLE 100,$1A, 0,0
sx_select: DECLE 180,$3C, 0,0
sx_go:    DECLE 180,$6C, 140,$6C, 110,$CC, 0,0
sx_error: DECLE 1500,$AC, 0,0
sx_turn:  DECLE 214,$5A, 170,$5B, 143,$5C, 107,$8F, 107,$5D, 107,$4B, 107,$49, 0,0 ; (a fanfare)
sx_talk:  DECLE 160,$29, 200,$29, 140,$28, 180,$27, 0,0

;; each cue's (B, C): SND_* 0-15, then a boom's sizes, then the UI's
sx_cues: DECLE sx_fire, sx_thump          ; SND_FIRE
        DECLE   sx_boom0, 0               ; SND_BOOM (size: below)
        DECLE   sx_boom3, sx_rumb3        ; SND_NUKE
        DECLE   sx_napalm, 0
        DECLE   sx_laser, 0
        DECLE   sx_plasma, 0
        DECLE   sx_dig, 0
        DECLE   sx_roll, 0
        DECLE   sx_dirt, 0
        DECLE   sx_shield, 0
        DECLE   sx_deflect, 0
        DECLE   sx_chute, 0
        DECLE   sx_death, 0
        DECLE   sx_bounce, 0
        DECLE   sx_crack, sx_rumb3        ; SND_THUNDER
        DECLE   sx_tick, 0                ; SND_TICK
sx_booms: DECLE sx_boom0, 0, sx_boom1, sx_rumb1, sx_boom2, sx_rumb1, sx_boom3, sx_rumb3
sx_ui:  DECLE   sx_utick, sx_select, sx_go, sx_error, sx_turn, sx_talk

;; SFX_CUE(id, size): what a cue sounds like
SFX_CUE: PROC
        CMPI    #64, R0
        BNC     @@snd
        SUBI    #64, R0                 ; the UI's: voice B
        CMPI    #6, R0
        BC      @@x
        ADDI    #sx_ui, R0
        MOVR    R0, R4
        MVI@    R4, R0
        CLRR    R1
        B       SFX_PLAY
@@snd:  CMPI    #16, R0
        BC      @@x
        CMPI    #const_SND_BOOM, R0     ; a boom: by its size
        BNEQ    @@t
        ANDI    #3, R1
        SLL     R1, 1
        ADDI    #sx_booms, R1
        MOVR    R1, R4
        B       @@pl
@@t:    SLL     R0, 1
        ADDI    #sx_cues, R0
        MOVR    R0, R4
@@pl:   MVI@    R4, R0
        MVI@    R4, R1
        B       SFX_PLAY
@@x:    JR      R5
        ENDP
