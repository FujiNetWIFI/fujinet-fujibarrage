; ui.inc -- text on the cart's graphics layer (PTEXT: 4 dots a character;
; 12 lines a row on the text screens, 5 with TM_SHORT on the HUD's), numbers,
; and the messages. FujiBlast's src/studio2 ui.inc.
;
; A text screen is 16 columns by 10 rows: row r at line 12 r. PTEXT in SET
; mode clears each character's box first, so text overwrites text.

TXROW   FUNCTION r,(r)*12

; txt_at: the NUL-ended string at R8 (any page: ROM, the cart's RAM) at x
; R9.0, y R9.1, in mode TXM; at most RC.0 characters (txt_at16: 16). R8 is
; left past the last character drawn.
        RFIT    38
txt_at16:
        LDI     16
        PLO     RC
txt_at: PARS
        GLO     R9
        PD
        GHI     R9
        PD
        GETV    TXM
        PD
ta1:    LDA     R8
        BZ      ta9
        PD
        DEC     RC
        GLO     RC
        BNZ     ta1
ta9:    GOP     TOP_PTEXT
        FRET

; txt_n: exactly RC.0 characters from R8 (past a NUL: spaces) at x R9.0,
; y R9.1, in mode TXM: a field padded to its width.
        RFIT    38
txt_n:  PARS
        GLO     R9
        PD
        GHI     R9
        PD
        GETV    TXM
        PD
tn1:    LDA     R8
        BNZ     tn2
        DEC     R8                      ; (stay on the NUL)
        LDI     ' '
tn2:    PD
        DEC     RC
        GLO     RC
        BNZ     tn1
        GOP     TOP_PTEXT
        FRET

; txt_ctr: the string at R8 centred on line R9.1 (mode TXM).
        RFIT    24
txt_ctr:
        GLO     R8
        PLO     RC
        GHI     R8
        PHI     RC
        LDI     0
        PLO     R9
tc1:    LDA     RC
        BZ      tc2
        INC     R9
        GLO     R9
        XRI     16
        BNZ     tc1
tc2:    GLO     R9                      ; x = 32 - 2 n
        SHL
        SDI     32
        PLO     R9
        FJMP    txt_at16

; num_s: R8 (0-65535) as decimal text in NUMB, NUL-ended, at least D
;   digits (zeros in front; 1: none). R8 left on NUMB. Clobbers R9, RC,
;   T5-T7.
        RFIT    113
num_s:  PUTV    T7                      ; at least this many digits
        LDR     RC, P10
        LDR     R9, NUMB
        SETV    T6, 5                   ; digits left (this one too)
nms1:    SETV    T5, 0                   ; this digit
nms2:    SEX     RC                      ; R8 >= 10^k: take it off
        GLO     R8
        SM
        PHI     RD
        INC     RC
        GHI     R8
        SMB
        DEC     RC
        SEX     R2
        BNF     nms3
        PHI     R8
        GHI     RD
        PLO     R8
        INCV    T5
        BR      nms2
nms3:    GETV    T5                      ; a digit: unless a zero in front
        BNZ     nms4
        GLO     R9
        XRI     LO(NUMB)
        BNZ     nms4
        GETV    T6                      ; (in front: only within the minimum)
        STR     R2
        GETV    T7
        SM
        BNF     nms5
nms4:    GETV    T5
        ADI     '0'
        STR     R9
        INC     R9
nms5:    INC     RC
        INC     RC
        GETV    T6
        SMI     1
        STR     RA
        BNZ     nms1
        LDI     0
        STR     R9
        LDR     R8, NUMB
        FRET
P10:    DB      LO(10000), HI(10000), LO(1000), HI(1000), 100, 0, 10, 0, 1, 0

; txt_num: the number R8 (at least D digits) at x R9.0, y R9.1 (mode TXM).
        RFIT    32
txt_num:
        PHI     RD
        GLO     R9
        PUTV    T3
        GHI     R9
        PUTV    T4
        GHI     RD
        FCALL   num_s
        GETV    T3
        PLO     R9
        GETV    T4
        PHI     R9
        FJMP    txt_at16

; txt_clr: lines R9.1 for D lines cleared across the screen (a RECT).
        RFIT    33
txt_clr:
        PHI     RD
        PARS
        P       0
        GHI     R9
        PD
        P       64
        GHI     RD
        PD
        P       TM_CLEAR
        GOP     TOP_RECT
        FRET


; ---- a text screen's row, built in HLINE (hud.inc's hl_clear, hl_num) ------
; ln_str: the string at R8 into HLINE from column D (to column 15).
        RFIT    19
ln_str: ADI     LO(HLINE)
        PLO     R9
        LDI     HI(HLINE)
        PHI     R9
lns1:   GLO     R9
        XRI     LO(HLINE+16)
        BZ      lns9
        LDA     R8
        BZ      lns9
        STR     R9
        INC     R9
        BR      lns1
lns9:   FRET

; ln_lp: the length-prefixed name at R8 (the catalog's) into HLINE from
;   column D, RC.0 characters at most.
        RFIT    32
ln_lp:  ADI     LO(HLINE)
        PLO     R9
        LDI     HI(HLINE)
        PHI     R9
        LDA     R8                      ; its length, cut to RC.0
        STR     R2
        GLO     RC
        SM
        BNF     lnl1                    ; (max < length: max)
        LDN     R2
        BR      lnl2
lnl1:   GLO     RC
lnl2:   PLO     RC
lnl3:   GLO     RC
        BZ      lnl9
        GLO     R9
        XRI     LO(HLINE+16)
        BZ      lnl9
        DEC     RC
        LDA     R8
        STR     R9
        INC     R9
        BR      lnl3
lnl9:   FRET

; ln_row: HLINE drawn as text row D (the 12-line font, line 12 D).
        RFIT    26
ln_row: PLO     R9                      ; y = 12 row
        SHL
        STR     R2
        GLO     R9
        ADD
        SHL
        SHL
        PHI     R9
        LDI     0
        PLO     R9
        SETV    TXM, TM_SET
        LDR     R8, HLINE
        FJMP    txt_at16

; ln_inv: text row D inverted (the cursor's: drawn again, it is put back).
        RFIT    41
ln_inv: PLO     R9
        SHL
        STR     R2
        GLO     R9
        ADD
        SHL
        SHL
        PHI     R9
        PARS
        P       0
        GHI     R9
        PD
        P       64
        P       12
        P       TM_XOR
        GOP     TOP_RECT
        FRET

; ln_seat: seat D's mark at HLINE's column T3, its name after it (RC.0
;   characters at most).
        RFIT    43
ln_seat:
        PUTV    T7                      ; (the seat)
        GETV    T3                      ; the mark
        ADI     LO(HLINE)
        PLO     R9
        LDI     HI(HLINE)
        PHI     R9
        GETV    T7
        ADI     GL_MARK
        STR     R9
        GETV    T7                      ; the name
        FCALL   seat_rec
        GLO     RB
        PLO     R8
        GHI     RB
        PHI     R8
        GETV    T3
        ADI     1
        FJMP    ln_strn

; ln_strn: as ln_str, RC.0 characters at most.
        RFIT    23
ln_strn:
        ADI     LO(HLINE)
        PLO     R9
        LDI     HI(HLINE)
        PHI     R9
lnn1:   GLO     RC
        BZ      lnn9
        GLO     R9
        XRI     LO(HLINE+16)
        BZ      lnn9
        LDA     R8
        BZ      lnn9
        STR     R9
        INC     R9
        DEC     RC
        BR      lnn1
lnn9:   FRET

; ln_val6: VALBUF right-aligned at HLINE's end, its first 6 at most.
        RFIT    30
ln_val6:
        LDR     R8, VALBUF
        LDI     0
        PLO     RC
lv61:   LDA     R8
        BZ      lv62
        INC     RC
        GLO     RC
        XRI     6
        BNZ     lv61
lv62:   LDR     R8, VALBUF              ; from column 16 - n
        GLO     RC
        SDI     16
        FJMP    ln_strn
