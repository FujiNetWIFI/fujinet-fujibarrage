; p_msg.inc -- the HUD's messages (src/play.c's talk; p_hud.inc's rows):
; the shot player's (FARPG from p_ply.inc: OP_TALK, OP_DAMAGE, OP_SHIELD,
; a step, the segment's end). A talk: both rows cleared, the tank's name
; (DIRT on SKY), then its text typed a letter a step, scrolling up when the
; rows fill; flushed at the segment's end (SCRIPT's bytes go); TALKJ
; frames. An event line over row 24: OP_DAMAGE "name E nnn HIT" (the
; energy left), OP_SHIELD "name WHAT nnn" (its hp). While TALKT runs the
; HUD isn't drawn (p_hud.inc HUDQ); its end asks for it (FD_HUD). TALKT
; counts eighths of a second (8 frames: a far call and a cart RAM write a
; frame for 2 seconds cost MEGA MIRV 7 frames of lag).
; No far calls from here: fixed code and its own only.

        BCTA,UN MTALK           ; E_OTALK
        BCTA,UN MDMG            ; E_ODMG
        BCTA,UN MSHLD           ; E_OSHLD
        BCTA,UN MSTEP           ; E_MSTEP
        BCTA,UN MFLUSH          ; E_MFLUSH

TALKJ   EQU     120/8           ; a talk stays (play.c talk_left: 120 frames)
EVTJ    EQU     96/8            ; an event line stays (TALKT: eighths of a second)
HUDR24  EQU     SCR2+(HUDROW-13)*16     ; the HUD's rows in screen RAM
HUDR25  EQU     HUDR24+16

; RAM-H, through pointer words (page 0 code)
PTALKT: DWBE    TALKT
PTALKP: DWBE    TALKP
PTALKN: DWBE    TALKN
PTALKC: DWBE    TALKC

; DIRTYH: the HUD to be drawn again. Leaf. Clobbers R0.
DIRTYH: LODA,R0 FDIRTY
        IORI,R0 FD_HUD
        STRA,R0 FDIRTY
        RETC,UN

; NAME6: seat R3's name in 6 cells (DCOLOR). Calls two down. Clobbers R0,
; R1, R3.
NAME6:  LODA,R0 SEAT25,R3
        STRZ    R3
        LODI,R1 6
NM61:   LODA,R0 ST+OFF_SEATS+P_NAME,R3
        ZBSR    *ZA2G
        ZBSR    *ZDPUTC
        ADDI,R3 1
        BDRR,R1 NM61
        RETC,UN

; ---- the messages (the shot player's: OP_TALK, OP_DAMAGE, OP_SHIELD) ----------------------
; MTALK: OP_TALK at PPC (seat, len, text): a tank on the field speaks: both
; rows cleared, its name (DIRT on SKY), then its text a letter a step
; (MSTEP), TALKJ frames. Calls two down. Clobbers R0-R3, T0.
MTALK:  LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3
        COMI,R0 SEATS
        RETC,GT
        RETC,EQ
        STRA,R0 T0
        STRZ    R3
        LODA,R0 TKX,R3
        COMI,R0 $FF
        RETC,EQ
        BSTA,UN MCLR
        DAT     HUDROW,1
        DCOL    HUDTI
        LODA,R3 T0
        LODA,R0 SEAT25,R3
        STRZ    R3
        LODI,R1 NAME_LEN
MT1:    LODA,R0 ST+OFF_SEATS+P_NAME,R3
        BCTR,EQ MT2
        ZBSR    *ZA2G
        ZBSR    *ZDPUTC
        ADDI,R3 1
        BDRR,R1 MT1
MT2:    LODZ    R2              ; the text from the cell after a space
        ADDI,R0 1
        CPUT    TALKC
        LODA,R3 PPC
        ADDI,R3 1
        LODA,R0 *PSCRIPT,R3
        CPUT    TALKN
        LODA,R0 PPC
        ADDI,R0 2
        CPUT    TALKP
        LODI,R0 TALKJ
        CPUT    TALKT
        RETC,UN

; MSTEP: a pass of the message (a frame drawn): the talk's next letter; or
; (the PLY page calls it every 8 frames then) its time down (over: the HUD
; again). Calls three down. Clobbers R0-R3, T0, T1.
MSTEP:  LODA,R0 *PTALKN
        BCTR,EQ MGST1
        ANDI,R0 1               ; (every other letter a blip, if nothing
        BCTA,EQ MLETTER         ;  else plays: tail calls)
        SND     UI_TALK
        BCTA,UN MLETTER
MGST1:  LODA,R0 *PTALKT
        RETC,EQ
        SUBI,R0 1
        CPUT    TALKT
        LODA,R0 *PTALKT
        RETC,GT
        BCTA,UN DIRTYH
; MFLUSH: the talk's letters left, all now (the segment's end).
MFLUSH: LODA,R0 *PTALKN
        RETC,EQ
        BSTR,UN MLETTER
        BCTR,UN MFLUSH
; MLETTER: the talk's next letter into its cell (TGLY: ASCII 32-95; $FF
; skipped); the rows full: row 25 up to row 24, typing on row 25. Calls
; two down. Clobbers R0-R3, T0, T1.
MLETTER: LODA,R0 *PTALKP
        STRZ    R3
        LODA,R0 *PSCRIPT,R3
        STRA,R0 T0
        LODZ    R3
        ADDI,R0 1
        CPUT    TALKP
        LODA,R0 *PTALKN
        SUBI,R0 1
        CPUT    TALKN
        LODA,R0 T0              ; its glyph
        SUBI,R0 32
        COMI,R0 64
        RETC,GT
        RETC,EQ
        STRZ    R3
        LODA,R0 TGLY,R3
        COMI,R0 $FF
        RETC,EQ
        STRA,R0 T0
        LODA,R0 *PTALKC         ; its cell (T1)
        COMI,R0 30
        BCFR,EQ MLE1
        BSTA,UN MSCROLL
        LODI,R0 15
MLE1:    STRA,R0 T1
        ADDI,R0 1
        CPUT    TALKC
        LODA,R0 T1
        COMI,R0 15
        BCTR,LT MLE2
        SUBI,R0 15
        LODI,R1 HUDROW+1
        BCTR,UN MLE3
MLE2:    LODI,R1 HUDROW
MLE3:    STRZ    R2
        ZBSR    *ZDPOS
        LODA,R0 T0
        IORI,R0 HUDI
        ZBRR    *ZDGLY
; MSCROLL: row 25's cells 1-15 up into row 24's, row 25's blank. MCLR:
; both rows' cells 1-15 blank. Leaves. Clobber R0, R3.
MSCROLL: LODI,R3 15
MSC1:   LODA,R0 HUDR25,R3
        STRA,R0 HUDR24,R3
        LODI,R0 HUDI
        STRA,R0 HUDR25,R3
        BDRR,R3 MSC1
        RETC,UN
MCLR:   LODI,R3 15
MCL1:   LODI,R0 HUDI
        STRA,R0 HUDR24,R3
        STRA,R0 HUDR25,R3
        BDRR,R3 MCL1
        RETC,UN

; MDMG: OP_DAMAGE at PPC (seat, amount, energy left): row 24 "name E nnn
; HIT" (the talk's letters left first: its bytes are this segment's).
MDMG:   BSTA,UN MEVT
        RETC,GT                 ; (no seat)
        EORZ    R0
        ZBSR    *ZDPUTC
        LODI,R0 'E'
        ZBSR    *ZDPUTC
        LODA,R3 PPC
        ADDI,R3 2
        LODA,R0 *PSCRIPT,R3
        LODI,R1 3
        ZBSR    *ZDNUM
        TXTAT   SHIT
        RETC,UN
; MSHLD: OP_SHIELD at PPC (seat, event, hp): row 24 "name WHAT nnn" (SH_*:
; SHIELD, BROKEN, DEFLCT, SHIELD, LIFTED).
MSHLD:  BSTA,UN MEVT
        RETC,GT
        LODA,R3 PPC
        ADDI,R3 1
        LODA,R0 *PSCRIPT,R3
        COMI,R0 SH_LIFT+1
        BCTR,LT MSH1
        LODI,R0 SH_HIT
MSH1:   STRZ    R3              ; 6 x it
        ADDZ    R0
        ADDZ    R3
        ADDZ    R0
        STRZ    R3
        LODI,R1 6
MSH2:   LODA,R0 SHWHAT,R3
        ZBSR    *ZDPUTC
        ADDI,R3 1
        BDRR,R1 MSH2
        LODA,R3 PPC
        ADDI,R3 2
        LODA,R0 *PSCRIPT,R3
        LODI,R1 3
        ZBRR    *ZDNUM
; MEVT: an event line for the seat at PPC: CC GT (and nothing) if it isn't
; one; else the talk flushed, the line's time (EVTJ at least), row 24
; from cell 1 with the seat's name (6, DIRT on SKY), DCOLOR the HUD's.
; Calls three down. Clobbers R0-R3, T0-T3.
MEVT:   LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3
        COMI,R0 SEATS
        BCTR,LT MEV0
        LODI,R0 1               ; (GT)
        RETC,UN
MEV0:   STRA,R0 T2
        BSTA,UN MFLUSH
        LODA,R0 *PTALKT
        COMI,R0 EVTJ
        BCFR,LT MEV1
        LODI,R0 EVTJ
        CPUT    TALKT
MEV1:   DAT     HUDROW,1
        DCOL    HUDTI
        LODA,R3 T2
        BSTA,UN NAME6
        DCOL    HUDI
        EORZ    R0              ; (EQ: go on)
        RETC,UN

SHIT:   DB      " HIT",$FF
SHWHAT: DB      "SHIELDBROKENDEFLCTSHIELDLIFTED"
; the talk's letters: ASCII 32-95 as the field shows them ($FF: none; ':'
; and the apostrophe have no glyph; '%' and '/' the diagonal)
TGLY:   DB      $00,$3E,$FF,$FF,$37,$01,$FF,$FF,$FF,$FF,$FF,$36,$35,$00,$34,$01
        DB      $10,$11,$12,$13,$14,$15,$16,$17,$18,$19,$FF,$FF,$3C,$FF,$3D,$3F
        DB      $FF,$1A,$1B,$1C,$1D,$1E,$1F,$20,$21,$22,$23,$24,$25,$26,$27,$28
        DB      $29,$2A,$2B,$2C,$2D,$2E,$2F,$30,$31,$32,$33,$FF,$FF,$FF,$FF,$FF
