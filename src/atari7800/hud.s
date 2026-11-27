;
; hud.s -- the field screen's HUD rows' characters (src/atari7800/fieldm.c
; builds their display lists): field_status and field_status_hi, which a
; script calls often (every hit, every energy), in assembly.
;
; void __fastcall__ field_status(uint8_t row, uint8_t col, const char *s)
; void __fastcall__ field_status_hi(uint8_t row, uint8_t col, const char *s)
;   s at row's col on: ASCII to the font's codes (the Atari's screen codes,
;   lower case as upper, the rest blank); _hi in inverse (+64)
;
        .export _field_status, _field_status_hi, _m_hud
        .import popa
        .importzp ptr1, ptr2, tmp1, tmp2, tmp3

        .include "a78.inc"

        .bss
_m_hud: .res NHUD * 40

        .rodata
hudlo:  .byte   <(_m_hud), <(_m_hud + 40), <(_m_hud + 80)
hudhi:  .byte   >(_m_hud), >(_m_hud + 40), >(_m_hud + 80)

        .code

_field_status_hi:
        ldy     #$40
        bne     go
_field_status:
        ldy     #0
go:     sty     tmp3            ; (inverse)
        sta     ptr1            ; s
        stx     ptr1+1
        jsr     popa
        sta     tmp1            ; col
        jsr     popa
        tax                     ; row
        lda     hudlo,x
        sta     ptr2
        lda     hudhi,x
        sta     ptr2+1
        ldy     #0
@l:     lda     (ptr1),y
        beq     @x
        sty     tmp2
        and     #$7F
        cmp     #'a'
        bcc     @up
        cmp     #'z' + 1
        bcs     @up
        and     #$DF            ; (upper case)
@up:    sec
        sbc     #32
        cmp     #64
        bcc     @ok             ; (32-95: a glyph; under 32 wrapped round)
        lda     #0
@ok:    ora     tmp3
        ldy     tmp1
        cpy     #40
        bcs     @x
        sta     (ptr2),y
        inc     tmp1
        ldy     tmp2
        iny
        bne     @l
@x:     rts

; void __fastcall__ txt_scroll(uint8_t row, uint8_t n, uint8_t up): text
; rows row..row+n-1 move a row (up: each takes the one under it; else the
; one over it), all of them just after the frame's DLI, so they are moved
; ahead of MARIA's raster (the blank under the screen and over row 0 is ~90
; lines; the copy ~5 lines a row); the row left over is blank: no frame
; shows a row twice (src/atari7800/scr.c's scr_scroll)
        .export _txt_scroll
        .import _m_wait

        .bss
tsup:   .res 1
tsrow:  .res 1
tsend:  .res 1

        .rodata
; text row r: page TXTPG + r / 3, byte TXTLO + 40 (r % 3)
txlo:   .repeat NTROWS, I
        .byte   TXTLO + 40 * (I .mod 3)
        .endrep
txhi:   .repeat NTROWS, I
        .byte   TXTPG + I / 3
        .endrep

        .code
_txt_scroll:
        sta     tsup
        jsr     popa
        sta     tsend           ; n
        jsr     popa
        sta     tsrow
        clc
        adc     tsend
        sta     tsend           ; row + n
        jsr     _m_wait
        lda     tsup
        beq     @down
        ldx     tsrow           ; row i gets row i + 1, top first
@u:     inx
        cpx     tsend
        bcs     @clr            ; (then the last, blank)
        jsr     move            ; (x to x - 1)
        jmp     @u
@clr:   dex
        jmp     blank
@down:  ldx     tsend           ; row i gets row i - 1, bottom first
        dex
@d:     cpx     tsrow
        beq     @c0
        bcc     @x
        dex
        txa
        pha
        inx
        lda     txlo,x          ; (to x, from x - 1)
        sta     ptr2
        lda     txhi,x
        sta     ptr2+1
        dex
        lda     txlo,x
        sta     ptr1
        lda     txhi,x
        sta     ptr1+1
        jsr     cp40
        pla
        tax
        jmp     @d
@c0:    jmp     blank           ; (the first, blank)
@x:     rts

; row x blank
blank:  lda     txlo,x
        sta     ptr2
        lda     txhi,x
        sta     ptr2+1
        lda     #0
        ldy     #39
@b:     sta     (ptr2),y
        dey
        bpl     @b
        rts

; row x to row x - 1
move:   lda     txlo,x
        sta     ptr1
        lda     txhi,x
        sta     ptr1+1
        dex
        lda     txlo,x
        sta     ptr2
        lda     txhi,x
        sta     ptr2+1
        inx
cp40:   ldy     #39
@c:     lda     (ptr1),y
        sta     (ptr2),y
        dey
        bpl     @c
        rts
