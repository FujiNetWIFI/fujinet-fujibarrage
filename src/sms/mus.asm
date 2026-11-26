; mus.asm -- the Master System's tunes: the title's march, a round's fanfare,
; the game's end, the shop's ring (sound.h's snd_tune: src/sound/snd.asm
; hands them here, SMS_MUSIC), played a frame at a time from the IRQ
; (smsint.asm) out of a ROM bank: tools/sms/mkmus_sms.py's streams.
;
; With a YM2413 (a Japanese SMS, a Mark III's FM unit: found at start-up,
; port F2h keeps what is written to it) the tune is the FM stream: rhythm
; mode's drums, lead, harmony, bass and a pad; the PSG keeps the effects.
; Without, it is the PSG stream: the lead on tone 1, a harmony on tone 0,
; the bass on tone 2, the drums on the noise. The effects come first
; (src/sn76489/sn.asm runs before this in the frame): a part plays only
; while sn.asm leaves its channel silent (its shadow's attenuation 15), and
; what the tune writes, sn.asm's shadow forgets (so it writes all of its own
; when it takes the channel back); a part that gets its channel back writes
; all of its own again.
;
; C: void sms_tune(const uint8_t *ch1, const uint8_t *ch2) (snd_tune's: the
; tune is known by its first channel's label, gen/sms/tunes.asm), void
; sms_mus_stop(void). Both only ask: the IRQ does it.

        SECTION code_user

        PUBLIC  _sms_tune, _sms_mus_stop, _mus_init, mus_frame, _fm_on, _mus_playing
        EXTERN  _t_title1, _t_win1, _t_over1, _t_shop1
        EXTERN  s_tone, s_nctl, s_natt

        defc    PSG_BANK = 4
        defc    FM_BANK = 5
        defc    SN = $7F
        defc    FM_ADDR = $F0
        defc    FM_DATA = $F1
        defc    FM_CTL = $F2
        defc    MP_CTL = $FFFC
        defc    MP_SLOT2 = $FFFF
        defc    STOP = $FF

; void sms_tune(const uint8_t *ch1, const uint8_t *ch2)
_sms_tune:
        ld      hl,2
        add     hl,sp
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      hl,tunes
        ld      b,4
st_f:   ld      a,(hl)
        inc     hl
        cp      e
        ld      a,(hl)
        inc     hl
        jr      nz,st_n
        cp      d
        jr      z,st_y
st_n:   djnz    st_f
        ld      a,STOP          ; (not one of ours: quiet)
        ld      (mus_req),a
        ret
st_y:   ld      a,5
        sub     b               ; 1-4
        ld      (mus_req),a
        ret
tunes:  defw    _t_title1, _t_win1, _t_over1, _t_shop1

; void sms_mus_stop(void)
_sms_mus_stop:
        ld      a,STOP
        ld      (mus_req),a
        ret

; void mus_init(void): a YM2413? (written values come back in F2h's low bits)
_mus_init:
        xor     a
        ld      (_fm_on),a
        ld      (mus_req),a
        ld      (_mus_playing),a
        ld      b,3
mi_l:   ld      a,b
        out     (FM_CTL),a
        in      a,(FM_CTL)
        and     3
        cp      b
        ret     nz              ; (an export console: the pad's port)
        dec     b
        jp      p,mi_l
        ld      a,1
        ld      (_fm_on),a
        ld      a,3             ; the PSG and the FM both
        out     (FM_CTL),a
        jp      fm_quiet

; mus_frame: the IRQ's step (A, BC, DE, HL, IX free)
mus_frame:
        ld      a,(mus_req)
        or      a
        jr      z,mf_play
        push    af
        xor     a
        ld      (mus_req),a
        call    m_stop
        pop     af
        cp      STOP
        ret     z
        dec     a               ; tune A: its stream's address, word A of the bank
        add     a,a
        ld      (mf_id),a
        call    bank_in
        ld      a,(mf_id)
        ld      l,a
        ld      h,$80
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      (m_ptr),de
        call    bank_out
        xor     a
        ld      (m_wait),a
        ld      hl,p_dirty      ; (every part writes its own when it plays)
        ld      b,5
mf_d:   ld      (hl),1
        inc     hl
        djnz    mf_d
        ld      a,1
        ld      (_mus_playing),a
mf_play:
        ld      a,(_mus_playing)
        or      a
        ret     z
        call    bank_in
        ld      a,(_fm_on)
        or      a
        jr      z,mf_psg
        call    fm_step
        jr      mf_x
mf_psg: call    ps_step
        call    ps_out
mf_x:   jp      bank_out

; bank_in: the tune's bank in slot 2 (what was there kept); bank_out: back
bank_in:
        ld      a,(MP_CTL)
        ld      (m_was),a
        ld      a,(MP_SLOT2)
        ld      (m_was+1),a
        xor     a
        ld      (MP_CTL),a
        ld      a,(_fm_on)
        or      a
        ld      a,PSG_BANK
        jr      z,bi_1
        ld      a,FM_BANK
bi_1:   ld      (MP_SLOT2),a
        ret
bank_out:
        ld      a,(m_was+1)
        ld      (MP_SLOT2),a
        ld      a,(m_was)
        ld      (MP_CTL),a
        ret

; m_stop: the tune's channels quiet
m_stop: ld      a,(_mus_playing)
        or      a
        ret     z
        xor     a
        ld      (_mus_playing),a
        ld      a,(_fm_on)
        or      a
        jp      nz,fm_quiet
        ld      hl,owned        ; the PSG's channels the tune had: silent
        ld      b,0
ms_l:   ld      a,(hl)
        or      a
        jr      z,ms_n
        ld      (hl),0
        ld      a,b             ; 9xh | 0Fh: channel B silent (3: the noise)
        rrca
        rrca
        rrca
        or      $9F
        out     (SN),a
ms_n:   inc     hl
        inc     b
        ld      a,b
        cp      4
        jr      c,ms_l
        ret

; ---- the PSG ---------------------------------------------------------------------

; ps_step: the stream's events up to the next wait
ps_step:
        ld      a,(m_wait)
        or      a
        jr      z,pe_go
        dec     a
        ld      (m_wait),a
        ret
pe_go:  ld      hl,(m_ptr)
pe_l:   ld      a,(hl)
        inc     hl
        or      a
        jr      z,pe_end
        cp      $01
        jr      z,pe_jp
        cp      $40
        jr      nc,pe_w
        ld      c,a
        and     $0F
        ld      (pe_p),a        ; the part
        ld      a,c
        and     $F0
        cp      $10
        jr      z,pe_per
        cp      $20
        jr      z,pe_att
        ld      a,(hl)          ; 30h: the noise's control (a drum: struck again)
        inc     hl
        ld      (p_nctl),a
        ld      a,1
        ld      (p_dirty+4),a
        jr      pe_l
pe_per: push    hl
        ld      a,(pe_p)
        add     a,a
        ld      hl,p_per
        call    add_hl_a
        ex      de,hl
        pop     hl
        ld      a,(hl)
        ld      (de),a
        inc     hl
        inc     de
        ld      a,(hl)
        ld      (de),a
        inc     hl
        jr      pe_dirty
pe_att: push    hl
        ld      a,(pe_p)
        ld      hl,p_att
        call    add_hl_a
        ex      de,hl
        pop     hl
        ld      a,(hl)
        ld      (de),a
        inc     hl
pe_dirty:
        push    hl
        ld      a,(pe_p)
        ld      hl,p_dirty
        call    add_hl_a
        ld      (hl),1
        pop     hl
        jr      pe_l
pe_jp:  ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ex      de,hl
        jr      pe_l
pe_w:   sub     $40
        ld      (m_wait),a
        ld      (m_ptr),hl
        ret
pe_end: ld      (m_ptr),hl
        jp      m_stop

; ps_out: each part to its channel, if sn.asm leaves it silent
; (part 0 lead -> channel 1, 1 harmony -> 0, 2 bass -> 2; 3 the noise)
ps_out: ld      b,0             ; the part
po_l:   ld      a,b
        ld      hl,chmap
        call    add_hl_a
        ld      c,(hl)          ; its channel
        ld      a,c             ; free? sn.asm's attenuation 15 (or FFh: never)
        add     a,a
        add     a,c
        ld      hl,s_tone + 2
        call    add_hl_a
        ld      a,(hl)
        cp      15
        jr      nc,po_free
        ld      hl,owned        ; an effect has it: not ours
        ld      a,c
        call    add_hl_a
        ld      (hl),0
        jr      po_n
po_free:
        ld      hl,owned
        ld      a,c
        call    add_hl_a
        ld      a,(hl)
        ld      (hl),1
        or      a
        jr      z,po_w          ; (just got it: all of it)
        ld      hl,p_dirty
        ld      a,b
        call    add_hl_a
        ld      a,(hl)
        or      a
        jr      z,po_n
po_w:   ld      hl,p_dirty
        ld      a,b
        call    add_hl_a
        ld      (hl),0
        ld      a,c             ; sn.asm's period forgotten
        add     a,a
        add     a,c
        ld      hl,s_tone
        call    add_hl_a
        ld      (hl),$FF
        inc     hl
        ld      (hl),$FF
        ld      hl,p_per        ; the period: 1cc0 low 4, then the high 6
        ld      a,b
        add     a,a
        call    add_hl_a
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      a,c
        rrca
        rrca
        rrca
        or      $80
        ld      h,a
        ld      a,e
        and     $0F
        or      h
        out     (SN),a
        ld      a,e
        srl     d
        rra
        srl     d
        rra
        srl     d
        rra
        srl     d
        rra
        and     $3F
        out     (SN),a
        ld      hl,p_att        ; the attenuation: 1cc1 att
        ld      a,b
        call    add_hl_a
        ld      a,c
        rrca
        rrca
        rrca
        or      $90
        or      (hl)
        out     (SN),a
po_n:   inc     b
        ld      a,b
        cp      3
        jp      c,po_l
        ; the noise: free while sn.asm leaves it silent
        ld      a,(s_natt)
        cp      15
        jr      nc,po_nf
        xor     a
        ld      (owned+3),a
        ret
po_nf:  ld      hl,owned+3
        ld      a,(hl)
        ld      (hl),1
        or      a
        jr      z,po_nw
        ld      a,(p_dirty+4)
        or      a
        jr      nz,po_nw
        ld      a,(p_dirty+3)
        or      a
        ret     z
        jr      po_na
po_nw:  xor     a
        ld      (p_dirty+4),a
        ld      a,$FF
        ld      (s_nctl),a      ; (sn.asm's forgotten)
        ld      a,(p_nctl)
        and     7
        or      $E0
        out     (SN),a
po_na:  xor     a
        ld      (p_dirty+3),a
        ld      a,(p_att+3)
        or      $F0
        out     (SN),a
        ret

chmap:  defb    1, 0, 2

add_hl_a:
        add     a,l
        ld      l,a
        ret     nc
        inc     h
        ret

; ---- the YM2413 -------------------------------------------------------------------

; fm_step: register writes up to the next wait
fm_step:
        ld      a,(m_wait)
        or      a
        jr      z,fe_go
        dec     a
        ld      (m_wait),a
        ret
fe_go:  ld      hl,(m_ptr)
fe_l:   ld      a,(hl)
        inc     hl
        cp      $40
        jr      nc,fe_c
        ld      c,a
        ld      a,(hl)
        inc     hl
        call    fm_wr
        jr      fe_l
fe_c:   cp      $FE
        jr      z,fe_jp
        jr      nc,fe_end
        sub     $80
        ld      (m_wait),a
        ld      (m_ptr),hl
        ret
fe_jp:  ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ex      de,hl
        jr      fe_l
fe_end: ld      (m_ptr),hl
        jp      m_stop

; fm_wr: YM2413 register C = A (12 clocks after the address, 84 after the data)
fm_wr:  push    af
        ld      a,c
        out     (FM_ADDR),a
        pop     af              ; (10 + 4: over 12)
        nop
        out     (FM_DATA),a
        push    bc
        ld      b,6
fw_l:   djnz    fw_l            ; (6 x 13 + the rest: over 84)
        pop     bc
        ret

; fm_quiet: every channel's key off, the rhythm's off
fm_quiet:
        ld      c,$20
fq_l:   xor     a
        call    fm_wr
        inc     c
        ld      a,c
        cp      $29
        jr      c,fq_l
        ld      c,$0E
        ld      a,$20
        call    fm_wr
        ret

        SECTION bss_user
_fm_on:         defs    1       ; a YM2413 plays the tunes
_mus_playing:   defs    1
mus_req:        defs    1       ; 1-4 a tune, FFh stop (the IRQ does it)
mf_id:          defs    1
pe_p:           defs    1
m_ptr:          defs    2
m_wait:         defs    1
m_was:          defs    2
p_per:          defs    6       ; the parts' periods (lead, harmony, bass)
p_att:          defs    4       ; their attenuations (the noise's 4th)
p_nctl:         defs    1
p_dirty:        defs    5       ; changed: the 3 tones, the noise's att, its control
owned:          defs    4       ; the tune has channel 0-2, the noise
