;
; timer.asm -- the PIT's interrupt (INT 8, set by plat.c): SND_SUB ticks a
; jiffy (msdos.h). Each tick it puts what sound.c picked onto the speaker
; (PIT channel 2: the top voice, or the top two by turns; a noise a random
; divisor about its middle); each SND_SUB-th it counts the jiffy
; (frame_count, the script's clock); every 65536 counts it chains to the
; BIOS's (the time of day, the floppy's motor). In assembly and short, and
; on the ticks it doesn't chain (most), the 8259 told at once and
; interrupts on again: the UART's interrupt cuts in rather than wait (at
; 115200 bps a third byte would overrun it after about 170 us). STATS:
; where it finds the CPU (tools/dosrig/prof86.py).
;
; timer_setup: once, before INT 8 is pointed here: the BIOS's vector
; (_old_int8) and the tick (_pit_tick) copied where the chaining finds them.
; 8086 only.
;
        .8086

DGROUP  GROUP   _DATA, _BSS
_DATA   SEGMENT WORD PUBLIC 'DATA'
_DATA   ENDS
_BSS    SEGMENT WORD PUBLIC 'BSS'
_BSS    ENDS

        EXTRN   _frame_count:WORD, _vbi_ticks:BYTE, _bios_acc:WORD, _sub_n:BYTE
        EXTRN   _old_int8:DWORD, _pit_tick:WORD, _snd_sub:BYTE
        EXTRN   _o_div:WORD, _o_mask:WORD, _o_n:BYTE, _mute_all:BYTE
        EXTRN   _spk_on:BYTE, _sounded:WORD, _rnd:WORD, _turn:BYTE
IFDEF STATS
        EXTRN   _prof_hist:WORD, _prof_other:WORD, _prof_uart:WORD, _prof_off:BYTE
        EXTRN   _code_seg:WORD, _uart_seg:WORD, _tick_base:DWORD
ENDIF

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP, SS:NOTHING

old8    DD      0               ; the BIOS's INT 8 (timer_setup)
tick    DW      0               ; PIT counts a tick

        PUBLIC  timer_setup_
timer_setup_ PROC NEAR
        push    ax
        mov     ax, word ptr _old_int8
        mov     word ptr cs:old8, ax
        mov     ax, word ptr _old_int8 + 2
        mov     word ptr cs:old8 + 2, ax
        mov     ax, _pit_tick
        mov     cs:tick, ax
        pop     ax
        ret
timer_setup_ ENDP

        PUBLIC  timer_isr_
timer_isr_ PROC FAR
        push    ax
        push    bx
        push    ds
        mov     ax, DGROUP
        mov     ds, ax
        mov     ax, cs:tick             ; the BIOS's tick due? (every 65536 counts)
        add     _bios_acc, ax
        mov     bh, 0
        jc      t_due
        mov     al, 20h                 ; no: this tick is ours, the UART's may cut in
        out     20h, al
        sti
        jmp     short t_go
t_due:  mov     bh, 1
t_go:   push    bx
IFDEF STATS
        mov     ax, cs:tick
        add     word ptr _tick_base, ax
        adc     word ptr _tick_base + 2, 0
        cmp     _prof_off, 0
        jne     tp_x
        mov     bx, sp                  ; (BX', DS BX AX, then IP, CS)
        mov     ax, ss:[bx + 10]
        cmp     ax, _code_seg
        jne     tp_nc
        mov     bx, ss:[bx + 8]
        push    cx
        mov     cl, 5
        shr     bx, cl                  ; (IP >> 6) * 2
        pop     cx
        and     bx, 511 * 2
        inc     _prof_hist[bx]
        jmp     short tp_x
tp_nc:  cmp     ax, _uart_seg
        jne     tp_o
        inc     _prof_uart
        jmp     short tp_x
tp_o:   inc     _prof_other
tp_x:
ENDIF
        inc     _sub_n                  ; a jiffy every SND_SUB ticks
        mov     al, _sub_n
        cmp     al, _snd_sub
        jb      t_snd
        mov     _sub_n, 0
        inc     _frame_count
        inc     _vbi_ticks
t_snd:  cmp     _mute_all, 0            ; ---- the speaker
        jne     t_off
        mov     al, _o_n
        or      al, al
        jz      t_off
        xor     bx, bx
        cmp     al, 2
        jne     t_one
        xor     _turn, 1                ; two: by turns
        mov     bl, _turn
        shl     bx, 1
t_one:  mov     ax, _o_div[bx]
        cmp     _o_mask[bx], 0
        je      t_tone
        push    dx                      ; noise: from half its divisor up
        mov     dx, _rnd
        shr     dx, 1
        jnc     t_n0
        xor     dx, 0B400h
t_n0:   mov     _rnd, dx
        and     dx, _o_mask[bx]
        shr     ax, 1
        add     ax, dx
        pop     dx
t_tone: cmp     ax, _sounded
        je      t_on
        mov     _sounded, ax
        mov     bl, al
        mov     al, 0B6h                ; channel 2, lo/hi, mode 3
        out     43h, al
        mov     al, bl
        out     42h, al
        mov     al, ah
        out     42h, al
t_on:   cmp     _spk_on, 0
        jne     t_bios
        in      al, 61h
        or      al, 3
        out     61h, al
        mov     _spk_on, 1
        jmp     short t_bios
t_off:  cmp     _spk_on, 0
        je      t_bios
        in      al, 61h
        and     al, 0FCh
        out     61h, al
        mov     _spk_on, 0
t_bios: pop     bx
        or      bh, bh
        jnz     t_chain
        pop     ds
        pop     bx
        pop     ax
        iret
t_chain:
        pop     ds
        pop     bx
        pop     ax
        jmp     dword ptr cs:old8       ; (its EOI and IRET)
timer_isr_ ENDP

_TEXT   ENDS
        END
