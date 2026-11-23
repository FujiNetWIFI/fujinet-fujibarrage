;
; pt.s -- sparks: pixels flying from a tank going up, or off its shield.
; Each is a colour-3 pixel on the sky, moved every frame (8.8 fixed point)
; and put back to sky when it moves on or dies. It only ever draws on sky,
; so it changes nothing the server knows (refview never sees them).
;
; void __fastcall__ field_sparks(uint16_t a)  low byte the seat, high the
;                       kind: 0-7 a death's effect (DEATH_*), 8 + a
;                       shield's event (SH_*)
; uint8_t pt_step(void) a frame (ply.s's anim); nonzero while any burn
;                       (they glow in the fire colours)
; void pt_clear(void)   all gone (a segment's end, a script's start)
;
        .export _field_sparks, _pt_step, _pt_clear
        .import _tk, _rowlo, _rowhi

NP      = 16
        .include "hw.inc"
RANDOM  = POKEY + $0A

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

        .segment "ZEROPAGE"
fp:     .res 2          ; the pixel's line

        .segment "HIGH"
xl:     .res NP
xh:     .res NP
yl:     .res NP
yh:     .res NP
vxl:    .res NP
vxh:    .res NP
vyl:    .res NP
vyh:    .res NP
life:   .res NP
grav:   .res NP
ox:     .res NP         ; where it shows ($FF: nowhere)
oy:     .res NP
cx:     .res 1
cy:     .res 1
left:   .res 1
kp:     .res 1          ; the kind's parameters in par
d:      .res 1
i:      .res 1
pidx:   .res 1
pm:     .res 1          ; the pixel's two bits in its byte
tl:     .res 1
th:     .res 1

        .rodata
mask:   .byte   $C0, $30, $0C, $03
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

        .code

_field_sparks:
        sta     tl              ; the tank: tk + 5 seat
        asl
        asl
        adc     tl
        tay
        lda     _tk+1,y         ; x (the low byte: it's on the field)
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
@f:     lda     life,x
        beq     @got
        dex
        bpl     @f
        rts
@got:   ldy     kp
        lda     RANDOM          ; its direction
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
@sp:    ldy     kp              ; then slower
        lda     RANDOM
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
        lda     RANDOM
        and     #7
        clc
        adc     par+P_LIFE,y
        sta     life,x
        lda     #$FF
        sta     ox,x
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
@p:     lda     life,x
        beq     @n
        sta     d
        stx     pidx
        jsr     erase
        dec     life,x
        beq     @n
        clc                     ; falling
        lda     vyl,x
        adc     grav,x
        sta     vyl,x
        bcc     @m
        inc     vyh,x
@m:     jsr     move
        lda     yh,x
        cmp     #168
        bcs     @die
        tay
        lda     xh,x
        cmp     #160
        bcs     @die
        jsr     at              ; on sky (only the low bit on): both on
        lda     pm
        and     #$55
        sta     tl
        lda     (fp),y
        and     pm
        cmp     tl
        bne     @nd
        lda     (fp),y
        ora     pm
        sta     (fp),y
        ldx     pidx
        lda     xh,x
        sta     ox,x
        lda     yh,x
        sta     oy,x
@nd:    ldx     pidx
        jmp     @n
@die:   lda     #0
        sta     life,x
@n:     dex
        bpl     @p
        lda     d
        rts

        .segment "CASCODE"      ; (barrage.cfg: the cassette buffer)

; move: spark X a frame on
move:   clc
        lda     xl,x
        adc     vxl,x
        sta     xl,x
        lda     xh,x
        adc     vxh,x
        sta     xh,x
        clc
        lda     yl,x
        adc     vyl,x
        sta     yl,x
        lda     yh,x
        adc     vyh,x
        sta     yh,x
        rts

; at: pixel (A, Y) = x, y: fp the line, Y its byte, pm its bits
at:     sty     tl
        tay
        and     #3
        tax
        lda     mask,x
        sta     pm
        tya
        ldy     tl
        tax
        lda     _rowlo,y
        sta     fp
        lda     _rowhi,y
        sta     fp+1
        txa
        lsr
        lsr
        tay
        rts

; erase: spark pidx's pixel back to sky (its high bit off), if it's still
; there (both bits on); X = pidx after
erase:  lda     ox,x
        cmp     #$FF
        beq     @x
        ldy     oy,x
        jsr     at
        lda     (fp),y
        and     pm
        cmp     pm
        bne     @x
        lda     pm
        and     #$AA
        eor     #$FF
        and     (fp),y
        sta     (fp),y
@x:     ldx     pidx
        lda     #$FF
        sta     ox,x
        rts

        .code

_pt_clear:
        ldx     #NP-1
@c:     lda     life,x
        beq     @n
        stx     pidx
        jsr     erase
        lda     #0
        sta     life,x
@n:     dex
        bpl     @c
        rts
