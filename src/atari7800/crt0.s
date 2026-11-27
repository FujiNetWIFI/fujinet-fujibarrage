;
; crt0.s -- the start of FujiBarrage on the 7800 (FujiBlast's, from
; fujinet-lib's bus/atari7800/crt0.s): the claim names SuperGame with the
; cart's RAM (kind 5 = A78MAP_SG_RAM + 1: the cartridge banks the image
; itself, 16K of RAM at $4000-$7FFF), and the NMI goes to the display's
; DLIs (maria.s).
;
; Nothing here stores to $00-$1F: INPTCTRL stays unlocked, as the cart
; wants it. The loader starts us with DMA off and interrupts off (and S at
; $16: ours is set). All of the RAM is cleared: zero page, the console's
; (but for zero page's and the stack's other faces, $2040-$20FF and
; $2140-$21FF) and the cart's 16K.
;
        .export         __STARTUP__ : absolute = 1
        .export         _exit
        .import         __CSTACK_START__, __CSTACK_SIZE__
        .import         initlib, donelib, zerobss, copydata
        .import         push0, _main, _nmi, _irq, _bank_set

        .include        "zeropage.inc"
        .include        "a78.inc"

        .segment "STARTUP"
start:  sei
        cld
        lda     #CT_OFF         ; MARIA's DMA off until the display is built
        sta     CTRL
        lda     #0
        sta     OFFSET
        ldx     #$FF
        txs
        lda     #0
        ldx     #$40
@zp:    sta     $00,x
        inx
        bne     @zp
        ldx     #$3F
@c2:    sta     $2000,x
        sta     $2100,x
        dex
        bpl     @c2
        lda     #$18            ; $1800-$1FFF
        ldx     #8
        jsr     clear
        lda     #$22            ; $2200-$27FF
        ldx     #6
        jsr     clear
        lda     #$40            ; the cart's RAM, $4000-$7FFF
        ldx     #$40
        jsr     clear
        lda     #<(__CSTACK_START__ + __CSTACK_SIZE__)
        sta     c_sp
        lda     #>(__CSTACK_START__ + __CSTACK_SIZE__)
        sta     c_sp+1
        jsr     zerobss
        jsr     copydata
        lda     #0              ; bank 0 in the window (the console may start
        jsr     _bank_set       ; with any)
        jsr     initlib
        jsr     push0           ; argc
        jsr     push0           ; argv
        ldy     #4
        jsr     _main
_exit:  jsr     donelib
@hang:  jmp     @hang

; X pages from page A, cleared
clear:  sta     ptr1+1
        lda     #0
        sta     ptr1
        tay
@p:     sta     (ptr1),y
        iny
        bne     @p
        inc     ptr1+1
        dex
        bne     @p
        rts

        .segment "CLAIM"
        .byte   "FUJI", 1, 5, 1, 0, 0, 0    ; version 1, SuperGame + RAM, cart RAM

        .segment "VECTORS"
        .byte   $FF, $87                    ; $FFF8/$FFF9: a 7800 cart
        .word   _nmi, start, _irq
