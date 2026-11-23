*
* snd2a.asm -- the CoCo 1/2's sound engine (src/coco/sound.c's): three
* voices mixed by the CPU into the 6-bit DAC while it would only wait (for
* a jiffy's end: eng_play). A sample every 129 cycles (6937 a second, the
* loop's every path the same: tools/mksfx_coco.py's CYC2): each voice's
* phase (16 bits) goes on by its step, and its high byte picks the level
* from its wave (a square: the first half up; noise: the Disk ROM's copy at
* $C000, bytes as good as random), masked by its volume. The DAC's writes
* keep bit 1, the bit-banger's serial line (a FujiNet's DriveWire), at mark.
*
* The voices are in the direct page ($0E90-, past the player's), set from
* C (eng_voice, eng_level) between the mixer's runs.
*
        section code

        include "../6809/dp.inc"

        export  _eng_init,_eng_voice,_eng_level,_eng_gate,_eng_play
        import  _frame_count

* a voice
ph0     equ     $90             ; 2: its phase
st0     equ     $92             ; 2: its step a sample (0: still)
wv0     equ     $94             ; 2: its wave (+128: indexed by the phase's high byte, signed)
lv0     equ     $96             ; its volume (a mask: 0 quiet)
VS      equ     7
ph1     equ     ph0+VS
st1     equ     st0+VS
wv1     equ     wv0+VS
lv1     equ     lv0+VS
ph2     equ     ph0+2*VS
st2     equ     st0+2*VS
wv2     equ     wv0+2*VS
lv2     equ     lv0+2*VS
acc     equ     $AB
mseen   equ     $AC
mact    equ     $AD             ; any voice sounds
NOISE   equ     $C000+128

* the square wave: phase 0-$7FFF up, $8000-$FFFF down (as indexed: +128)
square  fcb     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
        fcb     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
        fcb     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
        fcb     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
sqmid   fcb     $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
        fcb     $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
        fcb     $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
        fcb     $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
        fcb     $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
        fcb     $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
        fcb     $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
        fcb     $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF

* void eng_init(void)
_eng_init
        ldx     #DPBASE+ph0
@c      clr     ,x+
        cmpx    #DPBASE+mact+1
        blo     @c
        ldd     #sqmid
        std     DPBASE+wv0
        std     DPBASE+wv1
        std     DPBASE+wv2
        rts

* the voice in slot B: X (its phase)
voice   lda     #VS
        mul
        addd    #DPBASE+ph0
        tfr     d,x
        rts

* mact = any volume
active  lda     DPBASE+lv0
        ora     DPBASE+lv1
        ora     DPBASE+lv2
        sta     DPBASE+mact
        rts

* void eng_voice(uint8_t v, uint16_t units, uint8_t lvl, uint8_t noise)
* (v at 3,s, units 4-5, lvl 7, noise 9)
_eng_voice
        ldb     3,s
        bsr     voice
        ldd     4,s
        std     2,x             ; its step
        ldd     #sqmid
        tst     9,s
        beq     @w
        ldd     #NOISE
@w      std     4,x
        lda     7,s
        sta     6,x
        bra     active

* void eng_level(uint8_t v, uint8_t lvl)  (v at 3,s, lvl 5)
_eng_level
        ldb     3,s
        bsr     voice
        lda     5,s
        sta     6,x
        bra     active

* void eng_gate(uint8_t go): the DAC and the mux for sound (1), or for the
* joystick (0)
_eng_gate
        lda     3,s
        beq     @no
        lda     $FF01           ; the mux on the DAC
        anda    #$F7
        sta     $FF01
        lda     $FF03
        anda    #$F7
        sta     $FF03
        lda     #2
        sta     $FF20
        lda     $FF23           ; sound on
        ora     #$08
        sta     $FF23
        rts
@no     lda     $FF23
        anda    #$F7
        sta     $FF23
        rts

* void eng_play(uint8_t seen): until frame_count's low byte isn't seen,
* the voices sounding (or, all quiet, a wait)
_eng_play
        pshs    u,y,dp
        lda     #DPAGE
        tfr     a,dp
        lda     8,s
        sta     <mseen
        tst     <mact
        bne     @s
@w      lda     _frame_count+1
        cmpa    <mseen
        beq     @w
        puls    u,y,dp,pc
@s      ldd     <ph0            ; voice 0             (34 cycles)
        addd    <st0
        std     <ph0
        ldx     <wv0
        ldb     a,x
        andb    <lv0
        stb     <acc
        ldd     <ph1            ; voice 1             (38)
        addd    <st1
        std     <ph1
        ldx     <wv1
        ldb     a,x
        andb    <lv1
        addb    <acc
        stb     <acc
        ldd     <ph2            ; voice 2             (34)
        addd    <st2
        std     <ph2
        ldx     <wv2
        ldb     a,x
        andb    <lv2
        addb    <acc
        lslb                    ; the DAC             (11)
        lslb
        orb     #2
        stb     $FF20
        lda     _frame_count+1  ; the jiffy's end?    (12)
        cmpa    <mseen
        beq     @s
        puls    u,y,dp,pc
