*
* scr2a.asm -- the CoCo 1/2's text: a glyph into a cell of the PMODE 4
* screen (src/coco2/scr2.c). A cell is a byte (4 colour pixels) by 8
* lines, the lines 32 bytes apart; the font (gen/coco2/font2.asm) has a
* nibble a glyph row (3 pixels and a gap, bit 3 the left), which a 16-byte
* table turns into the byte: each pixel the row's ink or its ground.
*
        section hicode

        import  _font2
        export  _scr2_cell,_scr_same

* void scr2_cell(uint8_t *dst, uint8_t c, const uint8_t *tab)
*   dst: the cell's top line; c: ASCII (lower case shows as upper, the
*   rest of what isn't 32-95 as a space); tab: nibble -> byte.
* (After pshs u,y: 6,s dst, 9,s c, 10,s tab.)
_scr2_cell
        pshs    u,y
        ldb     9,s
        cmpb    #96
        blo     @up
        subb    #32             ; lower case as upper
@up     subb    #32
        bhs     @in
        clrb                    ; (below a space: a space)
@in     cmpb    #64
        blo     @ok
        clrb
@ok     clra
        lslb
        rola
        lslb
        rola
        lslb
        rola                    ; D = glyph * 8
        addd    #_font2
        tfr     d,x
        ldy     10,s
        ldu     6,s
        lda     #8
        pshs    a
@line   ldb     ,x+             ; the row's nibble
        ldb     b,y             ; its byte
        stb     ,u
        leau    32,u
        dec     ,s
        bne     @line
        leas    1,s
        puls    y,u,pc

* uint8_t scr_same(const uint8_t *cp, const char *s, uint8_t n): how many of
* s's first n characters the cells at cp have already (s's 0 ends it)
_scr_same
        pshs    u
        ldx     4,s             ; the cells
        ldu     6,s             ; the string
        ldb     9,s             ; n
        pshs    b
        beq     @x
@l      lda     ,u+
        beq     @x
        cmpa    ,x+
        bne     @x
        decb
        bne     @l
@x      negb                    ; n less those left
        addb    ,s+
        puls    u,pc
