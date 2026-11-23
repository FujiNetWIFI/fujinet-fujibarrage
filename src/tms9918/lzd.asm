; lzd.asm -- a packed stream unpacked (tools/mktitle_coco.py's lz_pack,
; the CoCo's src/coco/lzd.asm in Z80): a control byte c under 80h is c + 1
; bytes as they are; 81h-FFh copies (c & 7Fh) + 2 bytes from D + 1 back (D
; the next two bytes, high first); 80h ends it.

        SECTION code_user

        PUBLIC  _lz_unpack, _lz_src, _lz_dst

; void lz_unpack(void): lz_src's stream to lz_dst
_lz_unpack:
        ld      hl,(_lz_src)
        ld      de,(_lz_dst)
lz_l:   ld      a,(hl)
        inc     hl
        cp      $80
        ret     z
        jr      c,lz_lit
        and     $7F
        add     a,2
        ld      (lz_n),a
        ld      b,(hl)          ; D, high first
        inc     hl
        ld      c,(hl)
        inc     hl
        inc     bc
        push    hl
        ld      h,d
        ld      l,e
        or      a
        sbc     hl,bc           ; from D + 1 back
        ld      a,(lz_n)
        ld      c,a
        ld      b,0
        ldir                    ; (forward, a byte at a time: overlaps repeat)
        pop     hl
        jr      lz_l
lz_lit: inc     a
        ld      c,a
        ld      b,0
        ldir
        jr      lz_l

        SECTION bss_user
_lz_src:        defs    2
_lz_dst:        defs    2
lz_n:           defs    1
