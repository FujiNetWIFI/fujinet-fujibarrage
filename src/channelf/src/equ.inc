; ---------------------------------------------------------------------------
; equ.inc -- the console, the cart, the screen's layout.
;
; The cart is fujinet-firmware pico/channelf at protocol 2 (APPBANK pages and
; the tick, fuji_mailbox.h): FujiBlast's src/channelf equ.inc. The server's
; numbers are build/channelf/proto.inc (src/proto.h).
; ---------------------------------------------------------------------------

; --- the BIOS --------------------------------------------------------------
PUSHK   EQU 00107H              ; push K onto the K-stack (clobbers r7, A)
POPK    EQU 0011EH              ; pop it back
; The BIOS's K-stack: KU, KL at the scratch register r59 points to, r59 += 2.
; The BIOS starts it at r40 (9 levels); nothing here uses r16-r39, so it
; starts at r16: 21 levels (tools/chf/checkdepth.py keeps chains within).
KSTK0   EQU 16
KDEPTH  EQU 21
BCLRSCR EQU 000D0H              ; clear the screen to value r3 >> 6, palette 0

; --- the cart: its status page ($FC00) -------------------------------------
FNACKSQ EQU 0FC00H              ; echoes SEQ when the reply is ready
FNSTATE EQU 0FC01H
FNERR   EQU 0FC02H              ; FN_ERR_* of the last transaction
FNRCMD  EQU 0FC03H              ; $06 ACK / $15 NAK
FNRXLO  EQU 0FC04H              ; reply length, little-endian
FNRXHI  EQU 0FC05H
FNMAG0  EQU 0FC09H              ; 'F'
FNMAG1  EQU 0FC0AH              ; 'N'
FNPVER  EQU 0FC0BH              ; 2: APPBANK and the tick
FNTICK  EQU 0FC0EH              ; 60 Hz count, LE: read HI, LO, HI

FNREPLY EQU 0F800H              ; the whole 1K reply, read in place

; --- console -> cart: stores ------------------------------------------------
FNREG   EQU 0FD00H              ; store V at +n: register n = V
FNBANK  EQU 0FD80H              ; store anything at +n: page n at $4800
FNTX    EQU 0FF00H              ; store anywhere in the page: append

FR_DEV  EQU 000H
FR_CMD  EQU 001H
FR_NPAR EQU 002H
FR_DRST EQU 005H
FR_SEQ  EQU 010H

FD_FUJI EQU 070H
FD_NET  EQU 071H

; N: commands (fujinet-firmware NDevice)
NC_OPEN EQU 04FH                ; 'O'
NC_CLOS EQU 043H                ; 'C'
NC_READ EQU 052H                ; 'R'
NC_WRIT EQU 057H                ; 'W'
NC_STAT EQU 053H                ; 'S'
NM_RW   EQU 00CH                ; open mode: read and write

; appkeys (the FujiNet lobby's: creator 1, app 1)
FC_AKCLS EQU 0DBH               ; CLOSE_APPKEY
FC_AKOPN EQU 0DCH               ; OPEN_APPKEY
FC_AKRD EQU 0DDH                ; READ_APPKEY
FC_AKWR EQU 0DEH                ; WRITE_APPKEY

; --- the page window -------------------------------------------------------
PGWIN   EQU 04800H
PGSIZE  EQU 02000H

; --- the console's I/O ports -----------------------------------------------
; port 0: bits 6,5 the VRAM write strobe ($60 then $50); bits 0-3 the
;         console buttons when read (bit 6 clear)
; port 1: pixel value in bits 7-6 (complemented); the right controller when read
; port 4: column (complemented); the left controller when read
; port 5: row in bits 5-0 (complemented); bits 7-6 the tone (SNDB holds them)
; After the $50 nothing goes to a port for SETTLE clocks (flags.inc): the
; rigs' settle guard (tools/chfsim) fails any run that does.

; --- pixel values as port-1 codes: what the field and the shadows hold ------
CV0     EQU 0C0H                ; value 0: the row's background
CV1     EQU 080H                ; 1: blue (white in palette 0)
CV2     EQU 040H                ; 2: red
CV3     EQU 000H                ; 3: green

; the field's values (the Atari's PX_*: tank 0, sky 1, dirt 2, fire 3 in the
; protocol's sense, as port-1 codes here)
PX_SKY  EQU CV0                 ; the background: the round's sky by row
PX_TANK EQU CV1                 ; blue
PX_FX   EQU CV2                 ; red
PX_DIRT EQU CV3                 ; green

; --- the screen -------------------------------------------------------------
; VRAM is 128 x 64; the TV shows columns 4-105 and rows 4-61. The field is
; 102 x 46 from (FX0, FY0); two HUD text rows under it; text screens 9 rows
; of 24 cells (4 x 6, the 3 x 5 font) from (TX0, TY0).
FX0     EQU 4                   ; the field's left column on the screen
FY0     EQU 4                   ; its top row
FW      EQU 102
FH      EQU 46
TX0     EQU 7                   ; the text grid's left column (96 px wide)
TY0     EQU 5                   ; text screen row 0's top line
TCOLS   EQU 24
TROWS   EQU 9                   ; text screen rows 0-8
HROW0   EQU 9                   ; the HUD's two rows are text rows 9 and 10
NTROWS  EQU 11
HUDY0   EQU 50                  ; their top lines
HUDY1   EQU 56
PALCOL  EQU 125                 ; the palette's two columns

; --- our shape, as HELLO says it (server client.ChannelF) --------------------
PAR     EQU 48                  ; a pixel's width / height in 64ths
TANK_W  EQU 5
TANK_H  EQU 3
