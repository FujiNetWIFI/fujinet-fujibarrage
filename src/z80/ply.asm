; ply.asm -- the script player's timeline (docs/PROTOCOL.md "Playing a
; segment"): the CoCo's src/6809/ply.asm in Z80 (from the 6502's), each
; routine the same steps in the same order. Each frame every track, blast,
; path, flow, beam and settle moves a step, then the ops due run. The ops
; about tanks, talk, damage and sound go to C (c_op in play.c), which reads
; their arguments at ply_pc. Coordinates are 16-bit; the script's words
; are little-endian, as the Z80's are.
;
; void ply_reset(void)   a new shot: nothing moving
; void ply_step(void)    a frame
; void ply_draw(void)    the blasts at their size now
; void ply_end(void)     the segment's bytes are going: settles land, paths
;                        and flows finish (ops.asm)
; ply_pc, ply_wait, ply_done: where it is in script[], frames to wait, OP_END
;
; The field engine (src/tms9918/fld.asm) is called directly; it keeps IX
; and IY. The records are walked with IX; the C entries keep IX and IY
; for zsdcc, and so does each call back into C.

        SECTION code_user

        INCLUDE "proto.inc"

        PUBLIC  _ply_reset, _ply_step, _ply_draw, _ply_end
        PUBLIC  _ply_pc, _ply_wait, _ply_done
        EXTERN  _script, _seg_len, _st, _c_op, _c_anim, _field_fire, _shake_n
        EXTERN  trk_reset, trk_load, trk_stop, trk_step
        EXTERN  ops_path, ops_flow, ops_beam, ops_step, ops_seg_end, ops_reset
        EXTERN  pt_step, pt_clear
        EXTERN  _fld_ellipse, _fld_cols, _fld_ncols
        EXTERN  _fx, _fent, _ex, _ey, _erx, _ery, _fv, _emode

        defc    PX_SKY = 1
        defc    PX_DIRT = 2
        defc    PX_FX = 3
        defc    NBLAST = 10
        defc    NSETTLE = 8
        defc    DIRTBALL = $80          ; an OP_DIRT, kept with the blasts

; a blast
        defc    B_ON = 0
        defc    B_KIND = 1
        defc    B_N = 2
        defc    B_TOT = 3
        defc    B_RX = 4
        defc    B_RY = 5
        defc    B_DN = 6                ; the size last drawn
        defc    B_X = 7                 ; 2
        defc    B_Y = 9                 ; 2
        defc    B_SIZE = 11
; a settle
        defc    S_ON = 0
        defc    S_LEFT = 1
        defc    S_N = 2
        defc    S_X = 3                 ; 2
        defc    S_A = 5                 ; 2: its columns at script + this
        defc    S_SIZE = 7

_ply_reset:
        ld      hl,ply_reset
        jr      centry
_ply_step:
        ld      hl,ply_step
        jr      centry
_ply_draw:
        ld      hl,ply_draw
        jr      centry
_ply_end:
        ld      hl,ply_end
; centry: a C entry: IX and IY kept around HL's routine
centry: push    ix
        push    iy
        ld      de,ce_back
        push    de
        jp      (hl)
ce_back:
        pop     iy
        pop     ix
        ret

; setp: ply_p = script + ply_pc
setp:   ld      hl,(_ply_pc)
        ld      de,_script
        add     hl,de
        ld      (ply_p),hl
        ret

; adv: ply_pc += HL
adv:    ld      de,(_ply_pc)
        add     hl,de
        ld      (_ply_pc),hl
        ret

ply_reset:
        call    pt_clear
        call    trk_reset
        call    ops_reset
        ld      hl,0
        ld      (_ply_wait),hl
        ld      hl,tabs
        ld      de,tabs+1
        ld      bc,tabs_end-tabs-1
        ld      (hl),0
        ldir
        ld      a,1
        ld      (pfirst),a
        ret

; ---- a frame ------------------------------------------------------------------

; (the shot's first frame only runs its ops: view.go's Frame 0)
ply_step:
        ld      a,(pfirst)
        or      a
        call    z,anim
        xor     a
        ld      (pfirst),a
        ld      hl,(_ply_wait)
        ld      a,h
        or      l
        jr      z,ps_run
        dec     hl
        ld      (_ply_wait),hl
ps_run: ld      hl,(_ply_wait)
        ld      a,h
        or      l
        ret     nz
        ld      a,(_ply_done)
        or      a
        ret     nz
        ld      hl,(_ply_pc)
        ld      de,(_seg_len)
        or      a
        sbc     hl,de
        ret     nc
        call    op
        jr      ps_run

; ---- the ops ------------------------------------------------------------------

op:     call    setp
        ld      hl,(ply_p)
        ld      a,(hl)
        inc     hl
        ld      (ply_p),hl         ; its arguments
        push    af
        ld      hl,1
        call    adv
        pop     af              ; A: the op
        cp      OP_WAIT
        jr      z,o_wait
        cp      OP_END
        jr      z,o_end
        cp      OP_ARC
        jr      z,o_arc
        cp      OP_STOP
        jr      z,o_stop
        cp      OP_PATH
        jr      z,o_path
        cp      OP_FLOW
        jr      z,o_flow
        cp      OP_BEAM
        jr      z,o_beam
        cp      OP_EXPLODE
        jp      z,o_blast
        cp      OP_DIRT
        jp      z,o_blast
        cp      OP_SETTLE
        jp      z,o_settle
        cp      OP_WIND
        jr      z,o_wind
        cp      OP_SHAKE
        jr      z,o_shake
        cp      OP_TALK         ; 30h-3Ah: C's
        jr      c,o_end
        cp      OP_FIRE+1
        jr      nc,o_end
        push    ix              ; c_op(op)
        push    iy
        push    af
        inc     sp
        call    _c_op
        inc     sp
        pop     iy
        pop     ix
        ret

o_end:  ld      a,1             ; OP_END, or not ours: this segment is done
        ld      (_ply_done),a
        ret

o_wait: ld      hl,(ply_p)         ; n (0: 256)
        ld      l,(hl)
        ld      h,0
        ld      a,l
        or      a
        jr      nz,ow_1
        inc     h
ow_1:   ld      (_ply_wait),hl
        ld      hl,1
        jp      adv

o_arc:  ld      hl,(ply_p)
        call    trk_load
        ld      hl,18
        jp      adv

o_stop: ld      hl,(ply_p)
        ld      a,(hl)
        call    trk_stop
        ld      hl,1
        jp      adv

o_path: ld      hl,(ply_p)         ; (each says how many bytes it took)
        call    ops_path
        jp      adv

o_flow: ld      hl,(ply_p)
        call    ops_flow
        jp      adv

o_beam: ld      hl,(ply_p)
        call    ops_beam
        jp      adv

o_wind: ld      hl,(ply_p)         ; (the state's bytes as they come)
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      (_st+OFF_WIND),de
        ld      hl,2
        jp      adv

o_shake:
        ld      hl,(ply_p)
        ld      a,(hl)
        ld      (_shake_n),a
        ld      hl,1
        jp      adv

; EXPLODE: x16, y16, rx, ry, kind, frames; DIRT: the same without kind
o_blast:
        ld      (ob_op),a
        ld      ix,blasts       ; the first free one (or the last)
        ld      b,NBLAST-1
ob_f:   ld      a,(ix+B_ON)
        or      a
        jr      z,ob_got
        ld      de,B_SIZE
        add     ix,de
        djnz    ob_f
ob_got: ld      hl,(ply_p)
        ld      a,(hl)
        ld      (ix+B_X),a
        inc     hl
        ld      a,(hl)
        ld      (ix+B_X+1),a
        inc     hl
        ld      a,(hl)
        ld      (ix+B_Y),a
        inc     hl
        ld      a,(hl)
        ld      (ix+B_Y+1),a
        inc     hl
        ld      a,(hl)
        ld      (ix+B_RX),a
        inc     hl
        ld      a,(hl)
        ld      (ix+B_RY),a
        inc     hl
        ld      a,(ob_op)
        cp      OP_DIRT
        ld      a,DIRTBALL
        jr      z,ob_k
        ld      a,(hl)
        inc     hl
ob_k:   ld      (ix+B_KIND),a
        cp      EX_NUKE         ; a nuke shakes the screen
        jr      nz,ob_nn
        ld      a,30
        ld      (_shake_n),a
ob_nn:  ld      a,(hl)
        inc     hl
        or      a
        jr      nz,ob_t
        inc     a
ob_t:   ld      (ix+B_TOT),a
        ld      (ix+B_ON),1
        ld      (ix+B_N),0
        ld      (ix+B_DN),0
        ld      de,(ply_p)
        or      a
        sbc     hl,de
        jp      adv

; SETTLE: x16, n, frames, n column entries
o_settle:
        ld      ix,settles
        ld      b,NSETTLE
os_f:   ld      a,(ix+S_ON)
        or      a
        jr      z,os_got
        ld      de,S_SIZE
        add     ix,de
        djnz    os_f
        ld      ix,settles      ; full: the oldest lands now
        call    settle
        call    setp
        ld      ix,settles
os_got: ld      hl,(ply_p)
        ld      a,(hl)
        ld      (ix+S_X),a
        inc     hl
        ld      a,(hl)
        ld      (ix+S_X+1),a
        inc     hl
        ld      a,(hl)
        ld      (ix+S_N),a
        inc     hl
        ld      a,(hl)
        ld      (ix+S_LEFT),a
        ld      hl,4
        call    adv
        ld      hl,(_ply_pc)    ; its columns start here
        ld      (ix+S_A),l
        ld      (ix+S_A+1),h
        ld      (ix+S_ON),1
        ld      a,(ix+S_N)      ; past them
        ld      (os_n),a
os_skip:
        ld      a,(os_n)
        or      a
        jr      z,os_now
        call    setp
        ld      hl,(ply_p)
        call    elen
        ld      l,a
        ld      h,0
        call    adv
        ld      hl,os_n
        dec     (hl)
        jr      os_skip
os_now: ld      a,(ix+S_LEFT)
        or      a
        ret     nz
        jr      settle

; elen: A = the length of the column entry at HL (1, or 1 + 2 a span)
elen:   ld      a,(hl)
        cp      T_SPANS+1
        jr      c,el_one
        sub     T_SPANS
        add     a,a
        inc     a
        ret
el_one: ld      a,1
        ret

; settle IX: its columns written (fld_cols)
settle: ld      l,(ix+S_A)
        ld      h,(ix+S_A+1)
        ld      de,_script
        add     hl,de
        ld      (_fent),hl
        ld      l,(ix+S_X)
        ld      h,(ix+S_X+1)
        ld      (_fx),hl
        ld      a,(ix+S_N)
        ld      (_fld_ncols),a
        call    _fld_cols
        ld      (ix+S_ON),0
        ret

; ---- a frame's step -------------------------------------------------------------

anim:   call    trk_step
        xor     a
        ld      (fire),a
        ld      ix,blasts+(NBLAST-1)*B_SIZE
        ld      a,NBLAST
        ld      (bi),a
an_b:   ld      a,(ix+B_ON)
        or      a
        jr      z,an_nb
        inc     (ix+B_N)
        ld      a,(ix+B_N)
        cp      (ix+B_TOT)
        jr      nc,an_end
        ld      a,1
        ld      (fire),a
        jr      an_nb
an_end: ld      a,(ix+B_RX)     ; done: the crater (or the dirt) now
        ld      b,(ix+B_RY)
        call    elargs
        ld      a,(ix+B_KIND)
        cp      DIRTBALL
        jr      nz,an_cr
        ld      a,PX_DIRT
        ld      b,1
        jr      an_go
an_cr:  cp      EX_PLASMA
        jr      z,an_fl
        cp      EX_PUFF
        jr      z,an_fl
        ld      a,PX_SKY
        ld      b,0
        jr      an_go
an_fl:  ld      a,PX_SKY
        ld      b,2
an_go:  ld      (_fv),a
        ld      a,b
        ld      (_emode),a
        call    _fld_ellipse
        ld      (ix+B_ON),0
an_nb:  ld      de,-B_SIZE
        add     ix,de
        ld      hl,bi
        dec     (hl)
        jr      nz,an_b
        call    ops_step        ; (A: a fire flow burns)
        ld      hl,fire
        or      (hl)
        ld      (hl),a
        ld      ix,settles+(NSETTLE-1)*S_SIZE
        ld      a,NSETTLE
        ld      (bi),a
an_s:   ld      a,(ix+S_ON)
        or      a
        jr      z,an_ns
        dec     (ix+S_LEFT)
        call    z,settle
an_ns:  ld      de,-S_SIZE
        add     ix,de
        ld      hl,bi
        dec     (hl)
        jr      nz,an_s
        call    pt_step         ; sparks glow like fire
        ld      hl,fire
        or      (hl)
        push    ix
        push    iy
        push    af
        inc     sp
        call    _field_fire
        inc     sp
        call    _c_anim
        pop     iy
        pop     ix
        ret

; elargs: the ellipse at blast IX, radii A, B (0 is 1; ry at most 200)
elargs: or      a
        jr      nz,ea_a
        inc     a
ea_a:   ld      (_erx),a
        ld      a,b
        or      a
        jr      nz,ea_b
        inc     a
ea_b:   cp      201
        jr      c,ea_c
        ld      a,200
ea_c:   ld      (_ery),a
        ld      l,(ix+B_X)
        ld      h,(ix+B_X+1)
        ld      (_ex),hl
        ld      l,(ix+B_Y)
        ld      h,(ix+B_Y+1)
        ld      (_ey),hl
        ret

; ---- the blasts drawn at their size now --------------------------------------------

ply_draw:
        ld      ix,blasts+(NBLAST-1)*B_SIZE
        ld      a,NBLAST
        ld      (bi),a
pd_b:   ld      a,(ix+B_ON)
        or      a
        jr      z,pd_nb
        ld      a,(ix+B_N)
        cp      (ix+B_DN)
        jr      z,pd_nb
        ld      (ix+B_DN),a
        ld      a,(ix+B_RX)     ; rx n / total, ry n / total
        call    scale
        push    af
        ld      a,(ix+B_RY)
        call    scale
        ld      b,a
        pop     af
        call    elargs
        ld      a,(ix+B_KIND)
        cp      DIRTBALL
        jr      nz,pd_fi
        ld      a,PX_DIRT
        ld      b,1
        jr      pd_go
pd_fi:  ld      b,1             ; plasma, a puff: over the air only
        cp      EX_PLASMA
        jr      z,pd_f
        cp      EX_PUFF
        jr      z,pd_f
        ld      b,0             ; a crater's fireball eats the dirt
pd_f:   ld      a,PX_FX
pd_go:  ld      (_fv),a
        ld      a,b
        ld      (_emode),a
        call    _fld_ellipse
pd_nb:  ld      de,-B_SIZE
        add     ix,de
        ld      hl,bi
        dec     (hl)
        jr      nz,pd_b
        ret

; scale: A = A * n / total of blast IX (at most A: n <= total)
scale:  ld      e,(ix+B_N)
        ld      hl,0
        ld      d,0
        ld      b,8
sc_m:   add     hl,hl           ; HL = A * n
        rlca
        jr      nc,sc_mn
        add     hl,de
sc_mn:  djnz    sc_m
        ld      c,(ix+B_TOT)    ; / total: the high byte is below it
        ld      a,h
        ld      b,8
sc_d:   sla     l
        rla
        jr      c,sc_sub
        cp      c
        jr      c,sc_dn
sc_sub: sub     c
        inc     l
sc_dn:  djnz    sc_d
        ld      a,l
        ret

; ---- a segment's end ------------------------------------------------------------------

ply_end:
        call    pt_clear
        ld      ix,settles+(NSETTLE-1)*S_SIZE
        ld      a,NSETTLE
        ld      (bi),a
pe_s:   ld      a,(ix+S_ON)
        or      a
        call    nz,settle
        ld      de,-S_SIZE
        add     ix,de
        ld      hl,bi
        dec     (hl)
        jr      nz,pe_s
        jp      ops_seg_end

        SECTION bss_user
_ply_pc:        defs    2
_ply_wait:      defs    2
_ply_done:      defs    1
ply_p:           defs    2
fire:           defs    1
pfirst:         defs    1
bi:             defs    1
ob_op:          defs    1
os_n:           defs    1
tabs:
blasts:         defs    NBLAST * B_SIZE
settles:        defs    NSETTLE * S_SIZE
tabs_end:
