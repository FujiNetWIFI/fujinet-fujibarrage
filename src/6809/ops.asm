*
* ops.asm -- the script's paths (OP_PATH), flows (OP_FLOW) and beams
* (OP_BEAM) on the field (docs/PROTOCOL.md), the 6502's src/6502/ops.s in
* 6809 with 16-bit coordinates (the CoCo 3's field is 320 wide). Paths and
* flows read their bytes where they lie in script[]; at a segment's end
* they finish (ops_seg_end).
*
* ops_path, ops_flow, ops_beam   X: the op's arguments; D: the bytes taken
* ops_step                       a frame: A non-zero if a fire flow burns
* ops_seg_end                    paths stop, flows run out
* ops_reset                      all off (a new shot)
* C: ops_beams_off()             beams gone (the script's end)
*
        section plycode

        include "dp.inc"
        include "proto.inc"

        export  ops_path,ops_flow,ops_beam,ops_step,ops_seg_end,ops_reset
        export  _ops_beams_off
        import  fld_swap,fld_get,fld_plot,fld_release

NPATH   equ     4
NFLOW   equ     4
NBEAM   equ     2
PX_SKY  equ     1
PX_DIRT equ     2
PX_FX   equ     3

* a path
PA_ON    equ     0
PA_STY   equ     1
PA_X     equ     2               ; 2
PA_Y     equ     4               ; 2
PA_N     equ     6               ; steps left
PA_DR    equ     7               ; its head is drawn
PA_P     equ     8               ; 2: its next step
PA_SIZE  equ     10
* a flow
F_ON    equ     0
F_KIND  equ     1
F_FRM   equ     2               ; frames to spread over
F_F     equ     3               ; frames so far
F_HOLD  equ     4
F_N     equ     5
F_SH    equ     6               ; cells shown
F_ACC   equ     7               ; 2: n x frames so far, less frames x shown
F_P     equ     9               ; 2: its cells (x16, y: 3 bytes each)
F_SIZE  equ     11
* a beam
BE_LEFT equ     0
BE_X0   equ     1               ; 2
BE_Y0   equ     3
BE_X1   equ     5
BE_Y1   equ     7
BE_SIZE equ     9

_ops_beams_off  PENTRY  ops_beams_off

* ---- paths ---------------------------------------------------------------------

* phead: path U's head, drawn: a roller over air, a digger over air or dirt
phead   clr     PA_DR,u
        ldd     PA_X,u
        std     <fx
        ldd     PA_Y,u
        std     <fy
        pshs    u
        lbsr    fld_get
        puls    u
        cmpb    #PX_SKY
        beq     @draw
        cmpb    #PX_DIRT
        bne     @x
        lda     PA_STY,u
        cmpa    #ST_DIGGER
        bne     @x
@draw   lda     #PX_FX
        sta     <fv
        pshs    u
        lbsr    fld_plot
        puls    u
        inc     PA_DR,u
@x      rts

* pleave: path U's head moves on (a digger has bored: air behind it)
pleave  tst     PA_DR,u
        beq     @x
        clr     PA_DR,u
        ldd     PA_X,u
        std     <fx
        ldd     PA_Y,u
        std     <fy
        lda     #PX_FX
        sta     <fold
        lda     #PX_SKY
        sta     <fv
        pshs    u
        lbsr    fld_swap
        puls    u
@x      rts

* pend: path U stops
pend    tst     PA_ON,u
        beq     @x
        bsr     pleave
        clr     PA_ON,u
@x      rts

ops_path
        stx     <optr
        ldb     ,x              ; slot
        andb    #NPATH-1
        lda     #PA_SIZE
        mul
        addd    #paths
        tfr     d,u
        bsr     pend
        ldx     <optr
        lda     1,x
        sta     PA_STY,u
        ldd     2,x
        exg     a,b
        std     PA_X,u
        ldd     4,x
        exg     a,b
        std     PA_Y,u
        lda     6,x
        sta     PA_N,u
        leax    7,x
        stx     PA_P,u
        lda     #1
        sta     PA_ON,u
 lbsr    phead
        clra
        ldb     PA_N,u
        addd    #7
        rts

pstep   ldu     #paths+(NPATH-1)*PA_SIZE
        lda     #NPATH
        sta     <oi
@l      tst     PA_ON,u
        beq     @n
        tst     PA_N,u
        bne     @mv
        bsr     pend
        bra     @n
@mv     bsr     pleave
        dec     PA_N,u
        ldx     PA_P,u
        lda     ,x+
        stx     PA_P,u
        sta     <otmp
        asra                    ; x += the high nibble, signed
        asra
        asra
        asra
        tfr     a,b
        sex
        addd    PA_X,u
        std     PA_X,u
        ldb     <otmp           ; y += the low nibble, signed
        lslb
        lslb
        lslb
        lslb
        asrb
        asrb
        asrb
        asrb
        sex
        addd    PA_Y,u
        std     PA_Y,u
 lbsr    phead
@n      leau    -PA_SIZE,u
        dec     <oi
        bne     @l
        rts

* ---- flows -------------------------------------------------------------------------

* fcells: flow orec's cells ocnt to oto-1, fold -> fv (and, mud, fire -> fv)
fcells  ldu     <orec
@l      lda     <ocnt
        cmpa    <oto
        bhs     @x
        ldb     #3
        mul
        addd    F_P,u
        tfr     d,x
        ldd     ,x
        exg     a,b
        std     <fx
        clra
        ldb     2,x
        std     <fy
        lbsr    fld_swap
        tst     <omud
        beq     @n
        lda     <fold
        pshs    a
        lda     #PX_FX
        sta     <fold
        lbsr    fld_swap
        puls    a
        sta     <fold
@n      ldu     <orec
        inc     <ocnt
        bne     @l
@x      rts

* fshow: flow U's first A cells in (fire over the air, mud as dirt)
fshow   stu     <orec
        cmpa    F_SH,u
        bls     @x
        sta     <oto
        ldb     F_SH,u
        stb     <ocnt
        sta     F_SH,u
        lda     #PX_SKY
        sta     <fold
        clr     <omud
        lda     #PX_FX
        ldb     F_KIND,u
        cmpb    #FLOW_MUD
        bne     @f
        inc     <omud
        lda     #PX_DIRT
@f      sta     <fv
        bsr     fcells
@x      rts

* fend: flow U all in, then a fire goes out
fend    stu     <orec
        lda     F_N,u
        lbsr    fshow
        ldu     <orec
        lda     F_KIND,u
        cmpa    #FLOW_MUD
        beq     @off
        clr     <ocnt
        clr     <omud
        lda     F_N,u
        sta     <oto
        lda     #PX_FX
        sta     <fold
        lda     #PX_SKY
        sta     <fv
        bsr     fcells
        ldu     <orec
@off    clr     F_ON,u
        rts

ops_flow
        stx     <optr
        ldu     #flows          ; the first free one,
        ldb     #NFLOW-1
@free   tst     F_ON,u
        beq     @got
        leau    F_SIZE,u
        decb
        bne     @free
        tst     F_ON,u
        beq     @got
        bsr     fend            ;  or the last, ended now
        ldu     <orec
@got    ldx     <optr
        lda     ,x
        sta     F_KIND,u
        lda     1,x
        bne     @fr
        inca
@fr     sta     F_FRM,u
        lda     2,x
        sta     F_HOLD,u
        lda     3,x
        sta     F_N,u
        clr     F_F,u
        clr     F_SH,u
        clr     F_ACC,u
        clr     F_ACC+1,u
        leax    4,x
        stx     F_P,u
        lda     #1
        sta     F_ON,u
        lda     F_N,u           ; took 4 + 3n
        ldb     #3
        mul
        addd    #4
        rts

fstep   ldu     #flows+(NFLOW-1)*F_SIZE
        lda     #NFLOW
        sta     <oi
@l      tst     F_ON,u
        beq     @n
        lda     F_KIND,u
        cmpa    #FLOW_MUD
        beq     @m
        lda     #1
        sta     <ofire
@m      lda     F_F,u
        cmpa    F_FRM,u
        bhs     @hold
        inc     F_F,u
        clra                    ; acc += n; each frames of it: a cell more
        ldb     F_N,u
        addd    F_ACC,u
        std     F_ACC,u
        lda     F_SH,u
        sta     <ocnt
@sub    ldd     F_ACC,u
        subb    F_FRM,u
        sbca    #0
        bcs     @sh
        std     F_ACC,u
        inc     <ocnt
        bne     @sub
@sh     lda     <ocnt
        pshs    u
        lbsr    fshow
        puls    u
        bra     @end
@hold   tst     F_HOLD,u
        beq     @end
        dec     F_HOLD,u
@end    lda     F_F,u
        cmpa    F_FRM,u
        blo     @n
        tst     F_HOLD,u
        bne     @n
        pshs    u
        lbsr    fend
        puls    u
@n      leau    -F_SIZE,u
        dec     <oi
        bne     @l
        rts

* ---- beams ---------------------------------------------------------------------------

* lplot: (lx0, ly0): on, fire over air and dirt; off, fire back to air
lplot   ldd     <lx0
        std     <fx
        ldd     <ly0
        std     <fy
        tst     <lon
        beq     @off
        lda     #PX_FX
        sta     <fv
        lda     #PX_SKY
        sta     <fold
        lbsr    fld_swap
        lda     #PX_DIRT
        sta     <fold
        lbra    fld_swap
@off    lda     #PX_FX
        sta     <fold
        lda     #PX_SKY
        sta     <fv
        lbra    fld_swap

* bline: beam U's line (Bresenham, as the reference view draws it)
bline   stu     <orec
        ldd     BE_X0,u
        std     <lx0
        ldd     BE_Y0,u
        std     <ly0
        ldd     BE_X1,u
        std     <lx1
        ldd     BE_Y1,u
        std     <ly1
        ldx     #1
        ldd     <lx1
        subd    <lx0
        bpl     @px
        coma
        comb
        addd    #1
        ldx     #-1
@px     std     <ldx_
        stx     <lsx
        ldx     #1
        ldd     <ly1
        subd    <ly0
        bpl     @py
        coma
        comb
        addd    #1
        ldx     #-1
@py     std     <ldy_
        stx     <lsy
        ldd     <ldx_           ; err = dx - dy
        subd    <ldy_
        std     <lerr
@l      bsr     lplot
        ldd     <lx0
        cmpd    <lx1
        bne     @go
        ldd     <ly0
        cmpd    <ly1
        beq     @x
@go     ldd     <lerr           ; e2 = 2 err
        lslb
        rola
        std     <le2
        addd    <ldy_           ; e2 > -dy: x moves
        ble     @ny
        ldd     <lerr
        subd    <ldy_
        std     <lerr
        ldd     <lx0
        addd    <lsx
        std     <lx0
@ny     ldd     <le2            ; e2 < dx: y moves
        cmpd    <ldx_
        bge     @l
        ldd     <lerr
        addd    <ldx_
        std     <lerr
        ldd     <ly0
        addd    <lsy
        std     <ly0
        bra     @l
@x      ldu     <orec
        rts

* boff: beam U's pixels back to air, and it is gone
boff    tst     BE_LEFT,u
        beq     @x
        clr     BE_LEFT,u
        clr     <lon
        lbra    bline
@x      rts

ops_beam
        stx     <optr
        ldu     #beams          ; the first, unless it's live
        tst     BE_LEFT,u
        beq     @s
        leau    BE_SIZE,u
@s      bsr     boff
        ldx     <optr
        ldd     ,x
        exg     a,b
        std     BE_X0,u
        ldd     2,x
        exg     a,b
        std     BE_Y0,u
        ldd     4,x
        exg     a,b
        std     BE_X1,u
        ldd     6,x
        exg     a,b
        std     BE_Y1,u
        lda     9,x
        bne     @f
        inca
@f      sta     BE_LEFT,u
        lda     #1
        sta     <lon
        lbsr    bline
        ldd     #10
        rts

bstep   ldu     #beams+(NBEAM-1)*BE_SIZE
        lda     #NBEAM
        sta     <oi
@l      tst     BE_LEFT,u
        beq     @n
        dec     BE_LEFT,u
        bne     @n
        inc     BE_LEFT,u       ; boff wants it live
        bsr     boff
@n      leau    -BE_SIZE,u
        dec     <oi
        bne     @l
        rts

* ---- a frame, a segment's end, a new shot --------------------------------------------

ops_step
        clr     <ofire
        lbsr    pstep
        lbsr    fstep
        bsr     bstep
        lda     <ofire
        rts

ops_seg_end
        ldu     #paths+(NPATH-1)*PA_SIZE
        lda     #NPATH
        sta     <oi
@p      lbsr    pend
        leau    -PA_SIZE,u
        dec     <oi
        bne     @p
        ldu     #flows+(NFLOW-1)*F_SIZE
        lda     #NFLOW
        sta     <oi
@f      tst     F_ON,u
        beq     @n
        pshs    u
        lbsr    fend
        puls    u
@n      leau    -F_SIZE,u
        dec     <oi
        bne     @f
        rts

ops_beams_off
        ldu     #beams+(NBEAM-1)*BE_SIZE
        lda     #NBEAM
        sta     <oi
@l      lbsr    boff
        leau    -BE_SIZE,u
        dec     <oi
        bne     @l
        rts

ops_reset
        ldx     #data
@l      clr     ,x+
        cmpx    #data_end
        blo     @l
        rts

        section bss,bss
data
paths   rmb     NPATH*PA_SIZE
flows   rmb     NFLOW*F_SIZE
beams   rmb     NBEAM*BE_SIZE
data_end
