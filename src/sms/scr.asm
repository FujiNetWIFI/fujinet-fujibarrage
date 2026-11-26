; scr.asm -- the Master System's text screens (scr.h): 32 columns by 24 rows
; of the Namco font (gen/sms/font.asm), each letter a tile from the cache
; (tcache.asm). A tile is 4 bits a pixel, so a row's palette (rows below)
; gives each of a letter's eight lines its own ink and ground: a title's
; letters run from white down to orange, a heading is a bar lit from above.
;
; A cell's letter is in its tile's key: a row's palette changes by drawing
; its letters again in the new one, inverse the same. Each row keeps a bit
; a cell for the inverse ones. The name table is written a row's span at a
; time (tc_flush).
;
; C's arguments are on the stack (zsdcc); IX and IY are kept.

        SECTION code_user

        PUBLIC  _scr_init, _scr_begin, _scr_row, _scr_text, _scr_label, _scr_inverse
        PUBLIC  _scr_clear_row, _scr_scroll, _scr_show, _scr_picture
        PUBLIC  tc_make, mk_glyph, glyph_at, scr_rpal, scr_cram, sc_blank, put_glyph, row_start, r1, reg
        PUBLIC  _seat_rgb
        EXTERN  tc_reset, tc_get, tc_put, tc_map, tc_unmap, tc_set, tc_flush, tc_name, tc_keyof
        EXTERN  fl_row, fl_c0, fl_c1, _tc_key, _tc_tile
        EXTERN  vdp_wr, vdp_fill, _vdp_r1, _va_addr, _va_src, _va_len, _va_cram
        EXTERN  _font8, _inp_field, mk_field, _field_off

        defc    ROWS = 24
        defc    COLS = 32
        defc    KEYLEN = 18
        defc    K_GLYPH = $40
        defc    K_FIELD = $20
        defc    K_STAR = $60
        defc    PAL_TEXT = 0
        defc    PAL_TITLE = 1
        defc    PAL_HEAD = 2
        defc    PAL_HILITE = 3
        defc    PAL_DIM = 4
        defc    PAL_SEAT = 5
        defc    PAL_HUD = 15            ; (field.asm's: the HUD's rows)
        defc    PAL_HUDHI = 16
        defc    PAL_BAL = 17            ; a talk balloon's letters
        defc    PAL_HSEAT = 18          ; 18-27: a seat's name on the HUD
        defc    NPAL = 28
        defc    R1_ON = $E0
        defc    R1_OFF = $A0
        defc    VR_NAMES = $3800
        defc    VR_SAT = $3F00

; ---- the colours --------------------------------------------------------------
;
; Palette 1 (the sprites' too) is the same on every screen: the seats'
; colours, the text's white, the labels' yellow, greys, the panels' navy.
; Palette 0 is the screen's own: the text screens' ramps here, the field's
; sky, dirt and fire (field.asm).

        defc    C1_BLACK = 0
        defc    C1_WHITE = 11
        defc    C1_LABEL = 12
        defc    C1_GREY = 13
        defc    C1_STEEL = 14
        defc    C1_NAVY = 15

scr_cram:
        ; palette 0: navy, title ink white..orange, title ground reds, black,
        ; the heading's cyans, the highlight's greys, blues
        defb    $10, $3F, $2F, $0F, $0B, $07, $02, $01
        defb    $00, $3E, $3C, $28, $2A, $15, $30, $20
_seat_rgb:
        ; palette 1: black; the seats: red, yellow, cyan, green, magenta,
        ; orange, sky blue, pink, lime, violet; white, label yellow, grey,
        ; steel, navy
        defb    $00, $03, $0F, $3C, $0C, $33, $0B, $39
        defb    $3B, $1E, $36, $3F, $1F, $2A, $15, $10

; each seat's ink on a bar of its colour (palette 1)
seat_ink:
        defb    C1_WHITE, C1_BLACK, C1_BLACK, C1_BLACK, C1_WHITE
        defb    C1_BLACK, C1_BLACK, C1_BLACK, C1_BLACK, C1_WHITE

; a row's palette: 8 ink, 8 ground (a line each, top first), its label's ink,
; the name's high byte (08h: palette 1)
        defc    RP_SIZE = 18
rpals:  ; TEXT: white on navy
        defb    11, 11, 11, 11, 11, 11, 11, 11
        defb    15, 15, 15, 15, 15, 15, 15, 15
        defb    C1_LABEL, $08
        ; TITLE: white, light yellow, yellow, gold, orange down a dark red
        defb    1, 1, 2, 2, 3, 4, 5, 5
        defb    6, 6, 6, 6, 6, 6, 6, 6
        defb    3, $00
        ; HEAD: black on a cyan bar lit from above
        defb    8, 8, 8, 8, 8, 8, 8, 8
        defb    9, 10, 10, 10, 10, 10, 10, 11
        defb    8, $00
        ; HILITE: dark blue on a silver bar
        defb    15, 15, 15, 15, 15, 15, 15, 15
        defb    1, 12, 12, 12, 12, 12, 12, 13
        defb    15, $00
        ; DIM: grey on navy
        defb    13, 13, 13, 13, 13, 13, 13, 13
        defb    15, 15, 15, 15, 15, 15, 15, 15
        defb    13, $08
        ; (SEAT 5-14: worked out: rp_seat)
        defs    10 * RP_SIZE
        ; HUD: white on navy (labels yellow)
        defb    11, 11, 11, 11, 11, 11, 11, 11
        defb    15, 15, 15, 15, 15, 15, 15, 15
        defb    C1_LABEL, $08
        ; HUDHI: navy on yellow, to stand out
        defb    15, 15, 15, 15, 15, 15, 15, 15
        defb    12, 12, 12, 12, 12, 12, 12, 12
        defb    15, $08
        ; BAL: black on white (a talk balloon: over the sprites, priority)
        defb    0, 0, 0, 0, 0, 0, 0, 0
        defb    11, 11, 11, 11, 11, 11, 11, 11
        defb    0, $18

; rp_of: HL = palette A's record (a seat's made in rp_tmp)
rp_of:  cp      PAL_HSEAT
        jr      c,ro_bar
        sub     PAL_HSEAT       ; a name on the HUD: the seat's colour on navy
        inc     a
        ld      hl,rp_tmp
        ld      b,8
ro_hi:  ld      (hl),a
        inc     hl
        djnz    ro_hi
        ld      a,C1_NAVY
        ld      b,8
ro_hg:  ld      (hl),a
        inc     hl
        djnz    ro_hg
        ld      (hl),C1_LABEL
        inc     hl
        ld      (hl),$08
        ld      hl,rp_tmp
        ret
ro_bar: cp      PAL_SEAT
        jr      c,ro_tab
        cp      PAL_SEAT + 10
        jr      nc,ro_tab
        sub     PAL_SEAT
        ld      c,a
        ld      b,0
        ld      hl,seat_ink
        add     hl,bc
        ld      a,(hl)
        ld      hl,rp_tmp
        ld      b,8
ro_i:   ld      (hl),a
        inc     hl
        djnz    ro_i
        ld      a,c
        inc     a               ; the seat's colour: palette 1's 1-10
        ld      b,8
ro_g:   ld      (hl),a
        inc     hl
        djnz    ro_g
        ld      a,(rp_tmp)      ; (its label: the ink)
        ld      (hl),a
        inc     hl
        ld      (hl),$08
        ld      hl,rp_tmp
        ret
ro_tab: ld      l,a
        ld      h,0
        ld      d,h
        ld      e,l
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,de
        add     hl,hl           ; x 18
        ld      de,rpals
        add     hl,de
        ret

; ---- the letters' tiles -------------------------------------------------------

; tc_make: tc_tile = the picture of tc_key, by its kind (tcache.asm's maker)
tc_make:
        ld      a,(_tc_key)
        and     $E0
        cp      K_FIELD
        jp      z,mk_field      ; (render.asm: a field cell, a star)
        cp      K_STAR
        jp      z,mk_field
        cp      K_GLYPH
        jr      z,mk_glyph
        ld      hl,_tc_tile     ; (anything else: nothing)
        ld      de,_tc_tile + 1
        ld      bc,31
        ld      (hl),0
        ldir
        ret

; mk_glyph: key 0 K_GLYPH | inverse, 1 the character (bit 7 a label's), 2
; the palette
mk_glyph:
        ld      a,(_tc_key + 2)
        call    rp_of
        push    hl
        pop     ix              ; IX: the palette (ink 0-7, ground 8-15, label 16)
        ld      a,(_tc_key + 1)
        push    af
        and     $7F
        call    glyph_at        ; DE: the font's 8 rows
        pop     af
        rlca
        and     1
        ld      (g_lab),a
        ld      a,(_tc_key)
        and     1
        ld      (g_inv),a
        ld      a,(ix + 16)     ; (the label's ink: one for all the lines)
        ld      (g_labink),a
        ld      hl,_tc_tile
        ld      b,8
mg_l:   push    bc
        ld      a,(ix + 0)      ; the ink
        ld      c,a
        ld      a,(g_lab)
        or      a
        jr      z,mg_1
        ld      a,(g_labink)
        ld      c,a
mg_1:   ld      b,(ix + 8)      ; the ground
        ld      a,(g_inv)
        or      a
        jr      z,mg_2
        ld      a,b
        ld      b,c
        ld      c,a
mg_2:   ld      a,(de)          ; the letter's row
        inc     de
        call    planes          ; 4 bytes at HL
        inc     ix
        pop     bc
        djnz    mg_l
        ld      a,(_tc_key + 6) ; a gauge: the last line its bar on its track
        or      a
        ret     z
        ld      a,(_tc_key + 3)
        ld      e,a             ; the bar's first pixel
        ld      a,(_tc_key + 4)
        ld      d,a             ; and past its last
        ld      b,0
        ld      c,0             ; x
mg_bl:  sla     b
        ld      a,c
        cp      e
        jr      c,mg_bn
        cp      d
        jr      nc,mg_bn
        inc     b
mg_bn:  inc     c
        ld      a,c
        cp      8
        jr      c,mg_bl
        ld      e,b             ; (x 0 at bit 7)
        ld      a,(_tc_key + 5)
        ld      c,a             ; the bar's colour
        ld      a,(_tc_key + 6)
        ld      b,a             ; the track's
        ld      a,e
        ld      hl,_tc_tile + 28
        jp      planes

; planes: the row A (1 = ink) in colour C, the rest in colour B: its four
; planes at HL (HL moves on; DE kept)
planes: push    de
        ld      d,a
        cpl
        ld      e,a             ; D ink's pixels, E the ground's
        ld      a,4
pl_l:   push    af
        xor     a
        srl     c
        jr      nc,pl_1
        or      d
pl_1:   srl     b
        jr      nc,pl_2
        or      e
pl_2:   ld      (hl),a
        inc     hl
        pop     af
        dec     a
        jr      nz,pl_l
        pop     de
        ret

; glyph_at: DE = the font's rows of character A (' '..'_'; lower case as
; upper; anything else blank)
glyph_at:
        cp      $60
        jr      c,ga_1
        sub     $20
ga_1:   sub     $20
        jr      c,ga_sp
        cp      $40
        jr      c,ga_2
ga_sp:  xor     a
ga_2:   ld      l,a
        ld      h,0
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ld      de,_font8
        add     hl,de
        ex      de,hl
        ret

; ---- cells ------------------------------------------------------------------------

; char_of: A = the character cell (row B, column C) shows (a letter's key, or
; ' '). (mapped; BC kept)
char_of:
        push    bc
        call    tc_name
        ld      e,(hl)
        inc     hl
        ld      a,(hl)
        and     1
        ld      d,a
        ld      hl,31
        or      a
        sbc     hl,de
        jr      nc,co_sp        ; (under the cache's: blank)
        ex      de,hl
        call    tc_keyof
        ld      a,(hl)
        and     $E0
        cp      K_GLYPH
        jr      nz,co_sp
        inc     hl
        ld      a,(hl)
        pop     bc
        ret
co_sp:  ld      a,' '
        pop     bc
        ret

; inv_at: Z if cell (row B, column C) isn't inverse (BC kept)
inv_at: call    inv_bit
        and     (hl)
        ret

; inv_bit: HL = &rinv[row B][C / 8], A = C's bit (BC kept)
inv_bit:
        ld      a,b
        add     a,a
        add     a,a
        ld      l,a
        ld      a,c
        rrca
        rrca
        rrca
        and     3
        add     a,l
        ld      l,a
        ld      h,0
        ld      de,rinv
        add     hl,de
        ld      a,c
        and     7
        ld      d,a
        ld      a,$80
        inc     d
ib_l:   dec     d
        ret     z
        rrca
        jr      ib_l

; put_char: cell (row B, column C) shows character A in its row's palette,
; inverse as its bit says (mapped; BC kept)
put_char:
        push    af
        call    inv_at
        ld      a,0
        jr      z,pc_0
        inc     a
pc_0:   ld      (pc_inv),a
        ld      hl,scr_rpal
        ld      e,b
        ld      d,0
        add     hl,de
        ld      a,(hl)
        ld      (pc_pal),a
        pop     af
; put_glyph: cell (row B, column C) shows character A in palette pc_pal,
; inverse if pc_inv; a cell already showing it is left (mapped; BC kept)
put_glyph:
        push    af
        ld      hl,_tc_key      ; the key: kind, character, palette, the rest 0
        ld      de,_tc_key + 1
        push    bc
        ld      bc,KEYLEN - 1
        ld      (hl),0
        ldir
        pop     bc
        pop     af
        ld      (_tc_key + 1),a
        ld      hl,(pg_bar)     ; a gauge on its last line (the HUD's): start, end,
        ld      (_tc_key + 3),hl        ; its colour, the track's (0: none)
        ld      hl,(pg_bar + 2)
        ld      (_tc_key + 5),hl
        ld      a,(pc_inv)
        or      K_GLYPH
        ld      (_tc_key),a
        ld      a,(pc_pal)
        ld      (_tc_key + 2),a
        push    bc              ; the cell shows that already? (its tile's key)
        call    tc_name
        ld      e,(hl)
        inc     hl
        ld      a,(hl)
        and     1
        ld      d,a
        ld      hl,31
        or      a
        sbc     hl,de
        jr      nc,pg_new
        ex      de,hl
        call    tc_keyof
        ld      de,_tc_key
        ld      b,KEYLEN
pg_cmp: ld      a,(de)
        cp      (hl)
        jr      nz,pg_new
        inc     hl
        inc     de
        djnz    pg_cmp
        pop     bc              ; (the same: nothing to do)
        ret
pg_new: pop     bc
        push    bc
        call    rp_of           ; its name's high byte
        ld      de,17
        add     hl,de
        ld      a,(hl)
        ld      (pc_hi),a
        call    tc_get
        jr      nc,pc_2
        ld      hl,0            ; (nothing free: blank)
pc_2:   ld      a,(pc_hi)
        or      h
        ld      d,a
        ld      e,l
        pop     bc
        push    de              ; the same word as the cell's: let it be
        push    bc
        call    tc_name
        pop     bc
        pop     de
        ld      a,(hl)
        cp      e
        jr      nz,pc_set
        inc     hl
        ld      a,(hl)
        cp      d
        jr      nz,pc_set
        ex      de,hl           ; (one cell fewer again: the tc_get's)
        ld      a,h
        and     1
        ld      h,a
        push    bc
        call    tc_put
        pop     bc
        ret
pc_set: jp      tc_set

; row_start: the span's row is B (the last one's flushed first)
row_start:
        ld      a,(fl_row)
        cp      b
        ret     z
        push    bc
        call    tc_flush
        pop     bc
        ld      a,b
        ld      (fl_row),a
        ret

; redraw: the row B's columns C..C+E-1 again (its palette or inverse changed)
redraw: call    row_start
rd_l:   push    de
        call    char_of
        call    put_char
        pop     de
        inc     c
        dec     e
        jr      nz,rd_l
        ret

; ---- the screen -------------------------------------------------------------

; sc_blank: a text screen's start: blank, the cache empty, the names clear,
; no sprites, the text's colours, no scroll (field.asm's field_open, too)
sc_blank:
        xor     a
        ld      (pic_up),a
        call    _field_off      ; (the field's sprites, its fire, its shake stop)
        ld      a,R1_OFF
        call    r1
        call    tc_map
        call    tc_reset
        call    tc_unmap
        ld      a,255
        ld      (fl_c0),a
        xor     a
        ld      (fl_c1),a
        ld      (fl_row),a
        ld      (_inp_field),a
        ld      de,VR_NAMES     ; all the names: tile 0
        ld      bc,32 * 28 * 2
        xor     a
        call    vdp_fill
        ld      de,VR_SAT       ; no sprites
        ld      bc,1
        ld      a,$D0
        call    vdp_fill
        ld      hl,scr_rpal
        ld      de,scr_rpal + 1
        ld      bc,ROWS + ROWS * 4 - 1
        ld      (hl),0          ; (rpal, then rinv)
        ldir
        xor     a
        ld      c,8
        call    reg
        xor     a
        ld      c,9
        jp      reg

; r1: register 1 = A; reg: register C = A (BC kept)
r1:     ld      c,1
        ld      (_vdp_r1),a
reg:    di
        out     ($BF),a
        ld      a,c
        or      $80
        out     ($BF),a
        ei
        ret

; void scr_init(void)
_scr_init:
        ret

; void scr_begin(void): clear, all rows PAL_TEXT; off (scr_show puts it on)
_scr_begin:
        call    sc_blank
        ld      hl,scr_cram     ; the text's colours
        ld      (_va_src),hl
        ld      hl,0
        ld      (_va_addr),hl
        ld      hl,32
        ld      (_va_len),hl
        jp      _va_cram

; void scr_show(void)
_scr_show:
        ld      a,R1_ON
        jr      r1

; void scr_row(uint8_t row, uint8_t pal)
_scr_row:
        ld      hl,2
        add     hl,sp
        ld      b,(hl)
        inc     hl
        ld      a,(hl)
        ld      hl,scr_rpal
        ld      e,b
        ld      d,0
        add     hl,de
        ld      (hl),a
        ld      c,0
        ld      e,COLS
        jr      redraw_m

; void scr_inverse(uint8_t row, uint8_t col, uint8_t n)
_scr_inverse:
        ld      hl,2
        add     hl,sp
        ld      b,(hl)
        inc     hl
        ld      c,(hl)
        inc     hl
        ld      e,(hl)
        ld      a,c             ; n, at most to the row's end
        add     a,e
        cp      COLS + 1
        jr      c,si_1
        ld      a,COLS
        sub     c
        ld      e,a
si_1:   ld      a,e
        or      a
        ret     z
        push    bc
        push    de
si_l:   push    de
        call    inv_bit
        or      (hl)
        ld      (hl),a
        pop     de
        inc     c
        dec     e
        jr      nz,si_l
        pop     de
        pop     bc
redraw_m:
        call    tc_map
        call    redraw
        call    tc_flush
        jp      tc_unmap

; void scr_clear_row(uint8_t row): spaces in the row's palette, none inverse
_scr_clear_row:
        ld      hl,2
        add     hl,sp
        ld      b,(hl)
        ld      a,b
        add     a,a
        add     a,a
        ld      l,a
        ld      h,0
        ld      de,rinv
        add     hl,de
        xor     a
        ld      (hl),a
        inc     hl
        ld      (hl),a
        inc     hl
        ld      (hl),a
        inc     hl
        ld      (hl),a
        call    tc_map
        call    row_start
        ld      c,0
cr_l:   ld      a,' '
        call    put_char
        inc     c
        ld      a,c
        cp      COLS
        jr      c,cr_l
        call    tc_flush
        jp      tc_unmap

; void scr_text(uint8_t row, uint8_t col, const char *s)
; void scr_label(uint8_t row, uint8_t col, const char *s): bit 7 set (the
; label's ink)
_scr_text:
        xor     a
        jr      st_e
_scr_label:
        ld      a,$80
st_e:   ld      (st_lab),a
        ld      hl,2
        add     hl,sp
        ld      b,(hl)
        inc     hl
        ld      c,(hl)
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      hl,st_buf       ; the string first (it may be in the FAR bank)
        ld      a,c
st_c:   cp      COLS
        jr      nc,st_e2
        push    af
        ld      a,(de)
        or      a
        jr      z,st_e1
        push    hl
        ld      hl,st_lab
        or      (hl)
        pop     hl
        ld      (hl),a
        inc     hl
        inc     de
        pop     af
        inc     a
        jr      st_c
st_e1:  pop     af
st_e2:  ld      (hl),0
        call    tc_map
        call    row_start
        ld      hl,st_buf
st_l:   ld      a,(hl)
        or      a
        jr      z,st_done
        inc     hl
        push    hl
        call    put_char
        pop     hl
        inc     c
        jr      st_l
st_done:
        call    tc_flush
        jp      tc_unmap

; void scr_scroll(uint8_t row, uint8_t n, uint8_t up): rows row..row+n-1 a
; row up (the top one goes, the bottom one comes blank) or down
_scr_scroll:
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        ld      (ss_row),a
        inc     hl
        ld      a,(hl)
        ld      (ss_n),a
        inc     hl
        ld      a,(hl)
        ld      (ss_up),a
        call    tc_map
        call    tc_flush        ; (a span pending: out first)
        ld      a,(ss_up)       ; the row that goes: its tiles let go
        or      a
        ld      a,(ss_row)
        jr      nz,ss_g
        ld      hl,ss_n
        add     a,(hl)
        dec     a
ss_g:   call    row_free
        ld      a,(ss_n)        ; the rest move: names, palettes, inverse bits
        dec     a
        jr      z,ss_last
        ld      b,a
ss_l:   push    bc
        ld      a,(ss_up)
        or      a
        ld      a,(ss_n)
        jr      z,ss_dn
        sub     b               ; up: to row + n - 1 - b from the one under
        dec     a
        ld      hl,ss_row
        add     a,(hl)
        ld      e,a
        inc     a
        jr      ss_mv
ss_dn:  ld      a,(ss_row)      ; down: to row + b from the one over
        add     a,b
        ld      e,a
        dec     a
ss_mv:  ld      d,a             ; row D's to row E
        call    row_move
        pop     bc
        djnz    ss_l
ss_last:
        ld      a,(ss_up)       ; the row that came: blank, plain text
        or      a
        ld      a,(ss_row)
        jr      z,ss_b
        ld      hl,ss_n
        add     a,(hl)
        dec     a
ss_b:   ld      b,a
        ld      hl,scr_rpal
        ld      e,a
        ld      d,0
        add     hl,de
        ld      (hl),PAL_TEXT
        ld      a,b
        add     a,a
        add     a,a
        ld      l,a
        ld      h,0
        ld      de,rinv
        add     hl,de
        xor     a
        ld      (hl),a
        inc     hl
        ld      (hl),a
        inc     hl
        ld      (hl),a
        inc     hl
        ld      (hl),a
        call    row_blank
        jp      tc_unmap

; row_free: row A's tiles let go (its names left as they were) (mapped)
row_free:
        ld      b,a
        ld      c,0
        call    tc_name
        ld      b,COLS
rf_l:   push    bc
        ld      e,(hl)
        inc     hl
        ld      a,(hl)
        inc     hl
        push    hl
        and     1
        ld      h,a
        ld      l,e
        call    tc_put
        pop     hl
        pop     bc
        djnz    rf_l
        ret

; row_move: row D's names, palette and inverse bits to row E; its names to
; the VRAM (mapped)
row_move:
        push    de
        ld      b,d
        ld      c,0
        call    tc_name
        pop     de
        push    de
        push    hl
        ld      b,e
        ld      c,0
        call    tc_name
        ex      de,hl
        pop     hl
        ld      bc,COLS * 2
        ldir
        pop     de
        push    de
        ld      hl,scr_rpal
        ld      c,d
        ld      b,0
        add     hl,bc
        ld      a,(hl)
        ld      hl,scr_rpal
        ld      c,e
        add     hl,bc
        ld      (hl),a
        ld      a,d             ; rinv
        add     a,a
        add     a,a
        ld      c,a
        ld      hl,rinv
        add     hl,bc
        push    hl
        ld      a,e
        add     a,a
        add     a,a
        ld      c,a
        ld      hl,rinv
        add     hl,bc
        ex      de,hl
        pop     hl
        ld      bc,4
        ldir
        pop     de
        ld      a,e             ; the whole row out
        ld      (fl_row),a
        xor     a
        ld      (fl_c0),a
        ld      a,COLS - 1
        ld      (fl_c1),a
        jp      tc_flush

; row_blank: row B shows tile 0 everywhere (its tiles went elsewhere: no
; puts), names and VRAM (mapped)
row_blank:
        push    bc
        ld      c,0
        call    tc_name
        ld      d,h
        ld      e,l
        inc     de
        ld      (hl),0
        ld      bc,COLS * 2 - 1
        ldir
        pop     bc
        ld      a,b
        ld      (fl_row),a
        xor     a
        ld      (fl_c0),a
        ld      a,COLS - 1
        ld      (fl_c1),a
        jp      tc_flush

; void scr_picture(void): the title: a picture of its own (gen/sms/title.bin,
; tools/sms/mktitle_sms.py: its tiles, names and colours, in bank
; TITLE_BANK), copied into the VRAM as it is, its words in it. The cache
; is empty after it (its tiles are the picture's): the next screen starts
; afresh (sc_blank).
        defc    TITLE_BANK = 3
        defc    MP_CTL = $FFFC
        defc    MP_SLOT2 = $FFFF
_scr_picture:
        call    sc_blank        ; (blanked, nothing cached)
        ld      a,(MP_CTL)      ; slot 2 as it was (a FAR caller's), put back after
        push    af
        ld      a,(MP_SLOT2)
        push    af
        xor     a
        ld      (MP_CTL),a
        ld      a,TITLE_BANK
        ld      (MP_SLOT2),a
        ld      hl,$8000 + 2    ; the colours
        ld      (_va_src),hl
        ld      hl,0
        ld      (_va_addr),hl
        ld      hl,32
        ld      (_va_len),hl
        call    _va_cram
        ld      hl,$8000 + 34   ; the names: rows 0-23
        ld      de,VR_NAMES
        ld      bc,32 * 24 * 2
        call    vdp_wr
        ld      hl,($8000)      ; the tiles: N x 32
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ld      b,h
        ld      c,l
        ld      hl,$8000 + 34 + 32 * 24 * 2
        ld      de,0
        call    vdp_wr
        pop     af
        ld      (MP_SLOT2),a
        pop     af
        ld      (MP_CTL),a
        ld      a,1             ; (the cache's tables: the picture took its tiles)
        ld      (pic_up),a
        jp      _scr_show

        SECTION bss_user
scr_rpal:       defs    ROWS            ; each row's palette
rinv:           defs    ROWS * 4        ; each row's inverse cells, a bit each
rp_tmp:         defs    RP_SIZE
st_buf:         defs    COLS + 1
st_lab:         defs    1
g_lab:          defs    1
g_inv:          defs    1
g_labink:       defs    1
pc_hi:          defs    1
pic_up:         defs    1
        PUBLIC  pg_bar
pg_bar:         defs    4       ; a gauge for put_glyph's cells (field.asm's HUD)
        PUBLIC  pc_pal, pc_inv
pc_pal:         defs    1
pc_inv:         defs    1
ss_row:         defs    1
ss_n:           defs    1
ss_up:          defs    1
