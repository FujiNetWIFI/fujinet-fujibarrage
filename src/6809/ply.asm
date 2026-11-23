*
* ply.asm -- the script player's timeline (docs/PROTOCOL.md "Playing a
* segment"), the 6502's src/6502/ply.s in 6809: each frame every track,
* blast, path, flow, beam and settle moves a step, then the ops due run.
* The ops about tanks, talk, damage and sound go to C (c_op in play.c),
* which reads their arguments at ply_pc. Coordinates are 16-bit (the CoCo
* 3's field is 320 wide); the script's words are little-endian.
*
* void ply_reset(void)   a new shot: nothing moving
* void ply_step(void)    a frame
* void ply_draw(void)    the blasts at their size now
* void ply_end(void)     the segment's bytes are going: settles land, paths
*                        and flows finish (ops.asm)
* ply_pc, ply_wait, ply_done: where it is in script[], frames to wait, OP_END
*
* The field engine (src/coco2/fld2.asm, src/coco3/fld3.asm) is called at
* its internal entries, DP = $0E throughout (dp.inc).
*
        section plycode

        include "dp.inc"
        include "proto.inc"

        export  _ply_reset,_ply_step,_ply_draw,_ply_end
        export  _ply_pc,_ply_wait,_ply_done
        import  _script,_seg_len,_st,_c_op,_c_anim,_field_fire,_shake_n
        import  trk_reset,trk_load,trk_stop,trk_step
        import  ops_path,ops_flow,ops_beam,ops_step,ops_seg_end,ops_reset
        import  pt_step,pt_clear
        import  fld_ellipse,fld_cols,fld_release

PX_SKY  equ     1               ; (field.h)
PX_DIRT equ     2
PX_FX   equ     3
NBLAST  equ     10
NSETTLE equ     8
DIRTBALL equ    $80             ; an OP_DIRT, kept with the blasts

* a blast
B_ON    equ     0
B_KIND  equ     1
B_N     equ     2
B_TOT   equ     3
B_RX    equ     4
B_RY    equ     5
B_DN    equ     6               ; the size last drawn
B_X     equ     7               ; 2
B_Y     equ     9               ; 2
B_SIZE  equ     11
* a settle
S_ON    equ     0
S_LEFT  equ     1
S_N     equ     2
S_X     equ     3               ; 2
S_A     equ     5               ; 2: its columns at script + this
S_SIZE  equ     7

_ply_reset      PENTRY  ply_reset
_ply_step       PENTRY  ply_step
_ply_draw       PENTRY  ply_draw
_ply_end        PENTRY  ply_end

* pp = script + ply_pc
setp    ldd     _ply_pc
        addd    #_script
        std     <pp
        rts

* ply_pc += D
adv     addd    _ply_pc
        std     _ply_pc
        rts

ply_reset
        lbsr    pt_clear
        lbsr    trk_reset
        lbsr    ops_reset
        clra
        clrb
        std     _ply_wait
        ldx     #tabs
@c      sta     ,x+
        cmpx    #tabs_end
        blo     @c
        inca
        sta     <pfirst
        rts

* ---- a frame -----------------------------------------------------------------

* (the shot's first frame only runs its ops: view.go's Frame 0)
ply_step
        tst     <pfirst
        bne     @ops
        lbsr    anim
@ops    clr     <pfirst
        ldd     _ply_wait
        beq     @run
        subd    #1
        std     _ply_wait
@run    ldd     _ply_wait
        bne     @x
        tst     _ply_done
        bne     @x
        ldd     _ply_pc
        cmpd    _seg_len
        bhs     @x
        bsr     op
        bra     @run
@x      rts

* ---- the ops --------------------------------------------------------------------

op      bsr     setp
        ldx     <pp
        lda     ,x+
        stx     <pp
        pshs    a
        ldd     #1
        bsr     adv
        puls    a               ; A: the op; X, pp: its arguments
        cmpa    #OP_WAIT
        beq     o_wait
        cmpa    #OP_END
        beq     o_end
        cmpa    #OP_ARC
        beq     o_arc
        cmpa    #OP_STOP
        beq     o_stop
        cmpa    #OP_PATH
        beq     o_path
        cmpa    #OP_FLOW
        beq     o_flow
        cmpa    #OP_BEAM
        beq     o_beam
        cmpa    #OP_EXPLODE
        lbeq    o_blast
        cmpa    #OP_DIRT
        lbeq    o_blast
        cmpa    #OP_SETTLE
        lbeq    o_settle
        cmpa    #OP_WIND
        beq     o_wind
        cmpa    #OP_SHAKE
        beq     o_shake
        cmpa    #OP_TALK        ; $30-$3A: C's
        blo     o_end
        cmpa    #OP_FIRE+1
        bhs     o_end
        tfr     a,b             ; c_op(op)
        clra
        pshs    d
        CCALL   _c_op
        leas    2,s
        rts

o_end   lda     #1              ; OP_END, or not ours: this segment is done
        sta     _ply_done
        rts

o_wait  clra                    ; n (0: 256)
        ldb     ,x
        bne     @w
        inca
@w      std     _ply_wait
        ldd     #1
        lbra    adv

o_arc   lbsr    trk_load
        ldd     #18
        lbra    adv

o_stop  ldb     ,x
        lbsr    trk_stop
        ldd     #1
        lbra    adv

o_path  lbsr    ops_path        ; (each says how many bytes it took)
        lbra    adv

o_flow  lbsr    ops_flow
        lbra    adv

o_beam  lbsr    ops_beam
        lbra    adv

o_wind  ldd     ,x              ; (the state's bytes as they come: little-endian)
        std     _st+OFF_WIND
        ldd     #2
        lbra    adv

o_shake lda     ,x
        sta     _shake_n
        ldd     #1
        lbra    adv

* EXPLODE: x16, y16, rx, ry, kind, frames; DIRT: the same without kind
o_blast pshs    a
        ldu     #blasts         ; the first free one (or the last)
        ldb     #NBLAST-1
@f      tst     B_ON,u
        beq     @got
        leau    B_SIZE,u
        decb
        bne     @f
@got    ldd     ,x
        exg     a,b
        std     B_X,u
        ldd     2,x
        exg     a,b
        std     B_Y,u
        lda     4,x
        sta     B_RX,u
        lda     5,x
        sta     B_RY,u
        leax    6,x
        puls    a
        cmpa    #OP_DIRT
        beq     @dirt
        lda     ,x+
        bra     @k
@dirt   lda     #DIRTBALL
@k      sta     B_KIND,u
        cmpa    #EX_NUKE        ; a nuke shakes the screen
        bne     @nn
        ldb     #30
        stb     _shake_n
@nn     lda     ,x+
        bne     @t
        inca
@t      sta     B_TOT,u
        lda     #1
        sta     B_ON,u
        clr     B_N,u
        clr     B_DN,u
        tfr     x,d
        subd    <pp
        lbra    adv

* SETTLE: x16, n, frames, n column entries
o_settle
        ldu     #settles
        ldb     #NSETTLE
@f      tst     S_ON,u
        beq     @got
        leau    S_SIZE,u
        decb
        bne     @f
        ldu     #settles        ; full: the oldest lands now
        lbsr    settle
        lbsr    setp
        ldu     #settles
@got    stu     <bp
        ldx     <pp
        ldd     ,x
        exg     a,b
        std     S_X,u
        lda     2,x
        sta     S_N,u
        lda     3,x
        sta     S_LEFT,u
        ldd     #4
        lbsr    adv
        ldu     <bp
        ldd     _ply_pc         ; its columns start here
        std     S_A,u
        lda     #1
        sta     S_ON,u
        lda     S_N,u           ; past them
        sta     <den
@skip   tst     <den
        beq     @now
        lbsr    setp
        ldx     <pp
        bsr     elen
        clra
        lbsr    adv
        dec     <den
        bra     @skip
@now    ldu     <bp
        tst     S_LEFT,u
        bne     @x
        bra     settle
@x      rts

* elen: B = the length of the column entry at X (1, or 1 + 2 a span)
elen    ldb     ,x
        cmpb    #T_SPANS+1
        blo     @one
        subb    #T_SPANS
        lslb
        incb
        rts
@one    ldb     #1
        rts

* settle U: its columns written (a byte's at once: the engines' fld_cols)
settle  stu     <bp
        ldd     S_A,u
        addd    #_script
        std     <fent
        ldd     S_X,u
        std     <fx
        ldb     S_N,u
        lbsr    fld_cols
        ldu     <bp
        clr     S_ON,u
        rts

* ---- a frame's step ---------------------------------------------------------------

anim    lbsr    trk_step
        clr     <fire
        ldu     #blasts+(NBLAST-1)*B_SIZE
        lda     #NBLAST
        sta     <bi
@b      tst     B_ON,u
        beq     @nb
        inc     B_N,u
        lda     B_N,u
        cmpa    B_TOT,u
        bhs     @end
        lda     #1
        sta     <fire
        bra     @nb
@end    stu     <bp             ; done: the crater (or the dirt) now
        lda     B_RX,u
        ldb     B_RY,u
        lbsr    elargs
        lda     B_KIND,u
        cmpa    #DIRTBALL
        bne     @cr
        lda     #PX_DIRT
        ldb     #1
        bra     @go
@cr     cmpa    #EX_PLASMA
        beq     @fl
        cmpa    #EX_PUFF
        beq     @fl
        lda     #PX_SKY
        clrb
        bra     @go
@fl     lda     #PX_SKY
        ldb     #2
@go     sta     <fv
        stb     <emode
        lbsr    fld_ellipse
        ldu     <bp
        clr     B_ON,u
@nb     leau    -B_SIZE,u
        dec     <bi
        bne     @b
        lbsr    ops_step        ; (A: a fire flow burns)
        ora     <fire
        sta     <fire
        ldu     #settles+(NSETTLE-1)*S_SIZE
        lda     #NSETTLE
        sta     <bi
@s      tst     S_ON,u
        beq     @ns
        dec     S_LEFT,u
        bne     @ns
        lbsr    settle
        ldu     <bp
@ns     leau    -S_SIZE,u
        dec     <bi
        bne     @s
        lbsr    pt_step         ; sparks glow like fire
        ora     <fire
        tfr     a,b
        clra
        pshs    d
        CCALL   _field_fire
        leas    2,s
        CCALL   _c_anim
        rts

* elargs: the ellipse at blast U, radii A, B (0 is 1; ry at most 200)
elargs  tsta
        bne     @a
        inca
@a      sta     <erx
        tstb
        bne     @b
        incb
@b      cmpb    #201
        blo     @c
        ldb     #200
@c      stb     <ery
        ldd     B_X,u
        std     <ex
        ldd     B_Y,u
        std     <ey
        rts

* ---- the blasts drawn at their size now -------------------------------------------

ply_draw
        ldu     #blasts+(NBLAST-1)*B_SIZE
        lda     #NBLAST
        sta     <bi
@b      tst     B_ON,u
        beq     @nb
        lda     B_N,u
        cmpa    B_DN,u
        beq     @nb
        sta     B_DN,u
        stu     <bp
        lda     B_RX,u          ; rx n / total, ry n / total
        bsr     scale
        pshs    a
        lda     B_RY,u
        bsr     scale
        tfr     a,b
        puls    a
        bsr     elargs
        lda     B_KIND,u
        cmpa    #DIRTBALL
        bne     @fi
        lda     #PX_DIRT
        ldb     #1
        bra     @go
@fi     ldb     #1              ; plasma, a puff: over the air only
        cmpa    #EX_PLASMA
        beq     @f
        cmpa    #EX_PUFF
        beq     @f
        clrb                    ; a crater's fireball eats the dirt
@f      lda     #PX_FX
@go     sta     <fv
        stb     <emode
        lbsr    fld_ellipse
        ldu     <bp
@nb     leau    -B_SIZE,u
        dec     <bi
        bne     @b
        rts

* scale: A = A * n / total of blast U (at most A: n <= total)
scale   ldb     B_N,u
        mul
        std     <mul
        lda     B_TOT,u
        sta     <den
        lda     #8
        sta     <scnt
        lda     <mul            ; (the high byte is below the divisor)
@d      lsl     <mul+1
        rola
        bcs     @sub
        cmpa    <den
        blo     @dn
@sub    suba    <den
        inc     <mul+1
@dn     dec     <scnt
        bne     @d
        lda     <mul+1
        rts

* ---- a segment's end ---------------------------------------------------------------

ply_end lbsr    pt_clear
        ldu     #settles+(NSETTLE-1)*S_SIZE
        lda     #NSETTLE
        sta     <bi
@s      tst     S_ON,u
        beq     @n
        lbsr    settle
        ldu     <bp
@n      leau    -S_SIZE,u
        dec     <bi
        bne     @s
        lbra    ops_seg_end

        section bss,bss
_ply_pc         rmb     2
_ply_wait       rmb     2
_ply_done       rmb     1
tabs
blasts          rmb     NBLAST*B_SIZE
settles         rmb     NSETTLE*S_SIZE
tabs_end
