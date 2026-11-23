; menu.asm -- the menu program has no field: what the text screens and
; the clock call of it, nothing (the game's are src/tms9918/field.asm).

        SECTION code_user

        PUBLIC  _tms_idle, _tms_frame, _field_off

_tms_idle:
_tms_frame:
_field_off:
        ret
