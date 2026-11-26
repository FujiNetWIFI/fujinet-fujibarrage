; vdp.asm -- the Master System's VDP, written a burst at a time (FujiBlast's
; src/sms/vdp.asm, in bursts).
;
; With the display on, the data port takes a byte every 26 cycles at the
; fastest: these loops are OUTI + JR NZ, 28. A burst is 32 bytes at most,
; its address set and its bytes sent with the interrupt off (about 1000
; cycles, 4 lines: the frame interrupt waits that long at worst), so the
; frame's own VDP work between bursts (smsint.asm) spoils nothing.
;
;   va_addr   the VRAM address (va_cram: the colour), va_src the bytes,
;             va_len how many; va_val the byte va_fill writes va_len times
;   vdp_wr    the same from asm: HL the bytes, DE the VRAM address, BC the
;             count (IX, IY kept)

        SECTION code_user

        PUBLIC  _va_write, _va_fill, _va_cram, _va_read, _vdp_reg
        PUBLIC  _va_addr, _va_src, _va_len, _va_val, _vdp_r1
        PUBLIC  vdp_wr, vdp_fill, vdp_addr

        defc    VDP_DATA = $BE
        defc    VDP_CTRL = $BF
        defc    BURST = 32

; the address DE with the command bits in A (40h write VRAM, C0h CRAM, 00h
; read), the interrupt off (the caller puts it back on)
vdp_addr:
        di
        push    af
        ld      a,e
        out     (VDP_CTRL),a
        pop     af
        or      d
        out     (VDP_CTRL),a
        ret

_va_write:
        ld      a,$40
va_go:  ld      hl,(_va_src)
        ld      de,(_va_addr)
        ld      bc,(_va_len)
; A the command, HL the bytes, DE the address, BC the count
vdp_wrc:
        ld      (wr_cmd),a
wr_burst:
        ld      a,b
        or      c
        ret     z
        push    bc
        ld      a,b             ; this burst: min(BC, 32)
        or      a
        ld      a,BURST
        jr      nz,wr_n
        ld      a,c
        cp      BURST
        jr      c,wr_n
        ld      a,BURST
wr_n:   ld      b,a
        ld      a,(wr_cmd)
        call    vdp_addr
        ld      a,b
        ld      c,VDP_DATA
wr_l:   outi                    ; 16
        jr      nz,wr_l         ; 12
        ei
        ex      de,hl           ; the address + the burst
        ld      c,a
        ld      b,0
        add     hl,bc
        ex      de,hl           ; (HL: the bytes, on past the burst)
        ex      (sp),hl         ; the count - the burst
        or      a
        sbc     hl,bc
        ld      b,h
        ld      c,l
        pop     hl
        jr      wr_burst

vdp_wr:
        ld      a,$40
        jr      vdp_wrc

_va_cram:
        ld      a,$C0
        jr      va_go

; va_len bytes of va_val from va_addr
_va_fill:
        ld      de,(_va_addr)
        ld      bc,(_va_len)
        ld      a,(_va_val)
; A the byte, DE the address, BC the count
vdp_fill:
        ld      (fl_val),a
fl_burst:
        ld      a,b
        or      c
        ret     z
        push    bc
        ld      a,b
        or      a
        ld      a,BURST
        jr      nz,fl_n
        ld      a,c
        cp      BURST
        jr      c,fl_n
        ld      a,BURST
fl_n:   ld      b,a
        ld      a,$40
        call    vdp_addr
        ld      c,b
        ld      a,(fl_val)
fl_l:   out     (VDP_DATA),a    ; 11
        dec     b               ; 4
        jr      nz,fl_l         ; 12
        ei
        ld      b,0
        ex      de,hl
        add     hl,bc
        ex      de,hl
        pop     hl
        or      a
        sbc     hl,bc
        ld      b,h
        ld      c,l
        jr      fl_burst

; va_len bytes from VRAM at va_addr into va_src (the VDP reads ahead: a byte
; every 28 cycles is safe too)
_va_read:
        ld      hl,(_va_src)
        ld      de,(_va_addr)
        ld      bc,(_va_len)
rd_burst:
        ld      a,b
        or      c
        ret     z
        push    bc              ; the count
        ld      a,b
        or      a
        ld      a,BURST
        jr      nz,rd_n
        ld      a,c
        cp      BURST
        jr      c,rd_n
        ld      a,BURST
rd_n:   ld      b,a
        push    af              ; the burst
        xor     a
        call    vdp_addr
        ld      c,VDP_DATA
rd_l:   ini                     ; 16 (HL, the destination, moves on)
        jr      nz,rd_l
        ei
        pop     af
        ex      de,hl
        ld      c,a
        ld      b,0
        add     hl,bc
        ex      de,hl           ; the address + the burst
        ex      (sp),hl         ; (the destination kept)
        or      a
        sbc     hl,bc
        ld      b,h
        ld      c,l
        pop     hl
        jr      rd_burst

; void vdp_reg(uint8_t r, uint8_t v): register r = v (register 1's kept)
_vdp_reg:
        ld      hl,2
        add     hl,sp
        ld      e,(hl)          ; r
        inc     hl
        ld      a,(hl)          ; v
        ld      d,a
        ld      a,e
        cp      1
        jr      nz,rg_w
        ld      a,d
        ld      (_vdp_r1),a
rg_w:   di
        ld      a,d
        out     (VDP_CTRL),a
        ld      a,e
        or      $80
        out     (VDP_CTRL),a
        ei
        ret

        SECTION bss_user
_va_addr:       defs    2
_va_src:        defs    2
_va_len:        defs    2
_va_val:        defs    1
_vdp_r1:        defs    1
wr_cmd:         defs    1
fl_val:         defs    1
