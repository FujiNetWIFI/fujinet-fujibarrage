; main.inc -- the start and the main loop (src/main.c; FujiBlast's
; src/studio2 main.inc).

; ---- start --------------------------------------------------------------------
        RFIT    30
main:   LDI     HI(RAMVARS)
        PHI     RA
        LDR     R8, V_APP               ; our RAM: zeros, up to the stack
mn1:    LDI     0
        STR     R8
        INC     R8
        GHI     R8
        XRI     HI(STKLOW)
        BNZ     mn1
mn2:    LDI     0
        STR     R8
        INC     R8
        GLO     R8
        XRI     LO(STKLOW)
        BNZ     mn2
        FJMP    start

        RFIT    147
start:  PEEK    FN_MAGIC                ; a FujiNet cart, protocol 2?
        XRI     'F'
        BNZ     st_nocart
        PEEK    FN_MAGIC+1
        XRI     'N'
        BNZ     st_nocart
        PEEK    FN_PROTOVER
        SMI     2
        BNF     st_nocart
        GLO     RE
        PUTV    LASTRE
        FJMP    boot
st_nocart:
        OPN     TOP_CLS
        PARS
        P       4
        P       50
        P       TM_SET
        P       'N'
        P       'E'
        P       'E'
        P       'D'
        P       'S'
        P       ' '
        P       'F'
        P       'U'
        P       'J'
        P       'I'
        P       'N'
        P       'E'
        P       'T'
        GOP     TOP_PTEXT
st4:    BR      st4

; ---------------------------------------------------------------------------
; boot -- the rigs' scenes; the appkeys, the title, a name if there is none
;   (kept); then the rooms, or the room the lobby gave us (main.c).
; ---------------------------------------------------------------------------
S_NONE  EQU     0               ; SHOWS: the screen up
S_WAIT  EQU     1
S_SHOP  EQU     2
S_FIELD EQU     3
S_ROUND EQU     4
S_OVER  EQU     5

        RFIT    83
boot:   OPN     TOP_CLS
        SETV    DIRTM, TM_SET           ; (Checkpoint 1's: solid dirt)
        SETV    DCRUST, 0
        SETV    KEV, NONE
        SETV    KLAST, NONE
        SETV    MYSEAT, NONE
        FCALL   fld_clear
        FCALL   tk_init
        IF      TEST
        FCALL   t_check
        BZ      bt5
        FJMP    t_scene
        ENDIF
bt5:    BCALL   BK_BOOT, E_AKLOAD
        BCALL   BK_ROOM, E_TSCENE       ; the title, then a name if none
        BCALL   BK_BOOT, E_TITLE
        BZ      bt6
        BCALL   BK_BOOT, E_SAVENM
bt6:    FJMP    top

; ---- the rooms, when no room is known (menus.c room_list) ------------------
        RFIT    70
top:    OPN     TOP_SPRHIDE
        GET9    SRVTBL
        BZ      tp1
        FJMP    room
tp1:    SETV    MSGID, M_CONN
        BCALL   BK_BOOT, E_MSG
        BCALL   BK_BOOT, E_SOPEN
        BNZ     tpna
        FCALL   ninit
        BCALL   BK_BOOT, E_ROOMS
        BZ      tpna
        FJMP    room
tpna:   SETV    MSGID, M_NOANS          ; (no room to forget)
        FJMP    tfail

; ---- in a room (main.c play_room) --------------------------------------------
        RFIT    49
room:   BCALL   BK_BOOT, E_SAVERM
        SETV    MSGID, M_JOIN
        BCALL   BK_BOOT, E_MSG
        GETV    NLINK
        ANI     NL_LINK
        BNZ     rm_1
        BCALL   BK_BOOT, E_SOPEN
        BZ      rm_1
        FJMP    tnoans
rm_1:   FCALL   nhello                  ; the link's counters, ST zeroed, HELLO
room2:  SETV    SHOWS, S_NONE           ; (the test scenes come in here)
        FJMP    mainlp

; the pass in a room: the frame, the keys, the link's step; a segment whole
; played on the field; the screen the state's phase wants, picked only at a
; commit; its pass.
        RFIT    208
mainlp: FCALL   frame
        FCALL   in_poll
        FCALL   snd_tick
        FCALL   nstep                   ; NEVT
        GETV    NEVT
        ANI     EV_ERROR+EV_LOST+EV_NOANS
        BZ      ml1
        LDN     RA
        ANI     EV_ERROR
        BZ      ml0
        FJMP    terr
ml0:    LDN     RA
        ANI     EV_LOST
        BZ      ml0a
        FJMP    tlost
ml0a:   FJMP    tnoans
ml1:    GETV    KEV                     ; 0: leave
        XRI     0
        BNZ     ml2
        SETV    KEV, NONE
        FJMP    tleave
ml2:    GETV    NLINK                   ; no state yet: nothing to show
        ANI     NL_HEARD
        BZ      mlend
        GETV    NEVT                    ; a segment whole: on the field, its
        ANI     EV_SCRIPT               ;  playing begins
        BZ      ml2b
        GETV    SHOWS
        XRI     S_FIELD
        BZ      ml2a
        LDI     S_FIELD
        FCALL   enters
ml2a:   FCALL   ply_start
ml2b:   GETV    PLAYING                 ; a segment playing: a pass of it,
        BZ      ml3                     ;  then our D at its end
        FCALL   ply_pass
        GETV    PLAYING
        BNZ     mlend
        FCALL   ndone
        BR      mlend
ml3:    GETV    NEVT                    ; a screen is picked only at a commit
        ANI     EV_STATE
        BZ      ml7
        GETS    OFF_PHASE               ; the screen it wants
        SMI     PH_GAMEOVER+1
        BNF     ml4
        LDI     0FFH-PH_GAMEOVER        ; (past the last: the room)
ml4:    ADI     PH_GAMEOVER+1
        ADI     LO(SCRFOR)
        PLO     R8
        LDI     HI(SCRFOR)
        PHI     R8
        LDN     R8
        PLO     R9                      ; R9.0 the screen
        STR     R2
        GETV    SHOWS
        XOR
        BNZ     ml5
        GLO     R9                      ; the field again: a new round's?
        XRI     S_FIELD
        BNZ     ml6
        GETS    OFF_SEQ
        STR     R2
        GETV    SHSEQ
        XOR
        BZ      ml6
ml5:    GLO     R9                      ; another screen: drawn afresh
        FCALL   enters
        BR      ml7
ml6:    LDI     1                       ; the same: the new state on it
        FCALL   scr_call
ml7:    GETV    SHOWS                   ; and its pass
        BZ      mlend
        LDI     2
        FCALL   scr_call
mlend:  INCV    PASSES                  ; (the rigs count the passes)
        BNZ     ml9
        INCV    PASSES+1
ml9:    FJMP    mainlp

; enters: screen D shown afresh (SHOWS, SHSEQ), its enter.
        RFIT    42
enters: PUTV    SHOWS
        SETV    KEV, NONE               ; (a key before it isn't the new screen's)
        SETV    KNEW, 0
        SETV    KNEW+1, 0
        GETS    OFF_SEQ
        PUTV    SHSEQ
        LDI     0
        FJMP    scr_call

; scr_call: entry D (0 enter, 1 state, 2 pass) of the screen SHOWS: in a
;   bank (BCALL's way: bcallv) or fixed (NONE).
        RFIT    50
scr_call:
        PLO     R9                      ; 3 entry
        SHL
        STR     R2
        GLO     R9
        ADD
        PLO     R9
        GETV    SHOWS                   ; + 9 SHOWS
        SHL
        SHL
        SHL
        STR     R2
        GETV    SHOWS
        ADD
        STR     R2
        GLO     R9
        ADD
        ADI     LO(SCRTAB)              ; SCRTAB's: bank, routine
        PLO     R8
        LDI     HI(SCRTAB)
        ADCI    0
        PHI     R8
        LDA     R8                      ; the bank
        PLO     RC
        LDA     R8
        PHI     R9
        LDN     R8
        PLO     R9
        GLO     RC
        XRI     NONE
        BZ      scc1
        GLO     RC
        FCALL   bcallv
        FRET
scc1:   ICALL
        FRET

        PFIT    60
; by phase: the screen; by screen: its enter, state, pass (bank, routine)
SCRFOR: DB      S_WAIT, S_SHOP, S_FIELD, S_FIELD, S_FIELD, S_ROUND, S_OVER
SCRTAB: DB      NONE
        DW      no_op
        DB      NONE
        DW      no_op
        DB      NONE
        DW      no_op
        DB      BK_ROOM
        DW      E_WENTER
        DB      BK_ROOM
        DW      E_WSTATE
        DB      BK_ROOM
        DW      E_WPASS
        DB      BK_SHOP
        DW      E_SENTER
        DB      BK_SHOP
        DW      E_SSTATE
        DB      BK_SHOP
        DW      E_SPASS
        DB      NONE
        DW      fs_enter
        DB      NONE
        DW      fs_state
        DB      NONE
        DW      fs_pass
        DB      BK_SHOP
        DW      E_RENTER
        DB      BK_SHOP
        DW      E_RSTATE
        DB      BK_SHOP
        DW      E_RPASS
        DB      BK_SHOP
        DW      E_RENTER
        DB      BK_SHOP
        DW      E_RSTATE
        DB      BK_SHOP
        DW      E_RPASS
no_op:  FRET

; ---- leaving and failing --------------------------------------------------------
; the server said no (the room forgotten)
        RFIT    177
terr:   GETV    NERR
        XRI     ERR_VERSION
        BNZ     te1
        LDI     M_VERSION+80H
        BR      tfail
te1:    GETV    NERR
        XRI     ERR_CAPS
        BNZ     te2
        LDI     M_CAPS+80H
        BR      tfail
te2:    GETV    NERR
        XRI     ERR_FULL
        BNZ     te3
        LDI     M_FULL+80H
        BR      tfail
te3:    LDI     M_NOROOM+80H
        BR      tfail
; the link went: say so, then join again (the room kept)
tlost:  LDI     M_LOST
        BR      tfail
; 0: give the seat up, BYE (a few passes to get it out), close
tleave: GETV    NLINK
        ANI     NL_JOINED
        BZ      tlv1
        SETV    ANARG, 0
        LDI     ACT_LEAVE
        FCALL   act_q
tlv1:   GETV    TXF
        ORI     TX_BYE
        STR     RA
        SETV    MRC, 60
tlv2:   FCALL   frame
        FCALL   nstep
        GETV    TXF
        BNZ     tlv3
        GETV    NKIND
        BZ      tlv4
tlv3:   VAR     MRC
        LDN     RA
        SMI     1
        STR     RA
        BNZ     tlv2
tlv4:   LDI     M_QUIET+80H
        BR      tfail
; no answer: give the room up
tnoans: LDI     M_NOANS+80H
; tfail: the socket closed, message D (b7: the room forgotten too), then
; the rooms again.
tfail:  PUTV    MSGID
        GETV    FLAGS                   ; (a test scene is over too)
        ANI     0FFH-FL_TEST
        STR     RA
        SETV    SHOWS, S_NONE
        OPN     TOP_SPRHIDE
        BCALL   BK_BOOT, E_SCLOSE
        GETV    MSGID
        ANI     7FH
        XRI     M_QUIET
        BZ      tf0
        BCALL   BK_BOOT, E_MSGW
tf0:    GETV    MSGID
        ANI     80H
        BZ      tf1
        BCALL   BK_BOOT, E_CLRRM
tf1:    FJMP    top

; ---------------------------------------------------------------------------
; frame -- wait for the ISR's frame count (RE) to move; DT = the jiffies
;   since the last pass (at most 8: a longer stall is not caught up),
;   JIFFY += DT.
; ---------------------------------------------------------------------------
        RFIT    47
frame:  VAR     LASTRE
frm1:   GLO     RE
        SEX     RA
        SM                              ; RE.0 - LASTRE
        SEX     R2
        BZ      frm1
        SMI     9
        BNF     frm2
        LDI     0FFH                    ; more than 8: 8
frm2:   ADI     9
        PUTV    DT
        GLO     RE
        PUTV    LASTRE
        GETV    DT
        STR     R2
        GETV    JIFFY
        ADD
        STR     RA
        INC     RA
        LDN     RA
        ADCI    0
        STR     RA
        FRET
