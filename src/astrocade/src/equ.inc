; equ.inc -- the screen's layout, the raster's, the pages' entries, the
; client's own numbers. The hardware's are in hvglib.inc and fujilib.inc, the
; protocol's in proto.inc (tools/astro/mkproto.py from src/proto.h), the
; colours' in look.inc (tools/astro/mklook_astro.py).

; ---- the screen ------------------------------------------------------------------
; The field screen: the HUD (lines 0-4: one row of the 3x5 font), the field
; (lines 5-60: 160x56, 2 bits a pixel, src/platform-specific/field.h's
; values: tank 0, sky 1, dirt 2, fire 3), then the border (power and energy
; bars, then the ground's colour). The text screens show TLINES lines over
; the same RAM (the field and the shot's tables: a new round sends the whole
; field again).
SCR     EQU     NORMEM          ; 4000H
MAGOFF  EQU     4000H           ; a magic write's address = screen address - 4000H
HUDH    EQU     5               ; the HUD: lines 0-4
FROW    EQU     HUDH            ; the field's first line
FIELD_W EQU     160
FIELD_H EQU     56
FLINES  EQU     FIELD_H
LINES   EQU     FROW+FLINES     ; 61 lines on the field screen
FIELD   EQU     SCR+FROW*BYTEPL ; the field's first byte
TANK_W  EQU     7
TANK_H  EQU     5
PAR     EQU     54              ; a pixel's width/height in 64ths
SCRIPT_BUF EQU  240             ; the server cuts segments to fit (one op can be 230)
TLINES  EQU     82              ; the text screens' lines: 13 rows of TXH
TXH     EQU     6               ; a text row: the 4x6 glyph (its top row blank but '$')
PX_TANK EQU     0
PX_SKY  EQU     1
PX_DIRT EQU     2
PX_FX   EQU     3

; ---- the raster -------------------------------------------------------------------
; The field's colours change every NBL lines (a band): its sky (L1 = R1), its
; two tanks' (L0 left of the split, R0 right of it: HORCB). Dirt (L2 = R2)
; and fire (L3 = R3) are the frame's, from VBISR. Under the field the border
; shows HORCB's b6-7 colour: the bars' bands paint colour 0 (L0 left of the
; split: the bar, R0 right: its trough); VBISR's border is colour 2, the
; ground. A band's interrupt comes a scanline early (BANDOFS) and pads BPAD
; NOPs so its five writes start in the band's first scanline.
NBL     EQU     4               ; lines a band
NFB     EQU     FLINES/NBL      ; 14 field bands
NBARS   EQU     2               ; the power and energy bars
NBANDS  EQU     NFB+NBARS       ; 16 entries in a phase table
BARL    EQU     LINES           ; the power bar: lines 61-64; energy 65-68
VBLINE  EQU     LINES+NBARS*NBL ; the frame interrupt's line (69): the border
                                ;  below is the ground's colour
VBLLN   EQU     2*VBLINE-1      ; its scanline
BANDOFS EQU     1
BPAD    EQU     12              ; NOPs: MAME's clean window is 8-16 (idle and busy)
BLINE   MACRO   ln              ; the INLIN of a band starting at line ln
        DB      2*(ln)-BANDOFS
        ENDM
VERBLV  EQU     LINES*2         ; the field screen's end (VERBL, in scanlines)
TVERBL  EQU     TLINES*2        ; the text screens'
TVBLLN  EQU     204             ; a text screen's frame interrupt (FujiBlast's)
TBPAD   EQU     5               ; a text band's NOPs (FujiBlast's BPAD)
TBLINE  MACRO   ln              ; a text band's INLIN: one starting at line ln
        DB      2*(ln)-BANDOFS
        ENDM

; ---- the pages -------------------------------------------------------------------
; Every page opens with the 55H header (the BIOS's menu link, the name,
; RSTENT), then its entries, a JP each.
PGENT   EQU     2007H
E_ENTER EQU     PGENT           ; the screens' pages: drawn afresh
E_STATE EQU     PGENT+3         ;   a new state
E_TICK  EQU     PGENT+6         ;   a frame
E_EXTRA EQU     PGENT+9

; ---- input -------------------------------------------------------------------------
D_UP    EQU     01H             ; INDIR bits: the handle's own (port 10H b0-3)
D_DOWN  EQU     02H
D_LEFT  EQU     04H
D_RIGHT EQU     08H
K_NONE  EQU     0FFH            ; EVKEY: a key that went down this frame
K_ENT   EQU     0AH             ; '=' (0-9: the digits)
K_CLR   EQU     0BH             ; 'C'
K_MUTE  EQU     0CH             ; 'MR' (FRAME takes it: never seen by the game)
K_BACK  EQU     0DH             ; 'CE'
K_PLUS  EQU     0EH             ; '+'
K_MINUS EQU     0FH             ; '-'
K_MUL   EQU     10H             ; 'x'
K_DIV   EQU     11H             ; the divide key
K_PCT   EQU     12H             ; '%'
K_UP    EQU     13H             ; the up arrow
K_DN    EQU     14H             ; the down arrow
K_DOT   EQU     15H             ; '.'
K_MS    EQU     16H             ; 'MS'
K_CH    EQU     17H             ; 'CH'

; ---- flags (FLAGS) ------------------------------------------------------------------
FL_ROOM  EQU    01H             ; key 0x13 holds our server and table
FL_TEST  EQU    02H             ;   an offline test scene: the link is quiet
FL_MUTE  EQU    04H             ;   all sound off
FL_SNDON EQU    10H             ;   the cart is ours: the frame interrupt plays the sound
FL_FIELD EQU    20H             ;   the field screen's raster (else a text screen's)
FL_FIRE  EQU    40H             ;   fire is live: its colour cycles
FL_WIPED EQU    80H             ;   a text screen since a whole field began: a
                                ;   diff can't apply (PTERR: all of it, asked again)

NONE    EQU     0FFH
MAX_ROOMS EQU   8               ; the room list's records (src/game.h)

; ---- the link (p_net.inc) ------------------------------------------------------------
NETDEV  EQU     71H             ; N1:
NCOPEN  EQU     4FH             ; 'O'
NCCLOSE EQU     43H             ; 'C'
NCREAD  EQU     52H             ; 'R'
NCWRITE EQU     57H             ; 'W'
NCSTAT  EQU     53H             ; 'S'
NMRDWR  EQU     0CH             ; OPEN: read and write
FCCKEY  EQU     0DBH            ; CLOSE_APPKEY
FCOKEY  EQU     0DCH            ; OPEN_APPKEY
FCRKEY  EQU     0DDH            ; READ_APPKEY
FCWKEY  EQU     0DEH            ; WRITE_APPKEY
NK_READ  EQU    1               ; NKIND: the transaction out
NK_STATUS EQU   2
NK_WRITE EQU    3
NK_OTHER EQU    4               ; (a blocking one's, from the boot page)
NL_LINK  EQU    1               ; NLINK: the socket is open
NL_JOINED EQU   2               ;   HELLO sent: in a room
NL_HEARD EQU    4               ;   a commit came since
EV_STATE EQU    1               ; NEVT: what this frame's step brought
EV_ERROR EQU    2
EV_LOST  EQU    4
EV_NOANS EQU    8               ;   no state in JOIN_WAIT frames
EV_SCRIPT EQU   10H             ;   a segment is whole (SEGRDY)
EV_TERR  EQU    20H             ;   terrain came
TX_HELLO EQU    01H             ; TXF: what the next WRITE carries
TX_ACT   EQU    02H
TX_DONE  EQU    04H
TX_CREDIT EQU   08H
TX_PING  EQU    10H
TX_RESYNC EQU   20H
TX_ROOMS EQU    40H
TX_BYE   EQU    80H

; ---- the screens in a room (src/main.c) --------------------------------------------
S_NONE   EQU    0
S_WAIT   EQU    1
S_SHOP   EQU    2
S_FIELD  EQU    3
S_RESULTS EQU   4
S_OVER   EQU    5
JOIN_WAIT EQU   900             ; frames for the first state before giving up

; ---- sounds: SND_* (proto.inc: the script's), the UI's, the tunes ------------------
SND_BOOM0 EQU   16              ; a boom's sizes 0-3 (OP_SOUND's size)
UI_TICK  EQU    20              ; (tools/astro/mksnd_astro.py's IDS, TUNES)
UI_SELECT EQU   21
UI_GO    EQU    22
UI_ERROR EQU    23
UI_TURN  EQU    24
UI_TALK  EQU    25
MUS_TITLE EQU   26
MUS_WIN  EQU    27
MUS_OVER EQU    28
MUS_SHOP EQU    29
