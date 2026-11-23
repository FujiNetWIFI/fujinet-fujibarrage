; pt.asm -- sparks: pixels flying from a tank going up, or off its shield
; (the CoCo's src/6809/pt.asm in Z80, from the Apple's). Each is a fire
; pixel on the sky, moved every frame (x 16.8, y 8.8 fixed point, little-
; endian) and put back to sky when it moves on or dies. It only ever draws
; on sky, so it changes nothing the server knows (refview never sees them).
;
; C: field_sparks(uint16_t a)   low byte the seat, high the kind: 0-7 a
;                               death's effect (DEATH_*), 8 + a shield's
;                               event (SH_*)
; pt_step   a frame (ply.asm's anim): A nonzero while any burn (they glow
;           in the fire colours)
; pt_clear  all gone (a segment's end, a shot's start)

        SECTION code_user

        PUBLIC  _field_sparks, pt_step, pt_clear
        EXTERN  _tk, _fld_get, _fld_plot, _fld_swap, fld_w, fld_h, fld_mid
        EXTERN  _fx, _fy, _fv, _fold

        defc    NP = 16
        defc    PX_SKY = 1
        defc    PX_FX = 3

; a spark
        defc    SP_X = 0                ; 3: x 16.8 (fraction, low, high)
        defc    SP_Y = 3                ; 2: y 8.8 (fraction, line)
        defc    SP_VX = 5               ; 2: 8.8, signed
        defc    SP_VY = 7               ; 2
        defc    SP_LIFE = 9
        defc    SP_GRAV = 10
        defc    SP_SH = 11              ; it shows, at
        defc    SP_OX = 12              ; 2
        defc    SP_OY = 14
        defc    SP_SIZE = 15

; a kind's parameters (par, 9 bytes each)
        defc    P_N = 0                 ; how many
        defc    P_FIRST = 1             ; the first direction (0 right, 4 down, 12 up)
        defc    P_STEP = 2              ; 1: one direction after another
        defc    P_MASK = 3              ; a random spread added
        defc    P_K = 4                 ; frames moved before it shows (at 2 px a frame)
        defc    P_SH = 5                ; then its speed halved this often
        defc    P_SJ = 6                ; and once more at random (1) or not (0)
        defc    P_G = 7                 ; gravity, 1/256 px a frame per frame
        defc    P_LIFE = 8              ; frames (plus 0-7 at random)

cos:    defb    32, 30, 23, 12, 0, -12, -23, -30
        defb    -32, -30, -23, -12, 0, 12, 23, 30
        defc    RAYS = 0*9
        defc    SPRAY = 1*9
        defc    PLUME = 2*9
        defc    SCATTER = 3*9
        defc    DEBRIS = 4*9
        defc    RING = 5*9
        defc    BURST = 6*9
kmap:   defb    PLUME, DEBRIS, DEBRIS, DEBRIS, RAYS, SPRAY, PLUME, SCATTER
        defb    RING, BURST, RING, RING, RING   ; hit, break, deflect, up, lift
;               n   1st step mask k  sh sj g   life
par:    defb    16, 0, 1, 0, 2, 1, 0, 0, 14     ; rays
        defb    12, 9, 0, 7, 1, 0, 1, 20, 30    ; spray, up and over
        defb    10, 11, 0, 2, 0, 3, 1, 0, 34    ; plume, rising slowly
        defb    14, 0, 0, 15, 1, 0, 1, 16, 30   ; scatter, all ways
        defb    8, 9, 0, 7, 1, 1, 1, 20, 24     ; debris
        defb    16, 0, 1, 0, 3, 4, 0, 0, 8      ; ring: a dome round it
        defb    16, 0, 1, 0, 3, 0, 0, 0, 8      ; burst

; void field_sparks(uint16_t a)
_field_sparks:
        ld      hl,2
        add     hl,sp
        ld      b,(hl)          ; the seat
        inc     hl
        ld      a,(hl)          ; the kind
        push    ix
        push    iy
        call    sparks
        pop     iy
        pop     ix
        ret

; kpar: IY = the kind's parameters
kpar:   ld      a,(pkp)
        ld      e,a
        ld      d,0
        ld      iy,par
        add     iy,de
        ret

; sparks: kind A from seat B's tank
sparks: ld      (pkind),a
        cp      13
        ret     nc
        ld      a,b             ; the tank: tk + 5 seat
        add     a,a
        add     a,a
        add     a,b
        ld      e,a
        ld      d,0
        ld      hl,_tk+1
        add     hl,de
        ld      e,(hl)          ; x
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      (pcx),de
        ld      a,(fld_mid)     ; y, its bottom: its middle is up from there
        ld      b,a
        ld      a,(hl)
        sub     b
        ld      (pcy),a
        ld      a,(pkind)
        ld      e,a
        ld      d,0
        ld      hl,kmap
        add     hl,de
        ld      a,(hl)
        ld      (pkp),a
        call    kpar
        ld      a,(iy+P_N)
        ld      (pleft),a
        xor     a
        ld      (pi),a
sp_one: ld      ix,sparks_+(NP-1)*SP_SIZE   ; a free one
        ld      b,NP
sp_f:   ld      a,(ix+SP_LIFE)
        or      a
        jr      z,sp_got
        ld      de,-SP_SIZE
        add     ix,de
        djnz    sp_f
        ret
sp_got: call    kpar
        call    random          ; its direction
        and     (iy+P_MASK)
        add     a,(iy+P_FIRST)
        ld      b,a
        ld      a,(iy+P_STEP)
        or      a
        ld      a,b
        jr      z,sp_dir
        ld      hl,pi
        add     a,(hl)
sp_dir: and     15
        ld      (pd),a
        call    cos_a
        call    v16
        ld      (ix+SP_VX),l
        ld      (ix+SP_VX+1),h
        ld      a,(pd)
        add     a,12
        and     15
        call    cos_a
        call    v16
        ld      (ix+SP_VY),l
        ld      (ix+SP_VY+1),h
        ld      hl,(pcx)        ; from the middle (and a half)
        ld      (ix+SP_X),$80
        ld      (ix+SP_X+1),l
        ld      (ix+SP_X+2),h
        ld      a,(pcy)
        ld      (ix+SP_Y),$80
        ld      (ix+SP_Y+1),a
        call    kpar
        ld      a,(iy+P_K)      ; out from the middle
        or      a
        jr      z,sp_sp
        ld      (pd),a
sp_k:   call    move
        ld      hl,pd
        dec     (hl)
        jr      nz,sp_k
sp_sp:  call    kpar            ; then slower
        call    random
        and     (iy+P_SJ)
        add     a,(iy+P_SH)
        jr      z,sp_g
        ld      (pd),a
sp_sh:  sra     (ix+SP_VX+1)
        rr      (ix+SP_VX)
        sra     (ix+SP_VY+1)
        rr      (ix+SP_VY)
        ld      hl,pd
        dec     (hl)
        jr      nz,sp_sh
sp_g:   call    kpar
        ld      a,(iy+P_G)
        ld      (ix+SP_GRAV),a
        call    random
        and     7
        add     a,(iy+P_LIFE)
        ld      (ix+SP_LIFE),a
        ld      (ix+SP_SH),0
        ld      hl,pi
        inc     (hl)
        ld      hl,pleft
        dec     (hl)
        jp      nz,sp_one
        ret

; cos_a: A = cos[A]
cos_a:  ld      e,a
        ld      d,0
        ld      hl,cos
        add     hl,de
        ld      a,(hl)
        ret

; v16: A (1/16 px, signed) as HL (1/256 px)
v16:    ld      l,a
        add     a,a
        sbc     a,a
        ld      h,a
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ret

pt_step:
        xor     a
        ld      (plive),a
        ld      ix,sparks_+(NP-1)*SP_SIZE
        ld      a,NP
        ld      (pcnt),a
st_p:   ld      a,(ix+SP_LIFE)
        or      a
        jp      z,st_n
        ld      (plive),a
        call    erase
        dec     (ix+SP_LIFE)
        jp      z,st_n
        ld      a,(ix+SP_VY)    ; falling
        add     a,(ix+SP_GRAV)
        ld      (ix+SP_VY),a
        ld      a,(ix+SP_VY+1)
        adc     a,0
        ld      (ix+SP_VY+1),a
        call    move
        ld      a,(fld_h)
        ld      b,a
        ld      a,(ix+SP_Y+1)
        cp      b
        jr      nc,st_die
        ld      l,(ix+SP_X+1)
        ld      h,(ix+SP_X+2)
        ld      de,(fld_w)
        or      a
        sbc     hl,de
        jr      nc,st_die
        add     hl,de
        ld      (_fx),hl        ; on sky: fire
        ld      l,(ix+SP_Y+1)
        ld      h,0
        ld      (_fy),hl
        call    _fld_get
        cp      PX_SKY
        jr      nz,st_n
        ld      a,PX_FX
        ld      (_fv),a
        call    _fld_plot
        ld      hl,(_fx)
        ld      (ix+SP_OX),l
        ld      (ix+SP_OX+1),h
        ld      a,(ix+SP_Y+1)
        ld      (ix+SP_OY),a
        ld      (ix+SP_SH),1
        jr      st_n
st_die: ld      (ix+SP_LIFE),0
st_n:   ld      de,-SP_SIZE
        add     ix,de
        ld      hl,pcnt
        dec     (hl)
        jp      nz,st_p
        ld      a,(plive)
        ret

; move: spark IX a frame on (x += vx, sign-extended)
move:   ld      a,(ix+SP_VX+1)
        rla
        sbc     a,a
        ld      c,a
        ld      a,(ix+SP_X)
        add     a,(ix+SP_VX)
        ld      (ix+SP_X),a
        ld      a,(ix+SP_X+1)
        adc     a,(ix+SP_VX+1)
        ld      (ix+SP_X+1),a
        ld      a,(ix+SP_X+2)
        adc     a,c
        ld      (ix+SP_X+2),a
        ld      a,(ix+SP_Y)
        add     a,(ix+SP_VY)
        ld      (ix+SP_Y),a
        ld      a,(ix+SP_Y+1)
        adc     a,(ix+SP_VY+1)
        ld      (ix+SP_Y+1),a
        ret

; random: A = the next of an 8-bit LFSR (never 0)
random: ld      a,(pseed)
        or      a
        jr      nz,rn_go
        ld      a,$A5
rn_go:  add     a,a
        jr      nc,rn_n
        xor     $1D
rn_n:   ld      (pseed),a
        ret

; erase: spark IX's pixel back to sky, if it's still fire
erase:  ld      a,(ix+SP_SH)
        or      a
        ret     z
        ld      (ix+SP_SH),0
        ld      l,(ix+SP_OX)
        ld      h,(ix+SP_OX+1)
        ld      (_fx),hl
        ld      l,(ix+SP_OY)
        ld      h,0
        ld      (_fy),hl
        ld      a,PX_FX
        ld      (_fold),a
        ld      a,PX_SKY
        ld      (_fv),a
        jp      _fld_swap

pt_clear:
        ld      ix,sparks_+(NP-1)*SP_SIZE
        ld      a,NP
        ld      (pcnt),a
pc_c:   ld      a,(ix+SP_LIFE)
        or      a
        jr      z,pc_n
        call    erase
        ld      (ix+SP_LIFE),0
pc_n:   ld      de,-SP_SIZE
        add     ix,de
        ld      hl,pcnt
        dec     (hl)
        jr      nz,pc_c
        ret

        SECTION bss_user
pcx:    defs    2
pcy:    defs    1
pleft:  defs    1
pkp:    defs    1
pd:     defs    1
pi:     defs    1
pseed:  defs    1
plive:  defs    1
pcnt:   defs    1
pkind:  defs    1
sparks_: defs   NP * SP_SIZE
