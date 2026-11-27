;
; hud.s -- the NES HUD's letters (src/nes/fieldo.c's status, bank 7): a
; string into a HUD row, a cell at a time, drawn only where the cell shows
; something else (hud_sh: what each shows). A cell is its own tile in the
; HUD's CHR bank (band 7: bank 14, tile row * 32 + col), the Namco font's
; glyph in an ink (1 white, 2 a label's yellow, 3 a name's: its seat's
; colour in the row's palette) or ink 1 inverse; queued (crt0.s's Q_TILE)
; or, while the display is off (ppu_direct), written at once.
;
; hud_sh's key: the glyph (0-63, ' ' to '_'), bit 6 inverse, bit 7 ink 2,
; both ink 3; $FF none (an inverse in ink 2 or 3: drawn every time).
;
; void __fastcall__ hud_put(const char *s)  at hp_row, hp_col (on, past
;         the string), inverse if hp_inv; a character's bit 7: a label's;
;         the cells nm_col[row] .. + nm_n[row] in ink 3 (field_name)
;
; The gauges (the SMS's): bars on the last line of the cells under the
; name (energy, in the name's ink: columns 0-8, 72 pixels for 100), under
; POW (power, yellow: columns 17-24, 64 for 1000) and under WIND (white:
; columns 18-21 of row 1, from the middle toward its side, half its
; strength, 16 at most). Their numbers are read back from the HUD's own
; cells (row 0's columns 28-30 and 21-24, row 1's 23 and 24-26: layout.h's
; 32-column HUD); when a bar's length changes its cells are drawn again.
;
        .export _hud_put, _hp_row, _hp_col, _hp_inv, _hud_gauges_reset
        .import _font8, _hud_sh, _nm_col, _nm_n, _ppuq, _mmc3_chr
        .importzp _qhead, _qtail, _ppu_direct

HUD_BANK = 14                   ; (CHR_BAND(7))

        .segment "ZEROPAGE"
hs:     .res 2                  ; the string
gp:     .res 2                  ; the glyph's 8 bytes
g_e:    .res 1                  ; the gauges: energy's length (pixels),
g_p:    .res 1                  ; power's,
g_wlo:  .res 1                  ; the wind's ends (16 the middle)
g_whi:  .res 1
bar:    .res 1                  ; the cell in hand's bar (its last line)
bink:   .res 1                  ; and its ink
gn:     .res 2                  ; (a number read back,
gt:     .res 1                  ; a digit,
g2:     .res 2                  ; twice the number)
s_lo:   .res 1                  ; (span: the bar's first pixel,
s_hi:   .res 1                  ; past its last,
s_px:   .res 1                  ; the pixel in hand, its bit)
s_bit:  .res 1
n_e:    .res 1                  ; (the gauges as the numbers say now)
n_p:    .res 1
n_wlo:  .res 1
n_whi:  .res 1

        .segment "BSS"
_hp_row: .res 1
_hp_col: .res 1
_hp_inv: .res 1
hc:     .res 1
hg:     .res 1
hink:   .res 1
hk:     .res 1
ht:     .res 1                  ; the cell's tile (row * 32 + col)
invm:   .res 1
m0:     .res 1
m1:     .res 1

        .segment "FOPEN"

_hud_put:
        sta     hs
        stx     hs+1
@ch:    ldx     _hp_col
        cpx     #32
        bcc     @c1
@x:     lda     _hp_row         ; rows 0, 1: the gauges' numbers
        cmp     #2
        bcs     @x2
        jmp     gauges
@x2:    rts
@c1:    ldy     #0
        lda     (hs),y
        beq     @x
        sta     hc
        inc     hs
        bne     @ink
        inc     hs+1
@ink:   ldy     _hp_row         ; ink 3 in the row's name
        txa
        sec
        sbc     _nm_col,y
        cmp     _nm_n,y
        bcs     @ni
        lda     #3
        bne     @in
@ni:    lda     hc              ; else a label's 2, or 1
        bmi     @lab
        lda     #1
        bne     @in
@lab:   lda     #2
@in:    sta     hink
        lda     hc              ; the glyph: ' ' to '_' (a-z as A-Z)
        and     #$7F
        cmp     #'a'
        bcc     @u
        cmp     #'z'+1
        bcs     @u
        sbc     #31             ; (carry clear: less 32)
@u:     cmp     #' '
        bcc     @sp
        cmp     #'_'+1
        bcc     @g
@sp:    lda     #' '
@g:     sec
        sbc     #' '
        sta     hg
        ldx     hink            ; its key
        lda     _hp_inv
        beq     @kn
        cpx     #1
        bne     @kff            ; (inverse, not ink 1: drawn every time)
        lda     hg
        ora     #$40
        bne     @k
@kn:    lda     hg
        cpx     #1
        beq     @k
        ora     #$80
        cpx     #2
        beq     @k
        ora     #$40
        bne     @k
@kff:   lda     #$FF
@k:     sta     hk
        lda     _hp_row         ; the cell: row * 32 + col
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        ora     _hp_col
        sta     ht
        tay
        lda     hk
        cmp     #$FF
        beq     @dr
        cmp     _hud_sh,y
        beq     @nx
@dr:    sta     _hud_sh,y
        jsr     cell
@nx:    inc     _hp_col
        jmp     @ch

; cell: tile ht, glyph hg in ink hink (inverse: hp_inv), with its gauge's bar
cell:   jsr     barof
        lda     hg              ; gp = font8 + 8 hg
        asl     a
        asl     a
        asl     a
        sta     gp
        lda     hg
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        sta     gp+1
        lda     gp
        clc
        adc     #<_font8
        sta     gp
        lda     gp+1
        adc     #>_font8
        sta     gp+1
        lda     #0              ; each plane: the glyph's bytes or none,
        ldx     _hp_inv         ; inverted for an inverse
        beq     @ni
        lda     #$FF
@ni:    sta     invm
        lda     hink
        and     #1
        beq     @p0
        lda     #$FF
@p0:    sta     m0
        lda     hink
        and     #2
        beq     @p1
        lda     #$FF
@p1:    sta     m1
        lda     _ppu_direct
        beq     @w
        jmp     direct
@w:     lda     _qtail          ; room for a tile: 20 bytes
        clc
        sbc     _qhead
        cmp     #20
        bcc     @w
        ldx     _qhead
        lda     #0              ; Q_TILE
        sta     _ppuq,x
        inx
        lda     #HUD_BANK
        sta     _ppuq,x
        inx
        lda     ht
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        sta     _ppuq,x
        inx
        lda     ht
        asl     a
        asl     a
        asl     a
        asl     a
        sta     _ppuq,x
        inx
        ldy     #0
@q0:    jsr     line
        and     m0
        sta     _ppuq,x
        inx
        iny
        cpy     #8
        bne     @q0
        ldy     #0
@q1:    jsr     line
        and     m1
        sta     _ppuq,x
        inx
        iny
        cpy     #8
        bne     @q1
        dex                     ; (the bar: line 7 of each plane, in its ink)
        jsr     barp1
        txa
        sec
        sbc     #8
        tax
        jsr     barp0
        txa
        clc
        adc     #9
        tax
        stx     _qhead          ; (last: the drain reads up to it)
        rts

; line: the glyph's line Y, inverted for an inverse
line:   lda     (gp),y
        eor     invm
        rts

; barp0/barp1: the bar into plane 0's / 1's last line (queue byte X), if
; its ink has that plane
barp0:  lda     bar
        beq     @x
        lda     bink
        and     #1
        beq     @x
        lda     _ppuq,x
        ora     bar
        sta     _ppuq,x
@x:     rts
barp1:  lda     bar
        beq     @x
        lda     bink
        and     #2
        beq     @x
        lda     _ppuq,x
        ora     bar
        sta     _ppuq,x
@x:     rts

; the display off: straight into CHR-RAM, the HUD's bank in R0
direct: lda     #0
        ldx     #HUD_BANK
        jsr     _mmc3_chr
        bit     $2002
        lda     ht
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        sta     $2006
        lda     ht
        asl     a
        asl     a
        asl     a
        asl     a
        sta     $2006
        ldy     #0
@d0:    jsr     line
        and     m0
        cpy     #7
        bne     @w0
        jsr     barb0
@w0:    sta     $2007
        iny
        cpy     #8
        bne     @d0
        ldy     #0
@d1:    jsr     line
        and     m1
        cpy     #7
        bne     @w1
        jsr     barb1
@w1:    sta     $2007
        iny
        cpy     #8
        bne     @d1
        rts

; barb0/barb1: A with the bar, if its ink has plane 0 / 1 (A otherwise kept)
barb0:  pha
        lda     bink
        and     #1
        bne     bb
        pla
        rts
barb1:  pha
        lda     bink
        and     #2
        bne     bb
        pla
        rts
bb:     pla
        ora     bar
        rts

; ---- the gauges ----------------------------------------------------------

; barof: bar, bink for cell ht: the bar's pixels on its last line (0: none)
barof:  lda     #0
        sta     bar
        lda     ht
        cmp     #9              ; row 0, columns 0-8: energy from 0
        bcs     @p
        asl     a
        asl     a
        asl     a
        tax                     ; (the cell's first pixel)
        lda     #0
        ldy     g_e
        jsr     span
        lda     #3              ; (the name's ink)
        bne     @set
@p:     cmp     #17             ; columns 17-24: power from 0
        bcc     @x
        cmp     #25
        bcs     @w
        sec
        sbc     #17
        asl     a
        asl     a
        asl     a
        tax
        lda     #0
        ldy     g_p
        jsr     span
        lda     #2              ; (yellow)
        bne     @set
@w:     cmp     #32 + 18        ; row 1, columns 18-21: the wind
        bcc     @x
        cmp     #32 + 22
        bcs     @x
        sec
        sbc     #32 + 18
        asl     a
        asl     a
        asl     a
        tax
        lda     g_wlo
        ldy     g_whi
        jsr     span
        lda     #1              ; (white)
@set:   sta     bink
@x:     rts

; span: bar = the bits of the cell whose first pixel is X for the pixels
; A .. Y-1
span:   sta     s_lo
        sty     s_hi
        stx     s_px
        lda     #$80
        sta     s_bit
        lda     #0
        sta     bar
        ldx     #8
@px:    lda     s_px
        cmp     s_hi
        bcs     @n              ; (at or past the end)
        cmp     s_lo
        bcc     @n              ; (before the start)
        lda     bar
        ora     s_bit
        sta     bar
@n:     inc     s_px
        lsr     s_bit
        dex
        bne     @px
        rts

; void hud_gauges_reset(void): the HUD blanked (field_open): the bars all
; drawn again at the next numbers
_hud_gauges_reset:
        lda     #$FF
        sta     g_e
        sta     g_p
        sta     g_wlo
        sta     g_whi
        rts

; gauges: the bars' lengths from the HUD's numbers; their cells drawn
; again if one changed
gauges: lda     #28             ; energy: row 0, columns 28-30 (100 at most)
        jsr     num
        lda     gn+1
        bne     @e100
        lda     gn
        cmp     #101
        bcc     @e
@e100:  lda     #100
@e:     tax
        lda     etab,x
        sta     n_e
        lda     #21             ; power: columns 21-24, 64 pixels for 1000:
        jsr     num             ; p / 16 + p / 512
        lda     gn+1
        lsr     a
        sta     gt              ; (p / 512)
        lda     gn
        ldx     #4
@p16:   lsr     gn+1
        ror     a
        dex
        bne     @p16
        clc
        adc     gt
        ldx     gn+1
        bne     @p64
        cmp     #65
        bcc     @p
@p64:   lda     #64
@p:     sta     n_p
        lda     #32 + 24        ; the wind: row 1, its strength in 24-26,
        jsr     num             ; half of it (16 at most) from the middle
        lda     gn+1
        bne     @w16
        lda     gn
        clc
        adc     #1
        lsr     a
        cmp     #17
        bcc     @w
@w16:   lda     #16
@w:     sta     gt
        lda     #16
        sta     n_wlo
        sta     n_whi
        lda     _hud_sh + 32 + 23       ; its side: '<' (28) or '>' (30)
        and     #$3F
        cmp     #'<' - ' '
        bne     @wr
        lda     #16
        sec
        sbc     gt
        sta     n_wlo
        jmp     @cmp
@wr:    cmp     #'>' - ' '
        bne     @cmp
        lda     #16
        clc
        adc     gt
        sta     n_whi
@cmp:   ldx     #3              ; changed?
@c:     lda     n_e,x
        cmp     g_e,x
        bne     @new
        dex
        bpl     @c
        rts
@new:   ldx     #3
@cp:    lda     n_e,x
        sta     g_e,x
        dex
        bpl     @cp
        ldx     #0              ; the gauges' cells again
@r:     lda     gcells,x
        bmi     @x
        stx     gt
        jsr     again
        ldx     gt
        inx
        bne     @r
@x:     rts

; again: HUD cell A drawn from its shadow (an inverse in ink 2 or 3: not)
again:  sta     ht
        tay
        lda     _hud_sh,y
        cmp     #$FF
        beq     @x
        pha
        and     #$3F
        sta     hg
        pla
        ldx     #0
        stx     _hp_inv
        asl     a               ; bits 7, 6: 00 ink 1, 01 inverse, 10 ink 2, 11 ink 3
        bcc     @i1
        asl     a
        lda     #2
        adc     #0              ; (2, or 3 with bit 6)
        bne     @ink
@i1:    asl     a
        lda     #0
        rol     a
        sta     _hp_inv
        lda     #1
@ink:   sta     hink
        jmp     cell
@x:     rts

; num: gn = the decimal number in the HUD's cells from A (4 digits at most)
num:    tax
        lda     #0
        sta     gn
        sta     gn+1
        ldy     #4
@d:     lda     _hud_sh,x
        and     #$3F
        sec
        sbc     #'0' - ' '
        bcc     @x
        cmp     #10
        bcs     @x
        sta     gt
        asl     gn              ; gn = 10 gn + the digit
        rol     gn+1
        lda     gn
        sta     g2
        lda     gn+1
        sta     g2+1
        asl     gn
        rol     gn+1
        asl     gn
        rol     gn+1
        lda     gn
        clc
        adc     g2
        sta     gn
        lda     gn+1
        adc     g2+1
        sta     gn+1
        lda     gn
        clc
        adc     gt
        sta     gn
        bcc     @n
        inc     gn+1
@n:     inx
        dey
        bne     @d
@x:     rts

; the gauges' cells (row * 32 + column), $FF the end
gcells: .byte   0, 1, 2, 3, 4, 5, 6, 7, 8, 17, 18, 19, 20, 21, 22, 23, 24
        .byte   32 + 18, 32 + 19, 32 + 20, 32 + 21, $FF
; energy's bar for 0-100: 72 pixels for 100 (e x 23 / 32)
etab:   .repeat 101, I
        .byte   (I * 23) / 32
        .endrep
