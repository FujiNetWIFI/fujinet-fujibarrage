;
; txt.s -- text on the 40x24 screen (src/apple2/scr.c): a string into a
; row, as the machine's characters. Upper case; $20-$5F, the rest a space;
; normal (| $80) or inverse (& $3F).
;
        .export _txt_put, _numstr
        .import popax
        .exportzp _txt_dst, _txt_max, _txt_inv

        .zeropage
_txt_dst: .res 2        ; where (a row's cell)
_txt_max: .res 1        ; cells at most
_txt_inv: .res 1        ; 0 normal, else inverse
src:      .res 2

        .segment "LOWCODE"       ; (below the field: barrage.cfg)

; void __fastcall__ txt_put(const char *s)
_txt_put:
        sta     src
        stx     src+1
        ldy     #0
        lda     _txt_max
        beq     @x
@l:     lda     (src),y
        beq     @x
        cmp     #'a'
        bcc     :+
        cmp     #'z'+1
        bcs     :+
        and     #$DF            ; upper case
:       cmp     #$20
        bcc     @sp
        cmp     #$60
        bcs     @sp
        ldx     _txt_inv
        bne     @in
        ora     #$80
        bne     @st
@in:    and     #$3F
        bpl     @st
@sp:    lda     #$A0
        ldx     _txt_inv
        beq     @st
        lda     #$20
@st:    sta     (_txt_dst),y
        iny
        cpy     _txt_max
        bcc     @l
@x:     rts

; void __fastcall__ numstr(char *out, uint16_t v): v in decimal (cat.c's,
; by subtraction: the HUD's numbers come every aim step)
_numstr:
        sta     val
        stx     val+1
        jsr     popax
        sta     src             ; (out)
        stx     src+1
        ldy     #0
        lda     #0
        sta     any
        ldx     #3              ; 10000, 1000, 100, 10
@d:     lda     #'0'
        sta     dig
@s:     lda     val             ; val >= tens[x]?
        cmp     tenlo,x
        lda     val+1
        sbc     tenhi,x
        bcc     @o
        sta     val+1
        lda     val
        sbc     tenlo,x         ; (carry set)
        sta     val
        inc     dig
        bne     @s
@o:     lda     dig
        cmp     #'0'
        bne     @p
        bit     any
        bpl     @n
@p:     sta     (src),y
        iny
        lda     #$80
        sta     any
@n:     dex
        bpl     @d
        lda     val
        ora     #'0'
        sta     (src),y
        iny
        lda     #0
        sta     (src),y
        rts

        .rodata
tenlo:  .byte   <10, <100, <1000, <10000
tenhi:  .byte   >10, >100, >1000, >10000

        .bss
val:    .res 2
dig:    .res 1
any:    .res 1
