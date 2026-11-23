; chain.asm -- the game and the menu are two programs on one medium (the
; DDP the FujiNet serves, or a real tape or disk): this loads the other
; from the device we booted from and runs it, as the image's bootstrap
; ran the first (z88dk's adam_bootstrap.asm: 1K blocks to 0000h, then 0).
;
; chain_table ("CHNT", then for the game, the menu and the art the first
; block, low and high, and the count) is filled in by tools/adam/mkimage.py,
; in both programs' images. The art is a block of its own, the VRAM's
; 3800h-3BFFh: the tank sprites' patterns, then (3A00h) the font. Each
; program reads it at start-up into the field store's memory (free then)
; and puts it in the VRAM (art_load); the code reads both back from there.
;
; The loader runs from a copy at D200h, above any program (they end below
; C800h, the bootstrap's) and under EOS (D390h up), with its stack under
; it. First the VDP's interrupt goes off (R1's IE): the NMI's vector at
; 0066h is overwritten by the load. The new program's plat_init reads the
; status first (the CONFIG-boot fix), and puts IE back.

        SECTION code_user

        PUBLIC  _chain_run, _chain_table, _art_load
        EXTERN  _va_addr, _va_src, _va_len, _va_writef
        EXTERN  _vdp_busy

        defc    CURRENT_DEV = $FD6F     ; EOS: the device we booted from
        defc    READ_BLOCK  = $FCF3     ; A dev, HL to, BCDE block: Z ok
        defc    STUB        = $D200
        defc    VDP_CTRL    = $BF

; void chain_run(uint8_t which) __z88dk_fastcall: L 0 the game, 1 the menu
_chain_run:
        ld      a,l
        add     a,a
        add     a,l             ; 3 bytes an entry
        ld      e,a
        ld      d,0
        ld      hl,_chain_table+4
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)          ; DE the first block
        inc     hl
        ld      b,(hl)          ; B the count
        ld      a,b
        or      a
        ret     z               ; (not on this medium: the caller says so)
        push    bc
        push    de
        ; the VDP's interrupt off, the display blanked, the status read
        ld      a,1
        ld      (_vdp_busy),a
        ld      a,$80           ; R1: 16K, blank, no IE
        out     (VDP_CTRL),a
        ld      a,$81
        out     (VDP_CTRL),a
        in      a,(VDP_CTRL)
        ; the loader to D200h
        ld      hl,stub
        ld      de,STUB
        ld      bc,stub_end-stub
        ldir
        pop     de
        pop     bc
        jp      STUB

; (copied to D200h: position independent but for its own calls' targets,
; which are absolute EOS ones)
stub:
        ld      sp,STUB-16
        ld      hl,0            ; the destination
st_blk:
        push    bc
        ld      c,5             ; tries
st_try:
        push    bc
        push    de
        push    hl
        ld      a,(CURRENT_DEV)
        ld      bc,0
        call    READ_BLOCK
        pop     hl
        pop     de
        pop     bc
        jr      z,st_ok
        dec     c
        jr      nz,st_try
st_ok:
        ld      a,h
        add     a,4
        ld      h,a
        inc     de
        pop     bc
        djnz    st_blk
        ld      a,(CURRENT_DEV)
        ld      b,a             ; (as the bootstrap leaves it)
        rst     0
stub_end:

; void art_load(void): the art block to VRAM 3800h, read from the medium
; into A700h (the field store's: free at start-up), unless it's there
; already: "ART!" between the sprites and the font (the other program read
; it, or the rig put it there)
        defc    ART_AT = $A700
_art_load:
        ld      hl,ART_AT+508
        ld      de,art_sig
        ld      b,4
al_s:   ld      a,(de)
        cp      (hl)
        jr      nz,al_read
        inc     hl
        inc     de
        djnz    al_s
        jr      al_vram
al_read:
        ld      a,(_chain_table+4+8)
        or      a
        ret     z               ; (no art on this medium)
        push    ix
        push    iy
        ld      c,5             ; tries
al_t:   push    bc
        ld      a,(CURRENT_DEV)
        ld      hl,ART_AT
        ld      bc,0
        ld      de,(_chain_table+4+6)
        call    READ_BLOCK
        pop     bc
        jr      z,al_ok
        dec     c
        jr      nz,al_t
al_ok:  pop     iy
        pop     ix
al_vram:
        ld      hl,$3800
        ld      (_va_addr),hl
        ld      hl,ART_AT
        ld      (_va_src),hl
        ld      hl,1024
        ld      (_va_len),hl
        jp      _va_writef      ; (start-up: the display blanked)
art_sig:
        defm    "ART!"

        SECTION rodata_user
_chain_table:
        defm    "CHNT"
        defb    0, 0, 0         ; the game: first block, count
        defb    0, 0, 0         ; the menu
        defb    0, 0, 0         ; the art
