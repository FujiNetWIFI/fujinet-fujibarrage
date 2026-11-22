;
; mock.s -- the sound (sound.h) on a Mockingboard: its first AY-3-8910,
; through its first 6522 (port A the AY's bus, port B its BC1, BDIR and
; /RESET). Three voices:
;   A  an effect's body, or a tune's first channel
;   B  the falling shell's whistle and a talker's blips, or a tune's second
;   C  an effect's low end (a boom's rumble), or, while a tune plays, the
;      effect's body (the menus' cues)
; The effects are tools/mksfx_a2.py's steps (from the Intellivision's,
; scaled to this clock), the tunes tools/mkmus_a2.py's words; a jiffy at a
; time (mk_tick, from lowlevel.s's clock). Nothing here uses zero page: the
; 6522's addresses and the steps' are written into the code (it is RAM).
;
; No Mockingboard: the speaker, which sounds only while the CPU clicks it,
; gives each cue a short tone or burst of noise (4-70 ms: spk_tab) and a
; talker's letters a click; no tunes, no whistle.
;
        .export _mk_tick
        .export _snd_init := _mk_init, _snd_cue := _mk_cue, _snd_stop := _mk_stop
        .export _snd_tune := _mk_tune, _snd_whistle := _mk_whistle, _snd_blip := _mk_blip
        .export _snd_busy := _mk_busy, _snd_full
        .importzp _mb_slot
        .import sx_cues, sx_booms, sx_ui, mus_per
        .import popa, popax

R_NP    = 6                     ; the AY's registers
R_MIX   = 7
R_VA    = 8
R_EPL   = 11
R_EPH   = 12
R_ESH   = 13
NOISE0  = 112                   ; (tools/mkmus_a2.py) a tune's noise drums from here

        .bss
fx_lo:  .res 2                  ; each effect's next step (high 0: quiet): 0 the body, 1 the low end
fx_hi:  .res 2
fx_left: .res 2                 ; jiffies left of its step
fx_vo:  .res 2                  ; its voice (0 A, 2 C)
mu_on:  .res 1                  ; a tune's channels still going (bits 0, 1)
mu_lo:  .res 2                  ; each channel's next word
mu_hi:  .res 2
mu_rlo: .res 2                  ; where a call returns
mu_rhi: .res 2
mu_left: .res 2
bl_left: .res 1                 ; a blip's jiffies left
mix:    .res 1                  ; R7 as written
vo:     .res 1
sl:     .res 1
vb:     .res 1
pl:     .res 1
ph:     .res 1
id:     .res 1

        .code

; ---- the AY --------------------------------------------------------------

; register X = A (the 6522's addresses written in by mk_init)
ayw:
ora1:   stx     $C401           ; ORA: the register
        ldx     #$07            ; latch it
orb1:   stx     $C400
        ldx     #$04
orb2:   stx     $C400
ora2:   sta     $C401           ; ORA: the value
        ldx     #$06            ; write
orb3:   stx     $C400
        ldx     #$04
orb4:   stx     $C400
        rts

; void mk_init(void): the 6522's ports, the AY reset and quiet (a
; Mockingboard in mb_slot; else nothing here plays)
_mk_init:
        lda     _mb_slot
        beq     mkx
        sta     ora1+2
        sta     ora2+2
        sta     orb1+2
        sta     orb2+2
        sta     orb3+2
        sta     orb4+2
        sta     ddra+2
        sta     ddrb+2
        sta     orb5+2
        lda     #$FF            ; DDRA: the AY's bus
ddra:   sta     $C403
        lda     #$07            ; DDRB: BC1, BDIR, /RESET
ddrb:   sta     $C402
        lda     #$00            ; a reset
        jsr     orbx
        lda     #$04
        jsr     orbx
        jmp     _mk_stop
mkx:    rts

; ORB = A
orbx:
orb5:   sta     $C400
        rts

; void snd_full(uint8_t on): (the Atari's: four voices while the link is
; held; here all three are the field's always)
_snd_full:
        rts

; void mk_stop(void): every voice quiet, no tune, no effect
_mk_stop:
        lda     #0
        sta     fx_hi
        sta     fx_hi+1
        sta     mu_on
        sta     bl_left
        lda     _mb_slot
        beq     @x
        lda     #0
        ldx     #R_VA
        jsr     ayw
        lda     #0
        ldx     #R_VA+1
        jsr     ayw
        lda     #0
        ldx     #R_VA+2
        jsr     ayw
        lda     #$3F            ; tones and noise off
        sta     mix
        ldx     #R_MIX
        jmp     ayw
@x:     rts

; voice vo's volume = A
vol:    ldx     vo
        pha
        lda     vreg,x
        tax
        pla
        jmp     ayw

; voice vo a tone (noise off): its period pl, ph
tone:   ldx     vo
        lda     preg,x
        tax
        lda     pl
        jsr     ayw
        ldx     vo
        lda     preg,x
        tax
        inx
        lda     ph
        jsr     ayw
        ldx     vo
        lda     mix
        and     tonm,x          ; its tone on
        ora     nzb,x           ; its noise off
        jmp     setmix

; voice vo noise of period pl (its tone off)
noise:  lda     pl
        ldx     #R_NP
        jsr     ayw
        ldx     vo
        lda     mix
        and     nzm,x           ; its noise on
        ora     tob,x           ; its tone off
setmix: cmp     mix
        beq     :+
        sta     mix
        ldx     #R_MIX
        jmp     ayw
:       rts

; voice vo off: its volume 0, its tone and noise off
quiet:  lda     #0
        jsr     vol
        ldx     vo
        lda     mix
        ora     tob,x
        ora     nzb,x
        jmp     setmix

;            A    B    C   (by vo: 0, 1, 2)
vreg:   .byte   8,   9,   10
preg:   .byte   0,   2,   4
tob:    .byte   $01, $02, $04      ; R7: tone off
nzb:    .byte   $08, $10, $20      ; noise off
tonm:   .byte   $FE, $FD, $FB
nzm:    .byte   $F7, $EF, $DF

; ---- the effects ----------------------------------------------------------

; void __fastcall__ mk_cue(uint8_t id, uint8_t size): SND_* (a boom's size
; 0-3) or UI_* (sound.h)
_mk_cue:
        sta     vb              ; (size)
        jsr     popa
        sta     id
        lda     _mb_slot
        bne     :+
        jmp     spk_cue
:
        lda     id
        cmp     #$40
        bcc     @snd
        sbc     #$40            ; the UI's: a body alone
        cmp     #6
        bcs     @x
        asl     a
        tax
        lda     sx_ui,x
        ldy     sx_ui+1,x
        ldx     #0
        jmp     start2
@snd:   cmp     #16
        bcs     @x
        cmp     #1              ; SND_BOOM: by its size
        bne     @c
        lda     vb
        and     #3
        asl     a
        asl     a
        tax
        lda     sx_booms,x
        ldy     sx_booms+1,x
        jsr     body
        lda     sx_booms+2,x
        ldy     sx_booms+3,x
        jmp     low
@c:     asl     a
        asl     a
        tax
        lda     sx_cues,x
        ldy     sx_cues+1,x
        jsr     body
        lda     sx_cues+2,x
        ldy     sx_cues+3,x
        jmp     low
@x:     rts

; the body: steps at A, Y (0: none). X kept.
body:   stx     sl
        ldx     #0
        jsr     start2
        ldx     sl
        rts

; the low end (on C: none while a tune plays). X kept.
low:    stx     sl
        cpy     #0
        beq     @x
        ldx     mu_on
        bne     @x
        ldx     #1
        jsr     start2
@x:     ldx     sl
        rts

; effect slot X: steps at A, Y (high 0: nothing); the voice: the body A
; (C while a tune plays), the low end C
start2: cpy     #0
        beq     @x
        sta     fx_lo,x
        lda     #0
        sta     fx_left,x
        lda     #2              ; the low end: C
        cpx     #0
        bne     @v
        lda     mu_on           ; the body: A (0), C while a tune plays
        beq     @v
        lda     #2
@v:     sta     fx_vo,x
        tya
        sta     fx_hi,x         ; (last: mk_tick reads it)
@x:     rts

; a jiffy of effect slot X
fxstep: lda     fx_hi,x
        beq     @r
        lda     fx_left,x
        beq     @step
        dec     fx_left,x
        beq     @step
@r:     rts
@step:  stx     sl
        lda     fx_vo,x
        sta     vo
        lda     fx_lo,x
        sta     rd+1
        lda     fx_hi,x
        sta     rd+2
        ldy     #0
        jsr     rd              ; jiffies
        bne     @go
        ldx     sl              ; the end
        lda     #0
        sta     fx_hi,x
        jmp     quiet
@go:    ldx     sl
        sta     fx_left,x       ; (counted down from the next jiffy: this many)
        iny
        jsr     rd
        sta     vb              ; the volume and flags
        iny
        jsr     rd
        sta     pl
        iny
        jsr     rd
        sta     ph
        iny
        lda     vb
        and     #$20
        beq     @ne
        jsr     rd              ; the envelope's period, the decay again
        ldx     #R_EPL
        jsr     ayw
        iny
        jsr     rd
        ldx     #R_EPH
        jsr     ayw
        iny
        lda     #0
        ldx     #R_ESH
        jsr     ayw
@ne:    tya                     ; on to the next step
        ldx     sl
        clc
        adc     fx_lo,x
        sta     fx_lo,x
        bcc     :+
        inc     fx_hi,x
:       lda     vb
        and     #$10
        beq     @t
        jsr     noise
        jmp     @v
@t:     jsr     tone
@v:     lda     vb
        and     #$20
        beq     :+
        lda     #$10            ; the envelope's
        bne     :++
:       lda     vb
        and     #$0F
:       jsr     vol
        ldx     sl
@x:     rts

; A = the byte at (the step) + Y
rd:     lda     $FFFF,y
        rts

; uint8_t mk_busy(void): an effect still going
_mk_busy:
        lda     fx_hi
        ora     fx_hi+1
        ldx     #0
        rts

; ---- the whistle and the blips: voice B -----------------------------------

; void __fastcall__ mk_whistle(uint8_t on, uint8_t pitch): pitch the
; Atari's AUDF (play.c): the AY's period 2 (pitch + 1), the same note
_mk_whistle:
        sta     pl
        jsr     popa
        tay
        lda     _mb_slot
        beq     @x
        lda     mu_on           ; (a tune has B)
        bne     @x
        lda     bl_left         ; (a blip has it)
        bne     @x
        lda     #1
        sta     vo
        cpy     #0
        beq     @off
        jsr     b_tone
        lda     #6
        jmp     vol
@off:   jmp     quiet
@x:     rts

; void __fastcall__ mk_blip(uint8_t pitch): a talker's letter; 0 off
_mk_blip:
        sta     pl
        ldx     _mb_slot
        bne     :+
        jmp     spk_blip
:
        ldx     mu_on
        bne     @x
        ldx     #1
        stx     vo
        cmp     #0
        beq     @off
        lda     #3
        sta     bl_left
        jsr     b_tone
        lda     #10
        jmp     vol
@off:   lda     #0
        sta     bl_left
        jmp     quiet
@x:     rts

; voice B's tone: period 2 (pl + 1)
b_tone: lda     #0
        sta     ph
        inc     pl
        bne     :+
        inc     ph
:       asl     pl
        rol     ph
        jmp     tone

; ---- the tunes: A and B ---------------------------------------------------

; void __fastcall__ mk_tune(const uint16_t *ch1, const uint16_t *ch2):
; both channels from their first words, on the same jiffy
_mk_tune:
        sta     mu_lo+1
        stx     mu_hi+1
        jsr     popax
        sta     mu_lo
        stx     mu_hi
        lda     _mb_slot
        beq     @x
        lda     #0
        sta     mu_left
        sta     mu_left+1
        sta     fx_hi+1         ; (C's low end gone: C is the cues')
        lda     fx_vo           ; a body on A moves off: it ends
        bne     :+
        sta     fx_hi
:       lda     #3
        sta     mu_on
@x:     rts

; a jiffy of tune channel X (voice X: A, B)
mustep: lda     mu_on
        and     chb,x
        beq     @r
        lda     mu_left,x
        beq     @w
        dec     mu_left,x
        beq     @w
@r:     rts
@w:     stx     sl
        stx     vo
        lda     mu_lo,x
        sta     rw+1
        lda     mu_hi,x
        sta     rw+2
        ldy     #1
        jsr     rw              ; the word's high byte: jiffies << 3
        sta     ph
        lsr     a
        lsr     a
        lsr     a
        bne     @note
        dey                     ; a control: its code
        jsr     rw
        and     #3
        beq     @ret
        cmp     #3
        beq     @end
        cmp     #1
        bne     @jmp
        lda     mu_lo,x         ; a call: back to the word after its address
        clc
        adc     #4
        sta     mu_rlo,x
        lda     mu_hi,x
        adc     #0
        sta     mu_rhi,x
@jmp:   ldy     #2              ; to the address in the next word
        jsr     rw
        pha
        iny
        jsr     rw
        sta     mu_hi,x
        pla
        sta     mu_lo,x
        jmp     @w
@ret:   lda     mu_rlo,x
        sta     mu_lo,x
        lda     mu_rhi,x
        sta     mu_hi,x
        jmp     @w
@end:   lda     chb,x           ; the end: the channel quiet
        eor     #$FF
        and     mu_on
        sta     mu_on
        jmp     quiet
@note:  sta     mu_left,x
        dey
        jsr     rw              ; the low byte: the code, and the volume's low bit
        sta     pl
        lda     mu_lo,x         ; on to the next word
        clc
        adc     #2
        sta     mu_lo,x
        bcc     :+
        inc     mu_hi,x
:       lda     pl              ; the volume: bits 7-10
        asl     a
        lda     ph
        rol     a
        and     #15
        sta     vb
        lda     pl
        and     #$7F
        cmp     #NOISE0
        bcc     @tn
        sbc     #NOISE0         ; a drum: noise (on A)
        sta     pl
        jsr     noise
        jmp     @vol
@tn:    asl     a               ; a pitch: its period
        tay
        lda     mus_per,y
        sta     pl
        lda     mus_per+1,y
        sta     ph
        jsr     tone
@vol:   lda     vb
        jsr     vol
        ldx     sl
@x:     rts

rw:     lda     $FFFF,y
        rts

chb:    .byte   1, 2

; ---- a jiffy ----------------------------------------------------------------

; void mk_tick(void): a jiffy of everything (lowlevel.s's clock)
_mk_tick:
        lda     _mb_slot
        beq     @x
        ldx     #0
        jsr     fxstep
        ldx     #1
        jsr     fxstep
        ldx     #0
        jsr     mustep
        ldx     #1
        jsr     mustep
        lda     bl_left         ; a blip's end
        beq     @x
        dec     bl_left
        bne     @x
        lda     #1
        sta     vo
        jmp     quiet
@x:     rts

; ---- no Mockingboard: the speaker ------------------------------------------

SPKR    := $C030

; a cue (id, size vb) as a tone or a burst of noise
spk_no: rts
spk_cue:
        lda     id
        cmp     #16
        bcc     @i
        sbc     #$40-16         ; UI_* after the SND_*
        cmp     #16
        bcc     spk_no
        cmp     #21
        bcs     spk_no
@i:     sta     sl              ; x3: its entry
        asl     a
        adc     sl
        tax
        lda     spk_tab,x
        sta     sk
        lda     spk_tab+1,x
        sta     sd
        lda     spk_tab+2,x
        sta     sn
        lda     id
        cmp     #1              ; SND_BOOM: longer by its size
        bne     spk
        ldx     vb
        beq     spk
        lda     sn
:       clc
        adc     #12
        dex
        bne     :-
        sta     sn
        ; (on into spk)

; sn clicks of the speaker, sd x 5 cycles apart: a tone; sk: noise (each
; wait sd + 0-63 at random)
spk:    ldy     sn
        beq     @x
@l:     bit     SPKR
        ldx     sd
        lda     sk
        beq     @w
        jsr     rnd
        and     #$3F
        clc
        adc     sd
        tax
@w:     dex
        bne     @w
        dey
        bne     @l
@x:     rts

; a talker's letter: a click or two (0: nothing)
spk_blip:
        cmp     #0
        beq     @x
        lsr     a
        adc     #30
        sta     sd
        lda     #0
        sta     sk
        lda     #2
        sta     sn
        jmp     spk
@x:     rts

; an 8-bit LFSR
rnd:    lda     seed
        asl     a
        bcc     :+
        eor     #$1D
:       sta     seed
        rts

; each cue: noise (1) or a tone (0), the wait (x 5 cycles: a tone of
; 102000 / wait Hz), the clicks
spk_tab:
        .byte   1, 40, 30       ; SND_FIRE
        .byte   1, 120, 20      ; SND_BOOM (+12 a size)
        .byte   1, 200, 60      ; SND_NUKE
        .byte   1, 60, 40       ; SND_NAPALM
        .byte   0, 60, 50       ; SND_LASER
        .byte   0, 120, 30      ; SND_PLASMA
        .byte   1, 90, 24       ; SND_DIG
        .byte   0, 250, 12      ; SND_ROLL
        .byte   1, 100, 20      ; SND_DIRT
        .byte   0, 170, 20      ; SND_SHIELD
        .byte   0, 80, 30       ; SND_DEFLECT
        .byte   1, 50, 16       ; SND_CHUTE
        .byte   0, 230, 16      ; SND_DEATH
        .byte   0, 200, 8       ; SND_BOUNCE
        .byte   1, 180, 50      ; SND_THUNDER
        .byte   0, 100, 4       ; SND_TICK
        .byte   0, 100, 4       ; UI_TICK
        .byte   0, 80, 8        ; UI_SELECT
        .byte   0, 60, 16       ; UI_GO
        .byte   0, 250, 10      ; UI_ERROR
        .byte   0, 70, 14       ; UI_TURN

        .data
seed:   .byte   $5A
        .bss
sk:     .res 1
sd:     .res 1
sn:     .res 1
