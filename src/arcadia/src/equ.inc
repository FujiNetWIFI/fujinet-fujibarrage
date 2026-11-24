; equ.inc -- the Arcadia 2001, the FujiNet cart's mailbox, and this client's
; own numbers. The protocol's are in proto.inc (build.sh: sed from
; src/proto.h); the catalog's sizes in catequ.inc (tools/arc/mkcat_arc.py).

; ---- the 2637 UVI and the console's RAM -------------------------------------
SCR     EQU     $1800           ; rows 0-12, 16 cells each
SCR2    EQU     $1A00           ; rows 13-25 (26-row mode only: the field's lower half)
SPRPOS  EQU     $18F0           ; sprite n: Y at +2n (256 - line), X at +2n+1
VPOS    EQU     $18FC           ; rows start at line 255 - value ($FF: the top)
PITCH   EQU     $18FD           ; b7 multicolour (the field's), b0-6 pitch
VOLUME  EQU     $18FE           ; b0-2 volume, b3 tone, b4 noise, b5-7 shift
KEYPAD  EQU     $1900           ; P1 keypad columns $1900-$1902 (b3..b0, active high)
PANEL   EQU     $1908           ; b0 Start, b1 Option, b2 Select
UDC     EQU     $1980           ; 8 user glyphs $38-$3F, 8 bytes each; $38-$3B are
                                ; also sprites 0-3's pictures
GFXMOD  EQU     $19F8           ; b7 rows start in block graphics, b6 26 lines
BGCOL   EQU     $19F9           ; b7 8x8 cells, b6 pot mux, b3 ink set, b0-2 ground
SPRC23  EQU     $19FA           ; sprites 2/3: colour b3-5 / b0-2, b7/b6 1 = 8 lines
SPRC01  EQU     $19FB           ; sprites 0/1, the same
CHROW   EQU     $18FF           ; read: the character row the beam is on ($Fx)
POTP1   EQU     $19FF           ; P1 disc: mux clear vertical, set horizontal
                                ; (never a label named POT: AS took one for this)

MUXH    EQU     $40             ; BGCOL: the pot reads the horizontal axis
INK0    EQU     $00             ; a cell's ink (its top two bits)
INK1    EQU     $40
INK2    EQU     $80
INK3    EQU     $C0
GMOSAIC EQU     $C0             ; this exact cell starts block graphics to the row's end
GTEXT   EQU     $40             ; and this one ends it
; the eight colours (sprites, the ground)
CWHITE  EQU     0
CYELLOW EQU     1
CCYAN   EQU     2
CGREEN  EQU     3
CMAGEN  EQU     4
CRED    EQU     5
CBLUE   EQU     6
CBLACK  EQU     7

; The ROM's glyphs worth naming.
GSPACE  EQU     $00
GDIAG   EQU     $01             ; the whole-cell diagonal (the ROM's only slash)
GSOLID  EQU     $03
GDIGIT  EQU     $10             ; '0'
GALPHA  EQU     $1A             ; 'A'
GDOT    EQU     $34
GUDC    EQU     $38             ; user glyph 0 (sprite 0's picture)
; the text screens' user glyphs (TEXTSCR loads all eight: the sprites are
; off on a text screen)
GPCT    EQU     $38
GLEFT   EQU     $39
GBANG   EQU     $3A
GQUERY  EQU     $3B
GCURSOR EQU     $3C
GDASH   EQU     $3D
GCOLON  EQU     $3E
GSLASH  EQU     $3F

; ---- the cart's mailbox (fujinet-firmware pico/arcadia fuji_mailbox.h) ------
FNRDATA EQU     $2B00           ; the reply (slice 0)
FNACKSQ EQU     $2C00
FNERR   EQU     $2C02
FNREPLY EQU     $2C03
FNRXLL  EQU     $2C04
FNRXLH  EQU     $2C05
FNMAGF  EQU     $2C09
FNMAGN  EQU     $2C0A
FNPVER  EQU     $2C0B           ; 2: APPBANK, cart RAM, the tick
FNTICKL EQU     $2C0E           ; 60 Hz, read HI, LO, HI
FNTICKH EQU     $2C0F
FNRSEL  EQU     $2D00
FNBANK  EQU     $2D80           ; read +n: the window shows page n
FNRDAT  EQU     $2E00
FNDATA  EQU     $2F00
FRDEVIC EQU     $00
FRCMD   EQU     $01
FRNPARM EQU     $02
FRDATRS EQU     $05
FRSEQ   EQU     $10
FRRAMP0 EQU     $14             ; cart RAM pointer = $000 + value (RAM-L)
FRRAMP1 EQU     $15             ; $100 + value
FRRAMP2 EQU     $16             ; $200 + value (RAM-H)
FRRAMP3 EQU     $17             ; $300 + value
FRRAMPK EQU     $18             ; each REGDATA stores and steps the pointer
FCACK   EQU     $06
FNDFUJI EQU     $70
NETDEV  EQU     $71             ; N1:
NCOPEN  EQU     $4F             ; 'O'
NCCLOSE EQU     $43             ; 'C'
NCREAD  EQU     $52             ; 'R'
NCWRITE EQU     $57             ; 'W'
NCSTAT  EQU     $53             ; 'S'
NMRDWR  EQU     $0C             ; OPEN: read and write
FCCKEY  EQU     $DB             ; CLOSE_APPKEY
FCOKEY  EQU     $DC             ; OPEN_APPKEY
FCRKEY  EQU     $DD             ; READ_APPKEY
FCWKEY  EQU     $DE             ; WRITE_APPKEY

; ---- the window ---------------------------------------------------------------
WIN     EQU     $0800           ; every page: entries at $0800 + 3k, its id at $0FFF
E_ENTER EQU     0               ; the screens' pages: drawn afresh
E_STATE EQU     3               ;   a new state
E_TICK  EQU     6               ;   a frame
E_EXTRA EQU     9

; ---- input -----------------------------------------------------------------------
D_UP    EQU     $01             ; INDIR bits
D_DOWN  EQU     $02
D_LEFT  EQU     $04
D_RIGHT EQU     $08
JOYLO   EQU     $40             ; a pot under this: up / left
JOYHI   EQU     $C0             ; over this: down / right
K_NONE  EQU     $FF             ; EVKEY: a key that went down this frame
K_ENT   EQU     $0A             ; (0-9: the digits; FIRE is '2' and reads as EVBTN)
K_CLR   EQU     $0B
K_START EQU     $0C
K_OPT   EQU     $0D
K_SEL   EQU     $0E

; ---- the keys the screens share --------------------------------------------------
; Enter: done (the shop, the defence window), the next weapon (aiming), the
; next settings page; Select (or keypad 1): the settings (the room), the
; items line (the field); Clear: shuts the items line, else leaves the room
K_ITEMS EQU     1               ; keypad 1 (the panel's Select is K_SEL)

; ---- sounds: sndequ.inc (tools/arc/mksnd_arc.py): SND_BOOM0-3, UI_*, MUS_* -------

; ---- what HELLO says (docs/PROTOCOL.md, server/internal/client Arcadia) ---------
FIELD_W EQU     48              ; the field: 48x48 blocks (a cell 3x2 blocks)
FIELD_H EQU     48
PAR     EQU     78              ; a block is 2.67 pixels by 4 lines: 78/64 wide
TANK_W  EQU     2
TANK_H  EQU     1
SCRIPT_BUF EQU  160             ; the script buffer (RAM-H)

; ---- the field's screen (p_play.inc; look.inc's colours) ---------------------------
; 26 rows of 8x8 cells in block graphics: rows 0-23 the field (a cell 3x2
; blocks: b2 b1 b0 the top row's left, middle, right third, b5 b4 b3 the
; bottom's; b6 the ink, 1 HOT 0 DIRT; b7 the ground, 1 SKY 0 DIRT), rows
; 24-25 the HUD (text: each row starts with GTEXT). A sky/dirt cell is
; $80 | its dirt blocks, all dirt $00 (as tools/arc/mock_arc.py codes it);
; one with HOT blocks is HOT | $C0 (the ground sky) or $40 (dirt): the
; majority of the rest (all three can't show).
FROWS   EQU     24              ; the field's cell rows
HUDROW  EQU     24              ; the HUD's first row
FVPOS   EQU     228             ; VPOS: the picture from line 27 (centred)
FTOP    EQU     255-FVPOS       ; the field's first line
CSKY    EQU     $80             ; a cell all sky
CDIRT   EQU     $00             ;  all dirt
CDIRTP  EQU     $BF             ;  (the same, as sky | every block dirt)

; ---- the link (p_net.inc) --------------------------------------------------------
NK_READ  EQU    1               ; NKIND: the transaction out
NK_STATUS EQU   2
NK_WRITE EQU    3
NK_OTHER EQU    4               ; (a blocking one's, from the boot page)
NL_LINK  EQU    1               ; NLINK: the socket is open
NL_JOINED EQU   2               ;   HELLO sent: in a room
NL_HEARD EQU    4               ;   a commit came since
NL_DOWED EQU    8               ;   our D unanswered (again every ASK_AGAIN)
EV_STATE EQU    1               ; NEVT: what this frame's step brought
EV_ERROR EQU    2
EV_LOST  EQU    4
EV_NOANS EQU    8               ;   no state in JOIN_WAIT frames
EV_TERR  EQU    $10             ;   terrain came
EV_SCRIPT EQU   $20             ;   a segment is whole (SEGRDY)
TX_HELLO EQU    $01             ; TXF: what the next WRITE carries
TX_ACT   EQU    $02
TX_DONE  EQU    $04
TX_CREDIT EQU   $08
TX_PING  EQU    $10
TX_RESYNC EQU   $20
TX_ROOMS EQU    $40
TX_BYE   EQU    $80
FL_ROOM  EQU    $01             ; FLAGS: key 0x13 holds our server and room
FL_TEST  EQU    $02             ;   an offline test scene: the link is quiet
FL_MUTE  EQU    $04
FL_INSCR EQU    $08             ;   a shot's script under way: its next segment
                                ;   awaited (the PLY page; NSEGW runs flat out)
FL_FIELD EQU    $80             ;   the screen is the field's (p_play.inc): the
                                ;   decoder draws terrain only then; PITCH's b7
                                ;   (multicolour) is this bit
FD_HUD   EQU    $01             ; FDIRTY: the HUD's rows
FD_SPR   EQU    $02             ;   the sprites (the vblank's: E_VBL)
FD_TANKS EQU    $04             ;   the tanks from the state again
FD_SEL   EQU    $08             ;   the sprites' seats again (a guidance target)
BANNERJ  EQU    180             ; YOUR TURN! flashes this long (play.c BANNER_J)

; ---- the screens in a room (main.inc SHOWN) --------------------------------------
S_NONE  EQU     0
S_WAIT  EQU     1               ; the waiting room (p_room)
S_SHOP  EQU     2               ; the shop        (p_shop)
S_FIELD EQU     3               ; the field       (p_play, p_hud)
S_RESULTS EQU   4               ; a round's end   (p_shop)
S_OVER  EQU     5               ; the game's end  (p_shop)

NONE    EQU     $FF
MAX_ROOMS EQU   8               ; the room list's records (src/game.h)
JOIN_WAIT EQU   600             ; frames for the first state before giving up
