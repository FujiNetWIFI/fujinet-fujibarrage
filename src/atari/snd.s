;
; snd.s -- POKEY, played by the immediate VBI (dsp.s). Four slots, one a
; voice (X = 0, 2, 4, 6). A sound is steps of three bytes: AUDF, AUDC,
; frames; frames 0 ends it, and for the tunes (gen: tools/mkmus_atari.py)
; $FE calls the steps at AUDF/AUDC (one level), $FD returns, $FF jumps.
; A slot with no sound but a direct tone (snd_f/snd_c set by C: the falling
; shell's whistle, talk) plays that instead.
;
; While netstream runs, POKEY's voices 3 and 4 clock the serial port
; (nstream.s, AUDCTL $28): only slots 0 and 1 play, and AUDCTL, STIMER,
; SKCTL and IRQEN are never written. While the link is held for a script
; (snd_full 1) all four play and AUDCTL is ours (snd_ctl).
;
; The steps are 60ths of a second. On PAL every fifth VBI each sounding
; slot owes one; it pays it by ending a step of two frames or more a frame
; early (never a one-frame step: a drum's attack would go).
;
        .export _snd_vbi, _snd_ptr, _snd_left, _snd_all, _snd_ctl, _snd_f, _snd_c
        .export _snd_mute, _snd_pal

        .include "hw.inc"

        .segment "ZEROPAGE"
_snd_ptr:  .res 8       ; slot n at +2n (0 high: none)

        .segment "BSS2"
_snd_left: .res 8       ; frames left of the step
_snd_all: .res 1       ; 1: all four voices and AUDCTL
_snd_ctl:  .res 1       ; AUDCTL while full
_snd_mute: .res 1
_snd_f:    .res 8       ; a direct tone per slot (snd_c 0: none)
_snd_c:    .res 8
cur_f:     .res 8
cur_c:     .res 8
ret:       .res 8       ; where $FD goes back to
_snd_pal:  .res 1       ; 1 on PAL
pal_n:     .res 1
debt:      .res 8       ; 60ths a slot is behind (PAL)

        .code

_snd_vbi:
        lda     _snd_pal
        beq     @go
        dec     pal_n
        bpl     @go
        lda     #4
        sta     pal_n
        ldx     #6
@d:     lda     _snd_ptr+1,x
        beq     @dn
        inc     debt,x
@dn:    dex
        dex
        bpl     @d
@go:    jsr     steps
        lda     _snd_all
        beq     @out
        lda     _snd_ctl
        sta     AUDCTL
@out:   ldx     #0
        jsr     put
        ldx     #2
        jsr     put
        lda     _snd_all
        beq     @x
        ldx     #4
        jsr     put
        ldx     #6
        jsr     put
@x:     rts

; every slot a step on
steps:  ldx     #0
        jsr     step
        ldx     #2
        jsr     step
        lda     _snd_all
        beq     @x
        ldx     #4
        jsr     step
        ldx     #6
        jmp     step
@x:     rts

; slot X's voice gets what it plays this frame
put:    lda     _snd_mute
        bne     @q
        lda     cur_c,x
        bne     @s
        lda     _snd_c,x        ; no sound: a direct tone?
        beq     @q
        sta     AUDC1,x
        lda     _snd_f,x
        sta     AUDF1,x
        rts
@s:     sta     AUDC1,x
        lda     cur_f,x
        sta     AUDF1,x
        rts
@q:     lda     #0
        sta     AUDC1,x
        rts

; slot X: the next frame of its sound
step:   lda     _snd_ptr+1,x
        beq     @off
        lda     debt,x          ; a PAL frame owed: a held step pays it
        beq     @l
        lda     _snd_left,x
        beq     @l
        dec     _snd_left,x
        dec     debt,x
@l:     lda     _snd_left,x     ; frames left of this step (a step of n
        beq     @load           ; frames: the one it loads, then n - 1)
        dec     _snd_left,x
        rts
@load:  lda     (_snd_ptr,x)    ; AUDF
        sta     cur_f,x
        inc     _snd_ptr,x
        bne     @a
        inc     _snd_ptr+1,x
@a:     lda     (_snd_ptr,x)    ; AUDC
        sta     cur_c,x
        inc     _snd_ptr,x
        bne     @b
        inc     _snd_ptr+1,x
@b:     lda     (_snd_ptr,x)    ; frames
        beq     @end
        cmp     #$FD
        bcs     @code
        sta     _snd_left,x
        dec     _snd_left,x
        inc     _snd_ptr,x
        bne     @x
        inc     _snd_ptr+1,x
@x:     rts
@end:   lda     #0
        sta     _snd_ptr+1,x
@off:   lda     #0
        sta     cur_c,x
        rts
@code:  beq     @ret            ; $FD
        cmp     #$FF
        beq     @jump
        lda     _snd_ptr,x      ; $FE: back to the step after this one
        clc
        adc     #1
        sta     ret,x
        lda     _snd_ptr+1,x
        adc     #0
        sta     ret+1,x
@jump:  lda     cur_f,x
        sta     _snd_ptr,x
        lda     cur_c,x
        sta     _snd_ptr+1,x
        jmp     @load
@ret:   lda     ret,x
        sta     _snd_ptr,x
        lda     ret+1,x
        sta     _snd_ptr+1,x
        jmp     @load
