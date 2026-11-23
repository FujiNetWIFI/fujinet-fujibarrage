*
* snd3a.asm -- the CoCo 3's sound engine (src/coco/sound.c's): three voices
* on the 6-bit DAC, each a square wave or noise whose edges the GIME timer's
* IRQ makes. The work is a step ahead, so the timer and the DAC are written
* first thing (a fixed time after the timer's 0, LAT, taken off the count):
* at each IRQ the timer gets the interval worked out at the last one and the
* DAC the levels; then every voice's count goes down by that interval,
* those at 0 flip and start their next half wave (noise: a random level a
* half wave; only its changes are edges, a run of equal bits drawn at each),
* and the next interval is the soonest edge. The timer's IRQ is on only
* while a voice sounds. Its IRQ comes by the same vector as the field
* sync's: the timer's are taken here, the rest passed on (plat.c's
* irq_isr). The DAC's writes keep bit 1, the bit-banger's serial line (a
* FujiNet's DriveWire), at mark.
*
* Its variables are at fixed addresses: the IRQ can't count on DP.
*
        section mid

        include "../6809/dp.inc"

        export  _irq3,_eng_init,_eng_voice,_eng_level,_eng_gate
        import  _irq_isr

* a voice: in the direct page ($0EB0-: the IRQ sets DP itself)
V_ON    equ     0
V_NZ    equ     1
V_HALF  equ     2               ; 2: a half wave, in the timer's counts (3579545 Hz)
V_CNT   equ     4               ; 2: to its next edge
V_VOL   equ     6
V_LVL   equ     7               ; its level now (0 or V_VOL)
V_SIZE  equ     8
vox     equ     $B0             ; three of them
tcur    equ     $C8             ; 2: the interval from the next IRQ to the one after
tnext   equ     $CA             ; 2: (less LAT: what the timer gets)
tmin    equ     $CC             ; 2
lfsr    equ     $CE
dacnext equ     $CF
running equ     $D0
gated   equ     $D1
LAT     equ     132             ; counts from the timer's 0 to its write (measured: tools/cocosim, scene 9)
MINT    equ     LAT+900         ; the shortest interval (an IRQ takes ~255 cycles, 510 counts: under half the CPU)
MAXT    equ     LAT+4095        ; the longest (12 bits)
IRQON   equ     $6C             ; $FF90: CoCo 3, MMU, the GIME's IRQ, constant page
IRQOFF  equ     $4C

irq3f   jmp     _irq_isr        ; (not the timer's)
_irq3   lda     $FF92           ; (read: acknowledged)
        bita    #$20            ; the timer's?
        beq     irq3f
        lda     #DPAGE
        tfr     a,dp
        ldd     <tnext
        stb     $FF95
        sta     $FF94           ; (the count starts again here)
        lda     <dacnext
        sta     $FF20
        ldd     #MAXT
        std     <tmin
        tst     <vox+0*V_SIZE+V_ON           ; voice 0
        beq     n0
        ldd     <vox+0*V_SIZE+V_CNT
        subd    <tcur
        bgt     k0            ; no edge yet
        addd    <vox+0*V_SIZE+V_HALF       ; an edge: the next from it
        bgt     s0
        ldd     <vox+0*V_SIZE+V_HALF       ; (fell behind: from now)
s0      std     <vox+0*V_SIZE+V_CNT
        lda     <vox+0*V_SIZE+V_LVL        ; the level flips (noise: a run of equal bits ends)
        eora    <vox+0*V_SIZE+V_VOL
        sta     <vox+0*V_SIZE+V_LVL
        tst     <vox+0*V_SIZE+V_NZ
        beq     m0
r0      lsr     <lfsr           ; noise: its next run, a half wave more for each
        bcs     x0              ;  0 drawn (a random bit a half wave, as heard)
        ldd     <vox+0*V_SIZE+V_CNT
        addd    <vox+0*V_SIZE+V_HALF
        std     <vox+0*V_SIZE+V_CNT
        bra     r0
x0      lda     <lfsr
        eora    #$B8
        sta     <lfsr
m0      ldd     <vox+0*V_SIZE+V_CNT
        bra     c0
k0      std     <vox+0*V_SIZE+V_CNT
c0      cmpd    <tmin
        bhs     n0
        std     <tmin
n0
        tst     <vox+1*V_SIZE+V_ON           ; voice 1
        beq     n1
        ldd     <vox+1*V_SIZE+V_CNT
        subd    <tcur
        bgt     k1            ; no edge yet
        addd    <vox+1*V_SIZE+V_HALF       ; an edge: the next from it
        bgt     s1
        ldd     <vox+1*V_SIZE+V_HALF       ; (fell behind: from now)
s1      std     <vox+1*V_SIZE+V_CNT
        lda     <vox+1*V_SIZE+V_LVL        ; the level flips (noise: a run of equal bits ends)
        eora    <vox+1*V_SIZE+V_VOL
        sta     <vox+1*V_SIZE+V_LVL
        tst     <vox+1*V_SIZE+V_NZ
        beq     m1
r1      lsr     <lfsr           ; noise: its next run, a half wave more for each
        bcs     x1              ;  0 drawn (a random bit a half wave, as heard)
        ldd     <vox+1*V_SIZE+V_CNT
        addd    <vox+1*V_SIZE+V_HALF
        std     <vox+1*V_SIZE+V_CNT
        bra     r1
x1      lda     <lfsr
        eora    #$B8
        sta     <lfsr
m1      ldd     <vox+1*V_SIZE+V_CNT
        bra     c1
k1      std     <vox+1*V_SIZE+V_CNT
c1      cmpd    <tmin
        bhs     n1
        std     <tmin
n1
        tst     <vox+2*V_SIZE+V_ON           ; voice 2
        beq     n2
        ldd     <vox+2*V_SIZE+V_CNT
        subd    <tcur
        bgt     k2            ; no edge yet
        addd    <vox+2*V_SIZE+V_HALF       ; an edge: the next from it
        bgt     s2
        ldd     <vox+2*V_SIZE+V_HALF       ; (fell behind: from now)
s2      std     <vox+2*V_SIZE+V_CNT
        lda     <vox+2*V_SIZE+V_LVL        ; the level flips (noise: a run of equal bits ends)
        eora    <vox+2*V_SIZE+V_VOL
        sta     <vox+2*V_SIZE+V_LVL
        tst     <vox+2*V_SIZE+V_NZ
        beq     m2
r2      lsr     <lfsr           ; noise: its next run, a half wave more for each
        bcs     x2              ;  0 drawn (a random bit a half wave, as heard)
        ldd     <vox+2*V_SIZE+V_CNT
        addd    <vox+2*V_SIZE+V_HALF
        std     <vox+2*V_SIZE+V_CNT
        bra     r2
x2      lda     <lfsr
        eora    #$B8
        sta     <lfsr
m2      ldd     <vox+2*V_SIZE+V_CNT
        bra     c2
k2      std     <vox+2*V_SIZE+V_CNT
c2      cmpd    <tmin
        bhs     n2
        std     <tmin
n2
        lda     <vox+V_LVL      ; the DAC at the next edge
        adda    <vox+V_SIZE+V_LVL
        adda    <vox+2*V_SIZE+V_LVL
        cmpa    #63
        bls     dacok
        lda     #63
dacok   lsla
        lsla
        ora     #2
        sta     <dacnext
        ldd     <tmin           ; the interval after it
        cmpd    #MINT
        bhs     tok
        ldd     #MINT
tok     std     <tcur
        subd    #LAT
        std     <tnext
        rti

* void eng_init(void): quiet, and the IRQ's vector ($010D: plat.c's
* plat_irq_init set it to irq_isr before this code was loaded) ours
_eng_init
        pshs    cc
        orcc    #$50
        ldx     #_irq3
        stx     $010D
        puls    cc
        ldx     #DPBASE+vox
@c      clr     ,x+
        cmpx    #DPBASE+vox+3*V_SIZE
        blo     @c
        clr     DPBASE+running
        lda     #2
        sta     DPBASE+dacnext
        lda     #1
        sta     DPBASE+lfsr
        lda     #$20            ; the timer counts 279 ns
        sta     $FF91
        rts

* the voice in slot B: X
voice   lda     #V_SIZE
        mul
        addd    #DPBASE+vox
        tfr     d,x
        rts

* any voice on? (A) else the timer's IRQ off and the DAC quiet
stopq   lda     DPBASE+vox+V_ON
        ora     DPBASE+vox+V_SIZE+V_ON
        ora     DPBASE+vox+2*V_SIZE+V_ON
        bne     @x
        tst     DPBASE+running
        beq     @x
        clr     DPBASE+running
        clr     $FF92
        lda     #IRQOFF
        sta     $FF90
        lda     #2
        sta     $FF20
        sta     DPBASE+dacnext
@x      rts

* the timer's IRQ on, from now
startq  tst     DPBASE+running
        bne     @x
        lda     #1
        sta     DPBASE+running
        ldd     #MINT
        std     DPBASE+tcur
        subd    #LAT
        std     DPBASE+tnext
        stb     $FF95
        sta     $FF94
        lda     DPBASE+gated
        bne     @x
        lda     #$20
        sta     $FF92
        lda     #IRQON
        sta     $FF90
@x      rts

* void eng_voice(uint8_t v, uint16_t units, uint8_t lvl, uint8_t noise)
* (after pshs cc: v at 4,s, units 5-6, lvl 8, noise 10)
_eng_voice
        pshs    cc
        orcc    #$50
        ldb     4,s
        bsr     voice
        lda     8,s
        beq     @off
        ldd     5,s
        std     V_HALF,x
        lda     10,s
        sta     V_NZ,x
        lda     8,s
        sta     V_VOL,x
        tst     V_ON,x
        bne     @run
        ldd     V_HALF,x        ; (from silence: a half wave in)
        std     V_CNT,x
        clr     V_LVL,x
        lda     #1
        sta     V_ON,x
        bsr     startq
        puls    cc,pc
@run    tst     V_LVL,x         ; (going: up at the new level)
        beq     @c
        lda     V_VOL,x
        sta     V_LVL,x
@c      ldd     V_CNT,x         ; its next edge no further than a half wave
        cmpd    V_HALF,x
        bls     @x
        ldd     V_HALF,x
        std     V_CNT,x
@x      puls    cc,pc
@off    clr     V_ON,x
        clr     V_LVL,x
        lbsr    stopq
        puls    cc,pc

* void eng_level(uint8_t v, uint8_t lvl)  (after pshs cc: v at 4,s, lvl 6)
_eng_level
        pshs    cc
        orcc    #$50
        ldb     4,s
        lbsr    voice
        tst     V_ON,x
        beq     @x
        lda     6,s
        beq     @off
        sta     V_VOL,x
        tst     V_LVL,x
        beq     @x
        sta     V_LVL,x
@x      puls    cc,pc
@off    clr     V_ON,x
        clr     V_LVL,x
        lbsr    stopq
        puls    cc,pc

* void eng_gate(uint8_t go): the DAC and the mux for sound (1), or for the
* joystick (0: the timer's IRQ held off meanwhile)
_eng_gate
        pshs    cc
        orcc    #$50
        lda     4,s
        beq     @no
        clr     DPBASE+gated
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
        tst     DPBASE+running
        beq     @x
        lda     #$20
        sta     $FF92
        lda     #IRQON
        sta     $FF90
@x      puls    cc,pc
@no     lda     #1
        sta     DPBASE+gated
        tst     DPBASE+running
        beq     @n
        clr     $FF92
        lda     #IRQOFF
        sta     $FF90
@n      lda     $FF23
        anda    #$F7
        sta     $FF23
        puls    cc,pc

