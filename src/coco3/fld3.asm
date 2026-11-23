*
* fld3.asm -- the CoCo 3's field: the GIME's 320x200 in 16 colours
* (src/coco3/video.c), lines 0-175. Two pixels a byte (the left one in the
* high nibble), lines 256 bytes apart (HVEN): line y is in block
* $30 + y/32, seen in slot 6 at $C000 + (y%32)*256 + x/2.
*
* A pixel's colour is its value through the round's patterns:
*
*   0  tank   colour 0 (black); a tank's own colours are 6-15
*   1  sky    colours 1 and 2: the row's pattern (a dither, stars)
*   2  dirt   colours 3 and 4: the row's pattern (the strata)
*   3  fire   colour 5 (src/coco3/video.c cycles it)
*
* and back through dec (16 colours -> values). The patterns are rows:
* rows[y] is 6 bytes, the sky's pattern and its mask, the dirt's pattern
* and its mask (src/coco/field.c builds them for the round): a byte at
* index b of the line is pattern[b & mask], so a pattern repeats every
* mask + 1 bytes (2 for a dither, 64 for a row of stars).
*
* The Atari's src/atari/fld.s's routines and steps (the same pixels:
* tools/cocosim/refcheck.py), with the Apple's fld_dots. Arguments are in
* the direct page (src/6809/dp.inc); the internal entries want DP = $0E,
* the C ones (_) set it. Slot 6 is mapped to the screen lazily (curblk)
* and given back to the Disk ROM's block ($3E: DriveWire runs there) at
* the end of each C entry, and by fld_release (the player's).
*
        section hicode

        include "../6809/dp.inc"

        export  _fld_clear,_fld_plot,_fld_get,_fld_swap,_fld_hspan
        export  _fld_repat,_fld_tables,_fld_keep,_fld_vrun,_fld_col,_fld_ellipse,_fld_dots,_rows
        export  fld_plot,fld_get,fld_swap,fld_hspan,fld_vrun,fld_col,fld_cols,fld_ellipse,fld_dots
        export  fld_release,fld_w,fld_h,fld_mid

SCRBLK  equ     $30
ROMBLK  equ     $3E
FLINES  equ     176
FW      equ     320

* a C entry: DP $0E around the routine (its B the result; CMOC's U and Y
* kept), slot 6 the Disk ROM's again after
CENTRY  macro
        pshs    u,y,dp
        lda     #DPAGE
        tfr     a,dp
        lda     #$FF
        sta     <curblk
        lbsr    \1
        lda     #ROMBLK
        sta     $FFA6
        lda     #$FF            ; (none mapped: the next maps its own)
        sta     <curblk
        puls    u,y,dp,pc
        endm

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
_fld_repat      CENTRY  fld_repat
_fld_tables     CENTRY  fld_tables

* fld_release: slot 6 the Disk ROM's (A changed)
fld_release
        lda     #ROMBLK
        sta     $FFA6
        lda     #$FF
        sta     <curblk
        rts

* the field's size, a tank's middle over its foot (src/6809/pt.asm)
fld_w   fdb     FW
fld_h   fcb     FLINES
fld_mid fcb     3

* a quarter circle: q32[i] = 64 sqrt(1 - (i/32)^2) (fld.s's)
q32     fcb     64,64,64,63,63,62,62,61,60,59,57,56,54,53,51,49
        fcb     46,44,41,38,35,32,28,24,20,15,10,5,0,0,0,0,0
nibm    fcb     $F0,$0F         ; a pixel's nibble: the left one, the right one
kpat    fcb     $00,$55         ; tank, fire: one byte, mask 0
* a colour's value
dec     fcb     0,1,1,2,2,3,0,0,0,0,0,0,0,0,0,0

* mapy: B = y (0-175): its block in slot 6; A = its line's page ($C0-$DF)
mapy    tfr     b,a
        lsra
        lsra
        lsra
        lsra
        lsra
        adda    #SCRBLK
        cmpa    <curblk
        beq     @m
        sta     <curblk
        sta     $FFA6
@m      tfr     b,a
        anda    #31
        adda    #$C0
        rts

* rowsel: A = a value, B = y: U its pattern on that row, pm its mask
* (A, B, Y changed)
rowsel  cmpa    #1
        beq     @s              ; sky
        cmpa    #2
        beq     @d              ; dirt
        ldu     #kpat           ; tank, fire
        tsta
        beq     @k
        leau    1,u
@k      clr     <pm
        rts
@s      lda     #6
        mul
        ldy     #_rows
        leay    d,y
        ldu     ,y
        lda     2,y
        sta     <pm
        rts
@d      lda     #6
        mul
        ldy     #_rows+3
        leay    d,y
        ldu     ,y
        lda     2,y
        sta     <pm
        rts

* fld_clear: all sky
fld_clear
        clra
        clrb
        std     <fy
        std     <fx
        ldd     #FW-1
        std     <fx1
        lda     #1
        sta     <fv
@l      lbsr    fld_hspan
        ldd     <fy
        addd    #1
        std     <fy
        cmpd    #FLINES
        blo     @l
        rts

* at: (fx, fy) in the field? carry clear: X its byte, B its pixel (0 the
* left), xb its byte's index in the line
at      ldd     <fx
        cmpd    #FW
        bhs     @out            ; (negative too: unsigned)
        ldd     <fy
        cmpd    #FLINES
        bhs     @out
        lbsr    mapy
        sta     <fp
        ldd     <fx
        lsra
        rorb                    ; B = x / 2, carry the pixel
        stb     <fp+1
        stb     <xb
        ldb     #0
        rolb
        ldx     <fp
        andcc   #$FE
        rts
@out    orcc    #$01
        rts

* fld_plot: pixel (fx, fy) = fv
fld_plot
        lbsr    at
        bcs     pret
        pshs    b,x
        lda     <fv
        ldb     <fy+1
        lbsr    rowsel
        puls    b,x
* put: byte X, pixel B gets its colour from the pattern (U, pm) at xb
put     ldy     #nibm
        lda     b,y
        sta     <ft
        coma
        anda    ,x
        sta     <ft2
        lda     <xb
        anda    <pm
        lda     a,u
        anda    <ft
        ora     <ft2
        sta     ,x
pret    rts

* the value of pixel B of byte X, in A
value   lda     ,x
        tstb
        bne     @lo
        lsra
        lsra
        lsra
        lsra
@lo     anda    #15
        ldy     #dec
        lda     a,y
        rts

* fld_get: B = pixel (fx, fy); 1 (sky) off the field
fld_get
        lbsr    at
        bcs     @sky
        lbsr    value
        tfr     a,b
        rts
@sky    ldb     #1
        rts

* fld_swap: pixel (fx, fy) = fv if it is fold
fld_swap
        lbsr    at
        bcs     pret
        lbsr    value
        cmpa    <fold
        bne     pret
        pshs    b,x
        lda     <fv
        ldb     <fy+1
        lbsr    rowsel
        puls    b,x
        bra     put

* fld_hspan: line fy, x from fx to fx1 (both included) = fv, clipped
fld_hspan
        ldd     <fy
        cmpd    #FLINES
        bhs     @out
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
@go     ldb     <fy+1
        lbsr    mapy
        sta     <fp
        clr     <fp+1
        lda     <fv
        ldb     <fy+1
        lbsr    rowsel
        ldd     <fx
        lsra
        rorb                    ; B: the first byte, carry: x0 odd
        bcc     @l0
        lda     #$0F            ; x0 odd: its byte's right pixel
        lbsr    edge
        incb
@l0     stb     <cur            ; the whole bytes from here
        ldd     <fx1
        lsra
        rorb                    ; B: the last byte, carry: x1 odd
        bcs     @r1
        cmpb    <cur            ; x1 even: its byte's left pixel
        blo     @r1
        lda     #$F0
        lbsr    edge
        bra     @r2
@r1     incb
@r2     stb     <cnt            ; the whole bytes up to here (not included)
        subb    <cur
        bls     @x
        stb     <don
        ldb     <cur
        ldx     <fp
        abx
        lda     <pm
        beq     @const
        cmpa    #1
        beq     @pair
@mid    tfr     b,a             ; (a longer pattern: a row of stars)
        anda    <pm
        lda     a,u
        sta     ,x+
        incb
        dec     <don
        bne     @mid
@x      rts
@const  lda     ,u              ; (tank, fire: one byte)
        tfr     a,b
        bra     @w
@pair   andb    #1              ; a pattern of two bytes: the first's, the next's
        lda     b,u
        eorb    #1
        ldb     b,u
@w      lsr     <don            ; words (an odd byte first)
        bcc     @w1
        sta     ,x+
        exg     a,b
        tst     <don
        beq     @x
@w1     std     ,x++
        dec     <don
        bne     @w1
        rts

* edge: byte B of the line at fp, the pixels of mask A from the pattern
* (B kept)
edge    sta     <ft
        ldx     <fp
        abx
        coma
        anda    ,x
        sta     <ft2
        tfr     b,a
        anda    <pm
        lda     a,u
        anda    <ft
        ora     <ft2
        sta     ,x
        rts

* colset: column fx: xb its byte, ft its nibble's mask, nm the rest
colset  ldd     <fx
        lsra
        rorb
        stb     <xb
        ldb     #0
        rolb
        ldy     #nibm
        lda     b,y
        sta     <ft
        coma
        sta     <nm
        rts

* vrows: rows cur..cnt-1 of column xb get fv's pattern (its pixel ft, nm
* the other)
vrows   ldb     <cnt
        subb    <cur
        bls     @x
        stb     <don
        ldb     <cur
        lbsr    mapy
        ldb     <xb
        tfr     d,x             ; the first row's byte
        lda     <fv
        cmpa    #1
        beq     @pat
        cmpa    #2
        beq     @pat
        ldu     #kpat           ; tank, fire: one byte
        tsta
        beq     @k
        leau    1,u
@k      lda     ,u
        anda    <ft
        sta     <ft2
@kl     lda     ,x
        anda    <nm
        ora     <ft2
        sta     ,x
        leax    256,x
        cmpx    #$E000
        blo     @kn
        lbsr    nextblk
@kn     dec     <don
        bne     @kl
@x      rts
@pat    deca                    ; the row's record: 0 (sky) or 3 (dirt) in
        ldb     #3
        mul
        addd    #_rows
        tfr     d,y
        lda     #6
        ldb     <cur
        mul
        leay    d,y
@blk    lbsr    chunk           ; B: the rows in this block
@pl     ldu     ,y
        lda     2,y
        anda    <xb
        lda     a,u             ; the pattern's byte,
        eora    ,x              ;  its pixel ft into the screen's
        anda    <ft
        eora    ,x
        sta     ,x
        leay    6,y
        leax    256,x
        decb
        bne     @pl
        tst     <don
        beq     @y
        lbsr    nextblk
        bra     @blk
@y      rts

* nextblk: X went past slot 6: the next block's first line
nextblk leax    -$2000,x
        lda     <curblk
        inca
        sta     <curblk
        sta     $FFA6
        rts

* fld_vrun: column fx, rows fy to fx1-1 = fv (all in the field)
fld_vrun
        lbsr    colset
        ldb     <fy+1
        stb     <cur
        ldb     <fx1+1
        stb     <cnt
        lbra    vrows

* run: rows cur..cnt-1 of column fx = A; cur = cnt after
run     sta     <fv
        lda     <cnt
        cmpa    #FLINES
        bls     @ok
        lda     #FLINES
        sta     <cnt
@ok     lbsr    colset
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
        sta     <cnt            ; one plain column: sky to its top,
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
        sta     <ei             ; the spans
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
        dec     <ei
        bne     @sp
        lda     #FLINES
        sta     <cnt
        lda     #1
        lbsr    run
        puls    b
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
        sta     <erem           ; ery / 2: the rounding
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
        ldd     <ey             ; the row above: ey - dy
        subd    <edw
        std     <fy
        lbsr    erow
        tst     <edy
        beq     @nx
        ldd     <ey             ; and below: ey + dy
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
* many (dx, dy) pairs (signed bytes). fold $FF: each pixel = colour fv
* (0-15: a tank's own colours); else each pixel whose value is fold = value
* fv. Off the field is left out. (fx, fy) are kept.
fld_dots
        ldd     <fx
        std     <dox
        ldd     <fy
        std     <doy
        lda     <fv             ; the colour in both nibbles
        ldb     #$11
        mul
        stb     <rawb
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
        lbsr    at
        bcs     @n
        ldu     #DPBASE+rawb
        clr     <pm
        lbsr    put
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
* (fx1 a line); fold 0: kept, else put back. Kept in the screen's own
* memory past what shows: block $36's lines 208 on, seen through slot 7 for
* the while (this code is in the high core; the IRQs don't use slot 7)
fld_keep
        lda     <fv
        sta     <cnt
        ldb     <fy+1
        stb     <cur
        ldu     #$F000          ; ($E000 + 16 lines of 256: line 208)
        lda     #$36
        sta     $FFA7
@l      ldb     <cur
        lbsr    mapy
        ldb     <fx+1
        tfr     d,x
        ldb     <fx1+1
@b      tst     <fold
        bne     @put
        lda     ,x+
        sta     ,u+
        bra     @n
@put    lda     ,u+
        sta     ,x+
@n      decb
        bne     @b
        inc     <cur
        dec     <cnt
        bne     @l
        lda     #$37            ; slot 7: the art again
        sta     $FFA7
        rts

* fld_repat: every sky pixel again from its line's pattern (the rows are
* another sky's now)
fld_repat
        clr     <cur
@y      ldb     <cur
        lbsr    mapy
        clrb
        tfr     d,x
        lda     #1
        ldb     <cur
        lbsr    rowsel
        clrb
@b      lda     ,x
        sta     <ft
        tfr     b,a
        anda    <pm
        lda     a,u
        sta     <ft2            ; the pattern's byte
        lda     <ft
        anda    #$F0
        cmpa    #$10
        beq     @h
        cmpa    #$20
        bne     @nh
@h      lda     <ft             ; the left pixel is sky: the pattern's
        anda    #$0F
        sta     <ft
        lda     <ft2
        anda    #$F0
        ora     <ft
        sta     <ft
@nh     lda     <ft
        anda    #$0F
        cmpa    #1
        beq     @l
        cmpa    #2
        bne     @nl
@l      lda     <ft             ; the right one
        anda    #$F0
        sta     <ft
        lda     <ft2
        anda    #$0F
        ora     <ft
        sta     <ft
@nl     lda     <ft
        sta     ,x+
        incb
        cmpb    #FW/2
        blo     @b
        inc     <cur
        lda     <cur
        cmpa    #FLINES
        blo     @y
        rts

        section bss
_rows   rmb     FLINES*6        ; a row's sky pattern, its mask, its dirt pattern, its mask

* ---- (in the mid region: the high core is full) ---------------------------
        section mid

* chunk: of the don rows from X on, those in X's block (B; don less them)
chunk   tfr     x,d
        nega
        adda    #$E0            ; the block's lines from X's on
        cmpa    <don
        bls     @c
        lda     <don
@c      tfr     a,b
        nega
        adda    <don
        sta     <don
        rts

* vbytes: rows cur..cnt-1 of byte xb: both its pixels fv's (sky or dirt)
vbytes  ldb     <cnt
        subb    <cur
        bls     @x
        stb     <don
        ldb     <cur
        lbsr    mapy
        ldb     <xb
        tfr     d,x
        lda     <fv
        deca
        ldb     #3
        mul
        addd    #_rows
        tfr     d,y
        lda     #6
        ldb     <cur
        mul
        leay    d,y
@blk    bsr     chunk
        lsrb                    ; two rows a turn (an odd one first)
        bcc     @l
        ldu     ,y
        lda     2,y
        anda    <xb
        lda     a,u
        sta     ,x
        leay    6,y
        leax    256,x
        tstb
        beq     @e
@l      ldu     ,y
        lda     2,y
        anda    <xb
        lda     a,u
        sta     ,x
        ldu     6,y
        lda     8,y
        anda    <xb
        lda     a,u
        sta     256,x
        leay    12,y
        leax    512,x
        decb
        bne     @l
@e      tst     <don
        beq     @x
        lbsr    nextblk
        bra     @blk
@x      rts

* ---- a settle's columns (fld_cols): a byte's two pixels at once ----------
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
* (fx, fent past them after): four at once (two bytes, a word a row) where
* they can be, else two (a byte), else one
fld_cols
        stb     <gn
@c      lda     <gn
        beq     @x
        cmpa    #1
        beq     @one            ; the last: alone
        lda     <fx+1
        bita    #1
        bne     @one            ; odd: alone (its byte's other pixel isn't ours)
        bita    #2
        bne     @two            ; (a byte, then words)
        lda     <gn
        cmpa    #4
        blo     @two
        lda     #4
        sta     <gk
        lbsr    fld_quad
        bra     @nx
@two    lda     #2
        sta     <gk
        lbsr    fld_pair
        bra     @nx
@one    lbsr    fld_col         ; (B: its entry's length)
        clra
        addd    <fent
        std     <fent
        lda     #1
        sta     <gk
@nx     ldb     <gk
        clra
        addd    <fx
        std     <fx
        lda     <gn
        suba    <gk
        sta     <gn
        bra     @c
@x      rts

* fld_pair: columns fx (even) and fx + 1 become the entries at fent (fent
* past both after): where they are alike, whole bytes
fld_pair
        ldx     <fent
        ldu     #DPBASE+its
        lbsr    iinit
        clra
        addd    <fent
        std     <fent
        tfr     d,x
        ldu     #DPBASE+its+I_SIZE
        lbsr    iinit
        clra
        addd    <fent
        std     <fent
        ldd     <fx
        lsra
        rorb
        stb     <xb
        clr     <cur
@m      lda     <its+I_E        ; the nearer end
        cmpa    <its+I_SIZE+I_E
        bls     @n
        lda     <its+I_SIZE+I_E
@n      sta     <cnt
        cmpa    <cur
        bls     @adv            ; (an empty run)
        ldu     #DPBASE+its
        lbsr    pbyte
        lda     <cnt
        sta     <cur
@adv    lda     <its+I_E        ; the runs that end here: their next
        cmpa    <cur
        bhi     @a1
        ldu     #DPBASE+its
        lbsr    inext
@a1     lda     <its+I_SIZE+I_E
        cmpa    <cur
        bhi     @a2
        ldu     #DPBASE+its+I_SIZE
        lbsr    inext
@a2     lda     <cur
        cmpa    #FLINES
        blo     @m
        rts


* pbyte: rows cur..cnt-1 of byte xb: its left pixel the value of the
* iterator at U, its right the next one's: alike, whole bytes
pbyte   lda     I_V,u
        cmpa    I_SIZE+I_V,u
        bne     @two
        sta     <fv
        lbra    vbytes
@two    sta     <fv             ; not: the left pixel, then the right
        lda     #$F0
        sta     <ft
        coma
        sta     <nm
        pshs    u
        lbsr    vrows
        puls    u
        lda     I_SIZE+I_V,u
        sta     <fv
        lda     #$0F
        sta     <ft
        coma
        sta     <nm
        lbra    vrows

* fld_quad: columns fx to fx + 3 (fx a multiple of 4: bytes xb, xb + 1, xb
* even) become the entries at fent (fent past them after): where all four
* are alike, a word a row (a pattern's two bytes there: its period is 2 or
* 64), else by bytes
fld_quad
        ldu     #DPBASE+its
        lda     #4
        sta     <ei
@i      ldx     <fent
        lbsr    iinit
        clra
        addd    <fent
        std     <fent
        leau    I_SIZE,u
        dec     <ei
        bne     @i
        ldd     <fx
        lsra
        rorb
        stb     <xb
        clr     <cur
@m      lda     <its+I_E        ; the nearest end
        cmpa    <its+I_SIZE+I_E
        bls     @n1
        lda     <its+I_SIZE+I_E
@n1     cmpa    <its+2*I_SIZE+I_E
        bls     @n2
        lda     <its+2*I_SIZE+I_E
@n2     cmpa    <its+3*I_SIZE+I_E
        bls     @n3
        lda     <its+3*I_SIZE+I_E
@n3     sta     <cnt
        cmpa    <cur
        bls     @adv            ; (an empty run)
        lda     <its+I_V
        cmpa    <its+I_SIZE+I_V
        bne     @by
        cmpa    <its+2*I_SIZE+I_V
        bne     @by
        cmpa    <its+3*I_SIZE+I_V
        bne     @by
        sta     <fv             ; all alike: words
        lbsr    vwords
        bra     @dn
@by     ldu     #DPBASE+its     ; by bytes
        lbsr    pbyte
        inc     <xb
        ldu     #DPBASE+its+2*I_SIZE
        lbsr    pbyte
        dec     <xb
@dn     lda     <cnt
        sta     <cur
@adv    ldu     #DPBASE+its     ; the runs that end here: their next
        lda     #4
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

* vwords: rows cur..cnt-1 of bytes xb, xb + 1 (xb even): all four pixels
* fv's (sky or dirt), a word a row
vwords  ldb     <cnt
        subb    <cur
        bls     @x
        stb     <don
        ldb     <cur
        lbsr    mapy
        ldb     <xb
        tfr     d,x
        lda     <fv
        deca
        ldb     #3
        mul
        addd    #_rows
        tfr     d,y
        lda     #6
        ldb     <cur
        mul
        leay    d,y
@blk    lbsr    chunk           ; B: the rows in this block
        lsrb                    ; two a turn (an odd one first)
        stb     <ft2
        bcc     @l0
        ldu     ,y
        lda     2,y
        anda    <xb
        ldd     a,u
        std     ,x
        leay    6,y
        leax    256,x
@l0     tst     <ft2
        beq     @e
@l      ldu     ,y
        lda     2,y
        anda    <xb
        ldd     a,u
        std     ,x
        ldu     6,y
        lda     8,y
        anda    <xb
        ldd     a,u
        std     256,x
        leay    12,y
        leax    512,x
        dec     <ft2
        bne     @l
@e      tst     <don
        beq     @x
        lbsr    nextblk
        bra     @blk
@x      rts

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
@go     ldb     <fy+1
        lbsr    mapy
        sta     <fp
        clr     <fp+1
        lda     <fv
        ldb     <fy+1
        lbsr    rowsel          ; U: fv's pattern on the line, pm its mask
        ldy     #vmasks+128     ; the pixels of a byte that are sky (or fire):
        lda     <fold           ;  indexed by the byte, signed (b,y)
        cmpa    #1
        beq     @t
        leay    256,y
@t      ldd     <fx1
        lsra
        rorb
        stb     <cnt            ; the last byte
        lda     #$FF
        bcs     @e1
        lda     #$F0            ; (x1 even: its left pixel only)
@e1     sta     <nm
        ldd     <fx
        lsra
        rorb
        stb     <cur            ; the first
        lda     #$FF
        bcc     @e0
        lda     #$0F            ; (x0 odd: its right pixel only)
@e0     sta     <ft2
        ldx     <fp
        abx
@b      ldb     <cur
        cmpb    <cnt
        bne     @m
        lda     <ft2
        anda    <nm
        sta     <ft2
@m      ldb     ,x
        lda     b,y             ; its pixels of the old value
        anda    <ft2
        beq     @n
        sta     <ft
        lda     <cur
        anda    <pm
        lda     a,u             ; the new value's pattern there
        eora    ,x
        anda    <ft
        eora    ,x
        sta     ,x
@n      lda     #$FF
        sta     <ft2
        leax    1,x
        inc     <cur
        lda     <cur
        cmpa    <cnt
        bls     @b
        rts

* fld_tables: vmasks: for each byte, its pixels that are sky; then those
* that are fire (src/coco/field.c's field_init, once). Each table is read
* from its middle with the byte as a signed offset: byte c at c + 128 mod 256
fld_tables
        clr     <ei
@l      ldy     #dec
        lda     <ei
        anda    #15
        lda     a,y
        sta     <ft             ; the right pixel's value
        lda     <ei
        lsra
        lsra
        lsra
        lsra
        lda     a,y
        sta     <ft2            ; the left's
        ldx     #vmasks
        ldb     <ei
        addb    #128
        abx
        lda     #1
        bsr     vmk
        sta     ,x
        lda     #3
        bsr     vmk
        sta     256,x
        inc     <ei
        bne     @l
        rts

* vmk: of the pixels ft2 (left) and ft (right), those that are value A
vmk     clrb
        cmpa    <ft2
        bne     @r
        orb     #$F0
@r      cmpa    <ft
        bne     @x
        orb     #$0F
@x      tfr     b,a
        rts

        section midbss
vmasks  rmb     512             ; sky's, fire's

        section mid
