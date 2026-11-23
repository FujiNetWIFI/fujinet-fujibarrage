; trk.asm -- the script's tracks (OP_ARC): the CoCo's src/6809/trk.asm in
; Z80. Sixteen slots, each a moving head: position and velocity 12.12
; fixed point (24 bits), acceleration 4.12 (16). Each step
;
;       X += VX;  Y += VY;  VX += AX;  VY += AY
;
; and the head is drawn at (X >> 12, Y >> 12) -- the server's fitter runs
; the very same integers. A head is drawn (fire) only over sky (a smoke
; trail's over fire too), and the pixel under it is put back when it moves;
; a smoke trail's stays. The values are little-endian, as the script's.
;
; trk_load (HL: an OP_ARC's arguments), trk_step, trk_stop (A: the slot),
; trk_reset: the player's. C: trk_stop(slot), trk_fall().

        SECTION code_user

        INCLUDE "proto.inc"

        PUBLIC  trk_load, trk_step, trk_stop, trk_reset
        PUBLIC  _trk_stop, _trk_fall
        EXTERN  _fld_plot, _fld_get, _fx, _fy, _fv

        defc    N = 16
        defc    PX_SKY = 1
        defc    PX_FX = 3

; a track
        defc    T_ON = 0
        defc    T_STY = 1
        defc    T_X = 2                 ; 3
        defc    T_Y = 5                 ; 3
        defc    T_VX = 8                ; 3
        defc    T_VY = 11               ; 3
        defc    T_AX = 14               ; 2
        defc    T_AY = 16               ; 2
        defc    T_N = 18                ; 2: steps left
        defc    T_PX = 20               ; 2: where its head is
        defc    T_PY = 22               ; 2
        defc    T_UND = 24              ; what was under it
        defc    T_DRAWN = 25
        defc    T_SIZE = 26

; void trk_stop(uint8_t s)
_trk_stop:
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
        push    ix
        push    iy
        call    trk_stop
        pop     iy
        pop     ix
        ret

; uint8_t trk_fall(void): the lowest falling shell's line | 1, or 0
_trk_fall:
        ld      a,(tfall)
        ld      l,a
        ret

; slot: IX = the track in slot A (& 15): tracks + 26 A
slot:   and     N-1
        ld      l,a
        ld      h,0
        add     hl,hl           ; 2a
        ld      e,l
        ld      d,h
        add     hl,hl
        add     hl,hl           ; 8a
        push    hl
        add     hl,hl           ; 16a
        add     hl,de           ; 18a
        pop     de
        add     hl,de           ; 26a
        ld      de,tracks
        add     hl,de
        push    hl
        pop     ix
        ret

trk_reset:
        ld      hl,tracks
        ld      de,tracks+1
        ld      bc,N * T_SIZE - 1
        ld      (hl),0
        ldir
        xor     a
        ld      (tfall),a
        ret

; trk_stop: the track in slot A ends now
trk_stop:
        call    slot
        call    off
        ld      (ix+T_ON),0
        ret

; trk_load: an OP_ARC's arguments at HL (slot, style, x16, y16, vx24, vy24,
; ax16, ay16, n16: little-endian) start a track
trk_load:
        ld      a,(hl)
        push    hl
        call    slot
        call    off             ; its old head away
        pop     hl
        inc     hl
        ld      a,(hl)
        ld      (ix+T_STY),a
        inc     hl
        ld      (ix+T_X),0      ; X = x16 << 8
        ld      a,(hl)
        ld      (ix+T_X+1),a
        inc     hl
        ld      a,(hl)
        ld      (ix+T_X+2),a
        inc     hl
        ld      (ix+T_Y),0
        ld      a,(hl)
        ld      (ix+T_Y+1),a
        inc     hl
        ld      a,(hl)
        ld      (ix+T_Y+2),a
        inc     hl
        push    ix              ; vx, vy, ax, ay, n: as they come
        pop     de
        ld      a,e
        add     a,T_VX
        ld      e,a
        jr      nc,tl_1
        inc     d
tl_1:   ld      bc,12
        ldir
        ld      (ix+T_ON),1
        jp      on

; trk_step: every live track one step
trk_step:
        xor     a
        ld      (tfall),a
        ld      ix,tracks+(N-1)*T_SIZE
        ld      a,N
        ld      (tslot),a
ts_t:   ld      a,(ix+T_ON)
        or      a
        jp      z,ts_n
        call    off
        ld      l,(ix+T_N)      ; no steps left: it ends
        ld      h,(ix+T_N+1)
        ld      a,h
        or      l
        jr      nz,ts_go
        ld      (ix+T_ON),0
        jp      ts_n
ts_go:  dec     hl
        ld      (ix+T_N),l
        ld      (ix+T_N+1),h
        ld      a,(ix+T_X)      ; X += VX
        add     a,(ix+T_VX)
        ld      (ix+T_X),a
        ld      a,(ix+T_X+1)
        adc     a,(ix+T_VX+1)
        ld      (ix+T_X+1),a
        ld      a,(ix+T_X+2)
        adc     a,(ix+T_VX+2)
        ld      (ix+T_X+2),a
        ld      a,(ix+T_Y)      ; Y += VY
        add     a,(ix+T_VY)
        ld      (ix+T_Y),a
        ld      a,(ix+T_Y+1)
        adc     a,(ix+T_VY+1)
        ld      (ix+T_Y+1),a
        ld      a,(ix+T_Y+2)
        adc     a,(ix+T_VY+2)
        ld      (ix+T_Y+2),a
        ld      a,(ix+T_AX+1)   ; VX += AX, sign-extended
        rla
        sbc     a,a
        ld      c,a
        ld      a,(ix+T_VX)
        add     a,(ix+T_AX)
        ld      (ix+T_VX),a
        ld      a,(ix+T_VX+1)
        adc     a,(ix+T_AX+1)
        ld      (ix+T_VX+1),a
        ld      a,(ix+T_VX+2)
        adc     a,c
        ld      (ix+T_VX+2),a
        ld      a,(ix+T_AY+1)   ; VY += AY
        rla
        sbc     a,a
        ld      c,a
        ld      a,(ix+T_VY)
        add     a,(ix+T_AY)
        ld      (ix+T_VY),a
        ld      a,(ix+T_VY+1)
        adc     a,(ix+T_AY+1)
        ld      (ix+T_VY+1),a
        ld      a,(ix+T_VY+2)
        adc     a,c
        ld      (ix+T_VY+2),a
        call    on
        bit     7,(ix+T_VY+2)   ; falling (VY >= 0), on the field, not a digger:
        jr      nz,ts_n         ; the lowest is the whistle's
        ld      a,(ix+T_PY+1)
        or      a
        jr      nz,ts_n
        ld      a,(ix+T_STY)
        and     15
        cp      ST_DIGGER
        jr      z,ts_n
        ld      a,(tfall)
        ld      b,a
        ld      a,(ix+T_PY)
        cp      b
        jr      c,ts_n
        or      1
        ld      (tfall),a
ts_n:   ld      de,-T_SIZE
        add     ix,de
        ld      hl,tslot
        dec     (hl)
        jp      nz,ts_t
        ret

; off: the head of track IX off (its pixel back, unless a trail or overdrawn)
off:    ld      a,(ix+T_DRAWN)
        or      a
        ret     z
        ld      (ix+T_DRAWN),0
        ld      a,(ix+T_STY)
        and     15
        cp      ST_SMOKE
        ret     z
        ld      l,(ix+T_PX)
        ld      h,(ix+T_PX+1)
        ld      (_fx),hl
        ld      l,(ix+T_PY)
        ld      h,(ix+T_PY+1)
        ld      (_fy),hl
        call    _fld_get
        cp      PX_FX
        ret     nz
        ld      a,(ix+T_UND)
        ld      (_fv),a
        jp      _fld_plot

; on: the head of track IX on, at (X >> 12, Y >> 12)
on:     ld      l,(ix+T_X+1)    ; (the top 16 bits) >> 4, arithmetic
        ld      h,(ix+T_X+2)
        sra     h
        rr      l
        sra     h
        rr      l
        sra     h
        rr      l
        sra     h
        rr      l
        ld      (ix+T_PX),l
        ld      (ix+T_PX+1),h
        ld      (_fx),hl
        ld      l,(ix+T_Y+1)
        ld      h,(ix+T_Y+2)
        sra     h
        rr      l
        sra     h
        rr      l
        sra     h
        rr      l
        sra     h
        rr      l
        ld      (ix+T_PY),l
        ld      (ix+T_PY+1),h
        ld      (_fy),hl
        call    _fld_get
        cp      PX_SKY
        jr      z,on_draw
        cp      PX_FX           ; a trail's over fire too (refview's drawHead:
        ret     nz              ;  never over dirt or a tank)
        ld      b,a
        ld      a,(ix+T_STY)
        and     15
        cp      ST_SMOKE
        ret     nz
        ld      a,b
on_draw:
        ld      (ix+T_UND),a
        ld      (ix+T_DRAWN),1
        ld      a,PX_FX
        ld      (_fv),a
        jp      _fld_plot

        SECTION bss_user
tfall:  defs    1
tslot:  defs    1
tracks: defs    N * T_SIZE
