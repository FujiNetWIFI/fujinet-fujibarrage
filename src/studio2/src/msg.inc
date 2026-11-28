; msg.inc -- the HUD's messages (the shot player's OP_TALK, OP_DAMAGE,
; OP_SHIELD; the Arcadia's p_msg.inc): while HMSGT runs they hold the rows
; (fieldui.inc row1_held/row2_held); its end asks for the HUD again.
;   talk    both rows: the speaker's mark, then its words wrapped at the
;           spaces (15, then 16); what doesn't fit shows after half its
;           time. The words are copied to TALKB (SCRIPT's go with the
;           segment).
;   hit     row 1: mark, name (5), E and the energy left, HIT
;   shield  row 1: mark, name (5), what (6), the hp left
; Drawn once a pass after the ops that set it (ply_pass, HUDDRT's MSGDRT:
; the player draws no HUD) and when hud_rows asks.

; op_talk etc. land here with T0 the seat (PPC already past the op).
; msg_talk: T1 the length, T2-T3 the words (SCRIPT): a tank on the field
;   speaks.
        RFIT    133
msg_talk:
        GETV    T0
        SMI     SEATS
        BDF     mtk0
        GETV    T0
        ADI     LO(TKON)
        PLO     RA
        LDN     RA
        BZ      mtk0
        GETV    T0
        PUTV    MSGS
        SETV    MSGK, MK_TALK
        SETV    HMSGT, TALK_J
        SETV    MSGA, 0
        GETV    T1                      ; MAX_TALK at most
        SMI     MAX_TALK+1
        BNF     mtk1
        LDI     MAX_TALK
        BR      mtk2
mtk1:   GETV    T1
mtk2:   PUTV    MSGB
        PLO     RC
        GETV    T2
        PLO     R8
        GETV    T3
        PHI     R8
        POKEAT  TALKB
mtk3:   GLO     RC
        BZ      mtk4
        LDA     R8
        PKD
        DEC     RC
        BR      mtk3
mtk4:   PKEND
        FJMP    msg_mark
mtk0:   FRET

; msg_hit: T1 the energy left. msg_shield: T1 the event, T2 the hp.
        RFIT    63
msg_hit:
        FCALL   msg_event
        BZ      mev9
        SETV    MSGK, MK_HIT
        GETV    T1
        PUTV    MSGA
        FJMP    msg_mark
msg_shield:
        FCALL   msg_event
        BZ      mev9
        SETV    MSGK, MK_SHIELD
        GETV    T1
        PUTV    MSGB
        GETV    T2
        PUTV    MSGC
msg_mark:                               ; drawn once a pass (ply_pass: MSGDRT)
        GETV    HUDDRT
        ORI     MSGDRT
        STR     RA
mev9:   FRET

; msg_event: D = 0 if T0 isn't a seat; else 1, the message the seat's for
;   EVENT_J (a talk's second row given back to the HUD).
        RFIT    65
msg_event:
        GETV    T0
        SMI     SEATS
        BDF     mev0
        GETV    HMSGT                    ; a talk up: its row 2 cleared
        BZ      mev1
        GETV    MSGK
        XRI     MK_TALK
        BNZ     mev1
        SETV    HMSGT, 0
        FCALL   hl_clear
        LDI     1
        FCALL   hl_draw
        GETV    HUDDRT
        ORI     2
        STR     RA
mev1:   GETV    T0
        PUTV    MSGS
        SETV    HMSGT, EVENT_J
        LDI     1
        FRET
mev0:   LDI     0
        FRET

; msg_draw: the message's rows.
        RFIT    214
msg_draw:
        FCALL   hl_clear
        GETV    MSGK
        XRI     MK_TALK
        BNZ     mdr2
        GETV    MSGS                    ; talk: the mark, the words
        ADI     GL_MARK
        PUT9    HLINE
        GETV    MSGA
        PUTV    MSGC
        SETV    T6, 1
        LDI     15
        FCALL   mt_line
        LDI     0
        FCALL   hl_draw
        FCALL   hl_clear
        SETV    T6, 0
        LDI     16
        FCALL   mt_line
        LDI     1
        FJMP    hl_draw
mdr2:   SETV    T3, 0                   ; an event: the mark, the name
        LDI     5
        PLO     RC
        GETV    MSGS
        FCALL   ln_seat
        GETV    MSGK
        XRI     MK_HIT
        BNZ     mdr3
        LDI     'E'                     ; E nnn HIT
        PUT9    HLINE+7
        GETV    MSGA
        PLO     R8
        LDI     0
        PHI     R8
        LDI     LO(HLINE+8)
        PLO     R9
        LDI     3
        FCALL   hl_num
        LDR     R8, s_hit
        LDI     12
        FCALL   ln_str
        BR      mdr4
mdr3:   GETV    MSGB                    ; what (6), the hp
        SMI     SH_LIFT+1
        BNF     mdr3a
        LDI     SH_HIT
        BR      mdr3b
mdr3a:  GETV    MSGB
mdr3b:  PLO     R8                      ; 6 x it
        SHL
        STR     R2
        GLO     R8
        ADD
        SHL
        ADI     LO(SHWHAT)
        PLO     R8
        LDI     HI(SHWHAT)
        ADCI    0
        PHI     R8
        LDI     6
        PLO     RC
        LDI     7
        FCALL   ln_strn
        GETV    MSGC
        PLO     R8
        LDI     0
        PHI     R8
        LDI     LO(HLINE+13)
        PLO     R9
        LDI     3
        FCALL   hl_num
mdr4:   LDI     0
        FJMP    hl_draw
s_hit:  DB      "HIT", 0
SHWHAT: DB      "SHIELDBROKENDEFLCTSHIELDLIFTED"

; mt_line: the talk's next line into HLINE at column T6, D wide at most,
;   from TALKB + MSGC: broken at its last space that fits (none: cut);
;   MSGC past it (and the space). T4 the width, T5 what's left, T7 shown.
        RFIT    147
mt_line:
        PUTV    T4
        GETV    MSGC                    ; left = MSGB - MSGC
        STR     R2
        GETV    MSGB
        SM
        BZ      mtl9
        PUTV    T5
        GETV    MSGC                    ; R8 on its first letter
        ADI     LO(TALKB)
        PLO     R8
        LDI     HI(TALKB)
        ADCI    0
        PHI     R8
        GETV    T4                      ; all fit: all
        STR     R2
        GETV    T5
        SM
        BZ      mtl1
        BDF     mtl2
mtl1:   GETV    T5
        PUTV    T7
        BR      mtl5
mtl2:   GETV    T4                      ; else the last space at <= width
        PLO     RC
        STR     R2
        GLO     R8
        ADD
        PLO     R9
        GHI     R8
        ADCI    0
        PHI     R9
mtl3:   LDN     R9
        XRI     ' '
        BZ      mtl4
        DEC     R9
        DEC     RC
        GLO     RC
        BNZ     mtl3
        GETV    T4                      ; (none: cut at the width)
        PUTV    T7
        BR      mtl5
mtl4:   GLO     RC                      ; (the space: passed over too)
        PUTV    T7
        INCV    MSGC
mtl5:   GETV    T7                      ; MSGC on
        STR     R2
        GETV    MSGC
        ADD
        STR     RA
        GETV    T6                      ; the letters
        ADI     LO(HLINE)
        PLO     R9
        LDI     HI(HLINE)
        PHI     R9
        GETV    T7
        PLO     RC
mtl6:   GLO     RC
        BZ      mtl9
        LDA     R8
        STR     R9
        INC     R9
        DEC     RC
        BR      mtl6
mtl9:   FRET

; msg_tick: the message's time; a talk's rest at half of it; its end asks
;   for the HUD.
        RFIT    78
msg_tick:
        GETV    HMSGT
        BZ      mtc9
        GETV    DT
        STR     R2
        GETV    HMSGT
        SM
        BDF     mtc1
        LDI     0
mtc1:   STR     RA
        BNZ     mtc2
        SETV    HUDDRT, 3
        FRET
mtc2:   GETV    MSGK                    ; a talk on its first page, more to
        XRI     MK_TALK                 ;  come, at half its time
        BNZ     mtc9
        GETV    MSGA
        BNZ     mtc9
        GETV    MSGC
        STR     R2
        GETV    MSGB
        SM
        BZ      mtc9
        GETV    HMSGT
        SMI     TALK_J/2+1
        BDF     mtc9
        GETV    MSGC
        PUTV    MSGA
        FJMP    msg_draw
mtc9:   FRET
