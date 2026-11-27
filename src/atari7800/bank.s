;
; bank.s -- the 7800's SuperGame window ($8000-$BFFF): a store anywhere in
; it picks the bank; this is the only code that stores there
; (tools/atari7800/mkrom.py checks). Its $8000-$A3FF is REPL, the same in
; every bank (the font, more of the core); $A400-$BFFF is the bank's own:
; an overlay, reached through the wrappers gen/atari7800/far.s
; (tools/atari7800/far.txt, tools/nes/mkfar.py) that call far_call.
;
; far_call: a wrapper's (`jsr far_call`, then the bank and the function's
; address): the caller's bank kept on the stack, the overlay's put in, the
; function called with the fastcall argument (A, X), the caller's bank
; back, the result (A, X) returned to the wrapper's caller. The frame's
; DLI puts bank 4 (the sound's data) in while it plays, then bank_cur: so
; bank_cur is always set before the store that switches.
;
        .export _bank_set, far_call
        .exportzp _bank_cur

        .include "a78.inc"

        .segment "ZEROPAGE"
far_a:  .res 1          ; a far call's fastcall argument, then its result
far_x:  .res 1
far_p:  .res 2          ; the wrapper's bytes
far_t:  .res 2          ; the function
_bank_cur: .res 1       ; the bank in the window

        .code

; void __fastcall__ bank_set(uint8_t n)
_bank_set:
        sta     _bank_cur
        sta     BANKSEL
        rts

far_call:
        sta     far_a
        stx     far_x
        pla                     ; the wrapper's bytes (the address less 1)
        sta     far_p
        pla
        sta     far_p+1
        ldy     #2
        lda     (far_p),y
        sta     far_t
        iny
        lda     (far_p),y
        sta     far_t+1
        lda     _bank_cur       ; the caller's bank
        pha
        ldy     #1
        lda     (far_p),y
        sta     _bank_cur
        sta     BANKSEL
        lda     far_a
        ldx     far_x
        jsr     @call
        sta     far_a
        stx     far_x
        pla
        sta     _bank_cur
        sta     BANKSEL
        lda     far_a
        ldx     far_x
        rts                     ; (to the wrapper's caller)
@call:  jmp     (far_t)
