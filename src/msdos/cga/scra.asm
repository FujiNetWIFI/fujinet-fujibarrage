;
; scra.asm -- the CGA build's text cells (text.c): glyphs of the Namco font
; on the screen (B800h, mode 4: even lines at 0, odd at 2000h, 80 bytes a
; line, 4 pixels a byte), each 4-pixel half of a line a byte from a
; 16-entry table (the row's ink and ground). A run of cells a call: a cell
; at a time from C cost more than its drawing (TODO 2026-11-23).
;
; cga_run: AX the first cell's offset (row * 320 + col * 2), DX its
; characters (DGROUP; ASCII, else a space), CX how many, BX the table
; (DGROUP). C calls it through #pragma aux (cga.h): it changes AX BX CX DX
; SI DI ES. 8086 only.
;
        .8086

DGROUP  GROUP   CONST, _DATA, _BSS
CONST   SEGMENT WORD PUBLIC 'DATA'
CONST   ENDS
_DATA   SEGMENT WORD PUBLIC 'DATA'
_DATA   ENDS
_BSS    SEGMENT WORD PUBLIC 'BSS'
_BSS    ENDS

        EXTRN   _font8:BYTE             ; (gen/msdos/font.c: 20h-7Fh, 8 bytes each)

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP, SS:DGROUP

        PUBLIC  cga_run_
cga_run_ PROC   NEAR
        push    bp
        cld
        mov     di, ax
        mov     bp, dx                  ; the characters ([bp]: SS, DGROUP)
        mov     ax, 0B800h
        mov     es, ax
        jcxz    cr_x
cr_cell:
        push    cx
        mov     al, [bp]
        inc     bp
        and     al, 7Fh                 ; (bit 7: a label's, scr.h)
        sub     al, 32
        cmp     al, 96
        jb      cr_in
        xor     al, al                  ; (not ASCII: a space)
cr_in:  xor     ah, ah
        shl     ax, 1
        shl     ax, 1
        shl     ax, 1
        add     ax, OFFSET DGROUP:_font8
        mov     si, ax
        mov     cx, 4
cr_pair:
        lodsb                           ; an even line
        mov     ah, al
        shr     al, 1
        shr     al, 1
        shr     al, 1
        shr     al, 1
        xlat
        mov     dl, al
        mov     al, ah
        and     al, 0Fh
        xlat
        mov     dh, al
        mov     es:[di], dx
        lodsb                           ; the odd line under it
        mov     ah, al
        shr     al, 1
        shr     al, 1
        shr     al, 1
        shr     al, 1
        xlat
        mov     dl, al
        mov     al, ah
        and     al, 0Fh
        xlat
        mov     dh, al
        mov     es:[di + 2000h], dx
        add     di, 80
        loop    cr_pair
        sub     di, 318                 ; (4 line pairs down: the next cell's top)
        pop     cx
        loop    cr_cell
cr_x:   pop     bp
        ret
cga_run_ ENDP

; cga_move: AX a row's offset in a bank (row * 320), DX the one it takes
; from, CX bytes (even): both banks, forward (so AX below DX, or apart: a
; scroll down moves a row at a time, from the bottom; never STD, which an
; interrupt's code might not expect). It changes AX BX CX SI DI ES.
        PUBLIC  cga_move_
cga_move_ PROC  NEAR
        push    ds
        mov     bx, 0B800h
        mov     ds, bx
        mov     es, bx
        cld
        shr     cx, 1
        mov     bx, cx
        mov     di, ax
        mov     si, dx
        rep     movsw
        mov     cx, bx
        mov     di, ax
        add     di, 2000h
        mov     si, dx
        add     si, 2000h
        rep     movsw
        pop     ds
        ret
cga_move_ ENDP

; cga_grun: AX the first cell's offset, DX its characters (DGROUP; ASCII,
; else a space), CX how many, BX a segment of the look's glyphs drawn
; (text.c: glyph g's 8 lines of 2 bytes at g * 16). It changes AX BX CX DX
; SI DI ES.
        PUBLIC  cga_grun_
cga_grun_ PROC  NEAR
        push    bp
        push    ds
        mov     di, ax
        mov     bp, dx                  ; the characters ([bp]: SS, DGROUP)
        mov     dx, cx
        mov     ax, 0B800h
        mov     es, ax
        mov     ds, bx
        cld
        or      dx, dx
        jz      cg_x
cg_cell:
        mov     al, [bp]
        inc     bp
        and     al, 7Fh                 ; (bit 7: a label's, scr.h)
        sub     al, 32
        cmp     al, 96
        jb      cg_in
        xor     al, al                  ; (not ASCII: a space)
cg_in:  xor     ah, ah
        mov     cl, 4
        shl     ax, cl
        mov     si, ax
        lodsw
        mov     es:[di + 0], ax
        lodsw
        mov     es:[di + 2000h + 0], ax
        lodsw
        mov     es:[di + 80], ax
        lodsw
        mov     es:[di + 2000h + 80], ax
        lodsw
        mov     es:[di + 160], ax
        lodsw
        mov     es:[di + 2000h + 160], ax
        lodsw
        mov     es:[di + 240], ax
        lodsw
        mov     es:[di + 2000h + 240], ax
        add     di, 2
        dec     dx
        jnz     cg_cell
cg_x:   pop     ds
        pop     bp
        ret
cga_grun_ ENDP

_TEXT   ENDS
        END
