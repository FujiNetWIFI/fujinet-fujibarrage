; p_scr.inc -- the screens a room shows (src/room.c, src/play.c): each has
; an enter (drawn afresh), a state (a commit came) and a pass. M1: the
; skeleton -- the waiting room lists the seats and the host starts with 5;
; the shop is done with 9; the field shows the model and fires with 5 on
; our turn; a round's end and the game's say so.

; seat_rec: RB = the seat D's record in ST (OFF_SEATS + SEAT_SIZE s).
;   Keeps R8, R9, RC.
        RFIT    14
seat_rec:
        SHL
        ADI     LO(SEATREC)
        PLO     RB
        LDI     HI(SEATREC)
        PHI     RB
        LDA     RB
        PHI     RD                      ; (RD.1: SCRT's own, free till FRET)
        LDN     RB
        PLO     RB
        GHI     RD
        PHI     RB
        FRET
        PFIT    20
SEATREC: DW     ST+OFF_SEATS, ST+OFF_SEATS+25, ST+OFF_SEATS+50, ST+OFF_SEATS+75
        DW      ST+OFF_SEATS+100, ST+OFF_SEATS+125, ST+OFF_SEATS+150, ST+OFF_SEATS+175
        DW      ST+OFF_SEATS+200, ST+OFF_SEATS+225
        IF      SEAT_SIZE <> 25
        ERROR   "SEATREC assumes 25-byte seats"
        ENDIF

; ---- the field -------------------------------------------------------------------
; enter: the model drawn whole, the tanks shown again (CLS hid the sprites;
; their images survive it), the HUD built afresh. A commit: the tanks and
; the turn from the state. A pass: a few dirty columns, the aim, the tanks,
; the HUD if it changed, its bars.
        RFIT    77
fs_enter:
        OPN     TOP_CLS
        SETV    MENU, M_OFF             ; the field's own lines shut
        SETV    HMSGT, 0
        SETV    BANNERT, 0
        SETV    DEFOPN, 0
        SETV    GUIDE, I_NONE
        SETV    TARGET, NO_SEAT
        SETV    FIRED, 0
        FCALL   sky_set
        FCALL   fld_all
        FCALL   hud_clear
        FCALL   tk_reset
fs_state:
        FCALL   tk_state
        FCALL   turn_check
        SETV    HUDDRT, 3
        FRET

        RFIT    28
fs_pass:
        LDI     2
        FCALL   fld_flush
        FCALL   fx_pass                 ; the items line, the window, the lines
        BNZ     fps0                    ;  (the keys theirs: no aim)
        FCALL   aim_pass
fps0:   FCALL   tk_pass
        GETV    HUDDRT
        BZ      fps1
        FJMP    hud_rows
fps1:   FJMP    hud_bars

; sky_set: the round's sky (Checkpoint 1): a night sky (stars, storm, black)
;   is the picture as drawn, black above white dirt; a day sky (plain,
;   gradient, sunset, cavern) the picture inverted. VIEWF the VIEW's flags.
        RFIT    74
sky_set:
        GETS    OFF_SKY
        ANI     7
        ADI     LO(SKYDAY)
        PLO     R8
        LDI     HI(SKYDAY)
        PHI     R8
        LDN     R8
        PUTV    VIEWF
; view_set: VIEW 0, 0, VIEWF.
view_set:
        SETV    VIEWD, 0
        GETV    VIEWF
        PUTV    VIEWD+1
        PARS
        P       0
        P       0
        GETV    VIEWF
        PD
        GOP     TOP_VIEW
        FRET
SKYDAY: DB      TV_INVERT, TV_INVERT, 0, 0, TV_INVERT, TV_INVERT, 0, 0
