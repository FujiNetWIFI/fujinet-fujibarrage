; trk.asm -- the script's tracks (OP_ARC): the Z80's src/z80/trk.asm in 8086
; (the CoCo's, one for one). Sixteen slots, each a moving head: position and
; velocity 12.12 fixed point (24 bits), acceleration 4.12 (16). Each step
;
;       X += VX;  Y += VY;  VX += AX;  VY += AY
;
; and the head is drawn at (X >> 12, Y >> 12) -- the server's fitter runs
; the very same integers. A head is drawn (fire) only over sky (a smoke
; trail's over fire too), and the pixel under it is put back when it moves;
; a smoke trail's stays. The values are little-endian, as the script's.
;
; trk_load (AX: an OP_ARC's arguments), trk_step, trk_stop (AL: the slot),
; trk_reset: the player's. C: trk_stop(slot), trk_fall(), and the three
; others through entries that keep what Watcom keeps (trk_reset_ ...).
;
; Small model, near; DS = SS = DGROUP. The records are walked with SI.
;
        .8086

        INCLUDE proto.inc

NTRK    EQU     16
PX_SKY  EQU     1
PX_FX   EQU     3

; a track
T_ON    EQU     0
T_STY   EQU     1
T_X     EQU     2                       ; 3
T_Y     EQU     5                       ; 3
T_VX    EQU     8                       ; 3
T_VY    EQU     11                      ; 3
T_AX    EQU     14                      ; 2
T_AY    EQU     16                      ; 2
T_N     EQU     18                      ; 2: steps left
T_PX    EQU     20                      ; 2: where its head is
T_PY    EQU     22                      ; 2
T_UND   EQU     24                      ; what was under it
T_DRAWN EQU     25
T_SIZE  EQU     26

DGROUP  GROUP   _DATA, _BSS

_DATA   SEGMENT WORD PUBLIC 'DATA'
_DATA   ENDS

_BSS    SEGMENT WORD PUBLIC 'BSS'
tfall   DB      ?
tslot   DB      ?
tracks  DB      NTRK * T_SIZE DUP (?)
_BSS    ENDS

        EXTRN   fld_plot_:NEAR, fld_get_:NEAR
        EXTRN   _fx:WORD, _fy:WORD, _fv:BYTE

; centry: a C entry: everything Watcom keeps, kept around the routine
CENTRY  MACRO   target
        push    bx
        push    cx
        push    dx
        push    si
        push    di
        push    bp
        push    es
        cld
        call    target
        pop     es
        pop     bp
        pop     di
        pop     si
        pop     dx
        pop     cx
        pop     bx
        ret
        ENDM

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP, SS:DGROUP

        PUBLIC  trk_load, trk_step, trk_stop, trk_reset
        PUBLIC  trk_load_, trk_step_, trk_stop_, trk_reset_
        PUBLIC  trk_fall_

; void trk_stop(uint8_t s): the slot in AL
trk_stop_:
        CENTRY  trk_stop
; the others, for C (AX: trk_load's pointer)
trk_load_:
        CENTRY  trk_load
trk_step_:
        CENTRY  trk_step
trk_reset_:
        CENTRY  trk_reset

; uint8_t trk_fall(void): the lowest falling shell's line | 1, or 0
trk_fall_:
        mov     al, tfall
        xor     ah, ah
        ret

; slot: SI = the track in slot AL (& 15): tracks + 26 AL
trk_slot:
        and     al, NTRK - 1
        mov     bl, T_SIZE
        mul     bl
        add     ax, OFFSET DGROUP:tracks
        mov     si, ax
        ret

trk_reset:
        push    ds
        pop     es
        mov     di, OFFSET DGROUP:tracks
        mov     cx, NTRK * T_SIZE
        xor     al, al
        cld
        rep     stosb
        mov     tfall, 0
        ret

; trk_stop: the track in slot AL ends now
trk_stop:
        call    trk_slot
        call    toff
        mov     BYTE PTR [si + T_ON], 0
        ret

; trk_load: an OP_ARC's arguments at AX (slot, style, x16, y16, vx24, vy24,
; ax16, ay16, n16: little-endian) start a track
trk_load:
        mov     di, ax
        mov     al, [di]
        call    trk_slot
        call    toff                    ; its old head away
        inc     di
        mov     al, [di]
        mov     [si + T_STY], al
        inc     di
        mov     BYTE PTR [si + T_X], 0  ; X = x16 << 8
        mov     ax, [di]
        mov     [si + T_X + 1], ax
        add     di, 2
        mov     BYTE PTR [si + T_Y], 0
        mov     ax, [di]
        mov     [si + T_Y + 1], ax
        add     di, 2
        mov     bx, si                  ; vx, vy, ax, ay, n: as they come
        mov     si, di
        lea     di, [bx + T_VX]
        push    ds
        pop     es
        mov     cx, 6
        cld
        rep     movsw                   ; (12 bytes)
        mov     si, bx
        mov     BYTE PTR [si + T_ON], 1
        jmp     ton

; trk_step: every live track one step
trk_step:
        mov     tfall, 0
        mov     si, OFFSET DGROUP:tracks + (NTRK - 1) * T_SIZE
        mov     tslot, NTRK
ts_t:   cmp     BYTE PTR [si + T_ON], 0
        jne     ts_live
        jmp     ts_n
ts_live:
        call    toff
        mov     ax, [si + T_N]          ; no steps left: it ends
        or      ax, ax
        jnz     ts_go
        mov     BYTE PTR [si + T_ON], 0
        jmp     ts_n
ts_go:  dec     ax
        mov     [si + T_N], ax
        mov     ax, [si + T_VX]         ; X += VX
        add     [si + T_X], ax
        mov     al, [si + T_VX + 2]
        adc     [si + T_X + 2], al
        mov     ax, [si + T_VY]         ; Y += VY
        add     [si + T_Y], ax
        mov     al, [si + T_VY + 2]
        adc     [si + T_Y + 2], al
        mov     ax, [si + T_AX]         ; VX += AX, sign-extended
        cwd
        add     [si + T_VX], ax
        adc     [si + T_VX + 2], dl
        mov     ax, [si + T_AY]         ; VY += AY
        cwd
        add     [si + T_VY], ax
        adc     [si + T_VY + 2], dl
        call    ton
        test    BYTE PTR [si + T_VY + 2], 80h   ; falling (VY >= 0), on the
        jnz     ts_n                    ; field, not a digger: the lowest
        cmp     BYTE PTR [si + T_PY + 1], 0     ; is the whistle's
        jne     ts_n
        mov     al, [si + T_STY]
        and     al, 15
        cmp     al, ST_DIGGER
        je      ts_n
        mov     al, [si + T_PY]
        cmp     al, tfall
        jb      ts_n
        or      al, 1
        mov     tfall, al
ts_n:   sub     si, T_SIZE
        dec     tslot
        jz      ts_ret
        jmp     ts_t
ts_ret: ret

; off: the head of track SI off (its pixel back, unless a trail or overdrawn)
toff:   cmp     BYTE PTR [si + T_DRAWN], 0
        je      tf_ret
        mov     BYTE PTR [si + T_DRAWN], 0
        mov     al, [si + T_STY]
        and     al, 15
        cmp     al, ST_SMOKE
        je      tf_ret
        mov     ax, [si + T_PX]
        mov     _fx, ax
        mov     ax, [si + T_PY]
        mov     _fy, ax
        call    fld_get_
        cmp     al, PX_FX
        jne     tf_ret
        mov     al, [si + T_UND]
        mov     _fv, al
        jmp     fld_plot_
tf_ret: ret

; on: the head of track SI on, at (X >> 12, Y >> 12)
ton:    mov     ax, [si + T_X + 1]      ; (the top 16 bits) >> 4, arithmetic
        mov     cl, 4
        sar     ax, cl
        mov     [si + T_PX], ax
        mov     _fx, ax
        mov     ax, [si + T_Y + 1]
        sar     ax, cl
        mov     [si + T_PY], ax
        mov     _fy, ax
        call    fld_get_
        cmp     al, PX_SKY
        je      on_draw
        cmp     al, PX_FX               ; a trail's over fire too (refview's
        jne     on_ret                  ; drawHead: never over dirt or a tank)
        mov     bl, al
        mov     al, [si + T_STY]
        and     al, 15
        cmp     al, ST_SMOKE
        jne     on_ret
        mov     al, bl
on_draw:
        mov     [si + T_UND], al
        mov     BYTE PTR [si + T_DRAWN], 1
        mov     _fv, PX_FX
        jmp     fld_plot_
on_ret: ret

_TEXT   ENDS
        END
