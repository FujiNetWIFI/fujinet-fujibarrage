;
; ops.s -- the script's paths (OP_PATH), flows (OP_FLOW) and beams (OP_BEAM)
; on the field (docs/PROTOCOL.md). Paths and flows read their bytes where
; they lie in script[]; at a segment's end they finish (ops_seg_end). The
; coordinates are the field's own (x 0-159, y 0-167): a byte each, and one
; that wraps past an edge is simply off the field. FLD_WIDE (the NES: x
; 0-255, where no wrap is off it): x is the script's 16 bits.
;
; uint8_t __fastcall__ ops_path(const uint8_t *args)  an OP_PATH's arguments:
; uint8_t __fastcall__ ops_flow(const uint8_t *args)  each says how many
; uint8_t __fastcall__ ops_beam(const uint8_t *args)  bytes it took
; uint8_t ops_step(void)       a frame: non-zero if a fire flow burns
; void ops_seg_end(void)       paths stop, flows run out
; void ops_beams_off(void)     beams gone (the script's end)
; void ops_reset(void)         all off (a new shot)
;
        .export _ops_path, _ops_flow, _ops_beam, _ops_step
        .export _ops_seg_end, _ops_beams_off, _ops_reset
        .importzp _fx, _fy, _fv, _fold
        .import _fld_swap, _fld_get, _fld_plot

NPATH     = 4
NFLOW     = 4
NBEAM     = 2
ST_DIGGER = 4
FLOW_MUD  = 1
PX_SKY    = 1
PX_DIRT   = 2
PX_FX     = 3

        .segment "ZEROPAGE"
optr:   .res 2          ; the bytes an op or a path reads
oi:     .res 1          ; the path / flow / beam in hand
ocnt:   .res 1
oto:    .res 1
omud:   .res 1
otmp:   .res 1
ofire:  .res 1
lx0:    .res 1          ; a line being drawn
ly0:    .res 1
lx1:    .res 1
ly1:    .res 1
ldx_:   .res 1
ldy_:   .res 1
lsx:    .res 1
lsy:    .res 1
lerr:   .res 2
le2:    .res 2
lon:    .res 1
.ifdef FLD_WIDE
lx0h:   .res 1          ; the line's x, high bytes
lx1h:   .res 1
ldxh:   .res 1          ; |dx|'s
lsxh:   .res 1          ; the step's (0 or $FF)
ohi:    .res 1
.endif

        .segment "MIDBSS"
data:
pa_on:  .res NPATH
pa_sty: .res NPATH
pa_x:   .res NPATH
pa_y:   .res NPATH
pa_n:   .res NPATH
pa_dr:  .res NPATH      ; its head is drawn
pa_lo:  .res NPATH      ; its next step
pa_hi:  .res NPATH
fl_on:  .res NFLOW
fl_kind: .res NFLOW
fl_frm: .res NFLOW      ; frames to spread over
fl_f:   .res NFLOW      ; frames so far
fl_hold: .res NFLOW
fl_n:   .res NFLOW
fl_sh:  .res NFLOW      ; cells shown
fl_acc: .res NFLOW      ; n x frames so far, less frames x shown (16-bit)
fl_acch: .res NFLOW
fl_lo:  .res NFLOW
fl_hi:  .res NFLOW
be_left: .res NBEAM
be_x0:  .res NBEAM
be_y0:  .res NBEAM
be_x1:  .res NBEAM
be_y1:  .res NBEAM
.ifdef FLD_WIDE
pa_xh:  .res NPATH
be_x0h: .res NBEAM
be_x1h: .res NBEAM
.endif
data_end:

        .code

; (fx, fy) = (A, Y)
setxy:  sta     _fx
        sty     _fy
        lda     #0
        sta     _fx+1
        sta     _fy+1
        rts

; ---- paths --------------------------------------------------------------------

; phead: path X's head, drawn: a roller over air, a digger over air or dirt
phead:  stx     oi
        lda     #0
        sta     pa_dr,x
        lda     pa_x,x
        ldy     pa_y,x
        jsr     setxy
.ifdef FLD_WIDE
        lda     pa_xh,x
        sta     _fx+1
.endif
        jsr     _fld_get
        ldx     oi
        cmp     #PX_SKY
        beq     @draw
        cmp     #PX_DIRT
        bne     @x
        lda     pa_sty,x
        cmp     #ST_DIGGER
        bne     @x
@draw:  lda     #PX_FX
        sta     _fv
        jsr     _fld_plot
        ldx     oi
        inc     pa_dr,x
@x:     rts

; pleave: path X's head moves on (a digger has bored: air behind it)
pleave: stx     oi
        lda     pa_dr,x
        beq     @x
        lda     #0
        sta     pa_dr,x
        lda     pa_x,x
        ldy     pa_y,x
        jsr     setxy
.ifdef FLD_WIDE
        lda     pa_xh,x
        sta     _fx+1
.endif
        lda     #PX_FX
        sta     _fold
        lda     #PX_SKY
        sta     _fv
        jsr     _fld_swap
        ldx     oi
@x:     rts

; pend: path X stops
pend:   lda     pa_on,x
        beq     @x
        jsr     pleave
        lda     #0
        sta     pa_on,x
@x:     rts

_ops_path:
        sta     optr
        stx     optr+1
        ldy     #0
        lda     (optr),y        ; slot
        and     #NPATH-1
        tax
        jsr     pend
        ldy     #1
        lda     (optr),y
        sta     pa_sty,x
        iny
        lda     (optr),y        ; x (low byte)
        sta     pa_x,x
.ifdef FLD_WIDE
        iny
        lda     (optr),y
        sta     pa_xh,x
.endif
        ldy     #4
        lda     (optr),y        ; y
        sta     pa_y,x
        ldy     #6
        lda     (optr),y        ; n
        sta     pa_n,x
        clc
        lda     optr
        adc     #7
        sta     pa_lo,x
        lda     optr+1
        adc     #0
        sta     pa_hi,x
        lda     #1
        sta     pa_on,x
        jsr     phead
        lda     pa_n,x
        clc
        adc     #7
        ldx     #0
        rts

; nib: A's low nibble, signed, as a byte
nib:    asl
        asl
        asl
        asl
; sr4: A >> 4, arithmetic
sr4:    cmp     #$80
        ror
        cmp     #$80
        ror
        cmp     #$80
        ror
        cmp     #$80
        ror
        rts

pstep:  ldx     #NPATH-1
@l:     lda     pa_on,x
        beq     @n
        lda     pa_n,x
        bne     @mv
        jsr     pend
        jmp     @n
@mv:    jsr     pleave
        dec     pa_n,x
        lda     pa_lo,x
        sta     optr
        lda     pa_hi,x
        sta     optr+1
        inc     pa_lo,x
        bne     @s
        inc     pa_hi,x
@s:     ldy     #0
        lda     (optr),y
        sta     otmp
        jsr     sr4
.ifdef FLD_WIDE
        ldy     #0              ; (its sign, for the high byte)
        cmp     #$80
        bcc     @sx
        dey
@sx:    sty     ohi
        clc
        adc     pa_x,x
        sta     pa_x,x
        lda     pa_xh,x
        adc     ohi
        sta     pa_xh,x
.else
        clc
        adc     pa_x,x
        sta     pa_x,x
.endif
        lda     otmp
        jsr     nib
        clc
        adc     pa_y,x
        sta     pa_y,x
        jsr     phead
@n:     dex
        bpl     @l
        rts

; ---- flows ----------------------------------------------------------------------

; fcells: flow oi's cells ocnt to oto-1, fold -> fv (and, mud, fire -> fv)
fcells: ldx     oi
        lda     fl_lo,x
        sta     optr
        lda     fl_hi,x
        sta     optr+1
@l:     lda     ocnt
        cmp     oto
        bcs     @x
        asl                     ; 3 bytes a cell (at most 60 cells)
        adc     ocnt
        tay
        lda     (optr),y
        sta     _fx
        iny
        lda     (optr),y
        sta     _fx+1
        iny
        lda     (optr),y
        sta     _fy
        lda     #0
        sta     _fy+1
        jsr     _fld_swap
        lda     omud
        beq     @n
        lda     _fold
        pha
        lda     #PX_FX
        sta     _fold
        jsr     _fld_swap
        pla
        sta     _fold
@n:     inc     ocnt
        bne     @l
@x:     ldx     oi
        rts

; fshow: flow X's first A cells in (fire over the air, mud as dirt)
fshow:  stx     oi
        cmp     fl_sh,x
        beq     @x
        bcc     @x
        sta     oto
        lda     fl_sh,x
        sta     ocnt
        lda     oto
        sta     fl_sh,x
        lda     #PX_SKY
        sta     _fold
        lda     #0
        sta     omud
        lda     #PX_FX
        ldy     fl_kind,x
        cpy     #FLOW_MUD
        bne     @f
        inc     omud
        lda     #PX_DIRT
@f:     sta     _fv
        jsr     fcells
@x:     rts

; fend: flow X all in, then a fire goes out
fend:   lda     fl_n,x
        jsr     fshow
        lda     fl_kind,x
        cmp     #FLOW_MUD
        beq     @off
        stx     oi
        lda     #0
        sta     ocnt
        sta     omud
        lda     fl_n,x
        sta     oto
        lda     #PX_FX
        sta     _fold
        lda     #PX_SKY
        sta     _fv
        jsr     fcells
@off:   lda     #0
        sta     fl_on,x
        rts

_ops_flow:
        sta     optr
        stx     optr+1
        ldx     #0
@free:  lda     fl_on,x
        beq     @got
        inx
        cpx     #NFLOW-1
        bne     @free
        lda     fl_on,x
        beq     @got
        lda     optr            ; full: the last one ends now
        pha
        lda     optr+1
        pha
        jsr     fend
        pla
        sta     optr+1
        pla
        sta     optr
@got:   ldy     #0
        lda     (optr),y
        sta     fl_kind,x
        iny
        lda     (optr),y
        bne     @fr
        lda     #1
@fr:    sta     fl_frm,x
        iny
        lda     (optr),y
        sta     fl_hold,x
        iny
        lda     (optr),y
        sta     fl_n,x
        lda     #0
        sta     fl_f,x
        sta     fl_sh,x
        sta     fl_acc,x
        sta     fl_acch,x
        clc
        lda     optr
        adc     #4
        sta     fl_lo,x
        lda     optr+1
        adc     #0
        sta     fl_hi,x
        lda     #1
        sta     fl_on,x
        lda     fl_n,x          ; took 4 + 3n
        asl
        adc     fl_n,x
        adc     #4
        ldx     #0
        rts

fstep:  ldx     #NFLOW-1
@l:     lda     fl_on,x
        beq     @n
        lda     fl_kind,x
        cmp     #FLOW_MUD
        beq     @m
        lda     #1
        sta     ofire
@m:     lda     fl_f,x
        cmp     fl_frm,x
        bcs     @hold
        inc     fl_f,x
        clc                     ; acc += n; each frames of it: a cell more
        lda     fl_acc,x
        adc     fl_n,x
        sta     fl_acc,x
        bcc     @a
        inc     fl_acch,x
@a:     lda     fl_sh,x
        sta     ocnt
@sub:   sec
        lda     fl_acc,x
        sbc     fl_frm,x
        tay
        lda     fl_acch,x
        sbc     #0
        bcc     @sh
        sta     fl_acch,x
        tya
        sta     fl_acc,x
        inc     ocnt
        bne     @sub
@sh:    lda     ocnt
        jsr     fshow
        jmp     @end
@hold:  lda     fl_hold,x
        beq     @end
        dec     fl_hold,x
@end:   lda     fl_f,x
        cmp     fl_frm,x
        bcc     @n
        lda     fl_hold,x
        bne     @n
        jsr     fend
@n:     dex
        bpl     @l
        rts

; ---- beams --------------------------------------------------------------------------

; lplot: (lx0, ly0): on, fire over air and dirt; off, fire back to air
lplot:  lda     lx0
        ldy     ly0
        jsr     setxy
.ifdef FLD_WIDE
        lda     lx0h
        sta     _fx+1
.endif
        lda     lon
        beq     @off
        lda     #PX_FX
        sta     _fv
        lda     #PX_SKY
        sta     _fold
        jsr     _fld_swap
        lda     #PX_DIRT
        sta     _fold
        jmp     _fld_swap
@off:   lda     #PX_FX
        sta     _fold
        lda     #PX_SKY
        sta     _fv
        jmp     _fld_swap

; bline: beam X's line (Bresenham, as the reference view draws it)
bline:  stx     oi
        lda     be_x0,x
        sta     lx0
        lda     be_y0,x
        sta     ly0
        lda     be_x1,x
        sta     lx1
        lda     be_y1,x
        sta     ly1
.ifdef FLD_WIDE
        lda     be_x0h,x
        sta     lx0h
        lda     be_x1h,x
        sta     lx1h
        ldy     #1              ; dx = x1 - x0, 16-bit: |dx|, its sign
        lda     #0
        sta     lsxh
        sec
        lda     lx1
        sbc     lx0
        sta     ldx_
        lda     lx1h
        sbc     lx0h
        sta     ldxh
        bpl     @px
        sec
        lda     #0
        sbc     ldx_
        sta     ldx_
        lda     #0
        sbc     ldxh
        sta     ldxh
        ldy     #$FF
        sty     lsxh
@px:    sty     lsx
.else
        ldy     #1
        lda     lx1
        sec
        sbc     lx0
        bcs     @px
        eor     #$FF
        adc     #1
        ldy     #$FF
@px:    sta     ldx_
        sty     lsx
.endif
        ldy     #1
        lda     ly1
        sec
        sbc     ly0
        bcs     @py
        eor     #$FF
        adc     #1
        ldy     #$FF
@py:    sta     ldy_
        sty     lsy
        sec                     ; err = dx - dy
        lda     ldx_
        sbc     ldy_
        sta     lerr
.ifdef FLD_WIDE
        lda     ldxh
.else
        lda     #0
.endif
        sbc     #0
        sta     lerr+1
@l:     jsr     lplot
        lda     lx0
        cmp     lx1
        bne     @go
.ifdef FLD_WIDE
        lda     lx0h
        cmp     lx1h
        bne     @go
.endif
        lda     ly0
        cmp     ly1
        beq     @x
@go:    lda     lerr            ; e2 = 2 err
        asl
        sta     le2
        lda     lerr+1
        rol
        sta     le2+1
        clc                     ; e2 > -dy: x moves
        lda     le2
        adc     ldy_
        sta     otmp
        lda     le2+1
        adc     #0
        bmi     @ny
        ora     otmp
        beq     @ny
        sec
        lda     lerr
        sbc     ldy_
        sta     lerr
        lda     lerr+1
        sbc     #0
        sta     lerr+1
        clc
        lda     lx0
        adc     lsx
        sta     lx0
.ifdef FLD_WIDE
        lda     lx0h
        adc     lsxh
        sta     lx0h
.endif
@ny:    sec                     ; e2 < dx: y moves
        lda     le2
        sbc     ldx_
        lda     le2+1
.ifdef FLD_WIDE
        sbc     ldxh
.else
        sbc     #0
.endif
        bpl     @l
        clc
        lda     lerr
        adc     ldx_
        sta     lerr
        lda     lerr+1
.ifdef FLD_WIDE
        adc     ldxh
.else
        adc     #0
.endif
        sta     lerr+1
        clc
        lda     ly0
        adc     lsy
        sta     ly0
        jmp     @l
@x:     ldx     oi
        rts

; boff: beam X's pixels back to air, and it is gone
boff:   lda     be_left,x
        beq     @x
        lda     #0
        sta     be_left,x
        sta     lon
        jmp     bline
@x:     rts

_ops_beam:
        sta     optr
        stx     optr+1
        ldx     #0
        lda     be_left
        beq     @s
        inx
@s:     jsr     boff
        ldy     #0
        lda     (optr),y
        sta     be_x0,x
.ifdef FLD_WIDE
        iny
        lda     (optr),y
        sta     be_x0h,x
        ldy     #5
        lda     (optr),y
        sta     be_x1h,x
.endif
        ldy     #2
        lda     (optr),y
        sta     be_y0,x
        ldy     #4
        lda     (optr),y
        sta     be_x1,x
        ldy     #6
        lda     (optr),y
        sta     be_y1,x
        ldy     #9
        lda     (optr),y
        bne     @f
        lda     #1
@f:     sta     be_left,x
        lda     #1
        sta     lon
        jsr     bline
        lda     #10
        ldx     #0
        rts

bstep:  ldx     #NBEAM-1
@l:     lda     be_left,x
        beq     @n
        dec     be_left,x
        bne     @n
        inc     be_left,x       ; boff wants it live
        jsr     boff
@n:     dex
        bpl     @l
        rts

; ---- a frame, a segment's end, a new shot ----------------------------------------

_ops_step:
        lda     #0
        sta     ofire
        jsr     pstep
        jsr     fstep
        jsr     bstep
        lda     ofire
        ldx     #0
        rts

_ops_seg_end:
        ldx     #NPATH-1
@p:     jsr     pend
        dex
        bpl     @p
        ldx     #NFLOW-1
@f:     lda     fl_on,x
        beq     @n
        jsr     fend
@n:     dex
        bpl     @f
        rts

_ops_beams_off:
        ldx     #NBEAM-1
@l:     jsr     boff
        dex
        bpl     @l
        rts

_ops_reset:
        lda     #0
        ldx     #data_end-data
@l:     sta     data-1,x
        dex
        bne     @l
        rts
