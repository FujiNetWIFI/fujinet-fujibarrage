;
; bln.s -- a talking tank's balloon: its words in the 3x5 font (font35,
; gen/font35.h: a glyph's rows in 15 bits) on a black box over the tank.
; What it covers is kept and put back when it goes.
;
; void balloon(void)   bl_s's words bl_t, bl_n of them (0: just the old one
;                      taken away)
;
        .export _balloon, _keeping, _bl_s, _bl_t, _bl_n
        .import _tk, _rowlo, _rowhi, _fld_hspan, _fld_plot, _fx_cycle, _fx_col, _font35
        .importzp _fx, _fy, _fx1, _fv

B_ROWS  = 7
FIELD_W = 160
TANK_H  = 5
UP      = TANK_H + 6 + B_ROWS + 2   ; the box's top, over the tank's bottom
PX_TANK = 0
PX_FX   = 3

        .segment "ZEROPAGE"
kp:     .res 2                  ; in keep
rp:     .res 2                  ; in the field
tp:     .res 2                  ; the words
gl:     .res 2                  ; a glyph

        .segment "LOWBSS"
keep:   .res B_ROWS * 42        ; what the box covers

        .segment "BSS"
_bl_s:  .res 1
_bl_t:  .res 2
_bl_n:  .res 1
_keeping: .res 1
keep_y: .res 1
keep_b0: .res 1                 ; the first byte of a line it covers
keep_nb: .res 1                 ; and how many
x0:     .res 1
w:      .res 1
dir:    .res 1
i:      .res 1
r:      .res 1
col:    .res 1

        .code

; rows: dir 0 the field's bytes to keep, else back
rows:   sta     dir
        lda     #<keep
        sta     kp
        lda     #>keep
        sta     kp+1
        ldx     #0
@r:     txa
        clc
        adc     keep_y
        tay
        lda     _rowlo,y
        clc
        adc     keep_b0
        sta     rp
        lda     _rowhi,y
        adc     #0
        sta     rp+1
        ldy     #0
@b:     lda     dir
        bne     @back
        lda     (rp),y
        sta     (kp),y
        jmp     @n
@back:  lda     (kp),y
        sta     (rp),y
@n:     iny
        cpy     keep_nb
        bne     @b
        lda     kp
        clc
        adc     keep_nb
        sta     kp
        bcc     @k
        inc     kp+1
@k:     inx
        cpx     #B_ROWS
        bne     @r
        rts

unballoon:
        lda     _keeping
        beq     @x
        lda     #1
        jsr     rows
        lda     #0
        sta     _keeping
        lda     #$0E
        sta     _fx_col
@x:     rts

_balloon:
        jsr     unballoon
        lda     _bl_n
        bne     @go
        rts
@go:    cmp     #40
        bcc     @n
        lda     #39
        sta     _bl_n
@n:     asl                     ; w = 4 n + 1
        asl
        ora     #1
        sta     w
        lda     _bl_s           ; the tank: tk + 5 s
        asl
        asl
        adc     _bl_s
        tax
        lda     w               ; x0 = its x - w / 2, kept on the field
        lsr
        sta     x0
        sec
        lda     _tk+1,x
        sbc     x0
        tay
        lda     _tk+2,x
        sbc     #0
        bmi     @left
        bne     @right
        tya
        sta     x0
        clc
        adc     w
        bcs     @right
        cmp     #FIELD_W+1
        bcc     @okx
@right: lda     #FIELD_W
        sec
        sbc     w
        sta     x0
        jmp     @okx
@left:  lda     #0
        sta     x0
@okx:   lda     _tk+3,x         ; y0 = its y - UP, kept on the field
        sec
        sbc     #UP
        bcs     @y
        lda     #0
@y:     sta     keep_y
        lda     x0              ; the bytes it covers
        lsr
        lsr
        sta     keep_b0
        lda     x0
        clc
        adc     w
        sec
        sbc     #1
        lsr
        lsr
        sec
        sbc     keep_b0
        clc
        adc     #1
        sta     keep_nb
        lda     #0
        jsr     rows            ; kept
        lda     #1
        sta     _keeping
        lda     #PX_TANK        ; the box
        sta     _fv
        lda     #0
        sta     r
        sta     _fx+1
        sta     _fy+1
        sta     _fx1+1
@box:   lda     keep_y
        clc
        adc     r
        sta     _fy
        lda     x0
        sta     _fx
        clc
        adc     w
        sec
        sbc     #1
        sta     _fx1
        jsr     _fld_hspan
        inc     r
        lda     r
        cmp     #B_ROWS
        bne     @box
        lda     _bl_t           ; the words
        sta     tp
        lda     _bl_t+1
        sta     tp+1
        lda     #PX_FX
        sta     _fv
        lda     #0
        sta     i
@ch:    ldy     i
        lda     (tp),y
        cmp     #'a'
        bcc     @up
        cmp     #'z'+1
        bcs     @up
        sbc     #31             ; (carry clear: - 32)
@up:    sec
        sbc     #32
        cmp     #64
        bcc     @gl
        lda     #0              ; ' '
@gl:    asl
        tay
        lda     _font35,y
        sta     gl
        lda     _font35+1,y
        sta     gl+1
        lda     #0
        sta     r
@row:   lda     #4              ; the row's three bits, left to right
        sta     col
@bit:   lda     gl
        and     col
        beq     @nb
        lda     i               ; x0 + 1 + 4 i + (0, 1, 2)
        asl
        asl
        sec
        adc     x0
        ldy     col
        cpy     #4
        beq     @c0
        clc
        adc     #1
        cpy     #2
        beq     @c0
        adc     #1
@c0:    sta     _fx
        lda     #0
        sta     _fx+1
        lda     keep_y
        sec
        adc     r
        sta     _fy
        lda     #0
        sta     _fy+1
        jsr     _fld_plot
@nb:    lsr     col
        bne     @bit
        lsr     gl+1            ; the next row
        ror     gl
        lsr     gl+1
        ror     gl
        lsr     gl+1
        ror     gl
        inc     r
        lda     r
        cmp     #5
        bne     @row
        inc     i
        lda     i
        cmp     _bl_n
        beq     @done
        jmp     @ch
@done:  lda     #0
        sta     _fx_cycle
        lda     #$0E
        sta     _fx_col
        rts
