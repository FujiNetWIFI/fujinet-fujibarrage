;
; fld.s -- the field's pixels on the Apple II: HGR page 1 is the field,
; 140 colour pixels by 160 lines (mixed mode's text rows below it). A colour
; pixel is two dots, the game's value in them (tools/mktab_a2.py):
;
;   00  a tank  (black)
;   01  sky     (the even dot: blue in palette 1, violet in palette 0)
;   10  dirt    (the odd dot: orange, or green)
;   11  fire    (both: white)
;
; A byte holds 7 dots (the left one in bit 0) and its palette in bit 7,
; which every write here leaves alone: the whole field is drawn in the
; round's palette (_fpal; fld_pal sets it). The same API as the Atari's
; src/atari/fld.s, so src/6502's player draws through it unchanged; the
; arguments are in zero page.
;
        .export _fld_init, _fld_clear, _fld_plot, _fld_get, _fld_hspan
        .export _fld_vrun, _fld_col, _fld_swap, _fld_ellipse, _fld_pal, _fld_dots
        .exportzp _fx, _fy, _fx1, _fv, _fent, _fold, _ex, _ey, _erx, _ery, _emode, _fpal
        ; (the profilers name code by the export below it: these mark the rest)
        .export _p_at := at, _p_put := put, _p_pget := pget, _p_rmw := rmw
        .export _p_colset := colset, _p_vrows := vrows, _p_run := run, _p_erow := erow
        .import _rowlo, _rowhi, _xb0, _xm0, _xb1, _xm1, _frame_tick

FW      = 140
FLINES  = 160

        .segment "ZEROPAGE"
_fx:    .res 2          ; x (int16)
_fy:    .res 2          ; y (int16)
_fx1:   .res 2          ; a span's last x / a run's end y (exclusive)
_fv:    .res 1          ; the value, 0-3
_fold:  .res 1          ; fld_swap: the value to replace
_fent:  .res 2          ; fld_col: the column entry
_fpal:  .res 1          ; the field's palette bit: $80 blue/orange, 0 violet/green
fp:     .res 2
ft:     .res 1
cur:    .res 1
cnt:    .res 1
ys:     .res 1
sp2:    .res 1          ; fld_col: spans left
cb0:    .res 1          ; a column's dots: the even one's byte, mask, value bits,
cb1:    .res 1          ;  the odd one's (the same byte but at pixel 3 of 7)
nm0:    .res 1
nm1:    .res 1
vb0:    .res 1
vb1:    .res 1
pv0:    .res 1          ; a span's whole groups: the value in both bytes
pv1:    .res 1
_ex:    .res 2          ; fld_ellipse: the centre,
_ey:    .res 2
_erx:   .res 1          ; the radii (ery at most 200),
_ery:   .res 1
_emode: .res 1          ; 0 all fv, 1 sky becomes fv, 2 colour 3 becomes sky
edy:    .res 1
ei:     .res 1
erem:   .res 1
ehw:    .res 2
ft2:    .res 1

        .segment "LC"           ; (the language card: src/apple2/barrage.cfg)
; a quarter circle: q32[i] = 64 sqrt(1 - (i/32)^2)
q32:    .byte   64, 64, 64, 63, 63, 62, 62, 61, 60, 59, 57, 56, 54, 53, 51, 49
        .byte   46, 44, 41, 38, 35, 32, 28, 24, 20, 15, 10, 5, 0, 0, 0, 0, 0
; a value in all 7 pixels of a group: its first byte, its second
pat0:   .byte   $00, $55, $2A, $7F
pat1:   .byte   $00, $2A, $55, $7F


; void fld_init(void): the palette (the tables are tools/mktab_a2.py's)
_fld_init:
        lda     #$80
        sta     _fpal
        rts

; void fld_clear(void): all sky, in the palette
_fld_clear:
        lda     #$55
        ora     _fpal
        sta     pv0
        lda     #$2A
        ora     _fpal
        sta     pv1
        ldx     #0
@l:     lda     _rowlo,x
        sta     fp
        lda     _rowhi,x
        sta     fp+1
        ldy     #0
@b:     lda     pv0
        sta     (fp),y
        iny
        lda     pv1
        sta     (fp),y
        iny
        cpy     #40
        bne     @b
        inx
        cpx     #FLINES
        bne     @l
        rts

; void fld_pal(void): every byte of the field in palette _fpal
_fld_pal:
        ldx     #0
@l:     lda     _rowlo,x
        sta     fp
        lda     _rowhi,x
        sta     fp+1
        ldy     #39
@b:     lda     (fp),y
        and     #$7F
        ora     _fpal
        sta     (fp),y
        dey
        bpl     @b
        inx
        cpx     #FLINES
        bne     @l
        rts

; in range? carry clear: fp is the line's address, X the pixel
at:     lda     _fx+1
        bne     @out
        ldx     _fx
        cpx     #FW
        bcs     @out
        lda     _fy+1
        bne     @out
        ldy     _fy
        cpy     #FLINES
        bcs     @out
        lda     _rowlo,y
        sta     fp
        lda     _rowhi,y
        sta     fp+1
        clc
        rts
@out:   sec
        rts

; pixel X of line fp = fv. Clobbers A, Y.
put:    ldy     _xb0,x          ; the even dot: fv's bit 0
        lda     _xm0,x
        eor     #$FF
        and     (fp),y
        sta     ft
        lda     _fv
        lsr     a
        lda     ft
        bcc     @e
        ora     _xm0,x
@e:     sta     (fp),y
        ldy     _xb1,x          ; the odd dot: its bit 1
        lda     _xm1,x
        eor     #$FF
        and     (fp),y
        sta     ft
        lda     _fv
        and     #2
        beq     @o
        lda     ft
        ora     _xm1,x
        sta     (fp),y
        rts
@o:     lda     ft
        sta     (fp),y
        rts

; A = pixel X of line fp. Clobbers Y.
pget:   lda     #0
        sta     ft
        ldy     _xb0,x
        lda     (fp),y
        and     _xm0,x
        beq     @e
        inc     ft
@e:     ldy     _xb1,x
        lda     (fp),y
        and     _xm1,x
        beq     @o
        lda     ft
        ora     #2
        rts
@o:     lda     ft
        rts

; void fld_plot(void): pixel (fx, fy) = fv
_fld_plot:
        jsr     at
        bcs     @x
        jmp     put
@x:     rts

; uint8_t fld_get(void): pixel (fx, fy); 1 (sky) off the field
_fld_get:
        jsr     at
        bcs     @sky
        jsr     pget
        ldx     #0
        rts
@sky:   lda     #1
        ldx     #0
        rts

; void fld_swap(void): pixel (fx, fy) = fv if it is fold
_fld_swap:
        jsr     at
        bcs     @x
        jsr     pget
        cmp     _fold
        bne     @x
        jmp     put
@x:     rts

;
; void fld_dots(void): a shape around (fx, fy): fent points at its count,
; then that many (dx, dy) pairs (signed bytes). fold $FF: each pixel = fv;
; else each pixel that is fold = fv (field.c's tanks: drawn, and taken
; back off to sky). Off the field is left out.
;
dox     = ehw           ; (the ellipse's scratch: not in use between calls)
doy     = edy           ; (doy+1 = ei)
don     = erem
doi     = ft2

_fld_dots:
        lda     _fx
        sta     dox
        lda     _fx+1
        sta     dox+1
        lda     _fy
        sta     doy
        lda     _fy+1
        sta     doy+1
        ldy     #0
        lda     (_fent),y
        beq     @x
        sta     don
        iny
        sty     doi
@l:     ldy     doi
        lda     (_fent),y       ; fx = ox + dx
        tax
        clc
        adc     dox
        sta     _fx
        txa
        and     #$80
        beq     :+
        lda     #$FF
:       adc     dox+1
        sta     _fx+1
        iny
        lda     (_fent),y       ; fy = oy + dy
        tax
        clc
        adc     doy
        sta     _fy
        txa
        and     #$80
        beq     :+
        lda     #$FF
:       adc     doy+1
        sta     _fy+1
        iny
        sty     doi
        jsr     at
        bcs     @n
        lda     _fold
        cmp     #$FF
        beq     @put
        jsr     pget
        cmp     _fold
        bne     @n
@put:   jsr     put
@n:     dec     don
        bne     @l
@x:     lda     dox             ; (fx, fy) as they were
        sta     _fx
        lda     dox+1
        sta     _fx+1
        lda     doy
        sta     _fy
        lda     doy+1
        sta     _fy+1
        rts

; void fld_hspan(void): line fy, x from fx to fx1 (both included) = fv,
; clipped to the field, a byte at a time: a value's dots in a byte are
; pat0's in an even byte, pat1's in an odd one (7 dots a byte: the parity
; turns each byte), so the bytes between the ends are stores, and the end
; bytes take their dots through a mask (from x0's even dot up, to x1's odd
; dot down; bit 7, the palette, never in it)
_fld_hspan:
        lda     _fy+1
        bne     @out
        ldy     _fy
        cpy     #FLINES
        bcc     @in
@out:   rts
@in:    lda     _rowlo,y
        sta     fp
        lda     _rowhi,y
        sta     fp+1
        lda     _fx+1           ; clip x0 to 0, x1 to 139
        bpl     @c0
        lda     #0
        sta     _fx
        sta     _fx+1
@c0:    bne     @out            ; x0 >= 256: off the right
        lda     _fx1+1
        bmi     @out            ; x1 < 0
        bne     @c1
        lda     _fx1
        cmp     #FW
        bcc     @c2
@c1:    lda     #FW-1
        sta     _fx1
        lda     #0
        sta     _fx1+1
@c2:    lda     _fx
        cmp     #FW
        bcs     @out
        cmp     _fx1
        beq     @go
        bcs     @out            ; x0 > x1
@go:    ldx     _fv
        lda     pat0,x
        ora     _fpal
        sta     pv0             ; an even byte's
        lda     pat1,x
        ora     _fpal
        sta     pv1             ; an odd byte's
        ldx     _fx1            ; the last byte, its mask: bits to x1's odd dot
        lda     _xb1,x
        sta     cb1
        lda     _xm1,x
        asl
        sec
        sbc     #1
        sta     nm1
        ldx     _fx             ; the first, its mask: bits from x0's even dot
        lda     _xm0,x
        sec
        sbc     #1
        eor     #$7F
        ldy     _xb0,x
        cpy     cb1
        bne     @two
        and     nm1             ; one byte: both masks
        jmp     rmw
@two:   jsr     rmw
        iny
        tya                     ; the bytes between, by parity
        lsr
        bcs     @odd
@even:  cpy     cb1
        beq     @last
        lda     pv0
        sta     (fp),y
        iny
@odd:   cpy     cb1
        beq     @last
        lda     pv1
        sta     (fp),y
        iny
        bne     @even
@last:  lda     nm1
        ; (on into rmw)

; byte Y of line fp: its bits in mask A take the value's pattern (pv0 or
; pv1 by the byte's parity), the rest stay
rmw:    sta     ft
        tya
        lsr
        lda     pv0
        bcc     :+
        lda     pv1
:       eor     (fp),y
        and     ft
        eor     (fp),y
        sta     (fp),y
        rts

; column setup: pixel fx's dots, the value fv in them
colset: ldx     _fx
        lda     _xb0,x
        sta     cb0
        lda     _xb1,x
        sta     cb1
        lda     _xm0,x
        eor     #$FF
        sta     nm0
        lda     _xm1,x
        eor     #$FF
        sta     nm1
        lda     #0
        sta     vb0
        sta     vb1
        lda     _fv
        lsr     a
        bcc     @e
        lda     _xm0,x
        sta     vb0
@e:     lda     _fv
        and     #2
        beq     @o
        lda     _xm1,x
        sta     vb1
@o:     lda     cb0             ; both in one byte: one mask, one value
        cmp     cb1
        bne     @x
        lda     nm0
        and     nm1
        sta     nm0
        lda     vb0
        ora     vb1
        sta     vb0
@x:     rts

; rows cur..cnt-1 of the column colset set up (the lines aren't in order
; in memory: each line's address from the table)
vrows:  ldx     cur
        cpx     cnt
        bcs     @x
        lda     cb0
        cmp     cb1
        bne     @two
        ldy     cb0
@l1:    lda     _rowlo,x
        sta     fp
        lda     _rowhi,x
        sta     fp+1
        lda     (fp),y
        and     nm0
        ora     vb0
        sta     (fp),y
        inx
        cpx     cnt
        bne     @l1
@x:     rts
@two:   lda     _rowlo,x        ; pixel 3 of 7: its dots in two bytes
        sta     fp
        lda     _rowhi,x
        sta     fp+1
        ldy     cb0
        lda     (fp),y
        and     nm0
        ora     vb0
        sta     (fp),y
        ldy     cb1
        lda     (fp),y
        and     nm1
        ora     vb1
        sta     (fp),y
        inx
        cpx     cnt
        bne     @two
        rts

; void fld_vrun(void): column fx, rows fy to fx1-1 = fv (all in the field)
_fld_vrun:
        jsr     colset
        lda     _fy
        sta     cur
        lda     _fx1
        sta     cnt
        jmp     vrows

; void fld_col(void): column fx becomes the TERRAIN entry at fent (sky,
; dirt); returns the entry's length in A. (A jiffy's tick each: a whole
; field is 140 of these.)
_fld_col:
        jsr     _frame_tick
        lda     #0
        sta     cur             ; the row reached
        ldy     #0
        lda     (_fent),y
        cmp     #$F0
        beq     @empty
        bcs     @spans
        pha                     ; one plain column: sky down to its top, dirt from there
        sta     cnt
        lda     #1
        jsr     run
        pla
        sta     cur
        lda     #FLINES
        sta     cnt
        lda     #2
        jsr     run
        lda     #1
        ldx     #0
        rts
@empty: lda     #FLINES
        sta     cnt
        lda     #1
        jsr     run
        lda     #1
        ldx     #0
        rts
@spans: and     #$0F
        sta     sp2             ; spans left
        asl
        sec
        adc     #0              ; 1 + 2n: the entry's length
        pha
        ldy     #1
@sp:    sty     ys
        lda     (_fent),y       ; top: sky till there
        sta     cnt
        lda     #1
        jsr     run
        ldy     ys
        lda     (_fent),y
        sta     cur
        iny
        lda     (_fent),y       ; end: dirt till there
        sta     cnt
        lda     #2
        jsr     run
        ldy     ys
        iny
        iny
        dec     sp2
        bne     @sp
        lda     #FLINES
        sta     cnt
        lda     #1
        jsr     run
        pla
        ldx     #0
        rts

; rows cur..cnt-1 of column fx = A (0-3); cur = cnt after
run:    sta     _fv
        lda     cnt
        cmp     #FLINES+1
        bcc     @ok
        lda     #FLINES
        sta     cnt
@ok:    jsr     colset
        jsr     vrows
        lda     cnt
        sta     cur
        rts

; void fld_ellipse(void): the ellipse at (ex, ey), radii erx, ery (the
; Atari's: its row dy is erx * q32[i] / 64 to each side, i = dy*32/ery
; stepped without dividing)
_fld_ellipse:
        jsr     _frame_tick
        lda     _ery
        bne     @r
        inc     _ery
@r:     lsr
        sta     erem            ; ery / 2: the rounding
        lda     #0
        sta     edy
        sta     ei
@row:   ldx     ei              ; hw = (erx * q32[i] + 32) >> 6
        lda     q32,x
        sta     ft
        lda     #0
        sta     ehw
        sta     ehw+1
        lda     _erx
        sta     ft2
        ldx     #8
@m:     lsr     ft2
        bcc     @ms
        clc
        lda     ehw+1
        adc     ft
        sta     ehw+1
@ms:    ror     ehw+1
        ror     ehw
        dex
        bne     @m
        clc
        lda     ehw
        adc     #32
        sta     ehw
        bcc     @sh
        inc     ehw+1
@sh:    ldx     #6
@d:     lsr     ehw+1
        ror     ehw
        dex
        bne     @d
        sec                     ; the row above: ey - dy
        lda     _ey
        sbc     edy
        sta     _fy
        lda     _ey+1
        sbc     #0
        sta     _fy+1
        jsr     erow
        lda     edy
        beq     @nx
        clc                     ; and below: ey + dy
        lda     _ey
        adc     edy
        sta     _fy
        lda     _ey+1
        adc     #0
        sta     _fy+1
        jsr     erow
@nx:    lda     edy
        cmp     _ery
        bcs     @x
        inc     edy
        clc
        lda     erem
        adc     #32
        sta     erem
@i:     lda     erem
        cmp     _ery
        bcc     @row
        sbc     _ery
        sta     erem
        lda     ei
        cmp     #32
        bcs     @i
        inc     ei
        bne     @i
@x:     rts

; one row of the ellipse: line fy, ex - hw to ex + hw (the clock ticked:
; a big ring, pixel by pixel, is longer than its 6522 wraps)
erow:   jsr     _frame_tick
        sec
        lda     _ex
        sbc     ehw
        sta     _fx
        lda     _ex+1
        sbc     #0
        sta     _fx+1
        clc
        lda     _ex
        adc     ehw
        sta     _fx1
        lda     _ex+1
        adc     #0
        sta     _fx1+1
        lda     _emode
        bne     @px
        jmp     _fld_hspan
@px:    lda     _emode          ; pixel by pixel: only the right colour changes
        cmp     #1
        bne     @m2
        lda     #1
        sta     _fold
        bne     @l
@m2:    lda     #3
        sta     _fold
        lda     #1
        sta     _fv
@l:     lda     _fx+1           ; for fx up to fx1
        cmp     _fx1+1
        bmi     @go
        bne     @y
        lda     _fx
        cmp     _fx1
        beq     @go
        bcs     @y
@go:    lda     _fx
        pha
        lda     _fx+1
        pha
        jsr     _fld_swap
        pla
        sta     _fx+1
        pla
        sta     _fx
        inc     _fx
        bne     @l
        inc     _fx+1
        jmp     @l
@y:     rts
