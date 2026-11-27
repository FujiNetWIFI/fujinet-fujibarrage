; ---------------------------------------------------------------------------
; fld.inc -- the field's columns: a TERRAIN entry (and a settle's, the same
; form) written into FLD and, while the field shows, the screen. Resident.
;
; An entry: 00-EF dirt from that row down; F0 no dirt; F0+n then n spans
; (top, end exclusive). Rows past the field are clipped. Each column's top
; (its first dirt row, FH: none) is kept in the column's spare row FH
; (FLD + 64x + FH): the night skies' horizon (sky.inc).
; ---------------------------------------------------------------------------

; FCOLENT -- column r5 from the entry at DC1 (DC1 moves past it).
;   Clobbers A, r0-r4, r6-r8, H, DC0; keeps r5, DC1.
FCOLENT: ENTER
        XDC
        LM
        XDC
        LR 8,A                  ; the entry
        STB FCTOP               ; (its top: the entry, or a span's)
        CI 0EFH
        BNC FCE5                ; F0-FF: spans
        CI FH-1                 ; (past the field: all sky)
        BC FCE1
        LI FH
        LR 8,A
        STB FCTOP
FCE1:   LR A,8                  ; sky above it
        NI 0FFH
        BZ FCE2
        LR 7,A
        CLR
        LR 6,A
        LI PX_SKY
        LR 1,A
        PI FCOLW
FCE2:   LR A,8                  ; dirt from it down
        LR 6,A
        COM
        INC
        AI FH
        BZ FCEX
        LR 7,A
        LI PX_DIRT
        LR 1,A
        PI FCOLW
FCEX:   PI FCSTOP
        LEAVE
FCE5:   AI 010H                 ; n spans (- F0)
        STB DS0+9
        LI FH                   ; (none yet: no top)
        STB FCTOP
        CLR
        STB DS0+8               ; the row we're at
FCE6:   LDB DS0+9
        NI 0FFH
        BZ FCE9
        AI -1
        STB DS0+9
        XDC                     ; top, end
        LM
        LR 6,A
        LM
        LR 8,A
        XDC
        LR A,6                  ; (clipped to the field)
        CI FH
        BC FCE6A
        LI FH
        LR 6,A
FCE6A:  LR A,8
        CI FH
        BC FCE6B
        LI FH
        LR 8,A
FCE6B:  LDB FCTOP               ; the first span's top is the column's
        CI FH
        BNZ FCE6C
        LR A,6
        STB FCTOP
FCE6C:  LDB DS0+8               ; sky from where we are to the top
        LR 0,A
        COM
        INC
        AS 6                    ; top - at
        BZ FCE7
        BM FCE7                 ; (a span above us: nothing)
        LR 7,A
        LR A,6                  ; (the top kept: FCOLW's row is r6)
        STB DS0+10
        LR A,0
        LR 6,A
        LI PX_SKY
        LR 1,A
        PI FCOLW
        LDB DS0+10
        LR 6,A
FCE7:   LR A,6                  ; dirt from the top to the end
        COM
        INC
        AS 8
        BZ FCE8
        BM FCE8
        LR 7,A
        LI PX_DIRT
        LR 1,A
        PI FCOLW
FCE8:   LR A,8
        STB DS0+8
        BR FCE6
FCE9:   LDB DS0+8               ; sky to the bottom
        LR 6,A
        COM
        INC
        AI FH
        BZ FCEX2
        BM FCEX2
        LR 7,A
        LI PX_SKY
        LR 1,A
        PI FCOLW
FCEX2:  PI FCSTOP
        LEAVE

; FCSTOP -- column r5's top (FCTOP) into its spare row.  Leaf.
;   Clobbers A, H, DC0.
FCSTOP: LR A,5
        SR 1
        SR 1
        AI FLD>>8
        LR 10,A
        LR A,5
        SL 4
        SL 1
        SL 1
        AI FH
        LR 11,A
        LR DC,H
        DCI FCTOP
        LM
        LR DC,H
        ST
        POP
