;
; maria.s -- the 7800's display: three display-list lists (DLLs), one a
; screen, and the DLIs that colour them (a78.inc has the memory map).
;
;   blank  nothing but the frame's DLI (a screen being built)
;   text   24 rows of 320A text, 8 lines each (src/atari7800/scr.c)
;   field  3 HUD rows of 320A text, a 2-line gap, then the field: 21 zones
;          of 8 lines of 160A, the Atari's mode E bitmap (src/atari/fld.s)
;
; Every row and zone has two 64-byte display-list slots (console RAM): the
; C code writes the one not shown (m_back, m_busy set meanwhile), m_flip
; says so, and the frame's DLI points both DLLs' entries at it then. A text
; row's list is its runs of characters (5-byte indirect headers: each run an
; ink, its palette's C2); a field zone's is the tanks' underlays (2 bytes of
; a strip: a box in the seat's colour, behind the field, showing through
; the tanks' 00 pixels as the Atari's players do) and the zone's two halves
; of the bitmap (20 bytes each).
;
; The DLIs, each its own handler (dli_vec): the DLI on a DLL entry comes as
; MARIA ends the DMA of the zone above, a line before that zone's last
; line shows:
;
;   row    a text or HUD row's colours: WSYNC (to the row above's last
;          line, which the font leaves blank), then the background and its
;          inks (P0C2 text, P1C2 labels, P2C2 a name)
;   gap    the same wait, then 160A, the field's background, zone 0's sky
;          and dirt
;   zone   zone z's sky and dirt into P(z & 1) (z - 1's are the other pair:
;          they can come early): one handler a zone, unrolled
;   frame  on the blank under the screen: the next frame's DLL and first
;          handler, 320A and the font for its top rows, the flips, the
;          fire's colour, the flash and the shake, the seats' colours
;          (P3-P7), the clock, the sound (its data bank 4's: the DLI
;          puts it in the window and the main thread's back)
;
; The clock is frame_count in jiffies of 1/60 s however fast the console
; runs (FujiBlast's): NTSC (59.92 Hz) counts one more every 1421 frames,
; PAL (49.86) six for five frames and one more every 523.
;
        .export _ovl_m_init, _m_show, _m_wait, _m_back, _m_flip
        .export _m_ntsc, _m_cur, _m_busy, _m_want, _m_put, _m_nd, _m_ndlen
        .export _r_bg, _r_ink, _r_lab, _r_nam, _z_sky, _z_dirt, _zb_sky, _zb_dirt
        .export _m_seat, _m_seatnew
        .export _dsp_flash, _dsp_shake, _fx_col, _fx_cycle, _hue
        .export _frame_count, _vbi_count, _nmi, _irq
        .export dll_b, dll_t, dll_f         ; (tools/a78sim's dmacheck)
        .import _snd_vbi
        .importzp _bank_cur
        .import __DLSLOTS_RUN__, __DLSLOTS2_RUN__

        .include "a78.inc"

M_BLANK = 0
M_TEXT  = 1
M_FIELD = 2
SNDBANK = 4

        .segment "ZEROPAGE"
mp:     .res 2          ; a pointer (m_init; the flips)
dli_vec: .res 2         ; the next DLI's handler (_nmi: JMP (dli_vec))
pp:     .res 2          ; m_put's (mp is the frame DLI's)

        .segment "DLSLOTS"
        .res 33 * SLOTSZ
        .segment "DLSLOTS2"
        .res (NSLOTZ * 2 - 33) * SLOTSZ

        .bss                    ; (the cart's RAM: MARIA reads the DLLs there)
_m_nd:  .res SLOTSZ     ; m_put: a zone's new list,
_m_ndlen: .res 1        ; its length
dll_b:  .res 3 * 24
dll_t:  .res 3 * 36
dll_f:  .res 3 * 36
dll_toff: .res NSLOTZ   ; zone z's entry: dll_t + dll_toff[z] ($FF: none)
dll_foff: .res NSLOTZ

        .segment "CONSBSS"      ; (console RAM: what the DLIs read)
_r_bg:  .res NTROWS + NHUD
_r_ink: .res NTROWS + NHUD
_r_lab: .res NTROWS + NHUD
_r_nam: .res NTROWS + NHUD
_z_sky: .res NZONES     ; a zone's sky and dirt as shown (white in a flash)
_z_dirt: .res NZONES
_zb_sky: .res NZONES    ; its own
_zb_dirt: .res NZONES
_m_seat: .res 15
_m_seatnew: .res 1      ; 1: m_seat changed
zflip:  .res NSLOTZ     ; the back slot is ready: shown from the next frame
zfront: .res NSLOTZ     ; the slot shown (0, 1)
zpend:  .res 1          ; 1: some zflip is set
dli_row: .res 1         ; the next row's (text 0-23, HUD 24-26)
dli_end: .res 1
dli_aft: .res 2         ; the handler after the rows
fbg:    .res 1          ; the field's background
flashing: .res 1        ; 1: z_sky, z_dirt are the flash's white
_m_ntsc: .res 1
_m_cur: .res 1
_m_want: .res 1
_m_busy: .res 1
_dsp_flash: .res 1      ; frames of a white field left
_dsp_shake: .res 1      ; frames of a shaking screen left
_fx_col: .res 1         ; the fire's colour (C3 of P0, P1)
_fx_cycle: .res 1       ; 1: it cycles through the fire's colours
fire_t: .res 1
_frame_count: .res 2
_vbi_count: .res 1
c5:     .res 1
cfix:   .res 2
ntop:   .res 1          ; the DLLs' blank entries at the top
fire:   .res 8

        .rodata
null_dl: .byte  0, 0
fire0:  .byte   $36, $38, $3A, $1C, $1E, $1C, $3A, $38
.macro  slottab part
        .repeat NSLOTZ * 2, i
        .if i < 33
        .byte   part (__DLSLOTS_RUN__ + i * SLOTSZ)
        .else
        .byte   part (__DLSLOTS2_RUN__ + (i - 33) * SLOTSZ)
        .endif
        .endrepeat
.endmacro
slot_lo: slottab <
slot_hi: slottab >
dll_lo: .byte   <dll_b, <dll_t, <dll_f
dll_hi: .byte   >dll_b, >dll_t, >dll_f
; the strips' value pages (the rest of their pages is the crt0's 0):
; L at $F0-$F5 (values 1, 2, 3 a 2-byte pair), pages $40-$55, 8 pages of
; value from $47; F5 at $F6-$FB, pages $40-$48, 5 from $42; F6 at $F6-$FB,
; pages $49-$50, 6 from $4A
strv:   .byte   $55, $55, $AA, $AA, $FF, $FF

        .code

; ---- start-up -----------------------------------------------------------------

; void m_init(void): NTSC or PAL, the DLLs, the strips; the blank screen
; and its frame DLI on. Once, at start-up: the boot code's bank (3), its
; wrapper gen/atari7800/far.s's (tools/atari7800/far.txt)
        .segment "BOOTCODE"
_ovl_m_init:
        jsr     tvdetect
        ldy     #7
@f:     lda     fire0,y
        jsr     _hue
        sta     fire,y
        dey
        bpl     @f
        lda     #$0E
        sta     _fx_col
        jsr     strips
        ; the blank DLL: the top, 12 blanks of 16 lines, the frame's
        lda     #<dll_b
        sta     mp
        lda     #>dll_b
        sta     mp+1
        jsr     top
        ldx     #12
@b:     lda     #$0F
        jsr     null
        dex
        bne     @b
        jsr     bottom
        ; the text DLL: 24 rows, each its DLI
        lda     #<dll_t
        sta     mp
        lda     #>dll_t
        sta     mp+1
        jsr     top
        ldx     #0
@t:     lda     mp              ; (the entry's offset: mp - dll_t)
        sec
        sbc     #<dll_t
        sta     dll_toff,x
        lda     #$87
        jsr     slot
        inx
        cpx     #NTROWS
        bne     @t
        jsr     bottom
        ; the field DLL: 3 HUD rows, the gap, 21 zones
        lda     #<dll_f
        sta     mp
        lda     #>dll_f
        sta     mp+1
        jsr     top
        ldx     #0
@h:     lda     mp
        sec
        sbc     #<dll_f
        sta     dll_foff,x
        lda     #$87
        jsr     slot
        inx
        cpx     #NHUD
        bne     @h
        lda     #$81            ; the gap: 2 lines, its DLI the switch
        jsr     null
@z:     lda     mp
        sec
        sbc     #<dll_f
        sta     dll_foff,x
        lda     #$87            ; (zone 0's is the gap's: no DLI)
        cpx     #NHUD
        bne     @z1
        lda     #$07
@z1:    jsr     slot
        inx
        cpx     #NHUD + NZONES
        bne     @z
        jsr     bottom
        ; the blank screen, the frame's DLI, the clock; MARIA on
        lda     #<frame
        sta     dli_vec
        lda     #>frame
        sta     dli_vec+1
        lda     #M_BLANK
        sta     _m_cur
        sta     _m_want
        lda     #<dll_b
        sta     DPPL
        lda     #>dll_b
        sta     DPPH
        lda     #0
        sta     _frame_count
        sta     _frame_count+1
        sta     c5
        jsr     fixreset
@vb:    bit     MSTAT           ; in a blank: DMA on
        bpl     @vb
        lda     #CT_TEXT
        sta     CTRL
        rts

; NTSC or PAL: the lines from one blank to the next (FujiBlast's)
tvdetect:
        bit     MSTAT
        bpl     tvdetect
@e:     bit     MSTAT
        bmi     @e
        lda     #0
        sta     mp
        sta     mp+1
@l:     sta     WSYNC
        inc     mp
        bne     @n
        inc     mp+1
@n:     bit     MSTAT
        bpl     @l
        lda     mp+1            ; fewer than 267: NTSC
        cmp     #>267
        bcc     @ntsc
        bne     @pal
        lda     mp
        cmp     #<267
        bcc     @ntsc
@pal:   lda     #0
        sta     _m_ntsc
        rts
@ntsc:  lda     #1
        sta     _m_ntsc
        rts

; a DLL entry at mp: flags A, the null list
null:   pha
        ldy     #0
        sta     (mp),y
        iny
        lda     #>null_dl
        sta     (mp),y
        iny
        lda     #<null_dl
        sta     (mp),y
        pla
adv3:   lda     mp
        clc
        adc     #3
        sta     mp
        bcc     @x
        inc     mp+1
@x:     rts

; a DLL entry at mp: flags A, zone X's slot (both slots are empty at first:
; the crt0 cleared them; the front is slot 0)
slot:   ldy     #0
        sta     (mp),y
        txa
        asl     a
        tay
        lda     slot_hi,y
        pha
        lda     slot_lo,y
        ldy     #2
        sta     (mp),y
        pla
        dey
        sta     (mp),y
        jmp     adv3

; the top's blank entries: NTSC 25 lines (16 + 9), PAL 50 (3 x 16 + 2);
; the first is the shake's (16 lines, 12 while it shakes)
top:    lda     _m_ntsc
        beq     @pal
        lda     #$0F
        jsr     null
        lda     #$08
        jmp     null
@pal:   lda     #$0F
        jsr     null
        lda     #$0F
        jsr     null
        lda     #$0F
        jsr     null
        lda     #$01
        jmp     null

; under the screen: a 4-line blank (the frame's DLI comes as its DMA ends,
; lines before it shows: what it writes mustn't reach the screen's last
; line), the frame's DLI (on a 1-line blank), then blanks to the frame's end
bottom: lda     #$03
        jsr     null
        lda     #$80
        jsr     null
        txa
        pha
        ldx     #4
@b:     lda     #$0F
        jsr     null
        dex
        bne     @b
        pla
        tax
        rts

; the underlays' strips (the page ranges above)
strips: ldx     #$47
@l:     stx     mp+1
        lda     #$F0
        sta     mp
        ldy     #5
@l1:    lda     strv,y
        sta     (mp),y
        dey
        bpl     @l1
        inx
        cpx     #$4F
        bne     @l
        ldx     #$42
@f5:    jsr     fpage
        inx
        cpx     #$47
        bne     @f5
        ldx     #$4A
@f6:    jsr     fpage
        inx
        cpx     #$50
        bne     @f6
        rts
fpage:  stx     mp+1
        lda     #$F6
        sta     mp
        ldy     #5
@l:     lda     strv,y
        sta     (mp),y
        dey
        bpl     @l
        rts

        .code

fixreset:
        lda     _m_ntsc
        beq     @pal
        lda     #<1421
        sta     cfix
        lda     #>1421
        sta     cfix+1
        rts
@pal:   lda     #<523
        sta     cfix
        lda     #>523
        sta     cfix+1
        rts

; ---- for C --------------------------------------------------------------------

; void __fastcall__ m_show(uint8_t kind): from the next frame (waits for it)
_m_show:
        sta     _m_want
@w:     cmp     _m_cur
        bne     @w
        rts

; void m_wait(void): a whole frame (the next frame DLI)
_m_wait:
        lda     _vbi_count
@w:     cmp     _vbi_count
        beq     @w
        rts

; uint8_t *__fastcall__ m_back(uint8_t z): zone z's slot not shown
_m_back:
        tay
        asl     a
        ora     zfront,y
        eor     #1
        tay
        lda     slot_lo,y
        ldx     slot_hi,y
        rts

; void __fastcall__ m_put(uint8_t z): m_nd (m_ndlen bytes) as zone z's
; list from the next frame -- unless it is the list shown and no other is
; waiting (a scrolled text row's: nothing to do, nothing to flip)
_m_put: tay
        lda     zflip,y
        bne     @wr
        tya
        asl     a
        ora     zfront,y
        tax
        lda     slot_lo,x
        sta     pp
        lda     slot_hi,x
        sta     pp+1
        tya
        pha
        ldy     #0
@c:     lda     _m_nd,y
        cmp     (pp),y
        bne     @diff
        iny
        cpy     _m_ndlen
        bne     @c
        pla
        rts
@diff:  pla
        tay
@wr:    lda     #1
        sta     _m_busy
        tya
        pha
        asl     a
        ora     zfront,y
        eor     #1
        tax
        lda     slot_lo,x
        sta     pp
        lda     slot_hi,x
        sta     pp+1
        ldy     _m_ndlen
@cp:    dey
        lda     _m_nd,y
        sta     (pp),y
        tya
        bne     @cp
        pla
        tay
        lda     #1
        sta     zflip,y
        sta     zpend
        lda     #0
        sta     _m_busy
        rts

; void __fastcall__ m_flip(uint8_t z)
_m_flip:
        tay
        lda     #1
        sta     zflip,y
        sta     zpend
        rts

; uint8_t __fastcall__ hue(uint8_t c): as this console shows it (PAL: the
; hue one on; greys and $F0 up as they are: FujiBlast's)
_hue:   ldx     _m_ntsc
        bne     @x
        cmp     #$10
        bcc     @x
        cmp     #$F0
        bcs     @x
        adc     #$10            ; (carry clear)
@x:     ldx     #0
        rts

; ---- the DLIs -----------------------------------------------------------------

_nmi:   jmp     (dli_vec)
_irq:   rti

; a text or HUD row's colours
rowdli: pha
        txa
        pha
        ldx     dli_row
        lda     _r_bg,x
        sta     WSYNC           ; (MAME: all of it on the row above's last
        sta     BACKGRND        ; line, blank; a second WSYNC: the inks a
                                ; line late, on the row's first)
        lda     _r_ink,x
        sta     P0C1 + 1
        lda     _r_lab,x
        sta     P0C1 + 5
        lda     _r_nam,x
        sta     P0C1 + 9
        inx
        stx     dli_row
        cpx     dli_end
        bcc     @r
        lda     dli_aft
        sta     dli_vec
        lda     dli_aft+1
        sta     dli_vec+1
@r:     pla
        tax
        pla
        rti

; the HUD's end: the field's mode, background, zone 0's colours
gapdli: pha
        sta     WSYNC
        lda     #CT_FIELD
        sta     CTRL
        lda     fbg
        sta     BACKGRND
        lda     _z_sky
        sta     P0C1
        lda     _z_dirt
        sta     P0C1 + 1
        lda     #<zh1
        sta     dli_vec
        lda     #>zh1
        sta     dli_vec+1
        pla
        rti

; zone z's (1-20): its sky and dirt into P(z & 1), unrolled (no index)
.repeat NZONES - 1, I
.ident(.sprintf("zh%d", I + 1)):
        pha
        lda     _z_sky + I + 1
        sta     P0C1 + ((I + 1) & 1) * 4
        lda     _z_dirt + I + 1
        sta     P0C1 + 1 + ((I + 1) & 1) * 4
.if I + 2 < NZONES
        lda     #<.ident(.sprintf("zh%d", I + 2))
        sta     dli_vec
        lda     #>.ident(.sprintf("zh%d", I + 2))
        sta     dli_vec+1
.else
        lda     #<frame
        sta     dli_vec
        lda     #>frame
        sta     dli_vec+1
.endif
        pla
        rti
.endrepeat

; ---- the frame ----------------------------------------------------------------

frame:  pha
        txa
        pha
        tya
        pha
        lda     #CT_TEXT        ; the next frame's top rows: text (or blank)
        sta     CTRL
        lda     #FONTPG
        sta     CHARBASE
        lda     #0
        sta     BACKGRND
        ; the screen, its DLL and its first handler (before anything long:
        ; should this run on into the next frame's first DLI, it nests in
        ; the right one)
        ldx     _m_want
        stx     _m_cur
        lda     dll_lo,x
        sta     DPPL
        lda     dll_hi,x
        sta     DPPH
        lda     #<frame
        ldy     #>frame
        cpx     #M_BLANK
        beq     @v
        lda     #NTROWS
        sta     dli_end         ; (text: rows 0-23, then the frame's)
        lda     #0
        cpx     #M_TEXT
        beq     @rows
        lda     #<gapdli        ; (field: the HUD's 24-26, then the gap)
        sta     dli_aft
        lda     #>gapdli
        sta     dli_aft+1
        lda     #NTROWS + NHUD
        sta     dli_end
        lda     #NTROWS
        bne     @r2
@rows:  lda     #<frame
        sta     dli_aft
        lda     #>frame
        sta     dli_aft+1
        lda     #0
@r2:    sta     dli_row
        lda     #<rowdli
        ldy     #>rowdli
@v:     sta     dli_vec
        sty     dli_vec+1
        ; the flips
        lda     _m_busy
        bne     @nf
        lda     zpend
        beq     @nf
        lda     #0
        sta     zpend
        ldy     #NSLOTZ - 1
@f:     lda     zflip,y
        beq     @nx
        lda     #0
        sta     zflip,y
        lda     zfront,y
        eor     #1
        sta     zfront,y
        sta     mp
        tya
        asl     a
        ora     mp
        tax
        lda     slot_lo,x
        sta     mp
        lda     slot_hi,x
        sta     mp+1
        ldx     dll_toff,y
        cpx     #$FF
        beq     @nt
        sta     dll_t+1,x
        lda     mp
        sta     dll_t+2,x
        lda     mp+1
@nt:    ldx     dll_foff,y
        cpx     #$FF
        beq     @nx
        sta     dll_f+1,x
        lda     mp
        sta     dll_f+2,x
@nx:    dey
        bpl     @f
@nf:    ; the flash: the field white (its colours put back after)
        lda     _dsp_flash
        beq     @nofl
        dec     _dsp_flash
        lda     #$0E
        sta     fbg
        ldx     flashing
        bne     @fc
        inc     flashing
        ldx     #NZONES - 1
@w:     sta     _z_sky,x
        sta     _z_dirt,x
        dex
        bpl     @w
        bmi     @fc
@nofl:  sta     fbg             ; (A = 0)
        ldx     flashing
        beq     @fc
        sta     flashing
        ldx     #NZONES - 1
@r:     lda     _zb_sky,x
        sta     _z_sky,x
        lda     _zb_dirt,x
        sta     _z_dirt,x
        dex
        bpl     @r
@fc:    ; the fire's colour
        lda     _fx_cycle
        beq     @fx
        inc     fire_t
        lda     fire_t
        lsr     a
        and     #7
        tax
        lda     fire,x
        sta     _fx_col
@fx:    lda     _fx_col
        ldy     flashing
        beq     @f3
        lda     #$0E
@f3:    sta     P0C1 + 2
        sta     P0C1 + 6
        ; the shake: the screen 4 lines up, 2 frames in 4
        ldx     #$0F
        lda     _dsp_shake
        beq     @sk
        dec     _dsp_shake
        and     #2
        beq     @sk
        ldx     #$0B
@sk:    stx     dll_f
        ; the seats' colours: P3-P7, when they change
        lda     _m_seatnew
        beq     @ns
        lda     #0
        sta     _m_seatnew
        lda     _m_seat
        sta     $2D
        lda     _m_seat+1
        sta     $2E
        lda     _m_seat+2
        sta     $2F
        lda     _m_seat+3
        sta     $31
        lda     _m_seat+4
        sta     $32
        lda     _m_seat+5
        sta     $33
        lda     _m_seat+6
        sta     $35
        lda     _m_seat+7
        sta     $36
        lda     _m_seat+8
        sta     $37
        lda     _m_seat+9
        sta     $39
        lda     _m_seat+10
        sta     $3A
        lda     _m_seat+11
        sta     $3B
        lda     _m_seat+12
        sta     $3D
        lda     _m_seat+13
        sta     $3E
        lda     _m_seat+14
        sta     $3F
@ns:    inc     _vbi_count
        jsr     clock
        lda     #SNDBANK        ; the sound's data (bank 4), then the main
        sta     BANKSEL         ; thread's bank back (bank.s sets bank_cur
        jsr     _snd_vbi        ; before BANKSEL: this puts back the right one)
        lda     _bank_cur
        sta     BANKSEL
        pla
        tay
        pla
        tax
        pla
        rti

; frame_count: 60 a second
clock:  inc     _frame_count
        bne     @c1
        inc     _frame_count+1
@c1:    lda     _m_ntsc
        bne     @fix
        inc     c5              ; PAL: a sixth jiffy every five frames
        lda     c5
        cmp     #5
        bcc     @fix
        lda     #0
        sta     c5
        jsr     @one
@fix:   lda     cfix
        bne     @dl
        dec     cfix+1
@dl:    dec     cfix
        lda     cfix
        ora     cfix+1
        bne     @r
        jsr     fixreset
@one:   inc     _frame_count
        bne     @r
        inc     _frame_count+1
@r:     rts
