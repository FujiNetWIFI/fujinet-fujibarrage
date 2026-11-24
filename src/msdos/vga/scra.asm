;
; scra.asm -- the VGA build's text cells (text.c): glyphs of the Namco font
; on the screen (A000h, mode 13h, 320 bytes a line), each 4-pixel half of
; each line 4 bytes from a table of the row's colours (text.c's: line n's
; ink and ground, the 16 nibbles' 4 pixels at n * 64 + nibble * 4), two
; word moves. A run of cells a call. (A pixel at a time, a cell a call,
; took 1-1.7 s a shop scroll on a 4.77 MHz PC: TODO 2026-11-23.)
;
; vga_run: AX the first cell's offset (row * 2560 + col * 8), DX its
; characters (DGROUP; ASCII, else a space), CX how many, BX the table (512
; bytes, DGROUP). C calls it through #pragma aux (vga.h): it changes AX BX
; CX DX SI DI ES. 8086 only.
;
        .8086

DGROUP  GROUP   CONST, _DATA, _BSS
CONST   SEGMENT WORD PUBLIC 'DATA'
CONST   ENDS
_DATA   SEGMENT WORD PUBLIC 'DATA'
_DATA   ENDS
_BSS    SEGMENT WORD PUBLIC 'BSS'
chars   DW      ?                       ; the run's next character
left    DW      ?                       ;  and how many to go
_BSS    ENDS

        EXTRN   _font8:BYTE             ; (gen/msdos/font.c: 20h-7Fh, 8 bytes each)

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP, SS:DGROUP

        PUBLIC  vga_run_
vga_run_ PROC   NEAR
        push    bp
        cld
        mov     di, ax
        mov     chars, dx
        mov     left, cx
        mov     ax, 0A000h
        mov     es, ax
        jcxz    vr_x
vr_cell:
        mov     si, chars
        mov     al, [si]
        inc     chars
        and     al, 7Fh                 ; (bit 7: a label's, scr.h)
        sub     al, 32
        cmp     al, 96
        jb      vr_in
        xor     al, al                  ; (not ASCII: a space)
vr_in:  xor     ah, ah
        shl     ax, 1
        shl     ax, 1
        shl     ax, 1
        add     ax, OFFSET DGROUP:_font8
        mov     bp, ax                  ; the glyph ([bp]: SS, DGROUP)
        mov     cx, 8
vr_line:
        mov     al, [bp]                ; the glyph's line
        inc     bp
        mov     dl, al
        and     ax, 00F0h               ; its left 4: (line >> 4) * 4
        shr     ax, 1
        shr     ax, 1
        mov     si, bx
        add     si, ax
        movsw
        movsw
        mov     al, dl                  ; its right 4
        and     ax, 000Fh
        shl     ax, 1
        shl     ax, 1
        mov     si, bx
        add     si, ax
        movsw
        movsw
        add     di, 312
        add     bx, 64                  ; the next line's colours
        loop    vr_line
        sub     bx, 512
        sub     di, 2552                ; (8 lines down: the next cell's top)
        dec     left
        jnz     vr_cell
vr_x:   pop     bp
        ret
vga_run_ ENDP

; vga_grun: AX the first cell's offset, DX its characters (DGROUP; ASCII,
; else a space), CX how many, BX a segment of the look's glyphs drawn
; (text.c: glyph g's 8 lines of 8 pixels at g * 64): 32 word moves a cell.
; It changes AX BX CX DX SI DI ES.
        PUBLIC  vga_grun_
vga_grun_ PROC  NEAR
        push    bp
        push    ds
        mov     di, ax
        mov     bp, dx                  ; the characters ([bp]: SS, DGROUP)
        mov     dx, cx
        mov     ax, 0A000h
        mov     es, ax
        mov     ds, bx
        cld
        or      dx, dx
        jz      gr_x
gr_cell:
        mov     al, [bp]
        inc     bp
        and     al, 7Fh                 ; (bit 7: a label's, scr.h)
        sub     al, 32
        cmp     al, 96
        jb      gr_in
        xor     al, al                  ; (not ASCII: a space)
gr_in:  xor     ah, ah
        mov     cl, 6
        shl     ax, cl
        mov     si, ax
        movsw
        movsw
        movsw
        movsw
        add     di, 312
        movsw
        movsw
        movsw
        movsw
        add     di, 312
        movsw
        movsw
        movsw
        movsw
        add     di, 312
        movsw
        movsw
        movsw
        movsw
        add     di, 312
        movsw
        movsw
        movsw
        movsw
        add     di, 312
        movsw
        movsw
        movsw
        movsw
        add     di, 312
        movsw
        movsw
        movsw
        movsw
        add     di, 312
        movsw
        movsw
        movsw
        movsw
        add     di, 312
        sub     di, 2552                ; (8 lines down: the next cell's top)
        dec     dx
        jnz     gr_cell
gr_x:   pop     ds
        pop     bp
        ret
vga_grun_ ENDP

_TEXT   ENDS
        END
