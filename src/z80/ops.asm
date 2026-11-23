; ops.asm -- the script's paths (OP_PATH), flows (OP_FLOW) and beams
; (OP_BEAM) on the field (docs/PROTOCOL.md): the CoCo's src/6809/ops.asm
; in Z80, 16-bit coordinates. Paths and flows read their bytes where they
; lie in script[]; at a segment's end they finish (ops_seg_end).
;
; ops_path, ops_flow, ops_beam   HL: the op's arguments; HL: the bytes taken
; ops_step                       a frame: A non-zero if a fire flow burns
; ops_seg_end                    paths stop, flows run out
; ops_reset                      all off (a new shot)
; C: ops_beams_off()             beams gone (the script's end)

        SECTION code_user

        INCLUDE "proto.inc"

        PUBLIC  ops_path, ops_flow, ops_beam, ops_step, ops_seg_end, ops_reset
        PUBLIC  _ops_beams_off
        EXTERN  _fld_swap, _fld_get, _fld_plot, _fx, _fy, _fv, _fold

        defc    NPATH = 4
        defc    NFLOW = 4
        defc    NBEAM = 2
        defc    PX_SKY = 1
        defc    PX_DIRT = 2
        defc    PX_FX = 3

; a path
        defc    PA_ON = 0
        defc    PA_STY = 1
        defc    PA_X = 2                ; 2
        defc    PA_Y = 4                ; 2
        defc    PA_N = 6                ; steps left
        defc    PA_DR = 7               ; its head is drawn
        defc    PA_P = 8                ; 2: its next step
        defc    PA_SIZE = 10
; a flow
        defc    F_ON = 0
        defc    F_KIND = 1
        defc    F_FRM = 2               ; frames to spread over
        defc    F_F = 3                 ; frames so far
        defc    F_HOLD = 4
        defc    F_N = 5
        defc    F_SH = 6                ; cells shown
        defc    F_ACC = 7               ; 2: n x frames so far, less frames x shown
        defc    F_P = 9                 ; 2: its cells (x16, y: 3 bytes each)
        defc    F_SIZE = 11
; a beam
        defc    BE_LEFT = 0
        defc    BE_X0 = 1               ; 2
        defc    BE_Y0 = 3
        defc    BE_X1 = 5
        defc    BE_Y1 = 7
        defc    BE_SIZE = 9

; void ops_beams_off(void)
_ops_beams_off:
        push    ix
        push    iy
        call    ops_beams_off
        pop     iy
        pop     ix
        ret

; xyat: fx, fy = the words at IX + A (x) and IX + A + 2 (y)
xyat:   push    ix
        pop     hl
        call    add_hl_a
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      (_fx),de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      (_fy),de
        ret

add_hl_a:
        add     a,l
        ld      l,a
        ret     nc
        inc     h
        ret

; ---- paths ----------------------------------------------------------------------

; phead: path IX's head, drawn: a roller over air, a digger over air or dirt
phead:  ld      (ix+PA_DR),0
        ld      a,PA_X
        call    xyat
        call    _fld_get
        cp      PX_SKY
        jr      z,ph_draw
        cp      PX_DIRT
        ret     nz
        ld      a,(ix+PA_STY)
        cp      ST_DIGGER
        ret     nz
ph_draw:
        ld      a,PX_FX
        ld      (_fv),a
        call    _fld_plot
        inc     (ix+PA_DR)
        ret

; pleave: path IX's head moves on (a digger has bored: air behind it)
pleave: ld      a,(ix+PA_DR)
        or      a
        ret     z
        ld      (ix+PA_DR),0
        ld      a,PA_X
        call    xyat
        ld      a,PX_FX
        ld      (_fold),a
        ld      a,PX_SKY
        ld      (_fv),a
        jp      _fld_swap

; pend: path IX stops
pend:   ld      a,(ix+PA_ON)
        or      a
        ret     z
        call    pleave
        ld      (ix+PA_ON),0
        ret

ops_path:
        ld      (optr),hl
        ld      a,(hl)          ; slot
        and     NPATH-1
        ld      b,a
        add     a,a
        add     a,a
        add     a,b
        add     a,a             ; * 10
        ld      ix,paths
        ld      e,a
        ld      d,0
        add     ix,de
        call    pend
        ld      hl,(optr)
        inc     hl
        ld      a,(hl)
        ld      (ix+PA_STY),a
        inc     hl
        ld      a,(hl)
        ld      (ix+PA_X),a
        inc     hl
        ld      a,(hl)
        ld      (ix+PA_X+1),a
        inc     hl
        ld      a,(hl)
        ld      (ix+PA_Y),a
        inc     hl
        ld      a,(hl)
        ld      (ix+PA_Y+1),a
        inc     hl
        ld      a,(hl)
        ld      (ix+PA_N),a
        inc     hl
        ld      (ix+PA_P),l
        ld      (ix+PA_P+1),h
        ld      (ix+PA_ON),1
        call    phead
        ld      l,(ix+PA_N)
        ld      h,0
        ld      de,7
        add     hl,de
        ret

pstep:  ld      ix,paths+(NPATH-1)*PA_SIZE
        ld      a,NPATH
        ld      (oi),a
pt_l:   ld      a,(ix+PA_ON)
        or      a
        jr      z,pt_n
        ld      a,(ix+PA_N)
        or      a
        jr      nz,pt_mv
        call    pend
        jr      pt_n
pt_mv:  call    pleave
        dec     (ix+PA_N)
        ld      l,(ix+PA_P)
        ld      h,(ix+PA_P+1)
        ld      b,(hl)
        inc     hl
        ld      (ix+PA_P),l
        ld      (ix+PA_P+1),h
        ld      a,b             ; x += the high nibble, signed
        sra     a
        sra     a
        sra     a
        sra     a
        call    sext
        ld      l,(ix+PA_X)
        ld      h,(ix+PA_X+1)
        add     hl,de
        ld      (ix+PA_X),l
        ld      (ix+PA_X+1),h
        ld      a,b             ; y += the low nibble, signed
        add     a,a
        add     a,a
        add     a,a
        add     a,a
        sra     a
        sra     a
        sra     a
        sra     a
        call    sext
        ld      l,(ix+PA_Y)
        ld      h,(ix+PA_Y+1)
        add     hl,de
        ld      (ix+PA_Y),l
        ld      (ix+PA_Y+1),h
        call    phead
pt_n:   ld      de,-PA_SIZE
        add     ix,de
        ld      hl,oi
        dec     (hl)
        jr      nz,pt_l
        ret

; sext: DE = A sign-extended
sext:   ld      e,a
        add     a,a
        sbc     a,a
        ld      d,a
        ret

; ---- flows -----------------------------------------------------------------------

; fcells: flow orec's cells ocnt to oto-1, fold -> fv (and, mud, fire -> fv)
fcells: ld      a,(ocnt)
        ld      b,a
        ld      a,(oto)
        cp      b
        ret     z
        ret     c
        ld      l,b             ; its cell: F_P + 3 ocnt
        ld      h,0
        ld      e,l
        ld      d,h
        add     hl,hl
        add     hl,de
        ld      e,(ix+F_P)
        ld      d,(ix+F_P+1)
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      (_fx),de
        ld      e,(hl)
        ld      d,0
        ld      (_fy),de
        call    _fld_swap
        ld      a,(omud)
        or      a
        jr      z,fc_n
        ld      a,(_fold)
        push    af
        ld      a,PX_FX
        ld      (_fold),a
        call    _fld_swap
        pop     af
        ld      (_fold),a
fc_n:   ld      hl,ocnt
        inc     (hl)
        jr      nz,fcells
        ret

; fshow: flow IX's first A cells in (fire over the air, mud as dirt)
fshow:  cp      (ix+F_SH)
        ret     z
        ret     c
        ld      (oto),a
        ld      b,(ix+F_SH)
        ld      (ix+F_SH),a
        ld      a,b
        ld      (ocnt),a
        ld      a,PX_SKY
        ld      (_fold),a
        xor     a
        ld      (omud),a
        ld      a,(ix+F_KIND)
        cp      FLOW_MUD
        ld      a,PX_FX
        jr      nz,fs_f
        ld      a,1
        ld      (omud),a
        ld      a,PX_DIRT
fs_f:   ld      (_fv),a
        jr      fcells

; fend: flow IX all in, then a fire goes out
fend:   ld      a,(ix+F_N)
        call    fshow
        ld      a,(ix+F_KIND)
        cp      FLOW_MUD
        jr      z,fe_off
        xor     a
        ld      (ocnt),a
        ld      (omud),a
        ld      a,(ix+F_N)
        ld      (oto),a
        ld      a,PX_FX
        ld      (_fold),a
        ld      a,PX_SKY
        ld      (_fv),a
        call    fcells
fe_off: ld      (ix+F_ON),0
        ret

ops_flow:
        ld      (optr),hl
        ld      ix,flows        ; the first free one,
        ld      b,NFLOW-1
of_free:
        ld      a,(ix+F_ON)
        or      a
        jr      z,of_got
        ld      de,F_SIZE
        add     ix,de
        djnz    of_free
        ld      a,(ix+F_ON)
        or      a
        call    nz,fend         ;  or the last, ended now
of_got: ld      hl,(optr)
        ld      a,(hl)
        ld      (ix+F_KIND),a
        inc     hl
        ld      a,(hl)
        or      a
        jr      nz,of_fr
        inc     a
of_fr:  ld      (ix+F_FRM),a
        inc     hl
        ld      a,(hl)
        ld      (ix+F_HOLD),a
        inc     hl
        ld      a,(hl)
        ld      (ix+F_N),a
        inc     hl
        ld      (ix+F_F),0
        ld      (ix+F_SH),0
        ld      (ix+F_ACC),0
        ld      (ix+F_ACC+1),0
        ld      (ix+F_P),l
        ld      (ix+F_P+1),h
        ld      (ix+F_ON),1
        ld      l,(ix+F_N)      ; took 4 + 3n
        ld      h,0
        ld      e,l
        ld      d,h
        add     hl,hl
        add     hl,de
        ld      de,4
        add     hl,de
        ret

fstep:  ld      ix,flows+(NFLOW-1)*F_SIZE
        ld      a,NFLOW
        ld      (oi),a
fl_l:   ld      a,(ix+F_ON)
        or      a
        jr      z,fl_n
        ld      a,(ix+F_KIND)
        cp      FLOW_MUD
        jr      z,fl_m
        ld      a,1
        ld      (ofire),a
fl_m:   ld      a,(ix+F_F)
        cp      (ix+F_FRM)
        jr      nc,fl_hold
        inc     (ix+F_F)
        ld      l,(ix+F_ACC)    ; acc += n; each frames of it: a cell more
        ld      h,(ix+F_ACC+1)
        ld      e,(ix+F_N)
        ld      d,0
        add     hl,de
        ld      a,(ix+F_SH)
        ld      (ocnt),a
        ld      e,(ix+F_FRM)
fl_sub: or      a
        sbc     hl,de
        jr      c,fl_sh
        ld      (ix+F_ACC),l
        ld      (ix+F_ACC+1),h
        ld      a,(ocnt)
        inc     a
        ld      (ocnt),a
        jr      nz,fl_sub
        jr      fl_sh2
fl_sh:  add     hl,de           ; (the last one didn't go)
        ld      (ix+F_ACC),l
        ld      (ix+F_ACC+1),h
fl_sh2: ld      a,(ocnt)
        call    fshow
        jr      fl_end
fl_hold:
        ld      a,(ix+F_HOLD)
        or      a
        jr      z,fl_end
        dec     (ix+F_HOLD)
fl_end: ld      a,(ix+F_F)
        cp      (ix+F_FRM)
        jr      c,fl_n
        ld      a,(ix+F_HOLD)
        or      a
        call    z,fend
fl_n:   ld      de,-F_SIZE
        add     ix,de
        ld      hl,oi
        dec     (hl)
        jr      nz,fl_l
        ret

; ---- beams ----------------------------------------------------------------------------

; lplot: (lx0, ly0): on, fire over air and dirt; off, fire back to air
lplot:  ld      hl,(lx0)
        ld      (_fx),hl
        ld      hl,(ly0)
        ld      (_fy),hl
        ld      a,(lon)
        or      a
        jr      z,lp_off
        ld      a,PX_FX
        ld      (_fv),a
        ld      a,PX_SKY
        ld      (_fold),a
        call    _fld_swap
        ld      a,PX_DIRT
        ld      (_fold),a
        jp      _fld_swap
lp_off: ld      a,PX_FX
        ld      (_fold),a
        ld      a,PX_SKY
        ld      (_fv),a
        jp      _fld_swap

; bline: beam IX's line (Bresenham, as the reference view draws it)
bline:  ld      l,(ix+BE_X0)
        ld      h,(ix+BE_X0+1)
        ld      (lx0),hl
        ld      l,(ix+BE_Y0)
        ld      h,(ix+BE_Y0+1)
        ld      (ly0),hl
        ld      l,(ix+BE_X1)
        ld      h,(ix+BE_X1+1)
        ld      (lx1),hl
        ld      l,(ix+BE_Y1)
        ld      h,(ix+BE_Y1+1)
        ld      (ly1),hl
        ld      hl,(lx1)        ; dx = |x1 - x0|, sx
        ld      de,(lx0)
        ld      bc,1
        or      a
        sbc     hl,de
        jp      p,bl_px
        call    neg_hl
        ld      bc,-1
bl_px:  ld      (ldx),hl
        ld      (lsx),bc
        ld      hl,(ly1)        ; dy, sy
        ld      de,(ly0)
        ld      bc,1
        or      a
        sbc     hl,de
        jp      p,bl_py
        call    neg_hl
        ld      bc,-1
bl_py:  ld      (ldy),hl
        ld      (lsy),bc
        ld      hl,(ldx)        ; err = dx - dy
        ld      de,(ldy)
        or      a
        sbc     hl,de
        ld      (lerr),hl
bl_l:   call    lplot
        ld      hl,(lx0)
        ld      de,(lx1)
        or      a
        sbc     hl,de
        jr      nz,bl_go
        ld      hl,(ly0)
        ld      de,(ly1)
        or      a
        sbc     hl,de
        ret     z
bl_go:  ld      hl,(lerr)       ; e2 = 2 err
        add     hl,hl
        ld      (le2),hl
        ld      de,(ldy)        ; e2 > -dy: x moves
        add     hl,de
        bit     7,h
        jr      nz,bl_ny
        ld      a,h
        or      l
        jr      z,bl_ny
        ld      hl,(lerr)
        ld      de,(ldy)
        or      a
        sbc     hl,de
        ld      (lerr),hl
        ld      hl,(lx0)
        ld      de,(lsx)
        add     hl,de
        ld      (lx0),hl
bl_ny:  ld      hl,(le2)        ; e2 < dx: y moves
        ld      de,(ldx)
        or      a
        sbc     hl,de
        bit     7,h
        jr      z,bl_l
        ld      hl,(lerr)
        ld      de,(ldx)
        add     hl,de
        ld      (lerr),hl
        ld      hl,(ly0)
        ld      de,(lsy)
        add     hl,de
        ld      (ly0),hl
        jr      bl_l

neg_hl: ld      a,h
        cpl
        ld      h,a
        ld      a,l
        cpl
        ld      l,a
        inc     hl
        ret

; boff: beam IX's pixels back to air, and it is gone
boff:   ld      a,(ix+BE_LEFT)
        or      a
        ret     z
        ld      (ix+BE_LEFT),0
        xor     a
        ld      (lon),a
        jp      bline

ops_beam:
        ld      (optr),hl
        ld      ix,beams        ; the first, unless it's live
        ld      a,(ix+BE_LEFT)
        or      a
        jr      z,ob_s
        ld      de,BE_SIZE
        add     ix,de
ob_s:   call    boff
        ld      hl,(optr)
        ld      a,(hl)
        ld      (ix+BE_X0),a
        inc     hl
        ld      a,(hl)
        ld      (ix+BE_X0+1),a
        inc     hl
        ld      a,(hl)
        ld      (ix+BE_Y0),a
        inc     hl
        ld      a,(hl)
        ld      (ix+BE_Y0+1),a
        inc     hl
        ld      a,(hl)
        ld      (ix+BE_X1),a
        inc     hl
        ld      a,(hl)
        ld      (ix+BE_X1+1),a
        inc     hl
        ld      a,(hl)
        ld      (ix+BE_Y1),a
        inc     hl
        ld      a,(hl)
        ld      (ix+BE_Y1+1),a
        inc     hl
        inc     hl
        ld      a,(hl)          ; (byte 9: its frames)
        or      a
        jr      nz,ob_f
        inc     a
ob_f:   ld      (ix+BE_LEFT),a
        ld      a,1
        ld      (lon),a
        call    bline
        ld      hl,10
        ret

bstep:  ld      ix,beams+(NBEAM-1)*BE_SIZE
        ld      a,NBEAM
        ld      (oi),a
bs_l:   ld      a,(ix+BE_LEFT)
        or      a
        jr      z,bs_n
        dec     (ix+BE_LEFT)
        jr      nz,bs_n
        inc     (ix+BE_LEFT)    ; boff wants it live
        call    boff
bs_n:   ld      de,-BE_SIZE
        add     ix,de
        ld      hl,oi
        dec     (hl)
        jr      nz,bs_l
        ret

; ---- a frame, a segment's end, a new shot -------------------------------------------

ops_step:
        xor     a
        ld      (ofire),a
        call    pstep
        call    fstep
        call    bstep
        ld      a,(ofire)
        ret

ops_seg_end:
        ld      ix,paths+(NPATH-1)*PA_SIZE
        ld      a,NPATH
        ld      (oi),a
se_p:   call    pend
        ld      de,-PA_SIZE
        add     ix,de
        ld      hl,oi
        dec     (hl)
        jr      nz,se_p
        ld      ix,flows+(NFLOW-1)*F_SIZE
        ld      a,NFLOW
        ld      (oi),a
se_f:   ld      a,(ix+F_ON)
        or      a
        call    nz,fend
        ld      de,-F_SIZE
        add     ix,de
        ld      hl,oi
        dec     (hl)
        jr      nz,se_f
        ret

ops_beams_off:
        ld      ix,beams+(NBEAM-1)*BE_SIZE
        ld      a,NBEAM
        ld      (oi),a
bo_l:   call    boff
        ld      de,-BE_SIZE
        add     ix,de
        ld      hl,oi
        dec     (hl)
        jr      nz,bo_l
        ret

ops_reset:
        ld      hl,data
        ld      de,data+1
        ld      bc,data_end-data-1
        ld      (hl),0
        ldir
        ret

        SECTION bss_user
optr:   defs    2
oi:     defs    1
ocnt:   defs    1
oto:    defs    1
omud:   defs    1
ofire:  defs    1
lx0:    defs    2
ly0:    defs    2
lx1:    defs    2
ly1:    defs    2
ldx:    defs    2
ldy:    defs    2
lsx:    defs    2
lsy:    defs    2
lerr:   defs    2
le2:    defs    2
lon:    defs    1
data:
paths:  defs    NPATH * PA_SIZE
flows:  defs    NFLOW * F_SIZE
beams:  defs    NBEAM * BE_SIZE
data_end:
