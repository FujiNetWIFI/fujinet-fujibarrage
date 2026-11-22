;
; ply.s -- the script player's timeline (docs/PROTOCOL.md "Playing a
; segment"): each frame every track, blast, path, flow, beam and settle
; moves a step, then the ops due run. The ops about tanks, talk, damage and
; sound go to C (c_op in play.c), which reads their arguments at ply_pc.
;
; void ply_reset(void)   a new shot: nothing moving
; void ply_step(void)    a frame
; void ply_draw(void)    the blasts at their size now
; void ply_end(void)     the segment's bytes are going: settles land, paths
;                        and flows finish (ops.s)
; ply_pc, ply_wait, ply_done: where it is in script[], frames to wait, OP_END
;
        .export _ply_reset, _ply_step, _ply_draw, _ply_end
        .export _ply_pc, _ply_wait, _ply_done
        .import _script, _seg_len, _st
        .import _trk_reset, _trk_load, _trk_stop, _trk_step
        .import _ops_path, _ops_flow, _ops_beam, _ops_step, _ops_seg_end, _ops_reset
        .import _fld_ellipse, _fld_col, _field_fire
        .import _c_op, _c_anim, _pt_step, _pt_clear, _dsp_shake
        .importzp _fx, _fv, _fent, _ex, _ey, _erx, _ery, _emode

        .include "proto.inc"

PX_SKY  = 1                     ; (field.h)
PX_DIRT = 2
PX_FX   = 3
NBLAST  = 10
NSETTLE = 8
DIRTBALL = $80                  ; an OP_DIRT, kept with the blasts

        .segment "ZEROPAGE"
_ply_pc:   .res 2
pp:        .res 2               ; script + ply_pc
bi:        .res 1               ; the blast or settle in hand
fire:      .res 1
mul:       .res 2               ; a settle's offset; scale's product
den:       .res 1

        .segment "BSS"
_ply_wait: .res 2
_ply_done: .res 1

        .segment "MIDBSS"
tabs:
b_on:   .res NBLAST
b_kind: .res NBLAST
b_n:    .res NBLAST
b_tot:  .res NBLAST
b_rx:   .res NBLAST
b_ry:   .res NBLAST
b_dn:   .res NBLAST             ; the size last drawn
b_xl:   .res NBLAST
b_xh:   .res NBLAST
b_yl:   .res NBLAST
b_yh:   .res NBLAST
s_on:   .res NSETTLE
s_left: .res NSETTLE
s_n:    .res NSETTLE
s_xl:   .res NSETTLE
s_xh:   .res NSETTLE
s_al:   .res NSETTLE            ; its columns at script + this
s_ah:   .res NSETTLE
tabs_end:

        .code

; pp = script + ply_pc
setp:   clc
        lda     #<_script
        adc     _ply_pc
        sta     pp
        lda     #>_script
        adc     _ply_pc+1
        sta     pp+1
        rts

; ply_pc += A
adv:    clc
        adc     _ply_pc
        sta     _ply_pc
        bcc     @x
        inc     _ply_pc+1
@x:     rts

_ply_reset:
        jsr     _pt_clear
        jsr     _trk_reset
        jsr     _ops_reset
        lda     #0
        sta     _ply_wait
        sta     _ply_wait+1
        ldx     #tabs_end-tabs
@c:     sta     tabs-1,x
        dex
        bne     @c
        rts

; ---- a frame --------------------------------------------------------------

_ply_step:
        lda     _ply_pc         ; (the first frame only runs ops)
        ora     _ply_pc+1
        beq     @ops
        jsr     anim
@ops:   lda     _ply_wait
        ora     _ply_wait+1
        beq     @run
        lda     _ply_wait       ; wait--
        bne     @d
        dec     _ply_wait+1
@d:     dec     _ply_wait
@run:   lda     _ply_wait
        ora     _ply_wait+1
        bne     @x
        lda     _ply_done
        bne     @x
        lda     _ply_pc         ; pc < seg_len?
        cmp     _seg_len
        lda     _ply_pc+1
        sbc     _seg_len+1
        bcs     @x
        jsr     op
        jmp     @run
@x:     rts

; ---- the ops ---------------------------------------------------------------

op:     jsr     setp
        ldy     #0
        lda     (pp),y
        pha
        lda     #1
        jsr     adv
        inc     pp
        bne     @p
        inc     pp+1
@p:     pla                     ; A: the op; pp: its arguments
        cmp     #OP_WAIT
        beq     o_wait
        cmp     #OP_END
        beq     o_end
        cmp     #OP_ARC
        beq     o_arc
        cmp     #OP_STOP
        beq     o_stop
        cmp     #OP_PATH
        beq     o_path
        cmp     #OP_FLOW
        beq     o_flow
        cmp     #OP_BEAM
        beq     o_beam
        cmp     #OP_EXPLODE
        beq     @bl
        cmp     #OP_DIRT
        bne     @n0
@bl:    jmp     o_blast
@n0:    cmp     #OP_SETTLE
        bne     @n1
        jmp     o_settle
@n1:    cmp     #OP_WIND
        beq     o_wind
        cmp     #OP_SHAKE
        beq     o_shake
        cmp     #OP_TALK        ; $30-$3A: C's
        bcc     o_end
        cmp     #OP_FIRE+1
        bcs     o_end
        jmp     _c_op           ; (fastcall: the op in A)

o_end:  lda     #1              ; OP_END, or not ours: this segment is done
        sta     _ply_done
        rts

o_wait: lda     (pp),y          ; n (0: 256)
        sta     _ply_wait
        ldx     #0
        cmp     #0
        bne     @w
        inx
@w:     stx     _ply_wait+1
        lda     #1
        jmp     adv

o_arc:  lda     pp
        ldx     pp+1
        jsr     _trk_load
        lda     #18
        jmp     adv

o_stop: lda     (pp),y
        jsr     _trk_stop
        lda     #1
        jmp     adv

o_path: lda     pp
        ldx     pp+1
        jsr     _ops_path
        jmp     adv

o_flow: lda     pp
        ldx     pp+1
        jsr     _ops_flow
        jmp     adv

o_beam: lda     pp
        ldx     pp+1
        jsr     _ops_beam
        jmp     adv

o_wind: lda     (pp),y
        sta     _st+OFF_WIND
        iny
        lda     (pp),y
        sta     _st+OFF_WIND+1
        lda     #2
        jmp     adv

o_shake:
        lda     (pp),y
        sta     _dsp_shake
        lda     #1
        jmp     adv

; EXPLODE: x16, y16, rx, ry, kind, frames; DIRT: the same without kind
o_blast:
        pha
        ldx     #0              ; the first free one (or the last)
@f:     lda     b_on,x
        beq     @got
        inx
        cpx     #NBLAST-1
        bne     @f
@got:   lda     (pp),y
        sta     b_xl,x
        iny
        lda     (pp),y
        sta     b_xh,x
        iny
        lda     (pp),y
        sta     b_yl,x
        iny
        lda     (pp),y
        sta     b_yh,x
        iny
        lda     (pp),y
        sta     b_rx,x
        iny
        lda     (pp),y
        sta     b_ry,x
        iny
        pla
        cmp     #OP_DIRT
        beq     @dirt
        lda     (pp),y
        iny
        bne     @k              ; (always)
@dirt:  lda     #DIRTBALL
@k:     sta     b_kind,x
        cmp     #EX_NUKE        ; a nuke shakes the screen
        bne     @nn
        lda     #30
        sta     _dsp_shake
@nn:    lda     (pp),y
        bne     @t
        lda     #1
@t:     sta     b_tot,x
        iny
        lda     #1
        sta     b_on,x
        lda     #0
        sta     b_n,x
        sta     b_dn,x
        tya
        jmp     adv

; SETTLE: x16, n, frames, n column entries
o_settle:
        ldx     #0
@f:     lda     s_on,x
        beq     @got
        inx
        cpx     #NSETTLE
        bne     @f
        ldx     #0              ; full: the oldest lands now
        jsr     settle
        jsr     setp
        ldy     #0
        ldx     #0
@got:   stx     bi
        lda     (pp),y
        sta     s_xl,x
        iny
        lda     (pp),y
        sta     s_xh,x
        iny
        lda     (pp),y
        sta     s_n,x
        iny
        lda     (pp),y
        sta     s_left,x
        lda     #4
        jsr     adv
        lda     _ply_pc         ; its columns start here
        sta     s_al,x
        lda     _ply_pc+1
        sta     s_ah,x
        lda     #1
        sta     s_on,x
        lda     s_n,x           ; past them
        sta     den
@skip:  lda     den
        beq     @now
        jsr     setp
        jsr     elen
        jsr     adv
        dec     den
        jmp     @skip
@now:   ldx     bi
        lda     s_left,x
        bne     @x
        jmp     settle
@x:     rts

; elen: the length of the column entry at pp (1, or 1 + 2 a span)
elen:   ldy     #0
        lda     (pp),y
        cmp     #T_SPANS+1
        bcc     @one
        sbc     #T_SPANS        ; (carry set)
        asl
        adc     #1
        rts
@one:   lda     #1
        rts

; settle X: its columns written
settle: stx     bi
        lda     s_al,x
        sta     mul
        lda     s_ah,x
        sta     mul+1
        lda     s_xl,x
        sta     _fx
        lda     s_xh,x
        sta     _fx+1
        lda     s_n,x
        sta     den
@c:     lda     den
        beq     @done
        clc
        lda     #<_script
        adc     mul
        sta     _fent
        sta     pp
        lda     #>_script
        adc     mul+1
        sta     _fent+1
        sta     pp+1
        jsr     _fld_col
        jsr     elen
        clc
        adc     mul
        sta     mul
        bcc     @i
        inc     mul+1
@i:     inc     _fx
        bne     @n
        inc     _fx+1
@n:     dec     den
        jmp     @c
@done:  ldx     bi
        lda     #0
        sta     s_on,x
        rts

; ---- a frame's step --------------------------------------------------------

anim:   jsr     _trk_step
        lda     #0
        sta     fire
        ldx     #NBLAST-1
@b:     lda     b_on,x
        beq     @nb
        inc     b_n,x
        lda     b_n,x
        cmp     b_tot,x
        bcs     @end
        lda     #1
        sta     fire
        bne     @nb
@end:   stx     bi              ; done: the crater (or the dirt) now
        lda     b_rx,x
        ldy     b_ry,x
        jsr     elargs
        lda     b_kind,x
        cmp     #DIRTBALL
        bne     @cr
        lda     #PX_DIRT
        ldy     #1
        bne     @go
@cr:    cmp     #EX_PLASMA
        beq     @fl
        cmp     #EX_PUFF
        beq     @fl
        lda     #PX_SKY
        ldy     #0
        beq     @go
@fl:    lda     #PX_SKY
        ldy     #2
@go:    sta     _fv
        sty     _emode
        jsr     _fld_ellipse
        ldx     bi
        lda     #0
        sta     b_on,x
@nb:    dex
        bpl     @b
        jsr     _ops_step
        ora     fire
        sta     fire
        ldx     #NSETTLE-1
@s:     lda     s_on,x
        beq     @ns
        dec     s_left,x
        bne     @ns
        jsr     settle
@ns:    dex
        bpl     @s
        jsr     _pt_step        ; sparks glow like fire
        ora     fire
        jsr     _field_fire
        jmp     _c_anim

; elargs: the ellipse at blast X, radii A, Y (0 is 1; ry at most 200)
elargs: cmp     #0
        bne     @a
        lda     #1
@a:     sta     _erx
        tya
        bne     @b
        lda     #1
@b:     cmp     #201
        bcc     @c
        lda     #200
@c:     sta     _ery
        lda     b_xl,x
        sta     _ex
        lda     b_xh,x
        sta     _ex+1
        lda     b_yl,x
        sta     _ey
        lda     b_yh,x
        sta     _ey+1
        rts

; ---- the blasts drawn at their size now -----------------------------------

_ply_draw:
        ldx     #NBLAST-1
@b:     lda     b_on,x
        beq     @nb
        lda     b_n,x
        cmp     b_dn,x
        beq     @nb
        sta     b_dn,x
        stx     bi
        lda     b_rx,x          ; rx n / total, ry n / total
        jsr     scale
        pha
        ldx     bi
        lda     b_ry,x
        jsr     scale
        tay
        pla
        ldx     bi
        jsr     elargs
        lda     b_kind,x
        cmp     #DIRTBALL
        bne     @fi
        lda     #PX_DIRT
        ldy     #1
        bne     @go
@fi:    ldy     #1              ; plasma, a puff: over the air only
        cmp     #EX_PLASMA
        beq     @f
        cmp     #EX_PUFF
        beq     @f
        dey                     ; a crater's fireball eats the dirt
@f:     lda     #PX_FX
@go:    sta     _fv
        sty     _emode
        jsr     _fld_ellipse
        ldx     bi
@nb:    dex
        bpl     @b
        rts

; scale: A * n / total of blast bi (at most A: n <= total)
scale:  ldx     bi
        sta     den             ; A * n: shift and add, into A:mul
        lda     b_n,x
        sta     mul
        lda     #0
        ldy     #8
        lsr     mul
@m:     bcc     @na
        clc
        adc     den
@na:    ror
        ror     mul
        dey
        bne     @m
        ldy     b_tot,x         ; / total (A, the high byte, is below it)
        sty     den
        ldy     #8
@d:     asl     mul
        rol
        bcs     @sub
        cmp     den
        bcc     @dn
@sub:   sbc     den
        inc     mul
@dn:    dey
        bne     @d
        lda     mul
        rts

; ---- a segment's end -----------------------------------------------------

_ply_end:
        jsr     _pt_clear
        ldx     #NSETTLE-1
@s:     lda     s_on,x
        beq     @n
        jsr     settle
@n:     dex
        bpl     @s
        jmp     _ops_seg_end
