; look.inc -- the colours. The user chose look B with the "night" colours
; (2026-11-23): black sky and border, red dirt, yellow fire/shells/block
; tanks; the 4 sprites the seats' tanks (M2 sets the field's registers:
; multicolour, the two ink/ground pairs).
;
; The text screens: one ground for the screen and one set of four inks
; (BGCOL b3: 0 white/cyan/magenta/blue, 1 yellow/green/red/black). M4
; (2026-11-24, the TODO's "labels yellow, values white; names in the seat's
; colour where the hardware allows"): set 0 -- white and yellow can't share
; a screen, so values (and plain text) white, labels cyan; a seat's name in
; its tank's colour where set 0 has it (seats mod 5: 1 cyan, 3 magenta;
; white for 0, for 2 -- green isn't in set 0 -- and for 4 -- blue on black
; reads badly); warnings magenta. One constant each: change them here.
;
;   LOOKBG  BGCOL for the text screens (ground, ink set)
;   TXTI    text, values   LABI  labels   USI  ours (the cursor)
;   WARNI   warnings, the host   LISTI1  a room playing, in the room list
;   NAMEI   a seat's name's ink, by seat mod 5

LOOKBG  EQU     CBLACK          ; set 0: white, cyan, magenta, blue
I_WHT   EQU     INK0
I_CYN   EQU     INK1
I_MAG   EQU     INK2
I_BLU   EQU     INK3
TXTI    EQU     I_WHT
LABI    EQU     I_CYN
USI     EQU     I_CYN
WARNI   EQU     I_MAG
LISTI1  EQU     I_CYN
NAMEI0  EQU     I_WHT           ; seats 0, 5
NAMEI1  EQU     I_CYN           ; 1, 6
NAMEI2  EQU     I_WHT           ; 2, 7 (green: none in set 0)
NAMEI3  EQU     I_MAG           ; 3, 8
NAMEI4  EQU     I_WHT           ; 4, 9 (blue: too dark on black)

; The field (26-row block graphics, multicolour; tools/arc/mock_arc.py's
; Mock.build, "night"): BGCOL ink HOT (yellow), ground SKY (black, the
; border too); GFXMOD ink DIRT, ground DIRT (red), rows starting in block
; graphics, 26 of them. The HUD's text is HOT on DIRT (GTEXT | glyph); its
; timer DIRT on SKY. The sprites (the tanks) in their seats' colours: seat
; s is FSEATC + s mod 5.
FSKY    EQU     CBLACK
FDIRTC  EQU     CRED
FHOT    EQU     CYELLOW
FBGCOL  EQU     $80|(FHOT<<3)|FSKY              ; 8x8 cells
FGFXMOD EQU     $C0|(FDIRTC<<3)|FDIRTC          ; block graphics, 26 rows
HUDI    EQU     $40             ; the HUD's text: ink HOT, ground DIRT
HUDTI   EQU     $80             ; the timer: ink DIRT, ground SKY
