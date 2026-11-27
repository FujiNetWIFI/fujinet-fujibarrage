;
; bank.s -- the NES game's far calls (gen/nes/far.s: tools/nes/mkfar.py):
; far_go puts PRG bank A in R6 ($8000), X kept (a fastcall argument's high
; byte). The bank is noted in r6_bank first, then the select register
; (mmc3_sel, which the interrupts put back after their own switches), then
; the bank: an interrupt between them leaves it right.
;
; far_call: a wrapper's (`jsr far_call`, then the bank and the function's
; address): the caller's bank kept on the stack, the overlay's put in, the
; function called with the fastcall argument (A, X), the caller's bank
; back, the result (A, X) returned to the wrapper's caller.
;
        .export far_go, far_call
        .exportzp far_a
        .importzp _mmc3_sel, _r6_bank

        .segment "ZEROPAGE"
far_a:  .res 1          ; a far call's fastcall argument, then its result
far_x:  .res 1
far_p:  .res 2          ; the wrapper's bytes
far_t:  .res 2          ; the function

        .code
far_go: sta     _r6_bank
        ldy     #6
        sty     _mmc3_sel
        sty     $8000
        sta     $8001
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
        lda     _r6_bank        ; the caller's bank
        pha
        ldy     #1
        lda     (far_p),y
        jsr     far_go
        lda     far_a
        ldx     far_x
        jsr     @call
        sta     far_a
        stx     far_x
        pla
        jsr     far_go
        lda     far_a
        ldx     far_x
        rts                     ; (to the wrapper's caller)
@call:  jmp     (far_t)
