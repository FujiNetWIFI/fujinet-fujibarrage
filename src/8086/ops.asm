; ops.asm -- the script's paths (OP_PATH), flows (OP_FLOW) and beams
; (OP_BEAM) on the field (docs/PROTOCOL.md): the Z80's src/z80/ops.asm in
; 8086 (the CoCo's, 16-bit coordinates). Paths and flows read their bytes
; where they lie in script[]; at a segment's end they finish (ops_seg_end).
;
; ops_path, ops_flow, ops_beam   AX: the op's arguments; AX: the bytes taken
; ops_step                       a frame: AL non-zero if a fire flow burns
; ops_seg_end                    paths stop, flows run out
; ops_reset                      all off (a new shot)
; C: ops_beams_off()             beams gone (the script's end)
;
; Small model, near; DS = SS = DGROUP. The records are walked with SI (the
; Z80's IX), which the field engine keeps.
;
        .8086

        INCLUDE proto.inc

NPATH   EQU     4
NFLOW   EQU     4
NBEAM   EQU     2
PX_SKY  EQU     1
PX_DIRT EQU     2
PX_FX   EQU     3

; a path
PA_ON   EQU     0
PA_STY  EQU     1
PA_X    EQU     2                       ; 2
PA_Y    EQU     4                       ; 2
PA_N    EQU     6                       ; steps left
PA_DR   EQU     7                       ; its head is drawn
PA_P    EQU     8                       ; 2: its next step
PA_SIZE EQU     10
; a flow
F_ON    EQU     0
F_KIND  EQU     1
F_FRM   EQU     2                       ; frames to spread over
F_F     EQU     3                       ; frames so far
F_HOLD  EQU     4
F_N     EQU     5
F_SH    EQU     6                       ; cells shown
F_ACC   EQU     7                       ; 2: n x frames so far, less frames x shown
F_P     EQU     9                       ; 2: its cells (x16, y: 3 bytes each)
F_SIZE  EQU     11
; a beam
BE_LEFT EQU     0
BE_X0   EQU     1                       ; 2
BE_Y0   EQU     3
BE_X1   EQU     5
BE_Y1   EQU     7
BE_SIZE EQU     9

DGROUP  GROUP   _DATA, _BSS

_DATA   SEGMENT WORD PUBLIC 'DATA'
_DATA   ENDS

_BSS    SEGMENT WORD PUBLIC 'BSS'
optr    DW      ?
oi      DB      ?
ocnt    DB      ?
oto     DB      ?
omud    DB      ?
ofire   DB      ?
lx0     DW      ?
ly0     DW      ?
lx1     DW      ?
ly1     DW      ?
ldx     DW      ?
ldy     DW      ?
lsx     DW      ?
lsy     DW      ?
lerr    DW      ?
le2     DW      ?
lon     DB      ?
ops_data LABEL  BYTE
paths   DB      NPATH * PA_SIZE DUP (?)
flows   DB      NFLOW * F_SIZE DUP (?)
beams   DB      NBEAM * BE_SIZE DUP (?)
ops_data_end LABEL BYTE
_BSS    ENDS

        EXTRN   fld_swap_:NEAR, fld_get_:NEAR, fld_plot_:NEAR
        EXTRN   _fx:WORD, _fy:WORD, _fv:BYTE, _fold:BYTE

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP, SS:DGROUP

        PUBLIC  ops_path, ops_flow, ops_beam, ops_step, ops_seg_end, ops_reset
        PUBLIC  ops_beams_off_

; void ops_beams_off(void)
ops_beams_off_:
        push    bx
        push    cx
        push    dx
        push    si
        push    di
        push    bp
        push    es
        cld
        call    ops_beams_off
        pop     es
        pop     bp
        pop     di
        pop     si
        pop     dx
        pop     cx
        pop     bx
        ret

; xyat: fx, fy = the words at SI + AL (x) and SI + AL + 2 (y)
xyat:   mov     bl, al
        xor     bh, bh
        add     bx, si
        mov     ax, [bx]
        mov     _fx, ax
        mov     ax, [bx + 2]
        mov     _fy, ax
        ret

; ---- paths ----------------------------------------------------------------------

; phead: path SI's head, drawn: a roller over air, a digger over air or dirt
phead:  mov     BYTE PTR [si + PA_DR], 0
        mov     al, PA_X
        call    xyat
        call    fld_get_
        cmp     al, PX_SKY
        je      ph_draw
        cmp     al, PX_DIRT
        jne     ph_ret
        cmp     BYTE PTR [si + PA_STY], ST_DIGGER
        jne     ph_ret
ph_draw:
        mov     _fv, PX_FX
        call    fld_plot_
        inc     BYTE PTR [si + PA_DR]
ph_ret: ret

; pleave: path SI's head moves on (a digger has bored: air behind it)
pleave: cmp     BYTE PTR [si + PA_DR], 0
        je      pl_ret
        mov     BYTE PTR [si + PA_DR], 0
        mov     al, PA_X
        call    xyat
        mov     _fold, PX_FX
        mov     _fv, PX_SKY
        jmp     fld_swap_
pl_ret: ret

; pend: path SI stops
pend:   cmp     BYTE PTR [si + PA_ON], 0
        je      pe_ret
        call    pleave
        mov     BYTE PTR [si + PA_ON], 0
pe_ret: ret

ops_path:
        mov     optr, ax
        mov     bx, ax
        mov     al, [bx]                ; slot
        and     al, NPATH - 1
        mov     dl, PA_SIZE
        mul     dl                      ; * 10
        add     ax, OFFSET DGROUP:paths
        mov     si, ax
        call    pend
        mov     bx, optr
        mov     al, [bx + 1]
        mov     [si + PA_STY], al
        mov     ax, [bx + 2]
        mov     [si + PA_X], ax
        mov     ax, [bx + 4]
        mov     [si + PA_Y], ax
        mov     al, [bx + 6]
        mov     [si + PA_N], al
        add     bx, 7
        mov     [si + PA_P], bx
        mov     BYTE PTR [si + PA_ON], 1
        call    phead
        mov     al, [si + PA_N]
        xor     ah, ah
        add     ax, 7
        ret

pstep:  mov     si, OFFSET DGROUP:paths + (NPATH - 1) * PA_SIZE
        mov     oi, NPATH
pp_l:   cmp     BYTE PTR [si + PA_ON], 0
        je      pp_n
        mov     al, [si + PA_N]
        or      al, al
        jnz     pp_mv
        call    pend
        jmp     pp_n
pp_mv:  call    pleave
        dec     BYTE PTR [si + PA_N]
        mov     bx, [si + PA_P]
        mov     dl, [bx]
        inc     bx
        mov     [si + PA_P], bx
        mov     al, dl                  ; x += the high nibble, signed
        mov     cl, 4
        sar     al, cl
        cbw
        add     [si + PA_X], ax
        mov     al, dl                  ; y += the low nibble, signed
        shl     al, cl
        sar     al, cl
        cbw
        add     [si + PA_Y], ax
        call    phead
pp_n:   sub     si, PA_SIZE
        dec     oi
        jnz     pp_l
        ret

; ---- flows -----------------------------------------------------------------------

; fcells: flow SI's cells ocnt to oto-1, fold -> fv (and, mud, fire -> fv)
fcells: mov     al, oto
        cmp     al, ocnt
        jbe     fc_ret
        mov     al, ocnt                ; its cell: F_P + 3 ocnt
        xor     ah, ah
        mov     bx, ax
        add     bx, ax
        add     bx, ax
        add     bx, [si + F_P]
        mov     ax, [bx]
        mov     _fx, ax
        mov     al, [bx + 2]
        xor     ah, ah
        mov     _fy, ax
        call    fld_swap_
        cmp     omud, 0
        je      fc_n
        mov     al, _fold
        push    ax
        mov     _fold, PX_FX
        call    fld_swap_
        pop     ax
        mov     _fold, al
fc_n:   inc     ocnt
        jnz     fcells
fc_ret: ret

; fshow: flow SI's first AL cells in (fire over the air, mud as dirt)
fshow:  cmp     al, [si + F_SH]
        jbe     fs_ret
        mov     oto, al
        mov     bl, [si + F_SH]
        mov     [si + F_SH], al
        mov     ocnt, bl
        mov     _fold, PX_SKY
        mov     omud, 0
        mov     al, [si + F_KIND]
        cmp     al, FLOW_MUD
        mov     al, PX_FX
        jne     fs_f
        mov     omud, 1
        mov     al, PX_DIRT
fs_f:   mov     _fv, al
        jmp     fcells
fs_ret: ret

; fend: flow SI all in, then a fire goes out
fend:   mov     al, [si + F_N]
        call    fshow
        cmp     BYTE PTR [si + F_KIND], FLOW_MUD
        je      fe_off
        mov     ocnt, 0
        mov     omud, 0
        mov     al, [si + F_N]
        mov     oto, al
        mov     _fold, PX_FX
        mov     _fv, PX_SKY
        call    fcells
fe_off: mov     BYTE PTR [si + F_ON], 0
        ret

ops_flow:
        mov     optr, ax
        mov     si, OFFSET DGROUP:flows         ; the first free one,
        mov     cx, NFLOW - 1
of_free:
        cmp     BYTE PTR [si + F_ON], 0
        je      of_got
        add     si, F_SIZE
        loop    of_free
        cmp     BYTE PTR [si + F_ON], 0
        je      of_got
        call    fend                    ;  or the last, ended now
of_got: mov     bx, optr
        mov     al, [bx]
        mov     [si + F_KIND], al
        mov     al, [bx + 1]
        or      al, al
        jnz     of_fr
        inc     al
of_fr:  mov     [si + F_FRM], al
        mov     al, [bx + 2]
        mov     [si + F_HOLD], al
        mov     al, [bx + 3]
        mov     [si + F_N], al
        add     bx, 4
        mov     BYTE PTR [si + F_F], 0
        mov     BYTE PTR [si + F_SH], 0
        mov     WORD PTR [si + F_ACC], 0
        mov     [si + F_P], bx
        mov     BYTE PTR [si + F_ON], 1
        mov     al, [si + F_N]          ; took 4 + 3n
        xor     ah, ah
        mov     dx, ax
        add     ax, ax
        add     ax, dx
        add     ax, 4
        ret

fstep:  mov     si, OFFSET DGROUP:flows + (NFLOW - 1) * F_SIZE
        mov     oi, NFLOW
fl_l:   cmp     BYTE PTR [si + F_ON], 0
        jne     fl_on
        jmp     fl_n
fl_on:  cmp     BYTE PTR [si + F_KIND], FLOW_MUD
        je      fl_m
        mov     ofire, 1
fl_m:   mov     al, [si + F_F]
        cmp     al, [si + F_FRM]
        jae     fl_hold
        inc     BYTE PTR [si + F_F]
        mov     ax, [si + F_ACC]        ; acc += n; each frames of it: a cell more
        mov     dl, [si + F_N]
        xor     dh, dh
        add     ax, dx
        mov     bl, [si + F_SH]
        mov     ocnt, bl
        mov     dl, [si + F_FRM]
fl_sub: sub     ax, dx
        jc      fl_sh
        mov     [si + F_ACC], ax
        inc     ocnt
        jnz     fl_sub
        jmp     fl_sh2
fl_sh:  add     ax, dx                  ; (the last one didn't go)
        mov     [si + F_ACC], ax
fl_sh2: mov     al, ocnt
        call    fshow
        jmp     fl_end
fl_hold:
        mov     al, [si + F_HOLD]
        or      al, al
        jz      fl_end
        dec     BYTE PTR [si + F_HOLD]
fl_end: mov     al, [si + F_F]
        cmp     al, [si + F_FRM]
        jb      fl_n
        cmp     BYTE PTR [si + F_HOLD], 0
        jne     fl_n
        call    fend
fl_n:   sub     si, F_SIZE
        dec     oi
        jz      fl_ret
        jmp     fl_l
fl_ret: ret

; ---- beams ----------------------------------------------------------------------------

; lplot: (lx0, ly0): on, fire over air and dirt; off, fire back to air
lplot:  mov     ax, lx0
        mov     _fx, ax
        mov     ax, ly0
        mov     _fy, ax
        cmp     lon, 0
        je      lp_off
        mov     _fv, PX_FX
        mov     _fold, PX_SKY
        call    fld_swap_
        mov     _fold, PX_DIRT
        jmp     fld_swap_
lp_off: mov     _fold, PX_FX
        mov     _fv, PX_SKY
        jmp     fld_swap_

; bline: beam SI's line (Bresenham, as the reference view draws it)
bline:  mov     ax, [si + BE_X0]
        mov     lx0, ax
        mov     ax, [si + BE_Y0]
        mov     ly0, ax
        mov     ax, [si + BE_X1]
        mov     lx1, ax
        mov     ax, [si + BE_Y1]
        mov     ly1, ax
        mov     ax, lx1                 ; dx = |x1 - x0|, sx
        sub     ax, lx0
        mov     dx, 1
        jns     bl_px
        neg     ax
        mov     dx, -1
bl_px:  mov     ldx, ax
        mov     lsx, dx
        mov     ax, ly1                 ; dy, sy
        sub     ax, ly0
        mov     dx, 1
        jns     bl_py
        neg     ax
        mov     dx, -1
bl_py:  mov     ldy, ax
        mov     lsy, dx
        mov     ax, ldx                 ; err = dx - dy
        sub     ax, ldy
        mov     lerr, ax
bl_l:   call    lplot
        mov     ax, lx0
        cmp     ax, lx1
        jne     bl_go
        mov     ax, ly0
        cmp     ax, ly1
        jne     bl_go
        ret
bl_go:  mov     ax, lerr                ; e2 = 2 err
        add     ax, ax
        mov     le2, ax
        add     ax, ldy                 ; e2 > -dy: x moves
        or      ax, ax
        js      bl_ny
        jz      bl_ny
        mov     ax, lerr
        sub     ax, ldy
        mov     lerr, ax
        mov     ax, lx0
        add     ax, lsx
        mov     lx0, ax
bl_ny:  mov     ax, le2                 ; e2 < dx: y moves
        sub     ax, ldx
        jns     bl_l
        mov     ax, lerr
        add     ax, ldx
        mov     lerr, ax
        mov     ax, ly0
        add     ax, lsy
        mov     ly0, ax
        jmp     bl_l

; boff: beam SI's pixels back to air, and it is gone
boff:   cmp     BYTE PTR [si + BE_LEFT], 0
        je      bf_ret
        mov     BYTE PTR [si + BE_LEFT], 0
        mov     lon, 0
        jmp     bline
bf_ret: ret

ops_beam:
        mov     optr, ax
        mov     si, OFFSET DGROUP:beams         ; the first, unless it's live
        cmp     BYTE PTR [si + BE_LEFT], 0
        je      ob_s
        add     si, BE_SIZE
ob_s:   call    boff
        mov     bx, optr
        mov     ax, [bx]
        mov     [si + BE_X0], ax
        mov     ax, [bx + 2]
        mov     [si + BE_Y0], ax
        mov     ax, [bx + 4]
        mov     [si + BE_X1], ax
        mov     ax, [bx + 6]
        mov     [si + BE_Y1], ax
        mov     al, [bx + 9]            ; (byte 9: its frames)
        or      al, al
        jnz     ob_f
        inc     al
ob_f:   mov     [si + BE_LEFT], al
        mov     lon, 1
        call    bline
        mov     ax, 10
        ret

bstep:  mov     si, OFFSET DGROUP:beams + (NBEAM - 1) * BE_SIZE
        mov     oi, NBEAM
bs_l:   cmp     BYTE PTR [si + BE_LEFT], 0
        je      bs_n
        dec     BYTE PTR [si + BE_LEFT]
        jnz     bs_n
        inc     BYTE PTR [si + BE_LEFT] ; boff wants it live
        call    boff
bs_n:   sub     si, BE_SIZE
        dec     oi
        jnz     bs_l
        ret

; ---- a frame, a segment's end, a new shot -------------------------------------------

ops_step:
        mov     ofire, 0
        call    pstep
        call    fstep
        call    bstep
        mov     al, ofire
        ret

ops_seg_end:
        mov     si, OFFSET DGROUP:paths + (NPATH - 1) * PA_SIZE
        mov     oi, NPATH
se_p:   call    pend
        sub     si, PA_SIZE
        dec     oi
        jnz     se_p
        mov     si, OFFSET DGROUP:flows + (NFLOW - 1) * F_SIZE
        mov     oi, NFLOW
se_f:   cmp     BYTE PTR [si + F_ON], 0
        je      se_n
        call    fend
se_n:   sub     si, F_SIZE
        dec     oi
        jnz     se_f
        ret

ops_beams_off:
        mov     si, OFFSET DGROUP:beams + (NBEAM - 1) * BE_SIZE
        mov     oi, NBEAM
bo_l:   call    boff
        sub     si, BE_SIZE
        dec     oi
        jnz     bo_l
        ret

ops_reset:
        push    ds
        pop     es
        mov     di, OFFSET DGROUP:ops_data
        mov     cx, NPATH * PA_SIZE + NFLOW * F_SIZE + NBEAM * BE_SIZE
        xor     al, al
        cld
        rep     stosb
        ret

_TEXT   ENDS
        END
