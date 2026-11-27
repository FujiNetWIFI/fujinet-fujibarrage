;
; render.s -- the field's changed cells onto the screen (src/nes/fld.s marks
; them): each cell its own tile in its band's CHR bank (crt0.s), drawn
; through the PPU queue.
;
; A cell's pixels are colours: sky 1, dirt 2, fire 3, and 0 the crust (a
; dirt pixel with sky or fire one or two lines over it: the backdrop, the
; round's crust colour). A cell all sky, all fire, or all dirt with dirt
; over it is a constant tile of its band (T_*; a starry sky's cells some of
; the star tiles): its name byte only. Else its own tile (plane 0 = fire |
; not dirt, plane 1 = fire | dirt but the crust), then its name byte.
;
; The cells are taken in turn from the last one drawn, not from the top
; (the SMS's lesson: a fireball's cells, marked again every frame it grows,
; kept the rest of the field waiting). A cell under a talk balloon
; (bln_cells) is the balloon's: it is let be.
;
; uint8_t fld_flush1(void): a changed cell drawn (or let be); 0 if there was
; none, or no room in the queue for one
;
        .export _fld_flush1, _rnd_stars, _bln_cells
        .import fld_load, cellat
        .importzp wd, wf, code
        .import _fld_dirty, _fld_rowd, _fld_nd
        .importzp _qhead, _qtail
        .import _ppuq

FROWS   = 21
NT_FIELD = 4
T_SKY   = 96
T_DIRT  = 97
T_FIRE  = 99
T_STAR  = 100                   ; 100-107: a star each
C_SKY   = 1
C_DIRT  = 2
C_FIRE  = 3
ROOM    = 25                    ; a cell's command: 24 bytes

        .segment "ZEROPAGE"
rr:     .res 1                  ; the cell drawn
rc:     .res 1
a6:     .res 1                  ; its upper neighbour's last two lines of dirt
a7:     .res 1
rt:     .res 1
rn:     .res 1

        .segment "BSS"
_rnd_stars: .res 1              ; the round's sky has stars
_bln_cells: .res FROWS * 4      ; a bit a cell: a balloon's
nx_r:   .res 1                  ; where the next search starts
nx_c:   .res 1
nx_n:   .res 1

        .rodata
bitof:  .byte   $80, $40, $20, $10, $08, $04, $02, $01
; a row's band (its CHR bank / 2), its place in the band, its name row's address
bandof: .repeat FROWS, I
        .byte   I / 3
        .endrep
kof:    .repeat FROWS, I
        .byte   I .mod 3
        .endrep
ntlo:   .repeat FROWS, I
        .byte   <($2000 + (I + NT_FIELD) * 32)
        .endrep
ntho:   .repeat FROWS, I
        .byte   >($2000 + (I + NT_FIELD) * 32)
        .endrep

        .code

_fld_flush1:
        lda     _fld_nd
        ora     _fld_nd+1
        beq     none
        lda     _qtail          ; room in the queue?
        clc
        sbc     _qhead          ; (tail - head - 1)
        cmp     #ROOM
        bcc     none
        jsr     next
        bcs     none
        jsr     draw
        lda     #1
        ldx     #0
        rts
none:   lda     #0
        tax
        rts

; next: the next changed cell from where the last search ended (rows
; wrap), its mark taken off: rr, rc. Carry set: none (the count was wrong:
; it is 0).
next:   lda     #FROWS + 1      ; (the first row twice: its end, then its start)
        sta     nx_n
        ldy     nx_r
        ldx     nx_c
@row:   lda     _fld_rowd,y
        beq     @skip
        tya                     ; the row's bits from column x
        asl
        asl
        sta     rt
@col:   cpx     #32
        bcs     @skip
        txa
        lsr
        lsr
        lsr
        ora     rt
        sta     rn              ; its byte
        txa
        and     #7
        sta     a6
        stx     a7
        ldx     rn
        lda     _fld_dirty,x
        ldx     a6
        and     bitof,x
        bne     @got
        ldx     a7
        inx
        bne     @col
@skip:  ldx     #0
        iny
        cpy     #FROWS
        bcc     @s1
        ldy     #0
@s1:    dec     nx_n
        bne     @row
        lda     #0
        sta     _fld_nd
        sta     _fld_nd+1
        sec
        rts
@got:   lda     bitof,x         ; its mark off
        eor     #$FF
        ldx     rn
        and     _fld_dirty,x
        sta     _fld_dirty,x
        lda     _fld_rowd,y
        sec
        sbc     #1
        sta     _fld_rowd,y
        lda     _fld_nd
        bne     @d
        dec     _fld_nd+1
@d:     dec     _fld_nd
        sty     rr
        sty     nx_r
        ldx     a7
        stx     rc
        inx
        stx     nx_c            ; (32: the next row's start)
        clc
        rts

; draw: cell (rr, rc) into the queue
draw:   lda     rr              ; a balloon's: let be
        asl
        asl
        sta     rt
        lda     rc
        lsr
        lsr
        lsr
        ora     rt
        tax
        lda     rc
        and     #7
        tay
        lda     _bln_cells,x
        and     bitof,y
        beq     @mine
        rts
@mine:  lda     #$FF            ; the cell over it: its last lines of dirt
        sta     a6              ; (over row 0: dirt)
        sta     a7
        ldy     rr
        beq     @me
        dey
        ldx     rc
        jsr     cellat
        jsr     fld_load
        lda     wd+6
        sta     a6
        lda     wd+7
        sta     a7
@me:    ldy     rr
        ldx     rc
        jsr     cellat
        lda     code
        cmp     #C_SKY
        beq     @sky
        cmp     #C_FIRE
        beq     @fire
        cmp     #C_DIRT
        bne     @own
        lda     a6              ; all dirt: a constant if dirt is over it
        and     a7
        cmp     #$FF
        bne     @own
        lda     #T_DIRT
        jmp     name
@fire:  lda     #T_FIRE
        jmp     name
@sky:   lda     _rnd_stars
        beq     @plain
        lda     rc              ; a starry sky: a star in some cells
        asl
        asl
        asl
        asl
        asl                     ; 32c
        clc
        adc     rc
        adc     rc
        adc     rc
        adc     rc
        adc     rc              ; 37c
        sta     rt
        lda     rr
        asl
        asl
        asl
        asl
        asl
        asl                     ; 64r
        adc     rr
        eor     rt
        sta     rt
        and     #3
        bne     @plain
        lda     rt
        lsr
        lsr
        and     #7
        clc
        adc     #T_STAR
        jmp     name
@plain: lda     #T_SKY
        jmp     name
@own:   jsr     fld_load
        jmp     tile

; tile: wd, wf as cell (rr, rc)'s own tile and its name byte (0-95 of its
; band, 128 on in an odd band), one command (crt0.s's Q_CELL: op $80, its
; kind byte $40 + the band's CHR bank; a tile and a name apiece cost the
; drain a third more)
tile:   ldx     _qhead
        lda     #$80            ; Q_CELL
        sta     _ppuq,x
        inx
        ldy     rr
        lda     bandof,y        ; its band's CHR bank (2K: 1K units * 2)
        asl
        ora     #$40
        sta     _ppuq,x
        inx
        lda     kof,y           ; the tile in the bank: k * 32 + c, at 16 bytes
        asl
        asl
        asl
        asl
        asl
        ora     rc
        sta     rt
        lsr
        lsr
        lsr
        lsr
        sta     _ppuq,x         ; (high)
        inx
        lda     rt
        asl
        asl
        asl
        asl
        sta     _ppuq,x         ; (low)
        inx
        ; plane 0: fire or not dirt
        .repeat 8, I
        lda     wd+I
        eor     #$FF
        ora     wf+I
        sta     _ppuq,x
        inx
        .endrepeat
        ; plane 1: fire, or dirt with dirt the two lines over it
        .repeat 8, I
        .if I = 0
        lda     a6
        and     a7
        .elseif I = 1
        lda     a7
        and     wd+0
        .else
        lda     wd+I-2
        and     wd+I-1
        .endif
        and     wd+I
        ora     wf+I
        sta     _ppuq,x
        inx
        .endrepeat
        ldy     rr              ; the name byte: its address, its tile
        lda     ntho,y
        sta     _ppuq,x
        inx
        lda     ntlo,y
        ora     rc
        sta     _ppuq,x
        inx
        lda     kof,y
        asl
        asl
        asl
        asl
        asl
        ora     rc
        sta     rt
        lda     bandof,y
        lsr
        lda     rt
        bcc     @ev
        ora     #$80
@ev:    sta     _ppuq,x
        inx
        stx     _qhead
        rts

; name: cell (rr, rc)'s name byte: A a constant (T_*), or 0: its own tile
name:   sta     rn
        ldx     _qhead
        lda     #1              ; a copy of one byte
        sta     _ppuq,x
        inx
        lda     #$FF
        sta     _ppuq,x
        inx
        ldy     rr
        lda     ntho,y
        sta     _ppuq,x
        inx
        lda     ntlo,y
        ora     rc
        sta     _ppuq,x
        inx
        lda     rn              ; the tile: odd bands' are 128 on
        bne     @c
        lda     kof,y
        asl
        asl
        asl
        asl
        asl
        ora     rc
@c:     sta     rt
        lda     bandof,y
        lsr
        lda     rt
        bcc     @even
        ora     #$80
@even:  sta     _ppuq,x
        inx
        stx     _qhead
        rts
