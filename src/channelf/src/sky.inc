; ---------------------------------------------------------------------------
; sky.inc -- the field's rows' palettes: the round's sky (OFF_SKY). The sky
; is value 0, each row's background, so a sky is a set of row palettes:
; light blue (1), light grey (2), light green (3), or black (0, where every
; ink is white). Resident: the flash and the shake change them too.
;
; SKYTAB: by sky, a list of bands (the band's last field row, its palette),
; the last ending at FH-1; or FF, p: a night sky, black (palette 0) above
; the hills and the tanks, palette p from a row over them down (checkpoint 1,
; 2026-11-27: STARS grey, BLACK blue; STORM a black cloud band over grey).
; ---------------------------------------------------------------------------

SKYTAB: DW SKPLAIN, SKGRAD, SKSTARS, SKSTORM, SKSUNSET, SKCAVE, SKBLACK, SKPLAIN
SKPLAIN: DB FH-1, 1
SKGRAD: DB 13, 2, FH-1, 1
SKSTARS: DB 0FFH, 2
SKSTORM: DB 7, 0, FH-1, 2
SKSUNSET: DB 13, 1, 29, 2, FH-1, 3
SKCAVE: DB FH-1, 2
SKBLACK: DB 0FFH, 1

; SKYSET -- the field's rows in the round's sky (ST+OFF_SKY), the HUD's rows
;   black.
SKYSET: ENTER
        LDB ST+OFF_SKY
        CI 7
        BC SKS0
        CLR
SKS0:   SL 1
        DCI SKYTAB
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LIS FY0                 ; the first band's first screen row
        STB SKY0
        LR DC,Q                 ; a night sky?
        LM
        CI 0FFH
        BNZ SKS1
        LM
        STB SKYP
        PI SKHOR                ; r1 = the horizon's row
        LR A,1
        NI 0FFH
        BNZ SKS0A
        LI FY0-1                ; (none above: all below)
        LR 1,A
        BR SKS0B
SKS0A:  AI FY0-1                ; black above it
        LR 1,A
        LIS FY0
        LR 0,A
        CLR
        LR 2,A
        PI PALSET
SKS0B:  LR A,1                  ; (PALSET keeps r1) its palette below
        INC
        LR 0,A
        LI FY0+FH-1
        LR 1,A
        LDB SKYP
        LR 2,A
        PI PALSET
        BR SKS2
SKS1:   LR DC,Q                 ; a band: rows SKY0 .. FY0 + its last
        LM
        LR 1,A
        LM
        LR 2,A
        LR Q,DC
        LR A,1
        AI FY0
        LR 1,A
        STB SKY1
        LDB SKY0
        LR 0,A
        PI PALSET
        LDB SKY1
        INC
        STB SKY0
        CI FY0+FH
        BNZ SKS1
SKS2:   LDB MSGJ                ; the HUD's rows: black (a message's band,
        DCI FLK                 ;  a flash: theirs)
        OM
        BZ SKS3
        LEAVE
SKS3:   LI HUDY0
        LR 0,A
        LI 63
        LR 1,A
        CLR
        LR 2,A
        PI PALSET
        LEAVE

; SKHOR -- r1 = the night sky's horizon: the row over the highest column top
;   (FLD + 64x + FH, fld.inc) or tank (its barrel's and pennant's top: y - 7),
;   0 at least.  Leaf.  Clobbers A, r1-r4, DC0.
SKHOR:  DCI FLD+FH
        LI FH
        LR 1,A                  ; the least so far
        LI FW
        LR 2,A
SKH1:   LM                      ; this column's top
        LR 3,A
        COM                     ; less than the least: C
        AS 1
        BNC SKH2
        LR A,3
        LR 1,A
SKH2:   LI 63
        ADC
        DS 2
        BNZ SKH1
        DCI TKON                ; the tanks wanted
        LI SEATS
        LR 2,A
        CLR
        LR 4,A
SKH3:   LR A,4
        DCI TKON
        ADC
        LM
        NI 0FFH
        BZ SKH4
        LR A,4
        DCI TKY
        ADC
        LM
        AI -7
        BNC SKH5                ; (above the field: 0)
        LR 3,A
        COM
        AS 1
        BNC SKH4
        LR A,3
        LR 1,A
        BR SKH4
SKH5:   CLR
        LR 1,A
SKH4:   LR A,4
        INC
        LR 4,A
        DS 2
        BNZ SKH3
        LR A,1                  ; a row over it
        NI 0FFH
        BZ SKH6
        AI -1
        LR 1,A
SKH6:   POP

; PALX -- screen rows r0 .. r1, each row's palette XOR r2 (the flash, the
;   shake: SKYSET puts them back).  Clobbers A, r0-r8, DC0.
PALX:   ENTER
        LR A,0
        STB PXR0
        LR A,1
        STB PXR1
        LR A,2
        STB PXM
PXX1:   LDB PXR0
        LR 0,A
        LR 1,A
        DCI PALS
        ADC
        LM
        LR 2,A
        LDB PXM
        XS 2
        LR 2,A
        PI PALSET
        LDB PXR0
        LR 1,A
        INC
        STB PXR0
        LDB PXR1
        XS 1
        BNZ PXX1
        LEAVE
