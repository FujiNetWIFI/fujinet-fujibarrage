;
; The HDOS build's memory map (-pragma-define:MMAP=-1): z88dk's own, then the
; two overlays. Each is linked at the window, H89_OVL_WIN, with its number in
; the 64K digit, so the linker keeps them apart and an overlay stub's defq
; carries the number to ovl_tramp (src/h89/ovl.asm). z80asm writes each out
; as barrage_OVL_<name>.bin; tools/h89/hdosovl.py appends them to
; BARRAGE.ABS.
;

    INCLUDE "crt/classic/crt_section_standard.inc"

    SECTION OVL_ROOM
    org     $010000 + H89_OVL_WIN
    SECTION code_ovl_room
    SECTION rodata_ovl_room
    SECTION OVL_ROOM_END

    SECTION OVL_FIELD
    org     $020000 + H89_OVL_WIN
    SECTION code_ovl_field
    SECTION rodata_ovl_field
    SECTION OVL_FIELD_END
