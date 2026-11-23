;
; scra.asm -- the VGA build's text cell (text.c): a glyph of the Namco font
; on the screen (A000h, mode 13h, 320 bytes a line), line n's lit pixels
; ink[n], the rest ground[n].
;
; vga_cell: AX the cell's offset (row * 2560 + col * 8), DX the glyph (8
; bytes), BX ink[8], CX ground[8] (all DGROUP). C calls it through
; #pragma aux (vga.h): it changes AX BX CX DX SI DI ES. 8086 only.
;
        .8086

DGROUP  GROUP   _DATA, _BSS
_DATA   SEGMENT WORD PUBLIC 'DATA'
_DATA   ENDS
_BSS    SEGMENT WORD PUBLIC 'BSS'
_BSS    ENDS

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP, SS:DGROUP

        PUBLIC  vga_cell_
vga_cell_ PROC  NEAR
        push    bp
        cld
        mov     di, ax
        mov     si, dx
        mov     bp, cx                  ; ([bp]: SS, DGROUP)
        mov     ax, 0A000h
        mov     es, ax
        mov     ah, 8
vc_line:
        mov     ch, [si]                ; the glyph's line
        inc     si
        mov     dl, [bx]                ; lit
        inc     bx
        mov     dh, [bp]                ; unlit
        inc     bp
        mov     cl, 8
vc_bit: mov     al, dh
        shl     ch, 1
        jnc     vc_put
        mov     al, dl
vc_put: stosb
        dec     cl
        jnz     vc_bit
        add     di, 312
        dec     ah
        jnz     vc_line
        pop     bp
        ret
vga_cell_ ENDP

_TEXT   ENDS
        END
