; tank.inc -- the tanks: the cart's sprites 0-9, one a seat, in MASK mode
; (an image and its halo: the tank shows on sky and in dirt alike). A
; seat's image is its mark over the barrel at its angle (15-degree steps)
; over the hull (tools/s2/mktank_s2.py: build/studio2/tank.inc), sent again
; only when the step changes, two a pass at most. The marks are app glyphs
; $80-$89 too, beside the names. The tank aiming blinks.
;
; TKON/TKX/TKY/TKST: each seat as wanted (seated and alive; the hull's
; bottom centre; the barrel's step); TDST the step its image holds (NONE:
; none sent), TDON its sprite shown; TKDRT the seats to see to (a bit
; each, 16 bits). tk_state sets them from the STATE, the player from its
; ops (OP_TANK, OP_FALL, OP_DEATH), the aim from the keys.

IM_TANK EQU     0               ; seat s: image 2s, its mask 2s + 1
GL_MARK EQU     80H             ; seat s's mark: glyph $80 + s
TK_BLINK EQU    16              ; jiffies a blink lasts (on, then off)

; tk_init: the marks' glyphs; every tank hidden, nothing sent, all to see to.
        RFIT    83
tk_init:
        LDI     0
        PLO     R9
        LDR     R8, MARKGLY
tki1:   PARS                            ; GLYPH $80 + s, its 12 rows
        GLO     R9
        ADI     GL_MARK
        PD
        LDI     12
        PLO     RC
tki2:   LDA     R8
        PD
        DEC     RC
        GLO     RC
        BNZ     tki2
        GOP     TOP_GLYPH
        INC     R9
        GLO     R9
        XRI     SEATS
        BNZ     tki1
        OPN     TOP_SPRHIDE             ; (no parameters: every sprite)
        LDR     R8, TDST
        LDI     SEATS*2
        PLO     R9
tki3:   LDI     NONE                    ; TDST, TDON: nothing
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     tki3
        SETV    TKDRT, 0FFH
        SETV    TKDRT+1, 3
        FRET

; tk_reset: after a CLS: every sprite hidden (the images kept), all to see to.
        RFIT    36
tk_reset:
        LDR     R8, TDON
        LDI     SEATS
        PLO     R9
tkr1:   LDI     0
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     tkr1
        SETV    TKDRT, 0FFH
        SETV    TKDRT+1, 3
        SETV    TKBL, NONE
        FRET

; ---------------------------------------------------------------------------
; tk_state -- the tanks as the STATE has them; a seat that changed is to be
;   seen to. T0 the seat.
; ---------------------------------------------------------------------------
        RFIT    81
tk_state:
        SETV    T0, 0
tkst1:   GETV    T0
        FCALL   seat_rec                ; RB: its record
        GLO     RB                      ; on: seated and alive
        ADI     P_KIND
        PLO     R8
        GHI     RB
        ADCI    0
        PHI     R8
        LDN     R8
        BZ      tkst2
        GLO     RB
        ADI     P_FLAGS
        PLO     R8
        GHI     RB
        ADCI    0
        PHI     R8
        LDN     R8
        ANI     PF_ALIVE
tkst2:   PLO     R9                      ; R9.0 on (0 or 1)
        GLO     RB                      ; x (the low byte: on the field)
        ADI     P_X
        PLO     R8
        GHI     RB
        ADCI    0
        PHI     R8
        LDA     R8
        PHI     R9                      ; R9.1 x
        INC     R8
        LDN     R8                      ; y
        PLO     RC
        GLO     RB                      ; the angle: its step (15 degrees)
        ADI     P_ANGLE
        PLO     R8
        GHI     RB
        ADCI    0
        PHI     R8
        LDN     R8
        FCALL   ang_step
        PHI     RC                      ; RC.1 the step
        GETV    T0                      ; tk_set: seat T0 = R9.0 on, R9.1 x,
        FCALL   tk_set                  ;  RC.0 y, RC.1 the step
        INCV    T0
        XRI     SEATS
        BNZ     tkst1
        FRET

; ang_step: D = the angle D's step (D / 15, 0-12). Keeps R8, R9, RB, RC.
        RFIT    18
ang_step:
        PHI     RD
        LDI     0
        STR     R2
ags1:    GHI     RD
        SMI     15
        BNF     ags2
        PHI     RD
        LDN     R2
        ADI     1
        STR     R2
        BR      ags1
ags2:    LDN     R2
        FRET

; tk_set: seat D as R9.0 (on), R9.1 (x), RC.0 (y), RC.1 (step): what
;   changed is set and the seat marked. Keeps T0; T1 the seat.
        RFIT    91
tk_set: PUTV    T1
        ADI     LO(TKON)                ; on
        PLO     RA
        GLO     R9
        STR     R2
        LDN     RA
        XOR
        BZ      tse1
        LDN     R2
        STR     RA
        GETV    T1
        FCALL   tk_mark
tse1:   GETV    T1                      ; x
        ADI     LO(TKX)
        PLO     RA
        GHI     R9
        STR     R2
        LDN     RA
        XOR
        BZ      tse2
        LDN     R2
        STR     RA
        GETV    T1
        FCALL   tk_mark
tse2:   GETV    T1                      ; y
        ADI     LO(TKY)
        PLO     RA
        GLO     RC
        STR     R2
        LDN     RA
        XOR
        BZ      tse3
        LDN     R2
        STR     RA
        GETV    T1
        FCALL   tk_mark
tse3:   GETV    T1                      ; the barrel's step
        ADI     LO(TKST)
        PLO     RA
        GHI     RC
        STR     R2
        LDN     RA
        XOR
        BZ      tse4
        LDN     R2
        STR     RA
        GETV    T1
        FCALL   tk_mark
tse4:   FRET

; tk_mark: seat D to be seen to (TKDRT). Keeps R8, R9, RB, RC.
        RFIT    38
tk_mark:
        SMI     8
        BDF     tkm2
        ADI     8+LO(BITS)              ; seats 0-7: the low byte
        PLO     RA                      ; (BITS's page: no carry)
        LDI     HI(BITS)
        PHI     RA
        LDN     RA
        STR     R2
        LDI     HI(RAMVARS)
        PHI     RA
        VAR     TKDRT
        BR      tkm3
tkm2:   ADI     LO(BITS)                ; 8-9: the high
        PLO     RA
        LDI     HI(BITS)
        PHI     RA
        LDN     RA
        STR     R2
        LDI     HI(RAMVARS)
        PHI     RA
        VAR     TKDRT+1
tkm3:   LDN     RA
        OR
        STR     RA
        FRET

; ---------------------------------------------------------------------------
; tk_pass -- the seats to see to: each one's image sent if its step
;   changed (two a pass at most: the rest wait), its sprite placed or
;   hidden; then the blink. T0 the seat, T1 uploads left.
; ---------------------------------------------------------------------------
        RFIT    41
tk_pass:
        SETV    T1, 2
        SETV    T0, 0
tkp1:   GETV    T0                      ; to see to?
        FCALL   tk_bit                  ; (D = its bit in TKDRT, RA on the byte)
        STR     R2
        LDN     RA
        AND
        BZ      tkp8
        FJMP    tk_one
tkp8:   INCV    T0
        XRI     SEATS
        BNZ     tkp1
        FJMP    tk_blink

; tk_bit: D = seat D's bit, RA on its TKDRT byte. Keeps R8, R9, RB, RC.
        RFIT    25
tk_bit: SMI     8
        BDF     tkb2
        ADI     8
        PHI     RD
        VAR     TKDRT
        BR      tkb3
tkb2:   PHI     RD
        VAR     TKDRT+1
tkb3:   GHI     RD
        ADI     LO(BITS)
        PLO     RB
        LDI     HI(BITS)
        PHI     RB
        LDN     RB
        FRET

; tk_one: seat T0 seen to, then the loop again (tkp8).
        RFIT    97
tk_one: GETV    T0
        ADI     LO(TKON)
        PLO     RA
        LDN     RA
        BNZ     tko2
        GETV    T0                      ; off: hidden (if it was shown)
        ADI     LO(TDON)
        PLO     RA
        LDN     RA
        BZ      tko9
        LDI     0
        STR     RA
        GETV    T0
        FCALL   tk_hide
        BR      tko9
tko2:   GETV    T0                      ; on: its image, if its step changed
        ADI     LO(TKST)
        PLO     RA
        LDN     RA
        STR     R2
        GETV    T0
        ADI     LO(TDST)
        PLO     RA
        LDN     RA
        XOR
        BZ      tko4
        GETV    T1                      ; (none left this pass: later)
        BZ      tko8
        SMI     1
        STR     RA
        FCALL   tk_image
tko4:   GETV    T0                      ; its sprite where it is
        FCALL   tk_show
        GETV    T0
        ADI     LO(TDON)
        PLO     RA
        LDI     1
        STR     RA
tko9:   GETV    T0                      ; seen to
        FCALL   tk_bit
        XRI     0FFH
        STR     R2
        LDN     RA
        AND
        STR     RA
tko8:   FJMP    tkp8

; tk_image: seat T0's image and mask sent: its mark | the barrel at its
;   step | the hull, and their halos. TDST = the step. T2 the step.
        RFIT    149
tk_image:
        GETV    T0
        ADI     LO(TKST)
        PLO     RA
        LDN     RA
        PUTV    T2
        GETV    T0
        ADI     LO(TDST)
        PLO     R8
        LDI     HI(TDST)
        PHI     R8
        GETV    T2
        STR     R8
        LDI     0
        PLO     RC                      ; 0 the image, 1 the mask
tim1:   PARS                            ; IMAGE 2s (+1), 8 x 16
        GETV    T0
        SHL
        STR     R2
        GLO     RC
        ADD
        ADI     IM_TANK
        PD
        P       8
        P       TK_H
        GETV    T0                      ; R8: the mark, R9: the barrel, RB: the hull
        SHL
        SHL
        SHL
        SHL
        ADI     LO(TKMARK)
        PLO     R8
        LDI     HI(TKMARK)
        PHI     R8
        GETV    T2
        SHL
        SHL
        SHL
        SHL
        ADI     LO(TKBAR)
        PLO     R9
        LDI     HI(TKBAR)
        PHI     R9
        LDR     RB, TKHULL
        GLO     RC                      ; the mask: the halos'
        BZ      tim2
        GLO     R8
        ADI     LO(TKMARKM-TKMARK)
        PLO     R8
        GHI     R8
        ADCI    HI(TKMARKM-TKMARK)
        PHI     R8
        GLO     R9
        ADI     LO(TKBARM-TKBAR)
        PLO     R9
        GHI     R9
        ADCI    HI(TKBARM-TKBAR)
        PHI     R9
        LDR     RB, TKHULLM
tim2:   LDI     TK_H
        PHI     RC                      ; the rows
tim3:   LDA     R8
        STR     R2
        LDA     R9
        OR
        STR     R2
        LDA     RB
        OR
        PD
        GHI     RC
        SMI     1
        PHI     RC
        BNZ     tim3
        GOP     TOP_IMAGE
        GLO     RC
        BNZ     tim9
        INC     RC
        BR      tim1
tim9:   FRET

; tk_show: seat D's sprite at its tank (x - 3, y - 16), MASK mode.
        RFIT    63
tk_show:
        PUTV    T3
        PARS
        GETV    T3                      ; sprite s
        PD
        GETV    T3                      ; image 2s
        SHL
        ADI     IM_TANK
        PD
        GETV    T3
        ADI     LO(TKX)
        PLO     RA
        LDN     RA
        SMI     TK_CX
        PD
        GETV    T3
        ADI     LO(TKY)
        PLO     RA
        LDN     RA
        SMI     TK_H
        PD
        P       TM_MASK
        GOP     TOP_SPRITE
        FRET

; tk_hide: seat D's sprite hidden.
        RFIT    41
tk_hide:
        FCALL   tk_bit                  ; (its bit; the byte: the seat's half)
        PHI     RC
        GLO     RA
        XRI     LO(TKDRT)
        PLO     RC                      ; RC.0 0: the low byte
        PARS
        GLO     RC
        BNZ     tkh1
        GHI     RC
        PD
        P       0
        BR      tkh2
tkh1:   P       0
        GHI     RC
        PD
tkh2:   GOP     TOP_SPRHIDE
        FRET

; tk_blink: the seat aiming (TKBL, NONE: none) off for TK_BLINK jiffies,
;   then on for as long.
        RFIT    51
tk_blink:
        GETV    TKBL
        XRI     NONE
        BZ      tbl9
        GETV    JIFFY                   ; the phase: on, or off
        ANI     TK_BLINK
        STR     R2
        GETV    TKBLP
        XOR
        BZ      tbl9
        LDN     R2
        STR     RA
        BZ      tbl2
        GETV    TKBL                    ; off
        FJMP    tk_hide
tbl2:   GETV    TKBL                    ; on, if it is
        ADI     LO(TKON)
        PLO     RA
        LDN     RA
        BZ      tbl9
        GETV    TKBL
        FJMP    tk_show
tbl9:   FRET

; tk_blinkset: the seat D blinks (NONE: none; the one before is shown).
        RFIT    46
tk_blinkset:
        PUTV    T4
        STR     R2
        GETV    TKBL
        XOR
        BZ      tbs9
        GETV    TKBL                    ; the one before: shown again
        XRI     NONE
        BZ      tbs1
        GETV    TKBL
        FCALL   tk_mark
tbs1:   GETV    T4
        PUTV    TKBL
        SETV    TKBLP, 0
tbs9:   FRET
