; fld.inc -- the field: its model, and the picture drawn from it.
;
; THE MODEL is the server's TERRAIN column entries (docs/PROTOCOL.md), kept
; as they come: TOPS[x] the entry's first byte (00-EF dirt from that row to
; the bottom, F0 none, F0+n n spans), the spans' (top, end) pairs in SPN
; (page HI(SPN) + x/8, offset 32 (x mod 8); end exclusive). Only G frames
; and OP_SETTLE write it (fld_puts), so it is exact: every change to the
; dirt reaches the client as a settle (server/internal/project
; roundtrip_test.go holds the server to it for this shape).
;
; THE PICTURE is the cart's background, a cache of the model: a column is
; drawn as a RECT cleared over it, then a RECT a span of dirt (fld_draw).
; A column the model changed is marked in DIRTY and drawn again by
; fld_flush, a few a pass. The effects of a shot (fire, craters, mud) draw
; over the picture by themselves; the settles that follow draw the truth.


        PFIT    8
BITS:   DB      1, 2, 4, 8, 10H, 20H, 40H, 80H

; ---------------------------------------------------------------------------
; fld_draw -- LEAF: column R9.0 drawn from the model: cleared, then its
;   dirt. fld_spans: its dirt only (the column already clear). Keeps R9.0;
;   clobbers R8, R9.1, RB, D.
; ---------------------------------------------------------------------------
        RFIT    175
fld_draw:
        PARS
        GLO     R9
        PD
        P       0
        P       1
        P       FH
        P       TM_CLEAR
        GOP     TOP_RECT
fld_spans:
        LDI     HI(TOPS)
        PHI     RB
        GLO     R9
        ADI     LO(TOPS)
        PLO     RB
        LDN     RB                      ; the entry's first byte
        PLO     R8
        SMI     E_NONE
        BDF     fsp_sp                  ; F0 or more: none, or spans
        GLO     R8                      ; dirt from row t down, if on the field
        SMI     FH
        BDF     fsp9
        GLO     R8
        PHI     R8                      ; top
        LDI     FH
        PLO     R8                      ; end
        LDI     1
        PHI     R9                      ; one span
        BR      fsp2
fsp_sp: BZ      fsp9                    ; F0: no dirt
        PHI     R9                      ; n spans
        GLO     R9                      ; RB: the slot
        SHR
        SHR
        SHR
        ADI     HI(SPN)
        PHI     RB
        GLO     R9
        ANI     7
        SHL
        SHL
        SHL
        SHL
        SHL
        PLO     RB
fsp1:   LDA     RB                      ; top
        PHI     R8
        LDA     RB                      ; end
        PLO     R8
        SMI     FH+1                    ; (never past the field)
        BNF     fsp2
        LDI     FH
        PLO     R8
fsp2:   GHI     R8                      ; end - top lines, if any
        STR     R2
        GLO     R8
        SM
        BZ      fsp3
        BNF     fsp3
        PHI     RD                      ; (RD.1: free in a leaf)
        PARS
        GLO     R9
        PD
        GHI     R8
        PD
        P       1
        GHI     RD
        PD
        GETV    DIRTM                   ; (the dirt's mode and pattern)
        PD
        GOP     TOP_RECT
        GETV    DCRUST                  ; a crust: the top line solid
        BZ      fsp3
        PARS
        GLO     R9
        PD
        GHI     R8
        PD
        P       1
        P       1
        P       TM_SET
        GOP     TOP_RECT
fsp3:   GHI     R9
        SMI     1
        PHI     R9
        BNZ     fsp1
fsp9:   SEP     R3

; ---------------------------------------------------------------------------
; fld_dirt -- LEAF: DF = 1 if (R9.0, R9.1) is dirt in the model. Keeps R9;
;   clobbers R8, RB, D.
; ---------------------------------------------------------------------------
        RFIT    60
fld_dirt:
        LDI     HI(TOPS)
        PHI     RB
        GLO     R9
        ADI     LO(TOPS)
        PLO     RB
        LDN     RB
        PLO     R8
        SMI     E_NONE
        BDF     fdt_sp
        GHI     R9                      ; y >= t: dirt
        STR     R2
        GLO     R8
        SD
        SEP     R3
fdt_sp: BZ      fdt0                    ; F0: none
        PHI     R8                      ; n spans
        GLO     R9
        SHR
        SHR
        SHR
        ADI     HI(SPN)
        PHI     RB
        GLO     R9
        ANI     7
        SHL
        SHL
        SHL
        SHL
        SHL
        PLO     RB
        GHI     R9
        STR     R2                      ; y
fdt1:   LDA     RB                      ; top <= y?
        SD
        BNF     fdt2
        LDN     RB                      ; and y < end?
        SD                              ; (y - end: a borrow if y < end)
        BNF     fdt1y
fdt2:   INC     RB
        GHI     R8
        SMI     1
        PHI     R8
        BNZ     fdt1
fdt0:   ADI     0                       ; DF = 0
        SEP     R3
fdt1y:  SMI     0                       ; DF = 1
        SEP     R3

; ---------------------------------------------------------------------------
; fld_puts -- R9.1 column entries from R8 into the model, columns R9.0 on;
;   each column marked dirty. R8 left past them. A run of one-byte entries
;   is one poke stream (TOPS); a spanned one pokes its slot, then TOPS
;   again. Clobbers R9, RB, RC, D.
; ---------------------------------------------------------------------------
        RFIT    207
fld_puts:
        GHI     R9
        BNZ     fpu0
        FRET
fpu0:   GLO     R9                      ; the stream at TOPS + x
        ADI     LO(TOPS)
        PLO     RB
        LDI     HI(TOPS)
        PHI     RB
        POKER   RB
fpu1:   LDA     R8                      ; the entry's first byte, poked
        PHI     RD
        PLO     RF
        LDN     RF
        GLO     R9                      ; column x dirty
        ANI     7
        ADI     LO(BITS)
        PLO     RC
        LDI     HI(BITS)
        PHI     RC
        LDN     RC
        STR     R2
        GLO     R9
        SHR
        SHR
        SHR
        ADI     LO(DIRTY)
        PLO     RA
        LDN     RA
        OR
        STR     RA
        GHI     RD                      ; spans follow?
        SMI     E_NONE+1
        BDF     fpu_sp
fpu2:   INC     R9                      ; the next column
        GHI     R9
        SMI     1
        PHI     R9
        BNZ     fpu1
        PKEND
        FRET
fpu_sp: ADI     1                       ; n spans: 2n bytes into the slot
        SHL
        PLO     RC
        GLO     R9
        SHR
        SHR
        SHR
        ADI     HI(SPN)
        PHI     RB
        GLO     R9
        ANI     7
        SHL
        SHL
        SHL
        SHL
        SHL
        PLO     RB
        POKER   RB
fpu3:   LDA     R8
        PLO     RF
        LDN     RF
        DEC     RC
        GLO     RC
        BNZ     fpu3
        GHI     R9                      ; the last column: done
        SMI     1
        BNZ     fpu4
        PKEND
        FRET
fpu4:   PHI     R9                      ; TOPS again, at x + 1
        INC     R9
        GLO     R9
        ADI     LO(TOPS)
        PLO     RB
        LDI     HI(TOPS)
        PHI     RB
        POKER   RB
        BR      fpu1

; ---------------------------------------------------------------------------
; fld_flush -- the dirty columns drawn again, up to D bytes of DIRTY (eight
;   columns each): a run of them cleared by one RECT, then its dirt
;   (fld_run). D = 0 on return if none is left dirty. Clobbers R8, R9, RB,
;   RC, T0-T7.
; ---------------------------------------------------------------------------
        RFIT    173
fld_flush:
        PUTV    T0                      ; the budget
        LDI     0
        PUTV    T3                      ; DIRTY's byte
ffl1:   GETV    T3
        ADI     LO(DIRTY)
        PLO     RA
        LDN     RA
        BZ      ffl8
        PHI     RD                      ; its bits, all taken now
        LDI     0
        STR     RA
        GHI     RD
        PUTV    T1
        GETV    T3                      ; x = 8 i
        SHL
        SHL
        SHL
        PLO     R9
ffl2:   GETV    T1                      ; the next set bit
        BZ      ffl7
        SHR
        STR     RA
        BDF     ffl3
        INC     R9
        BR      ffl2
ffl3:   GLO     R9                      ; a run from x ...
        PUTV    T2
ffl4:   INC     R9                      ; ... while the next bit is set
        GETV    T1
        SHR
        BNF     ffl5
        STR     RA
        BR      ffl4
ffl5:   GLO     R9                      ; [T2, x): cleared, then its dirt
        PUTV    T4
        PARS
        GETV    T2
        PD
        P       0
        GETV    T2
        STR     R2
        GLO     R9
        SM
        PD
        P       FH
        P       TM_CLEAR
        GOP     TOP_RECT
        GETV    T2
        PLO     R9
        FCALL   fld_run                 ; (R9.0 = T4 after)
        BR      ffl2
ffl7:   GETV    T0                      ; a byte done: the budget
        SMI     1
        STR     RA
        BZ      ffl9
ffl8:   INCV    T3
        XRI     8
        BNZ     ffl1
        FRET                            ; (D = 0: all clean)
ffl9:   LDR     R8, DIRTY               ; spent: D = any left
        LDI     8
        PLO     R9
        LDI     0
        STR     R2
ffl10:  LDA     R8
        OR
        STR     R2
        DEC     R9
        GLO     R9
        BNZ     ffl10
        LDN     R2
        FRET

; ---------------------------------------------------------------------------
; fld_run -- the dirt of columns R9.0 up to T4 (exclusive), over clear
;   columns: a run of plain columns with one top is one RECT; a spanned
;   column is fld_spans's. R9.0 = T4 after. In the loop: R9.0 x, R9.1 the
;   end, RB TOPS + x, R8.0 the run's top (NONE: none), R8.1 its first x,
;   RD.1 this column's entry. Clobbers R8, R9.1, RB, FSV.
; ---------------------------------------------------------------------------
        RFIT    188
fld_run:
        GETV    T4
        PHI     R9
        LDI     HI(TOPS)
        PHI     RB
        GLO     R9
        ADI     LO(TOPS)
        PLO     RB
        LDI     NONE
        PLO     R8
fru1:   GHI     R9                      ; x < the end?
        STR     R2
        GLO     R9
        SM
        BDF     fru9
        LDA     RB                      ; t
        PHI     RD
        STR     R2
        GLO     R8                      ; the run's top again: on
        XOR
        BZ      fru8
        GHI     RD                      ; another: the run drawn (t kept
        PUTV    FTT                     ;  over fld_crust's FCALL)
        GLO     R8
        XRI     NONE
        BZ      fru2
        PARS
        GHI     R8
        PD
        GLO     R8
        PD
        GHI     R8
        STR     R2
        GLO     R9
        SM
        PD
        GLO     R8
        SDI     FH
        PD
        GETV    DIRTM
        PD
        GOP     TOP_RECT
        FCALL   fld_crust
fru2:   GETV    FTT
        PHI     RD
        LDI     NONE
        PLO     R8
        GHI     RD
        SMI     FH
        BDF     fru4                    ; not plain, or off the field
        GHI     RD                      ; plain: a new run from x
        PLO     R8
        GLO     R9
        PHI     R8
fru8:   INC     R9
        BR      fru1
fru4:   GHI     RD                      ; spans (F1-FF): by themselves
        SMI     E_NONE+1
        BNF     fru8
        GHI     R9                      ; (fld_spans spends R9.1, RB)
        PUTV    FSV
        GLO     RB
        PUTV    FSV+1
        LCALL   fld_spans
        GETV    FSV
        PHI     R9
        GETV    FSV+1
        PLO     RB
        LDI     HI(TOPS)
        PHI     RB
        LDI     NONE
        PLO     R8
        BR      fru8
fru9:   GLO     R8                      ; the last run
        XRI     NONE
        BZ      fru10
        PARS
        GHI     R8
        PD
        GLO     R8
        PD
        GHI     R8
        STR     R2
        GLO     R9
        SM
        PD
        GLO     R8
        SDI     FH
        PD
        GETV    DIRTM
        PD
        GOP     TOP_RECT
        FCALL   fld_crust
fru10:  FRET

; fld_crust: the run waiting (top R8.0, from x R8.1 to R9.0) its top line
;   solid, if the dirt has a crust (DCRUST). Keeps R8, R9, RB.
        RFIT    40
fld_crust:
        GETV    DCRUST
        BZ      fcr9
        PARS
        GHI     R8
        PD
        GLO     R8
        PD
        GHI     R8
        STR     R2
        GLO     R9
        SM
        PD
        P       1
        P       TM_SET
        GOP     TOP_RECT
fcr9:   FRET

; ---------------------------------------------------------------------------
; fld_all -- the whole field drawn from the model, held so it shows whole;
;   nothing left dirty. Clobbers R8, R9, RB, RC, T4-T7.
; ---------------------------------------------------------------------------
        RFIT    99
fld_all:
        HOLD1
        PARS
        P       0
        P       0
        P       FW
        P       FH
        P       TM_CLEAR
        GOP     TOP_RECT
        LDI     0
        PLO     R9
        LDI     FW
        PUTV    T4
        FCALL   fld_run
        HOLD0
        LDR     R8, DIRTY
        LDI     8
        PLO     R9
fal2:   LDI     0
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     fal2
        FRET

; ---------------------------------------------------------------------------
; fld_clear -- the model empty (every column F0), all of it dirty.
;   Clobbers R8, R9, RB.
; ---------------------------------------------------------------------------
        RFIT    74
fld_clear:
        LDR     RB, TOPS
        POKER   RB
        LDI     FW
        PLO     R9
fcl1:   LDI     E_NONE
        PKD
        DEC     R9
        GLO     R9
        BNZ     fcl1
        PKEND
        LDR     R8, DIRTY
        LDI     8
        PLO     R9
fcl2:   LDI     0FFH
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     fcl2
        FRET

; fld_mark: column D (0-63) dirty: drawn again from the model by fld_flush.
;   Keeps R8, R9, RC.
        RFIT    24
fld_mark:
        STR     R2
        ANI     7
        ADI     LO(BITS)
        PLO     RB
        LDI     HI(BITS)
        PHI     RB
        LDN     RB
        PHI     RD                      ; (its bit: RD.1 is free till FRET)
        LDN     R2
        SHR
        SHR
        SHR
        ADI     LO(DIRTY)
        PLO     RA
        GHI     RD
        STR     R2
        LDN     RA
        OR
        STR     RA
        FRET
