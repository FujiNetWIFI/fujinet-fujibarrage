;
; fld.asm -- the CGA build's field: the screen itself (B800h, mode 4), 320
; pixels a line, four a byte (pixel 0 in bits 7-6), 80 bytes a line, even
; lines from 0, odd lines from 2000h (rowoff), lines 0-175. A pixel's two
; bits are its colour, the value's through fillt (a value as a byte of
; four) and invt (two bits as a value):
;
;   value 0 a tank    colour 1 (the set's first: green or cyan)
;         1 sky       colour 0 (the background: any of 16, per round)
;         2 dirt      colour 3 (brown, yellow, grey or white)
;         3 fire      colour 2 (red or magenta)
;
; so a round's look is the palette (video.c), never a redraw. The CoCo
; 1/2's src/coco2/fld2.asm in 8086, its ellipse the same steps (so the same
; pixels: tools/dossim/refcheck.py), at 320 pixels a line.
;
; The arguments are DGROUP variables (src/msdos/fld.h; the player's
; src/8086/*.asm use the same). Every entry keeps SI, DI, BP and DS and
; leaves DF clear; it may change AX, BX, CX, DX and ES. C calls them
; through #pragma aux (fld.h). 8086 only.
;
        .8086

DGROUP  GROUP   _DATA, _BSS

FLINES  EQU     176
FW      EQU     320
SCRSEG  EQU     0B800h

_DATA   SEGMENT WORD PUBLIC 'DATA'
        PUBLIC  _fld_w, _fld_h, _fld_mid
_fld_w  DW      FW              ; the field's size, a tank's middle over its
_fld_h  DB      FLINES          ;  foot (src/8086/pt.asm)
_fld_mid DB     3
; a quarter circle: q32[i] = 64 sqrt(1 - (i/32)^2) (the Atari's fld.s)
q32     DB      64,64,64,63,63,62,62,61,60,59,57,56,54,53,51,49
        DB      46,44,41,38,35,32,28,24,20,15,10,5,0,0,0,0,0
pmask   DB      0C0h,30h,0Ch,03h        ; a pixel's bits
lmask   DB      0FFh,3Fh,0Fh,03h        ; a span's first byte: from pixel n on
rmask   DB      0C0h,0F0h,0FCh,0FFh     ; its last byte: up to pixel n
fillt   DB      55h,00h,0FFh,0AAh       ; a value as four pixels' colours
invt    DB      1,0,3,2                 ; a colour as a value
_DATA   ENDS

_BSS    SEGMENT WORD PUBLIC 'BSS'
        PUBLIC  _fx, _fy, _fx1, _ex, _ey, _fent
        PUBLIC  _fv, _fold, _erx, _ery, _emode, _fld_ncols
_fx     DW      ?
_fy     DW      ?
_fx1    DW      ?
_ex     DW      ?
_ey     DW      ?
_fent   DW      ?               ; a TERRAIN entry, a shape, a keep buffer
rowoff  DW      FLINES DUP (?)  ; a line's offset (fld_tables)
ehw     DW      ?               ; the ellipse: a row's half width
edw     DW      ?               ;  its dy as a word
xb      DW      ?               ; a column's byte in its line
_fv     DB      ?
_fold   DB      ?
_erx    DB      ?
_ery    DB      ?
_emode  DB      ?
_fld_ncols DB   ?
nm      DB      ?               ; a column's pixel: its other bits
cval    DB      ?               ;  and its value's bits
cur     DB      ?               ; a column's run: from line cur
cnt     DB      ?               ;  to before cnt
erem    DB      ?               ; the ellipse's steps
edy     DB      ?
ei      DB      ?
oldf    DB      ?               ; espan: four pixels of the old value
newf    DB      ?               ;  and of the new
emsk    DB      ?               ;  the byte's pixels in the span
dox     DW      ?               ; fld_dots: (fx, fy) kept
doy     DW      ?
; fld_cols' byte groups: a column's TERRAIN entry as an iterator
I_P     EQU     0               ; (word) its next end
I_K     EQU     2               ; ends left in it
I_V     EQU     3               ; this run's value (1 sky, 2 dirt)
I_E     EQU     4               ; this run's end (a line)
I_M     EQU     5               ; the column's bits in the byte
I_SIZE  EQU     6
its     DB      4 * I_SIZE DUP (?)
gn      DB      ?               ; columns left
gk      DB      ?               ;  in this byte
gkeep   DB      ?               ; the byte's bits not ours
gval    DB      ?               ; a run's byte
_BSS    ENDS

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP, SS:DGROUP

        PUBLIC  fld_tables_, fld_clear_, fld_plot_, fld_get_, fld_swap_
        PUBLIC  fld_hspan_, fld_vrun_, fld_col_, fld_cols_, fld_ellipse_
        PUBLIC  fld_dots_, fld_keep_

; fld_tables: the lines' offsets (once, at start-up)
fld_tables_ PROC NEAR
        xor     bx, bx
        xor     ax, ax                  ; line y's: (y & 1) * 2000h + (y >> 1) * 80
ft_l:   mov     rowoff[bx], ax
        mov     cx, ax
        add     cx, 2000h
        mov     rowoff[bx + 2], cx
        add     ax, 80
        add     bx, 4
        cmp     bx, FLINES * 2
        jb      ft_l
        ret
fld_tables_ ENDP

; fld_clear: the field all sky
fld_clear_ PROC NEAR
        push    di
        cld
        mov     ax, SCRSEG
        mov     es, ax
        mov     al, fillt[1]
        mov     ah, al
        xor     di, di
        mov     cx, FLINES / 2 * 80 / 2
        rep     stosw
        mov     di, 2000h
        mov     cx, FLINES / 2 * 80 / 2
        rep     stosw
        pop     di
        ret
fld_clear_ ENDP

; at: (fx, fy) in the field? CF clear: ES:DI its byte, BX its pixel (0-3)
; (ES the screen either way)
at      PROC    NEAR
        mov     ax, SCRSEG
        mov     es, ax
        mov     ax, _fx
        cmp     ax, FW
        jae     at_out                  ; (negative too: unsigned)
        mov     bx, _fy
        cmp     bx, FLINES
        jae     at_out
        shl     bx, 1
        mov     di, rowoff[bx]
        mov     bx, ax
        shr     ax, 1
        shr     ax, 1
        add     di, ax
        and     bx, 3
        clc
        ret
at_out: stc
        ret
at      ENDP

; put: the pixel BX of ES:DI = fv
put     PROC    NEAR
        mov     ah, pmask[bx]
        mov     bl, _fv
        xor     bh, bh
        mov     al, fillt[bx]
        and     al, ah
        not     ah
        and     ah, es:[di]
        or      al, ah
        mov     es:[di], al
        ret
put     ENDP

; fld_plot: pixel (fx, fy) = fv
fld_plot_ PROC NEAR
        push    di
        call    at
        jc      pl_x
        call    put
pl_x:   pop     di
        ret
fld_plot_ ENDP

; fld_get: AL = pixel (fx, fy)'s value; 1 (sky) off the field
fld_get_ PROC   NEAR
        push    di
        call    at
        jc      ge_sky
        mov     al, es:[di]
        mov     cx, 3
        sub     cx, bx
        shl     cx, 1                   ; its bits: 2 (3 - pixel) up
        shr     al, cl
        and     al, 3
        mov     bl, al
        xor     bh, bh
        mov     al, invt[bx]
        pop     di
        ret
ge_sky: mov     al, 1
        pop     di
        ret
fld_get_ ENDP

; fld_swap: pixel (fx, fy) = fv if it is fold
fld_swap_ PROC  NEAR
        push    di
        call    at
        jc      sw_x
        mov     ah, pmask[bx]
        mov     al, es:[di]
        and     al, ah                  ; its bits, in place
        mov     cx, bx
        mov     bl, _fold
        xor     bh, bh
        mov     dl, fillt[bx]           ; fold's, in place
        and     dl, ah
        mov     bx, cx
        cmp     al, dl
        jne     sw_x
        call    put
sw_x:   pop     di
        ret
fld_swap_ ENDP

; edge: the pixels of mask AH in ES:DI get DL's (AX changed)
edge    PROC    NEAR
        mov     al, dl
        and     al, ah
        not     ah
        and     ah, es:[di]
        or      al, ah
        mov     es:[di], al
        ret
edge    ENDP

; clip: line fy, fx to fx1 clipped to the field (fx, fx1 written back, as
; fld2's). CF set: nothing of it is in. Else DI the line's offset, AX x0,
; CX x1, ES the screen.
clip    PROC    NEAR
        mov     ax, SCRSEG
        mov     es, ax
        mov     bx, _fy
        cmp     bx, FLINES
        jae     cl_out
        shl     bx, 1
        mov     di, rowoff[bx]
        mov     ax, _fx
        or      ax, ax
        jns     cl_c0
        xor     ax, ax
        mov     _fx, ax
cl_c0:  cmp     ax, FW
        jge     cl_out                  ; x0 past the right
        mov     cx, _fx1
        or      cx, cx
        js      cl_out                  ; x1 left of the field
        cmp     cx, FW - 1
        jle     cl_c1
        mov     cx, FW - 1
        mov     _fx1, cx
cl_c1:  cmp     ax, cx
        jg      cl_out                  ; (x0 > x1: nothing)
        clc
        ret
cl_out: stc
        ret
clip    ENDP

; fld_hspan: line fy, x from fx to fx1 (both included) = fv, clipped
fld_hspan_ PROC NEAR
        push    di
        cld
        call    clip
        jc      hs_x
        mov     bl, _fv
        xor     bh, bh
        mov     dl, fillt[bx]           ; DL: the value's byte
        mov     bx, ax
        and     bx, 3
        mov     dh, lmask[bx]           ; DH: the first byte's mask
        mov     bx, cx
        and     bx, 3
        mov     bl, rmask[bx]           ; BL: the last's
        shr     ax, 1
        shr     ax, 1
        shr     cx, 1
        shr     cx, 1
        add     di, ax                  ; the first byte
        sub     cx, ax                  ; bytes after it
        jnz     hs_multi
        and     dh, bl                  ; one byte: both masks
        mov     ah, dh
        call    edge
        jmp     short hs_x
hs_multi:
        mov     ah, dh
        call    edge
        inc     di
        dec     cx                      ; the whole bytes between
        jz      hs_last
        mov     al, dl
        rep     stosb
hs_last:
        mov     ah, bl
        call    edge
hs_x:   pop     di
        ret
fld_hspan_ ENDP

; colset: fx's byte (xb), its pixel's other bits (nm), fv in its place (cval)
colset  PROC    NEAR
        mov     ax, _fx
        mov     bx, ax
        shr     ax, 1
        shr     ax, 1
        mov     xb, ax
        and     bx, 3
        mov     ah, pmask[bx]
        mov     al, ah
        not     al
        mov     nm, al
        mov     bl, _fv
        xor     bh, bh
        mov     al, fillt[bx]
        and     al, ah
        mov     cval, al
        ret
colset  ENDP

; vrows: rows cur..cnt-1 of column xb get cval (DI kept)
vrows   PROC    NEAR
        mov     cl, cnt
        sub     cl, cur
        jbe     vr_x
        push    di
        mov     ax, SCRSEG
        mov     es, ax
        mov     bl, cur
        xor     bh, bh
        shl     bx, 1
        mov     di, rowoff[bx]
        add     di, xb
        mov     dl, nm
        mov     dh, cval
        test    bl, 2                   ; (cur odd: from the odd bank)
        jnz     vr_odd
vr_l:   mov     al, es:[di]             ; an even line
        and     al, dl
        or      al, dh
        mov     es:[di], al
        add     di, 2000h
        dec     cl
        jz      vr_d
vr_odd: mov     al, es:[di]             ; an odd line
        and     al, dl
        or      al, dh
        mov     es:[di], al
        sub     di, 2000h - 80
        dec     cl
        jnz     vr_l
vr_d:   pop     di
vr_x:   ret
vrows   ENDP

; fld_vrun: column fx, rows fy to fx1-1 = fv (all in the field)
fld_vrun_ PROC  NEAR
        call    colset
        mov     al, byte ptr _fy
        mov     cur, al
        mov     al, byte ptr _fx1
        mov     cnt, al
        jmp     vrows
fld_vrun_ ENDP

; run: rows cur..cnt-1 of column fx = AL (cnt clamped to the field); cur =
; cnt after
run     PROC    NEAR
        mov     _fv, al
        cmp     cnt, FLINES
        jbe     rn_ok
        mov     cnt, FLINES
rn_ok:  call    colset
        call    vrows
        mov     al, cnt
        mov     cur, al
        ret
run     ENDP

; fld_col: column fx becomes the TERRAIN entry at fent (sky, dirt); AL its
; length
fld_col_ PROC   NEAR
        push    si
        mov     cur, 0
        mov     si, _fent
        mov     al, [si]
        cmp     al, 0F0h
        je      fc_empty
        ja      fc_spans
        mov     cnt, al                 ; one plain column: sky to its top,
        push    ax
        mov     al, 1
        call    run
        pop     ax
        mov     cur, al
        mov     cnt, FLINES             ;  dirt from there
        mov     al, 2
        call    run
        mov     al, 1
        pop     si
        ret
fc_empty:
        mov     cnt, FLINES
        mov     al, 1
        call    run
        mov     al, 1
        pop     si
        ret
fc_spans:
        and     al, 0Fh
        mov     ah, al                  ; AH: the spans left
        shl     al, 1
        inc     al                      ; 1 + 2n: the entry's length
        push    ax
        inc     si
fc_sp:  push    ax
        mov     al, [si]                ; top: sky till there
        mov     cnt, al
        mov     al, 1
        call    run
        mov     al, [si]
        mov     cur, al
        mov     al, [si + 1]            ; end: dirt till there
        mov     cnt, al
        add     si, 2
        mov     al, 2
        call    run
        pop     ax
        dec     ah
        jnz     fc_sp
        mov     cnt, FLINES
        mov     al, 1
        call    run
        pop     ax                      ; AL: the length
        pop     si
        ret
fld_col_ ENDP

; ---- a settle's columns (fld_cols): a byte's pixels at once (fld2's) ----
; A TERRAIN entry as its runs, top down: sky to the first end, then dirt and
; sky by turns (plain: its top, then the bottom; spans: top, end, top ...,
; then the bottom), an iterator I_* at DI.

; iinit: DI reads the entry at SI; AL its length
iinit   PROC    NEAR
        mov     byte ptr [di + I_V], 1
        mov     al, [si]
        cmp     al, 0F0h
        jb      ii_plain
        je      ii_empty
        and     al, 0Fh
        shl     al, 1                   ; 2n ends
        mov     [di + I_K], al
        inc     si
        mov     [di + I_P], si
        inc     al                      ; 1 + 2n
        push    ax
        call    iload
        pop     ax
        ret
ii_plain:
        mov     [di + I_P], si          ; its top, then the bottom
        mov     byte ptr [di + I_K], 1
        call    iload
        mov     al, 1
        ret
ii_empty:
        mov     byte ptr [di + I_K], 0
        call    iload
        mov     al, 1
        ret
iinit   ENDP

; inext: DI's next run
inext   PROC    NEAR
        xor     byte ptr [di + I_V], 3
inext   ENDP                            ; (on into iload)
; iload: DI's run's end: the entry's next, or the bottom when it has none
iload   PROC    NEAR
        mov     al, FLINES
        cmp     byte ptr [di + I_K], 0
        je      il_s
        dec     byte ptr [di + I_K]
        mov     bx, [di + I_P]
        mov     al, [bx]
        inc     bx
        mov     [di + I_P], bx
        cmp     al, FLINES
        jbe     il_s
        mov     al, FLINES
il_s:   mov     [di + I_E], al
        ret
iload   ENDP

; fld_cols: fld_ncols columns from fx on become the TERRAIN entries from
; fent on (fx, fent past them after)
fld_cols_ PROC  NEAR
        push    si
        push    di
        mov     al, _fld_ncols
        mov     gn, al
fs_c:   cmp     gn, 0
        je      fs_x
        mov     al, byte ptr _fx
        and     al, 3
        neg     al
        add     al, 4                   ; the byte's columns from fx on
        cmp     al, gn
        jbe     fs_k
        mov     al, gn
fs_k:   mov     gk, al
        cmp     al, 1
        jne     fs_grp
        call    fld_col_                ; alone (AL: its entry's length)
        xor     ah, ah
        add     _fent, ax
        jmp     short fs_nx
fs_grp: call    cgroup
fs_nx:  mov     al, gk
        xor     ah, ah
        add     _fx, ax
        sub     gn, al
        jmp     fs_c
fs_x:   mov     _fld_ncols, 0
        pop     di
        pop     si
        ret
fld_cols_ ENDP

; cgroup: the gk columns from fx on, all in one byte, become the entries at
; fent (fent past them after): the row runs alike in all of them, the byte
; at once
cgroup  PROC    NEAR
        mov     ax, _fx
        mov     bx, ax
        shr     ax, 1
        shr     ax, 1
        mov     xb, ax
        and     bx, 3                   ; the first column's bits: pmask[bx]
        mov     gkeep, 0
        mov     di, OFFSET DGROUP:its
        mov     cl, gk
gr_i:   mov     al, pmask[bx]
        mov     [di + I_M], al
        or      gkeep, al
        mov     si, _fent
        push    bx
        push    cx
        call    iinit                   ; (AL: the entry's length)
        pop     cx
        pop     bx
        xor     ah, ah
        add     _fent, ax
        inc     bx
        add     di, I_SIZE
        dec     cl
        jnz     gr_i
        not     gkeep                   ; (the byte's other bits stay)
        mov     cur, 0
gr_m:   mov     di, OFFSET DGROUP:its   ; the nearest end, and the byte till then
        mov     cl, gk
        mov     cnt, FLINES
        mov     gval, 0
gr_e:   mov     al, [di + I_E]
        cmp     al, cnt
        jae     gr_e1
        mov     cnt, al
gr_e1:  mov     bl, [di + I_V]
        xor     bh, bh
        mov     al, fillt[bx]
        and     al, [di + I_M]
        or      gval, al
        add     di, I_SIZE
        dec     cl
        jnz     gr_e
        mov     al, cnt
        cmp     al, cur
        jbe     gr_adv                  ; (an empty run)
        call    grows
        mov     al, cnt
        mov     cur, al
gr_adv: mov     di, OFFSET DGROUP:its   ; the runs that end here: their next
        mov     cl, gk
gr_a:   mov     al, [di + I_E]
        cmp     al, cur
        ja      gr_a1
        push    cx
        call    inext
        pop     cx
gr_a1:  add     di, I_SIZE
        dec     cl
        jnz     gr_a
        cmp     cur, FLINES
        jb      gr_m
        ret
cgroup  ENDP

; grows: rows cur..cnt-1 of byte xb = gval (gkeep: the bits kept)
grows   PROC    NEAR
        push    di
        mov     ax, SCRSEG
        mov     es, ax
        mov     cl, cnt
        sub     cl, cur
        mov     bl, cur
        xor     bh, bh
        shl     bx, 1
        mov     di, rowoff[bx]
        add     di, xb
        mov     dl, gkeep
        mov     dh, gval
        or      dl, dl
        jz      gw_full
        test    bl, 2
        jnz     gw_podd
gw_p:   mov     al, es:[di]             ; part of the byte: an even line
        and     al, dl
        or      al, dh
        mov     es:[di], al
        add     di, 2000h
        dec     cl
        jz      gw_x
gw_podd:
        mov     al, es:[di]             ; an odd line
        and     al, dl
        or      al, dh
        mov     es:[di], al
        sub     di, 2000h - 80
        dec     cl
        jnz     gw_p
        jmp     short gw_x
gw_full:
        test    bl, 2                   ; the whole byte ours
        jnz     gw_fodd
gw_f:   mov     es:[di], dh
        add     di, 2000h
        dec     cl
        jz      gw_x
gw_fodd:
        mov     es:[di], dh
        sub     di, 2000h - 80
        dec     cl
        jnz     gw_f
gw_x:   pop     di
        ret
grows   ENDP

; espan: line fy, x from fx to fx1 (clipped): its pixels that are fold (sky
; or fire) become fv, a byte at a time (fld_swap's, pixel for pixel)
espan   PROC    NEAR
        push    di
        call    clip
        jc      es_x
        mov     bl, _fold
        xor     bh, bh
        mov     dl, fillt[bx]
        mov     oldf, dl
        mov     bl, _fv
        mov     dl, fillt[bx]
        mov     newf, dl
        mov     bx, ax
        and     bx, 3
        mov     dl, lmask[bx]
        mov     emsk, dl                ; the first byte's pixels: from x0 on
        mov     bx, cx
        and     bx, 3
        mov     dh, rmask[bx]           ; DH: the last byte's: up to x1
        shr     ax, 1
        shr     ax, 1
        shr     cx, 1
        shr     cx, 1
        add     di, ax
        sub     cx, ax
        inc     cx                      ; CX: the bytes
es_b:   cmp     cx, 1
        jne     es_m
        and     emsk, dh                ; (the last)
es_m:   mov     al, es:[di]             ; the pixels that are the old value:
        xor     al, oldf                ;  their pairs 00 here
        mov     ah, al
        shr     ah, 1
        or      al, ah
        and     al, 55h                 ; (a pair's low bit: not 00)
        mov     ah, al
        shl     ah, 1
        or      al, ah
        not     al                      ; the pairs that were 00
        and     al, emsk
        jz      es_n
        mov     ah, al                  ; they become the new value
        mov     al, es:[di]
        xor     al, newf
        and     al, ah
        xor     al, es:[di]
        mov     es:[di], al
es_n:   mov     emsk, 0FFh
        inc     di
        loop    es_b
es_x:   pop     di
        ret
espan   ENDP

; erow: one row of the ellipse: line fy, ex - hw to ex + hw
erow    PROC    NEAR
        mov     ax, _ex
        sub     ax, ehw
        mov     _fx, ax
        mov     ax, _ex
        add     ax, ehw
        mov     _fx1, ax
        mov     al, _emode
        or      al, al
        jnz     er_m
        jmp     fld_hspan_
er_m:   cmp     al, 1                   ; only the right value changes: sky
        jne     er_m2                   ;  becomes fv,
        mov     _fold, 1
        jmp     espan
er_m2:  mov     _fold, 3                ;  or fire becomes sky
        mov     _fv, 1
        jmp     espan
erow    ENDP

; fld_ellipse: the ellipse at (ex, ey), radii erx, ery. Its row dy is half
; as wide as erx * sqrt(1 - (dy/ery)^2): q32[i], i = dy*32/ery stepped
; without dividing (fld.s's steps, to the pixel)
fld_ellipse_ PROC NEAR
        cld
        mov     al, _ery
        or      al, al
        jnz     el_r
        inc     _ery
        mov     al, _ery
el_r:   shr     al, 1
        mov     erem, al                ; ery / 2: the rounding
        mov     edy, 0
        mov     ei, 0
el_row: mov     bl, ei                  ; hw = (erx * q32[i] + 32) >> 6
        xor     bh, bh
        mov     al, q32[bx]
        mul     _erx
        add     ax, 32
        mov     cl, 6
        shr     ax, cl
        mov     ehw, ax
        mov     al, edy
        xor     ah, ah
        mov     edw, ax
        mov     ax, _ey                 ; the row above: ey - dy
        sub     ax, edw
        mov     _fy, ax
        call    erow
        cmp     edy, 0
        je      el_nx
        mov     ax, _ey                 ; and below: ey + dy
        add     ax, edw
        mov     _fy, ax
        call    erow
el_nx:  mov     al, edy
        cmp     al, _ery
        jae     el_x
        inc     edy
        add     erem, 32
el_i:   mov     al, erem
        cmp     al, _ery
        jb      el_row
        sub     al, _ery
        mov     erem, al
        cmp     ei, 32
        jae     el_i
        inc     ei
        jmp     el_i
el_x:   ret
fld_ellipse_ ENDP

; fld_dots: a shape around (fx, fy): fent points at its count, then that
; many (dx, dy) pairs (signed bytes). fold FFh: each pixel = fv; else each
; pixel that is fold = fv. Off the field is left out. (fx, fy) are kept.
fld_dots_ PROC  NEAR
        push    si
        mov     ax, _fx
        mov     dox, ax
        mov     ax, _fy
        mov     doy, ax
        mov     si, _fent
        mov     cl, [si]
        inc     si
        xor     ch, ch
        jcxz    dt_x
dt_l:   push    cx
        mov     al, [si]                ; fx = ox + dx
        cbw
        add     ax, dox
        mov     _fx, ax
        mov     al, [si + 1]            ; fy = oy + dy
        cbw
        add     ax, doy
        mov     _fy, ax
        add     si, 2
        cmp     _fold, 0FFh
        jne     dt_sw
        call    fld_plot_
        jmp     short dt_n
dt_sw:  call    fld_swap_
dt_n:   pop     cx
        loop    dt_l
dt_x:   mov     ax, dox
        mov     _fx, ax
        mov     ax, doy
        mov     _fy, ax
        pop     si
        ret
fld_dots_ ENDP

; fld_keep: a box of whole bytes: lines fy.. (fv of them), bytes fx.. (fx1
; a line); fold 0: kept at fent, else put back from there
fld_keep_ PROC  NEAR
        push    si
        push    di
        push    ds
        cld
        mov     bx, _fy
        shl     bx, 1
        mov     dl, _fv                 ; DL: the lines
        mov     cx, _fx1                ; bytes a line
        mov     si, _fent
        mov     ax, _fx
        cmp     _fold, 0
        jne     kp_put
        mov     dh, 0                   ; kept: the screen to fent
        jmp     short kp_go
kp_put: mov     dh, 1                   ; put back: fent to the screen
kp_go:  push    ax
        or      dl, dl
        jz      kp_x
kp_l:   pop     ax
        push    ax
        mov     di, rowoff[bx]
        add     di, ax
        push    cx
        or      dh, dh
        jnz     kp_w
        ; screen -> DGROUP: DS:SI the screen, ES:DI fent
        push    si
        mov     ax, ds
        mov     es, ax
        mov     ax, di
        pop     di                      ; ES:DI = fent's place
        mov     si, ax
        mov     ax, SCRSEG
        mov     ds, ax
        rep     movsb
        mov     ax, ss
        mov     ds, ax
        mov     si, di                  ; (fent moves on)
        jmp     short kp_n
kp_w:   mov     ax, SCRSEG              ; DGROUP -> screen: DS:SI fent, ES:DI the screen
        mov     es, ax
        rep     movsb
kp_n:   pop     cx
        add     bx, 2
        dec     dl
        jnz     kp_l
kp_x:   pop     ax
        pop     ds
        pop     di
        pop     si
        ret
fld_keep_ ENDP

_TEXT   ENDS
        END
