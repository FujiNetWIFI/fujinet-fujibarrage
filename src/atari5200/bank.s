;
; bank.s -- the 5200's two programs in one 128K Super Cart (barrage-a52.cfg,
; menu-a52.cfg): the menu is bank 0's (its entry $4000), the game banks
; 1-3's (bank 3's header names its entry; the console starts in bank 3).
; A read of $BFD0 + 4n picks bank n, any read of $BFE0-$BFFF bank 3.
;
; Going from one to the other is the Atari's reboot between its two
; programs: the interrupts off, a stub in RAM (the ROM it runs from goes
; away under it) that switches the bank and jumps, and the other program's
; crt0 starts from nothing. What passes between them is in the appkeys.
;
;   uint8_t xload(void)       the game: no name or room, so the menu
;   void plat_reboot(void)    either: the game (it reads the appkeys)
;
; This file and the crt0 are the only code that reads the bank switch
; (tools/atari5200/mkrom.py checks it).
;
        .export _xload, _plat_reboot

NMIEN   = $D40E
DMACTL  = $D400
IRQEN   = $E80E
STUB    = $0100         ; the stack page's bottom: nothing is pushed that deep

        .code

_xload: ldy     #to_menu - stubs
        jmp     go
_plat_reboot:
        ldy     #to_game - stubs
go:     sei
        lda     #0
        sta     NMIEN
        sta     DMACTL
        sta     IRQEN
        ldx     #0
@c:     lda     stubs,y
        sta     STUB,x
        iny
        inx
        cpx     #STUB_LEN
        bne     @c
        jmp     STUB

        .rodata
stubs:
to_menu:
        lda     $BFD0           ; bank 0: the menu
        jmp     $4000
STUB_LEN = * - to_menu
to_game:
        lda     $BFE0           ; bank 3 (the claim's first byte)
        jmp     ($BFFE)         ; the game's entry, from bank 3's header
        .assert * - to_game = STUB_LEN, error, "the stubs differ in length"

; ---- the game's far calls (gen/atari5200/far.s: tools/atari5200/mkfar.py) --
; far_go: bank A in (its read: $BFD0 + 4A), and noted in far_bank (X kept).
; The game starts in bank 3; an overlay's call puts the caller's back.
        .export far_go, far_bank
        .exportzp far_a

        .segment "ZEROPAGE"
far_a:  .res 1          ; a far call's fastcall argument, then its result

        .data
far_bank: .byte 3       ; the bank that's in (the console starts in 3)

        .code
far_go: sta     far_bank
        asl
        asl
        tay
        lda     $BFD0,y
        rts
