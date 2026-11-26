; sn.asm -- the sound's back end on the SN76489 (the Adam's, port FFh; the
; ColecoVision's): src/sound/snd.asm's three voices onto its three tones
; and its noise, each register written only when it changes (a write to
; the noise's control starts its shift register again).
;
;   A  tone 1        B  tone 2        C  tone 3
;
; The noise goes to one voice that wants it, A's first, then C's, then
; B's; it wants it to sound noise, or a tone under 109 Hz (a period past
; 1023, beyond the tones' 10 bits): true bass, the noise's periodic mode
; clocked by tone 3 (at a fifteenth of tone 3: period / 15). A noise
; clocked by tone 3 is the effects' own rate exactly (tone 3's period is
; the noise's); when C has tone 3 the noise takes the nearest of the
; chip's fixed rates, and a bass that can't have tone 3 goes up octaves
; on its own tone. Tone 3 driving the noise is silent itself.
;
; The volume: the AY's 3 dB a step as the SN's 2 dB attenuation.

        SECTION code_user

        PUBLIC  sn_update, sn_quiet
IFDEF SN_SEGA                           ; (the SMS's tunes share the chip: src/sms/mus.asm)
        PUBLIC  s_tone, s_nctl, s_natt
ENDIF
        EXTERN  snd_va, snd_vb, snd_vc

IFDEF SN_SEGA                           ; (the Master System's: port 7Fh; its noise's
        defc    SN = $7F                ; periodic mode is 16 steps, the TI's 15)
        defc    PDIV = 16
ELSE
        defc    SN = $FF
        defc    PDIV = 15
ENDIF

; the AY's volume (3 dB a step) as the SN's attenuation (2 dB a step)
att:    defb    15, 15, 15, 15, 14, 12, 11, 9, 8, 6, 5, 3, 2, 0, 0, 0

; void sn_quiet(void): everything off, the shadows forgotten
sn_quiet:
        ld      a,$9F
        call    out
        ld      a,$BF
        call    out
        ld      a,$DF
        call    out
        ld      a,$FF
        call    out
        ld      hl,shadow
        ld      b,shadow_end-shadow
sq_l:   ld      (hl),$FF
        inc     hl
        djnz    sq_l
        ret

; out: A to the chip (a write every 9 us at most: its 32 clocks)
out:    out     (SN),a
        ex      (sp),hl
        ex      (sp),hl
        ret

; wants: voice HL wants the noise? NZ if so (a noise, or a tone past 1023)
wants:  ld      a,(hl)
        cp      2
        jr      z,wn_y
        cp      1
        jr      nz,wn_n
        inc     hl
        inc     hl
        ld      a,(hl)          ; the period's high byte: past 3 is past 1023
        dec     hl
        dec     hl
        cp      4
        jr      nc,wn_y
wn_n:   xor     a
        ret
wn_y:   or      1
        ret

sn_update:
        ; who has the noise: A, C, B
        xor     a
        ld      (owner),a
        ld      hl,snd_va
        call    wants
        jr      z,su_c
        ld      a,1
        jr      su_own
su_c:   ld      hl,snd_vc
        call    wants
        jr      z,su_b
        ld      a,3
        jr      su_own
su_b:   ld      hl,snd_vb
        call    wants
        jr      z,su_tones
        ld      a,2
su_own: ld      (owner),a
su_tones:
        ; C keeps tone 3 if it sounds a tone and isn't the noise's
        xor     a
        ld      (ctone),a
        ld      a,(snd_vc)
        cp      1
        jr      nz,su_nz
        ld      a,(owner)
        cp      3
        jr      z,su_nz
        ld      a,1
        ld      (ctone),a
su_nz:  ; the noise: its control, its volume, and tone 3 if it drives it
        ld      a,$FF
        ld      (want_t3),a     ; (FF: tone 3 not the noise's)
        ld      a,(owner)
        or      a
        jr      z,su_nzoff
        call    owner_voice     ; HL the owner's voice
        ld      a,(hl)
        cp      2
        jr      z,su_white
        ; a bass: periodic, tone 3 at period / 15 (unless C has tone 3: then
        ; the bass on its own tone, octaves up)
        ld      a,(ctone)
        or      a
        jr      nz,su_unown
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      a,(hl)          ; volume
        push    af
        ex      de,hl
        ld      de,(b_per)      ; (period / 15: worked out when the note changes)
        or      a
        sbc     hl,de
        add     hl,de
        jr      z,su_same
        ld      (b_per),hl
        call    div15
        ld      (b_div),hl
su_same:
        ld      hl,(b_div)
        ld      (t3_per),hl
        ld      a,0
        ld      (want_t3),a
        pop     af
        call    vatt
        ld      c,a
        ld      a,$E3           ; periodic, clocked by tone 3
        jr      su_nset
su_unown:
        xor     a
        ld      (owner),a
        jr      su_nzoff
su_white:
        inc     hl
        ld      e,(hl)          ; n: its period
        inc     hl
        inc     hl
        ld      a,(hl)
        call    vatt
        ld      c,a
        ld      a,(ctone)
        or      a
        jr      nz,su_fixed
        ld      d,0             ; clocked by tone 3, at the effect's own rate
        ex      de,hl
        ld      (t3_per),hl
        xor     a
        ld      (want_t3),a
        ld      a,$E7
        jr      su_nset
su_fixed:
        ld      a,e             ; the nearest fixed rate: n ~16, ~32, ~64
        cp      23
        ld      a,$E4
        jr      c,su_nset
        ld      a,e
        cp      46
        ld      a,$E5
        jr      c,su_nset
        ld      a,$E6
su_nset:
        ld      hl,s_nctl
        cp      (hl)
        jr      z,su_nv
        ld      (hl),a
        call    out
su_nv:  ld      a,c
        jr      su_nvol
su_nzoff:
        ld      a,15
su_nvol:
        ld      hl,s_natt
        cp      (hl)
        jr      z,su_t
        ld      (hl),a
        or      $F0
        call    out
su_t:   ; the tones
        ld      hl,snd_va
        ld      a,(owner)
        cp      1
        ld      b,0
        call    tone
        ld      hl,snd_vb
        ld      a,(owner)
        cp      2
        ld      b,1
        call    tone
        ld      a,(want_t3)     ; tone 3: the noise's clock, silent
        or      a
        jr      nz,su_c3
        ld      hl,(t3_per)
        ld      a,15
        ld      b,2
        jp      set_tone
su_c3:  ld      hl,snd_vc
        ld      a,(owner)
        cp      3
        ld      b,2
; tone: channel B sounds voice HL (Z: it's the noise's, so silent)
tone:   jr      z,tn_off
        ld      a,(hl)
        cp      1
        jr      nz,tn_off
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      a,(hl)
        call    vatt
        ex      de,hl
tn_oct: ld      c,a             ; (past 1023: octaves up)
        ld      a,h
        cp      4
        ld      a,c
        jr      c,set_tone
        srl     h
        rr      l
        jr      tn_oct
tn_off: ld      a,15
        ld      hl,0
; set_tone: channel B (0-2) at period HL (10 bits), attenuation A
set_tone:
        push    af
        ld      a,b
        add     a,a
        add     a,a             ; 3 bytes a channel in the shadow: period (2), attenuation
        ; (b * 3)
        sub     b
        ld      de,s_tone
        add     a,e
        ld      e,a
        jr      nc,st_1
        inc     d
st_1:   pop     af
        push    af
        ld      a,h             ; (silent: the period as it was)
        or      l
        jr      z,st_att
        ld      a,(de)
        cp      l
        jr      nz,st_per
        inc     de
        ld      a,(de)
        dec     de
        cp      h
        jr      z,st_att
st_per: ld      a,l
        ld      (de),a
        inc     de
        ld      a,h
        ld      (de),a
        dec     de
        ld      a,b             ; latch: 1 cc 0 low 4 bits
        rrca
        rrca
        rrca
        ld      c,a
        ld      a,l
        and     $0F
        or      c
        or      $80
        call    out
        ld      a,l             ; data: the high 6 bits
        srl     h
        rra
        srl     h
        rra
        srl     h
        rra
        srl     h
        rra
        and     $3F
        call    out
st_att: inc     de
        inc     de
        pop     af
        ex      de,hl
        cp      (hl)
        ret     z
        ld      (hl),a
        ld      c,a
        ld      a,b
        rrca
        rrca
        rrca
        or      c
        or      $90
        jp      out

; vatt: A = the attenuation of AY volume A
vatt:   and     15
        push    hl
        ld      hl,att
        add     a,l
        ld      l,a
        jr      nc,va_1
        inc     h
va_1:   ld      a,(hl)
        pop     hl
        ret

; owner_voice: HL = the noise's owner's voice (1 A, 2 B, 3 C)
owner_voice:
        ld      hl,snd_va
        cp      1
        ret     z
        ld      hl,snd_vb
        cp      2
        ret     z
        ld      hl,snd_vc
        ret

; div15: HL = HL / 15 (at least 1)
div15:  ld      de,0
        ld      bc,PDIV
d15:    or      a
        sbc     hl,bc
        jr      c,d15x
        inc     de
        jr      d15
d15x:   ex      de,hl
        ld      a,h
        or      l
        ret     nz
        inc     l
        ret

        SECTION bss_user
owner:  defs    1
ctone:  defs    1
want_t3: defs   1
t3_per: defs    2
b_per:  defs    2
b_div:  defs    2
shadow:
s_tone: defs    9               ; per channel: period (2), attenuation
s_nctl: defs    1
s_natt: defs    1
shadow_end:
