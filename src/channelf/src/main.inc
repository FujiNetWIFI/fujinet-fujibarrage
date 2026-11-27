; ---------------------------------------------------------------------------
; main.inc -- the start and the main loop (src/main.c; the Arcadia's
; main.inc in FujiBlast's F8 form). Resident.
;
; The lobby's appkeys say who we are and where to play; else we ask, and
; list the rooms. In a room each pass runs FRAME (the tick, input, sound),
; the link's step (net.inc), then the screen the state's phase wants: the
; waiting room, the shop, a round's end and the game's (PG_SCR), the field
; (PG_HUD: E_STATE on a new state, E_TICK each pass). A segment whole
; (CAP_HOLD: the server sends nothing more until our D) is played on the
; field (the player, in RAM), then our D is owed (NDONE).
; ---------------------------------------------------------------------------

FL_ROOM  EQU 01H                ; FLAGS: key 0x13 holds our server and table
FL_TEST  EQU 02H                ;   an offline test scene: the link is quiet
FL_MUTE  EQU 04H

S_NONE   EQU 0                  ; SHOWS: the screen up
S_WAIT   EQU 1
S_SHOP   EQU 2
S_FIELD  EQU 3
S_ROUND  EQU 4
S_OVER   EQU 5

; the boot page's entries past E_EXTRA (barrage.asm)
E_MENU   EQU E_EXTRA            ; the title, and a name if none (r0 = 1: new)
E_SAVENM EQU E_EXTRA+3          ; key 0 = MYNAME
E_CONN   EQU E_EXTRA+6          ; "CONNECTING", the socket: r0 = 1 open
E_JOIN   EQU E_EXTRA+9          ; the room kept, "JOINING", the socket
E_CLOSE  EQU E_EXTRA+12         ; the socket closed
E_CLRRM  EQU E_EXTRA+15         ; key 0x13 emptied, no room
E_MSG    EQU E_EXTRA+18         ; message MSGID
E_FAIL   EQU E_EXTRA+21         ; the socket closed, message MSGID

M_JOIN   EQU 0                  ; MSGID (b7 on E_FAIL: the room forgotten too)
M_NOANS  EQU 1
M_LOST   EQU 2
M_NOROOM EQU 3
M_VERSION EQU 4
M_CONN   EQU 5
M_QUIET  EQU 6
M_FULL   EQU 7
M_CAPS   EQU 8

; ---- the start ----------------------------------------------------------------
MAIN:   GOPAGE PG_BOOT, E_ENTER         ; the rigs' test hook, the appkeys
        LR A,0
        NI 0FFH
        BZ MI1
        LDB FLAGS
        OI FL_TEST
        STB FLAGS
        GOPAGE PG_TEST, E_ENTER         ; an offline scene, to its end
        LDB TV+1                        ; (4: the screens: the room's loop on
        CI 4                            ;  its canned state, the link quiet)
        BNZ MIDLE0
        LI NL_LINK+NL_JOINED+NL_HEARD
        STB NLINK
        JMP ROOM2
MIDLE0: PI FRAME
        BR MIDLE0
MI1:    GOPAGE PG_BOOT, E_MENU          ; the title, and a name if none
        LR A,0
        NI 0FFH
        BZ TOP
        GOPAGE PG_BOOT, E_SAVENM        ; (a new one: kept)

; ---- the rooms, when no room is known (menus.c room_list) -----------------------
TOP:    LDB SRVTBL
        NI 0FFH
        BZ TOP1
        JMP ROOM
TOP1:   GOPAGE PG_BOOT, E_CONN          ; "CONNECTING", the socket: r0 = 1 open
        LR A,0
        NI 0FFH
        BZ TOPNA
        PI NINIT
        GOPAGE PG_BOOT, E_STATE         ; the list asked for
TOPL:   PI FRAME
        PI NSTEP
        LDB NEVT
        NI EV_LOST
        BNZ TOPNA
        GOPAGE PG_BOOT, E_TICK          ; r0: 0 on, 1 picked, 2 no list
        LR A,0
        NI 0FFH
        BZ TOPL
        CI 1
        BZ ROOM
TOPNA:  LIS M_NOANS                     ; (no room to forget)
        LR 0,A
        JMP TFAIL

; ---- in a room (main.c play_room) -----------------------------------------------
ROOM:   GOPAGE PG_BOOT, E_JOIN          ; the room kept, "JOINING", the socket
        LR A,0
        NI 0FFH
        BNZ ROOM1
        JMP TNOANS
ROOM1:  PI NHELLO                       ; the link's counters, ST zeroed, HELLO
ROOM2:  CLR                             ; (the test scenes come in here)
        STB SHOWS
MAINLP: LISU 7                          ; the K-stack empty again
        LISL 3
        LI KSTK0
        LR S,A
        PI FRAME
        PI NSTEP                        ; the link's step: NEVT
        LDB NEVT
        NI EV_ERROR
        BZ ML0A
        JMP TERR
ML0A:   LDB NEVT
        NI EV_LOST
        BZ ML0B
        JMP TLOST
ML0B:   LDB NEVT
        NI EV_NOANS
        BZ ML0C
        JMP TNOANS
ML0C:   LDB NEVT                        ; the field's columns written: the
        NI EV_TERR                      ;  tanks over them drawn again (a
        BZ ML0C1                        ;  shot cut short is over)
        CLR
        STB INSCR
        PI TKLOST
        LDB SHOWS
        CI S_FIELD
        BNZ ML0C1
        PI TKSTATE
        PI SKYSET                       ; (a night sky's horizon)
ML0C1:  LDB EVKEY                       ; MODE: leave
        CI KEY_LEAVE
        BNZ ML0D
        JMP TLEAVE
ML0D:   LDB NLINK                       ; no state yet: nothing to show
        NI NL_HEARD
        BNZ ML1
        JMP MLEND
ML1:    LDB NEVT                        ; a segment whole: on the field,
        NI EV_SCRIPT                    ;  played, then our D (a shot's
        BZ ML2                          ;  states are the seats before it)
        LDB SHOWS
        CI S_FIELD
        BZ ML1A
        LIS S_FIELD
        LR 0,A
        PI ENTERS
ML1A:   PI PLAYSEG                      ; (the player, in RAM)
        PI NDONE
        JMP MLEND
ML2:    LDB NEVT                        ; a screen is picked only at a commit
        NI EV_STATE                     ;  (ST is patched as frames come)
        BZ ML7
        LDB ST+OFF_PHASE                ; the screen it wants
        CI PH_GAMEOVER
        BC ML4
        LIS PH_WAIT
ML4:    DCI SCRFOR
        ADC
        LM
        LR 4,A
        LDB SHOWS
        XS 4
        BNZ ML5
        LR A,4                          ; the field again: a new round's?
        CI S_FIELD
        BNZ ML6
        LDB ST+OFF_SEQ
        LR 5,A
        LDB SHSEQ
        XS 5
        BZ ML6
ML5:    LR A,4                          ; another screen: drawn afresh
        LR 0,A
        PI ENTERS
        BR ML7
ML6:    LIS E_STATE                     ; the same one: the new state on it
        LR 6,A
        PI SCRCALL
ML7:    LDB SHOWS                       ; and its pass
        NI 0FFH
        BZ MLEND
        LIS E_TICK
        LR 6,A
        PI SCRCALL
MLEND:  INCB PASSES+1                   ; (the rigs count the passes)
        BNZ MLE1
        INCB PASSES
MLE1:   JMP MAINLP

; ENTERS -- screen r0 shown afresh (SHOWS, SHSEQ), its page's E_ENTER.
ENTERS: ENTER
        LR A,0
        STB SHOWS
        LDB ST+OFF_SEQ
        STB SHSEQ
        LIS E_ENTER
        LR 6,A
        PI SCRCALL
        LEAVE

; SCRCALL -- entry r6 of the page that runs the screen SHOWS.
;   Clobbers everything.
SCRCALL: ENTER
        LDB SHOWS
        DCI SCRPG
        ADC
        LM
        LR 9,A                          ; the page
        PI SELPG
        LI PGWIN>>8                     ; its entry: PGWIN + r6 (PK: PC1 = here,
        LR KU,A                         ;  so its POP comes back; the K-stack
        LR A,6                          ;  keeps our own return)
        LR KL,A
        PK
        LEAVE

; by phase: the screen; by screen: the page that draws and runs it
SCRFOR: DB S_WAIT, S_SHOP, S_FIELD, S_FIELD, S_FIELD, S_ROUND, S_OVER
SCRPG:  DB PG_BOOT, PG_SCR, PG_SCR, PG_HUD, PG_SCR, PG_SCR

; the server said no (the room forgotten)
TERR:   LDB NERR
        LR 1,A
        LI M_VERSION+80H
        LR 0,A
        LR A,1
        CI ERR_VERSION
        BZ TFAIL
        LI M_CAPS+80H
        LR 0,A
        LR A,1
        CI ERR_CAPS
        BZ TFAIL
        LI M_FULL+80H
        LR 0,A
        LR A,1
        CI ERR_FULL
        BZ TFAIL
        LI M_NOROOM+80H
        LR 0,A
        BR TFAIL
; the link went: say so, then join again (the room kept)
TLOST:  LIS M_LOST
        LR 0,A
        BR TFAIL
; MODE: give the seat up, BYE (a few passes to get it out), close
TLEAVE: LDB NLINK
        NI NL_JOINED
        BZ TLV1
        LIS ACT_LEAVE
        LR 1,A
        CLR
        LR 2,A
        PI ACTQ
TLV1:   LDB TXF
        OI TX_BYE
        STB TXF
        LI 30
        STB RCOUNT
TLV2:   PI FRAME
        PI NSTEP
        LDB TXF
        NI 0FFH
        BNZ TLV3
        LDB NKIND
        NI 0FFH
        BZ TLV4
TLV3:   DECB RCOUNT
        BNZ TLV2
TLV4:   LI M_QUIET+80H
        LR 0,A
        BR TFAIL
; no answer: give the room up
TNOANS: LI M_NOANS+80H
        LR 0,A
; TFAIL: the socket closed, message r0 (b7: the room forgotten too), then
; the rooms again.
TFAIL:  LR A,0
        STB MSGID
        LDB FLAGS                       ; (a test scene is over too)
        NI 0FFH-FL_TEST
        STB FLAGS
        CLR
        STB SHOWS
        GOPAGE PG_BOOT, E_FAIL
        JMP TOP

; ACTQ -- an action queued: r1 the kind, r2 its args' count (the args in
;   AARGS already); its seq never 0.  Leaf.  Clobbers A, DC0.
ACTQ:   DCI ASEQ
        LM
        INC
        BNZ ACQ1
        LIS 1
ACQ1:   DCI ASEQ
        ST
        LR A,1
        ST                              ; AK
        LR A,2
        STB ANARG
        LDB TXF
        OI TX_ACT
        STB TXF
        POP
