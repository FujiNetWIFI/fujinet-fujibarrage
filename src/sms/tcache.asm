; tcache.asm -- the Master System's tiles: one cache for everything on the
; screen (the field's cells, the letters, the balloons), so a tile is made
; once and shared by every cell that looks the same.
;
; Mode 4 has room for 448 tiles under the name table, and the field alone
; is 672 cells: no bitmap. A cell is described by a key of KEYLEN bytes
; (tc_key: its kind, then what makes its picture: a field cell's two planes
; and its row's colours, a letter's character and palette); the key is
; hashed, and a tile with the same key already in the VRAM is used again
; (counted: refs). A new key takes a tile nobody uses (the never-used ones
; first, then the clock's next with refs 0, its old key forgotten), the
; maker for its kind draws its 32 bytes (tc_tile) and they go into the VRAM.
; When nothing is free tc_get says so (carry): the caller draws the cell
; another way (render.asm: lossy, put right when tiles come free).
;
; The tables are in the cart's RAM, half 1 (src/sms/map.asm): the caller
; has it in slot 2 (tc_map/tc_unmap); everything else they need is in
; system RAM or ROM.
;
;   TC_KEYS   8000h  448 x 18   each tile's key (kind 0: none)
;   TC_NEXT   9F80h  448 x 2    the next tile in its hash bucket (FFFFh: end)
;   TC_REFS   A300h  448        cells using it (255 sticks)
;   TC_HB     A4C0h  448        its bucket
;   TC_HEADS  A680h  256 x 2    each bucket's first tile
;   TC_NAMES  A880h  768 x 2    the name table's rows 0-23 as written
;
; Tiles 0-31 aren't the cache's: tile 0 is blank (colour 0), 1-31 the
; sprites' (the tanks: field.asm).

        SECTION code_user

        PUBLIC  tc_reset, tc_get, tc_put, tc_map, tc_unmap
        PUBLIC  tc_set, tc_flush, tc_name, tc_keyof, tc_priv
        PUBLIC  _tc_key, _tc_tile, _tc_full, _tc_made
        EXTERN  vdp_wr, vdp_fill, tc_make

        defc    TC_KEYS = $8000
        defc    TC_NEXT = $9F80
        defc    TC_REFS = $A300
        defc    TC_HB = $A4C0
        defc    TC_HEADS = $A680
        defc    TC_NAMES = $A880
        defc    KEYLEN = 18
        defc    FIRST = 32
        defc    NSLOTS = 448
        defc    VR_NAMES = $3800
        defc    K_PRIV = $E0            ; a tile of one cell's own (not hashed)
        defc    MP_CTL = $FFFC

; ---- slot 2 -------------------------------------------------------------------

; tc_map: the cart's RAM half 1 in slot 2, what was there kept (A, HL kept)
tc_map: push    af
        ld      a,(MP_CTL)
        ld      (tc_was),a
        ld      a,$0C
        ld      (MP_CTL),a
        pop     af
        ret

; tc_unmap: slot 2 as tc_map found it (A kept). (A ROM bank under it stays
; in FFFFh, untouched: so a FAR caller's bank comes back too.)
tc_unmap:
        push    af
        ld      a,(tc_was)
        ld      (MP_CTL),a
        pop     af
        ret

; ---- the cache ----------------------------------------------------------------

; tc_reset: nothing cached, every cell blank (tile 0); the never-used tiles
; from FIRST (mapped)
tc_reset:
        ld      hl,TC_KEYS
        ld      de,TC_KEYS + 1
        ld      bc,TC_HEADS + 512 - TC_KEYS - 1
        ld      (hl),0
        ldir                    ; (keys, nexts, refs, buckets 0)
        ld      hl,TC_HEADS     ; the buckets empty
        ld      de,TC_HEADS + 1
        ld      bc,511
        ld      (hl),$FF
        ldir
        ld      hl,TC_NAMES
        ld      de,TC_NAMES + 1
        ld      bc,768 * 2 - 1
        ld      (hl),0
        ldir
        ld      hl,FIRST
        ld      (tc_fresh),hl
        ld      (tc_clock),hl
        xor     a
        ld      (_tc_full),a
        ld      (fl_c1),a
        ld      a,255
        ld      (fl_c0),a
        ret

; keyaddr: HL = tile DE's key (DE kept)
keyaddr:
        ld      h,d
        ld      l,e
        add     hl,hl
        push    hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        pop     bc
        add     hl,bc           ; x 18
        ld      bc,TC_KEYS
        add     hl,bc
        ret

; tc_keyof: HL = tile HL's key (for a caller reading what a cell shows)
tc_keyof:
        ex      de,hl
        jr      keyaddr

; nextaddr: HL = &next[DE] (DE kept)
nextaddr:
        ld      h,d
        ld      l,e
        add     hl,hl
        ld      bc,TC_NEXT
        add     hl,bc
        ret

; headaddr: HL = &heads[A]
headaddr:
        ld      l,a
        ld      h,0
        add     hl,hl
        ld      bc,TC_HEADS
        add     hl,bc
        ret

; hash: A = tc_key's hash (8 bits)
hash:   ld      hl,_tc_key
        ld      b,KEYLEN
        xor     a
hs_l:   rlca
        xor     (hl)
        add     a,b
        inc     hl
        djnz    hs_l
        ret

; tc_get: HL = the tile for tc_key, counted; carry: no tile free (mapped;
; IX, IY kept)
tc_get:
        call    hash
        ld      (g_h),a
        call    headaddr
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
g_walk: ld      a,d
        inc     a
        jr      z,g_miss
        push    de
        call    keyaddr
        ld      de,_tc_key
        ld      b,KEYLEN
g_cmp:  ld      a,(de)
        cp      (hl)
        jr      nz,g_ne
        inc     hl
        inc     de
        djnz    g_cmp
        pop     de              ; the same: one more cell on it
        ld      hl,TC_REFS
        add     hl,de
        inc     (hl)
        jr      nz,g_hit
        dec     (hl)            ; (255 sticks)
g_hit:  ex      de,hl
        or      a
        ret
g_ne:   pop     de
        call    nextaddr
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        jr      g_walk

g_miss: call    victim          ; DE: a tile nobody uses, unhashed
        ret     c
        push    de
        call    keyaddr         ; its key
        ex      de,hl
        ld      hl,_tc_key
        ld      bc,KEYLEN
        ldir
        pop     de
        ld      hl,TC_HB
        add     hl,de
        ld      a,(g_h)
        ld      (hl),a
        call    headaddr        ; in front of its bucket
        ld      c,(hl)
        ld      (hl),e
        inc     hl
        ld      b,(hl)
        ld      (hl),d
        push    bc
        call    nextaddr
        pop     bc
        ld      (hl),c
        inc     hl
        ld      (hl),b
        ld      hl,TC_REFS
        add     hl,de
        ld      (hl),1
        push    de
        push    ix
        push    iy
        call    tc_make         ; its picture into tc_tile (render.asm, scr.asm)
        pop     iy
        pop     ix
        pop     de
        push    de
        ex      de,hl           ; the VRAM: tile x 32
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ex      de,hl
        ld      hl,_tc_tile
        ld      bc,32
        call    vdp_wr
        ld      hl,(_tc_made)
        inc     hl
        ld      (_tc_made),hl
        pop     hl
        or      a
        ret

; victim: DE = a tile with no cells (unlinked from its bucket); carry: none
victim: ld      hl,(tc_fresh)   ; a never-used one
        ld      de,NSLOTS
        or      a
        sbc     hl,de
        jr      nc,v_old
        add     hl,de
        ex      de,hl
        inc     de
        ld      (tc_fresh),de
        dec     de
        or      a
        ret
v_old:  ld      de,(tc_clock)   ; the clock's next with refs 0
        ld      bc,NSLOTS - FIRST
v_l:    ld      hl,TC_REFS
        add     hl,de
        ld      a,(hl)
        or      a
        jr      z,v_got
        inc     de
        ld      hl,NSLOTS
        or      a
        sbc     hl,de
        jr      nz,v_n
        ld      de,FIRST
v_n:    dec     bc
        ld      a,b
        or      c
        jr      nz,v_l
        ld      a,1
        ld      (_tc_full),a
        scf
        ret
v_got:  push    de              ; the clock on past it
        inc     de
        ld      hl,NSLOTS
        or      a
        sbc     hl,de
        jr      nz,v_c
        ld      de,FIRST
v_c:    ld      (tc_clock),de
        pop     de
        call    keyaddr         ; was it hashed? (kind 0: never; K_PRIV: not)
        ld      a,(hl)
        or      a
        ret     z               ; (NC)
        cp      K_PRIV
        jr      nz,v_hash
        ld      (hl),0
        or      a
        ret
v_hash: push    hl              ; (its key forgotten once it's out of its bucket)
        ld      hl,TC_HB        ; out of its bucket
        add     hl,de
        ld      a,(hl)
        call    headaddr
        push    hl              ; &heads[b]
        call    nextaddr
        ld      c,(hl)
        inc     hl
        ld      b,(hl)          ; BC = next[victim]
        pop     hl
v_u:    ld      a,(hl)          ; (HL) the link that may be it
        cp      e
        jr      nz,v_un
        inc     hl
        ld      a,(hl)
        cp      d
        dec     hl
        jr      z,v_cut
v_un:   push    de              ; on along the chain
        push    bc              ; (nextaddr's BC: the victim's next is in it)
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        call    nextaddr
        pop     bc
        pop     de
        jr      v_u
v_cut:  ld      (hl),c
        inc     hl
        ld      (hl),b
        pop     hl
        ld      (hl),0
        or      a
        ret

; tc_priv: HL = a tile of the caller's own (its 32 bytes the caller's to
; write), counted once, never shared; carry: none free (mapped)
tc_priv:
        call    victim
        ret     c
        call    keyaddr
        ld      (hl),K_PRIV
        ld      hl,TC_REFS
        add     hl,de
        ld      (hl),1
        ex      de,hl
        or      a
        ret

; tc_put: tile HL one cell fewer (mapped; nothing for tiles under FIRST)
tc_put: ld      a,h
        or      a
        jr      nz,p_go
        ld      a,l
        cp      FIRST
        ret     c
p_go:   ld      de,TC_REFS
        add     hl,de
        ld      a,(hl)
        or      a
        ret     z
        inc     a
        ret     z               ; (255 sticks)
        dec     (hl)
        ret

; ---- the name table ---------------------------------------------------------

; tc_name: HL = &names[row B, column C] (BC kept)
tc_name:
        ld      l,b
        ld      h,0
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl           ; row x 32
        ld      a,c
        add     a,l
        ld      l,a
        add     hl,hl
        ld      de,TC_NAMES
        add     hl,de
        ret

; tc_set: cell (row B, column C) shows name word DE (tile | attributes << 8);
; the tile it showed one cell fewer; the column joins the span to flush
; (fl_row must be B: tc_flush first when it isn't). (mapped; BC kept)
tc_set: push    bc
        push    de
        call    tc_name
        ld      e,(hl)
        inc     hl
        ld      d,(hl)          ; the old word
        pop     bc
        ld      (hl),b
        dec     hl
        ld      (hl),c          ; the new
        ld      a,d
        and     1
        ld      h,a
        ld      l,e
        call    tc_put          ; (the new tile was counted by its tc_get)
        pop     bc
        ld      a,(fl_c0)
        cp      c
        jr      c,ts_1
        ld      a,c
        ld      (fl_c0),a
ts_1:   ld      a,(fl_c1)
        cp      c
        ret     nc
        ld      a,c
        ld      (fl_c1),a
        ret

; tc_flush: the row fl_row's span fl_c0..fl_c1 to the VRAM (mapped)
tc_flush:
        ld      a,(fl_c1)
        ld      hl,fl_c0
        sub     (hl)
        ret     c               ; (nothing: c1 < c0)
        inc     a
        add     a,a
        ld      c,a
        ld      b,0             ; the bytes
        ld      a,(fl_row)
        ld      l,a
        ld      h,0
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ld      a,(fl_c0)
        add     a,l
        ld      l,a
        add     hl,hl
        push    hl
        ld      de,VR_NAMES
        add     hl,de
        ex      de,hl
        pop     hl
        push    bc
        ld      bc,TC_NAMES
        add     hl,bc
        pop     bc
        call    vdp_wr
        ld      a,255
        ld      (fl_c0),a
        xor     a
        ld      (fl_c1),a
        ret

        SECTION bss_user
_tc_key:        defs    KEYLEN
_tc_tile:       defs    32
_tc_full:       defs    1       ; a tc_get found nothing free (since the last reset)
_tc_made:       defs    2       ; tiles made (a rig's count)
tc_fresh:       defs    2
tc_clock:       defs    2
tc_was:         defs    1
g_h:            defs    1
        PUBLIC  fl_row, fl_c0, fl_c1
fl_row:         defs    1
fl_c0:          defs    1
fl_c1:          defs    1
