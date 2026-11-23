; ply.asm -- the script player's timeline (docs/PROTOCOL.md "Playing a
; segment"): the Z80's src/z80/ply.asm in 8086 (the CoCo's, from the 6502's),
; each routine the same steps in the same order. Each frame every track,
; blast, path, flow, beam and settle moves a step, then the ops due run.
; The ops about tanks, talk, damage and sound go to C (c_op in play.c),
; which reads their arguments at ply_pc. Coordinates are 16-bit; the
; script's words are little-endian, as the 8086's are.
;
; void ply_reset(void)   a new shot: nothing moving
; void ply_step(void)    a frame
; void ply_draw(void)    the blasts at their size now
; void ply_end(void)     the segment's bytes are going: settles land, paths
;                        and flows finish (ops.asm)
; ply_pc, ply_wait, ply_done: where it is in script[], frames to wait, OP_END
;
; The field engine (each screen's fld.asm) is called directly; it keeps SI,
; DI and BP (the Z80's IX and IY). The records are walked with SI; the C
; entries keep BX, CX, DX, SI, DI, BP and ES for Watcom (the Z80's centry),
; and so does each C function called from here.
;
; Small model, near; DS = SS = DGROUP. Open Watcom wasm, .8086. Internal
; routines pass their argument in AX (the Z80's HL) or AL (its A).
;
        .8086

        INCLUDE proto.inc

PX_SKY  EQU     1
PX_DIRT EQU     2
PX_FX   EQU     3
NBLAST  EQU     10
NSETTLE EQU     8
DIRTBALL EQU    80h                     ; an OP_DIRT, kept with the blasts

; a blast
B_ON    EQU     0
B_KIND  EQU     1
B_N     EQU     2
B_TOT   EQU     3
B_RX    EQU     4
B_RY    EQU     5
B_DN    EQU     6                       ; the size last drawn
B_X     EQU     7                       ; 2
B_Y     EQU     9                       ; 2
B_SIZE  EQU     11
; a settle
S_ON    EQU     0
S_LEFT  EQU     1
S_N     EQU     2
S_X     EQU     3                       ; 2
S_A     EQU     5                       ; 2: its columns at script + this
S_SIZE  EQU     7

DGROUP  GROUP   _DATA, _BSS

_DATA   SEGMENT WORD PUBLIC 'DATA'
_DATA   ENDS

; the state (zero at start: the C start-up clears BSS)
_BSS    SEGMENT WORD PUBLIC 'BSS'
        PUBLIC  _ply_pc, _ply_wait, _ply_done
_ply_pc         DW      ?
_ply_wait       DW      ?
_ply_done       DB      ?
ply_p           DW      ?
fire            DB      ?
pfirst          DB      ?
bi              DB      ?
ob_op           DB      ?
os_n            DB      ?
tabs            LABEL   BYTE
blasts          DB      NBLAST * B_SIZE DUP (?)
settles         DB      NSETTLE * S_SIZE DUP (?)
tabs_end        LABEL   BYTE
_BSS    ENDS

        EXTRN   _script:BYTE
        EXTRN   _seg_len:WORD
        EXTRN   _st:BYTE
        EXTRN   _shake_n:BYTE
        EXTRN   c_op_:NEAR
        EXTRN   c_anim_:NEAR
        EXTRN   field_fire_:NEAR
        EXTRN   trk_reset:NEAR, trk_load:NEAR, trk_stop:NEAR, trk_step:NEAR
        EXTRN   ops_path:NEAR, ops_flow:NEAR, ops_beam:NEAR, ops_step:NEAR
        EXTRN   ops_seg_end:NEAR, ops_reset:NEAR
        EXTRN   pt_step:NEAR, pt_clear:NEAR
        EXTRN   fld_ellipse_:NEAR, fld_cols_:NEAR
        EXTRN   _fld_ncols:BYTE
        EXTRN   _fx:WORD, _fent:WORD, _ex:WORD, _ey:WORD
        EXTRN   _erx:BYTE, _ery:BYTE, _fv:BYTE, _emode:BYTE

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

        PUBLIC  ply_reset_, ply_step_, ply_draw_, ply_end_

ply_reset_:
        CENTRY  ply_reset
ply_step_:
        CENTRY  ply_step
ply_draw_:
        CENTRY  ply_draw
ply_end_:
        CENTRY  ply_end

; ply_setp: ply_p = script + ply_pc
ply_setp:   mov     ax, _ply_pc
        add     ax, OFFSET DGROUP:_script
        mov     ply_p, ax
        ret

; adv: ply_pc += AX
adv:    add     _ply_pc, ax
        ret

ply_reset:
        call    pt_clear
        call    trk_reset
        call    ops_reset
        xor     ax, ax
        mov     _ply_wait, ax
        push    ds
        pop     es
        mov     di, OFFSET DGROUP:tabs
        mov     cx, NBLAST * B_SIZE + NSETTLE * S_SIZE
        cld
        rep     stosb                   ; (AL = 0)
        mov     pfirst, 1
        ret

; ---- a frame ------------------------------------------------------------------

; (the shot's first frame only runs its ops: view.go's Frame 0)
ply_step:
        cmp     pfirst, 0
        jne     ps_nf
        call    anim
ps_nf:  mov     pfirst, 0
        mov     ax, _ply_wait
        or      ax, ax
        jz      ps_run
        dec     ax
        mov     _ply_wait, ax
ps_run: mov     ax, _ply_wait
        or      ax, ax
        jnz     ps_ret
        cmp     _ply_done, 0
        jne     ps_ret
        mov     ax, _ply_pc
        cmp     ax, _seg_len
        jae     ps_ret
        call    ply_op
        jmp     ps_run
ps_ret: ret

; ---- the ops ------------------------------------------------------------------

ply_op: call    ply_setp
        mov     bx, ply_p
        mov     al, [bx]
        inc     bx
        mov     ply_p, bx               ; its arguments
        inc     _ply_pc                 ; (adv 1)
                                        ; AL: the op
        cmp     al, OP_WAIT
        jne     op_1
        jmp     o_wait
op_1:   cmp     al, OP_END
        jne     op_2
        jmp     o_end
op_2:   cmp     al, OP_ARC
        jne     op_3
        jmp     o_arc
op_3:   cmp     al, OP_STOP
        jne     op_4
        jmp     o_stop
op_4:   cmp     al, OP_PATH
        jne     op_5
        jmp     o_path
op_5:   cmp     al, OP_FLOW
        jne     op_6
        jmp     o_flow
op_6:   cmp     al, OP_BEAM
        jne     op_7
        jmp     o_beam
op_7:   cmp     al, OP_EXPLODE
        je      o_blast
        cmp     al, OP_DIRT
        je      o_blast
        cmp     al, OP_SETTLE
        jne     op_8
        jmp     o_settle
op_8:   cmp     al, OP_WIND
        jne     op_9
        jmp     o_wind
op_9:   cmp     al, OP_SHAKE
        jne     op_a
        jmp     o_shake
op_a:   cmp     al, OP_TALK             ; 30h-3Ah: C's
        jb      o_end
        cmp     al, OP_FIRE + 1
        jae     o_end
        xor     ah, ah                  ; c_op(op)
        jmp     c_op_

o_end:  mov     _ply_done, 1            ; OP_END, or not ours: this segment is done
        ret

o_wait: mov     bx, ply_p               ; n (0: 256)
        mov     al, [bx]
        xor     ah, ah
        or      al, al
        jnz     ow_1
        inc     ah
ow_1:   mov     _ply_wait, ax
        mov     ax, 1
        jmp     adv

o_arc:  mov     ax, ply_p
        call    trk_load
        mov     ax, 18
        jmp     adv

o_stop: mov     bx, ply_p
        mov     al, [bx]
        call    trk_stop
        mov     ax, 1
        jmp     adv

o_path: mov     ax, ply_p               ; (each says how many bytes it took)
        call    ops_path
        jmp     adv

o_flow: mov     ax, ply_p
        call    ops_flow
        jmp     adv

o_beam: mov     ax, ply_p
        call    ops_beam
        jmp     adv

o_wind: mov     bx, ply_p               ; (the state's bytes as they come)
        mov     ax, [bx]
        mov     WORD PTR _st + OFF_WIND, ax
        mov     ax, 2
        jmp     adv

o_shake:
        mov     bx, ply_p
        mov     al, [bx]
        mov     _shake_n, al
        mov     ax, 1
        jmp     adv

; EXPLODE: x16, y16, rx, ry, kind, frames; DIRT: the same without kind
o_blast:
        mov     ob_op, al
        mov     si, OFFSET DGROUP:blasts        ; the first free one (or the last)
        mov     cx, NBLAST - 1
ob_f:   cmp     BYTE PTR [si + B_ON], 0
        je      ob_got
        add     si, B_SIZE
        loop    ob_f
ob_got: mov     bx, ply_p
        mov     ax, [bx]
        mov     [si + B_X], ax
        mov     ax, [bx + 2]
        mov     [si + B_Y], ax
        mov     al, [bx + 4]
        mov     [si + B_RX], al
        mov     al, [bx + 5]
        mov     [si + B_RY], al
        add     bx, 6
        cmp     ob_op, OP_DIRT
        mov     al, DIRTBALL
        je      ob_k
        mov     al, [bx]
        inc     bx
ob_k:   mov     [si + B_KIND], al
        cmp     al, EX_NUKE             ; a nuke shakes the screen
        jne     ob_nn
        mov     _shake_n, 30
ob_nn:  mov     al, [bx]
        inc     bx
        or      al, al
        jnz     ob_t
        inc     al
ob_t:   mov     [si + B_TOT], al
        mov     BYTE PTR [si + B_ON], 1
        mov     BYTE PTR [si + B_N], 0
        mov     BYTE PTR [si + B_DN], 0
        mov     ax, bx
        sub     ax, ply_p
        jmp     adv

; SETTLE: x16, n, frames, n column entries
o_settle:
        mov     si, OFFSET DGROUP:settles
        mov     cx, NSETTLE
os_f:   cmp     BYTE PTR [si + S_ON], 0
        je      os_got
        add     si, S_SIZE
        loop    os_f
        mov     si, OFFSET DGROUP:settles       ; full: the oldest lands now
        call    settle
        call    ply_setp
        mov     si, OFFSET DGROUP:settles
os_got: mov     bx, ply_p
        mov     ax, [bx]
        mov     [si + S_X], ax
        mov     al, [bx + 2]
        mov     [si + S_N], al
        mov     al, [bx + 3]
        mov     [si + S_LEFT], al
        mov     ax, 4
        call    adv
        mov     ax, _ply_pc             ; its columns start here
        mov     [si + S_A], ax
        mov     BYTE PTR [si + S_ON], 1
        mov     al, [si + S_N]          ; past them
        mov     os_n, al
os_skip:
        cmp     os_n, 0
        je      os_now
        call    ply_setp
        mov     bx, ply_p
        call    elen
        xor     ah, ah
        call    adv
        dec     os_n
        jmp     os_skip
os_now: cmp     BYTE PTR [si + S_LEFT], 0
        jne     os_ret
        jmp     settle
os_ret: ret

; elen: AL = the length of the column entry at BX (1, or 1 + 2 a span)
elen:   mov     al, [bx]
        cmp     al, T_SPANS + 1
        jb      el_one
        sub     al, T_SPANS
        add     al, al
        inc     al
        ret
el_one: mov     al, 1
        ret

; settle SI: its columns written (fld_cols)
settle: mov     ax, [si + S_A]
        add     ax, OFFSET DGROUP:_script
        mov     _fent, ax
        mov     ax, [si + S_X]
        mov     _fx, ax
        mov     al, [si + S_N]
        mov     _fld_ncols, al
        call    fld_cols_
        mov     BYTE PTR [si + S_ON], 0
        ret

; ---- a frame's step -------------------------------------------------------------

anim:   call    trk_step
        mov     fire, 0
        mov     si, OFFSET DGROUP:blasts + (NBLAST - 1) * B_SIZE
        mov     bi, NBLAST
an_b:   cmp     BYTE PTR [si + B_ON], 0
        je      an_nb
        inc     BYTE PTR [si + B_N]
        mov     al, [si + B_N]
        cmp     al, [si + B_TOT]
        jae     an_end
        mov     fire, 1
        jmp     an_nb
an_end: mov     al, [si + B_RX]         ; done: the crater (or the dirt) now
        mov     bl, [si + B_RY]
        call    elargs
        mov     al, [si + B_KIND]
        cmp     al, DIRTBALL
        jne     an_cr
        mov     al, PX_DIRT
        mov     bl, 1
        jmp     an_go
an_cr:  cmp     al, EX_PLASMA
        je      an_fl
        cmp     al, EX_PUFF
        je      an_fl
        mov     al, PX_SKY
        mov     bl, 0
        jmp     an_go
an_fl:  mov     al, PX_SKY
        mov     bl, 2
an_go:  mov     _fv, al
        mov     _emode, bl
        call    fld_ellipse_
        mov     BYTE PTR [si + B_ON], 0
an_nb:  sub     si, B_SIZE
        dec     bi
        jnz     an_b
        call    ops_step                ; (AL: a fire flow burns)
        or      fire, al
        mov     si, OFFSET DGROUP:settles + (NSETTLE - 1) * S_SIZE
        mov     bi, NSETTLE
an_s:   cmp     BYTE PTR [si + S_ON], 0
        je      an_ns
        dec     BYTE PTR [si + S_LEFT]
        jnz     an_ns
        call    settle
an_ns:  sub     si, S_SIZE
        dec     bi
        jnz     an_s
        call    pt_step                 ; sparks glow like fire
        or      al, fire
        xor     ah, ah
        call    field_fire_
        call    c_anim_
        ret

; elargs: the ellipse at blast SI, radii AL, BL (0 is 1; ry at most 200)
elargs: or      al, al
        jnz     ea_a
        inc     al
ea_a:   mov     _erx, al
        mov     al, bl
        or      al, al
        jnz     ea_b
        inc     al
ea_b:   cmp     al, 201
        jb      ea_c
        mov     al, 200
ea_c:   mov     _ery, al
        mov     ax, [si + B_X]
        mov     _ex, ax
        mov     ax, [si + B_Y]
        mov     _ey, ax
        ret

; ---- the blasts drawn at their size now --------------------------------------------

ply_draw:
        mov     si, OFFSET DGROUP:blasts + (NBLAST - 1) * B_SIZE
        mov     bi, NBLAST
pd_b:   cmp     BYTE PTR [si + B_ON], 0
        je      pd_nb
        mov     al, [si + B_N]
        cmp     al, [si + B_DN]
        je      pd_nb
        mov     [si + B_DN], al
        mov     al, [si + B_RX]         ; rx n / total, ry n / total
        call    scale
        push    ax
        mov     al, [si + B_RY]
        call    scale
        mov     bl, al
        pop     ax
        call    elargs
        mov     al, [si + B_KIND]
        cmp     al, DIRTBALL
        jne     pd_fi
        mov     al, PX_DIRT
        mov     bl, 1
        jmp     pd_go
pd_fi:  mov     bl, 1                   ; plasma, a puff: over the air only
        cmp     al, EX_PLASMA
        je      pd_f
        cmp     al, EX_PUFF
        je      pd_f
        mov     bl, 0                   ; a crater's fireball eats the dirt
pd_f:   mov     al, PX_FX
pd_go:  mov     _fv, al
        mov     _emode, bl
        call    fld_ellipse_
pd_nb:  sub     si, B_SIZE
        dec     bi
        jnz     pd_b
        ret

; scale: AL = AL * n / total of blast SI (at most AL: n <= total)
scale:  mov     bl, [si + B_N]
        xor     bh, bh                  ; BX: n (the Z80's DE)
        xor     dx, dx                  ; DX: the product (HL)
        mov     cx, 8
sc_m:   add     dx, dx                  ; DX = AL * n
        rol     al, 1
        jnc     sc_mn
        add     dx, bx
sc_mn:  loop    sc_m
        mov     bl, [si + B_TOT]        ; / total: the high byte is below it
        mov     al, dh
        mov     cx, 8
sc_d:   shl     dl, 1
        rcl     al, 1
        jc      sc_sub
        cmp     al, bl
        jb      sc_dn
sc_sub: sub     al, bl
        inc     dl
sc_dn:  loop    sc_d
        mov     al, dl
        ret

; ---- a segment's end ------------------------------------------------------------------

ply_end:
        call    pt_clear
        mov     si, OFFSET DGROUP:settles + (NSETTLE - 1) * S_SIZE
        mov     bi, NSETTLE
pe_s:   cmp     BYTE PTR [si + S_ON], 0
        je      pe_n
        call    settle
pe_n:   sub     si, S_SIZE
        dec     bi
        jnz     pe_s
        jmp     ops_seg_end

_TEXT   ENDS
        END
