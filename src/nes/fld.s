;
; fld.s -- the NES's field store: 256x168 pixels of sky (1), dirt (2) and
; fire (3), kept by cell (8x8: 21 rows of 32), for 2K of RAM and the
; cartridge's 8K (the Adam's two planes would be 10.75K).
;
; A cell's code (fmap) is 1, 2 or 3 when it is all sky, all dirt or all
; fire; else 4-255, a block of the pool: 8 bytes, a plane (a line a byte,
; pixel 0 in bit 7), and its kind (fkind):
;
;   K_DSKY   the plane is the dirt, the rest sky
;   K_FSKY   the plane is the fire, the rest sky
;   K_FDIRT  the plane is the fire, the rest dirt
;   K_3D     all three: the plane is the dirt, the next block (K_3F) the fire
;
; server/cmd/tilestat -pools measured the blocks a field needs at once over
; 377 scenes: 208 at most (median 88); the pool has 252. Should it run
; out, a cell is left all of one value (fld_lossy counts them).
;
; Every change goes through fld_load (a cell into the work planes wd, wf:
; dirt and fire, the rest sky) and fld_store (the planes back as the
; fewest blocks, the cell marked changed for the renderer while the field
; is shown). The routines take their arguments in zero page (fld.h; the
; Atari's), not on cc65's stack. The ellipse is drawn a band of 8 lines at
; a time: a cell inside on all 8 is set whole.
;
        .export _fld_init, _fld_clear, _fld_plot, _fld_get, _fld_hspan
        .export _fld_vrun, _fld_col, _fld_swap, _fld_ellipse
        .exportzp _fx, _fy, _fx1, _fv, _fent, _fold, _ex, _ey, _erx, _ery, _emode
        .export _fld_nd, _fld_live, _fld_dirty, _fld_rowd, _fld_lossy
        .export _fmap, _fpool, _fkind, _fld_blocks, fld_mark_all, _fld_mark_all
        .export fld_load, cellat, mark_below, _fld_touch
        .exportzp wd, wf, fcp, crow, ccol, code

FROWS   = 21
FLINES  = 168
NBLK    = 252                   ; codes 4-255
C_SKY   = 1
C_DIRT  = 2
C_FIRE  = 3
K_DSKY  = 0
K_FSKY  = 1
K_FDIRT = 2
K_3D    = 3
K_3F    = 4

        .segment "ZEROPAGE"
_fx:    .res 2          ; x (int16)
_fy:    .res 2          ; y (int16)
_fx1:   .res 2          ; a span's last x / a run's end y (exclusive)
_fv:    .res 1          ; the value, 1-3
_fold:  .res 1          ; fld_swap: the value to replace
_fent:  .res 2          ; fld_col: the column entry
_ex:    .res 2          ; fld_ellipse: the centre,
_ey:    .res 2
_erx:   .res 1          ; the radii,
_ery:   .res 1
_emode: .res 1          ; 0 all fv, 1 sky becomes fv, 2 fire becomes sky
wd:     .res 8          ; the cell in hand: its dirt,
wf:     .res 8          ; its fire (just after: wd,y for y 8-15 is wf)
fcp:    .res 2          ; its code's address
bp:     .res 2          ; a block's
crow:   .res 1          ; its row, column
ccol:   .res 1
ry:     .res 1          ; a pixel's line in its cell
rbit:   .res 1          ; and its bit
code:   .res 1
chg:    .res 1
t0:     .res 1
t1:     .res 1
t2:     .res 1
t3:     .res 1
mk:     .res 8          ; a band's 8 masks for one cell (the ellipse)
ea_d:   .res 1          ; (el_apply: a line's old dirt, fire)
ea_f:   .res 1
ea_m:   .res 1
el_k:   .res 1          ; (el_spans: the line)
el_a:   .res 2          ; (a span's ends, int16)
el_b:   .res 2
el_w:   .res 2          ; (the half width)
od6:    .res 1          ; the cell's last two lines of dirt as loaded (the
od7:    .res 1          ; cell under it shows them as its crust)

        .segment "BSS"
_fmap:  .res FROWS * 32
_fpool: .res NBLK * 8
_fkind: .res NBLK
bfree:  .res 32         ; a bit a code (pixel order): its block is free
_fld_dirty: .res FROWS * 4      ; a bit a cell (bit 7: the byte's first column)
_fld_rowd: .res FROWS   ; changed cells in a row
_fld_nd: .res 2         ; changed cells
_fld_live: .res 1       ; the field is shown: changes are marked
_fld_lossy: .res 2      ; cells left whole for want of a block
_fld_blocks: .res 1     ; blocks in use
bscan:  .res 1          ; where the next search for a free block starts
ecur:   .res 1          ; (fld_col)
ecnt:   .res 1
eleft:  .res 1
eys:    .res 1
el_y0:  .res 1          ; the ellipse's band: its first line
el_bot: .res 1          ; the ellipse's last field line
el_x0:  .res 8          ; the band's lines' spans (x0 > x1: none)
el_x1:  .res 8
el_in0: .res 1          ; the cells inside on all 8 lines: from,
el_in1: .res 1          ; to (in0 > in1: none)
el_c0:  .res 1          ; the cells touched: from,
el_c1:  .res 1          ; to
el_dy:  .res 1

        .segment "CODE2"        ; (R7's bank: always in, and the fixed banks are full)
; a quarter circle: q32[i] = 64 sqrt(1 - (i/32)^2) (the Atari's)
q32:    .byte   64, 64, 64, 63, 63, 62, 62, 61, 60, 59, 57, 56, 54, 53, 51, 49
        .byte   46, 44, 41, 38, 35, 32, 28, 24, 20, 15, 10, 5, 0, 0, 0, 0, 0
bitof:  .byte   $80, $40, $20, $10, $08, $04, $02, $01
lmask:  .byte   $FF, $7F, $3F, $1F, $0F, $07, $03, $01  ; from pixel n on
rmask:  .byte   $80, $C0, $E0, $F0, $F8, $FC, $FE, $FF  ; up to pixel n
; a row's codes, a code's block
rowlo:  .repeat FROWS, I
        .byte   <(_fmap + I * 32)
        .endrep
rowhi:  .repeat FROWS, I
        .byte   >(_fmap + I * 32)
        .endrep
blklo:  .repeat 256, I
        .byte   <(_fpool + (I - 4) * 8)
        .endrep
blkhi:  .repeat 256, I
        .byte   >(_fpool + (I - 4) * 8)
        .endrep
kindof = _fkind - 4             ; kindof,y: code y's kind

        .code

; ---- the store's own -----------------------------------------------------------

; void fld_init(void): no field yet (all sky, nothing shown)
_fld_init:
        lda     #0
        sta     _fld_live
        sta     _fld_lossy
        sta     _fld_lossy+1
        sta     _fld_nd
        sta     _fld_nd+1
        ldx     #FROWS * 4 - 1
@z:     sta     _fld_dirty,x
        dex
        bpl     @z
        ldx     #FROWS - 1
@zr:    sta     _fld_rowd,x
        dex
        bpl     @zr
        ; fall through

; void fld_clear(void): all sky (all of it changed, if shown)
_fld_clear:
        ldx     #31             ; every block free (codes 0-3 aren't blocks)
        lda     #$FF
@f:     sta     bfree,x
        dex
        bpl     @f
        lda     #$0F
        sta     bfree
        lda     #0
        sta     _fld_blocks
        sta     bscan
        tax
        lda     #C_SKY
@m:     sta     _fmap,x
        sta     _fmap+256,x
        inx
        bne     @m
@m2:    sta     _fmap+512,x
        inx
        cpx     #FROWS * 32 - 512
        bne     @m2
        ; fall through
; all cells changed (the field is drawn again)
_fld_mark_all:
fld_mark_all:
        lda     _fld_live
        beq     @x
        ldx     #FROWS * 4 - 1
        lda     #$FF
@d:     sta     _fld_dirty,x
        dex
        bpl     @d
        ldx     #FROWS - 1
        lda     #32
@r:     sta     _fld_rowd,x
        dex
        bpl     @r
        lda     #<(FROWS * 32)
        sta     _fld_nd
        lda     #>(FROWS * 32)
        sta     _fld_nd+1
@x:     rts

; cellat: the cell at row Y, column X: fcp its code's address, code (and A)
; the code
cellat: sty     crow
        stx     ccol
        txa
        clc
        adc     rowlo,y
        sta     fcp
        lda     rowhi,y
        adc     #0
        sta     fcp+1
        ldy     #0
        lda     (fcp),y
        sta     code
        rts

; fld_load: the cell at hand (cellat) into wd, wf (od6, od7 its last lines' dirt)
fld_load:
        jsr     load
        lda     wd+6
        sta     od6
        lda     wd+7
        sta     od7
        rts
load:   lda     code
        cmp     #4
        bcs     @blk
        ldx     #0              ; whole: (dirt, fire) = (0,0) (FF,0) (0,FF)
        ldy     #0
        cmp     #C_DIRT
        bne     @nd
        dex
@nd:    cmp     #C_FIRE
        bne     @nf
        dey
@nf:    txa
        .repeat 8, I
        sta     wd+I
        .endrep
        tya
        .repeat 8, I
        sta     wf+I
        .endrep
        rts
@blk:   tay
        lda     blklo,y
        sta     bp
        lda     blkhi,y
        sta     bp+1
        lda     kindof,y
        beq     @dsky
        cmp     #K_FSKY
        beq     @fsky
        cmp     #K_FDIRT
        beq     @fdirt
        ldy     #15             ; K_3D: dirt here, the fire in the next
@3d:    lda     (bp),y
        sta     wd,y
        dey
        bpl     @3d
        rts
@dsky:  ldy     #7
@dl:    lda     (bp),y
        sta     wd,y
        lda     #0
        sta     wf,y
        dey
        bpl     @dl
        rts
@fsky:  ldy     #7
@fl:    lda     (bp),y
        sta     wf,y
        lda     #0
        sta     wd,y
        dey
        bpl     @fl
        rts
@fdirt: ldy     #7
@fdl:   lda     (bp),y
        sta     wf,y
        eor     #$FF
        sta     wd,y
        dey
        bpl     @fdl
        rts

; ---- the pool ------------------------------------------------------------------

; free code A's block(s)
bfree_code:
        cmp     #4
        bcc     bf_x
        tay
        lda     kindof,y
        cmp     #K_3D
        bne     bfree_y
        iny
        jsr     bfree_y
        dey
; free code Y's block (Y kept)
bfree_y:
        tya
        lsr
        lsr
        lsr
        tax
        tya
        and     #7
        sty     t3
        tay
        lda     bitof,y
        ora     bfree,x
        sta     bfree,x
        ldy     t3
        dec     _fld_blocks
bf_x:   rts

; a free block: its code in A (carry set: none). The search goes on from
; where the last one ended.
balloc: ldx     bscan
        ldy     #32
@b:     lda     bfree,x
        bne     @have
        inx
        txa
        and     #31
        tax
        dey
        bne     @b
        sec
        rts
@have:  stx     bscan
        ldy     #0
@bit:   asl
        bcs     @got
        iny
        bne     @bit
@got:   lda     bitof,y         ; taken
        eor     #$FF
        and     bfree,x
        sta     bfree,x
        sty     t2
        txa
        asl
        asl
        asl
        ora     t2              ; x * 8 + bit
        inc     _fld_blocks
        clc
        rts

; carry set: code A's block is free (X kept)
isfree: stx     t3
        pha
        and     #7
        tay
        pla
        lsr
        lsr
        lsr
        tax
        lda     bitof,y
        and     bfree,x
        ldx     t3
        cmp     #1              ; (carry set if non-zero)
        rts

; take code A's block
take:   pha
        and     #7
        tay
        pla
        lsr
        lsr
        lsr
        tax
        lda     bitof,y
        eor     #$FF
        and     bfree,x
        sta     bfree,x
        inc     _fld_blocks
        rts

; two free blocks in a row: the first's code in A (carry set: none)
bpair:  ldx     #4
@p:     txa
        jsr     isfree
        bcc     @n
        inx
        beq     @none
        txa
        jsr     isfree
        bcs     @got
        bcc     @p
@n:     inx
        bne     @p
@none:  sec
        rts
@got:   dex                     ; x, x+1 free: both taken
        txa
        pha
        jsr     take
        pla
        pha
        clc
        adc     #1
        jsr     take
        pla
        clc
        rts

; ---- storing -------------------------------------------------------------------

; fld_store: wd, wf back into the cell at hand (code: its code so far), the
; cell marked changed (mark)
fld_store:
        ; what is there: any dirt, all dirt, any fire, all fire, any sky
        lda     #0
        sta     t0              ; or D
        sta     t2              ; or F
        sta     chg             ; or sky
        lda     #$FF
        sta     t1              ; and D
        sta     t3              ; and F
        ldx     #7
@w:     lda     wd,x
        ora     t0
        sta     t0
        lda     wd,x
        and     t1
        sta     t1
        lda     wf,x
        ora     t2
        sta     t2
        lda     wf,x
        and     t3
        sta     t3
        lda     wd,x
        ora     wf,x
        eor     #$FF
        ora     chg
        sta     chg
        dex
        bpl     @w
        lda     t2
        bne     @fire
        lda     t0              ; no fire
        bne     @d1
        lda     #C_SKY
        jmp     whole
@d1:    lda     t1
        cmp     #$FF
        bne     @d2
        lda     #C_DIRT
        jmp     whole
@d2:    lda     #K_DSKY
        jmp     one
@fire:  lda     t3
        cmp     #$FF
        bne     @f1
        lda     #C_FIRE
        jmp     whole
@f1:    lda     t0
        bne     @f2
        lda     #K_FSKY
        jmp     one
@f2:    lda     chg
        beq     @f3
        jmp     three
@f3:    lda     #K_FDIRT
        jmp     one

; the cell is all A
whole:  pha
        lda     code
        jsr     bfree_code
        pla
setcode:
        sta     code
        ldy     #0
        sta     (fcp),y
        lda     wd+6            ; its last lines' dirt changed: the cell
        cmp     od6             ; under it too (its crust)
        bne     @b
        lda     wd+7
        cmp     od7
        beq     @m
@b:     jsr     mark_below
@m:     jmp     mark

; void __fastcall__ fld_touch(uint16_t rc): cell (row X, column A) changed
; (a balloon gone: drawn from the store again)
_fld_touch:
        sta     ccol
        stx     crow
        jmp     mark

; mark the cell under the one at hand changed
mark_below:
        lda     crow
        cmp     #FROWS - 1
        bcs     @x
        inc     crow
        jsr     mark
        dec     crow
@x:     rts

; the cell is a block of kind A (its plane the dirt for K_DSKY, else the fire)
one:    sta     t0
        lda     code
        cmp     #4
        bcc     @new
        tay                     ; a block already: kept
        lda     kindof,y
        cmp     #K_3D
        bne     @fill
        iny                     ; (a pair: the second goes)
        jsr     bfree_y
        jmp     @fill
@new:   jsr     balloc
        bcc     @got
        jmp     lossy
@got:   sta     code
@fill:  ldy     code
        lda     t0
        sta     kindof,y
        lda     blklo,y
        sta     bp
        lda     blkhi,y
        sta     bp+1
        ldy     #7
        lda     t0
        bne     @f
@d:     lda     wd,y
        sta     (bp),y
        dey
        bpl     @d
        lda     code
        jmp     setcode
@f:     lda     wf,y
        sta     (bp),y
        dey
        bpl     @f
        lda     code
        jmp     setcode

; the cell has all three: a pair, dirt then fire
three:  lda     code
        cmp     #4
        bcc     @new
        tay
        lda     kindof,y
        cmp     #K_3D
        beq     @fill           ; a pair already
        cpy     #255            ; one block: its neighbour too?
        beq     @swap
        tya
        clc
        adc     #1
        jsr     isfree
        bcc     @swap
        lda     code            ; (isfree leaves its test in A)
        clc
        adc     #1
        jsr     take
        jmp     @fill
@swap:  ldy     code            ; no: a new pair, the old block let go
        jsr     bfree_y
        lda     #C_SKY          ; (nothing of its own, should none be free)
        sta     code
@new:   jsr     bpair
        bcc     @got
        jmp     lossy
@got:   sta     code
@fill:  ldy     code
        lda     #K_3D
        sta     kindof,y
        lda     #K_3F
        sta     kindof+1,y
        lda     blklo,y
        sta     bp
        lda     blkhi,y
        sta     bp+1
        ldy     #15             ; (wd then wf: the pair's 16 bytes)
@w:     lda     wd,y
        sta     (bp),y
        dey
        bpl     @w
        lda     code
        jmp     setcode

; no block free: the cell is all fire if any, else all dirt or sky by its
; middle (cosmetic, till it changes again)
lossy:  inc     _fld_lossy
        bne     @c
        inc     _fld_lossy+1
@c:     lda     t2
        beq     @nf
        lda     #C_FIRE
        jmp     whole
@nf:    lda     wd+4
        cmp     #$80
        lda     #C_SKY
        bcc     @s
        lda     #C_DIRT
@s:     jmp     whole

; mark the cell at hand changed
mark:   lda     _fld_live
        beq     @x
        lda     crow
        asl
        asl
        sta     t0
        lda     ccol
        lsr
        lsr
        lsr
        ora     t0
        tax
        lda     ccol
        and     #7
        tay
        lda     bitof,y
        and     _fld_dirty,x
        bne     @x
        lda     bitof,y
        ora     _fld_dirty,x
        sta     _fld_dirty,x
        ldx     crow
        inc     _fld_rowd,x
        inc     _fld_nd
        bne     @x
        inc     _fld_nd+1
@x:     rts

; ---- pixels ----------------------------------------------------------------

; in range? carry clear: the cell (cellat), ry, rbit
at:     lda     _fx+1
        bne     @out
        lda     _fy+1
        bne     @out
        lda     _fy
        cmp     #FLINES
        bcs     @out
        and     #7
        sta     ry
        lda     _fx
        and     #7
        tax
        lda     bitof,x
        sta     rbit
        lda     _fy
        lsr
        lsr
        lsr
        tay
        lda     _fx
        lsr
        lsr
        lsr
        tax
        jsr     cellat
        clc
        rts
@out:   sec
        rts

; the value at (ry, rbit) of the cell at hand: A
pixel:  lda     code
        cmp     #4
        bcc     @x              ; whole: its code is its value
        tay
        lda     blklo,y
        sta     bp
        lda     blkhi,y
        sta     bp+1
        lda     kindof,y
        sta     t0
        ldy     ry
        lda     (bp),y
        and     rbit
        sta     t1              ; the plane's bit
        lda     t0
        beq     @dsky
        cmp     #K_FSKY
        beq     @fsky
        cmp     #K_FDIRT
        beq     @fdirt
        lda     t1              ; K_3D: dirt? else the pair's fire
        bne     @dirt
        tya
        clc
        adc     #8
        tay
        lda     (bp),y
        and     rbit
        bne     @fire
        lda     #C_SKY
@x:     rts
@dsky:  lda     t1
        bne     @dirt
        lda     #C_SKY
        rts
@fsky:  lda     t1
        bne     @fire
        lda     #C_SKY
        rts
@fdirt: lda     t1
        bne     @fire
@dirt:  lda     #C_DIRT
        rts
@fire:  lda     #C_FIRE
        rts

; uint8_t fld_get(void): pixel (fx, fy); 1 (sky) off the field
_fld_get:
        jsr     at
        bcs     @sky
        jsr     pixel
        ldx     #0
        rts
@sky:   lda     #C_SKY
        ldx     #0
        rts

; void fld_plot(void): pixel (fx, fy) = fv
_fld_plot:
        jsr     at
        bcs     @x
        jsr     pixel
        cmp     _fv
        beq     @x
        jsr     fld_load
        ldx     ry
        lda     rbit
        jsr     setpx
        jmp     fld_store
@x:     rts

; line X's pixels in mask A become fv (wd, wf; X and Y kept)
setpx:  sta     t0
        eor     #$FF
        sta     t1
        lda     wd,x
        and     t1
        sta     wd,x
        lda     wf,x
        and     t1
        sta     wf,x
        lda     _fv
        cmp     #C_DIRT
        bne     @nd
        lda     wd,x
        ora     t0
        sta     wd,x
        rts
@nd:    cmp     #C_FIRE
        bne     @x
        lda     wf,x
        ora     t0
        sta     wf,x
@x:     rts

; void fld_swap(void): pixel (fx, fy) = fv if it is fold
_fld_swap:
        jsr     at
        bcs     @x
        jsr     pixel
        cmp     _fold
        bne     @x
        cmp     _fv
        beq     @x
        jsr     fld_load
        ldx     ry
        lda     rbit
        jsr     setpx
        jmp     fld_store
@x:     rts

; ---- spans and runs ---------------------------------------------------------

hs_out: rts
; void fld_hspan(void): line fy, x from fx to fx1 (both included) = fv,
; clipped to the field
_fld_hspan:
        lda     _fy+1
        bne     hs_out
        lda     _fy
        cmp     #FLINES
        bcs     hs_out
        lda     _fx+1           ; clip x0 to 0, x1 to 255
        bpl     @c0
        lda     #0
        sta     _fx
        sta     _fx+1
@c0:    bne     hs_out          ; x0 >= 256
        lda     _fx1+1
        bmi     hs_out          ; x1 < 0
        beq     @c1
        lda     #255
        sta     _fx1
        lda     #0
        sta     _fx1+1
@c1:    lda     _fx1
        cmp     _fx
        bcc     @x              ; x0 > x1
        lda     _fy
        and     #7
        sta     ry
        lda     _fx
        lsr
        lsr
        lsr
        sta     t2              ; the first cell
        lda     _fx1
        lsr
        lsr
        lsr
        sta     t3              ; the last
@cell:  lda     t2              ; this cell's mask: its first x
        asl
        asl
        asl
        sta     t0
        lda     #$FF
        sta     t1
        lda     _fx             ; from fx's pixel, if fx is in it
        cmp     t0
        bcc     @m1
        and     #7
        tax
        lda     lmask,x
        sta     t1
@m1:    lda     t0              ; up to fx1's, if it is in it
        clc
        adc     #7
        cmp     _fx1
        bcc     @m2
        beq     @m2
        lda     _fx1
        and     #7
        tax
        lda     rmask,x
        and     t1
        sta     t1
@m2:    lda     _fy
        lsr
        lsr
        lsr
        tay
        ldx     t2
        jsr     cellat
        lda     code            ; all fv already: nothing
        cmp     _fv
        beq     @next
        jsr     fld_load
        ldx     ry
        lda     t1
        jsr     setpx
        lda     t2              ; (store takes t0-t3)
        pha
        lda     t3
        pha
        jsr     fld_store
        pla
        sta     t3
        pla
        sta     t2
@next:  lda     t2
        cmp     t3
        beq     @x
        inc     t2
        bne     @cell
@x:     rts

; rows t2..t3-1 of column fx = fv (all in the field)
vrows:  lda     _fx
        and     #7
        tax
        lda     bitof,x
        sta     rbit
@cell:  lda     t2
        cmp     t3
        bcs     @x
        lsr
        lsr
        lsr
        tay
        lda     _fx
        lsr
        lsr
        lsr
        tax
        jsr     cellat
        lda     t2              ; this cell's lines of the run: from
        and     #7
        sta     t0
        lda     t2
        ora     #7
        clc
        adc     #1              ; the next cell's first line
        cmp     t3
        bcc     @to
        lda     t3
@to:    sta     t1              ; to (exclusive)
        lda     code            ; all fv already: nothing
        cmp     _fv
        beq     @next
        jsr     fld_load
        lda     t1              ; (to - t2) lines from line t0
        sec
        sbc     t2
        tay
        ldx     t0
@l:     lda     rbit
        jsr     setpx           ; (X, Y kept)
        inx
        dey
        bne     @l
        lda     t2
        pha
        lda     t3
        pha
        jsr     fld_store
        pla
        sta     t3
        pla
        sta     t2
@next:  lda     t2              ; on to the next cell's first line
        ora     #7
        clc
        adc     #1
        sta     t2
        jmp     @cell
@x:     rts

; void fld_vrun(void): column fx, rows fy to fx1-1 = fv (all in the field)
_fld_vrun:
        lda     _fy
        sta     t2
        lda     _fx1
        sta     t3
        jmp     vrows

; void fld_col(void): column fx becomes the TERRAIN entry at fent (sky,
; dirt); returns the entry's length in A
_fld_col:
        lda     _fv
        pha
        lda     #0
        sta     ecur
        ldy     #0
        lda     (_fent),y
        cmp     #$F0
        beq     @empty
        bcs     @spans
        ; one plain column: sky down to its top, dirt from there
        sta     ecnt
        lda     #C_SKY
        jsr     erun
        lda     #FLINES
        sta     ecnt
        lda     #C_DIRT
        jsr     erun
        lda     #1
        jmp     @r
@empty: lda     #FLINES
        sta     ecnt
        lda     #C_SKY
        jsr     erun
        lda     #1
        jmp     @r
@spans: and     #$0F
        sta     eleft           ; spans left
        asl
        sec
        adc     #0              ; 1 + 2n: the entry's length
        pha
        ldy     #1
@sp:    sty     eys
        lda     (_fent),y       ; top: sky till there
        sta     ecnt
        lda     #C_SKY
        jsr     erun
        ldy     eys
        lda     (_fent),y
        sta     ecur
        iny
        lda     (_fent),y       ; end: dirt till there
        sta     ecnt
        lda     #C_DIRT
        jsr     erun
        ldy     eys
        iny
        iny
        dec     eleft
        bne     @sp
        lda     #FLINES
        sta     ecnt
        lda     #C_SKY
        jsr     erun
        pla
@r:     tax                     ; (the length, while fv comes back)
        pla
        sta     _fv
        txa
        ldx     #0
        rts

; rows ecur..ecnt-1 of column fx = A; ecur = ecnt after
erun:   sta     _fv
        lda     ecnt
        cmp     #FLINES+1
        bcc     @ok
        lda     #FLINES
        sta     ecnt
@ok:    lda     ecur
        sta     t2
        lda     ecnt
        sta     t3
        jsr     vrows
        lda     ecnt
        sta     ecur
        rts

; ---- the ellipse ------------------------------------------------------------

; void fld_ellipse(void): the ellipse at (ex, ey), radii erx, ery: the
; Atari's (src/atari/fld.s): line ey +- dy is ex - hw .. ex + hw, hw =
; (erx * q32[i] + 32) >> 6, i = min(32, (ery/2 + 32 dy) / ery). Mode 0: all
; fv; 1: sky becomes fv; 2: fire becomes sky. A band of 8 lines at a time:
; each line's span, then each cell the band touches, once.
_fld_ellipse:
        lda     _ery
        bne     @r
        inc     _ery
@r:     sec                     ; the first line: ey - ery, from 0
        lda     _ey
        sbc     _ery
        sta     el_a
        lda     _ey+1
        sbc     #0
        bmi     @t0
        bne     @x              ; below the field
        lda     el_a
        cmp     #FLINES
        bcs     @x
        bcc     @t1
@t0:    lda     #0
@t1:    and     #$F8
        sta     el_y0
        clc                     ; the last: ey + ery, to 167
        lda     _ey
        adc     _ery
        sta     el_a
        lda     _ey+1
        adc     #0
        bmi     @x              ; above the field
        bne     @b167
        lda     el_a
        cmp     #FLINES
        bcc     @b
@b167:  lda     #FLINES - 1
@b:     sta     el_bot
@band:  jsr     el_spans
        bcs     @nb             ; (no line of it in the ellipse)
        jsr     el_cells
@nb:    lda     el_y0
        clc
        adc     #8
        sta     el_y0
        cmp     el_bot
        bcc     @band
        beq     @band
@x:     rts

; el_spans: lines el_y0 .. +7: their spans (clipped to 0-255) in el_x0/x1
; (x0 > x1: none), the cells touched (el_c0..el_c1) and those inside on all
; 8 lines (el_in0..el_in1; in0 > in1: none). Carry set: no line has a span.
el_spans:
        lda     #32
        sta     el_c0
        lda     #0
        sta     el_in0
        sta     el_c1
        sta     el_k
        lda     #31
        sta     el_in1
        lda     #1
        sta     t1              ; (no span yet)
@line:  lda     el_y0           ; dy = |y - ey|, y = el_y0 + k
        clc
        adc     el_k
        sec
        sbc     _ey
        sta     el_a
        lda     #0
        sbc     _ey+1
        sta     el_a+1
        bpl     @pos
        sec                     ; negative: ey - y
        lda     #0
        sbc     el_a
        sta     el_a
        lda     #0
        sbc     el_a+1
        sta     el_a+1
@pos:   lda     el_a+1
        beq     @lo
@far:   jmp     @none           ; dy > 255
@lo:    lda     el_a
        cmp     _ery
        beq     @in
        bcs     @far            ; dy > ery
@in:    sta     el_dy
        jsr     el_half
        sec                     ; x0 = ex - hw
        lda     _ex
        sbc     el_w
        sta     el_a
        lda     _ex+1
        sbc     el_w+1
        sta     el_a+1
        clc                     ; x1 = ex + hw
        lda     _ex
        adc     el_w
        sta     el_b
        lda     _ex+1
        adc     el_w+1
        sta     el_b+1
        bmi     @none           ; x1 < 0
        lda     el_a+1
        bmi     @x0z
        bne     @none           ; x0 > 255
        beq     @x1
@x0z:   lda     #0
        sta     el_a
@x1:    lda     el_b+1
        beq     @keep
        lda     #255
        sta     el_b
@keep:  ldx     el_k
        lda     el_a
        sta     el_x0,x
        lda     el_b
        sta     el_x1,x
        lda     #0
        sta     t1              ; (a span)
        lda     el_a            ; touched: c0 = min(x0 / 8), c1 = max(x1 / 8)
        lsr
        lsr
        lsr
        cmp     el_c0
        bcs     @c0
        sta     el_c0
@c0:    lda     el_b
        lsr
        lsr
        lsr
        cmp     el_c1
        bcc     @c1
        sta     el_c1
@c1:    lda     el_a            ; inside: in0 = max((x0 + 7) / 8)
        clc
        adc     #7
        bcc     @i0a
        lda     #32             ; (x0 > 248: no whole cell)
        bne     @i0
@i0a:   lsr
        lsr
        lsr
@i0:    cmp     el_in0
        bcc     @i1
        sta     el_in0
@i1:    lda     el_b            ; in1 = min((x1 + 1) / 8 - 1)
        clc
        adc     #1
        bcs     @l31            ; (x1 = 255: 31)
        lsr
        lsr
        lsr
        sec
        sbc     #1
        bcs     @i1c
        lda     #33             ; (none whole)
        sta     el_in0
        bne     @next
@l31:   lda     #31
@i1c:   cmp     el_in1
        bcs     @next
        sta     el_in1
        jmp     @next
@none:  ldx     el_k            ; no span on this line
        lda     #1
        sta     el_x0,x
        lda     #0
        sta     el_x1,x
        lda     #33             ; (so no cell is inside on all 8)
        sta     el_in0
@next:  inc     el_k
        lda     el_k
        cmp     #8
        bcs     @done
        jmp     @line
@done:  lda     t1              ; carry set if no span at all
        lsr
        rts

; el_half: el_w = the half width at dy el_dy: i = min(32, (ery/2 + 32 dy)
; / ery), hw = (erx * q32[i] + 32) >> 6
el_half:
        lda     #0              ; n = 32 dy + ery/2 (16-bit)
        sta     el_w+1
        lda     el_dy
        asl
        rol     el_w+1
        asl
        rol     el_w+1
        asl
        rol     el_w+1
        asl
        rol     el_w+1
        asl
        rol     el_w+1
        sta     el_w
        lda     _ery
        lsr
        clc
        adc     el_w
        sta     el_w
        bcc     @dv
        inc     el_w+1
@dv:    ldy     #0              ; i = n / ery, at most 32
@d:     lda     el_w
        sec
        sbc     _ery
        tax
        lda     el_w+1
        sbc     #0
        bcc     @q
        sta     el_w+1
        stx     el_w
        iny
        cpy     #32
        bcc     @d
@q:     lda     q32,y           ; hw = (erx * q32[i] + 32) >> 6
        sta     t3
        lda     #0
        sta     el_w
        sta     el_w+1
        lda     _erx
        sta     t2
        ldx     #8
@m:     lsr     t2
        bcc     @ms
        clc
        lda     el_w+1
        adc     t3
        sta     el_w+1
@ms:    ror     el_w+1
        ror     el_w
        dex
        bne     @m
        clc
        lda     el_w
        adc     #32
        sta     el_w
        bcc     @sh
        inc     el_w+1
@sh:    ldx     #6
@sd:    lsr     el_w+1
        ror     el_w
        dex
        bne     @sd
        rts

; el_cells: the band's cells, each once: masks from the 8 spans, then the
; mode's change
el_cells:
        lda     el_c0
        sta     t2              ; the cell column
@cell:  lda     el_c1
        cmp     t2
        bcs     @go
        rts
@go:    lda     t2              ; inside on all 8 lines: whole
        cmp     el_in0
        bcc     @masks
        lda     el_in1
        cmp     t2
        bcc     @masks
        jsr     el_whole
        jmp     @next
@masks: lda     t2              ; this cell's x range: 8c .. 8c+7
        asl
        asl
        asl
        sta     t3
        lda     #0
        sta     t1              ; (any mask)
        ldx     #7
@mk:    lda     #0
        sta     mk,x
        lda     el_x1,x         ; none (x0 > x1), or x1 < 8c: none
        cmp     el_x0,x
        bcc     @mn
        cmp     t3
        bcc     @mn
        lda     t3              ; 8c+7 < x0: none
        clc
        adc     #7
        cmp     el_x0,x
        bcc     @mn
        lda     #$FF
        sta     mk,x
        lda     el_x0,x         ; from x0's pixel, if in this cell
        cmp     t3
        bcc     @r
        and     #7
        tay
        lda     lmask,y
        sta     mk,x
@r:     lda     t3              ; up to x1's, if in this cell
        clc
        adc     #7
        cmp     el_x1,x
        bcc     @mo
        beq     @mo
        lda     el_x1,x
        and     #7
        tay
        lda     rmask,y
        and     mk,x
        sta     mk,x
@mo:    lda     mk,x
        ora     t1
        sta     t1
@mn:    dex
        bpl     @mk
        lda     t1
        beq     @next
        lda     el_y0
        lsr
        lsr
        lsr
        tay
        ldx     t2
        jsr     cellat
        jsr     el_skip         ; (carry: the mode changes nothing here)
        bcs     @next
        jsr     fld_load
        jsr     el_apply
        lda     chg
        beq     @next
        lda     t2
        pha
        jsr     fld_store
        pla
        sta     t2
@next:  inc     t2
        lda     t2
        cmp     #32
        bcs     @x
        jmp     @cell
@x:     rts

; el_skip: carry set if the mode can't change a whole cell of this code
el_skip:
        lda     code
        cmp     #4
        bcs     @no             ; a block: look
        ldx     _emode
        beq     @m0
        cpx     #1
        beq     @m1
        cmp     #C_FIRE         ; mode 2: only fire changes
        bne     @yes
        beq     @no
@m1:    cmp     #C_SKY          ; mode 1: only sky changes
        bne     @yes
        cmp     _fv
        beq     @yes
        bne     @no
@m0:    cmp     _fv
        beq     @yes
@no:    clc
        rts
@yes:   sec
        rts

; el_apply: the masks mk on wd, wf by the mode; chg non-zero if a pixel changed
el_apply:
        lda     #0
        sta     chg
        ldx     #7
@l:     lda     mk,x
        beq     @n
        ldy     wd,x
        sty     ea_d
        ldy     wf,x
        sty     ea_f
        ldy     _emode
        beq     @m0
        cpy     #1
        beq     @m1
        eor     #$FF            ; mode 2: fire in the mask becomes sky
        and     wf,x
        sta     wf,x
        jmp     @cmp
@m1:    sta     ea_m            ; mode 1: sky in the mask becomes fv
        lda     wd,x
        ora     wf,x
        eor     #$FF
        and     ea_m
@m0:    jsr     setpx
@cmp:   lda     wd,x
        eor     ea_d
        ora     chg
        sta     chg
        lda     wf,x
        eor     ea_f
        ora     chg
        sta     chg
@n:     dex
        bpl     @l
        rts

; el_whole: cell t2 of the band, inside on all 8 lines: the mode on all of it
el_whole:
        lda     el_y0
        lsr
        lsr
        lsr
        tay
        ldx     t2
        jsr     cellat
        jsr     el_skip
        bcs     @x
        lda     _emode
        bne     @px
        lda     t2              ; mode 0: all fv
        pha
        jsr     mark_below      ; (its crust: not known, so drawn again)
        lda     code
        jsr     bfree_code
        lda     _fv
        jsr     setcode
        pla
        sta     t2
@x:     rts
@px:    ldx     #7
        lda     #$FF
@m:     sta     mk,x
        dex
        bpl     @m
        jsr     fld_load
        jsr     el_apply
        lda     chg
        beq     @x
        lda     t2
        pha
        jsr     fld_store
        pla
        sta     t2
        rts
