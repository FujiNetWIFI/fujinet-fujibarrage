*
* scr2a.asm -- the CoCo 1/2's text: a glyph into a cell of the PMODE 4
* screen (src/coco2/scr2.c). A cell is a byte (4 colour pixels) by 8
* lines, the lines 32 bytes apart; the font (gen/coco2/font2.asm) has a
* nibble a glyph row (3 pixels and a gap, bit 3 the left), which a 16-byte
* table turns into the byte: each pixel the row's ink or its ground.
*
        section hicode

        import  _font2
        export  _scr2_run,_scr2_move,_scr_same

* void scr2_run(uint8_t *dst, const uint8_t *chars, uint8_t n, const uint8_t *tab,
*               const uint8_t *ltab)
*   n cells from dst (the first's top line), their characters at chars
*   (ASCII: lower case shows as upper, the rest of what isn't 32-95 as a
*   space; bit 7 a label's, through ltab); tab: nibble -> byte. A run a
*   call: a cell a call from C cost more than its drawing (TODO 2026-11-23).
* (After pshs u,y: 6,s dst, 8,s chars, 11,s n, 12,s tab, 14,s ltab.)
_scr2_run
        pshs    u,y
        tst     11,s
        beq     @x
@cell   ldx     8,s
        ldb     ,x+
        stx     8,s
        ldy     12,s
        tstb
        bpl     @pl
        ldy     14,s            ; (a label)
        andb    #$7F
@pl     cmpb    #96
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
        ldu     6,s
        lda     #8
@line   ldb     ,x+             ; the row's nibble
        ldb     b,y             ; its byte
        stb     ,u
        leau    32,u
        deca
        bne     @line
        ldu     6,s
        leau    1,u
        stu     6,s
        dec     11,s
        bne     @cell
@x      puls    y,u,pc

* void scr2_move(uint8_t *dst, const uint8_t *src, uint16_t n)
*   n bytes (16s) from src to dst, forward: dst below src, or apart (a
*   scroll down moves a row at a time, from the bottom). PULU and offset
*   stores: CMOC's memcpy, a byte at a time, took 119 ms a shop scroll.
* (After pshs u,y: 6,s dst, 8,s src, 10,s n.)
_scr2_move
        pshs    u,y
        ldy     6,s
        ldu     8,s
        ldd     10,s
        lsra
        rorb
        lsra
        rorb
        lsra
        rorb
        lsra
        rorb                    ; 16 bytes a pass
        pshs    d
        beq     @x
@blk    pulu    d,x
        std     ,y
        stx     2,y
        pulu    d,x
        std     4,y
        stx     6,y
        pulu    d,x
        std     8,y
        stx     10,y
        pulu    d,x
        std     12,y
        stx     14,y
        leay    16,y
        ldd     ,s
        subd    #1
        std     ,s
        bne     @blk
@x      leas    2,s
        puls    y,u,pc

* uint8_t scr_same(const uint8_t *cp, const char *s, uint8_t n, uint8_t m):
* how many of s's first n characters (each OR m: 80h a label's) the cells
* at cp have already (s's 0 ends it)
_scr_same
        pshs    u
        ldx     4,s             ; the cells
        ldu     6,s             ; the string
        ldb     9,s             ; n
        pshs    b
        beq     @x
@l      lda     ,u+
        beq     @x
        ora     12,s
        cmpa    ,x+
        bne     @x
        decb
        bne     @l
@x      negb                    ; n less those left
        addb    ,s+
        puls    u,pc
