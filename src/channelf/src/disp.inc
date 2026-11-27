; ---------------------------------------------------------------------------
; disp.inc -- the screen. A pixel is written only when what it shows changes.
;
; The console writes VRAM one pixel at a time: the value to port 1, the
; column to port 4, the row (and the tone bits) to port 5, a $60 then $50
; strobe to port 0, then no port write for SETTLE clocks (flags.inc; the
; rigs' settle guard fails any run that writes sooner). A written pixel
; costs ~140 clocks at the least, so ~200 a frame: the lever is writing
; fewer pixels.
;
; THE FIELD is FLD (ram.inc): a byte a pixel, column-major, the port-1 code
; of the pixel's value. While VMODE is VM_FIELD the field's 102 x 46 pixels
; on the screen ARE FLD, so FSET and the column kernels write VRAM only
; where FLD changes; otherwise (a text screen up) they change FLD alone,
; and FSHOW paints it whole when the field comes back (the screen's field
; area all value 0 then: the text cleared first).
;
; TEXT is the 3x5 font in 4x6 cells, 24 columns, on 11 text rows: 0-8 the
; text screens', 9 and 10 the HUD's (under the field). TXS shadows every
; cell (code, ink): a cell drawn again writes only the pixels whose value
; changes between the old glyph and the new. Text rows 7-8 and the HUD
; overlap on the screen: a text screen clears the HUD first (TXCLR), the
; field clears rows 0-8.
;
; PALETTES: a row's palette is its columns 125 and 126 (bit 1 of each);
; PALS shadows them and PALSET writes only the columns that change.
; ---------------------------------------------------------------------------

VM_TEXT  EQU 0                  ; VMODE: the field area holds text rows (or nothing)
VM_FIELD EQU 1                  ;   the field is on the screen: FLD is what it shows

; NOPs after a column kernel's strobe: its loop spends exactly 94 clocks
; from the $50's write to the next pixel's OUTS 1 (tools/chf: count them).
PADK    EQU ((SETTLE-94+3)/4)*(SETTLE>94)
; FSHOW's and FHIDE's loops spend 66 (FHIDE's LI: 76).
PADS    EQU ((SETTLE-66+3)/4)*(SETTLE>66)
; PALSET from column 125's strobe to 126's OUTS 1: 70.
PADP    EQU ((SETTLE-70+3)/4)*(SETTLE>70)
; TXCH from a pixel's strobe to the next's OUTS 1: 96 at the least.
PADT    EQU ((SETTLE-96+3)/4)*(SETTLE>96)

; ---------------------------------------------------------------------------
; DINIT -- the screen cleared (the BIOS: value 0, palette 0 on every row),
;   the shadows to match, the field all sky, VM_TEXT.
; ---------------------------------------------------------------------------
DINIT:  ENTER
        CLR
        LR 3,A
        PI BCLRSCR
        DCI PALS                ; palette 0 everywhere
        LI 64
        LR 0,A
DIN1:   CLR
        ST
        DS 0
        BNZ DIN1
        DCI TXS                 ; every cell a blank
        LI (NTROWS*TCOLS+255)>>8
        LR 1,A
        LI (NTROWS*TCOLS)&0FFH
        LR 0,A
DIN2:   LI ' '
        ST
        LI CV1
        ST
        DS 0
        BNZ DIN2
        DS 1
        BNZ DIN2
        PI FCLEAR
        CLR
        STB VMODE
        LEAVE

; FCLEAR -- FLD all sky (only FLD: nothing on the screen).  Leaf.
;   Clobbers A, r0, r1, DC0.
FCLEAR: DCI FLD
        LI (FW*64+255)>>8       ; (r0 first: FW*64 = 25 x 256 + 128)
        LR 1,A
        LI (FW*64)&0FFH
        LR 0,A
FCL1:   LI PX_SKY
        ST
        DS 0
        BNZ FCL1
        DS 1
        BNZ FCL1
        POP

; ---------------------------------------------------------------------------
; FADDR -- DC0 = FLD + 64 r5 + r6 (the pixel (x, y)).  Leaf.  Clobbers A,
;   H (r10, r11).
; ---------------------------------------------------------------------------
FADDR:  LR A,5
        SR 1
        SR 1
        AI FLD>>8
        LR 10,A
        LR A,5
        SL 4
        SL 1
        SL 1
        AS 6
        LR 11,A
        LR DC,H
        POP

; ---------------------------------------------------------------------------
; FGET -- A = the field's pixel (r5, r6).  Leaf.  Clobbers H, DC0.
; ---------------------------------------------------------------------------
FGET:   LR A,5
        SR 1
        SR 1
        AI FLD>>8
        LR 10,A
        LR A,5
        SL 4
        SL 1
        SL 1
        AS 6
        LR 11,A
        LR DC,H
        LM
        POP

; ---------------------------------------------------------------------------
; FSET -- the field's pixel (r5, r6) = code r1; on the screen too when it
;   shows the field and the pixel changed.  Leaf.  Clobbers A, r4, H, DC0.
; ---------------------------------------------------------------------------
FSET:   LR A,5
        SR 1
        SR 1
        AI FLD>>8
        LR 10,A
        LR A,5
        SL 4
        SL 1
        SL 1
        AS 6
        LR 11,A
        LR DC,H
        LM
        XS 1
        BZ FSX                  ; the same: nothing
FSETW:  LR DC,H                 ; (FSETW: the pixel at H differs: r1 it is)
        LR A,1
        ST
        DCI VMODE
        LM
        CI VM_FIELD
        BNZ FSX
        DCI SNDB
        LM
        LR 4,A
        LR A,1
        OUTS 1
        LR A,5
        AI FX0
        COM
        OUTS 4
        LR A,6
        AI FY0
        COM
        NI 03FH
        AS 4
        OUTS 5
        LI 060H
        OUTS 0
        LI 050H
        OUTS 0
FSX:    POP

; ---------------------------------------------------------------------------
; FCOLW -- rows r6 .. r6 + r7 - 1 of column r5 (r7 >= 1, all inside the
;   field) = code r1: FLD, and the screen where the pixel changes when it
;   shows the field.  Leaf.  Clobbers A, r0, r2-r4, r7, H, DC0.
;
;   The loop: a pixel the same is 50 clocks; a written one 48 from its
;   OUTS 1 to the strobe, and 94 from the strobe to the next pixel's OUTS 1
;   (+ PADK NOPs): ~142 a written pixel.
; ---------------------------------------------------------------------------
FCOLW:  DCI VMODE
        LM
        CI VM_FIELD
        BNZ FCM                 ; not on the screen: FLD only
        DCI SNDB
        LM
        LR 4,A
        LR A,6                  ; r2 = tone | ~(FY0 + y): port 5, a DS a row
        AI FY0
        COM
        NI 03FH
        AS 4
        LR 2,A
        LR A,5                  ; the column, once
        AI FX0
        COM
        OUTS 4
        LI 060H
        LR 0,A
        LI 050H
        LR 3,A
        LR A,5                  ; DC0 = FLD + 64x + y
        SR 1
        SR 1
        AI FLD>>8
        LR 10,A
        LR A,5
        SL 4
        SL 1
        SL 1
        AS 6
        LR 11,A
        LR DC,H
FCW1:   LR A,1
        CM                      ; the same? (DC0 moves on)
        BZ FCW2
        LI 0FFH
        ADC
        LR A,1
        ST                      ; FLD = it (DC0 moves on)
        OUTS 1
        LR A,2
        OUTS 5
        LR A,0
        OUTS 0                  ; $60
        LR A,3
        OUTS 0                  ; $50: the strobe
        NOPS PADK
FCW2:   DS 2                    ; the next row
        DS 7
        BNZ FCW1
        POP
FCM:    LR A,5                  ; FLD only
        SR 1
        SR 1
        AI FLD>>8
        LR 10,A
        LR A,5
        SL 4
        SL 1
        SL 1
        AS 6
        LR 11,A
        LR DC,H
FCM1:   LR A,1
        ST
        DS 7
        BNZ FCM1
        POP

; ---------------------------------------------------------------------------
; FCOLSW -- rows r6 .. r6 + r7 - 1 of column r5 (r7 >= 1, inside the field):
;   each pixel that is r8 becomes r1 (FLD, and the screen when it shows the
;   field).  Leaf.  Clobbers A, r0, r2-r4, r7, H, DC0.  FCOLW's loop, the
;   test against r8: the same 94 clocks from a strobe to the next OUTS 1.
; ---------------------------------------------------------------------------
FCOLSW: DCI VMODE
        LM
        CI VM_FIELD
        BNZ FCSM
        DCI SNDB
        LM
        LR 4,A
        LR A,6
        AI FY0
        COM
        NI 03FH
        AS 4
        LR 2,A
        LR A,5
        AI FX0
        COM
        OUTS 4
        LI 060H
        LR 0,A
        LI 050H
        LR 3,A
        LR A,5
        SR 1
        SR 1
        AI FLD>>8
        LR 10,A
        LR A,5
        SL 4
        SL 1
        SL 1
        AS 6
        LR 11,A
        LR DC,H
FCS1:   LR A,8
        CM                      ; r8 here? (DC0 moves on)
        BNZ FCS2
        LI 0FFH
        ADC
        LR A,1
        ST
        OUTS 1
        LR A,2
        OUTS 5
        LR A,0
        OUTS 0
        LR A,3
        OUTS 0
        NOPS PADK
FCS2:   DS 2
        DS 7
        BNZ FCS1
        POP
FCSM:   LR A,5                  ; FLD only
        SR 1
        SR 1
        AI FLD>>8
        LR 10,A
        LR A,5
        SL 4
        SL 1
        SL 1
        AS 6
        LR 11,A
        LR DC,H
FCSM1:  LR A,8
        CM
        BNZ FCSM2
        LI 0FFH
        ADC
        LR A,1
        ST
FCSM2:  DS 7
        BNZ FCSM1
        POP

; ---------------------------------------------------------------------------
; FSHOW -- the field onto the screen, which shows value 0 there (text rows
;   0-8 blank): every pixel not sky written; VM_FIELD.  The rows' palettes
;   are the caller's.
; ---------------------------------------------------------------------------
FSHOW:  ENTER
        DCI SNDB
        LM
        LR 4,A
        LI 060H
        LR 0,A
        LI 050H
        LR 3,A
        CLR
        LR 5,A                  ; x
FSH1:   LR A,5                  ; the column
        AI FX0
        COM
        OUTS 4
        LI FY0                  ; r2 = tone | ~FY0
        COM
        NI 03FH
        AS 4
        LR 2,A
        LR A,5                  ; DC0 = FLD + 64x
        SR 1
        SR 1
        AI FLD>>8
        LR 10,A
        LR A,5
        SL 4
        SL 1
        SL 1
        LR 11,A
        LR DC,H
        LI FH
        LR 7,A
FSH2:   LM
        CI PX_SKY
        BZ FSH3
        OUTS 1
        LR A,2
        OUTS 5
        LR A,0
        OUTS 0
        LR A,3
        OUTS 0
        NOPS PADS
FSH3:   DS 2
        DS 7
        BNZ FSH2
        LR A,5
        INC
        LR 5,A
        CI FW-1
        BC FSH1
        LIS VM_FIELD
        STB VMODE
        LEAVE

; ---------------------------------------------------------------------------
; FHIDE -- the field off the screen: every pixel not sky written 0 (the
;   screen's field area all value 0 again); VM_TEXT.  FLD keeps it.
; ---------------------------------------------------------------------------
FHIDE:  ENTER
        DCI VMODE
        LM
        CI VM_FIELD
        BZ FHD0
        LEAVE
FHD0:   DCI SNDB
        LM
        LR 4,A
        LI 060H
        LR 0,A
        LI 050H
        LR 3,A
        CLR
        LR 5,A
FHD1:   LR A,5
        AI FX0
        COM
        OUTS 4
        LI FY0
        COM
        NI 03FH
        AS 4
        LR 2,A
        LR A,5
        SR 1
        SR 1
        AI FLD>>8
        LR 10,A
        LR A,5
        SL 4
        SL 1
        SL 1
        LR 11,A
        LR DC,H
        LI FH
        LR 7,A
FHD2:   LM
        CI PX_SKY
        BZ FHD3
        LI CV0
        OUTS 1
        LR A,2
        OUTS 5
        LR A,0
        OUTS 0
        LR A,3
        OUTS 0
        NOPS PADS
FHD3:   DS 2
        DS 7
        BNZ FHD2
        LR A,5
        INC
        LR 5,A
        CI FW-1
        BC FHD1
        CLR
        STB VMODE
        LEAVE

; ---------------------------------------------------------------------------
; PALSET -- screen rows r0 .. r1 (inclusive, 0-63) in palette r2 (0-3):
;   only the palette columns that change are written.  Leaf.
;   Clobbers A, r0, r3-r8, DC0.
; ---------------------------------------------------------------------------
PALSET: DCI SNDB
        LM
        LR 8,A
PLS1:   LR A,0                  ; the row's palette now
        DCI PALS
        ADC
        LM
        XS 2
        BZ PLS4                 ; the same
        LR 3,A                  ; the bits that change
        LI 0FFH
        ADC
        LR A,2
        ST                      ; PALS = it
        LR A,0                  ; r5 = port 5: tone | ~row
        COM
        NI 03FH
        AS 8
        LR 5,A
        LR A,3                  ; column 125: bit 0
        NI 1
        BZ PLS2
        LR A,2
        NI 1
        LI CV0
        BZ PLS1A
        LI CV2
PLS1A:  OUTS 1
        LI (~PALCOL)&0FFH
        OUTS 4
        LR A,5
        OUTS 5
        LI 060H
        OUTS 0
        LI 050H
        OUTS 0
        NOPS PADP               ; (the settle: column 126 may be next)
PLS2:   LR A,3                  ; column 126: bit 1
        NI 2
        BZ PLS4
        LR A,2
        NI 2
        LI CV0
        BZ PLS2A
        LI CV2
PLS2A:  OUTS 1
        LI (~(PALCOL+1))&0FFH
        OUTS 4
        LR A,5
        OUTS 5
        LI 060H
        OUTS 0
        LI 050H
        OUTS 0
PLS4:   LR A,0
        XS 1
        BZ PLSX
        LR A,0
        INC
        LR 0,A
        BR PLS1
PLSX:   POP

; ---------------------------------------------------------------------------
; Text.
;
; TXCH -- the character r0 in TXINK at text cell (TXX, TXY); TXX moves on.
;   Only the pixels that change are written.  Clobbers A, r0-r8, H, Q, DCs.
; TXSTR -- the string at DC0 (NUL ends it) from (TXX, TXY).
; TXCLR -- text rows r0 .. r1 blank.
; TXAT -- TXX = r0, TXY = r1, TXINK = r2.  Leaf.
; ---------------------------------------------------------------------------
TXAT:   LR A,0
        STB TXX
        LR A,1
        STB TXY
        LR A,2
        STB TXINK
        POP

TXSTR:  ENTER
TXS1:   LM
        LR 0,A
        NI 0FFH
        BZ TXSX
        LR Q,DC
        LR A,QU                 ; (TXCH spends Q: the pointer lives in RAM)
        STB TXPTR
        LR A,QL
        STB TXPTR+1
        PI TXCH
        DCI TXPTR
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        BR TXS1
TXSX:   LEAVE

TXCLR:  ENTER
        LR A,0
        STB GT
        LR A,1
        STB GT+1
TXC1:   CLR
        STB TXX
        LDB GT
        STB TXY
TXC2:   LI ' '
        LR 0,A
        PI TXCH
        LDB TXX
        CI TCOLS-1
        BC TXC2
        LDB GT
        LR 0,A
        LDB GT+1
        XS 0
        BZ TXCX
        LR A,0
        INC
        STB GT
        BR TXC1
TXCX:   LEAVE

; each text row's cells in TXS
TXROWS: DW TXS, TXS+48, TXS+96, TXS+144, TXS+192, TXS+240, TXS+288, TXS+336
        DW TXS+384, TXS+432, TXS+480
; the screen line of each text row's top
TROWY:  DB TY0, TY0+6, TY0+12, TY0+18, TY0+24, TY0+30, TY0+36, TY0+42, TY0+48
        DB HUDY0, HUDY1

TXCH:   LDB TXX                 ; past the row's last column: nothing
        CI TCOLS-1
        BC TXCH0
        INC
        STB TXX
        POP
TXCH0:  LR A,0                  ; lower case folds to upper; past $5F a blank
        CI 060H-1
        BC TXH0
        AI -32
        CI 05FH
        BC TXH0
        LI ' '
TXH0:   CI ' '-1                ; (control characters: a blank)
        BNC TXH0A
        LI ' '
TXH0A:  LR 0,A                  ; r0 = the new code
        LDB TXINK
        LR 1,A                  ; r1 = the new ink
        LDB TXX                 ; the cell: TXROWS[TXY] + 2 col
        SL 1
        LR 2,A
        LDB TXY
        SL 1
        DCI TXROWS
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LR A,2
        ADC
        LR Q,DC                 ; Q = the cell
        LM
        LR 3,A                  ; r3 = the old code
        LM
        LR 4,A                  ; r4 = the old ink
        LR A,3
        XS 0
        BNZ TXH1
        LR A,4
        XS 1
        BNZ TXH1
        JMP TXHNX               ; nothing changes
TXH1:   LR DC,Q                 ; the shadow = the new cell
        LR A,0
        ST
        LR A,1
        ST
        ; the pixels: x0 = TX0 + 4 TXX, y0 = TROWY[TXY]
        LDB TXX
        SL 1
        SL 1
        AI TX0
        STB DS0                 ; x0
        LDB TXY
        DCI TROWY
        ADC
        LM
        STB DS0+1               ; y0 (the row we're on)
        LR A,3                  ; H = the old glyph's row 0 (FNTR0 + code - $20)
        AI -32
        DCI FNTR0
        ADC                     ; (0-63: ADC's signed range is enough)
        LR H,DC
        LR A,0                  ; Q = the new one's
        AI -32
        DCI FNTR0
        ADC
        LR Q,DC
        LIS 5
        STB DS0+2               ; rows left
TXH2:   LR DC,H                 ; the old row's bits; the next row 64 on
        LM
        LR 5,A                  ; r5 = old bits
        LI 63
        ADC
        LR H,DC
        LR DC,Q                 ; the new row's
        LM
        LR 6,A                  ; r6 = new bits
        LI 63
        ADC
        LR Q,DC
        LR A,4                  ; the inks the same: what differs is the bits
        XS 1
        LR A,5
        BNZ TXH3
        XS 6
        BR TXH4
TXH3:   LR 7,A                  ; inks differ: every pixel lit in either
        LR A,6
        COM
        NS 7                    ; old & ~new ...
        AS 6                    ; ... + new = old | new
TXH4:   LR 7,A                  ; r7 = the pixels to write (bit 2 = the left)
        NI 0FFH
        BZ TXH9
        DCI SNDB
        LM
        LR 8,A
        LDB DS0+1               ; port 5: tone | ~y
        COM
        NI 03FH
        AS 8
        LR 8,A
        LDB DS0                 ; r2 = ~x of the left pixel
        COM
        LR 2,A
        LIS 4                   ; bit 2, 1, 0
        LR 5,A
TXH5:   LR A,7
        NS 5
        BZ TXH7
        LR A,6                  ; lit in the new: the ink, else 0
        NS 5
        LR A,1
        BNZ TXH6
        LI CV0
TXH6:   OUTS 1
        LR A,2
        OUTS 4
        LR A,8
        OUTS 5
        LI 060H
        OUTS 0
        LI 050H
        OUTS 0
        NOPS PADT               ; (the settle)
TXH7:   DS 2                    ; the next column (~x - 1)
        LR A,5
        SR 1
        LR 5,A
        NI 0FFH
        BNZ TXH5
TXH9:   LDB DS0+1               ; the next row
        INC
        STB DS0+1
        DECB DS0+2
        BZ TXHNX
        JMP TXH2
TXHNX:  LDB TXX
        INC
        STB TXX
        POP

; ---------------------------------------------------------------------------
; DCLS -- a clear screen for text: the field off, every text row blank, every
;   row in palette r0 (0: black, white text).
; ---------------------------------------------------------------------------
DCLS:   ENTER
        LR A,0
        STB DS0+12
        PI FHIDE
        CLR
        LR 0,A
        LI NTROWS-1
        LR 1,A
        PI TXCLR
        CLR
        LR 0,A
        LI 63
        LR 1,A
        LDB DS0+12
        LR 2,A
        PI PALSET
        LEAVE

; TXCENT -- the string at DC0 centred on text row r1 in ink r2.
TXCENT: ENTER
        LR Q,DC
        CLR
        LR 0,A
TXCE1:  LM                      ; its length
        NI 0FFH
        BZ TXCE2
        LR A,0
        INC
        LR 0,A
        BR TXCE1
TXCE2:  LR A,0                  ; TXX = (24 - length) / 2
        COM
        INC
        AI TCOLS
        SR 1
        LR 0,A
        PI TXAT
        LR DC,Q
        PI TXSTR
        LEAVE
