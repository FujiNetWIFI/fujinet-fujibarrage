; pages.inc -- which page holds what (barrage.asm includes each page's file).
PG_BOOT  EQU    0               ; start-up checks, appkeys, the socket, messages
PG_MENU  EQU    1               ; the title, the name, the rooms
PG_NET   EQU    2               ; the link: a step a frame
PG_ROOM  EQU    3               ; the waiting room
PG_SHOP  EQU    4               ; the shop, a round's results, the game's (p_shop.inc)
PG_CAT   EQU    5               ; the CATALOG, in ROM (tools/arc/mkcat_arc.py)
PG_SEND  EQU    6               ; the link's sending side (NPOST's FARPG)
PG_PLAY  EQU    7               ; the field: the screen, the HUD, aiming, the sprites
PG_FLD   EQU    8               ; the field's drawing: terrain, the tanks
PG_PLY   EQU    9               ; the shot player: a segment played (p_ply.inc)
PG_OPS   EQU    10              ; its paths, flows, beams (p_ops.inc)
PG_TEST  EQU    11              ; the offline test scenes (p_test.inc)
PG_TRK   EQU    12              ; the shot's tracks (p_trk.inc)
PG_BLS   EQU    13              ; its blasts drawn: the ellipses (p_bls.inc)
PG_SET   EQU    14              ; its settles (p_set.inc)
PG_FLW   EQU    15              ; its flows (p_flw.inc)
PG_HUD   EQU    16              ; the field's HUD, items line, defence window, talk
PG_TSCR  EQU    17              ; the screens' offline test scenes (p_tscr.inc)
PG_SETS  EQU    18              ; the waiting room's settings view (p_sets.inc)
PG_MSG   EQU    19              ; the HUD's messages: talk, damage, shields
PG_SND   EQU    20              ; the sound's notes (the vblank reads them: fixa.inc
PG_SNDT  EQU    21              ;  VBN*), its test scenes; the title's notes
NPAGES   EQU    22
PHFIRST  EQU    PG_ROOM         ; the pages main.inc's tables name (checkdepth)
PHLAST   EQU    PG_PLAY

; the boot page's entries
E_SAVENM EQU    E_EXTRA         ; key 0 = MYNAME
E_CONN   EQU    E_EXTRA+3       ; "CONNECTING", the socket: R0 = 1 open
E_JOIN   EQU    E_EXTRA+6       ; the room kept, "JOINING", the socket: R0 = 1
E_CLOSE  EQU    E_EXTRA+9       ; the socket closed
E_CLRRM  EQU    E_EXTRA+12      ; key 0x13 emptied, no room
E_FAIL   EQU    E_EXTRA+15      ; the socket closed, message R2 (b7: the room forgotten)
; the link's
E_HELLO  EQU    E_STATE         ; in a room: counters, the state zeroed, HELLO
E_INIT   EQU    E_TICK          ; a fresh link's counters (the room list)
E_DONE   EQU    E_EXTRA         ; the segment played: D owed
E_TKOFF  EQU    E_EXTRA+3       ; the block tanks off the field (TBN 0)
; the field's (p_play.inc)
E_VBL    EQU    E_EXTRA         ; the vblank's: the sprites
; the field's drawing's (p_fld.inc)
E_COL    EQU    0               ; the decoder's: a TERRAIN column entry's byte
E_TANKS  EQU    3               ; the tanks as the state has them (TKSTATE)
E_TKDRAW EQU    6               ; the block tanks off, then again (TKDRAW)
E_COLEND EQU    9               ; a G frame's end: the columns waiting drawn
E_TKSEL  EQU    12              ; the shot's tanks: the sprites' seats, the blocks
; the player's (p_ops.inc; p_ply.inc's E_ENTER plays a segment)
E_OSTEP  EQU    0               ; a frame of the paths, flows, beams
E_OPATH  EQU    3               ; OP_PATH, OP_BEAM (their arguments at PPC; the
E_OBEAM  EQU    6               ;  bytes they took: R2)
E_OSEGEND EQU   9               ; the segment's end: the paths stop
E_TSTEP  EQU    0               ; p_trk.inc: every track a step
E_TLOAD  EQU    3               ;  OP_ARC, OP_STOP (their arguments at PPC)
E_TSTOP  EQU    6
E_BGROW  EQU    0               ; p_bls.inc: blast PBLK grown to its n
E_BEND   EQU    3               ;  its end
E_SAPPLY EQU    0               ; p_set.inc: settle PBLK lands
E_SNOW   EQU    3               ;  the OP_SETTLE at PPC lands now
E_FSTEP  EQU    0               ; p_flw.inc: every flow a frame
E_OFLOW  EQU    3               ;  OP_FLOW (at PPC; R2 its bytes)
E_FSEGEND EQU   6               ;  the segment's end: the flows run out
; the test page's (p_test.inc)
E_TFIX   EQU    E_ENTER         ; the fixture's field and tanks, the shot's start
E_TSEG   EQU    E_STATE         ; its next segment into SCRIPT: R0 = 0 none left
E_TEND   EQU    E_TICK          ; the shot's end
; the field's UI page's (p_hud.inc)
E_UI     EQU    0               ; a frame of it (p_play.inc PTICK): R2 = UF_*
E_HUD    EQU    3               ; the HUD's rows drawn (none while a message shows)
E_UIOFF  EQU    6               ; the field's UI shut, AIMV zeroed (a new field,
                                ;  a script's end, our turn's start)
; the messages' page's (p_msg.inc; the PLY page's far calls)
E_OTALK  EQU    0               ; OP_TALK at PPC
E_ODMG   EQU    3               ; OP_DAMAGE at PPC
E_OSHLD  EQU    6               ; OP_SHIELD at PPC
E_MSTEP  EQU    9               ; a step of the message: a letter, its time
E_MFLUSH EQU    12              ; the segment's end: the talk's letters all now
UF_AIM   EQU    1               ; E_UI's R2: the disc and keys are the aim's
UF_SEL   EQU    2               ;  the sprites' seats again (the target changed)
; the settings view's (p_sets.inc; p_room.inc's far calls)
E_SDRAW  EQU    0               ; the view (after TEXTSCR)
E_SALL   EQU    3               ; its slots again
E_SLINE  EQU    6               ; slot WI
E_SCHG   EQU    9               ; the chosen one's value changed: T1 key, T2 value
; the screens' test page's (p_tscr.inc)
E_TSND   EQU    0               ; p_snd.inc: the sound's test scenes (11, 12)
E_TSET   EQU    E_ENTER         ; the scene's state: R0 the screen (S_*)
E_TTICK  EQU    E_STATE         ; its frame: R0 1 = a new state
E_TFLD   EQU    E_TICK          ; the field's scenes: their seats (after E_TFIX)
; the catalog's
E_INAME  EQU    E_ENTER         ; item T0's name in T1 cells at the display (R2)

; the boot page's messages (R2 to E_FAIL; MSGID)
M_JOIN   EQU    0
M_NOANS  EQU    1
M_LOST   EQU    2
M_NOROOM EQU    3
M_VERSION EQU   4
M_CONN   EQU    5
M_QUIET  EQU    6               ; (none)
M_CAPS   EQU    7
M_FULL   EQU    8
