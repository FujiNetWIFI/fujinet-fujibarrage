*
* trk.asm -- the script's tracks (OP_ARC), the 6502's src/6502/trk.s in
* 6809: sixteen slots, each a moving head: position and velocity 12.12
* fixed point (24 bits), acceleration 4.12 (16). Each step
*
*       X += VX;  Y += VY;  VX += AX;  VY += AY
*
* and the head is drawn at (X >> 12, Y >> 12) -- the server's fitter runs
* the very same integers. A head is drawn (fire) only over sky (a smoke
* trail's over fire too), and the pixel under it is put back when it moves;
* a smoke trail's stays.
*
* trk_load (X: an OP_ARC's arguments), trk_step, trk_stop (B: the slot),
* trk_reset: the player's (DP = $0E). C: trk_stop(slot), trk_fall().
*
        section plycode

        include "dp.inc"
        include "proto.inc"

        export  trk_load,trk_step,trk_stop,trk_reset
        export  _trk_stop,_trk_fall
        import  fld_plot,fld_get,fld_release

N       equ     16
PX_SKY  equ     1
PX_FX   equ     3

* a track (the 24-bit values high byte first)
T_ON    equ     0
T_STY   equ     1
T_X     equ     2               ; 3
T_Y     equ     5               ; 3
T_VX    equ     8               ; 3
T_VY    equ     11              ; 3
T_AX    equ     14              ; 2
T_AY    equ     16              ; 2
T_N     equ     18              ; 2: steps left
T_PX    equ     20              ; 2: where its head is
T_PY    equ     22              ; 2
T_UND   equ     24              ; what was under it
T_DRAWN equ     25
T_SIZE  equ     26

* void trk_stop(uint8_t s)  (after pshs u,y,dp: the argument's byte at 8,s)
_trk_stop
        pshs    u,y,dp
        lda     #DPAGE
        tfr     a,dp
        lda     #$FF
        sta     <curblk
        ldb     8,s
        bsr     trk_stop
        lbsr    fld_release
        puls    u,y,dp,pc

* uint8_t trk_fall(void): the lowest falling shell's line | 1, or 0
_trk_fall
        ldb     DPBASE+tfall
        rts

* the track in slot B (& 15): U
slot    andb    #N-1
        lda     #T_SIZE
        mul
        addd    #tracks
        tfr     d,u
        rts

trk_reset
        ldx     #tracks
@l      clr     ,x+
        cmpx    #tracks+N*T_SIZE
        blo     @l
        clr     <tfall
        rts

* trk_stop: the track in slot B ends now
trk_stop
        bsr     slot
        lbsr    off
        clr     T_ON,u
        rts

* trk_load: an OP_ARC's arguments at X (slot, style, x16, y16, vx24, vy24,
* ax16, ay16, n16: little-endian) start a track
trk_load
        ldb     ,x
        bsr     slot
        pshs    x
        lbsr    off             ; its old head away
        puls    x
        lda     1,x
        sta     T_STY,u
        lda     3,x             ; X = x16 << 8
        sta     T_X,u
        lda     2,x
        sta     T_X+1,u
        clr     T_X+2,u
        lda     5,x
        sta     T_Y,u
        lda     4,x
        sta     T_Y+1,u
        clr     T_Y+2,u
        lda     8,x
        sta     T_VX,u
        lda     7,x
        sta     T_VX+1,u
        lda     6,x
        sta     T_VX+2,u
        lda     11,x
        sta     T_VY,u
        lda     10,x
        sta     T_VY+1,u
        lda     9,x
        sta     T_VY+2,u
        lda     13,x
        sta     T_AX,u
        lda     12,x
        sta     T_AX+1,u
        lda     15,x
        sta     T_AY,u
        lda     14,x
        sta     T_AY+1,u
        lda     17,x
        sta     T_N,u
        lda     16,x
        sta     T_N+1,u
        lda     #1
        sta     T_ON,u
        lbra    on

* trk_step: every live track one step
trk_step
        clr     <tfall
        ldu     #tracks+(N-1)*T_SIZE
        lda     #N
        sta     <tslot
@t      tst     T_ON,u
        lbeq    @n
        stu     <tp
        lbsr    off
        ldu     <tp
        ldd     T_N,u           ; no steps left: it ends
        bne     @go
        clr     T_ON,u
        lbra    @n
@go     subd    #1
        std     T_N,u
        ldd     T_X+1,u         ; X += VX
        addd    T_VX+1,u
        std     T_X+1,u
        lda     T_X,u
        adca    T_VX,u
        sta     T_X,u
        ldd     T_Y+1,u         ; Y += VY
        addd    T_VY+1,u
        std     T_Y+1,u
        lda     T_Y,u
        adca    T_VY,u
        sta     T_Y,u
        ldd     T_VX+1,u        ; VX += AX (sign-extended; tst keeps the carry)
        addd    T_AX,u
        std     T_VX+1,u
        lda     T_VX,u
        tst     T_AX,u
        bmi     @axn
        adca    #0
        bra     @axs
@axn    adca    #$FF
@axs    sta     T_VX,u
        ldd     T_VY+1,u        ; VY += AY
        addd    T_AY,u
        std     T_VY+1,u
        lda     T_VY,u
        tst     T_AY,u
        bmi     @ayn
        adca    #0
        bra     @ays
@ayn    adca    #$FF
@ays    sta     T_VY,u
        lbsr    on
        ldu     <tp
        tst     T_VY,u          ; falling (VY >= 0), on the field, not a digger:
        bmi     @n              ; the lowest is the whistle's
        tst     T_PY,u
        bne     @n
        lda     T_STY,u
        anda    #15
        cmpa    #ST_DIGGER
        beq     @n
        lda     T_PY+1,u
        cmpa    <tfall
        blo     @n
        ora     #1
        sta     <tfall
@n      leau    -T_SIZE,u
        dec     <tslot
        lbne    @t
        rts

* off: the head of track U off (its pixel back, unless a trail or overdrawn)
off     tst     T_DRAWN,u
        beq     @x
        clr     T_DRAWN,u
        lda     T_STY,u
        anda    #15
        cmpa    #ST_SMOKE
        beq     @x
        ldd     T_PX,u
        std     <fx
        ldd     T_PY,u
        std     <fy
        pshs    u
        lbsr    fld_get
        puls    u
        cmpb    #PX_FX
        bne     @x
        lda     T_UND,u
        sta     <fv
        pshs    u
        lbsr    fld_plot
        puls    u
@x      rts

* on: the head of track U on, at (X >> 12, Y >> 12)
on      ldd     T_X,u           ; (the top 16 bits) >> 4, arithmetic
        asra
        rorb
        asra
        rorb
        asra
        rorb
        asra
        rorb
        std     T_PX,u
        std     <fx
        ldd     T_Y,u
        asra
        rorb
        asra
        rorb
        asra
        rorb
        asra
        rorb
        std     T_PY,u
        std     <fy
        pshs    u
        lbsr    fld_get
        puls    u
        cmpb    #PX_SKY
        beq     @draw
        cmpb    #PX_FX          ; a trail's over fire too (refview's drawHead:
        bne     @x              ;  never over dirt or a tank)
        lda     T_STY,u
        anda    #15
        cmpa    #ST_SMOKE
        bne     @x
@draw   stb     T_UND,u
        lda     #1
        sta     T_DRAWN,u
        lda     #PX_FX
        sta     <fv
        pshs    u
        lbsr    fld_plot
        puls    u
@x      rts

        section bss,bss
tracks  rmb     N*T_SIZE
