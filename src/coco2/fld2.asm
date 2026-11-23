*
* fld2.asm -- the CoCo 1/2's field: PMODE 4 at $1000 (src/coco2/video.c),
* 128 colour pixels a line, four a byte (pixel 0 in bits 7-6), 32 bytes a
* line, lines 0-167. A pixel's two bits are its value through the artifact
* phase: fillt (a value as a byte of four) and invt (two bits as a value),
* both swapped for 1 and 2 when the phase is (fld_phase).
*
*   0  a tank (black)    1  sky (blue)    2  dirt (orange)    3  fire (white)
*
* The Atari's src/atari/fld.s in 6809, its ellipse the same steps (so the
* same pixels: tools/cocosim/refcheck.py), and the Apple's fld_dots. The
* arguments are in the direct page (src/6809/dp.inc); the internal entries
* (no underscore) want DP = $0E, the C ones (_) set it.
*
        section art             ; (the CoCo 1/2: $E000, with the art)

        include "../6809/dp.inc"

        export  _fld_phase,_fld_clear,_fld_plot,_fld_get,_fld_swap,_fld_hspan
        export  _fld_keep,_fld_vrun,_fld_col,_fld_ellipse,_fld_dots
        export  fld_plot,fld_get,fld_swap,fld_hspan,fld_vrun,fld_col,fld_cols,fld_ellipse,fld_dots
        export  fld_release,fld_w,fld_h,fld_mid

SCR     equ     $1000
FLINES  equ     168
FW      equ     128

* a C entry: DP $0E around the routine (its B the result; CMOC's U and Y kept)
CENTRY  macro
        pshs    u,y,dp
        lda     #DPAGE
        tfr     a,dp
        lbsr    \1
        puls    u,y,dp,pc
        endm

_fld_phase      CENTRY  fld_phase
_fld_clear      CENTRY  fld_clear
_fld_plot       CENTRY  fld_plot
_fld_get        CENTRY  fld_get
_fld_swap       CENTRY  fld_swap
_fld_hspan      CENTRY  fld_hspan
_fld_vrun       CENTRY  fld_vrun
_fld_col        CENTRY  fld_col
_fld_ellipse    CENTRY  fld_ellipse
_fld_dots       CENTRY  fld_dots
_fld_keep       CENTRY  fld_keep

* the field's size, a tank's middle over its foot (src/6809/pt.asm)
fld_w   fdb     FW
fld_h   fcb     FLINES
fld_mid fcb     2

* fld_release: nothing (the CoCo 3's gives slot 6 back)
fld_release
        rts

* a quarter circle: q32[i] = 64 sqrt(1 - (i/32)^2) (fld.s's)
q32     fcb     64,64,64,63,63,62,62,61,60,59,57,56,54,53,51,49
        fcb     46,44,41,38,35,32,28,24,20,15,10,5,0,0,0,0,0
mask    fcb     $C0,$30,$0C,$03
lmask   fcb     $FF,$3F,$0F,$03 ; a span's first byte: from pixel n on
rmask   fcb     $C0,$F0,$FC,$FF ; its last byte: up to pixel n

* fld_phase: the tables for the artifact phase (fswap: 0 01 is sky, 1 10 is)
fld_phase
        ldd     #$0055
        tst     <fswap
        beq     @n
        ldb     #$AA
@n      std     <fillt           ; 0: $00, 1: sky
        eorb    #$FF            ; (the other of $55 and $AA)
        lda     #$FF
        stb     <fillt+2
        sta     <fillt+3
        ldd     #$0001
        tst     <fswap
        beq     @i
        incb
@i      std     <invt            ; bits 00: 0, 01: 1 (or 2)
        eorb    #3
        lda     #3
        stb     <invt+2          ; bits 10: 2 (or 1)
        sta     <invt+3
        rts

* fld_clear: all sky
fld_clear
        ldx     #SCR
        lda     <fillt+1
        tfr     a,b
@l      std     ,x++
        cmpx    #SCR+FLINES*32
        blo     @l
        rts

* at: (fx, fy) in the field? carry clear: X its byte, B its pixel (0-3)
at      ldd     <fx
        cmpd    #FW
        bhs     @out            ; (negative too: unsigned)
        ldd     <fy
        cmpd    #FLINES
        bhs     @out
        lda     #32
        mul                     ; D = y * 32
        addd    #SCR
        tfr     d,x
        ldb     <fx+1
        lsrb
        lsrb
        abx
        ldb     <fx+1
        andb    #3
        andcc   #$FE
        rts
@out    orcc    #$01
        rts

* fld_plot: pixel (fx, fy) = fv
fld_plot
        bsr     at
        bcs     pret
put     ldy     #mask           ; (X, B from at)
        lda     b,y
        sta     <ft
        coma
        anda    ,x
        sta     <ft2
        ldb     <fv
        ldy     #DPBASE+fillt
        lda     b,y
        anda    <ft
        ora     <ft2
        sta     ,x
pret    rts

* fld_get: B = pixel (fx, fy); 1 (sky) off the field
fld_get
        bsr     at
        bcs     @sky
        lda     ,x
@sh     cmpb    #3
        beq     @r
        lsra
        lsra
        incb
        bra     @sh
@r      anda    #3
        ldy     #DPBASE+invt
        ldb     a,y
        rts
@sky    ldb     #1
        rts

* fld_swap: pixel (fx, fy) = fv if it is fold
fld_swap
        bsr     at
        bcs     @x
        pshs    b
        ldy     #mask
        lda     b,y
        anda    ,x              ; its bits, in place
        sta     <ft
        ldb     <fold
        ldy     #DPBASE+fillt
        lda     b,y             ; fold's, in place
        puls    b
        ldy     #mask
        anda    b,y
        cmpa    <ft
        bne     @x
        bra     put
@x      rts

* fld_hspan: line fy, x from fx to fx1 (both included) = fv, clipped
fld_hspan
        ldd     <fy
        cmpd    #FLINES
        bhs     @out
        lda     #32
        mul
        addd    #SCR
        std     <fp
        ldd     <fx
        bpl     @c0
        clra
        clrb
        std     <fx
@c0     cmpd    #FW
        bge     @out            ; x0 past the right
        ldd     <fx1
        bmi     @out            ; x1 left of the field
        cmpd    #FW-1
        ble     @c1
        ldd     #FW-1
        std     <fx1
@c1     ldd     <fx
        cmpd    <fx1
        ble     @go             ; (x0 > x1: nothing)
@out    rts
@go     ldb     <fv
        ldy     #DPBASE+fillt
        lda     b,y
        sta     <val
        ldb     <fx+1
        lsrb
        lsrb
        stb     <xb              ; the first byte
        ldb     <fx1+1
        lsrb
        lsrb
        stb     <cur             ; the last
        ldb     <fx+1
        andb    #3
        ldy     #lmask
        lda     b,y
        sta     <ft
        ldb     <fx1+1
        andb    #3
        ldy     #rmask
        lda     b,y
        sta     <ft2
        ldx     <fp
        ldb     <xb
        abx
        cmpb    <cur
        bne     @multi
        lda     <ft              ; one byte: both masks
        anda    <ft2
        bra     edge
@multi  lda     <ft
        bsr     edge
        leax    1,x
        ldb     <cur
        subb    <xb
        decb                    ; the whole bytes between
        beq     @last
        lda     <val
@mid    sta     ,x+
        decb
        bne     @mid
@last   lda     <ft2
* edge: byte X, the pixels of mask A get val
edge    sta     <ft
        coma
        anda    ,x
        sta     <cnt
        lda     <val
        anda    <ft
        ora     <cnt
        sta     ,x
        rts

* colset: fx's byte (xb), its pixel's mask inverted (nm), fv in its place (val)
colset  ldb     <fx+1
        lsrb
        lsrb
        stb     <xb
        ldb     <fx+1
        andb    #3
        ldy     #mask
        lda     b,y
        sta     <ft
        coma
        sta     <nm
        ldb     <fv
        ldy     #DPBASE+fillt
        lda     b,y
        anda    <ft
        sta     <val
        rts

* vrows: rows cur..cnt-1 of column xb get val
vrows   ldb     <cnt
        subb    <cur
        bls     @x
        stb     <ft2
        lda     #32
        ldb     <cur
        mul
        addd    #SCR
        tfr     d,x
        ldb     <xb
        abx
        ldb     <ft2
@l      lda     ,x
        anda    <nm
        ora     <val
        sta     ,x
        leax    32,x
        decb
        bne     @l
@x      rts

* fld_vrun: column fx, rows fy to fx1-1 = fv (all in the field)
fld_vrun
        lbsr    colset
        ldb     <fy+1
        stb     <cur
        ldb     <fx1+1
        stb     <cnt
        bra     vrows

* run: rows cur..cnt-1 of column fx = A; cur = cnt after
run     sta     <fv
        lda     <cnt
        cmpa    #FLINES
        bls     @ok
        lda     #FLINES
        sta     <cnt
@ok     bsr     colset
        lbsr    vrows
        lda     <cnt
        sta     <cur
        rts

* fld_col: column fx becomes the TERRAIN entry at fent (sky, dirt); B its
* length
fld_col clr     <cur
        ldx     <fent
        lda     ,x
        cmpa    #$F0
        beq     @empty
        bhi     @spans
        sta     <cnt             ; one plain column: sky to its top,
        pshs    a
        lda     #1
        lbsr    run
        puls    a
        sta     <cur
        lda     #FLINES         ;  dirt from there
        sta     <cnt
        lda     #2
        lbsr    run
        ldb     #1
        rts
@empty  lda     #FLINES
        sta     <cnt
        lda     #1
        lbsr    run
        ldb     #1
        rts
@spans  anda    #$0F
        sta     <don             ; the spans
        lsla
        inca                    ; 1 + 2n: the entry's length
        pshs    a
        ldx     <fent
        leax    1,x
        stx     <dop
@sp     ldx     <dop
        lda     ,x              ; top: sky till there
        sta     <cnt
        lda     #1
        lbsr    run
        ldx     <dop
        lda     ,x+
        sta     <cur
        lda     ,x+             ; end: dirt till there
        stx     <dop
        sta     <cnt
        lda     #2
        lbsr    run
        dec     <don
        bne     @sp
        lda     #FLINES
        sta     <cnt
        lda     #1
        lbsr    run
        puls    b
        rts

* ---- a settle's columns (fld_cols): a byte's pixels at once -------------
* A TERRAIN entry as its runs, top down: sky to the first end, then dirt and
* sky by turns (plain: its top, then the bottom; spans: top, end, top ...,
* then the bottom), an iterator I_* (dp.inc) at U.

* iinit: U reads the entry at X; B its length
iinit   lda     #1
        sta     I_V,u
        ldb     ,x
        cmpb    #$F0
        blo     @plain
        beq     @empty
        andb    #$0F
        lslb                    ; 2n ends
        stb     I_K,u
        leax    1,x
        stx     I_P,u
        incb                    ; 1 + 2n
        pshs    b
        bsr     iload
        puls    b,pc
@plain  stx     I_P,u           ; its top, then the bottom
        lda     #1
        sta     I_K,u
        bsr     iload
        ldb     #1
        rts
@empty  clr     I_K,u
        bsr     iload
        ldb     #1
        rts

* inext: U's next run
inext   lda     I_V,u
        eora    #3
        sta     I_V,u
* iload: U's run's end: the entry's next, or the bottom when it has none
iload   lda     #FLINES
        tst     I_K,u
        beq     @s
        dec     I_K,u
        ldx     I_P,u
        lda     ,x+
        stx     I_P,u
        cmpa    #FLINES
        bls     @s
        lda     #FLINES
@s      sta     I_E,u
        rts

* fld_cols: B columns from fx on become the TERRAIN entries from fent on
* (fx, fent past them after)
fld_cols
        stb     <gn
@c      lda     <gn
        beq     @x
        lda     <fx+1
        anda    #3
        nega
        adda    #4              ; the byte's columns from fx on
        cmpa    <gn
        bls     @k
        lda     <gn
@k      sta     <gk
        cmpa    #1
        bne     @grp
        lbsr    fld_col         ; alone (B: its entry's length)
        clra
        addd    <fent
        std     <fent
        bra     @nx
@grp    lbsr    fld_group
@nx     ldb     <gk
        clra
        addd    <fx
        std     <fx
        lda     <gn
        suba    <gk
        sta     <gn
        bra     @c
@x      rts

* fld_group: the gk columns from fx on, all in one byte, become the entries
* at fent (fent past them after): row runs alike in all of them, the byte
* at once
fld_group
        ldd     <fx
        lsra
        rorb
        lsra
        rorb
        stb     <xb
        ldb     <fx+1
        andb    #3
        ldx     #mask
        abx                     ; the first column's bits
        stx     <dop
        clr     <gkeep
        ldu     #DPBASE+its
        lda     <gk
        sta     <ei
@i      ldx     <dop
        lda     ,x+
        stx     <dop
        sta     I_M,u
        ora     <gkeep
        sta     <gkeep
        ldx     <fent
        lbsr    iinit           ; (B: the entry's length)
        clra
        addd    <fent
        std     <fent
        leau    I_SIZE,u
        dec     <ei
        bne     @i
        com     <gkeep          ; (the byte's other bits stay)
        clr     <cur
@m      ldu     #DPBASE+its     ; the nearest end, and the byte till then
        lda     <gk
        sta     <ei
        lda     #FLINES
        sta     <cnt
        clr     <val
@e      lda     I_E,u
        cmpa    <cnt
        bhs     @e1
        sta     <cnt
@e1     ldb     I_V,u
        ldx     #DPBASE+fillt
        lda     b,x
        anda    I_M,u
        ora     <val
        sta     <val
        leau    I_SIZE,u
        dec     <ei
        bne     @e
        lda     <cnt
        cmpa    <cur
        bls     @adv            ; (an empty run)
        lda     #32
        ldb     <cur
        mul
        addd    #SCR
        tfr     d,x
        ldb     <xb
        abx
        ldb     <cnt
        subb    <cur
        lda     <gkeep
        beq     @full
@part   lda     ,x
        anda    <gkeep
        ora     <val
        sta     ,x
        leax    32,x
        decb
        bne     @part
        bra     @drawn
@full   lda     <val            ; (the whole byte ours)
@f      sta     ,x
        leax    32,x
        decb
        bne     @f
@drawn  lda     <cnt
        sta     <cur
@adv    ldu     #DPBASE+its     ; the runs that end here: their next
        lda     <gk
        sta     <ei
@a      lda     I_E,u
        cmpa    <cur
        bhi     @a1
        lbsr    inext
@a1     leau    I_SIZE,u
        dec     <ei
        bne     @a
        lda     <cur
        cmpa    #FLINES
        blo     @m
        rts

* fld_ellipse: the ellipse at (ex, ey), radii erx, ery. Its row dy is half
* as wide as erx * sqrt(1 - (dy/ery)^2): q32[i], i = dy*32/ery stepped
* without dividing (fld.s's steps, to the pixel)
fld_ellipse
        lda     <ery
        bne     @r
        inc     <ery
        lda     <ery
@r      lsra
        sta     <erem            ; ery / 2: the rounding
        clr     <edy
        clr     <ei
@row    ldx     #q32            ; hw = (erx * q32[i] + 32) >> 6
        ldb     <ei
        lda     b,x
        ldb     <erx
        mul
        addd    #32
        lsra
        rorb
        lsra
        rorb
        lsra
        rorb
        lsra
        rorb
        lsra
        rorb
        lsra
        rorb
        std     <ehw
        clra
        ldb     <edy
        std     <edw
        ldd     <ey              ; the row above: ey - dy
        subd    <edw
        std     <fy
        lbsr    erow
        tst     <edy
        beq     @nx
        ldd     <ey              ; and below: ey + dy
        addd    <edw
        std     <fy
        lbsr    erow
@nx     lda     <edy
        cmpa    <ery
        bhs     @x
        inc     <edy
        lda     <erem
        adda    #32
        sta     <erem
@i      lda     <erem
        cmpa    <ery
        blo     @row
        suba    <ery
        sta     <erem
        lda     <ei
        cmpa    #32
        bhs     @i
        inc     <ei
        bra     @i
@x      rts

* erow: one row of the ellipse: line fy, ex - hw to ex + hw
erow    ldd     <ex
        subd    <ehw
        std     <fx
        ldd     <ex
        addd    <ehw
        std     <fx1
        lda     <emode
        lbeq    fld_hspan
        cmpa    #1              ; only the right value changes: sky becomes fv,
        bne     @m2
        lda     #1
        sta     <fold
        lbra    espan
@m2     lda     #3              ;  or fire becomes sky
        sta     <fold
        lda     #1
        sta     <fv
        lbra    espan

* fld_dots: a shape around (fx, fy): fent points at its count, then that
* many (dx, dy) pairs (signed bytes). fold $FF: each pixel = fv; else each
* pixel that is fold = fv. Off the field is left out. (fx, fy) are kept.
fld_dots
        ldd     <fx
        std     <dox
        ldd     <fy
        std     <doy
        ldx     <fent
        lda     ,x+
        beq     @x
        sta     <don
@l      ldb     ,x+             ; fx = ox + dx
        sex
        addd    <dox
        std     <fx
        ldb     ,x+             ; fy = oy + dy
        sex
        addd    <doy
        std     <fy
        pshs    x
        lda     <fold
        cmpa    #$FF
        bne     @sw
        lbsr    fld_plot
        bra     @n
@sw     lbsr    fld_swap
@n      puls    x
        dec     <don
        bne     @l
@x      ldd     <dox
        std     <fx
        ldd     <doy
        std     <fy
        rts

* fld_keep: a box of whole bytes: lines fy.. (fv of them), bytes fx..
* (fx1 a line); fold 0: kept at fent, else put back from there
fld_keep
        lda     <fy+1
        ldb     #32
        mul
        addd    #SCR
        addd    <fx
        tfr     d,x
        ldu     <fent
        lda     <fv
        sta     <cnt
@l      ldb     <fx1+1
        pshs    x
@b      tst     <fold
        bne     @put
        lda     ,x+
        sta     ,u+
        bra     @n
@put    lda     ,u+
        sta     ,x+
@n      decb
        bne     @b
        puls    x
        leax    32,x
        dec     <cnt
        bne     @l
        rts

* ---- (in the high core: the art region is full) ---------------------------
        section hicode

* espan: line fy, x from fx to fx1 (clipped): its pixels that are fold (sky
* or fire) become fv, a byte at a time (fld_swap's, pixel for pixel)
espan   ldd     <fy
        cmpd    #FLINES
        bhs     @out
        ldd     <fx
        bpl     @c0
        clra
        clrb
        std     <fx
@c0     cmpd    #FW
        bge     @out
        ldd     <fx1
        bmi     @out
        cmpd    #FW-1
        ble     @c1
        ldd     #FW-1
        std     <fx1
@c1     ldd     <fx
        cmpd    <fx1
        ble     @go
@out    rts
@go     ldy     #DPBASE+fillt
        ldb     <fold
        lda     b,y
        sta     <val            ; four pixels of the old value
        ldb     <fv
        lda     b,y
        sta     <ft2            ;  and of the new (not erem: the ellipse's)
        lda     #32
        ldb     <fy+1
        mul
        addd    #SCR
        tfr     d,x
        ldb     <fx1+1
        lsrb
        lsrb
        stb     <cnt            ; the last byte
        ldb     <fx+1
        lsrb
        lsrb
        stb     <cur            ; the first
        abx
        ldb     <fx+1
        andb    #3
        ldy     #lmask
        lda     b,y
        sta     <ft             ; the first byte's pixels: from x0 on
@b      ldb     <cur
        cmpb    <cnt
        bne     @m
        ldb     <fx1+1          ; the last's: up to x1
        andb    #3
        ldy     #rmask
        lda     b,y
        anda    <ft
        sta     <ft
@m      lda     ,x              ; the pixels that are the old value: their
        eora    <val            ;  pairs 00 here
        tfr     a,b
        lsrb
        stb     <nm
        ora     <nm
        anda    #$55            ; (a pair's low bit: not 00)
        tfr     a,b
        lslb
        stb     <nm
        ora     <nm
        coma                    ; the pairs that were 00
        anda    <ft
        beq     @n
        sta     <nm
        lda     ,x              ; they become the new value
        eora    <ft2
        anda    <nm
        eora    ,x
        sta     ,x
@n      lda     #$FF
        sta     <ft
        leax    1,x
        inc     <cur
        lda     <cur
        cmpa    <cnt
        bls     @b
        rts

