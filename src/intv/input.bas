' ===========================================================================
' input.bas -- the hand controllers (input.h's), read in the frame handler.
'
' A controller reports one byte, which carries the disc, the keypad and the
' side buttons at once: a key grounds one row line (bits 5-7) and one column
' line (bits 0-3); a button two row lines; the disc only bits 0-4. So the
' row bits say which it is: none, the disc; one, a key (its disc bits are
' the key's, not the disc's); two or three, a button, with the disc beside
' it. A byte is believed once it has held for two frames (a press closes
' its lines one at a time). Both controllers play.
'
' The three side buttons are told apart (inp_btn and ev_btn a bit each):
' the top one fires, the lower left picks the next weapon, the lower right
' opens the items line.
'
' TEST_HOOK ($A5 at TEST_HOOK, the byte at TEST_HOOK + 1) replaces them,
' for the rigs (tools/intvsim/intvsim.py). (FujiBlast's input.bas.)
' ===========================================================================

    CONST IN_UP    = 1
    CONST IN_DOWN  = 2
    CONST IN_LEFT  = 4
    CONST IN_RIGHT = 8
    CONST KEY_NONE   = 0
    CONST KEY_RETURN = 13       ' ENTER
    CONST KEY_ESC    = 27       ' CLEAR
    CONST KEY_MUTE   = 48       ' 0
    CONST BTN_FIRE   = 1        ' the top buttons
    CONST BTN_NEXT   = 2        ' lower left
    CONST BTN_ITEMS  = 4        ' lower right

    DIM inp_dir, inp_btn        ' held (the frame handler's)
    DIM ev_btn, ev_key          ' pressed since the last input_take (ev_btn: BTN_* bits)
    DIM prev_dir                ' the disc, as screens.bas last saw it
    DIM in_raw, in_last, in_b, in_b0, in_k, in_k0, l_btn, l_key

' read_input: READ_INPUT (io.asm), in the frame handler.

' input_take: the presses since the last time into ev_btn and ev_key.
input_take: PROCEDURE
    ASM DIS
    ev_btn = l_btn : l_btn = 0
    ev_key = l_key : l_key = 0
    ASM EIS
END

' The disc's 16 directions (bits 0-4) as 8: a diagonal only at its own
' notch, the notches either side of a straight one the straight way.
disc_tab:
    DATA 0, 2, 8, 2, 1, 0, 8, 0, 4, 4, 0, 0, 1, 0, 0, 0
    DATA 0, 2, 8, 10, 1, 0, 9, 0, 4, 6, 0, 0, 5, 0, 0, 0

' The keypad: (byte / 32) * 16 + (byte AND 15) to the key (ASCII; CLEAR
' is ESC, ENTER RETURN). Column: bit 7 left, 6 middle, 5 right; row: bit 0-3.
key_tab:
    DATA 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0     ' (no column)
    DATA 0, 51, 54, 0, 57, 0, 0, 0, 13, 0, 0, 0, 0, 0, 0, 0 ' right: 3 6 9 ENTER
    DATA 0, 50, 53, 0, 56, 0, 0, 0, 48, 0, 0, 0, 0, 0, 0, 0 ' middle: 2 5 8 0
    DATA 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    DATA 0, 49, 52, 0, 55, 0, 0, 0, 27, 0, 0, 0, 0, 0, 0, 0 ' left: 1 4 7 CLEAR
