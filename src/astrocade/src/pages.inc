; pages.inc -- which page holds what (barrage.asm lays them out). Page 1 is
; the resident half (3000H-3AFFH), never selected.
PG_BOOT  EQU    0               ; start-up checks, appkeys, the socket, messages
PG_NET   EQU    2               ; the link: a step a frame
PG_PLAY  EQU    3               ; the field: the HUD, aiming, the items line, tanks, bands
PG_PLY   EQU    4               ; the script: the timeline, tracks, blasts, settles
PG_OPS   EQU    5               ; the script: paths, flows, beams
PG_MENU  EQU    6               ; the title, the name, the rooms
PG_ROOM  EQU    7               ; the waiting room, the settings
PG_SHOP  EQU    8               ; the shop, a round's end, the game's
PG_CAT   EQU    9               ; the catalog (the server's, in ROM)
PG_SND   EQU    10              ; the sound: its tables and its tick
PG_ART   EQU    11              ; the logo, the title's art
PG_TEST  EQU    12              ; the rigs' offline scenes, the fixture
NPAGES   EQU    13

; the boot page's entries
E_SAVENM EQU    E_EXTRA         ; key 0 = MYNAME
E_CONN   EQU    E_EXTRA+3       ; "CONNECTING", the socket: A = 1 open
E_JOIN   EQU    E_EXTRA+6       ; the room kept, "JOINING", the socket: A = 1
E_CLOSE  EQU    E_EXTRA+9       ; the socket closed
E_CLRRM  EQU    E_EXTRA+12      ; key 0x13 emptied, no room
E_MSG    EQU    E_EXTRA+15      ; message MSGID
E_FAIL   EQU    E_EXTRA+18      ; the socket closed, message MSGID (b7: the room forgotten)
; the link's
E_HELLO  EQU    E_STATE         ; in a room: counters, the state zeroed, HELLO
E_INIT   EQU    E_TICK          ; a fresh link's counters (the room list)
E_DONE   EQU    E_EXTRA         ; the segment played: D owed
; the field's (PLAY)
E_LOAD   EQU    E_EXTRA         ; the whole field coming (a text screen blanked)
E_COLSKY EQU    E_EXTRA+3       ; column B all sky
E_COLRUN EQU    E_EXTRA+6       ; column B, rows D to E-1, dirt
E_SCRIPT EQU    E_EXTRA+9       ; a segment whole: play it

; the boot page's messages (MSGID)
M_JOIN   EQU    0
M_NOANS  EQU    1
M_LOST   EQU    2
M_NOROOM EQU    3
M_VERSION EQU   4
M_CONN   EQU    5
M_QUIET  EQU    6               ; (none)
M_CAPS   EQU    7
