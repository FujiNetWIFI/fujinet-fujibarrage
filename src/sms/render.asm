; render.asm -- the field's dirty cells into tiles (fld.asm keeps the store
; in the cart's RAM, half 0, and marks them; tcache.asm hands the tiles out).
;
; A cell's picture is its 8 D and 8 F bytes (sky D0 F0, dirt D1 F0, fire D0
; F1) painted with its lines' colours: each line has its sky's and its
; dirt's palette index (ln_sky, ln_dirt: the round's look, field.asm), the
; fire's index goes by the line in the cell (fire_ix), so the IRQ's colour
; cycle makes the flames climb. The key is the two planes and a style: the
; row whose line colours the cell needs -- its sky's only (no dirt in it),
; its dirt's only (no sky), or both -- the first row that has the same, so
; rows alike share tiles. A cell all sky on a starry night may be a star
; tile instead (one of NSTAR, by a hash of where it is).
;
; A cell under a talk balloon (its flag's bit 1) is the balloon's: skipped.
; When the cache has nothing free the cell stays as it was, dirty, and the
; pass ends (fld_lossy counts: what frees later draws it).

        SECTION code_user

        PUBLIC  _fld_flush1, _fld_lossy, mk_field, _ln_sky, _ln_dirt, sid_make
        PUBLIC  _st_stars, rnd_cell, _sms_crust
        EXTERN  _fld_dirty, _fld_rowd, _fld_nd
        EXTERN  tc_map, tc_unmap, tc_get, tc_set, tc_flush, tc_priv, fl_row, fl_c0, fl_c1
        EXTERN  ovl_paint, vdp_wr, cell_dirty, tc_name, tc_keyof, shape_paint
        EXTERN  _tc_key, _tc_tile

        defc    SH_HI = $80
        defc    FROWS = 21
        defc    KEYLEN = 18
        defc    K_FIELD = $20
        defc    K_STAR = $60
        defc    MP_CTL = $FFFC
        defc    STAR_IX = 6
        defc    CRUST_IX = 7
        ; each line's sky and dirt index (palette 0): in the cart's RAM, half 1,
        ; after tcache.asm's tables (read only with half 1 in: tc_map)
        defc    _ln_sky = $AE80
        defc    _ln_dirt = $AE80 + 168
        defc    NSTAR = 5               ; star tiles: variants 1-5 (of 8: 0, 6, 7 none)

; the fire's index by the line in a cell (palette 0's 11-13, cycled)
fire_ix:
        defb    11, 12, 13, 11, 12, 13, 11, 12
; a star tile's pixel: line, bit (variants 1-5)
stars:  defb    1, $40,   5, $08,   2, $02,   6, $20,   4, $10

; uint8_t fld_flush1(void): a dirty cell drawn; 0 if there was none
_fld_flush1:
        ld      hl,(_fld_nd)
        ld      a,h
        or      l
        ld      l,a
        ret     z
        push    ix
        push    iy
        call    next
        pop     iy
        pop     ix
        ld      l,1
        ret

; next: the first dirty cell, top down: drawn (or skipped), its flag off
next:   ld      hl,_fld_rowd
        ld      b,0
nx_r:   ld      a,(hl)
        or      a
        jr      nz,nx_row
        inc     hl
        inc     b
        ld      a,b
        cp      FROWS
        jr      c,nx_r
        ld      hl,0            ; (the count was wrong: it is 0)
        ld      (_fld_nd),hl
        ret
nx_row: ld      (nx_rp),hl
        ld      a,b
        ld      (rc_r),a
        ld      l,b             ; the row's flags: dirty + r * 32
        ld      h,0
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ld      de,_fld_dirty
        add     hl,de
        ld      c,0
nx_c:   bit     0,(hl)
        jr      nz,nx_cell
        inc     hl
        inc     c
        ld      a,c
        cp      32
        jr      c,nx_c
        ld      hl,(nx_rp)      ; (none: the count was wrong)
        ld      (hl),0
        ret
nx_cell:
        ld      a,c
        ld      (rc_c),a
        ld      (rc_fp),hl
        bit     1,(hl)
        jr      nz,nx_done      ; under the balloon
        call    rnd_cell
        ret     c               ; (no tile: it stays dirty)
nx_done:
        ld      hl,(rc_fp)
        res     0,(hl)
        ld      hl,(nx_rp)
        dec     (hl)
        ld      hl,(_fld_nd)
        dec     hl
        ld      (_fld_nd),hl
        ret

; rnd_cell: the cell rc_r, rc_c into its tile; carry: no tile free
rnd_cell:
        ld      a,(MP_CTL)      ; slot 2 as it was, put back after
        ld      (rc_was),a
        ld      a,$08           ; the store: half 0
        ld      (MP_CTL),a
        ld      a,(rc_r)        ; D at SH_HI + 2r : c * 8, F a page on
        add     a,a
        add     a,SH_HI
        ld      h,a
        ld      a,(rc_c)
        add     a,a
        add     a,a
        add     a,a
        ld      l,a
        ld      de,_tc_key + 2
        ld      bc,8
        ldir
        ld      bc,-8
        add     hl,bc
        inc     h
        ld      bc,8
        ldir
        ; the crust: a dirt pixel with sky (or fire) one or two lines over it,
        ; as D1 F1 in the key (the store never has it). Over row 0: dirt.
        ld      a,(_sms_crust)
        or      a
        jr      z,rc_nocr
        ld      bc,$FFFF        ; B the line two over, C the one over
        ld      a,(rc_r)
        or      a
        jr      z,rc_ab
        dec     a
        add     a,a
        add     a,SH_HI
        ld      h,a
        ld      a,(rc_c)
        add     a,a
        add     a,a
        add     a,a
        add     a,6
        ld      l,a
        ld      b,(hl)
        inc     hl
        ld      c,(hl)
rc_ab:  ld      hl,_tc_key + 2
        ld      e,8
rc_cl:  ld      a,b
        and     c
        cpl
        and     (hl)            ; dirt, not dirt both over it
        ld      d,a
        push    hl
        ld      a,l
        add     a,8
        ld      l,a
        jr      nc,rc_c1
        inc     h
rc_c1:  ld      a,(hl)          ; (fire stays fire)
        push    af
        cpl
        and     d
        ld      d,a
        pop     af
        or      d
        ld      (hl),a
        pop     hl
        ld      b,c
        ld      c,(hl)
        inc     hl
        dec     e
        jr      nz,rc_cl
rc_nocr:
        ; what it has: dirt (any D), sky (any ~(D|F)), fire (any F)
        ld      hl,_tc_key + 2
        ld      b,8
        ld      c,0             ; D's
        ld      d,0             ; F's
        ld      e,0             ; sky's
rc_k:   ld      a,(hl)
        or      c
        ld      c,a
        push    hl
        ld      a,l
        add     a,8
        ld      l,a
        jr      nc,rc_k1
        inc     h
rc_k1:  ld      a,(hl)
        pop     hl
        push    af
        or      d
        ld      d,a
        pop     af
        or      (hl)
        cpl
        or      e
        ld      e,a
        inc     hl
        djnz    rc_k
        ld      a,(rc_r)
        ld      l,a
        ld      h,0
        ld      a,c
        or      a
        jr      nz,rc_d
        ld      a,(_st_stars)   ; all sky (and fire): its sky's style
        or      a
        jr      z,rc_sky
        ld      a,d
        or      a
        jr      nz,rc_sky
        ld      a,(rc_c)        ; a starry night's sky cell: a star, maybe
        ld      b,a
        add     a,a
        add     a,a
        add     a,b             ; 5c
        add     a,a
        add     a,a
        add     a,a
        sub     b               ; 39c
        ld      b,a
        ld      a,(rc_r)
        add     a,a
        add     a,a
        add     a,a
        add     a,a
        add     a,(hl)          ; (h=0, l=r: (hl) is ROM; a stir)
        xor     b
        rrca
        rrca
        and     7
        jr      z,rc_sky
        cp      NSTAR + 1
        jr      nc,rc_sky
        ld      (_tc_key + 2),a ; the variant, the rest 0
        ld      hl,_tc_key + 3
        ld      b,15
rc_z:   ld      (hl),0
        inc     hl
        djnz    rc_z
        ld      a,K_STAR
        ld      (_tc_key),a
        ld      a,(rc_r)
        ld      e,a
        ld      d,0
        ld      hl,sky_sid
        add     hl,de
        ld      a,(hl)
        ld      (_tc_key + 1),a
        jr      rc_get
rc_sky: ld      de,sky_sid
        jr      rc_st
rc_d:   ld      a,e             ; dirt: and sky?
        or      a
        ld      de,dirt_sid
        jr      z,rc_st
        ld      de,both_sid
rc_st:  add     hl,de
        ld      a,(hl)
        ld      (_tc_key + 1),a
        ld      a,K_FIELD
        ld      (_tc_key),a
rc_get: call    tc_map          ; (half 1)
        ld      a,(_sms_crust)  ; its last two lines changed: the cell under it's crust
        or      a
        call    nz,rc_below
        ld      hl,(rc_fp)      ; under a tank the sprites couldn't take, or a
        ld      a,(hl)          ; balloon's rounded end or tail?
        and     $74
        jr      z,rc_hash
        call    mk_field        ; its own tile: the field's, that on it
        ld      hl,(rc_fp)
        bit     2,(hl)
        jr      z,rc_nt
        ld      a,(rc_r)
        ld      b,a
        ld      a,(rc_c)
        ld      c,a
        call    ovl_paint
rc_nt:  ld      hl,(rc_fp)
        ld      a,(hl)
        rrca
        rrca
        rrca
        rrca
        and     7
        call    nz,shape_paint
        call    tc_priv
        jr      c,rc_full
        push    hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ex      de,hl
        ld      hl,_tc_tile
        ld      bc,32
        call    vdp_wr
        pop     hl
        jr      rc_have
rc_hash:
        call    tc_get
        jr      c,rc_full
rc_have:
        ld      a,(rc_r)
        ld      b,a
        ld      a,(rc_c)
        ld      c,a
        ld      a,(fl_row)      ; (a row's span pending: out first)
        cp      b
        jr      z,rc_set
        push    hl
        push    bc
        call    tc_flush
        pop     bc
        pop     hl
        ld      a,b
        ld      (fl_row),a
rc_set: ex      de,hl           ; palette 0, no flips
        call    tc_set
        call    tc_flush
        ld      a,(rc_was)
        ld      (MP_CTL),a
        or      a
        ret
rc_full:
        ld      hl,(_fld_lossy)
        inc     hl
        ld      (_fld_lossy),hl
        ld      a,(rc_was)
        ld      (MP_CTL),a
        scf
        ret

; rc_below: the cell under rc_r, rc_c dirty if this one's last two lines of
; dirt aren't what its tile shows (its key's D6, D7; a star's: none) (mapped)
rc_below:
        ld      a,(rc_r)
        cp      FROWS - 1
        ret     nc
        ld      b,a
        ld      a,(rc_c)
        ld      c,a
        call    tc_name
        ld      e,(hl)
        inc     hl
        ld      a,(hl)
        and     1
        ld      d,a
        ld      hl,31
        or      a
        sbc     hl,de
        jr      nc,rb_dirty     ; (no tile of the cache's: not known)
        ex      de,hl
        call    tc_keyof
        ld      a,(hl)
        cp      K_STAR
        jr      z,rb_star
        cp      K_FIELD
        jr      nz,rb_dirty
        ld      de,8            ; (kind, style, D0..: D6 at 8)
        add     hl,de
        ld      a,(_tc_key + 8)
        cp      (hl)
        jr      nz,rb_dirty
        inc     hl
        ld      a,(_tc_key + 9)
        cp      (hl)
        ret     z
        jr      rb_dirty
rb_star:
        ld      a,(_tc_key)     ; a star was there: all sky
        cp      K_STAR
        ret     z
        ld      a,(_tc_key + 8)
        ld      hl,_tc_key + 9
        or      (hl)
        ret     z
rb_dirty:
        ld      a,(rc_r)        ; (r + 1) * 32 + c
        inc     a
        ld      l,a
        ld      h,0
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ld      a,(rc_c)
        add     a,l
        ld      l,a
        jp      cell_dirty

; ---- a field cell's tile (tcache.asm's maker, through scr.asm's tc_make) ----

; mk_field: tc_key K_FIELD (style, D x 8, F x 8) or K_STAR (style, variant)
; into tc_tile
mk_field:
        ld      a,(_tc_key + 1) ; the style: its row's lines (IX sky, IY dirt)
        and     31
        add     a,a
        add     a,a
        add     a,a
        ld      e,a
        ld      d,0
        ld      ix,_ln_sky
        add     ix,de
        ld      iy,_ln_dirt
        add     iy,de
        ld      a,(_tc_key)
        cp      K_STAR
        jr      z,mk_star
        ld      hl,_tc_key + 2  ; D (F 8 on)
        ld      de,_tc_tile
        ld      b,0             ; the line
mf_l:   push    bc
        ld      a,(hl)          ; D
        ld      c,a
        push    hl
        ld      a,l
        add     a,8
        ld      l,a
        jr      nc,mf_1
        inc     h
mf_1:   ld      a,(hl)          ; F
        pop     hl
        ld      (mf_ft),a
        and     c
        ld      (mf_c),a        ; the crust: D and F
        ld      a,(mf_ft)
        cpl
        and     c
        ld      (mf_d),a        ; dirt: D and not F
        ld      a,c
        cpl
        push    hl
        ld      hl,mf_ft
        and     (hl)
        ld      (mf_f),a        ; fire: F and not D
        ld      a,(hl)
        pop     hl
        or      c
        cpl
        ld      (mf_s),a        ; sky: neither
        ld      a,b             ; the fire's index for the line
        push    hl
        ld      hl,fire_ix
        add     a,l
        ld      l,a
        jr      nc,mf_2
        inc     h
mf_2:   ld      a,(hl)
        pop     hl
        ld      (mf_fi),a
        ld      a,(ix + 0)
        ld      (mf_si),a
        ld      a,(iy + 0)
        ld      (mf_di),a
        call    mf_planes       ; 4 bytes at DE
        inc     hl
        inc     ix
        inc     iy
        pop     bc
        inc     b
        ld      a,b
        cp      8
        jr      c,mf_l
        ret

; mk_star: the line's sky everywhere, the variant's one star
mk_star:
        ld      a,(_tc_key + 2)
        dec     a
        add     a,a
        ld      c,a
        ld      b,0
        ld      hl,stars
        add     hl,bc
        ld      a,(hl)
        ld      (ms_line),a
        inc     hl
        ld      a,(hl)
        ld      (ms_bit),a
        xor     a
        ld      (mf_d),a
        ld      (mf_di),a
        ld      (mf_c),a
        ld      de,_tc_tile
        ld      b,0
ms_l:   push    bc
        xor     a
        ld      (mf_f),a        ; (fire: none; the star in its place)
        ld      a,$FF
        ld      (mf_s),a
        ld      a,(ms_line)
        cp      b
        jr      nz,ms_1
        ld      a,(ms_bit)
        ld      (mf_f),a
        cpl
        ld      (mf_s),a
ms_1:   ld      a,STAR_IX
        ld      (mf_fi),a
        ld      a,(ix + 0)
        ld      (mf_si),a
        call    mf_planes
        inc     ix
        pop     bc
        inc     b
        ld      a,b
        cp      8
        jr      c,ms_l
        ret

; mf_planes: a line's 4 plane bytes at DE (DE moves on): the sky's pixels
; mf_s in colour mf_si, the dirt's mf_d in mf_di, the fire's mf_f in mf_fi
mf_planes:
        push    hl
        ld      a,(mf_si)
        ld      h,a
        ld      a,(mf_di)
        ld      l,a
        ld      a,(mf_fi)
        ld      c,a
        ld      b,4
mp_l:   xor     a
        srl     h
        jr      nc,mp_1
        ld      a,(mf_s)
mp_1:   srl     l
        jr      nc,mp_2
        push    hl
        ld      hl,mf_d
        or      (hl)
        pop     hl
mp_2:   srl     c
        jr      nc,mp_3
        push    hl
        ld      hl,mf_f
        or      (hl)
        pop     hl
mp_3:   push    af              ; the crust's colour: 7 (0111b: planes 0-2, B 4-2)
        ld      a,b
        dec     a
        jr      z,mp_n
        pop     af
        push    hl
        ld      hl,mf_c
        or      (hl)
        pop     hl
        jr      mp_4
mp_n:   pop     af
mp_4:   ld      (de),a
        inc     de
        djnz    mp_l
        pop     hl
        ret

; ---- the styles ---------------------------------------------------------

; sid_make: each row's style for a cell of sky only, dirt only, both: the
; first row with the same lines (field.asm, after ln_sky and ln_dirt)
sid_make:
        call    tc_map
        call    sid_go
        jp      tc_unmap
sid_go: ld      hl,sky_sid
        ld      ix,_ln_sky
        ld      iy,_ln_sky      ; (only one table)
        ld      c,0
        call    sm_one
        ld      hl,dirt_sid
        ld      ix,_ln_dirt
        ld      iy,_ln_dirt
        ld      c,32
        call    sm_one
        ld      hl,both_sid
        ld      ix,_ln_sky
        ld      iy,_ln_dirt
        ld      c,64
; sm_one: for each row r: the first row q <= r whose 8 lines of IX and of IY
; are r's: (HL)[r] = C + q
sm_one: ld      b,0             ; r
so_r:   push    hl
        ld      e,0             ; q
so_q:   ld      a,e
        cp      b
        jr      z,so_got
        call    so_same
        jr      z,so_got
        inc     e
        jr      so_q
so_got: pop     hl
        ld      a,e
        add     a,c
        push    hl
        ld      d,0
        ld      e,b
        add     hl,de
        ld      (hl),a
        pop     hl
        inc     b
        ld      a,b
        cp      FROWS
        jr      c,so_r
        ret

; so_same: Z if rows E and B have the same lines in IX's and IY's tables
so_same:
        push    bc
        push    de
        push    hl
        ld      a,e
        ld      (so_qv),a
        call    so_cmp8         ; IX
        jr      nz,ss_x
        push    ix
        push    iy
        pop     ix
        ld      a,(so_qv)
        call    so_cmp8         ; IY
        pop     ix
ss_x:   pop     hl
        pop     de
        pop     bc
        ret

; so_cmp8: Z if IX's 8 lines of row A are those of row B
so_cmp8:
        push    bc
        add     a,a
        add     a,a
        add     a,a
        ld      e,a
        ld      d,0
        push    ix
        pop     hl
        add     hl,de
        push    hl
        ld      a,b
        add     a,a
        add     a,a
        add     a,a
        ld      e,a
        push    ix
        pop     hl
        add     hl,de
        pop     de
        ld      b,8
sc_l:   ld      a,(de)
        cp      (hl)
        jr      nz,sc_x
        inc     de
        inc     hl
        djnz    sc_l
sc_x:   pop     bc
        ret

        SECTION bss_user
sky_sid:        defs    FROWS
dirt_sid:       defs    FROWS
both_sid:       defs    FROWS
_st_stars:      defs    1               ; the round's sky is a starry night
_fld_lossy:     defs    2               ; cells left as they were: no tile free
rc_r:           defs    1
rc_c:           defs    1
rc_fp:          defs    2
rc_was:         defs    1
nx_rp:          defs    2
mf_s:           defs    1
mf_c:           defs    1
mf_ft:          defs    1
so_qv:          defs    1
_sms_crust:     defs    1               ; the dirt's crust drawn (field.asm's look)
mf_d:           defs    1
mf_f:           defs    1
mf_si:          defs    1
mf_di:          defs    1
mf_fi:          defs    1
ms_line:        defs    1
ms_bit:         defs    1
