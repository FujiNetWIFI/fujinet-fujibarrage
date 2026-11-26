; map.asm -- the Master System's memory, as FujiBarrage has it: a Sega-mapper
; image (tools/sms/mkrom.py) on the FujiNet SMS cartridge.
;
;   0000h-3FFFh   bank 0, slot 0: z88dk's crt0, the resident code
;   4000h-7FFFh   slot 1: the rest of the program (and its overlays, when
;                 it needs them: each a bank of its own, the resident part
;                 copied into each); the "FUJI" claim at 7FDCh and the
;                 SEGA header at 7FF0h
;   8000h-AFFFh   slot 2: the cart's RAM, half 0 most of the time:
;                   8000h-A9FFh   the field's store (src/tms9918/fld.asm,
;                                 SH_HI 80h: 21 rows x 2 pages, D then F)
;                   AA00h-AFFFh   the script (net.c's script[], SCRIPT_AT)
;                 half 1 the tile cache's tables (tcache.asm maps it in and
;                 back); a ROM bank (the art, the title, the tunes) only for
;                 a moment, put back after
;   B000h-BFFFh   the cartridge's mailbox, whatever is in slot 2
;   C000h-DFEFh   RAM: the variables, then the stack down from DFF0h; DFFCh-
;                 DFFFh mirror the mapper's registers (FFFCh-FFFFh), so
;                 reading those says what is mapped
;
; The cartridge tells a Sega mapper's image from others by counting LD
; (nn),A stores in the first 32K: only the ones to FFFFh may be there
; (tools/sms/mkrom.py checks). Slot 2's RAM is written through pointers.

; FAR code: menus.c, room.c and appkey.c (they never touch the cart's RAM)
; run from a ROM bank in slot 2 (section FAR, bank FAR_BANK: tools/sms/
; mkrom.py), reached through stubs (tools/sms/mkfar.py: `call far_tramp;
; defb bank; defw target`). far_tramp keeps slot 2 as the caller had it and
; puts it back after, so the rule everywhere is: whatever changes slot 2
; puts it back before it returns. So FAR code may call anything resident,
; and a resident routine that maps the cart's RAM takes what it needs from
; its caller's pointers first (they may point into the FAR bank).

        SECTION FAR
        org     $8000

        SECTION code_user

        PUBLIC  _sms_ram0, _sms_ram1, _sms_rom, _sms_ram_ok, _script
        PUBLIC  sms_ram0, sms_ram1, _sms_map_init, far_tramp

        defc    FS_DEPTH = 12           ; far calls inside far calls, at most

        defc    _script = $AA00         ; (field.h's SCRIPT_BUF: 1536)
        defc    MP_CTL = $FFFC          ; bit 3 RAM in slot 2, bit 2 its half
        defc    MP_SLOT2 = $FFFF

; slot 2: the cart's RAM, half 0 (A changed)
_sms_ram0:
sms_ram0:
        ld      a,$08
        ld      (MP_CTL),a
        ret

; half 1
_sms_ram1:
sms_ram1:
        ld      a,$0C
        ld      (MP_CTL),a
        ret

; ROM bank L (__z88dk_fastcall)
_sms_rom:
        xor     a
        ld      (MP_CTL),a
        ld      a,l
        ld      (MP_SLOT2),a
        ret

; 1 (in L) if the cart's 32K of RAM is there: each half keeps its own byte
; (written through HL: a constant store to 8000h would read as a
; Codemasters mapper's). Half 0 is left in slot 2.
_sms_ram_ok:
        ld      hl,$8000
        call    sms_ram0
        ld      (hl),$5A
        call    sms_ram1
        ld      (hl),$A5
        ld      a,(hl)
        cp      $A5
        jr      nz,ro_no
        call    sms_ram0
        ld      a,(hl)
        cp      $5A
        jr      nz,ro_no
        ld      (hl),$C3
        ld      a,(hl)
        cp      $C3
        jr      nz,ro_no
        ld      l,1
        ret
ro_no:  call    sms_ram0
        ld      l,0
        ret

; the mapper as the program wants it at start: slots 0-2 banks 0, 1, 2, then
; the cart's RAM half 0 in slot 2 (each write also lands in the RAM's
; mirror at DFFCh-DFFFh, which is what far_tramp reads back)
_sms_map_init:
        ld      a,0
        ld      ($FFFD),a
        inc     a
        ld      ($FFFE),a
        inc     a
        ld      (MP_SLOT2),a
        ld      hl,fs_stack + FS_DEPTH * 4
        ld      (fs_ptr),hl
        jp      sms_ram0

; the stub's call: (SP) -> bank, target; under it the caller's return
far_tramp:
        pop     hl
        ld      b,(hl)          ; the bank
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)          ; the target
        pop     hl              ; the caller's return
        push    de
        ex      de,hl
        ld      hl,(fs_ptr)     ; a frame: the return, slot 2's two registers
        dec     hl
        ld      (hl),d
        dec     hl
        ld      (hl),e
        ld      a,(MP_CTL)      ; (the RAM's mirror)
        dec     hl
        ld      (hl),a
        ld      a,(MP_SLOT2)
        dec     hl
        ld      (hl),a
        ld      (fs_ptr),hl
        xor     a
        ld      (MP_CTL),a      ; ROM in slot 2: the bank
        ld      a,b
        ld      (MP_SLOT2),a
        pop     hl
        call    ft_jp           ; the target sees its arguments as a direct call leaves them
        push    hl              ; (the result: L, HL or DEHL)
        push    de
        ld      hl,(fs_ptr)
        ld      a,(hl)
        ld      (MP_SLOT2),a
        inc     hl
        ld      a,(hl)
        ld      (MP_CTL),a
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      (fs_ptr),hl
        ld      (ft_ret),de
        pop     de
        pop     hl
        push    hl
        ld      hl,(ft_ret)
        ex      (sp),hl
        ret
ft_jp:  jp      (hl)

        SECTION bss_user
fs_ptr:         defs    2
ft_ret:         defs    2
fs_stack:       defs    FS_DEPTH * 4

