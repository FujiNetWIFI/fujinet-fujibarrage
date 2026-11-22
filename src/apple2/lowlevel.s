;
; lowlevel.s -- the Apple II's hardware under the game: the clock, the
; display modes, the joystick, the speaker, ProDOS. (FujiBlast's
; src/apple2/lowlevel.s and hgr.s's frame_tick, from M.U.L.E.'s.)
;
; The clock: the game counts 60ths of a second (frame_count; vbi_count is
; the script's clock). Nothing interrupts the program, so frame_tick is
; called often (the loops that draw call it) and counts what passed:
;   3  a Mockingboard: its first 6522's timer 1 counts down every cycle,
;      unasked, so the cycles since the last call are known exactly (17030
;      a jiffy) if calls come less than 65536 cycles apart;
;   0  a //e: the vertical blank's flag ($C019), polled: a jiffy is lost
;      when no call falls in a blank (plat.c's clock_check puts them back);
;   1  a IIgs: the same flag, the other way up;
;   2  a //c: the flag latches (read through IOUDIS), so none is lost.
;
        .export         _clk_init, _frame_tick, _vbi_now, _mb_detect
        .export         _hgr_on, _text_on, _joy_read, _snd_tone, _mli
        .export         _frame_count, _vbi_count, _dsp_shake, _flashn
        .exportzp       _joy_x, _joy_y, _btn_latch, _mb_slot, _clk_mode
        .import         popa, _mk_tick

RDVBLBAR  := $C019
PTRIG     := $C070
PADDL0    := $C064
PADDL1    := $C065
BUTN0     := $C061
BUTN1     := $C062
SPKR      := $C030
ENVBL     := $C05B
IOUDISON  := $C07E
IOUDISOFF := $C07F

        .zeropage
_joy_x:     .res 1
_joy_y:     .res 1
_btn_latch: .res 1      ; a button went down since input.c last looked ($80)
_mb_slot:   .res 1      ; the Mockingboard's page ($C1-$C7), 0 none
_clk_mode:  .res 1
mbp:        .res 2

        .data                   ; (not BSS: the constructor runs first)
_frame_count: .word 0
_vbi_count:   .byte 0
_dsp_shake:   .byte 0           ; (src/6502/ply.s sets it: the Apple doesn't shake)
_flashn:      .byte 0           ; jiffies of the flash left (full text: white)
last:         .byte 0           ; the VBL flag as last seen
t1last:       .word 0           ; timer 1 as last read
acc:          .word 0           ; cycles counted towards the next jiffy

        .segment "LOWCODE"

; void clk_init(uint8_t mode): the clock's source (3 a Mockingboard in
; _mb_slot). The //c's VBL flag is enabled with the CPU's IRQs masked (the
; game takes none) and IOUDIS left on (the flag is read through it).
.proc   _clk_init
        sta     _clk_mode
        sei
        cmp     #2
        bne     :+
        sta     IOUDISOFF
        bit     ENVBL
        sta     IOUDISON
        bit     PTRIG
:       cmp     #3
        bne     :+
        lda     _mb_slot        ; timer 1's counter at $Cn04 (low), $Cn05 (high)
        sta     t1h+2
        sta     t1l+2
        sta     t1h2+2
        sta     t1l2+2
        jsr     t1read
        sta     t1last
        stx     t1last+1
:       rts
.endproc

; A, X = timer 1's count (low, high), read so a carry between the bytes
; can't tear it
t1read:
t1h:    ldx     $C405
t1l:    lda     $C404
t1h2:   cpx     $C405
        beq     :+
t1l2:   lda     $C404           ; (the high byte moved: the low again, with it)
        dex
:       rts

; void frame_tick(void): the jiffies since the last call counted
_frame_tick:
        lda     _clk_mode
        beq     iie
        cmp     #3
        bne     :+
        jmp     mb
:       cmp     #2
        beq     iic
        lda     RDVBLBAR        ; the IIgs
        eor     #$80
        and     #$80            ; $00 in the blank
        cmp     last
        beq     buttons
        sta     last
        tax
        bne     buttons
        beq     jiffy
iie:    lda     RDVBLBAR        ; the //e, the usual case: no edge, out at once
        and     #$80
        cmp     last
        beq     buttons
        sta     last
        tax
        bne     buttons         ; drawing began
        beq     jiffy
iic:    bit     RDVBLBAR
        bpl     buttons
        bit     PTRIG
jiffy:  inc     _frame_count
        bne     :+
        inc     _frame_count+1
:       inc     _vbi_count
        jsr     _mk_tick        ; (mock.s: the sound's jiffy)
        lda     _flashn         ; a flash: its jiffies, then the picture again
        beq     buttons
        dec     _flashn
        bne     buttons
        sta     $C050
buttons:
        lda     BUTN0
        ora     BUTN1
        and     #$80
        ora     _btn_latch
        sta     _btn_latch
        rts
mb:     jsr     t1read          ; the cycles since: last - now (it counts down)
        sta     mbp
        stx     mbp+1
        sec
        lda     t1last
        sbc     mbp
        tay
        lda     t1last+1
        sbc     mbp+1
        tax
        lda     mbp
        sta     t1last
        lda     mbp+1
        sta     t1last+1
        tya                     ; acc += them (17 bits: a carry is a jiffy's worth)
        clc
        adc     acc
        sta     acc
        txa
        adc     acc+1
        sta     acc+1
        bcc     @j
        lda     acc             ; (past 65535: 65536 - 17030 = 48506 left over, and
        clc                     ;  the jiffy it held)
        adc     #<(65536-17030)
        sta     acc
        lda     acc+1
        adc     #>(65536-17030)
        sta     acc+1
        jsr     jiffy
@j:     lda     acc+1           ; each 17030 a jiffy
        cmp     #>17030
        bcc     buttons
        bne     @s
        lda     acc
        cmp     #<17030
        bcc     buttons
@s:     lda     acc
        sec
        sbc     #<17030
        sta     acc
        lda     acc+1
        sbc     #>17030
        sta     acc+1
        jsr     jiffy
        jmp     @j

; uint8_t vbi_now(void): the script's clock, ticked (plat.h's vbi_count)
_vbi_now:
        jsr     _frame_tick
        lda     _vbi_count
        ldx     #0
        rts

; uint8_t mb_detect(void): the Mockingboard's slot page ($C1-$C7) or 0.
; A 6522's timer 1 counts down every cycle: two reads 109 cycles apart
; differ by about that (emulators' timers move in steps). It is left
; running: the clock's. (FujiBlast's src/apple2/mock.s.)
.proc   _mb_detect
        lda     #$C7
@slot:  sta     mbp+1
        lda     #0
        sta     mbp
        ldy     #4              ; start timer 1 (emulators' 6522s don't count
        lda     #$FF            ; until it is; a real one always does; other
        sta     (mbp),y         ; cards' $Cnxx is ROM, which takes no writes)
        iny
        sta     (mbp),y
        dey
        lda     (mbp),y         ; (5 cycles: read on the last)
        sta     tmp             ; 4
        ldx     #20             ; 2
:       dex                     ; 20 x 5 - 1 = 99
        bne     :-
        lda     (mbp),y         ; 5: about 109 cycles after
        eor     #$FF            ; tmp - A, the cycles it counted
        sec
        adc     tmp
        sec
        sbc     #100            ; 100..120: a 6522 counting
        cmp     #21
        bcs     @no
        lda     mbp+1
        sta     _mb_slot
        ldx     #0
        rts
@no:    lda     mbp+1
        sec
        sbc     #1
        cmp     #$C0
        bne     @slot
        lda     #0
        sta     _mb_slot
        tax
        rts
        .data
tmp:    .byte   0
        .segment "LOWCODE"
.endproc

; void hgr_on(void): high-res page 1 over mixed mode's four text rows;
; 80STORE off, 40 columns, AN3 on (not double high-res)
.proc   _hgr_on
        sta     $C000           ; 80STORE off
        sta     $C00C           ; 40 columns
        sta     $C05F           ; AN3 on
        sta     $C057           ; high-res
        sta     $C054           ; page 1
        sta     $C053           ; mixed
        sta     $C050           ; graphics
        rts
.endproc

; void text_on(void): the 40x24 text page 1
.proc   _text_on
        sta     $C000
        sta     $C00C
        sta     $C054
        sta     $C051           ; text
        rts
.endproc

; void joy_read(void): both paddle timers in one loop, into joy_x and joy_y
; (timers still running are waited out first)
.proc   _joy_read
        ldy     #0
:       lda     PADDL0
        ora     PADDL1
        bpl     :+
        dey
        bne     :-
:       lda     #0
        sta     _joy_x
        sta     _joy_y
        tax
        lda     PTRIG
loop:   lda     PADDL0
        bpl     :+
        inc     _joy_x
:       lda     PADDL1
        bpl     :+
        inc     _joy_y
:       inx
        beq     done
        lda     PADDL0
        ora     PADDL1
        bmi     loop
done:   rts
.endproc

; void snd_tone(uint8_t delay, uint8_t n): n clicks of the speaker, delay x
; 14 cycles apart (a square wave of 1020000 / (28 * delay) Hz)
.proc   _snd_tone
        sta     count
        jsr     popa
        sta     delay
        lda     count
        beq     done
loop:   bit     SPKR
        ldx     delay
:       pha
        pla
        nop
        dex
        bne     :-
        dec     count
        bne     loop
done:   rts
        .bss
count:  .res 1
delay:  .res 1
        .segment "LOWCODE"
.endproc

; uint8_t __fastcall__ mli(uint8_t cmd, void *parms): a ProDOS MLI call
; (written into the code, which is RAM); its error, 0 for none
.proc   _mli
        sta     parms
        stx     parms+1
        jsr     popa
        sta     cmd
        jsr     $BF00
cmd:    .byte   0
parms:  .addr   0
        ldx     #0
        rts
.endproc

; plat.c's plat_boot runs as a constructor: before main, in the ONCE
; segment (BSS takes the room after). First both stacks' free room is
; filled with $A5 (the 6502's below what is in use, the C stack's page but
; its top 32 bytes): a rig counts what is left of them (the deepest use).
        .import         _plat_boot
        .segment "ONCE"
boot:   tsx
        txa
        sec
        sbc     #16
        tax
        lda     #$A5
:       sta     $0100,x
        dex
        bne     :-
        sta     $0100
        ldx     #0
:       sta     $BE00,x
        inx
        cpx     #$E0
        bne     :-
        jmp     _plat_boot
        .constructor    boot
