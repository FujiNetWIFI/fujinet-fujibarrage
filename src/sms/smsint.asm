; smsint.asm -- the Master System's frame: the VDP's interrupt (IM 1: z88dk's
; crt0 reads the status and calls the handlers in raster_procs; sms_frame is
; the first). From FujiBlast's src/sms/smsint.asm.
;
; A frame: vbi_ticks +1 (the scripts' clock: display frames), frame_count
; +1 (60ths of a second: on a 50 Hz console every fifth frame counts twice),
; then the VDP's frame work (vdpf, field.asm: the sprites, the colours, the
; shake) and the sound's (snd_nmi, src/sound/snd.asm; twice on a fifth 50th).
;
; The VDP may be had here: the main code sets an address and streams its
; bytes with the interrupt off (vdp.asm), each burst setting its own, so
; whatever this does to the address between bursts spoils nothing.
;
; The crt0 keeps AF, HL, BC and DE; this keeps IX, IY and the other set.

        SECTION code_user

        PUBLIC  _sms_int_init, _sms_pal_probe
        PUBLIC  _frame_count, _vbi_ticks, _vdp_seen, _sms_pal
        EXTERN  raster_procs, __tms9918_status_register
        EXTERN  _snd_nmi, vdpf, mus_frame

_sms_int_init:
        di
        ld      hl,sms_frame
        ld      (raster_procs),hl
        ld      a,5
        ld      (hz50_n),a
        ei
        ret

; 1 (in L) on a 50 Hz console: a frame there is 71364 Z80 cycles, on a
; 60 Hz one 59736, so the passes of a 25-cycle loop between two frames
; are about 2850 or 2390.
_sms_pal_probe:
        ld      hl,_vbi_ticks
        ld      a,(hl)
probe_w:
        cp      (hl)            ; a frame begins
        jr      z,probe_w
        ld      a,(hl)
        ld      bc,0
probe_n:
        inc     bc              ; 6
        cp      (hl)            ; 7
        jr      z,probe_n       ; 12
        ld      hl,2620
        or      a
        sbc     hl,bc
        ld      l,0
        ret     nc
        inc     l
        ret

sms_frame:
        ld      a,(__tms9918_status_register)
        ld      hl,_vdp_seen
        or      (hl)
        ld      (hl),a
        ld      hl,_vbi_ticks
        inc     (hl)
        push    ix
        push    iy
        ex      af,af'
        push    af
        exx
        push    bc
        push    de
        push    hl
        call    vdpf            ; (the VDP first: the blank is short)
        ld      hl,(_frame_count)
        inc     hl
        ld      a,(_sms_pal)
        or      a
        jr      z,frame_n
        ld      a,(hz50_n)
        dec     a
        jr      nz,frame_5
        inc     hl              ; the fifth 50th: two 60ths
        ld      (_frame_count),hl
        call    _snd_nmi
        call    mus_frame
        ld      hl,(_frame_count)
        ld      a,5
frame_5:
        ld      (hz50_n),a
frame_n:
        ld      (_frame_count),hl
        call    _snd_nmi
        call    mus_frame
        pop     hl
        pop     de
        pop     bc
        exx
        pop     af
        ex      af,af'
        pop     iy
        pop     ix
        ret

        SECTION bss_user
_frame_count:   defs    2
_vbi_ticks:     defs    1
_vdp_seen:      defs    1
_sms_pal:       defs    1
hz50_n:         defs    1
