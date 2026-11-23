*
* lzd.asm -- unpacks tools/mktitle_coco.py's streams (the title pictures):
* a control byte c < $80 is c + 1 bytes as they are; $81-$FF copies
* (c & $7F) + 2 bytes from D + 1 back (D: the next two bytes, high first);
* $80 ends it.
*
        section hicode

        export  _lz_unpack

* const uint8_t *lz_unpack(uint8_t *dst, const uint8_t *src): the stream at
* src into dst; returns where the stream ended (the next one)
_lz_unpack
        pshs    u,y
        ldu     6,s             ; dst
        ldx     8,s             ; src
@c      ldb     ,x+
        bmi     @m
        incb                    ; c + 1 bytes as they are
@l      lda     ,x+
        sta     ,u+
        decb
        bne     @l
        bra     @c
@m      andb    #$7F
        beq     @end
        addb    #2
        pshs    b
        ldd     ,x++
        coma                    ; -(D + 1)
        comb
        leay    d,u
        puls    b
@ml     lda     ,y+
        sta     ,u+
        decb
        bne     @ml
        bra     @c
@end    tfr     x,d
        puls    u,y,pc
