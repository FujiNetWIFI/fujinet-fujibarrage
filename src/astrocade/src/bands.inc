; bands.inc -- the text screens' colours (resident: the frame interrupt reads
; them whatever page is mapped). A text screen is all the right palette
; (HORCB 0): R0 its ground (P_TEXT's), R1-R3 its inks, which a band (a text
; row, TXH lines, or a run of them) sets: HORCB, R3, R2, R1, L1, then the
; next band's scanline (TBLINE), TVBLLN after the last. Inks: 3 white, 2
; yellow, 1 cyan unless a table says otherwise.

; the kept colours: L0, L2, L3, R0 (black)
P_TEXT: DB      00H,00H,00H,00H

; plain: white, yellow, cyan on black
B_PLAIN: DB     0,07H,76H,0D5H,0,TVBLLN
B_MSG   EQU     B_PLAIN
; nothing shown (the field arriving over a text screen)
B_BLANK: DB     0,0,0,0,0,TVBLLN

; the title: the logo (rows 2-3) a fire gradient two lines a band, the
; tag lines, PRESS FIRE
TLOGO   EQU     2*TXH
B_TITLE: DB     0,07H,76H,0D5H,0
        TBLINE  TLOGO
        DB      0,77H,77H,77H,0
        TBLINE  TLOGO+2
        DB      0,6EH,6EH,6EH,0
        TBLINE  TLOGO+4
        DB      0,66H,66H,66H,0
        TBLINE  TLOGO+6
        DB      0,5DH,5DH,5DH,0
        TBLINE  TLOGO+8
        DB      0,5BH,5BH,5BH,0
        TBLINE  TLOGO+10
        DB      0,07H,76H,0D5H,0                ; the rest: white, yellow, cyan
        DB      TVBLLN

; the name and the rooms: the heading (row 0) yellow; the list white (a
; room playing: green), the cursor cyan; the hints (rows 11-12) yellow
B_MENU: DB      0,76H,07H,0D5H,0
        TBLINE  1*TXH
        DB      0,07H,0A4H,0D5H,0
        TBLINE  11*TXH
        DB      0,76H,07H,0D5H,0
        DB      TVBLLN

; the waiting room, the results: row 0 the heading (yellow, white, cyan); a
; seat a row (rows 1-10): its name white (3), its kind grey (2), a block of
; its colour (1: the seat's); rows 11-12 what next
B_SEATS: DB     0,76H,07H,0D5H,0
        TBLINE  1*TXH
        DB      0,07H,04H,52H,0                 ; seat 0
        TBLINE  2*TXH
        DB      0,07H,04H,7EH,0
        TBLINE  3*TXH
        DB      0,07H,04H,07H,0
        TBLINE  4*TXH
        DB      0,07H,04H,0A4H,0
        TBLINE  5*TXH
        DB      0,07H,04H,13H,0
        TBLINE  6*TXH
        DB      0,07H,04H,64H,0
        TBLINE  7*TXH
        DB      0,07H,04H,4EH,0
        TBLINE  8*TXH
        DB      0,07H,04H,86H,0
        TBLINE  9*TXH
        DB      0,07H,04H,6AH,0
        TBLINE  10*TXH
        DB      0,07H,04H,0C4H,0                ; seat 9
        TBLINE  11*TXH
        DB      0,76H,07H,0D5H,0
        DB      TVBLLN

; the settings (the waiting room's other view): a setting's label yellow
; (3), its value white (2), the cursor cyan (1) (labels yellow, values
; white: the user, 2026-11-23)
B_SETS: DB      0,76H,07H,0D5H,0,TVBLLN

; the shop: the heading (rows 0-1: the cash) yellow and white; the list
; white, owned counts cyan; the item's line (row 11) and the hints yellow
B_SHOP: DB      0,76H,07H,0D5H,0
        TBLINE  2*TXH
        DB      0,07H,04H,0D5H,0
        TBLINE  11*TXH
        DB      0,76H,07H,0D5H,0
        DB      TVBLLN
