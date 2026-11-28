; equ.inc -- FujiBarrage's own numbers on the Studio II. The protocol's are
; in build/studio2/proto.inc (tools/chf/mkproto.py from src/proto.h); the
; cart's in rt/fujinet.inc; the RAM's in ram.inc.

; ---- our shape (the HELLO's bytes 18-29; server/internal/client Studio2) ------
FW      EQU     64              ; the field: 64 dots ...
FH      EQU     108             ;  ... by 108 lines, at the top of the picture
PAR     EQU     210             ; a dot is ~3.3 lines wide (MAME's 100x246 at 4:3)
HZ      EQU     60
TANK_W  EQU     3
TANK_H  EQU     4
SCRIPT_BUF EQU  1536
CAT_MAX EQU     1664            ; the catalog (K frames), as the Channel F's
HUD_Y1  EQU     108             ; the HUD's rows (the short font: 5 lines + 1)
HUD_Y2  EQU     114

; ---- the field's model (fld.inc) ---------------------------------------------------
; TOPS[x] is column x's TERRAIN entry's first byte: 00-EF dirt from that row
; down, F0 none, F0+n n spans; the spans (top, end) are in SPN: page
; HI(SPN) + x/8, offset 32 (x mod 8).
E_NONE  EQU     0F0H

; ---- banks ($C400-$CFFF) -----------------------------------------------------
; build.sh assembles barrage.asm once a bank (-D BANK=n): fixed code reaches
; a bank only through its entries (an FJMP each, from $C400), never by its
; labels (only that bank's assembly has them).
BK_BOOT EQU     0               ; the appkeys, the socket, the messages, the
                                ;  title, the name, the room list
BK_ROOM EQU     1               ; the waiting room, the settings
BK_SHOP EQU     2               ; the shop, the results
BK_FIX  EQU     3               ; (TEST) FIXBUF: the rigs' fixture
NBANKS  EQU     3+TEST          ; (build.sh's too)
E_AKLOAD   EQU  0C400H          ; BK_BOOT
E_TITLE    EQU  0C403H
E_SAVENM   EQU  0C406H
E_SAVERM   EQU  0C409H
E_CLRRM    EQU  0C40CH
E_SOPEN    EQU  0C40FH
E_SCLOSE   EQU  0C412H
E_ROOMS    EQU  0C415H
E_MSG      EQU  0C418H
E_MSGW     EQU  0C41BH
E_WENTER   EQU  0C400H          ; BK_ROOM
E_WSTATE   EQU  0C403H
E_WPASS    EQU  0C406H
E_TSCENE   EQU  0C409H
E_SENTER   EQU  0C400H          ; BK_SHOP
E_SSTATE   EQU  0C403H
E_SPASS    EQU  0C406H
E_RENTER   EQU  0C409H
E_RSTATE   EQU  0C40CH
E_RPASS    EQU  0C40FH

; ---- the shot player's tables (p_ply.inc) ------------------------------------------
NTRK    EQU     6               ; tracks (the server's Slots for us)
NBLAST  EQU     8               ; blasts and dirt balls
NSETTLE EQU     8               ; settles waiting to land
NPATH   EQU     4               ; paths (rollers, diggers): slots 0-3
NFLOW   EQU     4
NBEAM   EQU     4

; ---- the network --------------------------------------------------------------
NETDEV  EQU     71H             ; N1:
NCOPEN  EQU     4FH
NCCLOSE EQU     43H
NCREAD  EQU     52H
NCSTAT  EQU     53H
NCWRITE EQU     57H
NMRW    EQU     0CH             ; OPEN mode: read and write
FCACK   EQU     06H
FCCKEY  EQU     0DBH            ; CLOSE_APPKEY
FCOKEY  EQU     0DCH            ; OPEN_APPKEY
FCRKEY  EQU     0DDH            ; READ_APPKEY
FCWKEY  EQU     0DEH            ; WRITE_APPKEY
AK_CREATOR EQU  1               ; the lobby's keys: creator 1, app 1
AK_APP  EQU     1
AK_NAME EQU     0               ; our name
AK_ROOM EQU     13H             ; FujiBarrage's room key: tcp://host:port/?table=id

MAX_ROOMS EQU   8               ; the room list's records (src/game.h)
NONE    EQU     0FFH

; the field's own lines (fieldui.inc, msg.inc)
M_OFF   EQU     0               ; MENU: the items line shut,
M_ITEM  EQU     1               ;  an item being chosen,
M_TARGET EQU    2               ;  a target,
M_DRIVE EQU     3               ;  driving
BANNER_J EQU    180             ; YOUR TURN! stays (jiffies)
TALK_J  EQU     120             ; a talk (play.c talk_left)
EVENT_J EQU     60              ; a hit, a shield
MK_TALK EQU     1               ; MSGK
MK_HIT  EQU     2
MK_SHIELD EQU   3
MSGDRT  EQU     4               ; HUDDRT: the message to draw (the player's pass)
