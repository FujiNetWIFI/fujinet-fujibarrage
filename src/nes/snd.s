;
; snd.s -- the NES's sound (sound.h) on the 2A03's APU: the Z80 machines'
; engine (src/sound/snd.asm: the effects, the tunes and the falling shell's
; whistle as three voices, a jiffy at a time) in 6502, on the same data
; (tools/mksfx_z80.py --ca65: gen/nes/sfx.s, gen/nes/mus.s).
;
; What C asks only goes into a few bytes (a queue of cues, a tune to start,
; a stop, the whistle, a blip). The NMI does the rest: crt0.s calls
; nmi_hook (snd_nmi) in the vertical blank, or after the IRQ's drain when
; that ran into the NMI. The player and its data are in bank 6: snd_nmi
; puts it in R6 and the game's bank back after (r6_bank; the select from
; mmc3_sel, which anything that writes the select notes first).
;
; The voices (snd.asm's): A an effect's body or a tune's first channel, B
; the whistle and the blips or the tune's second, C an effect's low end or,
; while a tune plays, the cues' body. On the APU:
;   no tune   A pulse 1, B pulse 2, C the triangle
;   a tune    A the triangle (the bass), B pulse 2, C pulse 1
; and a voice's noise on the noise channel (A's first, then C's, then B's).
; The periods are the AY's at 1.79 MHz (the CPU's clock / 16 / Hz): a
; pulse's timer is P - 1 (an octave up while P is past 2048), the
; triangle's P / 2 - 1; a noise's rate is the nearest of the APU's
; sixteen; the AY's volume (3 dB a step) is the APU's linear 0-15, and the
; triangle, which has none, sounds from 8 up. A pulse's high timer byte is
; written only when it changes (the write restarts the wave: a click).
;
        .export _snd_init, _snd_cue, _snd_full, _snd_whistle, _snd_blip
        .export _snd_stop, _snd_busy, _snd_tune
        .import popa, popax
        .import _sx_cues, _sx_booms, _sx_ui, _mus_per, _mus_nz
        .import _frame_count, _nmi_hook
        .importzp _mmc3_sel, _r6_bank

SND_BANK = 6
UI_TICK  = $40
SND_BOOM = 1
NOISE0   = 112                  ; a tune's code from here: noise
TRI_MIN  = 8                    ; the triangle sounds from this volume up

        .segment "ZEROPAGE"
pp:     .res 2                  ; the player's pointer (the NMI's)

        .segment "SNDBSS"
; what C asks (written outside the NMI)
on:     .res 1
stop_req: .res 1
tq_go:  .res 1                  ; a tune to start: tq1, tq2
tq1:    .res 2
tq2:    .res 2
qhead:  .res 1                  ; the cues: C's index
qtail:  .res 1                  ; and the NMI's
q_id:   .res 4
q_sz:   .res 4
c_sz:   .res 1
wh_on:  .res 1                  ; the whistle: on, its pitch (the Atari's AUDF)
wh_p:   .res 1
bl_req: .res 1                  ; a blip: its pitch (0 none), new
bl_new: .res 1
; the NMI's
last:   .res 1                  ; frame_count when last played
jn:     .res 1
sz:     .res 1
bl_left: .res 1
mu_on:  .res 1                  ; a tune's channels going (bits 0, 1)
t0:     .res 1
t1:     .res 1
t2:     .res 1
t3:     .res 1
tl:     .res 1
th:     .res 1
ch:     .res 1
ctl:    .res 1
; the effects' slots: 0 the body, 1 the low end
fx_pl:  .res 2                  ; the next step (high 0: none)
fx_ph:  .res 2
fx_left: .res 2
fx_vol: .res 2
fx_fl:  .res 2                  ; bit 4 noise, bit 5 the envelope
fx_perl: .res 2
fx_perh: .res 2
fx_envl: .res 2                 ; the envelope's volume (8.8)
fx_envh: .res 2
fx_dvl: .res 2                  ; its fall a jiffy
fx_dvh: .res 2
; the tune's channels
mu_pl:  .res 2
mu_ph:  .res 2
mu_rl:  .res 2                  ; back from a call
mu_rh:  .res 2
mu_left: .res 2
mu_kind: .res 2
mu_perl: .res 2
mu_perh: .res 2
mu_vol: .res 2
; the voices A, B, C: kind (0 off, 1 tone, 2 noise), period, volume (the AY's)
v_k:    .res 3
v_pl:   .res 3
v_ph:   .res 3
v_v:    .res 3
a_hi:   .res 3                  ; the pulses' and the triangle's high timer bytes, as written
tri_on: .res 1

; ---- what C asks (the fixed banks) -------------------------------------------
        .code

; void snd_init(void): the APU's channels on and quiet, the NMI's hook
_snd_init:
        lda     #$0F            ; pulses, triangle, noise
        sta     $4015
        lda     #$08            ; no sweeps
        sta     $4001
        sta     $4005
        lda     #$30            ; volume 0, the length counters held
        sta     $4000
        sta     $4004
        sta     $400C
        lda     #$80            ; the triangle off
        sta     $4008
        lda     #$F8            ; the length counters loaded (held from now on)
        sta     $4003
        sta     $4007
        sta     $400B
        sta     $400F
        lda     #0
        sta     a_hi
        sta     a_hi+1
        sta     a_hi+2
        sta     tri_on
        lda     _frame_count
        sta     last
        lda     #<snd_nmi
        sta     _nmi_hook
        lda     #>snd_nmi       ; (the high byte last: the NMI looks at it)
        sta     _nmi_hook+1
        lda     #1
        sta     on
        rts

; void __fastcall__ snd_cue(uint8_t id, uint8_t size): SND_* (a boom's
; size 0-3) or UI_* (sound.h); a full queue drops it
_snd_cue:
        sta     c_sz
        jsr     popa
        ldy     qhead
        sta     q_id,y
        lda     c_sz
        sta     q_sz,y
        iny
        tya
        and     #3
        cmp     qtail
        beq     @x
        sta     qhead           ; (last: the NMI reads up to it)
@x:     rts

; void snd_full(uint8_t on): (the Atari's: here every voice is the field's)
_snd_full:
        rts

; void __fastcall__ snd_whistle(uint8_t on, uint8_t pitch)
_snd_whistle:
        sta     wh_p
        jsr     popa
        sta     wh_on
        rts

; void __fastcall__ snd_blip(uint8_t pitch): a talker's letter (0: off)
_snd_blip:
        sta     bl_req
        lda     #1
        sta     bl_new
        rts

; void __fastcall__ snd_tune(const uint8_t *ch1, const uint8_t *ch2)
_snd_tune:
        ldy     #0              ; (a request half written isn't taken)
        sty     tq_go
        sta     tq2
        stx     tq2+1
        jsr     popax
        sta     tq1
        stx     tq1+1
        lda     #1
        sta     tq_go
        rts

; void snd_stop(void)
_snd_stop:
        lda     #0
        sta     tq_go
        sta     wh_on
        lda     #1
        sta     stop_req
        rts

; uint8_t snd_busy(void): an effect still sounding (or asked for)
_snd_busy:
        lda     fx_ph
        ora     fx_ph+1
        bne     @y
        lda     qhead
        cmp     qtail
        beq     @n
@y:     lda     #1
@n:     ldx     #0
        rts

; snd_nmi: the NMI's hook (crt0.s: A, X, Y kept by it): bank 6 in R6, the
; frame's sound, the game's bank back
snd_nmi:
        lda     on
        beq     @x
        lda     #6
        sta     $8000
        lda     #SND_BANK
        sta     $8001
        jsr     snd_play
        lda     #6
        sta     $8000
        lda     _r6_bank
        sta     $8001
        lda     _mmc3_sel
        sta     $8000
@x:     rts

; ---- the player (bank 6) -----------------------------------------------------
        .segment "SOUND"

snd_play:
        lda     stop_req
        beq     @t
        jsr     do_stop
@t:     lda     tq_go
        beq     @q
        jsr     do_tune
@q:     ldx     qtail           ; the cues asked for
        cpx     qhead
        beq     @bl
        lda     q_sz,x
        sta     sz
        lda     q_id,x
        pha
        inx
        txa
        and     #3
        sta     qtail
        pla
        jsr     cue
        jmp     @q
@bl:    lda     bl_new          ; a blip asked for
        beq     @j
        lda     #0
        sta     bl_new
        lda     bl_req
        beq     @b0
        lda     #3
@b0:    sta     bl_left
@j:     lda     _frame_count    ; a jiffy each that passed (8 at most)
        sec
        sbc     last
        ldx     _frame_count
        stx     last
        cmp     #0
        beq     @out
        cmp     #9
        bcc     @n
        lda     #8
@n:     sta     jn
@jl:    jsr     jiffy
        dec     jn
        bne     @jl
@out:   jsr     voices
        jmp     apu_out

do_stop:
        lda     #0
        sta     stop_req
        sta     mu_on
        sta     bl_left
        sta     fx_ph
        sta     fx_ph+1
        lda     qhead
        sta     qtail
        rts

do_tune:
        lda     #0
        sta     tq_go
        sta     mu_left
        sta     mu_left+1
        sta     mu_kind
        sta     mu_kind+1
        sta     fx_ph+1         ; the low end gone (C is the cues' now)
        sta     fx_ph           ; and the body (it was on A)
        lda     tq1
        sta     mu_pl
        lda     tq1+1
        sta     mu_ph
        lda     tq2
        sta     mu_pl+1
        lda     tq2+1
        sta     mu_ph+1
        lda     #3
        sta     mu_on
        rts

; cue: A the id, sz the size
cue:    cmp     #UI_TICK
        bcc     @snd
        sbc     #UI_TICK
        cmp     #6
        bcs     @x
        asl     a
        tay
        lda     _sx_ui,y
        sta     t0
        lda     _sx_ui+1,y
        sta     t1
        ldx     #0
        jmp     start
@snd:   cmp     #16
        bcs     @x
        cmp     #SND_BOOM
        bne     @c
        lda     sz              ; a boom: by its size
        and     #3
        ldx     #<_sx_booms
        ldy     #>_sx_booms
        bne     @tab
@c:     ldx     #<_sx_cues
        ldy     #>_sx_cues
@tab:   stx     pp
        sty     pp+1
        asl     a
        asl     a
        tay
        lda     (pp),y          ; the body
        sta     t0
        iny
        lda     (pp),y
        sta     t1
        iny
        lda     (pp),y          ; the low end
        sta     t2
        iny
        lda     (pp),y
        sta     t3
        ldx     #0
        jsr     start
        lda     mu_on           ; (no low end while a tune plays)
        bne     @x
        lda     t2
        sta     t0
        lda     t3
        sta     t1
        ldx     #1
        jmp     start
@x:     rts

; start: effect slot X from the steps at t0, t1 (0: none)
start:  lda     t1
        beq     @x
        lda     t0
        sta     fx_pl,x
        lda     t1
        sta     fx_ph,x
        jmp     fx_next
@x:     rts

; fx_next: slot X's next step
fx_next:
        lda     fx_pl,x
        sta     pp
        lda     fx_ph,x
        sta     pp+1
        ldy     #0
        lda     (pp),y          ; its jiffies (0: the end)
        bne     @go
        sta     fx_ph,x
        rts
@go:    sta     fx_left,x
        iny
        lda     (pp),y          ; the volume and flags
        sta     fx_fl,x
        and     #15
        sta     fx_vol,x
        iny
        lda     (pp),y
        sta     fx_perl,x
        iny
        lda     (pp),y
        sta     fx_perh,x
        iny
        lda     fx_fl,x
        and     #$20
        beq     @p
        lda     #0              ; the envelope: from 15, falling
        sta     fx_envl,x
        lda     #15
        sta     fx_envh,x
        lda     (pp),y
        sta     fx_dvl,x
        iny
        lda     (pp),y
        sta     fx_dvh,x
        iny
@p:     tya                     ; on past this step
        clc
        adc     pp
        sta     fx_pl,x
        lda     pp+1
        adc     #0
        sta     fx_ph,x
        rts

; jiffy: everything a jiffy on
jiffy:  ldx     #0
        jsr     fx_jiffy
        ldx     #1
        jsr     fx_jiffy
        ldx     #0
        jsr     mu_step
        ldx     #1
        jsr     mu_step
        lda     bl_left
        beq     @x
        dec     bl_left
@x:     rts

fx_jiffy:
        lda     fx_ph,x
        beq     @r
        lda     fx_fl,x
        and     #$20
        beq     @l
        sec                     ; the envelope falls (to 0)
        lda     fx_envl,x
        sbc     fx_dvl,x
        sta     fx_envl,x
        lda     fx_envh,x
        sbc     fx_dvh,x
        sta     fx_envh,x
        bcs     @l
        lda     #0
        sta     fx_envl,x
        sta     fx_envh,x
@l:     dec     fx_left,x
        bne     @r
        jmp     fx_next
@r:     rts

; mu_step: tune channel X a jiffy on: a word a step (bits 0-6 the code: an
; index into mus_per, 0 silent, or NOISE0 + n a drum; 7-10 the volume;
; 11-15 the jiffies); jiffies 0 a control (0 return, 1 call, 2 jump: the
; next word an address; 3 the end)
mu_step:
        lda     mu_on
        and     chb,x
        beq     @r
        lda     mu_left,x
        beq     @w
        dec     mu_left,x
        bne     @r
@w:     lda     mu_pl,x
        sta     pp
        lda     mu_ph,x
        sta     pp+1
        ldy     #1
        lda     (pp),y          ; the high byte
        sta     t1
        and     #$F8
        bne     @note
        dey
        lda     (pp),y          ; a control
        and     #3
        bne     @c1
        lda     mu_rl,x         ; return
        sta     mu_pl,x
        lda     mu_rh,x
        sta     mu_ph,x
        jmp     @w
@c1:    cmp     #3
        bne     @c2
        lda     chb,x           ; the end: the channel quiet
        eor     #$FF
        and     mu_on
        sta     mu_on
        lda     #0
        sta     mu_kind,x
@r:     rts
@c2:    cmp     #1
        bne     @jp
        lda     pp              ; a call: back to the word after the address
        clc
        adc     #4
        sta     mu_rl,x
        lda     pp+1
        adc     #0
        sta     mu_rh,x
@jp:    ldy     #2
        lda     (pp),y
        pha
        iny
        lda     (pp),y
        sta     mu_ph,x
        pla
        sta     mu_pl,x
        jmp     @w
@note:  lsr     a
        lsr     a
        lsr     a
        sta     mu_left,x
        dey
        lda     (pp),y          ; the low byte
        sta     t0
        lda     pp              ; on past the word
        clc
        adc     #2
        sta     mu_pl,x
        lda     pp+1
        adc     #0
        sta     mu_ph,x
        lda     t0              ; the volume: bits 7-10
        asl     a
        lda     t1
        rol     a
        and     #15
        sta     mu_vol,x
        lda     t0
        and     #$7F
        bne     @snd
        sta     mu_kind,x       ; (silent)
        rts
@snd:   cmp     #NOISE0
        bcc     @tone
        sbc     #NOISE0
        tay
        lda     _mus_nz,y
        sta     mu_perl,x
        lda     #0
        sta     mu_perh,x
        lda     #2
        sta     mu_kind,x
        rts
@tone:  asl     a
        tay
        lda     _mus_per,y
        sta     mu_perl,x
        lda     _mus_per+1,y
        sta     mu_perh,x
        lda     #1
        sta     mu_kind,x
        rts

chb:    .byte   1, 2

; ---- the voices ------------------------------------------------------------

voices: lda     mu_on
        beq     @fx
        ldx     #0              ; a tune: its channels on A and B, the cues on C
        ldy     #0
        jsr     mu_voice
        ldx     #1
        ldy     #1
        jsr     mu_voice
        ldx     #0
        ldy     #2
        jmp     fx_voice
@fx:    ldx     #0              ; the effects: the body on A, the low end on C
        ldy     #0
        jsr     fx_voice
        ldx     #1
        ldy     #2
        jsr     fx_voice
        lda     bl_left         ; B: a blip, else the whistle
        beq     @wh
        lda     bl_req
        jsr     per35
        lda     #13
        bne     @tv
@wh:    lda     wh_on
        beq     @off
        lda     wh_p
        jsr     per35
        lda     #11
@tv:    sta     v_v+1
        lda     #1
        sta     v_k+1
        lda     tl
        sta     v_pl+1
        lda     th
        sta     v_ph+1
        rts
@off:   lda     #0
        sta     v_k+1
        rts

; fx_voice: voice Y from effect slot X
fx_voice:
        lda     fx_ph,x
        beq     @off
        lda     fx_fl,x
        and     #$10
        beq     @t
        lda     #2
        bne     @k
@t:     lda     #1
@k:     sta     v_k,y
        lda     fx_perl,x
        sta     v_pl,y
        lda     fx_perh,x
        sta     v_ph,y
        lda     fx_fl,x
        and     #$20
        beq     @v
        lda     fx_envh,x
        bpl     @s
@v:     lda     fx_vol,x
@s:     sta     v_v,y
        rts
@off:   lda     #0
        sta     v_k,y
        rts

; mu_voice: voice Y from tune channel X
mu_voice:
        lda     mu_kind,x
        sta     v_k,y
        lda     mu_perl,x
        sta     v_pl,y
        lda     mu_perh,x
        sta     v_ph,y
        lda     mu_vol,x
        sta     v_v,y
        rts

; per35: tl, th = 3.5 ((A & 127) + 1): the whistle's and the blips' period
; (the Atari's AUDF's note: the Apple's AY period 2 (p + 1) at 1020484 Hz)
per35:  and     #127
        clc
        adc     #1
        sta     t0
        lda     #0
        sta     th
        lda     t0
        asl     a
        rol     th
        asl     a
        rol     th
        asl     a
        rol     th              ; 8q
        sec
        sbc     t0
        sta     tl
        lda     th
        sbc     #0
        sta     th              ; 7q
        lsr     th
        ror     tl
        rts

; ---- the APU -----------------------------------------------------------------

apu_out:
        ldx     #0              ; the noise: A's, else C's, else B's
        lda     v_k
        cmp     #2
        beq     @nz
        ldx     #2
        lda     v_k+2
        cmp     #2
        beq     @nz
        ldx     #1
        lda     v_k+1
        cmp     #2
        beq     @nz
        ldx     #$FF
@nz:    jsr     noise_out
        ldy     #0              ; pulse 1: A, or C with a tune
        lda     mu_on
        beq     @p1
        ldy     #2
@p1:    ldx     #0
        jsr     pulse_out
        ldy     #1              ; pulse 2: B
        ldx     #1
        jsr     pulse_out
        ldy     #2              ; the triangle: C, or A with a tune
        lda     mu_on
        beq     @tr
        ldy     #0
@tr:    jmp     tri_out

; noise_out: the noise channel from voice X ($FF: none)
noise_out:
        cpx     #$FF
        beq     @off
        ldy     v_v,x
        lda     voltab,y
        beq     @off
        ora     #$30
        sta     $400C
        lda     v_pl,x
        and     #31
        tay
        lda     nztab,y
        sta     $400E
        rts
@off:   lda     #$30
        sta     $400C
        rts

; pulse_out: pulse X (0, 1) from voice Y
pulse_out:
        stx     ch
        txa
        asl     a
        asl     a
        tax                     ; (its registers' offset)
        lda     v_k,y
        cmp     #1
        bne     @off
        lda     v_v,y
        sty     t0
        tay
        lda     voltab,y
        beq     @off
        ora     #$B0            ; a square, the volume constant, the length held
        sta     ctl
        ldy     t0
        lda     v_pl,y
        sta     tl
        lda     v_ph,y
        sta     th
@oc:    lda     th              ; past 2048: an octave up
        cmp     #8
        bcc     @ok
        bne     @half
        lda     tl
        beq     @ok
@half:  lsr     th
        ror     tl
        jmp     @oc
@ok:    lda     tl              ; the timer: P - 1 (8 at least)
        sec
        sbc     #1
        sta     tl
        lda     th
        sbc     #0
        sta     th
        bne     @set
        lda     tl
        cmp     #8
        bcs     @set
        lda     #8
        sta     tl
@set:   lda     ctl
        sta     $4000,x
        lda     tl
        sta     $4002,x
        ldy     ch
        lda     th
        cmp     a_hi,y
        beq     @x
        sta     a_hi,y
        ora     #$F8
        sta     $4003,x
@x:     rts
@off:   lda     #$30
        sta     $4000,x
        rts

; tri_out: the triangle from voice Y
tri_out:
        lda     v_k,y
        cmp     #1
        bne     @off
        lda     v_v,y
        cmp     #TRI_MIN
        bcc     @off
        lda     v_pl,y
        sta     tl
        lda     v_ph,y
        sta     th
@oc:    lda     th              ; past 4096: an octave up
        cmp     #16
        bcc     @ok
        bne     @half
        lda     tl
        beq     @ok
@half:  lsr     th
        ror     tl
        jmp     @oc
@ok:    lsr     th              ; the timer: P / 2 - 1 (2 at least)
        ror     tl
        lda     tl
        sec
        sbc     #1
        sta     tl
        lda     th
        sbc     #0
        sta     th
        bne     @set
        lda     tl
        cmp     #2
        bcs     @set
        lda     #2
        sta     tl
@set:   lda     #$FF            ; on: the linear counter reloaded, held
        sta     $4008
        lda     tl
        sta     $400A
        lda     th
        ldx     tri_on
        beq     @w              ; (off till now: the counter needs the write)
        cmp     a_hi+2
        beq     @x
@w:     sta     a_hi+2
        ora     #$F8
        sta     $400B
        lda     #1
        sta     tri_on
@x:     rts
@off:   lda     tri_on
        beq     @r
        lda     #0
        sta     tri_on
        lda     #$80            ; the counter reloaded with 0: quiet
        sta     $4008
        lda     a_hi+2
        ora     #$F8
        sta     $400B
@r:     rts

; the AY's volume as the APU's (3 dB a step: 15 x 2 ^ ((v - 15) / 2))
voltab: .byte   0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 3, 4, 5, 8, 11, 15
; an AY noise period n (a bit every 16 n cycles) as the APU's nearest rate
nztab:  .byte   2, 2, 3, 4, 4, 5, 5, 6, 6, 7, 7, 7, 8, 8, 8, 9
        .byte   9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11
