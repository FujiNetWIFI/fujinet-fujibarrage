;
; same.asm -- scr.c's: how many of n characters at s a row's cells at cp
; hold already (REPE CMPSB: the C loop took 88 us a character on a 4.77 MHz
; PC, as much as drawing it: TODO 2026-11-23).
;
; txt_same: AX cp, DX s, CX n (all DGROUP); the count in AX. C calls it
; through #pragma aux (text.h): it changes AX BX CX SI DI ES. 8086 only.
;
        .8086

DGROUP  GROUP   _DATA, _BSS
_DATA   SEGMENT WORD PUBLIC 'DATA'
_DATA   ENDS
_BSS    SEGMENT WORD PUBLIC 'BSS'
_BSS    ENDS

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP

        PUBLIC  txt_same_
txt_same_ PROC  NEAR
        mov     bx, ds
        mov     es, bx
        mov     di, ax
        mov     si, dx
        mov     bx, cx
        xor     ax, ax
        jcxz    ts_x
        cld
        repe    cmpsb
        je      ts_all
        inc     cx                      ; (the one that differed)
ts_all: mov     ax, bx
        sub     ax, cx
ts_x:   ret
txt_same_ ENDP

_TEXT   ENDS
        END
