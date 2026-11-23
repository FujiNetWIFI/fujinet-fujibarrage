; pt.asm -- sparks: pixels flying from a tank going up, or off its shield
; (the Z80's src/z80/pt.asm in 8086; the CoCo's, from the Apple's). Each is
; a fire pixel on the sky, moved every frame (x 16.8, y 8.8 fixed point,
; little-endian) and put back to sky when it moves on or dies. It only ever
; draws on sky, so it changes nothing the server knows (refview never sees
; them).
;
; C: field_sparks(uint16_t a)   low byte (AL) the seat, high (AH) the kind:
;                               0-7 a death's effect (DEATH_*), 8 + a
;                               shield's event (SH_*)
; pt_step   a frame (ply.asm's anim): AL nonzero while any burn (they glow
;           in the fire colours)
; pt_clear  all gone (a segment's end, a shot's start)
;
; Small model, near; DS = SS = DGROUP. The sparks are walked with SI, the
; kind's parameters with DI (the Z80's IX and IY); the engine keeps both.
;
        .8086

NP      EQU     16
PX_SKY  EQU     1
PX_FX   EQU     3

; a spark
SP_X    EQU     0                       ; 3: x 16.8 (fraction, low, high)
SP_Y    EQU     3                       ; 2: y 8.8 (fraction, line)
SP_VX   EQU     5                       ; 2: 8.8, signed
SP_VY   EQU     7                       ; 2
SP_LIFE EQU     9
SP_GRAV EQU     10
SP_SH   EQU     11                      ; it shows, at
SP_OX   EQU     12                      ; 2
SP_OY   EQU     14
SP_SIZE EQU     15

; a kind's parameters (par, 9 bytes each)
P_N     EQU     0                       ; how many
P_FIRST EQU     1                       ; the first direction (0 right, 4 down, 12 up)
P_STEP  EQU     2                       ; 1: one direction after another
P_MASK  EQU     3                       ; a pt_random spread added
P_K     EQU     4                       ; frames moved before it shows (at 2 px a frame)
P_SH    EQU     5                       ; then its speed halved this often
P_SJ    EQU     6                       ; and once more at pt_random (1) or not (0)
P_G     EQU     7                       ; gravity, 1/256 px a frame per frame
P_LIFE  EQU     8                       ; frames (plus 0-7 at pt_random)

RAYS    EQU     0*9
SPRAY   EQU     1*9
PLUME   EQU     2*9
SCATTER EQU     3*9
DEBRIS  EQU     4*9
RING    EQU     5*9
BURST   EQU     6*9

DGROUP  GROUP   _DATA, _BSS

_DATA   SEGMENT WORD PUBLIC 'DATA'
pt_cos  DB      32, 30, 23, 12, 0, -12, -23, -30
        DB      -32, -30, -23, -12, 0, 12, 23, 30
pt_kmap DB      PLUME, DEBRIS, DEBRIS, DEBRIS, RAYS, SPRAY, PLUME, SCATTER
        DB      RING, BURST, RING, RING, RING   ; hit, break, deflect, up, lift
;               n   1st step mask k  sh sj g   life
pt_par  DB      16, 0, 1, 0, 2, 1, 0, 0, 14     ; rays
        DB      12, 9, 0, 7, 1, 0, 1, 20, 30    ; spray, up and over
        DB      10, 11, 0, 2, 0, 3, 1, 0, 34    ; plume, rising slowly
        DB      14, 0, 0, 15, 1, 0, 1, 16, 30   ; scatter, all ways
        DB      8, 9, 0, 7, 1, 1, 1, 20, 24     ; debris
        DB      16, 0, 1, 0, 3, 4, 0, 0, 8      ; ring: a dome round it
        DB      16, 0, 1, 0, 3, 0, 0, 0, 8      ; burst
_DATA   ENDS

_BSS    SEGMENT WORD PUBLIC 'BSS'
v_pcx   DW      ?
v_pcy   DB      ?
v_pleft DB      ?
v_pkp   DB      ?
v_pd    DB      ?
v_pi    DB      ?
v_pseed DB      ?
v_plive DB      ?
v_pcnt  DB      ?
v_pkind DB      ?
sparks_ DB      NP * SP_SIZE DUP (?)
_BSS    ENDS

        EXTRN   _tk:BYTE
        EXTRN   fld_get_:NEAR, fld_plot_:NEAR, fld_swap_:NEAR
        EXTRN   _fld_w:WORD, _fld_h:BYTE, _fld_mid:BYTE
        EXTRN   _fx:WORD, _fy:WORD, _fv:BYTE, _fold:BYTE

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP, SS:DGROUP

        PUBLIC  field_sparks_, pt_step, pt_clear

; void field_sparks(uint16_t a)
field_sparks_:
        push    bx
        push    cx
        push    dx
        push    si
        push    di
        push    bp
        push    es
        cld
        mov     bl, al                  ; the seat
        mov     al, ah                  ; the kind
        call    pt_sparks
        pop     es
        pop     bp
        pop     di
        pop     si
        pop     dx
        pop     cx
        pop     bx
        ret

; pt_kpar: DI = the kind's parameters
pt_kpar:   mov     al, v_pkp
        xor     ah, ah
        add     ax, OFFSET DGROUP:pt_par
        mov     di, ax
        ret

; sparks: kind AL from seat BL's tank
pt_sparks:      mov     v_pkind, al
        cmp     al, 13
        jb      q_ok
        ret
q_ok:  mov     al, bl                  ; the tank: tk + 5 seat
        add     al, al
        add     al, al
        add     al, bl
        xor     ah, ah
        mov     bx, ax
        mov     ax, WORD PTR _tk[bx + 1]        ; x
        mov     v_pcx, ax
        mov     al, _tk[bx + 3]         ; y, its bottom: its middle is up from there
        sub     al, _fld_mid
        mov     v_pcy, al
        mov     al, v_pkind
        xor     ah, ah
        mov     bx, ax
        mov     al, pt_kmap[bx]
        mov     v_pkp, al
        call    pt_kpar
        mov     al, [di + P_N]
        mov     v_pleft, al
        mov     v_pi, 0
q_one: mov     si, OFFSET DGROUP:sparks_ + (NP - 1) * SP_SIZE  ; a free one
        mov     cx, NP
q_f:   cmp     BYTE PTR [si + SP_LIFE], 0
        je      q_got
        sub     si, SP_SIZE
        loop    q_f
        ret
q_got: call    pt_kpar
        call    pt_random                  ; its direction
        and     al, [di + P_MASK]
        add     al, [di + P_FIRST]
        mov     bl, al
        mov     al, [di + P_STEP]
        or      al, al
        mov     al, bl
        jz      q_dir
        add     al, v_pi
q_dir: and     al, 15
        mov     v_pd, al
        call    cos_a
        call    v16
        mov     [si + SP_VX], ax
        mov     al, v_pd
        add     al, 12
        and     al, 15
        call    cos_a
        call    v16
        mov     [si + SP_VY], ax
        mov     ax, v_pcx               ; from the middle (and a half)
        mov     BYTE PTR [si + SP_X], 80h
        mov     [si + SP_X + 1], ax
        mov     al, v_pcy
        mov     BYTE PTR [si + SP_Y], 80h
        mov     [si + SP_Y + 1], al
        call    pt_kpar
        mov     al, [di + P_K]          ; out from the middle
        or      al, al
        jz      q_sp
        mov     v_pd, al
q_k:   call    pt_move
        dec     v_pd
        jnz     q_k
q_sp:  call    pt_kpar                    ; then slower
        call    pt_random
        and     al, [di + P_SJ]
        add     al, [di + P_SH]
        jz      q_g
        mov     v_pd, al
q_sh:  sar     WORD PTR [si + SP_VX], 1
        sar     WORD PTR [si + SP_VY], 1
        dec     v_pd
        jnz     q_sh
q_g:   call    pt_kpar
        mov     al, [di + P_G]
        mov     [si + SP_GRAV], al
        call    pt_random
        and     al, 7
        add     al, [di + P_LIFE]
        mov     [si + SP_LIFE], al
        mov     BYTE PTR [si + SP_SH], 0
        inc     v_pi
        dec     v_pleft
        jz      q_ret
        jmp     q_one
q_ret: ret

; cos_a: AL = cos[AL]
cos_a:  xor     ah, ah
        mov     bx, ax
        mov     al, pt_cos[bx]
        ret

; v16: AL (1/16 px, signed) as AX (1/256 px)
v16:    cbw
        mov     cl, 4
        shl     ax, cl
        ret

pt_step:
        mov     v_plive, 0
        mov     si, OFFSET DGROUP:sparks_ + (NP - 1) * SP_SIZE
        mov     v_pcnt, NP
st_p:   mov     al, [si + SP_LIFE]
        or      al, al
        jz      st_n
        mov     v_plive, al
        call    pt_erase
        dec     BYTE PTR [si + SP_LIFE]
        jz      st_n
        mov     al, [si + SP_GRAV]      ; falling
        add     [si + SP_VY], al
        adc     BYTE PTR [si + SP_VY + 1], 0
        call    pt_move
        mov     al, [si + SP_Y + 1]
        cmp     al, _fld_h
        jae     st_die
        mov     ax, [si + SP_X + 1]
        cmp     ax, _fld_w
        jae     st_die
        mov     _fx, ax                 ; on sky: fire
        mov     al, [si + SP_Y + 1]
        xor     ah, ah
        mov     _fy, ax
        call    fld_get_
        cmp     al, PX_SKY
        jne     st_n
        mov     _fv, PX_FX
        call    fld_plot_
        mov     ax, [si + SP_X + 1]     ; (the Z80 reads _fx back)
        mov     [si + SP_OX], ax
        mov     al, [si + SP_Y + 1]
        mov     [si + SP_OY], al
        mov     BYTE PTR [si + SP_SH], 1
        jmp     st_n
st_die: mov     BYTE PTR [si + SP_LIFE], 0
st_n:   sub     si, SP_SIZE
        dec     v_pcnt
        jz      st_ret
        jmp     st_p
st_ret: mov     al, v_plive
        ret

; pt_move: spark SI a frame on (x += vx, sign-extended)
pt_move: mov     ax, [si + SP_VX]
        cwd
        add     [si + SP_X], al
        adc     [si + SP_X + 1], ah
        adc     [si + SP_X + 2], dl
        mov     al, [si + SP_VY]
        add     [si + SP_Y], al
        mov     al, [si + SP_VY + 1]
        adc     [si + SP_Y + 1], al
        ret

; pt_random: AL = the next of an 8-bit LFSR (never 0)
pt_random: mov     al, v_pseed
        or      al, al
        jnz     rn_go
        mov     al, 0A5h
rn_go:  add     al, al
        jnc     rn_n
        xor     al, 1Dh
rn_n:   mov     v_pseed, al
        ret

; pt_erase: spark SI's pixel back to sky, if it's still fire
pt_erase:  cmp     BYTE PTR [si + SP_SH], 0
        je      er_ret
        mov     BYTE PTR [si + SP_SH], 0
        mov     ax, [si + SP_OX]
        mov     _fx, ax
        mov     al, [si + SP_OY]
        xor     ah, ah
        mov     _fy, ax
        mov     _fold, PX_FX
        mov     _fv, PX_SKY
        jmp     fld_swap_
er_ret: ret

pt_clear:
        mov     si, OFFSET DGROUP:sparks_ + (NP - 1) * SP_SIZE
        mov     v_pcnt, NP
pc_c:   cmp     BYTE PTR [si + SP_LIFE], 0
        je      pc_n
        call    pt_erase
        mov     BYTE PTR [si + SP_LIFE], 0
pc_n:   sub     si, SP_SIZE
        dec     v_pcnt
        jnz     pc_c
        ret

_TEXT   ENDS
        END
