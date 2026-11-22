;
; trk.s -- the script's tracks (play.c; docs/PROTOCOL.md OP_ARC): sixteen
; slots, each a moving head: position and velocity 12.12 fixed point (24
; bits), acceleration 4.12 (16). Each step
;
;       X += VX;  Y += VY;  VX += AX;  VY += AY
;
; and the head is drawn at (X >> 12, Y >> 12) -- the server's fitter runs
; the very same integers. A head is drawn (colour 3) only over sky (a smoke
; trail's over fire too), and the pixel under it is put back when it moves;
; a smoke trail's stays.
;
        .export _trk_load, _trk_step, _trk_stop, _trk_reset, _trk_fall
        .importzp _fx, _fy, _fv, ptr1
        .import _fld_plot, _fld_get

N       = 16
ST_SMOKE = 2
ST_DIGGER = 4
PX_SKY  = 1
PX_FX   = 3

        .segment "HIGH"
on:     .res N
style:  .res N
xl:     .res N
xm:     .res N
xh:     .res N
yl:     .res N
ym:     .res N
yh:     .res N
vxl:    .res N
vxm:    .res N
vxh:    .res N
vyl:    .res N
vym:    .res N
vyh:    .res N
axl:    .res N
axh:    .res N
ayl:    .res N
ayh:    .res N
nl:     .res N
nh:     .res N
pxl:    .res N
pxh:    .res N
pyl:    .res N
pyh:    .res N
under:  .res N
drawn:  .res N
slot:   .res 1
fall:   .res 1

        .code

; void trk_reset(void)
_trk_reset:
        lda     #0
        ldx     #N-1
@l:     sta     on,x
        sta     drawn,x
        dex
        bpl     @l
        rts

; void __fastcall__ trk_stop(uint8_t s): the track ends now
_trk_stop:
        and     #N-1
        tax
        jsr     off
        lda     #0
        sta     on,x
        rts

; void __fastcall__ trk_load(const uint8_t *a): an OP_ARC's arguments
; (slot, style, x16, y16, vx24, vy24, ax16, ay16, n16) start a track
_trk_load:
        sta     ptr1
        stx     ptr1+1
        ldy     #0
        lda     (ptr1),y
        and     #N-1
        tax
        jsr     off             ; its old head away
        ldy     #1
        lda     (ptr1),y
        sta     style,x
        lda     #0              ; X = x16 << 8
        sta     xl,x
        sta     yl,x
        iny
        lda     (ptr1),y
        sta     xm,x
        iny
        lda     (ptr1),y
        sta     xh,x
        iny
        lda     (ptr1),y
        sta     ym,x
        iny
        lda     (ptr1),y
        sta     yh,x
        iny
        lda     (ptr1),y
        sta     vxl,x
        iny
        lda     (ptr1),y
        sta     vxm,x
        iny
        lda     (ptr1),y
        sta     vxh,x
        iny
        lda     (ptr1),y
        sta     vyl,x
        iny
        lda     (ptr1),y
        sta     vym,x
        iny
        lda     (ptr1),y
        sta     vyh,x
        iny
        lda     (ptr1),y
        sta     axl,x
        iny
        lda     (ptr1),y
        sta     axh,x
        iny
        lda     (ptr1),y
        sta     ayl,x
        iny
        lda     (ptr1),y
        sta     ayh,x
        iny
        lda     (ptr1),y
        sta     nl,x
        iny
        lda     (ptr1),y
        sta     nh,x
        lda     #1
        sta     on,x
        jmp     on_

; void trk_step(void): every live track one step
_trk_step:
        lda     #0
        sta     fall
        ldx     #N-1
@t:     lda     on,x
        bne     @live
        jmp     @n
@live:  stx     slot
        jsr     off
        ldx     slot
        lda     nl,x            ; no steps left: it ends
        ora     nh,x
        bne     @go
        sta     on,x
        jmp     @n
@go:    lda     nl,x
        bne     @d
        dec     nh,x
@d:     dec     nl,x
        clc                     ; X += VX
        lda     xl,x
        adc     vxl,x
        sta     xl,x
        lda     xm,x
        adc     vxm,x
        sta     xm,x
        lda     xh,x
        adc     vxh,x
        sta     xh,x
        clc                     ; Y += VY
        lda     yl,x
        adc     vyl,x
        sta     yl,x
        lda     ym,x
        adc     vym,x
        sta     ym,x
        lda     yh,x
        adc     vyh,x
        sta     yh,x
        ldy     #0              ; VX += AX (sign-extended)
        lda     axh,x
        bpl     @ax
        dey
@ax:    clc
        lda     vxl,x
        adc     axl,x
        sta     vxl,x
        lda     vxm,x
        adc     axh,x
        sta     vxm,x
        tya
        adc     vxh,x
        sta     vxh,x
        ldy     #0              ; VY += AY
        lda     ayh,x
        bpl     @ay
        dey
@ay:    clc
        lda     vyl,x
        adc     ayl,x
        sta     vyl,x
        lda     vym,x
        adc     ayh,x
        sta     vym,x
        tya
        adc     vyh,x
        sta     vyh,x
        jsr     on_
        ldx     slot
        lda     vyh,x           ; falling (VY > 0), on the field, not a digger:
        bmi     @n              ; the lowest is the whistle's
        lda     pyh,x
        bne     @n
        lda     style,x
        and     #15
        cmp     #ST_DIGGER
        beq     @n
        lda     pyl,x
        cmp     fall
        bcc     @n
        ora     #1
        sta     fall
@n:     dex
        bmi     @done
        jmp     @t
@done:  rts

; uint8_t trk_fall(void): the lowest falling shell's line | 1, or 0
_trk_fall:
        lda     fall
        ldx     #0
        rts

; the head of track X off (its pixel back, unless a trail or overdrawn)
off:    lda     drawn,x
        beq     @x
        lda     #0
        sta     drawn,x
        lda     style,x
        and     #15
        cmp     #ST_SMOKE
        beq     @x
        stx     slot
        jsr     at
        jsr     _fld_get
        cmp     #PX_FX
        bne     @r
        ldx     slot
        lda     under,x
        sta     _fv
        jsr     _fld_plot
@r:     ldx     slot
@x:     rts

; the head of track X on, at (X >> 12, Y >> 12)
on_:    stx     slot
        lda     xm,x            ; px = (xh:xm) >> 4, arithmetic
        sta     pxl,x
        lda     xh,x
        jsr     shr4
        sta     pxh,x
        lda     ym,x
        sta     pyl,x
        lda     yh,x
        jsr     shr4p
        sta     pyh,x
        jsr     at
        jsr     _fld_get
        ldx     slot
        cmp     #PX_SKY
        beq     @draw
        cmp     #PX_FX          ; a trail's over fire too (refview's drawHead:
        bne     @x              ;  never over dirt or a tank)
        tay
        lda     style,x
        and     #15
        cmp     #ST_SMOKE
        bne     @x
        tya
@draw:  sta     under,x
        lda     #1
        sta     drawn,x
        lda     #PX_FX
        sta     _fv
        jsr     _fld_plot
@x:     ldx     slot
        rts

; A:pxl,x := (A:pxl,x) >> 4, arithmetic, for x and y
shr4:   ldy     #4
@s:     cmp     #$80
        ror
        ror     pxl,x
        dey
        bne     @s
        rts
shr4p:  ldy     #4
@s:     cmp     #$80
        ror
        ror     pyl,x
        dey
        bne     @s
        rts

; fx, fy from track X's head
at:     lda     pxl,x
        sta     _fx
        lda     pxh,x
        sta     _fx+1
        lda     pyl,x
        sta     _fy
        lda     pyh,x
        sta     _fy+1
        rts
