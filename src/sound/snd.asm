; snd.asm -- the sound (sound.h) on the Z80 machines: the CoCo's
; src/coco/sound.c (the Apple's mock.s) in Z80, stepped a jiffy at a time
; from the frame's NMI (snd_nmi). It makes three voices and hands them to
; the chip's back end (src/sn76489/sn.asm on the Adam: sn_update), which
; says how they sound:
;
;   A  an effect's body, or a tune's first channel
;   B  the falling shell's whistle and a talker's blips, or a tune's second
;   C  an effect's low end (a boom's rumble), or, while a tune plays, the
;      effect's body (the menus' cues)
;
; A voice is a kind (0 off, 1 tone, 2 noise), a period (111861 / Hz; a
; noise's: its bits' period) and a volume (0-15, the AY's 3 dB a step).
; The effects are tools/mksfx_z80.py's steps (the Intellivision's), the
; tunes its words (from music/).
;
; C asks; the NMI does. A cue goes into a queue of four, a tune's start
; and a stop are flags, the whistle and the blip variables: nothing C does
; is half done when the NMI comes (it can't be masked).

        SECTION code_user

        INCLUDE "proto.inc"

        PUBLIC  _snd_init, _snd_cue, _snd_full, _snd_whistle, _snd_blip, _snd_stop
        PUBLIC  _snd_busy, _snd_tune, _snd_nmi
        PUBLIC  snd_va, snd_vb, snd_vc
        EXTERN  sn_update, sn_quiet
        EXTERN  _sx_cues, _sx_booms, _sx_ui, _mus_per, _mus_nz, _frame_count

        defc    UI_TICK = $40
        defc    NOISE0 = 112

; an effect's slot (0 the body, 1 the low end)
        defc    FX_P = 0                ; 2: its next step (0: none)
        defc    FX_LEFT = 2             ; jiffies left of this one
        defc    FX_VOL = 3
        defc    FX_FL = 4               ; bit 4 noise, bit 5 the envelope
        defc    FX_PER = 5              ; 2
        defc    FX_ENV = 7              ; 2: the envelope's volume (8.8)
        defc    FX_DV = 9               ; 2: its fall a jiffy
        defc    FX_SIZE = 11
; a tune's channel
        defc    MU_P = 0                ; 2
        defc    MU_R = 2                ; 2: back from a call
        defc    MU_LEFT = 4
        defc    MU_KIND = 5             ; its voice now
        defc    MU_PER = 6              ; 2
        defc    MU_VOL = 8
        defc    MU_SIZE = 9

; ---- what C asks ----------------------------------------------------------------

; void snd_init(void)
_snd_init:
        ld      a,(_frame_count)
        ld      (last),a
        call    sn_quiet
        ld      a,1
        ld      (on),a
        ret

; void snd_cue(uint8_t id, uint8_t size)
_snd_cue:
        ld      hl,2
        add     hl,sp
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      a,(qhead)       ; (full: dropped)
        inc     a
        and     3
        ld      hl,qtail
        cp      (hl)
        ret     z
        ld      a,(qhead)
        add     a,a
        ld      hl,queue
        call    add_hl_a
        ld      (hl),e
        inc     hl
        ld      (hl),d
        ld      a,(qhead)       ; (the index last: the NMI reads up to it)
        inc     a
        and     3
        ld      (qhead),a
        ret

; void snd_full(uint8_t on): (all the voices are the field's always)
_snd_full:
        ret

; void snd_whistle(uint8_t on, uint8_t pitch): the falling shell's (the
; Atari's AUDF: the Apple's AY period 2 (p + 1) at 1020484 Hz, so the
; period here is 3.5 (p + 1))
_snd_whistle:
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        inc     hl
        ld      l,(hl)
        ld      (wh_on),a
        call    blip_per
        ld      (wh_per),hl
        ret

; blip_per: HL = 3.5 (L + 1)
blip_per:
        ld      a,l
        and     127
        inc     a
        ld      l,a
        ld      h,0
        ld      e,l
        ld      d,h
        add     hl,hl
        add     hl,de
        add     hl,hl
        add     hl,de           ; 7 (p + 1)
        srl     h
        rr      l
        ret

; void snd_blip(uint8_t pitch): a talker's letter (0: off)
_snd_blip:
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        ld      (bl_req),a
        ld      a,1
        ld      (bl_new),a
        ret

; void snd_tune(const uint8_t *ch1, const uint8_t *ch2)
_snd_tune:
        ld      hl,2
        add     hl,sp
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      (tq1),de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      (tq2),de
        ld      a,1
        ld      (tq_go),a
        ret

; void snd_stop(void)
_snd_stop:
        ld      a,1
        ld      (stop_req),a
        xor     a
        ld      (wh_on),a
        ret

; uint8_t snd_busy(void): an effect still sounding (or asked for)
_snd_busy:
        ld      hl,(fx0+FX_P)
        ld      a,h
        or      l
        ld      hl,(fx1+FX_P)
        or      h
        or      l
        ld      l,a
        ld      a,(qhead)
        ld      h,a
        ld      a,(qtail)
        cp      h
        jr      z,sb_x
        ld      l,1
sb_x:   ld      h,0
        ret

add_hl_a:
        add     a,l
        ld      l,a
        ret     nc
        inc     h
        ret

; ---- the NMI's ------------------------------------------------------------------

; void snd_nmi(void): the frame's sound (the NMI: every register but AF,
; BC, DE, HL, IX, IY kept by it)
_snd_nmi:
        ld      a,(on)
        or      a
        ret     z
        ld      a,(stop_req)
        or      a
        call    nz,do_stop
        ld      a,(tq_go)
        or      a
        call    nz,do_tune
sn_q:   ld      a,(qtail)       ; the cues asked for
        ld      hl,qhead
        cp      (hl)
        jr      z,sn_bl
        ld      b,a
        add     a,a
        ld      hl,queue
        call    add_hl_a
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      a,b
        inc     a
        and     3
        ld      (qtail),a
        call    cue
        jr      sn_q
sn_bl:  ld      a,(bl_new)      ; a blip asked for
        or      a
        jr      z,sn_j
        xor     a
        ld      (bl_new),a
        ld      a,(bl_req)
        or      a
        jr      z,sn_b0
        ld      l,a
        call    blip_per
        ld      (bl_per),hl
        ld      a,3
sn_b0:  ld      (bl_left),a
sn_j:   ld      a,(_frame_count) ; a jiffy each that passed (8 at most)
        ld      hl,last
        ld      b,(hl)
        ld      (hl),a
        sub     b
        jr      z,sn_out
        cp      9
        jr      c,sn_n
        ld      a,8
sn_n:   ld      b,a
sn_jl:  push    bc
        call    jiffy
        pop     bc
        djnz    sn_jl
sn_out: call    voices
        jp      sn_update

do_stop:
        xor     a
        ld      (stop_req),a
        ld      (mu_on),a
        ld      (bl_left),a
        ld      hl,0
        ld      (fx0+FX_P),hl
        ld      (fx1+FX_P),hl
        ld      a,(qhead)
        ld      (qtail),a
        ret

do_tune:
        xor     a
        ld      (tq_go),a
        ld      hl,(tq1)
        ld      (mu0+MU_P),hl
        ld      hl,(tq2)
        ld      (mu1+MU_P),hl
        ld      (mu0+MU_LEFT),a
        ld      (mu1+MU_LEFT),a
        ld      (mu0+MU_KIND),a
        ld      (mu1+MU_KIND),a
        ld      hl,0            ; the low end gone (C is the cues' now),
        ld      (fx1+FX_P),hl
        ld      (fx0+FX_P),hl   ;  and the body (it was on A)
        ld      a,3
        ld      (mu_on),a
        ret

; cue: E the id, D the size
cue:    ld      a,e
        cp      UI_TICK
        jr      c,cu_snd
        sub     UI_TICK
        cp      6
        ret     nc
        add     a,a
        ld      hl,_sx_ui
        call    add_hl_a
        call    ptr
        ld      ix,fx0
        jp      start
cu_snd: cp      16
        ret     nc
        cp      SND_BOOM
        jr      nz,cu_c
        ld      a,d
        and     3
        add     a,a
        add     a,a
        ld      hl,_sx_booms
        jr      cu_t
cu_c:   add     a,a
        add     a,a
        ld      hl,_sx_cues
cu_t:   call    add_hl_a
        push    hl
        call    ptr
        ld      ix,fx0
        call    start
        pop     hl
        inc     hl
        inc     hl
        ld      a,(mu_on)
        or      a
        ret     nz
        call    ptr
        ld      ix,fx1
        jp      start

; ptr: HL = the word at HL
ptr:    ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        ret

; start: effect slot IX from steps HL (0: none)
start:  ld      a,h
        or      l
        ret     z
        ld      (ix+FX_P),l
        ld      (ix+FX_P+1),h
; fx_next: slot IX's next step
fx_next:
        ld      l,(ix+FX_P)
        ld      h,(ix+FX_P+1)
        ld      a,(hl)
        or      a
        jr      nz,fn_go
        ld      (ix+FX_P),a     ; the end
        ld      (ix+FX_P+1),a
        ret
fn_go:  ld      (ix+FX_LEFT),a
        inc     hl
        ld      a,(hl)
        ld      (ix+FX_FL),a
        and     15
        ld      (ix+FX_VOL),a
        inc     hl
        ld      a,(hl)
        ld      (ix+FX_PER),a
        inc     hl
        ld      a,(hl)
        ld      (ix+FX_PER+1),a
        inc     hl
        bit     5,(ix+FX_FL)
        jr      z,fn_p
        ld      (ix+FX_ENV),0   ; the envelope: from 15, falling
        ld      (ix+FX_ENV+1),15
        ld      a,(hl)
        ld      (ix+FX_DV),a
        inc     hl
        ld      a,(hl)
        ld      (ix+FX_DV+1),a
        inc     hl
fn_p:   ld      (ix+FX_P),l
        ld      (ix+FX_P+1),h
        ret

; jiffy: everything a jiffy on
jiffy:  ld      ix,fx0
        call    fx_jiffy
        ld      ix,fx1
        call    fx_jiffy
        ld      ix,mu0
        ld      c,1
        call    mu_step
        ld      ix,mu1
        ld      c,2
        call    mu_step
        ld      a,(bl_left)
        or      a
        ret     z
        dec     a
        ld      (bl_left),a
        ret

fx_jiffy:
        ld      a,(ix+FX_P)
        or      (ix+FX_P+1)
        ret     z
        bit     5,(ix+FX_FL)
        jr      z,fj_l
        ld      l,(ix+FX_ENV)   ; the envelope falls
        ld      h,(ix+FX_ENV+1)
        ld      e,(ix+FX_DV)
        ld      d,(ix+FX_DV+1)
        or      a
        sbc     hl,de
        jr      nc,fj_e
        ld      hl,0
fj_e:   ld      (ix+FX_ENV),l
        ld      (ix+FX_ENV+1),h
fj_l:   dec     (ix+FX_LEFT)
        ret     nz
        jp      fx_next

; mu_step: tune channel IX (C its bit in mu_on) a jiffy on
mu_step:
        ld      a,(mu_on)
        and     c
        ret     z
        ld      a,(ix+MU_LEFT)
        or      a
        jr      z,ms_w
        dec     (ix+MU_LEFT)
        ret     nz
ms_w:   ld      l,(ix+MU_P)
        ld      h,(ix+MU_P+1)
        ld      e,(hl)
        inc     hl
        ld      d,(hl)          ; DE the word, HL at its high byte
        ld      a,d
        and     $F8             ; the jiffies (bits 11-15)
        jr      nz,ms_note
        ld      a,e             ; a control
        and     3
        jr      nz,ms_c1
        ld      a,(ix+MU_R)     ; return
        ld      (ix+MU_P),a
        ld      a,(ix+MU_R+1)
        ld      (ix+MU_P+1),a
        jr      ms_w
ms_c1:  cp      3
        jr      nz,ms_c2
        ld      a,c             ; the end: the channel quiet
        cpl
        ld      hl,mu_on
        and     (hl)
        ld      (hl),a
        ld      (ix+MU_KIND),0
        ret
ms_c2:  cp      1
        inc     hl              ; (HL at the address word)
        jr      nz,ms_jp
        push    hl              ; a call: back to the word after the address
        inc     hl
        inc     hl
        ld      (ix+MU_R),l
        ld      (ix+MU_R+1),h
        pop     hl
ms_jp:  ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        ld      (ix+MU_P),l
        ld      (ix+MU_P+1),h
        jr      ms_w
ms_note:
        rrca
        rrca
        rrca
        ld      (ix+MU_LEFT),a  ; (the jiffies: the high byte's top 5 bits)
        inc     hl
        ld      (ix+MU_P),l
        ld      (ix+MU_P+1),h
        ld      a,e             ; the volume: bits 7-10
        rla
        ld      a,d
        rla
        and     15
        ld      (ix+MU_VOL),a
        ld      a,e
        and     $7F             ; the code
        jr      nz,ms_snd
        ld      (ix+MU_KIND),a  ; (silent)
        ret
ms_snd: cp      NOISE0
        jr      c,ms_tone
        sub     NOISE0
        ld      hl,_mus_nz
        call    add_hl_a
        ld      a,(hl)
        ld      (ix+MU_PER),a
        ld      (ix+MU_PER+1),0
        ld      (ix+MU_KIND),2
        ret
ms_tone:
        add     a,a
        ld      hl,_mus_per
        call    add_hl_a
        ld      a,(hl)
        ld      (ix+MU_PER),a
        inc     hl
        ld      a,(hl)
        ld      (ix+MU_PER+1),a
        ld      (ix+MU_KIND),1
        ret

; ---- the voices -------------------------------------------------------------

; fx_voice: voice DE (4 bytes: kind, period, volume) from effect slot IX
fx_voice:
        ld      a,(ix+FX_P)
        or      (ix+FX_P+1)
        jr      z,vo_off
        ld      a,1
        bit     4,(ix+FX_FL)
        jr      z,fv_k
        inc     a
fv_k:   ld      (de),a
        inc     de
        ld      a,(ix+FX_PER)
        ld      (de),a
        inc     de
        ld      a,(ix+FX_PER+1)
        ld      (de),a
        inc     de
        ld      a,(ix+FX_VOL)
        bit     5,(ix+FX_FL)
        jr      z,fv_v
        ld      a,(ix+FX_ENV+1)
fv_v:   ld      (de),a
        ret
vo_off: xor     a
        ld      (de),a
        ret

; mu_voice: voice DE from tune channel IX
mu_voice:
        ld      a,(ix+MU_KIND)
        ld      (de),a
        inc     de
        ld      a,(ix+MU_PER)
        ld      (de),a
        inc     de
        ld      a,(ix+MU_PER+1)
        ld      (de),a
        inc     de
        ld      a,(ix+MU_VOL)
        ld      (de),a
        ret

; tone_voice: voice DE a tone of period HL, volume A
tone_voice:
        ex      de,hl
        ld      (hl),1
        inc     hl
        ld      (hl),e
        inc     hl
        ld      (hl),d
        inc     hl
        ld      (hl),a
        ret

voices: ld      a,(mu_on)
        or      a
        jr      z,vs_fx
        ld      ix,mu0          ; a tune: its channels on A and B, the cues on C
        ld      de,snd_va
        call    mu_voice
        ld      ix,mu1
        ld      de,snd_vb
        call    mu_voice
        ld      ix,fx0
        ld      de,snd_vc
        jr      fx_voice
vs_fx:  ld      ix,fx0          ; the effects: the body on A, the low end on C
        ld      de,snd_va
        call    fx_voice
        ld      ix,fx1
        ld      de,snd_vc
        call    fx_voice
        ld      de,snd_vb       ; B: a blip, else the whistle
        ld      a,(bl_left)
        or      a
        jr      z,vs_wh
        ld      hl,(bl_per)
        ld      a,13
        jr      tone_voice
vs_wh:  ld      a,(wh_on)
        or      a
        jr      z,vo_off
        ld      hl,(wh_per)
        ld      a,11
        jr      tone_voice

        SECTION bss_user
on:     defs    1
last:   defs    1
stop_req: defs  1
tq_go:  defs    1
tq1:    defs    2
tq2:    defs    2
qhead:  defs    1
qtail:  defs    1
queue:  defs    8
wh_on:  defs    1
wh_per: defs    2
bl_req: defs    1
bl_new: defs    1
bl_left: defs   1
bl_per: defs    2
mu_on:  defs    1
fx0:    defs    FX_SIZE
fx1:    defs    FX_SIZE
mu0:    defs    MU_SIZE
mu1:    defs    MU_SIZE
snd_va: defs    4
snd_vb: defs    4
snd_vc: defs    4
