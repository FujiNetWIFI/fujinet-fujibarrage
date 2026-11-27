;
; pt.s -- the NES's sparks: dots flying from a tank going up, or off its
; shield (the Atari's src/atari/pt.s, its motion the same, 8.8 fixed point).
; Here they are sprites (field.c's sprites() puts them in after the tanks,
; which they give way to on a crowded line): a spark shows only where the
; field is sky, in the fire's colours as it cools. Nothing the server knows
; changes (refview never sees them).
;
; void __fastcall__ field_sparks(uint16_t a)  low byte the seat, high the
;                       kind: 0-7 a death's effect (DEATH_*), 8 + a
;                       shield's event (SH_*)
; uint8_t pt_step(void) a frame (ply.s's anim); nonzero while any burn
; void pt_clear(void)   all gone (a segment's end, a script's start)
;
; pt_x, pt_y, pt_life: where each shows (pt_y $FF: nowhere) and its frames
; left, for the sprites.
;
        .export _field_sparks, _pt_step, _pt_clear, _dsp_shake
        .export _pt_x, _pt_y, _pt_life
        .import _tk, _fld_get
        .importzp _fx, _fy

NP      = 16

; a kind's parameters (par, 9 bytes each)
P_N     = 0             ; how many
P_FIRST = 1             ; the first direction (0 right, 4 down, 12 up)
P_STEP  = 2             ; 1: one direction after another
P_MASK  = 3             ; a random spread added
P_K     = 4             ; frames moved before it shows (at 2 px a frame)
P_SH    = 5             ; then its speed halved this often
P_SJ    = 6             ; and once more at random (1) or not (0)
P_G     = 7             ; gravity, 1/256 px a frame per frame
P_LIFE  = 8             ; frames (plus 0-7 at random)

        .segment "STKLO"        ; ($0120-$017F: the stack page's bottom)
xl:     .res NP
xh:     .res NP
yl:     .res NP
yh:     .res NP
vxl:    .res NP
vxh:    .res NP

        .segment "BSS"
vyl:    .res NP
vyh:    .res NP
_pt_life: .res NP
grav:   .res NP
_pt_x:  .res NP         ; where it shows
_pt_y:  .res NP         ; ($FF: nowhere)
_dsp_shake: .res 1      ; the player's shake (ply.s): frames left
cx:     .res 1
cy:     .res 1
left:   .res 1
kp:     .res 1          ; the kind's parameters in par
d:      .res 1
i:      .res 1
pidx:   .res 1
tl:     .res 1
th:     .res 1
rnd:    .res 2          ; (a 16-bit LFSR: the Atari had POKEY's)

        .segment "CODE2"        ; (R7: the fixed banks are full)
cos:    .byte   32, 30, 23, 12, 0, <-12, <-23, <-30
        .byte   <-32, <-30, <-23, <-12, 0, 12, 23, 30
RAYS    = 0*9
SPRAY   = 1*9
PLUME   = 2*9
SCATTER = 3*9
DEBRIS  = 4*9
RING    = 5*9
BURST   = 6*9
kmap:   .byte   PLUME, DEBRIS, DEBRIS, DEBRIS, RAYS, SPRAY, PLUME, SCATTER
        .byte   RING, BURST, RING, RING, RING   ; hit, break, deflect, up, lift
;               n   1st step mask k  sh sj g   life
par:    .byte   16, 0,  1,   0,   2, 1, 0, 0,  14   ; rays
        .byte   12, 9,  0,   7,   1, 0, 1, 20, 30   ; spray, up and over
        .byte   10, 11, 0,   2,   0, 3, 1, 0,  34   ; plume, rising slowly
        .byte   14, 0,  0,   15,  1, 0, 1, 16, 30   ; scatter, all ways
        .byte   8,  9,  0,   7,   1, 1, 1, 20, 24   ; debris
        .byte   16, 0,  1,   0,   3, 4, 0, 0,  8    ; ring: a dome round it
        .byte   16, 0,  1,   0,   3, 0, 0, 0,  8    ; burst

        .segment "CODE2"

; random: A a random byte (the LFSR stepped 8 times; X kept)
random: ldy     #8
        lda     rnd
        ora     rnd+1
        bne     @l
        lda     #$A5            ; (never all zero)
        sta     rnd
@l:     asl     rnd
        rol     rnd+1
        bcc     @n
        lda     rnd
        eor     #$2D
        sta     rnd
@n:     dey
        bne     @l
        lda     rnd
        rts

_field_sparks:
        sta     tl              ; the tank: tk + 5 seat
        asl
        asl
        adc     tl
        tay
        lda     _tk+2,y         ; x: on the field (its high byte 0)?
        bne     @x0
        lda     _tk+1,y
        sta     cx
        lda     _tk+3,y         ; y, its bottom: the middle is 2 up
        sec
        sbc     #2
        sta     cy
        lda     kmap,x
        sta     kp
        tax
        lda     par+P_N,x
        sta     left
        lda     #0
        sta     i
@one:   ldx     #NP-1           ; a free one
@f:     lda     _pt_life,x
        beq     @got
        dex
        bpl     @f
@x0:    rts
@got:   ldy     kp
        jsr     random          ; its direction
        ldy     kp
        and     par+P_MASK,y
        clc
        adc     par+P_FIRST,y
        sta     d
        lda     par+P_STEP,y
        beq     @dir
        lda     d
        clc
        adc     i
        sta     d
@dir:   lda     d
        and     #15
        tay
        lda     cos,y
        jsr     v16
        sta     vxl,x
        lda     th
        sta     vxh,x
        tya
        clc
        adc     #12
        and     #15
        tay
        lda     cos,y
        jsr     v16
        sta     vyl,x
        lda     th
        sta     vyh,x
        lda     #$80
        sta     xl,x
        sta     yl,x
        lda     cx
        sta     xh,x
        lda     cy
        sta     yh,x
        ldy     kp
        lda     par+P_K,y       ; out from the middle
        beq     @sp
        sta     d
@k:     jsr     move
        dec     d
        bne     @k
@sp:    jsr     random          ; then slower
        ldy     kp
        and     par+P_SJ,y
        clc
        adc     par+P_SH,y
        beq     @g
        tay
@sh:    lda     vxh,x
        cmp     #$80
        ror     vxh,x
        ror     vxl,x
        lda     vyh,x
        cmp     #$80
        ror     vyh,x
        ror     vyl,x
        dey
        bne     @sh
@g:     ldy     kp
        lda     par+P_G,y
        sta     grav,x
        jsr     random
        and     #7
        ldy     kp
        clc
        adc     par+P_LIFE,y
        sta     _pt_life,x
        lda     #$FF
        sta     _pt_y,x
        inc     i
        dec     left
        beq     @x
        jmp     @one
@x:     rts

; v16: A (1/16 px, signed) as 1/256 px: low byte in A, high in th
v16:    sta     tl
        lda     #0
        bit     tl
        bpl     @p
        lda     #$FF
@p:     sta     th
        lda     tl
        asl
        rol     th
        asl
        rol     th
        asl
        rol     th
        asl
        rol     th
        rts

_pt_step:
        lda     #0
        sta     d               ; (any still going)
        ldx     #NP-1
@p:     lda     _pt_life,x
        beq     @n
        sta     d
        stx     pidx
        lda     #$FF            ; (nowhere, till it shows again)
        sta     _pt_y,x
        dec     _pt_life,x
        beq     @n
        clc                     ; falling
        lda     vyl,x
        adc     grav,x
        sta     vyl,x
        bcc     @m
        inc     vyh,x
@m:     jsr     move
        bcs     @die            ; (off a side)
        lda     yh,x
        cmp     #168
        bcs     @die
        sta     _fy             ; on sky: it shows
        lda     xh,x
        sta     _fx
        lda     #0
        sta     _fx+1
        sta     _fy+1
        jsr     _fld_get
        ldx     pidx
        cmp     #1
        bne     @n
        lda     xh,x
        sta     _pt_x,x
        lda     yh,x
        sta     _pt_y,x
        jmp     @n
@die:   lda     #0
        sta     _pt_life,x
@n:     dex
        bpl     @p
        lda     d
        ldx     #0
        rts

; move: spark X a frame on; carry set if it went off a side (x past 0-255)
move:   clc
        lda     xl,x
        adc     vxl,x
        sta     xl,x
        lda     xh,x
        adc     vxh,x
        sta     xh,x
        ; (off: a rightward one carried, a leftward one didn't)
        lda     vxh,x
        bmi     @l
        bcs     @off
        bcc     @y
@l:     bcc     @off
@y:     clc
        lda     yl,x
        adc     vyl,x
        sta     yl,x
        lda     yh,x
        adc     vyh,x
        sta     yh,x
        clc
        rts
@off:   sec
        rts

_pt_clear:
        ldx     #NP-1
        lda     #0
@c:     sta     _pt_life,x
        dex
        bpl     @c
        ldx     #NP-1
        lda     #$FF
@y:     sta     _pt_y,x
        dex
        bpl     @y
        rts
