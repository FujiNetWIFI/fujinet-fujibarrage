; macros.inc -- FujiBarrage's macros on the Studio II, after fujinet.inc
; and s2macro.inc (FujiBlast's src/studio2 macros.inc, M.U.L.E.'s before it).
;
; Registers (barrage.asm has the whole list): RA.1 is $08 everywhere
; outside the runtime, so RA reaches the variables on page $08 in three
; instructions (VAR/GETV/PUTV/SETV). RB reads the cart's RAM (GETS sets it
; whole: the state is two pages). RC is the leaves' PC (LCALL).

; Keep `size` bytes on one page, and out of $x000-$x3FF. Every routine with
; a short branch starts with one; tools/s2/fitcheck.py holds the sizes to
; the routines (tools/s2/fitfix.py sets them). Never fall into an RFIT:
; the gap is zeros, and $00 is IDL.
RFIT    MACRO   size
        IF      ((($) & 0FFH) + (size)) > 100H
        ORG     (($) + 0FFH) & 0FF00H
        ENDIF
        IF      (($) & 0C00H) == 0
        ORG     (($) & 0F000H) + 0400H
        ENDIF
        ENDM

; Keep a data block of `size` bytes out of $x000-$x3FF (no page rule).
DFIT    MACRO   size
        IF      (($) & 0C00H) == 0
        ORG     (($) & 0F000H) + 0400H
        ENDIF
        IF      ($) + (size) > (($) & 0F000H) + 1000H
        ORG     (($) & 0F000H) + 1400H
        ENDIF
        ENDM

; A table that one LDI of its low byte must index: all on one page.
PFIT    MACRO   size
        IF      ((($) & 0FFH) + (size)) > 100H
        ORG     (($) + 0FFH) & 0FF00H
        ENDIF
        IF      (($) & 0C00H) == 0
        ORG     (($) & 0F000H) + 0400H
        ENDIF
        ENDM

; ---- console RAM --------------------------------------------------------
VAR     MACRO   v               ; RA -> v (page $08)
        IF      HI(v) <> HI(RAMVARS)
        ERROR   "VAR: not on page $08 (GET9/PUT9)"
        ENDIF
        LDI     LO(v)
        PLO     RA
        ENDM
; page $09's (cold paths): RA.1 there and back. D kept through RD.1.
GET9    MACRO   v               ; D = v
        LDI     HI(v)
        PHI     RA
        LDI     LO(v)
        PLO     RA
        LDN     RA
        PHI     RD
        LDI     HI(RAMVARS)
        PHI     RA
        GHI     RD
        ENDM
PUT9    MACRO   v               ; v = D
        PHI     RD
        LDI     HI(v)
        PHI     RA
        LDI     LO(v)
        PLO     RA
        GHI     RD
        STR     RA
        LDI     HI(RAMVARS)
        PHI     RA
        GHI     RD
        ENDM
GETV    MACRO   v               ; D = v
        VAR     v
        LDN     RA
        ENDM
PUTV    MACRO   v               ; v = D (RD.1 is free between calls)
        PHI     RD
        VAR     v
        GHI     RD
        STR     RA
        ENDM
SETV    MACRO   v, val          ; v = val
        VAR     v
        LDI     val
        STR     RA
        ENDM
VARX    MACRO   base            ; RA -> base[s], s = R9.0 (page $08)
        GLO     R9
        ADI     LO(base)
        PLO     RA
        ENDM
INCV    MACRO   v               ; v += 1; D = the new v
        VAR     v
        LDN     RA
        ADI     1
        STR     RA
        ENDM

; ---- the state, in the cart's RAM -----------------------------------------
; GETS off: D = ST[off] (RB left on it).
GETS    MACRO   off
        LDI     HI(ST+(off))
        PHI     RB
        LDI     LO(ST+(off))
        PLO     RB
        LDN     RB
        ENDM

; D = the byte at a (any cart page: the reply, the status). R8.
SLD     MACRO   a
        LDI     HI(a)
        PHI     R8
        LDI     LO(a)
        PLO     R8
        LDN     R8
        ENDM

; ---- poking the cart's RAM --------------------------------------------------
; POKEAT a: the poke pointer at a (a constant), the stream open; then PKD
; stores D there and moves on (RF.1 on REGDATA meanwhile: PKEND parks it).
; A REGSEL anywhere closes the stream. 27 instructions, then 2 a byte.
POKEAT  MACRO   a
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_PTRLO
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        LDI     LO(a)
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_PTRHI
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        LDI     HI(a)
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_POKE
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        ENDM
; POKEST: the poke pointer at ST's first page, its low byte D (ST + D:
; ST is page-aligned), the stream open: 22 instructions.
POKEST  MACRO
        PHI     RD
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_PTRLO
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        GHI     RD
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_PTRHI
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        LDI     HI(ST)
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_POKE
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        ENDM
; POKESTP page: the poke pointer at page + R9.0, the stream open. Clobbers
; D (keeps R9).
POKESTP MACRO   page
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_PTRLO
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        GLO     R9
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_PTRHI
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        LDI     HI(page)
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_POKE
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        ENDM
PKD     MACRO                   ; store D, the pointer on
        PLO     RF
        LDN     RF
        ENDM
PKEND   MACRO
        LDI     FN_PARK
        PHI     RF
        ENDM

; ---- hotspots ------------------------------------------------------------
; A text-engine parameter list: RF.1 on TX_PARAM, then P/PD three
; instructions a byte, then GOP runs the operation and parks RF.
PARS    MACRO
        LDI     TX_PARAM
        PHI     RF
        ENDM
P       MACRO   v
        LDI     v
        PLO     RF
        LDN     RF
        ENDM
PD      MACRO                   ; the parameter in D
        PLO     RF
        LDN     RF
        ENDM
GOP     MACRO   code
        LDI     TX_OP
        PHI     RF
        LDI     code
        PLO     RF
        LDN     RF
        LDI     FN_PARK
        PHI     RF
        ENDM
; A register write: REGSEL n, REGDATA v.
REG     MACRO   n, v
        LDI     n
        HOT     HI(FN_REGSEL)
        LDI     v
        HOT     HI(FN_REGDATA)
        ENDM

; HOLD1 / HOLD0: the cart holds the picture as it is while a pass draws, so
; it shows whole (CLS lets it go too).
HOLD1   MACRO
        PARS
        P       1
        GOP     TOP_HOLD
        ENDM
HOLD0   MACRO
        PARS
        P       0
        GOP     TOP_HOLD
        ENDM

; ---- calls ----------------------------------------------------------------
; LCALL leaf: a leaf on its own page, run with P = C; it returns with
; SEP R3. 6 instructions (SCRT's FCALL/FRET: 33). A leaf never FCALLs,
; FJMPs or LCALLs, never touches RD.0 (the beep), keeps RA.1, and
; returns its result in D/DF or R8/R9 (tools/s2/checkleaf.py).
LCALL   MACRO   adr
        LDI     HI(adr)
        PHI     RC
        LDI     LO(adr)
        PLO     RC
        SEP     RC
        ENDM

; The routine at R9 (icall), as if FCALLed.
ICALL   MACRO
        SEP     R4
        DW      icall
        ENDM
; Routine `adr` in bank `bk` (calls.inc bcall), the caller's bank after.
BCALL   MACRO   bk, adr
        SEP     R4
        DW      bcall
        DB      bk
        DW      adr
        ENDM

; ---- drawing ---------------------------------------------------------------
OPN     MACRO   code            ; a text-engine operation, no parameters
        LDI     code
        HOT     TX_OP
        ENDM

; ---- sound -----------------------------------------------------------------
SND     MACRO   id              ; a cue (sound.inc)
        LDI     id
        FCALL   snd_play
        ENDM

; ---- segments ---------------------------------------------------------------
SEGEND  MACRO   lim             ; a segment must stop short of lim
        IF      $ > lim
        ERROR   "segment overflows its window"
        ENDIF
        ENDM

; ---- FujiBarrage's own ------------------------------------------------------
; POKER r: the poke pointer at register r (any), the stream open. Clobbers
; D. 24 instructions.
POKER   MACRO   r
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_PTRLO
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        GLO     r
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_PTRHI
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        GHI     r
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_POKE
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        ENDM

; RECT with its parameters from registers: x R8.0, y R8.1, w R9.0, h R9.1,
; the mode (and pattern) a constant. 24 instructions; RF parked after.
RECTR   MACRO   mode
        PARS
        GLO     R8
        PD
        GHI     R8
        PD
        GLO     R9
        PD
        GHI     R9
        PD
        P       mode
        GOP     TOP_RECT
        ENDM

; GOPN: an operation, RF left on the operations' page (the next PARS moves
; it): 5 instructions. Only when another hotspot list follows at once.
GOPN    MACRO   code
        LDI     TX_OP
        PHI     RF
        LDI     code
        PLO     RF
        LDN     RF
        ENDM
