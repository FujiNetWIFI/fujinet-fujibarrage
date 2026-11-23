;
; fld.asm -- the VGA build's field: the screen itself (A000h, mode 13h), a
; byte a pixel, 320 a line, lines 0-175. A pixel's colour comes from its
; value, its line and its x (vcol), through tables src/msdos/vga/video.c
; builds once:
;
;   value 0 tank  colour 0 (black); a tank's own colours are the seats'
;         1 sky   _v_sky[y] (the sky's ramp down the field); with stars on
;                 (_v_stars) a pixel here and there is the star's (V_STAR)
;         2 dirt  _v_dirt[y] (its stratum) + _v_tex[(x + _v_toff[y]) & 63]
;         3 fire  _v_fire[y] (its band: video.c cycles the fire's DAC)
;
; and back through _v_class (a colour's value). A round's look is the
; DAC's (video.c): the pixels stay as they are. The CoCo 3's
; src/coco3/fld3.asm's routines and steps (so the same pixels:
; tools/dossim/refcheck.py), a byte a pixel.
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
SCRSEG  EQU     0A000h
V_STAR  EQU     48              ; (vga.h)

        EXTRN   _v_sky:BYTE, _v_dirt:BYTE, _v_fire:BYTE, _v_toff:BYTE
        EXTRN   _v_tex:BYTE, _v_rand:BYTE, _v_class:BYTE, _v_stars:BYTE

_DATA   SEGMENT WORD PUBLIC 'DATA'
        PUBLIC  _fld_w, _fld_h, _fld_mid
_fld_w  DW      FW              ; the field's size, a tank's middle over its
_fld_h  DB      FLINES          ;  foot (src/8086/pt.asm)
_fld_mid DB     3
; a quarter circle: q32[i] = 64 sqrt(1 - (i/32)^2) (the Atari's fld.s)
q32     DB      64,64,64,63,63,62,62,61,60,59,57,56,54,53,51,49
        DB      46,44,41,38,35,32,28,24,20,15,10,5,0,0,0,0,0
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
cx0     DW      ?               ; a span's first x
_fv     DB      ?
_fold   DB      ?
_erx    DB      ?
_ery    DB      ?
_emode  DB      ?
_fld_ncols DB   ?
cur     DB      ?               ; a column's run: from line cur
cnt     DB      ?               ;  to before cnt
erem    DB      ?               ; the ellipse's steps
edy     DB      ?
ei      DB      ?
dox     DW      ?               ; fld_dots: (fx, fy) kept
doy     DW      ?
_BSS    ENDS

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP, SS:DGROUP

        PUBLIC  fld_tables_, fld_clear_, fld_plot_, fld_get_, fld_swap_
        PUBLIC  fld_hspan_, fld_vrun_, fld_col_, fld_cols_, fld_ellipse_
        PUBLIC  fld_dots_, fld_keep_

; fld_tables: the lines' offsets (once, at start-up)
fld_tables_ PROC NEAR
        xor     bx, bx
        xor     ax, ax
ft_l:   mov     rowoff[bx], ax
        add     ax, FW
        add     bx, 2
        cmp     bx, FLINES * 2
        jb      ft_l
        ret
fld_tables_ ENDP

; vcol: AL = the colour of value AL at x DX, line BX (0-175) (AH changed)
vcol    PROC    NEAR
        cmp     al, 1
        je      vc_sky
        cmp     al, 2
        je      vc_dirt
        cmp     al, 3
        je      vc_fire
        xor     al, al                  ; tank: black
        ret
vc_fire:
        mov     al, _v_fire[bx]
        ret
vc_dirt:
        push    bx
        mov     ah, _v_dirt[bx]
        mov     bl, _v_toff[bx]
        add     bl, dl
        and     bx, 63
        mov     al, _v_tex[bx]
        add     al, ah
        pop     bx
        ret
vc_sky: mov     al, _v_sky[bx]
        cmp     _v_stars, 0
        je      vc_x
        push    bx
        mov     bl, _v_rand[bx]         ; a star where the line's hash of x is 0
        add     bl, dl
        xor     bh, bh
        cmp     _v_rand[bx], 0
        pop     bx
        jne     vc_x
        mov     al, V_STAR
vc_x:   ret
vcol    ENDP

; at: (fx, fy) in the field? CF clear: ES:DI its byte, BX y, DX x (ES the
; screen either way)
at      PROC    NEAR
        mov     ax, SCRSEG
        mov     es, ax
        mov     dx, _fx
        cmp     dx, FW
        jae     at_out                  ; (negative too: unsigned)
        mov     bx, _fy
        cmp     bx, FLINES
        jae     at_out
        mov     di, bx
        shl     di, 1
        mov     di, rowoff[di]
        add     di, dx
        clc
        ret
at_out: stc
        ret
at      ENDP

; fld_plot: pixel (fx, fy) = fv
fld_plot_ PROC NEAR
        push    di
        call    at
        jc      pl_x
        mov     al, _fv
        call    vcol
        mov     es:[di], al
pl_x:   pop     di
        ret
fld_plot_ ENDP

; fld_get: AL = pixel (fx, fy)'s value; 1 (sky) off the field
fld_get_ PROC   NEAR
        push    di
        call    at
        jc      ge_sky
        mov     bl, es:[di]
        xor     bh, bh
        mov     al, _v_class[bx]
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
        push    bx
        mov     bl, es:[di]
        xor     bh, bh
        mov     al, _v_class[bx]
        pop     bx
        cmp     al, _fold
        jne     sw_x
        mov     al, _fv
        call    vcol
        mov     es:[di], al
sw_x:   pop     di
        ret
fld_swap_ ENDP

; clip: line fy, fx to fx1 clipped to the field (fx, fx1 written back, as
; fld3's). CF set: nothing of it is in. Else ES:DI the first pixel, BX the
; line, DX x0, CX the pixels.
clip    PROC    NEAR
        mov     ax, SCRSEG
        mov     es, ax
        mov     bx, _fy
        cmp     bx, FLINES
        jae     cl_out
        mov     dx, _fx
        or      dx, dx
        jns     cl_c0
        xor     dx, dx
        mov     _fx, dx
cl_c0:  cmp     dx, FW
        jge     cl_out                  ; x0 past the right
        mov     cx, _fx1
        or      cx, cx
        js      cl_out                  ; x1 left of the field
        cmp     cx, FW - 1
        jle     cl_c1
        mov     cx, FW - 1
        mov     _fx1, cx
cl_c1:  cmp     dx, cx
        jg      cl_out                  ; (x0 > x1: nothing)
        sub     cx, dx
        inc     cx
        mov     di, bx
        shl     di, 1
        mov     di, rowoff[di]
        add     di, dx
        clc
        ret
cl_out: stc
        ret
clip    ENDP

; fill: CX pixels from ES:DI (x DX, line BX) = value AL's colours
fill    PROC    NEAR
        cld
        cmp     al, 2
        je      fi_dirt                 ; dirt: its texture
        cmp     al, 1
        jne     fi_solid
        cmp     _v_stars, 0
        jne     fi_each                 ; sky with stars
fi_solid:
        call    vcol                    ; (one colour all along)
        rep     stosb
        ret
fi_each:
        mov     ah, al
fi_l:   mov     al, ah
        push    ax
        call    vcol
        stosb
        pop     ax
        inc     dx
        loop    fi_l
        ret
fi_dirt:
        mov     ah, _v_dirt[bx]         ; the stratum, the grain along the line
        mov     al, _v_toff[bx]
        add     al, dl
        mov     bl, al
fd_l:   and     bx, 63
        mov     al, _v_tex[bx]
        add     al, ah
        stosb
        inc     bx
        loop    fd_l
        ret
fill    ENDP

; fld_hspan: line fy, x from fx to fx1 (both included) = fv, clipped
fld_hspan_ PROC NEAR
        push    di
        call    clip
        jc      hs_x
        mov     al, _fv
        call    fill
hs_x:   pop     di
        ret
fld_hspan_ ENDP

; fld_clear: the field all sky
fld_clear_ PROC NEAR
        push    di
        mov     ax, SCRSEG
        mov     es, ax
        xor     bx, bx
cr_l:   mov     di, bx
        shl     di, 1
        mov     di, rowoff[di]
        xor     dx, dx
        mov     cx, FW
        mov     al, 1
        push    bx
        call    fill
        pop     bx
        inc     bx
        cmp     bx, FLINES
        jb      cr_l
        pop     di
        ret
fld_clear_ ENDP

; vrows: rows cur..cnt-1 of column fx = value fv (a loop for each value)
vrows   PROC    NEAR
        mov     cl, cnt
        sub     cl, cur
        jbe     vr_x
        xor     ch, ch
        push    si
        push    di
        mov     ax, SCRSEG
        mov     es, ax
        mov     dx, _fx
        mov     bl, cur
        xor     bh, bh
        mov     di, bx
        shl     di, 1
        mov     di, rowoff[di]
        add     di, dx
        mov     al, _fv
        cmp     al, 1
        je      vr_sky
        cmp     al, 2
        je      vr_dirt
        cmp     al, 3
        je      vr_fire
vr_tk:  mov     byte ptr es:[di], 0     ; tank: black
        add     di, FW
        loop    vr_tk
        jmp     short vr_d
vr_fire:
        mov     al, _v_fire[bx]
        mov     es:[di], al
        add     di, FW
        inc     bx
        loop    vr_fire
        jmp     short vr_d
vr_sky: cmp     _v_stars, 0
        jne     vr_star
vr_s:   mov     al, _v_sky[bx]
        mov     es:[di], al
        add     di, FW
        inc     bx
        loop    vr_s
        jmp     short vr_d
vr_star:
        mov     al, 1                   ; (stars: vcol's test a line)
        call    vcol
        mov     es:[di], al
        add     di, FW
        inc     bx
        loop    vr_star
        jmp     short vr_d
vr_dirt:
        mov     al, _v_toff[bx]         ; the stratum and the grain at x
        add     al, dl
        and     ax, 63
        mov     si, ax
        mov     al, _v_tex[si]
        add     al, _v_dirt[bx]
        mov     es:[di], al
        add     di, FW
        inc     bx
        loop    vr_dirt
vr_d:   pop     di
        pop     si
vr_x:   ret
vrows   ENDP

; fld_vrun: column fx, rows fy to fx1-1 = fv (all in the field)
fld_vrun_ PROC  NEAR
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
rn_ok:  call    vrows
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

; fld_cols: fld_ncols columns from fx on become the TERRAIN entries from
; fent on (fx, fent past them after)
fld_cols_ PROC  NEAR
fs_l:   cmp     _fld_ncols, 0
        je      fs_x
        call    fld_col_
        xor     ah, ah
        add     _fent, ax
        inc     _fx
        dec     _fld_ncols
        jmp     fs_l
fs_x:   ret
fld_cols_ ENDP

; espan: line fy, x from fx to fx1 (clipped): its pixels that are fold (sky
; or fire) become fv (fld_swap's, pixel for pixel)
espan   PROC    NEAR
        push    di
        call    clip
        jc      es_x
es_l:   push    bx
        mov     bl, es:[di]
        xor     bh, bh
        mov     al, _v_class[bx]
        pop     bx
        cmp     al, _fold
        jne     es_n
        mov     al, _fv
        call    vcol
        mov     es:[di], al
es_n:   inc     di
        inc     dx
        loop    es_l
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
; many (dx, dy) pairs (signed bytes). fold FFh: each pixel = colour fv;
; else each pixel that is fold = fv. Off the field is left out. (fx, fy)
; are kept.
fld_dots_ PROC  NEAR
        push    si
        push    di
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
        call    at                      ; a colour, as it is
        jc      dt_n
        mov     al, _fv
        mov     es:[di], al
        jmp     short dt_n
dt_sw:  call    fld_swap_
dt_n:   pop     cx
        loop    dt_l
dt_x:   mov     ax, dox
        mov     _fx, ax
        mov     ax, doy
        mov     _fy, ax
        pop     di
        pop     si
        ret
fld_dots_ ENDP

; fld_keep: a box: lines fy.. (fv of them), pixels fx.. (fx1 a line); fold
; 0: kept at fent, else put back from there
fld_keep_ PROC  NEAR
        push    si
        push    di
        push    ds
        cld
        mov     bx, _fy
        shl     bx, 1
        mov     dl, _fv                 ; DL: the lines
        mov     cx, _fx1                ; pixels a line
        mov     si, _fent
        mov     ax, _fx
        mov     dh, _fold
        push    ax
        or      dl, dl
        jz      kp_x
kp_l:   pop     ax
        push    ax
        mov     di, rowoff[bx]
        add     di, ax
        push    cx
        or      dh, dh
        jnz     kp_w
        push    si                      ; kept: the screen to fent
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
kp_w:   mov     ax, SCRSEG              ; put back: fent to the screen
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
