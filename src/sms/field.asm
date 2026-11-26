; field.asm -- the field (field.h) on the Master System: the store and its
; renderer (src/tms9918/fld.asm, render.asm), the tanks as sprites, the HUD
; and the talk on the text's tiles, the frame's colours (the Adam's
; src/tms9918/field.asm, in Mode 4).
;
; The round's look: palette 0 is the field's own --
;    0  black          1-5  the sky's colours, top down (bands by line)
;    6  a star         7-10 the dirt's strata, light to dark (by line)
;   11-13  the fire (cycled, a step every 4 frames)   14-15 (spare)
; -- so a round's look is a load of palette 0 and the line tables; the
; tiles follow from them (render.asm).
;
; The tanks are 8x8 sprites, a tile each (1-10: seat s's is 1 + s, drawn
; again when its angle changes), in its seat's colour with a steel track
; and a white glint. Yours, the turn's and the rest in that order take
; them while a line has room for one (8 a line).
;
; The HUD is the text's last three rows (scr.asm's letters, palette 1):
; values white, labels yellow, a seat's name in its colour, on navy. A
; tank's talk is a balloon of letters over it, black on white: the cells it
; covers are the balloon's till it goes, then drawn from the store again.
;
; The frame's VDP work is the IRQ's (vdpf, smsint.asm): the sprite table,
; the colours (the round's, the flash's white, the fire's and the stars'
; cycles) and the shake (the horizontal scroll, the sprites with it).
;
; C's arguments are on the stack (zsdcc: right to left, a byte as a byte);
; IX and IY are kept.

        SECTION code_user

        INCLUDE "proto.inc"

        PUBLIC  _field_init, _field_loading, _field_open, _field_off, _field_column, _field_columns
        PUBLIC  _field_plot, _field_get, _field_swap, _field_span, _field_ellipse
        PUBLIC  _field_tank, _field_colours, _field_name
        PUBLIC  _field_status, _field_status_hi, _field_status_clear, _field_balloon
        PUBLIC  _field_flash, _field_fire, _sms_idle, _sms_frame, vdpf
        PUBLIC  _shake_n, _flash_n, _seat_ix, ovl_paint, cell_dirty, shape_paint

        EXTERN  _fx, _fy, _fx1, _fv, _fold, _fent, _ex, _ey, _erx, _ery, _emode
        EXTERN  _fld_cols, _fld_ncols
        EXTERN  _fld_col, _fld_plot, _fld_get, _fld_swap, _fld_hspan, _fld_ellipse
        EXTERN  _fld_clear, _fld_mark_all, _fld_flush1
        EXTERN  _fld_live, _fld_dirty, _fld_rowd, _fld_nd
        EXTERN  _ln_sky, _ln_dirt, sid_make, _st_stars, _sms_crust
        EXTERN  sparks_, plive
        EXTERN  _st, _my_seat, _tk, _frame_count, _inp_field
        EXTERN  sc_blank, put_glyph, pc_pal, pc_inv, scr_cram, _seat_rgb, r1, reg, pg_bar
        EXTERN  tc_map, tc_unmap, tc_flush, fl_row, _tc_tile, tc_name
        EXTERN  vdp_wr, _va_addr, _va_src, _va_len, _va_cram, _frame_wait

        defc    FIELD_H = 168
        defc    FROWS = 21
        defc    HUD_ROW = 21
        defc    VR_SAT = $3F00
        defc    VDP_DATA = $BE
        defc    VDP_CTRL = $BF
        defc    R1_ON = $E0
        defc    R1_OFF = $A0
        defc    TD_SIZE = 6             ; td: x (2), y, angle, on, me
        defc    OV_SIZE = 27            ; ovl: x (2), y, the masks (24)
        defc    NP = 16                 ; sparks (src/z80/pt.asm's)
        defc    SP_SIZE = 16
        defc    SP_LIFE = 9
        defc    SP_SH = 11
        defc    SP_OX = 12
        defc    SP_OY = 14
        defc    NSPR = SEATS + NP       ; sprites: the tanks, then the sparks
        defc    LMAX = 30               ; a balloon's line
        defc    PAL_HUD = 15
        defc    PAL_HUDHI = 16
        defc    PAL_BAL = 17
        defc    PAL_HSEAT = 18
        defc    C1_WHITE = 11
        defc    C1_GREY = 13
        defc    C1_STEEL = 14

; ---- the round's look -----------------------------------------------------------

; SKY_*: the sky's colours, top down (the count first, then up to 5)
skies:  defb    1, $39, 0, 0, 0, 0                      ; plain: sky blue
        defb    5, $20, $30, $34, $39, $3E              ; gradient: deep blue to pale
        defb    2, $00, $10, 0, 0, 0                    ; stars: black, a navy horizon
        defb    4, $15, $25, $2A, $3A, 0                ; storm: slate to grey
        defb    5, $10, $21, $22, $07, $0B              ; sunset: blue, purple, magenta, red, gold
        defb    3, $00, $01, $02, 0, 0                  ; cavern: black to a red glow
        defb    1, $00, 0, 0, 0, 0                      ; black
        defb    5, $20, $30, $34, $39, $3E              ; random: as gradient
; the dirt: its crust (the top two pixels of every surface: grass, snow,
; sand...), then its strata light to dark, each palette
dirts:  defb    $0D, $0B, $06, $05                      ; earth: grass over ochre, brown, olive
        defb    $2E, $0C, $08, $04                      ; grass: pale green over greens
        defb    $2A, $15, $25, $14                      ; rock: light grey over slate, steel, teal
        defb    $0B, $07, $03, $02                      ; mars: gold over orange, red, maroon
        defb    $2F, $1F, $1A, $15                      ; sand: pale over sand, tan, khaki
        defb    $3F, $3E, $39, $24                      ; ice: snow over frost, ice, blue
        defb    $3B, $37, $22, $21                      ; alien: pink over magenta, purple, violet
        defb    $0F, $0B, $07, $03                      ; lava: yellow over gold, orange, red
; the fire's three, cycled
fires:  defb    $03, $07, $0F

; styles: the round's look for sky C, dirt palette B: palette 0 (cram0),
; the line tables, the styles
styles: ld      a,c
        and     7
        ld      (sy_sky),a
        ld      a,b
        and     7
        ld      (sy_dirt),a
        ld      hl,cram0        ; black, then the sky's (the last repeated)
        ld      (hl),0
        inc     hl
        ld      a,(sy_sky)
        ld      e,a
        add     a,a
        add     a,e
        add     a,a             ; x 6
        ld      e,a
        ld      d,0
        push    hl
        ld      hl,skies
        add     hl,de
        ld      a,(hl)
        ld      (sy_n),a
        inc     hl
        ex      de,hl
        pop     hl
        ld      b,5
        ld      c,0
sy_c:   ld      a,(sy_n)
        cp      c
        jr      z,sy_cl
        jr      c,sy_cl
        ld      a,(de)
        ld      (sy_last),a
        inc     de
        jr      sy_cw
sy_cl:  ld      a,(sy_last)
sy_cw:  ld      (hl),a
        inc     hl
        inc     c
        djnz    sy_c
        ld      (hl),$3F        ; 6: a star
        inc     hl
        ld      a,(sy_dirt)     ; 7-10: the dirt's
        add     a,a
        add     a,a
        ld      e,a
        ld      d,0
        push    hl
        ld      hl,dirts
        add     hl,de
        pop     de
        ld      bc,4
        ldir
        ld      hl,fires        ; 11-13: the fire's
        ld      bc,3
        ldir
        ex      de,hl
        ld      (hl),0          ; 14, 15 (spare)
        inc     hl
        ld      (hl),0
        ; the lines: the sky's band y * n / 168, the dirt's stratum by depth
        ld      a,(sy_sky)
        cp      SKY_STARS
        ld      a,0
        jr      nz,sy_ns
        inc     a
sy_ns:  ld      (_st_stars),a
        call    tc_map          ; (the line tables are in half 1)
        ld      hl,_ln_sky
        ld      de,_ln_dirt
        ld      b,0             ; y
        ld      c,0             ; y * n, to compare with 168 per band
sy_l:   push    bc
        ld      a,(sy_n)        ; the band: (y * n) / 168
        ld      c,a
        ld      a,b
        call    mul_div         ; A = y * C / 168
        inc     a
        ld      (hl),a
        inc     hl
        ld      a,(_sms_crust)  ; the crust's look: three broad strata under it
        or      a
        jr      z,sy_old
        ld      a,b
        ld      c,3
        call    mul_div
        add     a,8
        jr      sy_dset
sy_old: ld      a,b             ; the stratum: y * 4 / 168, lighter in every third 9-line band
        ld      c,4
        call    mul_div
        ld      c,a
        ld      a,b
        push    bc
        ld      b,0
sy_9:   sub     9               ; (y / 9) mod 3
        jr      c,sy_9x
        inc     b
        jr      sy_9
sy_9x:  ld      a,b
sy_3:   sub     3
        jr      nc,sy_3
        add     a,3
        pop     bc
        cp      1
        ld      a,c
        jr      nz,sy_s
        or      a
        jr      z,sy_s
        dec     a
sy_s:   add     a,7
sy_dset:
        ld      (de),a
        inc     de
        pop     bc
        inc     b
        ld      a,b
        cp      FIELD_H
        jr      c,sy_l
        call    tc_unmap
        push    ix
        push    iy
        call    sid_make
        pop     iy
        pop     ix
        ld      a,1
        ld      (cram_dirty),a
        ret

; mul_div: A = A * C / 168 (A < 168, C <= 5: under C) (B, DE, HL kept)
mul_div:
        push    hl
        push    de
        ld      l,a
        ld      h,0
        ld      d,h
        ld      e,l
        ld      a,c
        ld      hl,0
md_m:   or      a
        jr      z,md_d
        add     hl,de
        dec     a
        jr      md_m
md_d:   ld      de,168
        xor     a
md_l:   or      a
        sbc     hl,de
        jr      c,md_x
        inc     a
        jr      md_l
md_x:   pop     de
        pop     hl
        ret

; ---- field.h ----------------------------------------------------------------------

; void field_init(void): all sky (the store), the look a gradient
_field_init:
        ld      a,1             ; (the look: the dirt's crust)
        ld      (_sms_crust),a
        xor     a
        ld      (_fld_live),a
        call    _fld_clear
        ld      bc,SKY_GRADIENT
        jp      styles

; void field_loading(void): (the store isn't the screen: a field that comes
; over a text screen fills it, and field_open draws it)
_field_loading:
        ret

; void field_open(uint8_t sky, uint8_t pal): the field screen on, drawn
_field_open:
        ld      hl,2
        add     hl,sp
        ld      c,(hl)
        inc     hl
        ld      b,(hl)
        push    bc
        call    sc_blank        ; (blanked; the cache and the names empty)
        pop     bc
        call    styles
        ld      hl,_fld_dirty
        ld      de,_fld_dirty+1
        ld      bc,FROWS * 32 - 1
        ld      (hl),0
        ldir
        xor     a
        ld      (b_on),a
        ld      (fire_on),a
        ld      (flash_on),a
        ld      a,FROWS
        ld      (b_tr),a
        xor     a
        inc     a
        ld      (_inp_field),a
        ld      (_fld_live),a
        call    hud_reset
        call    _fld_mark_all
fo_draw:
        call    _fld_flush1     ; (blanked)
        ld      a,l
        or      a
        jr      nz,fo_draw
        ld      hl,tile_dirty   ; (a title may have had tiles 1-31: all again)
        ld      b,SEATS
fo_td:  ld      (hl),1
        inc     hl
        djnz    fo_td
        ld      hl,spark_t
        ld      de,SPARK_TILE * 32
        ld      bc,3 * 32
        call    vdp_wr
        ld      a,1
        ld      (tanks_dirty),a
        ld      (cram_dirty),a
        call    _frame_wait     ; (the colours in, from the IRQ)
        ld      a,R1_ON
        jp      r1

; void field_off(void): a text screen comes up (sc_blank)
_field_off:
        xor     a
        ld      (ovl_n),a
        ld      (_fld_live),a
        ld      (b_on),a
        ld      (sat_n),a
        ld      (fire_on),a
        ld      (_shake_n),a
        ret

; void field_column(uint16_t x, const uint8_t *entry)
_field_column:
        ld      hl,2
        add     hl,sp
        call    word
        ld      (_fx),de
        call    word
        ld      (_fent),de
        jp      _fld_col

; void field_columns(uint16_t x, const uint8_t *entry, uint16_t n)
_field_columns:
        ld      hl,2
        add     hl,sp
        call    word
        ld      (_fx),de
        call    word
        ld      (_fent),de
        call    word            ; DE n
fcs_l:  ld      a,d
        or      e
        ret     z
        ld      hl,128
        ex      de,hl
        or      a
        sbc     hl,de           ; n - 128
        jr      nc,fcs_k
        add     hl,de
        ld      e,l
        ld      hl,0
fcs_k:  push    hl
        ld      a,e
        ld      (_fld_ncols),a
        call    _fld_cols
        pop     de
        jr      fcs_l

; word: DE = (HL), HL past it
word:   ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ret

; xy: fx, fy from the stack at HL
xy:     call    word
        ld      (_fx),de
        call    word
        ld      (_fy),de
        ret

; void field_plot(int16_t x, int16_t y, uint8_t v)
_field_plot:
        ld      hl,2
        add     hl,sp
        call    xy
        ld      a,(hl)
        ld      (_fv),a
        jp      _fld_plot

; uint8_t field_get(int16_t x, int16_t y)
_field_get:
        ld      hl,2
        add     hl,sp
        call    xy
        jp      _fld_get

; void field_swap(int16_t x, int16_t y, uint8_t from, uint8_t to)
_field_swap:
        ld      hl,2
        add     hl,sp
        call    xy
        ld      a,(hl)
        ld      (_fold),a
        inc     hl
        ld      a,(hl)
        ld      (_fv),a
        jp      _fld_swap

; void field_span(int16_t y, int16_t x0, int16_t x1, uint8_t v)
_field_span:
        ld      hl,2
        add     hl,sp
        call    word
        ld      (_fy),de
        call    word
        ld      (_fx),de
        call    word
        ld      (_fx1),de
        ld      a,(hl)
        ld      (_fv),a
        jp      _fld_hspan

; void field_ellipse(int16_t x, int16_t y, uint8_t rx, uint8_t ry, uint8_t v, uint8_t mode)
_field_ellipse:
        ld      hl,2
        add     hl,sp
        call    word
        ld      (_ex),de
        call    word
        ld      (_ey),de
        ld      a,(hl)          ; rx: 0 is 1
        or      a
        jr      nz,fe_rx
        inc     a
fe_rx:  ld      (_erx),a
        inc     hl
        ld      a,(hl)          ; ry: 1-200
        or      a
        jr      nz,fe_ry
        inc     a
fe_ry:  cp      201
        jr      c,fe_ry2
        ld      a,200
fe_ry2: ld      (_ery),a
        inc     hl
        ld      a,(hl)
        ld      (_fv),a
        inc     hl
        ld      a,(hl)
        ld      (_emode),a
        jp      _fld_ellipse

; void field_flash(uint8_t frames)
_field_flash:
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        ld      (_flash_n),a
        ret

; void field_fire(uint8_t on): the fire's colours cycle
_field_fire:
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        ld      (fire_on),a
        ret

; cell_dirty: cell HL (r * 32 + c) dirty (keeps BC, DE)
cell_dirty:
        push    de
        ld      de,_fld_dirty
        add     hl,de
        bit     0,(hl)
        jr      nz,cd_x
        set     0,(hl)
        or      a
        sbc     hl,de
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ld      a,h             ; its row: cell >> 5
        ld      hl,_fld_rowd
        call    add_hl_a
        inc     (hl)
        ld      hl,(_fld_nd)
        inc     hl
        ld      (_fld_nd),hl
cd_x:   pop     de
        ret

add_hl_a:
        add     a,l
        ld      l,a
        ret     nc
        inc     h
        ret

; ---- the tanks --------------------------------------------------------------------

; void field_tank(uint8_t s, uint8_t draw): tk[s] as it is drawn (or off)
_field_tank:
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        cp      SEATS
        ret     nc
        ld      (ft_s),a
        ld      c,a
        inc     hl
        ld      a,(hl)          ; draw
        or      a
        jr      z,ft_o
        call    tk_at
        ld      a,(hl)          ; tk[s].on
        or      a
        jr      z,ft_o
        ld      a,1
ft_o:   ld      (ft_new+4),a
        ld      hl,ft_new       ; x, y, angle: tk's, or 0s when off
        ld      b,4
ft_z:   ld      (hl),0
        inc     hl
        djnz    ft_z
        inc     hl
        ld      (hl),0          ; (me)
        or      a
        jr      z,ft_cmp
        ld      a,(ft_s)
        ld      c,a
        call    tk_at
        inc     hl
        ld      de,ft_new
        ld      bc,4
        ldir
        ld      a,(ft_s)
        ld      c,a
        ld      a,(_my_seat)
        cp      c
        jr      nz,ft_cmp
        ld      a,1
        ld      (ft_new+5),a
ft_cmp: ld      a,(ft_s)        ; the same as drawn: nothing
        ld      c,a
        call    td_at
        ld      de,ft_new
        ld      b,TD_SIZE
ft_c:   ld      a,(de)
        cp      (hl)
        jr      nz,ft_cp
        inc     de
        inc     hl
        djnz    ft_c
        ret
ft_cp:  ld      a,(ft_s)
        ld      c,a
        call    td_at
        ex      de,hl
        ld      hl,ft_new
        ld      bc,TD_SIZE
        ldir
        ld      a,(ft_s)        ; its picture again (its angle, the pennant)
        ld      hl,tile_dirty
        call    add_hl_a
        ld      (hl),1
; void field_colours(void): the tanks moved: their sprites again
_field_colours:
        ld      a,1
        ld      (tanks_dirty),a
        ret

; tk_at: HL = &tk[C] (5 bytes each: on, x (2), y, angle)
tk_at:  ld      a,c
        add     a,a
        add     a,a
        add     a,c
        ld      hl,_tk
        jp      add_hl_a

; td_at: HL = &td[C]
td_at:  ld      a,c
        add     a,a
        add     a,c
        add     a,a
        ld      hl,td
        jp      add_hl_a

; tank_tile: seat C's tile (1 + C) drawn: its body in its colour, the track
; steel, a glint, the barrel at its angle, yours with a pennant
tank_tile:
        push    bc
        call    tt_masks
        jp      tt_draw

; tt_masks: tt_m = seat C's tank as three masks a row (S its colour, W
; white, K steel), from its td record
tt_masks:
        call    td_at
        inc     hl
        inc     hl
        inc     hl
        ld      a,(hl)          ; the angle: its tenth, 0-18
        cp      181
        jr      c,tt_a
        ld      a,180
tt_a:   add     a,5
        ld      b,-1
tt_d:   inc     b
        sub     10
        jr      nc,tt_d
        inc     hl
        inc     hl
        ld      a,(hl)          ; me
        ld      (tt_me),a
        ld      a,b
        ld      (tt_ang),a
        ld      hl,tt_m         ; the masks: S (seat), W (white), K (steel)
        ld      de,tt_m+1
        ld      bc,23
        ld      (hl),0
        ldir
        ld      hl,body         ; rows 3-7
        ld      de,tt_m + 3 * 3
        ld      bc,15
        ldir
        ld      a,(tt_ang)      ; the barrel: 3 pixels
        ld      b,a
        add     a,a
        add     a,b
        add     a,a             ; x 6
        ld      hl,barrels
        call    add_hl_a
        ld      b,3
tt_b:   push    bc
        ld      a,(hl)          ; x
        inc     hl
        ld      c,(hl)          ; y
        inc     hl
        push    hl
        call    tt_px           ; S
        pop     hl
        pop     bc
        djnz    tt_b
        ld      a,(tt_me)       ; the pennant: white, on the side away from it
        or      a
        ret     z
        ld      a,(tt_ang)
        cp      10
        ld      a,0
        jr      c,tt_p1
        ld      a,6
tt_p1:  ld      (tt_px0),a
        ld      hl,pennant
        ld      b,4
tt_p:   push    bc
        ld      a,(tt_px0)
        add     a,(hl)
        inc     hl
        ld      c,(hl)
        inc     hl
        push    hl
        call    tt_pw           ; W
        pop     hl
        pop     bc
        djnz    tt_p
        ret
tt_draw:
        pop     bc              ; C the seat: its colour 1 + C
        ld      a,c
        inc     a
        ld      (tt_si),a
        ld      hl,tt_m
        ld      de,tank_t
        ld      b,8
tt_r:   push    bc
        ld      a,(hl)
        ld      (mf_s),a        ; S
        inc     hl
        ld      a,(hl)
        ld      (mf_w),a        ; W
        inc     hl
        ld      a,(hl)
        ld      (mf_k),a        ; K
        inc     hl
        push    hl
        ld      a,(tt_si)
        ld      h,a
        ld      l,C1_WHITE
        ld      c,C1_STEEL
        ld      b,4
tt_pl:  xor     a
        srl     h
        jr      nc,tt_1
        push    hl
        ld      hl,mf_s
        or      (hl)
        pop     hl
tt_1:   srl     l
        jr      nc,tt_2
        push    hl
        ld      hl,mf_w
        or      (hl)
        pop     hl
tt_2:   srl     c
        jr      nc,tt_3
        push    hl
        ld      hl,mf_k
        or      (hl)
        pop     hl
tt_3:   ld      (de),a
        inc     de
        djnz    tt_pl
        pop     hl
        pop     bc
        djnz    tt_r
        ld      a,(tt_si)       ; into the VRAM: tile 1 + seat
        ld      l,a
        ld      h,0
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ex      de,hl
        ld      hl,tank_t
        ld      bc,32
        jp      vdp_wr

; tt_px: S pixel at x A, y C; tt_pw: a W one (in the 8x8, else nothing)
tt_px:  ld      e,0
        jr      tt_pg
tt_pw:  ld      e,1
tt_pg:  cp      8
        ret     nc
        ld      d,a
        ld      a,c
        cp      8
        ret     nc
        ld      b,a             ; row * 3 + E
        add     a,a
        add     a,b
        add     a,e
        ld      hl,tt_m
        call    add_hl_a
        ld      a,$80
        inc     d
tp_s:   dec     d
        jr      z,tp_o
        rrca
        jr      tp_s
tp_o:   or      (hl)
        ld      (hl),a
        ret

; the body, rows 3-7: S, W, K a row
body:   defb    %00111000, %00000000, %00000000      ; ..SSS...
        defb    %01011100, %00100000, %00000000      ; .SWSSS..
        defb    %11111110, %00000000, %00000000      ; SSSSSSS.
        defb    %00000000, %00000000, %11111110      ; KKKKKKK.
        defb    %00000000, %00000000, %10101010      ; K.K.K.K.
; the barrel's three pixels at each tenth of 180 degrees (x, y), as the
; Adam's: x = 3 + round(k cos a), y = 3 - round(k sin a * 64 / 73)
barrels:
        defb    4,3, 5,3, 6,3,   4,3, 5,3, 6,3,   4,3, 5,2, 6,2
        defb    4,3, 5,2, 6,2,   4,2, 5,2, 5,1,   4,2, 4,2, 5,1
        defb    4,2, 4,1, 5,1,   3,2, 4,1, 4,1,   3,2, 3,1, 4,0
        defb    3,2, 3,1, 3,0,   3,2, 3,1, 2,0,   3,2, 2,1, 2,1
        defb    3,2, 2,1, 2,1,   2,2, 2,2, 1,1,   2,2, 1,2, 1,1
        defb    2,3, 1,2, 0,2,   2,3, 1,2, 0,2,   2,3, 1,3, 0,3
        defb    2,3, 1,3, 0,3
; the sparks' sprites (tiles 11-13): a hot one, a warm one, a cooling one
spark_t:
        defb    $00, $00, $00, $00, $00, $00, $00, $00, $00, $10, $00, $00, $10, $38, $00, $10     ; hot
        defb    $00, $10, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
        defb    $00, $00, $00, $00, $00, $00, $00, $00, $00, $10, $10, $00, $00, $38, $28, $00     ; warm
        defb    $00, $10, $10, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
        defb    $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $28, $10, $10, $00     ; cool
        defb    $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00, $00
        defc    SPARK_TILE = 11

; the pennant's pixels (dx, y): a flag on a staff
pennant:
        defb    0,0, 1,0, 0,1, 0,2

; allocate: which tanks get sprites (8 a line): yours, the turn's, the rest
allocate:
        ld      a,(ovl_n)       ; the old overflow's cells: the store's again
        or      a
        jr      z,al_new
        ld      b,a
        ld      hl,ovl
al_old: push    bc
        push    hl
        ld      c,0
        call    ovl_cells
        pop     hl
        ld      de,OV_SIZE
        add     hl,de
        pop     bc
        djnz    al_old
al_new: xor     a
        ld      (ovl_n),a
        ld      hl,lines
        ld      de,lines+1
        ld      bc,FIELD_H + 8 - 1
        ld      (hl),0
        ldir
        xor     a
        ld      (w_n),a
        ld      a,(_my_seat)
        cp      SEATS
        call    c,al_one
        ld      a,(_st+OFF_TURN)
        cp      SEATS
        jr      nc,al_rest
        ld      hl,_my_seat
        cp      (hl)
        call    nz,al_one
al_rest:
        xor     a
al_r:   push    af
        ld      hl,_my_seat
        cp      (hl)
        jr      z,al_skip
        ld      hl,_st+OFF_TURN
        cp      (hl)
        call    nz,al_one
al_skip:
        pop     af
        inc     a
        cp      SEATS
        jr      c,al_r
        call    al_sparks
        di                      ; (the IRQ reads sat_: all of it at once)
        ld      hl,w_n
        ld      de,sat_n
        ld      bc,1 + NSPR * 3
        ldir
        ld      a,1
        ld      (sat_dirty),a
        ei
        xor     a
        ld      (tanks_dirty),a
        ret

; al_one: tank A: a sprite if its lines have room (M2: else not drawn)
al_one: ld      c,a
        ld      (al_s),a
        call    td_at
        ld      (al_td),hl
        inc     hl
        inc     hl
        inc     hl
        inc     hl
        ld      a,(hl)          ; on
        or      a
        ret     z
        ld      a,(al_s)        ; its picture first, if it changed
        ld      hl,tile_dirty
        call    add_hl_a
        ld      a,(hl)
        or      a
        jr      z,ao_t
        ld      (hl),0
        ld      a,(al_s)
        ld      c,a
        call    tank_tile
ao_t:   ld      hl,(al_td)
        inc     hl
        inc     hl
        ld      a,(hl)          ; y (at most the field's lines)
        cp      FIELD_H + 1
        jr      c,ao_y
        ld      a,FIELD_H
ao_y:   ld      (al_y),a
        ld      hl,lines        ; its lines: y-8 .. y-1, lines[y .. y+7]
        call    add_hl_a
        ld      (al_l),hl
        ld      b,8
        xor     a
ao_m:   cp      (hl)
        jr      nc,ao_m1
        ld      a,(hl)
ao_m1:  inc     hl
        djnz    ao_m
        cp      8
        jp      nc,ao_ovl
        ld      hl,(al_l)
        ld      b,8
ao_i:   inc     (hl)
        inc     hl
        djnz    ao_i
        ld      a,(w_n)         ; w_y[n] = y - 9, w_x[n] = x - 3, its tile
        ld      e,a
        ld      d,0
        ld      hl,w_y
        add     hl,de
        ld      a,(al_y)
        sub     9
        ld      (hl),a
        ld      hl,w_x
        add     hl,de
        add     hl,de
        push    hl
        ld      hl,(al_td)
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      hl,-3
        add     hl,de
        ld      a,h             ; (off the left: not shown; the SMS has no early clock)
        or      a
        pop     hl
        ret     nz
        ld      (hl),e
        dec     (hl)
        dec     (hl)
        dec     (hl)
        inc     hl
        ld      a,(al_s)
        inc     a
        ld      (hl),a
        ld      hl,w_n
        inc     (hl)
        ret

; al_sparks: the sparks shown (pt.asm: SP_SH) after the tanks, a sprite each:
; hot, warm, cooling by the life they have left (a line too full drops them)
al_sparks:
        ld      ix,sparks_
        ld      b,NP
as_l:   push    bc
        ld      a,(ix+SP_SH)
        or      a
        jr      z,as_n
        ld      a,(w_n)
        ld      e,a
        ld      d,0
        ld      hl,w_y
        add     hl,de
        ld      a,(ix+SP_OY)    ; the glow's middle at the sprite's (3, 3)
        sub     4
        ld      (hl),a
        ld      hl,w_x
        add     hl,de
        add     hl,de
        ld      a,(ix+SP_OX+1)
        or      a
        jr      nz,as_n         ; (x past 255: none)
        ld      a,(ix+SP_OX)
        sub     3
        jr      c,as_n
        ld      (hl),a
        inc     hl
        ld      a,(ix+SP_LIFE)
        ld      c,SPARK_TILE + 2
        cp      6
        jr      c,as_t
        dec     c
        cp      14
        jr      c,as_t
        dec     c
as_t:   ld      (hl),c
        ld      hl,w_n
        inc     (hl)
as_n:   ld      de,SP_SIZE
        add     ix,de
        pop     bc
        djnz    as_l
        ret

; ao_ovl: tank al_s has no sprite: into the tiles (two at most: a ninth and
; a tenth on a line), its colour palette 0's 14 + k
ao_ovl: ld      a,(ovl_n)
        cp      2
        ret     nc
        ld      (ov_k),a
        ld      c,a             ; its record
        ld      b,OV_SIZE
        xor     a
ao_k:   add     a,c
        djnz    ao_k
        ld      hl,ovl
        call    add_hl_a
        push    hl
        ld      hl,(al_td)      ; x, y
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        pop     hl
        push    hl
        ld      (hl),e
        inc     hl
        ld      (hl),d
        inc     hl
        ld      a,(al_y)
        ld      (hl),a
        inc     hl
        push    hl
        ld      a,(al_s)        ; its masks
        ld      c,a
        call    tt_masks
        pop     de
        ld      hl,tt_m
        ld      bc,24
        ldir
        ld      a,(ov_k)        ; its colour into palette 0
        add     a,14
        ld      hl,cram0
        call    add_hl_a
        push    hl
        ld      a,(al_s)
        inc     a
        ld      hl,_seat_rgb
        call    add_hl_a
        ld      a,(hl)
        pop     hl
        ld      (hl),a
        ld      a,1
        ld      (cram_dirty),a
        ld      hl,ovl_n
        inc     (hl)
        pop     hl
        ld      c,1
; ovl_cells: the cells of overflow record HL: CF_TANK set (C 1) or cleared
; (C 0), each dirty
ovl_cells:
        ld      e,(hl)          ; x - 3: the left
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      a,(hl)          ; y - 8: the top
        sub     8
        jr      nc,oc_y
        xor     a
oc_y:   ld      (oc_y0),a
        ld      hl,-3
        add     hl,de
        bit     7,h
        jr      z,oc_x
        ld      hl,0
oc_x:   ld      a,l             ; (x < 256)
        ld      (oc_x0),a
        ld      a,c
        ld      (oc_set),a
        ld      a,(oc_y0)       ; rows: top / 8 .. (top + 7) / 8 (under 21)
        rrca
        rrca
        rrca
        and     31
        ld      b,a
oc_r:   ld      a,b
        cp      FROWS
        ret     nc
        ld      a,(oc_x0)       ; columns: left / 8 .. (left + 7) / 8
        rrca
        rrca
        rrca
        and     31
        ld      c,a
oc_c:   push    bc
        ld      l,b             ; the cell: r * 32 + c
        ld      h,0
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ld      a,c
        call    add_hl_a
        push    hl
        ld      de,_fld_dirty
        add     hl,de
        ld      a,(oc_set)
        or      a
        jr      z,oc_clr
        set     2,(hl)
        jr      oc_d
oc_clr: res     2,(hl)
oc_d:   pop     hl
        call    cell_dirty
        pop     bc
        ld      a,(oc_x0)       ; the last column?
        add     a,7
        jr      c,oc_cx         ; (past the right: the last)
        rrca
        rrca
        rrca
        and     31
        cp      c
        jr      z,oc_cx
        inc     c
        jr      oc_c
oc_cx:  ld      a,(oc_y0)       ; the last row?
        add     a,7
        rrca
        rrca
        rrca
        and     31
        cp      b
        ret     z
        inc     b
        jr      oc_r

; ovl_paint: the overflow tanks over cell (row B, column C)'s tile in
; tc_tile (render.asm, for a CF_TANK cell): each record's masks, S and W in
; its colour (14 + k), K black (0)
ovl_paint:
        ld      a,(ovl_n)
        or      a
        ret     z
        ld      (op_n),a
        ld      a,b
        add     a,a
        add     a,a
        add     a,a
        ld      (op_cy),a       ; the cell's top line
        ld      a,c
        add     a,a
        add     a,a
        add     a,a
        ld      (op_cx),a       ; its left
        ld      hl,ovl
        xor     a
        ld      (op_k),a
op_t:   push    hl
        call    op_one
        pop     hl
        ld      de,OV_SIZE
        add     hl,de
        ld      a,(op_k)
        inc     a
        ld      (op_k),a
        ld      hl,op_n
        cp      (hl)
        ld      hl,ovl + OV_SIZE
        jr      c,op_t
        ret

; op_one: record HL over the cell
op_one: ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        push    hl
        ld      hl,-3
        add     hl,de           ; the tank's left (16-bit)
        ld      (op_tx),hl
        pop     hl
        ld      a,(hl)          ; its foot's line: rows y-8 .. y-1
        sub     8
        ld      (op_ty),a
        inc     hl
        ld      (op_m),hl       ; its masks
        ld      b,0             ; the cell's line
ol_l:   ld      a,(op_cy)
        add     a,b
        ld      c,a
        ld      a,(op_ty)       ; the tank's row: line - top, 0-7
        ld      d,a
        ld      a,c
        sub     d
        jr      c,ol_n
        cp      8
        jr      nc,ol_n
        ld      e,a
        add     a,a
        add     a,e             ; x 3
        ld      hl,(op_m)
        call    add_hl_a
        ld      a,(hl)          ; S | W: the colour's pixels
        inc     hl
        or      (hl)
        ld      (op_c),a
        inc     hl
        ld      a,(hl)
        ld      (op_b),a        ; K: black
        call    op_shift        ; both into the cell's columns
        ld      a,b             ; the tile's row: 4 planes
        add     a,a
        add     a,a
        ld      hl,_tc_tile
        call    add_hl_a
        ld      a,(op_k)
        add     a,14
        ld      c,a             ; the colour's index
        ld      a,(op_c)
        ld      d,a
        ld      a,(op_b)
        or      d
        cpl
        ld      e,a             ; the pixels kept
        push    bc
        ld      b,4
ol_p:   ld      a,(hl)
        and     e
        srl     c
        jr      nc,ol_q
        or      d
ol_q:   ld      (hl),a
        inc     hl
        djnz    ol_p
        pop     bc
ol_n:   inc     b
        ld      a,b
        cp      8
        jr      c,ol_l
        ret

; op_shift: op_c and op_b (the tank's columns tx..tx+7) as the cell's
; (cx..cx+7)
op_shift:
        ld      hl,(op_tx)
        ld      a,(op_cx)
        ld      e,a
        ld      d,0
        or      a
        sbc     hl,de           ; tank - cell: -7..7, else none
        ld      a,h
        or      a
        jr      z,os_pos
        inc     a
        jr      nz,os_none
        ld      a,l             ; negative: left by cell - tank
        cp      -7
        jr      c,os_none
        neg
os_l:   ld      hl,op_c
        sla     (hl)
        ld      hl,op_b
        sla     (hl)
        dec     a
        jr      nz,os_l
        ret
os_pos: ld      a,l
        or      a
        ret     z
        cp      8
        jr      nc,os_none
os_r:   ld      hl,op_c
        srl     (hl)
        ld      hl,op_b
        srl     (hl)
        dec     a
        jr      nz,os_r
        ret
os_none:
        xor     a
        ld      (op_c),a
        ld      (op_b),a
        ret

; ---- the frame ---------------------------------------------------------------------

; void sms_idle(void): while waiting for a frame: a dirty cell drawn
_sms_idle:
        ld      a,(_fld_live)
        or      a
        ret     z
        jp      _fld_flush1

; void sms_frame(void): a frame came: the sprites placed (the IRQ puts them in)
_sms_frame:
        ld      a,(_fld_live)
        or      a
        ret     z
        ld      a,(_shake_n)    ; a shake starts: the rows its wrap shows
        or      a
        ld      hl,shake_prep
        jr      nz,sf_sh
        ld      (hl),a
        jr      sf_go
sf_sh:  ld      a,(hl)
        or      a
        call    z,shake_rows
sf_go:
        ld      a,(plive)       ; sparks flying (or just gone): the sprites again
        ld      hl,sp_was
        ld      b,(hl)
        ld      (hl),a
        or      b
        ld      hl,tanks_dirty
        or      (hl)
        ret     z
        push    ix
        call    allocate
        pop     ix
        ret

; shake_rows: under the screen for the vertical shake: row 27 (over the top,
; when it moves down) the field's row 0, row 24 (under the bottom) the HUD's
; blank
shake_rows:
        ld      (hl),1
        call    tc_map
        ld      bc,0            ; row 0's names
        call    tc_name
        ld      de,$3800 + 27 * 64
        ld      bc,64
        call    vdp_wr
        ld      bc,23 << 8 | 31 ; the HUD's last cell (blank): row 24 all of it
        call    tc_name
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      hl,shk_row
        ld      b,32
sr_l:   ld      (hl),e
        inc     hl
        ld      (hl),d
        inc     hl
        djnz    sr_l
        call    tc_unmap
        ld      hl,shk_row
        ld      de,$3800 + 24 * 64
        ld      bc,64
        jp      vdp_wr

; vdpf: the IRQ's frame (smsint.asm): the colours, the sprites, the scroll
vdpf:   ld      a,(_fld_live)
        or      a
        jr      nz,vf_go
        xor     a               ; (a text screen: no scroll, the colours its own)
        ld      (flash_on),a
        ret
vf_go:  ld      a,(_frame_count)
        ld      (vf_t),a
        ; the colours: the flash's white over palette 0, else the round's
        ld      a,(_flash_n)
        or      a
        jr      z,vf_nf
        dec     a
        ld      (_flash_n),a
        ld      a,(flash_on)
        or      a
        jp      nz,vf_sat
        ld      a,1
        ld      (flash_on),a
        xor     a
        call    cram_at
        ld      b,16
vf_w:   ld      a,$3F
        out     (VDP_DATA),a
        nop
        nop
        djnz    vf_w
        jp      vf_sat
vf_nf:  ld      a,(flash_on)
        ld      hl,cram_dirty
        or      (hl)
        jr      z,vf_cyc
        xor     a
        ld      (flash_on),a
        ld      (cram_dirty),a
        xor     a               ; palette 0 and 1 again
        call    cram_at
        ld      hl,cram0
        ld      b,16
        call    outb
        ld      hl,_seat_rgb
        ld      b,16
        call    outb
vf_cyc: ld      a,(fire_on)     ; the fire's colours: a step every 4 frames
        or      a
        jr      z,vf_star
        ld      a,(vf_t)
        and     3
        jr      nz,vf_star
        ld      hl,fire_ph
        ld      a,(hl)
        inc     a
        cp      3
        jr      c,vf_fp
        xor     a
vf_fp:  ld      (hl),a
        ld      c,a
        ld      a,11
        call    cram_at
        ld      b,3
vf_fl:  ld      a,c
        ld      hl,fires
        call    add_hl_a
        ld      a,(hl)
        out     (VDP_DATA),a
        inc     c
        ld      a,c
        cp      3
        jr      c,vf_f1
        ld      c,0
vf_f1:  djnz    vf_fl
vf_star:
vf_sat: ; the shake: the scrolls (both ways), and the sprites with them
        ld      de,0            ; D dy, E dx
        ld      a,(_shake_n)
        or      a
        jr      z,vf_sx
        dec     a
        ld      (_shake_n),a
        jr      z,vf_sx         ; (the last: still)
        and     3
        ld      hl,shake_x
        call    add_hl_a
        ld      e,(hl)
        ld      a,4
        call    add_hl_a
        ld      d,(hl)
vf_sx:  ld      hl,sat_dx
        ld      a,e
        cp      (hl)
        jr      nz,vf_sc
        inc     hl
        ld      a,d
        cp      (hl)
        jr      z,vf_sd
vf_sc:  ld      (sat_dx),de     ; (sat_dx, then sat_dy)
        ld      a,e
        ld      c,8
        call    vreg
        ld      a,d             ; R9: dy, up is 224 - dy
        bit     7,a
        jr      z,vf_r9
        add     a,224
vf_r9:  ld      c,9
        call    vreg
        ld      a,e             ; (column 0 masked while it moves)
        or      a
        ld      a,$06
        jr      z,vf_r0
        ld      a,$26
vf_r0:  ld      c,0
        call    vreg
        ld      a,1
        ld      (sat_dirty),a
vf_sd:  ld      a,(sat_dirty)
        or      a
        ret     z
        xor     a
        ld      (sat_dirty),a
        ld      de,VR_SAT       ; the Ys, then D0h
        ld      a,e
        out     (VDP_CTRL),a
        ld      a,d
        or      $40
        out     (VDP_CTRL),a
        ld      a,(sat_n)
        or      a
        jr      z,vf_end
        ld      b,a
        ld      hl,sat_y
        ld      a,(sat_dy)
        ld      c,a
vf_y:   ld      a,(hl)          ; (up the scroll's dy)
        sub     c
        out     (VDP_DATA),a
        inc     hl
        djnz    vf_y
vf_end: ld      a,$D0
        out     (VDP_DATA),a
        ld      a,(sat_n)
        or      a
        ret     z
        ld      de,VR_SAT + $80 ; the Xs (the shake's on them) and tiles
        ld      a,e
        out     (VDP_CTRL),a
        ld      a,d
        or      $40
        out     (VDP_CTRL),a
        ld      b,a
        ld      a,(sat_n)
        ld      b,a
        ld      hl,sat_x
        ld      a,(sat_dx)
        ld      c,a
vf_x:   ld      a,(hl)
        add     a,c
        out     (VDP_DATA),a
        inc     hl
        ld      a,(hl)
        out     (VDP_DATA),a
        inc     hl
        djnz    vf_x
        ret

; vreg: register C = A (the IRQ's: the interrupt is off already)
vreg:   out     (VDP_CTRL),a
        ld      a,c
        or      $80
        out     (VDP_CTRL),a
        ret

; cram_at: the VDP at colour A (CRAM)
cram_at:
        out     (VDP_CTRL),a
        ld      a,$C0
        out     (VDP_CTRL),a
        ret

; outb: B bytes from HL to the data port
outb:   ld      a,(hl)
        out     (VDP_DATA),a
        inc     hl
        djnz    outb
        ret

; the shake's offsets, a frame each: x, then y
shake_x:
        defb    3, 253, 2, 254
        defb    2, 255, 254, 1

; ---- the HUD: the text's last three rows ---------------------------------------

hud_reset:
        ld      hl,nm_n
        ld      b,3
hr_n:   ld      (hl),0
        inc     hl
        djnz    hr_n
        ld      hl,hud_ch
        ld      de,hud_ch + 1
        ld      bc,95
        ld      (hl),' '
        ldir
        ld      hl,0
        ld      (pg_bar),hl
        ld      (pg_bar + 2),hl
        ld      a,$FF           ; (the gauges, the rows: drawn afresh)
        ld      (g_old),a
        ld      hl,row_st
        ld      b,12
hr_st:  ld      (hl),a
        inc     hl
        djnz    hr_st
        call    tc_map
        ld      b,HUD_ROW
hr_r:   push    bc
        call    hud_row
        ld      c,0
hr_c:   ld      a,PAL_HUD
        ld      (pc_pal),a
        xor     a
        ld      (pc_inv),a
        ld      a,' '
        call    put_glyph
        inc     c
        ld      a,c
        cp      32
        jr      c,hr_c
        call    tc_flush
        pop     bc
        inc     b
        ld      a,b
        cp      HUD_ROW + 3
        jr      c,hr_r
        jp      tc_unmap

; hud_row: the span's row is B
hud_row:
        ld      a,(fl_row)
        cp      b
        ret     z
        push    bc
        call    tc_flush
        pop     bc
        ld      a,b
        ld      (fl_row),a
        ret

; void field_name(uint8_t row, uint8_t col, uint8_t n, uint8_t seat): row's
; name, n cells from col in seat's colour (field_status draws it so)
_field_name:
        ld      hl,2
        add     hl,sp
        ld      a,(hl)          ; row
        cp      3
        ret     nc
        ld      e,a
        ld      d,0
        inc     hl
        ld      a,(hl)
        push    hl
        ld      hl,nm_c
        add     hl,de
        ld      (hl),a
        pop     hl
        inc     hl
        ld      a,(hl)
        push    hl
        ld      hl,nm_n
        add     hl,de
        ld      (hl),a
        pop     hl
        inc     hl
        ld      a,(hl)
        ld      hl,nm_s
        add     hl,de
        ld      (hl),a
        ret

; void field_status(uint8_t row, uint8_t col, const char *s)
_field_status:
        xor     a
        jr      hud_text
; void field_status_hi(...): the same, to stand out
_field_status_hi:
        ld      a,1
hud_text:
        ld      (ht_hi),a
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        cp      3
        ret     nc
        ld      (ht_row),a
        inc     hl
        ld      a,(hl)
        ld      (ht_col),a
        inc     hl
        call    word
        ld      hl,ht_buf       ; the string first (where it is may go)
        ld      b,32
ht_cp:  ld      a,(de)
        ld      (hl),a
        or      a
        jr      z,ht_go
        inc     de
        inc     hl
        djnz    ht_cp
        ld      (hl),0
ht_go:  call    ht_style        ; the row drawn the same way as last time? (ht_same)
        ld      a,(ht_row)      ; the shadow: hud_ch + row * 32 + col
        rrca
        rrca
        rrca
        ld      e,a
        ld      a,(ht_col)
        add     a,e
        ld      e,a
        ld      d,0
        ld      hl,hud_ch
        add     hl,de
        ex      de,hl
        ld      hl,ht_buf
        ld      a,(ht_col)
        ld      c,a
ht_sh:  ld      a,(hl)
        or      a
        jr      z,ht_sx
        ld      a,c
        cp      32
        jr      nc,ht_sx
        ld      a,(de)          ; unchanged (and drawn the same way): FFh, skipped
        cp      (hl)
        ld      a,(hl)
        ld      (de),a
        jr      nz,ht_s1
        ld      a,(ht_same)
        or      a
        jr      z,ht_s1
        ld      (hl),$FF
ht_s1:  inc     hl
        inc     de
        inc     c
        jr      ht_sh
ht_sx:  call    gauges          ; the gauges' lengths from the shadow
        call    tc_map
        ld      a,(ht_row)
        add     a,HUD_ROW
        ld      b,a
        call    hud_row
        ld      a,(ht_col)
        ld      c,a
        ld      hl,ht_buf
ht_ch:  ld      a,(hl)
        or      a
        jr      z,ht_done
        inc     a
        jr      z,ht_skip       ; (FFh: as it was)
        ld      a,c
        cp      32
        jr      nc,ht_done
        push    hl
        call    ht_cell
        pop     hl
ht_skip:
        inc     hl
        inc     c
        jr      ht_ch
ht_done:
        ld      a,(ht_hi)       ; the gauges' cells all again (a part of the row came)
        or      a
        jr      nz,ht_end
        ld      a,(g_chg)       ; (only if a gauge's length changed)
        or      a
        jr      z,ht_end
        xor     a
        ld      (g_chg),a
        ld      a,(ht_row)
        cp      2
        jr      nc,ht_end
        ld      hl,spans
        or      a
        jr      z,ht_sp
        ld      hl,spans + 4
ht_sp:  ld      a,(hl)          ; (first, last+1) pairs, 0 0 ends
        inc     hl
        or      a
        jr      nz,ht_sp1
        ld      a,(hl)
        or      a
        jr      z,ht_end
        dec     hl
        ld      a,(hl)
        inc     hl
ht_sp1: ld      c,a
        ld      a,(hl)
        inc     hl
        ld      (ht_sl),a
ht_spc: push    hl
        call    ht_cell
        pop     hl
        inc     c
        ld      a,(ht_sl)
        cp      c
        jr      nz,ht_spc
        jr      ht_sp
ht_end: call    tc_flush
        ld      hl,0
        ld      (pg_bar),hl
        ld      (pg_bar + 2),hl
        jp      tc_unmap

; ht_style: ht_same = the row ht_row's way of drawing (hi, its name's span
; and seat) is what it was the last time; it is now
ht_style:
        ld      a,(ht_row)
        ld      e,a
        ld      d,0
        ld      hl,nm_c
        add     hl,de
        ld      a,(hl)
        ld      (st_now+1),a
        ld      hl,nm_n
        add     hl,de
        ld      a,(hl)
        ld      (st_now+2),a
        ld      hl,nm_s
        add     hl,de
        ld      a,(hl)
        ld      (st_now+3),a
        ld      a,(ht_hi)
        ld      (st_now),a
        ex      de,hl           ; row_st + row * 4
        add     hl,hl
        add     hl,hl
        ld      de,row_st
        add     hl,de
        ld      de,st_now
        ld      b,4
        ld      c,1
hs_l2:  ld      a,(de)
        cp      (hl)
        jr      z,hs_eq
        ld      c,0
        ld      (hl),a
hs_eq:  inc     hl
        inc     de
        djnz    hs_l2
        ld      a,c
        ld      (ht_same),a
        ret

; ht_cell: column C of row ht_row (B its screen row) again: its character
; (the shadow's), its palette, its gauge (BC kept)
ht_cell:
        push    bc
        call    ht_pal          ; pc_pal for column C (B again)
        call    gauge_at        ; pg_bar
        ld      a,(ht_row)
        rrca
        rrca
        rrca
        add     a,c
        ld      e,a
        ld      d,0
        ld      hl,hud_ch
        add     hl,de
        ld      a,(hl)
        pop     bc
        push    bc
        call    put_glyph
        pop     bc
        ret

; the gauges' cells by row: (first column, past the last), 0 0 ending
spans:  defb    0, 9, 17, 25    ; row 0: energy under the name, power under POW
        defb    18, 22, 0, 0    ; row 1: the wind under WIND

; gauges: the bars' lengths from the shadow: row 0's energy (columns 28-30)
; and power (21-24), row 1's wind (its direction 23, its strength 24-26)
; (layout.h's 32-column HUD)
gauges: ld      hl,hud_ch + 28
        call    num
        ld      h,d             ; energy: 72 pixels for 100: x 23 / 32
        ld      l,e
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl           ; 16e
        add     hl,de
        add     hl,de
        add     hl,de
        add     hl,de
        add     hl,de
        add     hl,de
        add     hl,de           ; 23e
        ld      a,72
        call    sh5
        ld      (g_e),a
        ld      hl,hud_ch + 21
        call    num
        ld      h,d             ; power: 64 pixels for 1000: x 33 / 512
        ld      l,e
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl           ; 32p
        add     hl,de           ; 33p
        ld      a,h             ; / 512: H / 2
        srl     a
        cp      65
        jr      c,gp_1
        ld      a,64
gp_1:   ld      (g_p),a
        ld      hl,hud_ch + 32 + 24
        call    num
        ld      a,d             ; wind: half the strength, 16 at most
        or      a
        jr      nz,gw_m
        ld      a,e
        inc     a
        srl     a
        cp      17
        jr      c,gw_1
gw_m:   ld      a,16
gw_1:   ld      b,a
        ld      a,(hud_ch + 32 + 23)
        ld      c,16            ; (lo, hi) about the middle, 16
        ld      d,16
        cp      '<'
        jr      nz,gw_r
        ld      a,16
        sub     b
        ld      c,a
        jr      gw_x
gw_r:   cp      '>'
        jr      nz,gw_x
        ld      a,16
        add     a,b
        ld      d,a
gw_x:   ld      a,c
        ld      (g_wlo),a
        ld      a,d
        ld      (g_whi),a
        ld      hl,g_e          ; changed since the last? (g_e, g_p, g_wlo, g_whi)
        ld      de,g_old
        ld      b,4
gc_l:   ld      a,(de)
        cp      (hl)
        jr      z,gc_n
        ld      a,1
        ld      (g_chg),a
gc_n:   ld      a,(hl)
        ld      (de),a
        inc     hl
        inc     de
        djnz    gc_l
        ret

; sh5: A = HL >> 5, at most A
sh5:    ld      c,a
        ld      b,5
s5_l:   srl     h
        rr      l
        djnz    s5_l
        ld      a,h
        or      a
        jr      nz,s5_m
        ld      a,l
        cp      c
        ret     c
s5_m:   ld      a,c
        ret

; num: DE = the decimal number at HL (up to 4 digits; none: 0)
num:    ld      de,0
        ld      b,4
nm_l:   ld      a,(hl)
        and     $7F
        sub     '0'
        ret     c
        cp      10
        ret     nc
        push    hl
        ld      h,d             ; DE x 10 + A
        ld      l,e
        add     hl,hl
        add     hl,hl
        add     hl,de
        add     hl,hl
        ld      e,a
        ld      d,0
        add     hl,de
        ex      de,hl
        pop     hl
        inc     hl
        djnz    nm_l
        ret

; gauge_at: pg_bar for column C of HUD row ht_row: a bar on a steel track
; under the spans (energy in the name's colour, power yellow, wind cyan);
; else none (BC kept)
gauge_at:
        ld      hl,0
        ld      (pg_bar),hl
        ld      (pg_bar + 2),hl
        ld      a,(ht_hi)
        or      a
        ret     nz
        ld      a,(ht_row)
        or      a
        jr      z,ga_r0
        dec     a
        ret     nz
        ld      a,c             ; row 1: the wind, columns 18-21
        sub     18
        ret     c
        cp      4
        ret     nc
        add     a,a
        add     a,a
        add     a,a             ; the cell's first pixel
        ld      e,a
        ld      a,(g_wlo)
        call    in_cell
        ld      (pg_bar),a
        ld      a,(g_whi)
        call    in_cell
        ld      (pg_bar + 1),a
        ld      a,3             ; cyan
        jr      ga_set
ga_r0:  ld      a,c             ; row 0: energy, columns 0-8
        cp      9
        jr      nc,ga_p
        add     a,a
        add     a,a
        add     a,a
        ld      e,a
        ld      a,(g_e)
        call    in_cell
        ld      (pg_bar + 1),a
        ld      a,(nm_n)        ; its seat's colour (none: white)
        or      a
        ld      a,C1_WHITE
        jr      z,ga_set
        ld      a,(nm_s)
        inc     a
        jr      ga_set
ga_p:   sub     17              ; power, columns 17-24
        ret     c
        cp      8
        ret     nc
        add     a,a
        add     a,a
        add     a,a
        ld      e,a
        ld      a,(g_p)
        call    in_cell
        ld      (pg_bar + 1),a
        ld      a,12            ; yellow
ga_set: ld      (pg_bar + 2),a
        ld      a,C1_STEEL
        ld      (pg_bar + 3),a
        ret

; in_cell: A = pixel A of the span as the cell's (from pixel E): 0-8
in_cell:
        sub     e
        jr      nc,ic_1
        xor     a
        ret
ic_1:   cp      9
        ret     c
        ld      a,8
        ret

; ht_pal: pc_pal (and pc_inv 0) for column C of row ht_row: hi, a name, plain
ht_pal: xor     a
        ld      (pc_inv),a
        ld      a,(ht_hi)
        or      a
        ld      a,PAL_HUDHI
        jr      nz,hp_x
        ld      a,(ht_row)
        ld      e,a
        ld      d,0
        ld      hl,nm_n
        add     hl,de
        ld      a,(hl)
        or      a
        jr      z,hp_p
        ld      b,a
        ld      hl,nm_c
        add     hl,de
        ld      a,c
        sub     (hl)
        jr      c,hp_p
        cp      b
        jr      nc,hp_p
        ld      hl,nm_s
        add     hl,de
        ld      a,(hl)
        add     a,PAL_HSEAT
        jr      hp_x
hp_p:   ld      a,PAL_HUD
hp_x:   ld      (pc_pal),a
        ld      a,(ht_row)      ; (B: the screen's row again)
        add     a,HUD_ROW
        ld      b,a
        ret

; void field_status_clear(void)
_field_status_clear:
        jp      hud_reset

; ---- the talk balloon --------------------------------------------------------------

; balloon_off: the balloon's cells the store's again
balloon_off:
        ld      a,(b_on)
        or      a
        ret     z
        xor     a
        ld      (b_on),a
        ld      a,(b_r0)
        ld      b,a
        ld      a,(b_h)
        ld      c,a
bo_r:   push    bc
        ld      l,b
        ld      h,0
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ld      a,(b_c0)
        call    add_hl_a
        ld      a,(b_w)
        ld      b,a
bo_c:   push    bc
        push    hl
        ld      de,_fld_dirty
        add     hl,de
        res     1,(hl)
        ld      a,(hl)
        and     $8F
        ld      (hl),a
        pop     hl
        push    hl
        call    cell_dirty
        pop     hl
        inc     hl
        pop     bc
        djnz    bo_c
        pop     bc
        inc     b
        dec     c
        jr      nz,bo_r
        ld      a,(b_tr)        ; the tail
        cp      FROWS
        ret     nc
        ld      a,(b_tc)
        ld      l,a
        ld      a,(b_tr)
        call    cell_at
        push    hl
        ld      de,_fld_dirty
        add     hl,de
        ld      a,(hl)
        and     $8F
        ld      (hl),a
        pop     hl
        jp      cell_dirty

; void field_balloon(uint8_t seat, const char *s, uint8_t n): n 0: away
_field_balloon:
        call    balloon_off
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        ld      (bl_s),a
        inc     hl
        call    word
        ld      a,(hl)          ; n
        or      a
        ret     z
        cp      MAX_TALK + 1
        jr      c,bl_n
        ld      a,MAX_TALK
bl_n:   ld      (bl_n_),a
        ld      c,a             ; the text first (it is in the script: the
        ld      b,0             ; cart's RAM, which the cache's half displaces)
        ex      de,hl
        ld      de,bl_buf
        ldir
        ld      a,(bl_s)
        cp      SEATS
        ret     nc
        ld      a,(_fld_live)
        or      a
        ret     z
        ld      a,(bl_n_)       ; lines, and their length
        ld      b,1
        cp      LMAX + 1
        jr      c,bl_1
        inc     b
        inc     a
        srl     a
bl_1:   ld      (bl_len),a
        ld      a,b
        ld      (b_h),a
        ld      a,(bl_len)
        add     a,2
        ld      (b_w),a
        ld      a,(bl_s)        ; its column: the tank's x / 8 - w / 2
        ld      c,a
        call    tk_at
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      a,(hl)
        ld      (bl_y),a
        ex      de,hl
        srl     h
        rr      l
        srl     h
        rr      l
        srl     h
        rr      l
        ld      a,(b_w)
        srl     a
        ld      e,a
        ld      d,0
        or      a
        sbc     hl,de
        bit     7,h
        jr      z,bl_c1
        ld      hl,0
bl_c1:  ld      a,(b_w)
        add     a,l
        cp      33
        ld      a,l
        jr      c,bl_c2
        ld      a,(b_w)
        neg
        add     a,32
bl_c2:  ld      (b_c0),a
        ld      a,(bl_y)        ; its row: the one over the tank's box
        sub     16
        jr      nc,bl_r1
        xor     a
bl_r1:  rrca
        rrca
        rrca
        and     31
        ld      b,a
        ld      a,(b_h)
        dec     a
        ld      c,a
        ld      a,b
        sub     c
        jr      nc,bl_r2
        xor     a
bl_r2:  ld      (b_r0),a
        ld      a,1
        ld      (b_on),a
        xor     a
        ld      (bl_i),a
        ld      (pc_inv),a
        ld      a,PAL_BAL
        ld      (pc_pal),a
        call    tc_map
        ld      a,(b_r0)
        ld      b,a
        ld      a,(b_h)
        ld      (bl_rows),a
bl_row: push    bc
        call    hud_row         ; (the span's row)
        ld      a,(b_c0)
        ld      c,a
        xor     a
        ld      (bl_k),a
bl_cell:
        push    bc              ; the cell: the balloon's
        ld      l,b
        ld      h,0
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ld      a,c
        call    add_hl_a
        call    bl_cap          ; an end: a rounded cap over the field
        jr      z,bl_body
        pop     bc
        jr      bl_next
bl_body:
        ld      de,_fld_dirty
        add     hl,de
        set     1,(hl)
        ld      a,(bl_k)        ; a character: columns 1 .. len, while any are left
        ld      e,' '
        or      a
        jr      z,bl_ch
        ld      b,a
        ld      a,(bl_len)
        cp      b
        jr      c,bl_ch
        ld      a,(bl_i)
        ld      b,a
        ld      a,(bl_n_)
        cp      b
        jr      z,bl_ch
        jr      c,bl_ch
        ld      hl,bl_buf
        ld      a,b
        call    add_hl_a
        ld      e,(hl)
        ld      hl,bl_i
        inc     (hl)
bl_ch:  pop     bc
        ld      a,e
        call    put_glyph
bl_next:
        inc     c
        ld      hl,bl_k
        inc     (hl)
        ld      a,(b_w)
        cp      (hl)
        jr      nz,bl_cell
        call    tc_flush
        pop     bc
        inc     b
        ld      hl,bl_rows
        dec     (hl)
        jr      nz,bl_row
        call    tc_unmap
        ; the tail: under the bottom row, at the talker's column (inside the ends)
        ld      a,(b_r0)
        ld      hl,b_h
        add     a,(hl)
        cp      FROWS
        ret     nc
        ld      (b_tr),a
        ld      a,(bl_s)
        ld      c,a
        call    tk_at
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ex      de,hl
        srl     h
        rr      l
        srl     h
        rr      l
        srl     h
        rr      l
        ld      a,(b_c0)        ; at least c0 + 1, at most c0 + w - 2
        inc     a
        cp      l
        jr      nc,bt_1
        ld      a,(b_c0)
        ld      hl,b_w
        add     a,(hl)
        sub     2
        cp      l
        jr      c,bt_1
        ld      a,l
bt_1:   ld      (b_tc),a
        ld      l,a
        ld      a,(b_tr)
        call    cell_at
        ld      a,SH_TAIL
        jp      shape_set

; bl_cap: Z for a middle cell; else cell HL (its index) an end: its shape
; set (single row: left 1, right 2; two: top 3/5, bottom 4/6) and NZ
; (BC kept: B the row)
bl_cap: ld      a,(bl_k)
        or      a
        ld      d,SH_LEFT
        jr      z,bc_end
        ld      e,a
        ld      a,(b_w)
        dec     a
        cp      e
        ld      d,SH_RIGHT
        jr      z,bc_end
        xor     a               ; (Z)
        ret
bc_end: ld      a,(b_h)
        dec     a
        jr      z,bc_one
        ld      a,(b_r0)        ; two rows: top (+2) or bottom (+3)
        cp      b
        ld      a,2
        jr      z,bc_2
        inc     a
bc_2:   add     a,d
        ld      d,a
bc_one: ld      a,d
        push    bc
        call    shape_set
        pop     bc
        or      1               ; (NZ)
        ret

; cell_at: HL = cell (row A, column L)
cell_at:
        ld      h,0
        push    hl
        ld      l,a
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        pop     de
        add     hl,de
        ret

; shape_set: cell HL's flag gets shape A (bits 4-6) and it's dirty
shape_set:
        rlca
        rlca
        rlca
        rlca
        ld      c,a
        push    hl
        ld      de,_fld_dirty
        add     hl,de
        ld      a,(hl)
        and     $8F
        or      c
        ld      (hl),a
        pop     hl
        jp      cell_dirty

; shape_paint: shape A (1-7) over the tile in tc_tile, in white (palette 0's
; 6: 0110b) (render.asm, for a cell whose flag has one)
shape_paint:
        dec     a
        add     a,a
        add     a,a
        add     a,a
        ld      hl,shapes
        call    add_hl_a
        ld      de,_tc_tile
        ld      b,8
sp_r:   ld      c,(hl)          ; the shape's row
        inc     hl
        push    bc
        ld      a,c
        cpl
        ld      b,a             ; kept
        ld      a,(de)          ; plane 0: off
        and     b
        ld      (de),a
        inc     de
        ld      a,(de)          ; planes 1, 2: on
        or      c
        ld      (de),a
        inc     de
        ld      a,(de)
        or      c
        ld      (de),a
        inc     de
        ld      a,(de)          ; plane 3: off
        and     b
        ld      (de),a
        inc     de
        pop     bc
        djnz    sp_r
        ret

; the shapes: a row each, top down (bit 7 the left)
shapes: defb    $07, $1F, $3F, $7F, $7F, $3F, $1F, $07      ; 1 left end
        defb    $E0, $F8, $FC, $FE, $FE, $FC, $F8, $E0      ; 2 right end
        defb    $07, $1F, $3F, $7F, $7F, $FF, $FF, $FF      ; 3 left top
        defb    $FF, $FF, $FF, $7F, $7F, $3F, $1F, $07      ; 4 left bottom
        defb    $E0, $F8, $FC, $FE, $FE, $FF, $FF, $FF      ; 5 right top
        defb    $FF, $FF, $FF, $FE, $FE, $FC, $F8, $E0      ; 6 right bottom
        defb    $7E, $3C, $3C, $18, $18, $10, $00, $00      ; 7 the tail
        defc    SH_LEFT = 1
        defc    SH_RIGHT = 2
        defc    SH_TAIL = 7

        SECTION rodata_user
_seat_ix:                       ; (a seat's colour: palette 1's 1-10)
        defb    1, 2, 3, 4, 5, 6, 7, 8, 9, 10

        SECTION bss_user
cram0:          defs    16      ; palette 0: the round's
cram_dirty:     defs    1
fire_on:        defs    1
fire_ph:        defs    1
flash_on:       defs    1
_flash_n:       defs    1
_shake_n:       defs    1
vf_t:           defs    1
sy_sky:         defs    1
sy_dirt:        defs    1
sy_n:           defs    1
sy_last:        defs    1
td:             defs    SEATS * TD_SIZE
ft_s:           defs    1
ft_new:         defs    TD_SIZE
tile_dirty:     defs    SEATS
tanks_dirty:    defs    1
tt_m:           defs    24      ; a tank's masks: S, W, K a row
tank_t:         defs    32
tt_me:          defs    1
tt_ang:         defs    1
tt_px0:         defs    1
tt_si:          defs    1
mf_s:           defs    1
mf_w:           defs    1
mf_k:           defs    1
lines:          defs    FIELD_H + 8
al_s:           defs    1
al_y:           defs    1
al_td:          defs    2
al_l:           defs    2
sat_n:          defs    1       ; the IRQ's sprites (a copy of w_: allocate)
sat_y:          defs    NSPR
sat_x:          defs    NSPR * 2
w_n:            defs    1
w_y:            defs    NSPR
w_x:            defs    NSPR * 2
sp_was:         defs    1
sat_dx:         defs    1
sat_dy:         defs    1
shake_prep:     defs    1
sat_dirty:      defs    1
nm_c:           defs    3
nm_n:           defs    3
nm_s:           defs    3
ht_hi:          defs    1
ht_row:         defs    1
ht_col:         defs    1
ht_buf:         defs    33
hud_ch:         defs    96      ; the HUD's characters (the gauges read their numbers)
ht_sl:          defs    1
g_e:            defs    1
g_p:            defs    1
g_wlo:          defs    1
g_whi:          defs    1
g_old:          defs    4
row_st:         defs    12      ; each HUD row's way of drawing, as last drawn
st_now:         defs    4
ht_same:        defs    1
g_chg:          defs    1
b_on:           defs    1
b_r0:           defs    1
b_c0:           defs    1
b_w:            defs    1
b_h:            defs    1
b_tr:           defs    1       ; the tail's row (FROWS and up: none), its column
b_tc:           defs    1
bl_s:           defs    1
bl_n_:          defs    1
bl_len:         defs    1
bl_y:           defs    1
bl_i:           defs    1
bl_k:           defs    1
bl_rows:        defs    1
bl_buf:         defs    MAX_TALK
shk_row:        defs    64
ovl_n:          defs    1
ovl:            defs    OV_SIZE * 2
ov_k:           defs    1
oc_x0:          defs    1
oc_y0:          defs    1
oc_set:         defs    1
op_n:           defs    1
op_k:           defs    1
op_cx:          defs    1
op_cy:          defs    1
op_tx:          defs    2
op_ty:          defs    1
op_m:           defs    2
op_c:           defs    1
op_b:           defs    1
