;
; fld.s -- the field's pixels: ANTIC mode E, 160x168, four pixels a byte
; (pixel 0 in bits 7-6), 40 bytes a line from FIELD on (barrage.cfg).
;
;   00  a tank (COLBK, black; a player's colour shows through it)
;   01  sky    (COLPF0, the DLIs' gradient)
;   10  dirt   (COLPF1, the DLIs' gradient)
;   11  fire, shells, text (COLPF2, cycled by the VBI)
;
; The routines take their arguments in zero page (fld.h), not on cc65's
; stack: they run for every pixel of a blast.
;
        .export _fld_init, _fld_clear, _fld_plot, _fld_get, _fld_hspan
        .export _fld_vrun, _fld_col, _fld_swap, _fld_ellipse
        .exportzp _fx, _fy, _fx1, _fv, _fent, _fold, _ex, _ey, _erx, _ery, _emode
        .export _rowlo, _rowhi
.ifdef __ATARI7800__
        .export _fld_cols4
.endif

        .include "hw.inc"
        .include "proto.inc"

        .segment "ZEROPAGE"
_fx:    .res 2          ; x (int16)
_fy:    .res 2          ; y (int16)
_fx1:   .res 2          ; a span's last x / a run's end y (exclusive)
_fv:    .res 1          ; the value, 0-3
_fold:  .res 1          ; fld_swap: the value to replace
_fent:  .res 2          ; fld_col: the column entry
fp:     .res 2
ft:     .res 1
ft2:    .res 1
xb:     .res 1          ; a column's byte in its line
nm:     .res 1          ; its pixel's mask, inverted
val:    .res 1          ; the value in that pixel's place
cur:    .res 1
cnt:    .res 1
ys:     .res 1          ; fld_col: where it is in the entry
_ex:    .res 2          ; fld_ellipse: the centre,
_ey:    .res 2
_erx:   .res 1          ; the radii (ery at most 200),
_ery:   .res 1
_emode: .res 1          ; 0 all fv, 1 sky becomes fv, 2 colour 3 becomes sky
edy:    .res 1
ei:     .res 1
erem:   .res 1
ehw:    .res 2
.ifdef __ATARI7800__
t4:     .res 4          ; fld_cols4: the 4 columns' tops
nxt:    .res 1
swp:    .res 1          ; hswap: 1 (hspan: 0)
oldf:   .res 1          ; hswap: fold's byte
.endif

.ifdef __ATARI7800__
        .rodata                 ; (MARIA's zones: line y is zone z = y / 8's line
_rowlo: .repeat FLINES, I       ; r = y % 8, at page $40 + 8 (z / 6) + 7 - r,
        .byte   40 * ((I / 8) .mod 6)   ; byte 40 (z % 6): a78.inc)
        .endrep
_rowhi: .repeat FLINES, I
        .byte   $40 + 8 * (I / 48) + 7 - (I .mod 8)
        .endrep
.elseif .defined(__ATARI5200__)
        .rodata                 ; (16K of RAM: the field never moves, so they're ROM)
_rowlo: .repeat FLINES, I
        .byte   <(FIELD + I * 40)
        .endrep
_rowhi: .repeat FLINES, I
        .byte   >(FIELD + I * 40)
        .endrep
.else
        .segment "LOWBSS"
_rowlo: .res FLINES
_rowhi: .res FLINES
.endif

        .rodata
; a quarter circle: q32[i] = 64 sqrt(1 - (i/32)^2)
q32:    .byte   64, 64, 64, 63, 63, 62, 62, 61, 60, 59, 57, 56, 54, 53, 51, 49
        .byte   46, 44, 41, 38, 35, 32, 28, 24, 20, 15, 10, 5, 0, 0, 0, 0, 0
mask:   .byte   $C0, $30, $0C, $03
fill:   .byte   $00, $55, $AA, $FF
lmask:  .byte   $FF, $3F, $0F, $03      ; a span's first byte: from pixel n on
rmask:  .byte   $C0, $F0, $FC, $FF      ; its last byte: up to pixel n
.ifdef __ATARI7800__
cmask:  .byte   $3F, $CF, $F3, $FC      ; fld_cols4: all but pixel n,
cdirt:  .byte   $80, $20, $08, $02      ; pixel n's dirt
.endif

        .code

; void fld_init(void): the line address tables
_fld_init:
.if .not (.defined(__ATARI5200__) .or .defined(__ATARI7800__))  ; (theirs are ROM)
        lda     #<FIELD
        sta     fp
        lda     #>FIELD
        sta     fp+1
        ldx     #0
@l:     lda     fp
        sta     _rowlo,x
        lda     fp+1
        sta     _rowhi,x
        lda     fp
        clc
        adc     #40
        sta     fp
        bcc     @n
        inc     fp+1
@n:     inx
        cpx     #FLINES
        bne     @l
.endif
        rts

; void fld_clear(void): all sky
_fld_clear:
.ifdef __ATARI7800__
        ldx     #0              ; (line by line: the zones' spare bytes are
@ln:    lda     _rowlo,x        ; the underlays' strips and the text screen)
        sta     fp
        lda     _rowhi,x
        sta     fp+1
        lda     #$55
        ldy     #39
@b:     sta     (fp),y
        dey
        bpl     @b
        inx
        cpx     #FLINES
        bne     @ln
        rts
.else
        lda     #<FIELD
        sta     fp
        lda     #>FIELD
        sta     fp+1
        ldx     #>(FLINES*40)   ; whole pages
        lda     #$55
        ldy     #0
@p:     sta     (fp),y
        iny
        bne     @p
        inc     fp+1
        dex
        bne     @p
        ldy     #<(FLINES*40)
        beq     @x
@r:     dey
        sta     (fp),y
        bne     @r
@x:     rts
.endif

; in range? carry clear: fp is the line's address, A the byte, X the pixel
at:     lda     _fx+1
        bne     @out
        lda     _fx
        cmp     #160
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
        lda     _fx
        and     #3
        tax
        lda     _fx
        lsr
        lsr
        tay
        clc
        rts
@out:   sec
        rts

; void fld_plot(void): pixel (fx, fy) = fv
_fld_plot:
        jsr     at
        bcs     @x
        lda     mask,x
        sta     ft
        eor     #$FF
        and     (fp),y
        sta     ft2
        ldx     _fv
        lda     fill,x
        and     ft
        ora     ft2
        sta     (fp),y
@x:     rts

; uint8_t fld_get(void): pixel (fx, fy); 1 (sky) off the field
_fld_get:
        jsr     at
        bcs     @sky
        lda     (fp),y
        and     mask,x
@sh:    cpx     #3
        beq     @r
        lsr
        lsr
        inx
        bne     @sh
@r:     ldx     #0
        rts
@sky:   lda     #1
        ldx     #0
        rts

; void fld_swap(void): pixel (fx, fy) = fv if it is fold
_fld_swap:
        jsr     at
        bcs     @x
        stx     cnt
        lda     (fp),y
        and     mask,x
        sta     ft
        ldx     _fold
        lda     fill,x
        ldx     cnt
        and     mask,x
        cmp     ft
        bne     @x
        lda     mask,x
        sta     ft
        eor     #$FF
        and     (fp),y
        sta     ft2
        ldx     _fv
        lda     fill,x
        and     ft
        ora     ft2
        sta     (fp),y
@x:     rts

; void fld_hspan(void): line fy, x from fx to fx1 (both included) = fv,
; clipped to the field
_fld_hspan:
.ifdef __ATARI7800__
        lda     #0              ; (hswap: the same span, but only its pixels
        sta     swp             ; of value fold become fv, a byte at a time)
        beq     hspan2
; hswap: line fy, fx to fx1, the pixels that are fold become fv (an
; ellipse's modes 1 and 2: it was fld_swap a pixel at a time)
hswap:  ldx     _fold
        lda     fill,x
        sta     oldf
        lda     #1
        sta     swp
hspan2:
.endif
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
        ; clip x0 to 0, x1 to 159
        lda     _fx+1
        bpl     @c0
        lda     #0
        sta     _fx
        sta     _fx+1
.ifdef __ATARI7800__
@c0:    bne     @out            ; x0 >= 256: off the right
        lda     _fx1+1
        bmi     @out            ; x1 < 0
.else
@c0:    bne     @x              ; x0 >= 256: off the right
        lda     _fx1+1
        bmi     @x              ; x1 < 0
.endif
        bne     @c1
        lda     _fx1
        cmp     #160
        bcc     @c2
@c1:    lda     #159
        sta     _fx1
        lda     #0
        sta     _fx1+1
@c2:    lda     _fx
        cmp     #160
        bcs     @x
        cmp     _fx1
        beq     @go
        bcs     @x              ; x0 > x1
@go:    ldx     _fv
        lda     fill,x
        sta     val
        lda     _fx
        lsr
        lsr
        sta     xb              ; first byte
        lda     _fx1
        lsr
        lsr
        sta     cur             ; last byte
        lda     _fx
        and     #3
        tax
        lda     lmask,x
        sta     ft              ; first byte's mask
        lda     _fx1
        and     #3
        tax
        lda     rmask,x
        sta     ft2             ; last byte's mask
        ldy     xb
        cpy     cur
        bne     @multi
        lda     ft              ; one byte: both masks
        and     ft2
        sta     ft
        jmp     @edge
@multi: jsr     @edge1
        iny
@mid:   cpy     cur
        beq     @last
.ifdef __ATARI7800__
        lda     swp
        bne     @msw
.endif
        lda     val
        sta     (fp),y
        iny
        bne     @mid
.ifdef __ATARI7800__
@msw:   lda     #$FF
        sta     ft
        jsr     @edge1
        iny
        bne     @mid
.endif
@last:  lda     ft2
        sta     ft
@edge:  ; byte y: mask ft gets val
@edge1:
.ifdef __ATARI7800__
        lda     swp
        bne     @sw
.endif
        lda     ft
        eor     #$FF
        and     (fp),y
        sta     cnt
        lda     val
        and     ft
        ora     cnt
        sta     (fp),y
@x:     rts
.ifdef __ATARI7800__
@sw:    lda     (fp),y          ; its pixels that are fold, in mask ft
        eor     oldf
        sta     cnt
        lsr     a
        ora     cnt
        and     #$55
        sta     cnt
        asl     a
        ora     cnt             ; (11 where a pixel isn't fold)
        eor     #$FF
        and     ft
        sta     cnt
        eor     #$FF
        and     (fp),y
        sta     nm
        lda     val
        and     cnt
        ora     nm
        sta     (fp),y
        rts
.endif

; column setup: fx's byte and pixel mask, the value fv in it
colset: lda     _fx
        lsr
        lsr
        sta     xb
        lda     _fx
        and     #3
        tax
        lda     mask,x
        sta     ft
        eor     #$FF
        sta     nm
        ldx     _fv
        lda     fill,x
        and     ft
        sta     val
        rts

; rows cur..cnt-1 of column xb get val
.ifdef __ATARI7800__
vrows:  lda     #0              ; (MARIA's zones: a line down is a page down,
        sta     fp              ; but every 8th, a zone's own block)
@z:     ldx     cur
        cpx     cnt
        bcs     @x
        lda     _rowhi,x
        sta     fp+1
        lda     _rowlo,x
        clc
        adc     xb              ; (no carry: the byte is under 240)
        tay
        txa
        ora     #7
        adc     #1              ; the zone's end, or cnt
        cmp     cnt
        bcc     @k
        lda     cnt
@k:     sta     ft
        sec
        sbc     cur
        tax                     ; its lines
        lda     ft
        sta     cur
@l:     lda     (fp),y
        and     nm
        ora     val
        sta     (fp),y
        dec     fp+1
        dex
        bne     @l
        beq     @z
@x:     rts
.else
vrows:  lda     cnt             ; (the lines are 40 bytes apart, all of
        sec                     ; them: line 102 starts at $B000)
        sbc     cur
        beq     @x
        bcc     @x
        tax                     ; lines
        ldy     cur
        lda     _rowlo,y
        clc
        adc     xb
        sta     fp
        lda     _rowhi,y
        adc     #0
        sta     fp+1
        ldy     #0
@l:     lda     (fp),y
        and     nm
        ora     val
        sta     (fp),y
        tya                     ; (carry clear)
        adc     #40
        tay
        bcc     @n
        inc     fp+1
        clc
@n:     dex
        bne     @l
@x:     rts
.endif

.ifdef __ATARI7800__
; uint8_t fld_cols4(void): columns fx .. fx+3 (fx a multiple of 4) the 4
; plain TERRAIN entries at fent (each a top: sky above it, dirt from it
; down; $F0 none), a byte column at a time: each line's byte built from the
; four and stored, not read (by columns, every byte was read and written
; four times: the NES's fld_cols8, for MARIA's zones). 1 if an entry has
; spans (nothing done: fld_col's, a column at a time); the Z flag as A
; (src/6502/ply.s's settle tests it).
_fld_cols4:
        ldy     #3
@t:     lda     (_fent),y
        cmp     #T_SPANS+1
        bcs     @no
        cmp     #FLINES
        bcc     @tt
        lda     #FLINES         ; ($F0: all sky)
@tt:    sta     t4,y
        dey
        bpl     @t
        lda     _fx
        lsr     a
        lsr     a
        sta     xb
        lda     #$55            ; the byte: all sky, then each column's dirt
        sta     val             ; from its top on
        lda     #0
        sta     cur
@ev:    ldy     #3              ; the columns whose dirt starts at cur
@e1:    lda     t4,y
        cmp     cur
        bne     @e2
        lda     val
        and     cmask,y
        ora     cdirt,y
        sta     val
@e2:    dey
        bpl     @e1
        lda     #FLINES         ; the next top after cur
        sta     nxt
        ldy     #3
@n1:    lda     t4,y
        cmp     cur
        beq     @n2
        bcc     @n2
        cmp     nxt
        bcs     @n2
        sta     nxt
@n2:    dey
        bpl     @n1
        jsr     cfill
        lda     nxt
        sta     cur
        cmp     #FLINES
        bcc     @ev
        ldx     #0
        lda     #0              ; (A last: the caller tests Z)
        rts
@no:    ldx     #0
        lda     #1
        rts

; lines cur..nxt-1 of byte column xb = val (MARIA's zones, as vrows)
cfill:  lda     #0
        sta     fp
        ldx     cur
@z:     cpx     nxt
        bcs     @x
        lda     _rowhi,x
        sta     fp+1
        lda     _rowlo,x
        clc
        adc     xb
        tay
        txa
        ora     #7
        adc     #1
        cmp     nxt
        bcc     @k
        lda     nxt
@k:     sta     ft
        sec
        sbc     cur
        tax
        lda     ft
        sta     cur
        lda     val
@l:     sta     (fp),y
        dec     fp+1
        dex
        bne     @l
        ldx     cur
        jmp     @z
@x:     rts
.endif

; void fld_vrun(void): column fx, rows fy to fx1-1 = fv (all in the field)
_fld_vrun:
        jsr     colset
        lda     _fy
        sta     cur
        lda     _fx1
        sta     cnt
        jmp     vrows

; void fld_col(void): column fx becomes the TERRAIN entry at fent (sky,
; dirt); returns the entry's length in A
_fld_col:
        lda     #0
        sta     cur             ; the row reached
        ldy     #0
        lda     (_fent),y
        cmp     #$F0
        beq     @empty
        bcs     @spans
        ; one plain column: sky down to its top, dirt from there
        pha
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
        sta     ft2             ; spans left
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
        dec     ft2
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

; void fld_ellipse(void): the ellipse at (ex, ey), radii erx, ery. Its row
; dy is half as wide as erx * sqrt(1 - (dy/ery)^2): q32[i], i = dy*32/ery
; stepped without dividing.
_fld_ellipse:
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

; one row of the ellipse: line fy, ex - hw to ex + hw
erow:   sec
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
@l:
.ifdef __ATARI7800__
        jmp     hswap           ; (a byte at a time)
.endif
        lda     _fx+1           ; for fx up to fx1
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
