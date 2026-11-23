;
; scra.asm -- the CGA build's text cell (text.c): a glyph of the Namco font
; on the screen (B800h, mode 4: even lines at 0, odd at 2000h, 80 bytes a
; line, 4 pixels a byte), each 4-pixel half of its line a byte from a
; 16-entry table (the row's ink and ground).
;
; cga_cell: AX the cell's offset (its first line: row * 320 + col * 2), DX
; the glyph (8 bytes, DGROUP), BX the table (DGROUP). C calls it through
; #pragma aux (cga.h): it changes AX CX DX SI DI ES. 8086 only.
;
        .8086

DGROUP  GROUP   _DATA, _BSS
_DATA   SEGMENT WORD PUBLIC 'DATA'
_DATA   ENDS
_BSS    SEGMENT WORD PUBLIC 'BSS'
_BSS    ENDS

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP, SS:DGROUP

        PUBLIC  cga_cell_
cga_cell_ PROC  NEAR
        cld
        mov     di, ax
        mov     si, dx
        mov     ax, 0B800h
        mov     es, ax
        mov     cx, 4
cc_pair:
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
        loop    cc_pair
        ret
cga_cell_ ENDP

_TEXT   ENDS
        END
