;
; lzd.asm -- the title pictures' unpacker (tools/mktitle_coco.py's lz_pack,
; written by tools/msdos/mktitle_dos.py): a control byte c < 80h is c + 1
; bytes as they are; 81h-FFh copies (c & 7Fh) + 2 bytes from D + 1 back
; (D the next two bytes, high first); 80h ends it. Straight onto the
; screen: the copies read what is already there.
;
; lz_unpack: AX the screen's segment (it starts at 0), DX the stream
; (DGROUP). C calls it through #pragma aux (src/msdos/text.h). 8086 only.
;
        .8086

DGROUP  GROUP   _DATA, _BSS
_DATA   SEGMENT WORD PUBLIC 'DATA'
_DATA   ENDS
_BSS    SEGMENT WORD PUBLIC 'BSS'
_BSS    ENDS

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP, SS:DGROUP

        PUBLIC  lz_unpack_
lz_unpack_ PROC NEAR
        push    si
        push    di
        cld
        mov     es, ax
        xor     di, di
        mov     si, dx
lz_next:
        lodsb
        cmp     al, 80h
        je      lz_done
        ja      lz_copy
        xor     ah, ah                  ; AL + 1 bytes as they are
        inc     ax
        mov     cx, ax
        rep     movsb
        jmp     lz_next
lz_copy:
        and     al, 7Fh                 ; (AL & 7Fh) + 2 bytes from D + 1 back
        xor     ah, ah
        add     ax, 2
        mov     cx, ax
        lodsb
        mov     ah, al
        lodsb                           ; AX = D
        mov     bx, di
        sub     bx, ax
        dec     bx
        push    si
        push    ds
        push    es
        pop     ds
        mov     si, bx
        rep     movsb
        pop     ds
        pop     si
        jmp     lz_next
lz_done:
        pop     di
        pop     si
        ret
lz_unpack_ ENDP

_TEXT   ENDS
        END
