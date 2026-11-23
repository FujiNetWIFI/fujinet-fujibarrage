*
* scr3a.asm -- the CoCo 3's text: a glyph into a cell of the GIME screen
* (src/coco3/scr3.c). A cell is 8 pixels (4 bytes) by 8 lines; row r's
* lines start at y = 8r, so a cell never straddles an 8K block (32 lines):
* its block (SCR_BLOCK + y / 32) goes into slot 6 ($C000) and the line is
* $C000 + (y % 32) * 256 (HVEN: 256-byte lines). The font (gen/coco3/
* font3.asm) has a byte a glyph row; a 16-word table turns each nibble into
* two bytes: 4 pixels, each the row's ink or its ground. Slot 6 goes back
* to the Disk ROM's block ($3E) at the end.
*
        section hicode

        import  _font3
        export  _scr3_cell,_scr_same

SCRBLK  equ     $30
ROMBLK  equ     $3E

* void scr3_cell(uint8_t row, uint8_t col, uint8_t c, const uint16_t *tab)
*   row 0-24, col 0-39; c: ASCII 32-127 (else a space); tab: nibble -> word.
* (After pshs u,y: 7,s row, 9,s col, 11,s c, 12,s tab.)
_scr3_cell
        pshs    u,y
        lda     7,s
        ldb     #8
        mul                     ; B = y = row * 8
        pshs    b
        lsrb
        lsrb
        lsrb
        lsrb
        lsrb
        addb    #SCRBLK
        stb     $FFA6           ; the row's block in slot 6
        puls    b
        andb    #31
        tfr     b,a
        adda    #$C0            ; A: $C0 + y % 32 (the line's page)
        ldb     9,s
        lslb
        lslb                    ; B: col * 4
        tfr     d,u             ; U: the cell's top line
        ldb     11,s
        subb    #32
        bhs     @in
        clrb
@in     cmpb    #96
        blo     @ok
        clrb
@ok     clra
        lslb
        rola
        lslb
        rola
        lslb
        rola                    ; D = glyph * 8
        addd    #_font3
        tfr     d,x
        ldy     12,s
        lda     #8
        pshs    a
@line   ldb     ,x
        lsrb
        lsrb
        lsrb
        lsrb
        lslb                    ; the high nibble, as a word index
        ldd     b,y
        std     ,u
        ldb     ,x+
        andb    #15
        lslb
        ldd     b,y
        std     2,u
        leau    256,u
        dec     ,s
        bne     @line
        leas    1,s
        lda     #ROMBLK
        sta     $FFA6
        puls    y,u,pc

* void scr3_fill(uint8_t y, uint8_t n, uint8_t b)
*   lines y..y+n-1, all 160 bytes of each, b (two pixels). Slot 6 back to
*   the ROM's block at the end. (After pshs u,y: 7,s y, 9,s n, 11,s b.)
        export  _scr3_fill
_scr3_fill
        pshs    u,y
@next   tst     9,s
        beq     @done
        ldb     7,s
        lsrb
        lsrb
        lsrb
        lsrb
        lsrb
        addb    #SCRBLK
        stb     $FFA6
        ldb     7,s
        andb    #31
        tfr     b,a
        adda    #$C0
        clrb
        tfr     d,u
        leau    160,u           ; (filled from the end down: pshu)
        lda     11,s
        tfr     a,b             ; D = two bytes of b
        tfr     d,x
        tfr     d,y
        lda     #16             ; 16 x 10 bytes = 160
        sta     ,-s
        lda     12,s            ; (b again: the counter moved S by 1)
        tfr     a,b
@fill   pshu    d,x,y
        pshu    d,x
        dec     ,s
        bne     @fill
        leas    1,s
        inc     7,s
        dec     9,s
        bra     @next
@done   lda     #ROMBLK
        sta     $FFA6
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
