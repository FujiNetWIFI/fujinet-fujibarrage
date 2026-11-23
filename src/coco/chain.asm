*
* chain.asm -- the menu and the game are two programs on the disk (as on
* the Atari and the Apple II): chain(name) loads the other one's low core
* (NAME.BIN, a DECB file: blocks of 00 len addr data, then FF 0000 exec)
* with DSKCON and runs it; its bank_init loads the rest. It runs from the
* caller's high core ($8000 up), which the low core's load doesn't touch,
* with its own stack there; the new program's bank_init loads its own high
* core over it only once it has jumped. The loader is chainld.asm (shared
* with launch.asm). Interrupts stay masked: the new program sets its own
* up. A sector read that fails is tried again, 8 times (MAME's DSKCON
* fails now and then: bank.c's load_try). chain doesn't come back: a file
* that isn't there or can't be read leaves a message (the caller's memory
* may be gone by then) and stops.
*
        section hicode

        export  _chain

* uint8_t chain(const char *name)  (8 characters, space padded)
_chain
        orcc    #$50
        clra                    ; (DSKCON's parameters: BASIC's direct page)
        tfr     a,dp
        ldx     2,s
        ldu     #cname
        ldb     #8
@cp     lda     ,x+
        sta     ,u+
        decb
        bne     @cp
        lds     #chstk
        include "chainld.asm"

* start: the new program, the machine as BASIC's LOADM leaves it (CMOC's
* start-up calls into BASIC's ROM): no interrupt source on (the vector at
* $010C is still the old program's); a CoCo 3's slot 7 its own block
* again; a CoCo 1/2 back in ROM mode -- from low RAM, as this code (the
* old high core) goes under the ROM: sta $FFDE / jmp exec at $0EF8 (the
* direct page's top, past fixed.asm's bytes)
start   lds     #$8000          ; the stack where a LOADM'd program has it (this
                                ;  one's goes under the ROM)
        lda     $FF01
        anda    #$FE            ; PIA0 CA1 (hsync) off
        sta     $FF01
        lda     $FF03
        anda    #$FE            ; PIA0 CB1 (field sync) off
        sta     $FF03
        lda     $FF02           ; (any flag cleared)
        lda     $FF00
        ldx     hdr+3
        lda     $FFF8
        cmpa    #$FE
        bne     @c12
        clr     $FF92           ; the GIME's sources off
        clr     $FF93
        lda     #$3F            ; slot 7: BASIC's block
        sta     $FFA7
        jmp     ,x
@c12    ldd     #$B7FF          ; sta $FFDE
        std     $0EF8
        ldd     #$DE7E          ;  jmp exec
        std     $0EFA
        stx     $0EFC
        jmp     $0EF8
