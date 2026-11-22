;
; chain.s -- the menu and the game are two ProDOS programs, as on the
; Atari (MENU.COM, BARRAGE.COM): each one, to run the other, loads that
; one's loader (MENU.SYSTEM, BARRAGE.SYSTEM: tools/a2loader) to $2000 and
; jumps to it, as ProDOS would have. $2000-$27FF is HGR page 1, which
; neither program's file holds (the loaders skip it), so loading it
; overwrites nothing that runs; the loader then loads its program over
; this one. The machine is left as ProDOS leaves it for a system program:
; the ROM in (not the language card: the MLI keeps whichever it finds),
; the 6502's stack empty.
;
        .export _xload, _chain
        .import _text_on

MLI     := $BF00
OPEN    = $C8
READ    = $CA
CLOSE   = $CC
LOADER  = $2000
IOBUF   = $2400

        .zeropage
ptr:    .res 2

        .code

; uint8_t xload(void): the menu, which doesn't come back (a ProDOS error
; if it can't be loaded)
_xload:
        lda     #<menu
        ldx     #>menu
        ; (on into chain)

; uint8_t __fastcall__ chain(const char *name): the program whose loader
; is the file name, in place of this one; back only with ProDOS's error
_chain:
        sta     ptr
        stx     ptr+1
        ldy     #0              ; the name, length first
:       lda     (ptr),y
        beq     :+
        sta     path+1,y
        iny
        cpy     #15
        bne     :-
:       sty     path
        jsr     _text_on        ; (the I/O buffer is in the page HGR shows)
        jsr     MLI
        .byte   OPEN
        .addr   p_open
        bne     fail
        lda     p_open+5
        sta     p_read+1
        sta     p_close+1
        jsr     MLI             ; the loader, all of it (ProDOS says how long)
        .byte   READ
        .addr   p_read
        pha
        jsr     MLI
        .byte   CLOSE
        .addr   p_close
        pla
        bne     fail
        bit     $C082           ; as ProDOS starts a system program: the ROM
        ldx     #$FF            ; in (cc65's start-up asks it what machine this
        txs                     ; is), the stack empty
        jmp     LOADER
fail:   ldx     #0
        rts

        .rodata
menu:   .byte   "MENU.SYSTEM", 0

        .data
path:   .res    16
p_open: .byte   3
        .addr   path, IOBUF
        .byte   0
p_read: .byte   4, 0
        .addr   LOADER, IOBUF - LOADER
        .word   0
p_close: .byte  1, 0
