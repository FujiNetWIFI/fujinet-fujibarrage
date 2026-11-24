; fldglue.asm -- the script player's (src/z80) names for the field's
; primitives (fld.c): its asm keeps IX and IY and expects BC and DE kept,
; and takes a result in A and L.

        SECTION code_user

        PUBLIC  _fld_plot, _fld_get, _fld_swap, _fld_hspan, _fld_vrun
        PUBLIC  _fld_col, _fld_cols, _fld_ellipse
        PUBLIC  fld_w, fld_h, fld_mid
        EXTERN  _c_fld_plot, _c_fld_get, _c_fld_swap, _c_fld_hspan, _c_fld_vrun
        EXTERN  _c_fld_col, _c_fld_cols, _c_fld_ellipse

; the field's size, a tank's middle over its foot (the sparks: src/z80/pt.asm)
fld_w:  defw    160
fld_h:  defb    44
fld_mid: defb   1

        MACRO   GLUE name, cname
name:   push    bc
        push    de
        push    ix
        push    iy
        call    cname
        pop     iy
        pop     ix
        pop     de
        pop     bc
        ld      a,l
        ret
        ENDM

        GLUE    _fld_plot, _c_fld_plot
        GLUE    _fld_get, _c_fld_get
        GLUE    _fld_swap, _c_fld_swap
        GLUE    _fld_hspan, _c_fld_hspan
        GLUE    _fld_vrun, _c_fld_vrun
        GLUE    _fld_col, _c_fld_col
        GLUE    _fld_cols, _c_fld_cols
        GLUE    _fld_ellipse, _c_fld_ellipse
