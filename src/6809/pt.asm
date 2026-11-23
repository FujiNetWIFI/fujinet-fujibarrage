*
* pt.asm -- sparks: pixels flying from a tank going up, or off its shield
* (the Apple's src/apple2/pt.s in 6809). Each is a fire pixel on the sky,
* moved every frame (x 16.8, y 8.8 fixed point) and put back to sky when it
* moves on or dies. It only ever draws on sky, so it changes nothing the
* server knows (refview never sees them).
*
* C: field_sparks(uint16_t a)   low byte the seat, high the kind: 0-7 a
*                               death's effect (DEATH_*), 8 + a shield's
*                               event (SH_*)
* pt_step   a frame (ply.asm's anim): A nonzero while any burn (they glow
*           in the fire colours)
* pt_clear  all gone (a segment's end, a shot's start)
*
        section plycode

        include "dp.inc"

        export  _field_sparks,pt_step,pt_clear
        import  _tk,fld_get,fld_plot,fld_swap,fld_release,fld_w,fld_h,fld_mid

NP      equ     16
PX_SKY  equ     1
PX_FX   equ     3

* a spark
SP_X    equ     0               ; 3: x 16.8
SP_Y    equ     3               ; 2: y 8.8
SP_VX   equ     5               ; 2: 8.8, signed
SP_VY   equ     7               ; 2
SP_LIFE equ     9
SP_GRAV equ     10
SP_SH   equ     11              ; it shows, at
SP_OX   equ     12              ; 2
SP_OY   equ     14
SP_SIZE equ     15

* a kind's parameters (par, 9 bytes each)
P_N     equ     0               ; how many
P_FIRST equ     1               ; the first direction (0 right, 4 down, 12 up)
P_STEP  equ     2               ; 1: one direction after another
P_MASK  equ     3               ; a random spread added
P_K     equ     4               ; frames moved before it shows (at 2 px a frame)
P_SH    equ     5               ; then its speed halved this often
P_SJ    equ     6               ; and once more at random (1) or not (0)
P_G     equ     7               ; gravity, 1/256 px a frame per frame
P_LIFE  equ     8               ; frames (plus 0-7 at random)

cos     fcb     32,30,23,12,0,-12,-23,-30
        fcb     -32,-30,-23,-12,0,12,23,30
RAYS    equ     0*9
SPRAY   equ     1*9
PLUME   equ     2*9
SCATTER equ     3*9
DEBRIS  equ     4*9
RING    equ     5*9
BURST   equ     6*9
kmap    fcb     PLUME,DEBRIS,DEBRIS,DEBRIS,RAYS,SPRAY,PLUME,SCATTER
        fcb     RING,BURST,RING,RING,RING       ; hit, break, deflect, up, lift
*               n   1st step mask k  sh sj g   life
par     fcb     16,0,1,0,2,1,0,0,14    ; rays
        fcb     12,9,0,7,1,0,1,20,30    ; spray, up and over
        fcb     10,11,0,2,0,3,1,0,34    ; plume, rising slowly
        fcb     14,0,0,15,1,0,1,16,30    ; scatter, all ways
        fcb     8,9,0,7,1,1,1,20,24    ; debris
        fcb     16,0,1,0,3,4,0,0,8    ; ring: a dome round it
        fcb     16,0,1,0,3,0,0,0,8    ; burst

* void field_sparks(uint16_t a)  (after pshs u,y,dp: the kind at 7,s, the
* seat at 8,s)
_field_sparks
        pshs    u,y,dp
        lda     #DPAGE
        tfr     a,dp
        lda     #$FF
        sta     <curblk
        ldd     7,s
        bsr     sparks
        lbsr    fld_release
        puls    u,y,dp,pc

* the kind's parameters: X
kpar    ldx     #par
        ldb     <pkp
        abx
        rts

* sparks: kind A from seat B's tank
sparks  sta     <pkind
        cmpa    #13
        bhs     @x
        lda     #5              ; the tank: tk + 5 seat
        mul
        addd    #_tk
        tfr     d,x
        ldd     1,x             ; x
        std     <pcx
        lda     3,x             ; y, its bottom: its middle is up from there
        suba    fld_mid
        sta     <pcy
        ldx     #kmap
        ldb     <pkind
        lda     b,x
        sta     <pkp
        lbsr    kpar
        lda     P_N,x
        sta     <pleft
        clr     <pi
@one    ldu     #sparks_+(NP-1)*SP_SIZE     ; a free one
        ldb     #NP
@f      tst     SP_LIFE,u
        beq     @got
        leau    -SP_SIZE,u
        decb
        bne     @f
@x      rts
@got    stu     <prec
        lbsr    kpar
        lbsr    random          ; its direction
        anda    P_MASK,x
        adda    P_FIRST,x
        tst     P_STEP,x
        beq     @dir
        adda    <pi
@dir    anda    #15
        sta     <pd
        ldx     #cos
        lda     a,x
        lbsr    v16
        std     SP_VX,u
        lda     <pd
        adda    #12
        anda    #15
        ldx     #cos
        lda     a,x
        lbsr    v16
        std     SP_VY,u
        ldd     <pcx            ; from the middle (and a half)
        std     SP_X,u
        lda     #$80
        sta     SP_X+2,u
        lda     <pcy
        sta     SP_Y,u
        lda     #$80
        sta     SP_Y+1,u
        lbsr    kpar
        lda     P_K,x           ; out from the middle
        beq     @sp
        sta     <pd
@k      lbsr    move
        dec     <pd
        bne     @k
@sp     lbsr    kpar            ; then slower
        lbsr    random
        anda    P_SJ,x
        adda    P_SH,x
        beq     @g
        sta     <pd
@sh     ldd     SP_VX,u
        asra
        rorb
        std     SP_VX,u
        ldd     SP_VY,u
        asra
        rorb
        std     SP_VY,u
        dec     <pd
        bne     @sh
@g      lbsr    kpar
        lda     P_G,x
        sta     SP_GRAV,u
        lbsr    random
        anda    #7
        adda    P_LIFE,x
        sta     SP_LIFE,u
        clr     SP_SH,u
        inc     <pi
        dec     <pleft
        lbne    @one
        rts

* v16: A (1/16 px, signed) as D (1/256 px)
v16     tfr     a,b
        sex
        lslb
        rola
        lslb
        rola
        lslb
        rola
        lslb
        rola
        rts

pt_step clr     <plive
        ldu     #sparks_+(NP-1)*SP_SIZE
        lda     #NP
        sta     <pcnt
@p      lda     SP_LIFE,u
        beq     @n
        sta     <plive
        lbsr    erase
        dec     SP_LIFE,u
        beq     @n
        clra                    ; falling
        ldb     SP_GRAV,u
        addd    SP_VY,u
        std     SP_VY,u
        bsr     move
        lda     SP_Y,u
        cmpa    fld_h
        bhs     @die
        ldd     SP_X,u
        cmpd    fld_w
        bhs     @die
        std     <fx             ; on sky: fire
        clra
        ldb     SP_Y,u
        std     <fy
        pshs    u
        lbsr    fld_get
        puls    u
        cmpb    #PX_SKY
        bne     @n
        lda     #PX_FX
        sta     <fv
        pshs    u
        lbsr    fld_plot
        puls    u
        ldd     <fx
        std     SP_OX,u
        lda     SP_Y,u
        sta     SP_OY,u
        lda     #1
        sta     SP_SH,u
        bra     @n
@die    clr     SP_LIFE,u
@n      leau    -SP_SIZE,u
        dec     <pcnt
        bne     @p
        lda     <plive
        rts

* move: spark U a frame on (x += vx sign-extended: tst keeps the carry)
move    ldd     SP_X+1,u
        addd    SP_VX,u
        std     SP_X+1,u
        lda     SP_X,u
        tst     SP_VX,u
        bmi     @neg
        adca    #0
        bra     @s
@neg    adca    #$FF
@s      sta     SP_X,u
        ldd     SP_Y,u
        addd    SP_VY,u
        std     SP_Y,u
        rts

* random: A = the next of an 8-bit LFSR (never 0)
random  lda     <pseed
        bne     @go
        lda     #$A5
@go     lsla
        bcc     @n
        eora    #$1D
@n      sta     <pseed
        rts

* erase: spark U's pixel back to sky, if it's still fire
erase   tst     SP_SH,u
        beq     @x
        clr     SP_SH,u
        ldd     SP_OX,u
        std     <fx
        clra
        ldb     SP_OY,u
        std     <fy
        lda     #PX_FX
        sta     <fold
        lda     #PX_SKY
        sta     <fv
        pshs    u
        lbsr    fld_swap
        puls    u
@x      rts

pt_clear
        ldu     #sparks_+(NP-1)*SP_SIZE
        lda     #NP
        sta     <pcnt
@c      tst     SP_LIFE,u
        beq     @n
        bsr     erase
        clr     SP_LIFE,u
@n      leau    -SP_SIZE,u
        dec     <pcnt
        bne     @c
        rts

        section bss,bss
sparks_ rmb     NP*SP_SIZE
