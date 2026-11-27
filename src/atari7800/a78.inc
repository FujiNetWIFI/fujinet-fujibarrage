;
; a78.inc -- FujiBarrage on the Atari 7800 (the FujiNet cartridge): the
; chips and the memory map (barrage-a78.cfg; a78.h is the C side's copy).
;
; MARIA only: never the TIA (a store to $00-$1F rewrites INPTCTRL, which the
; cartridge keeps unlocked). The sound is the cartridge's POKEY at $0450.
;
; The field: MARIA 160A, the Atari's mode E packing (2 bits a pixel, pixel
; 0 in bits 7-6), 160x168 in 21 zones of 8 lines. Zone z's lines are in
; block FLDPG + 8 (z / 6), from byte 40 (z % 6); its line r (0 the top)
; on page block + 7 - r (MARIA counts OFFSET down). So $4000-$5FFF:
;
;   pages $40-$57   zones 0-17, bytes $00-$EF; $F0-$FB the underlays'
;                   strips (maria.s), $FC-$FF spare
;   pages $58-$5F   zones 18-20, bytes $00-$77; $78-$EF the text screen's
;                   24 rows of 40 (3 a page), $F0-$FF spare
;
BACKGRND = $20
WSYNC    = $24
MSTAT    = $28          ; bit 7: vertical blank
DPPH     = $2C
DPPL     = $30
CHARBASE = $34
OFFSET   = $38
CTRL     = $3C
P0C1     = $21          ; palette n colour c: $20 + 4n + c

POKEY    = $0450
SWCHA    = $0280
CTLSWA   = $0281
SWCHB    = $0282
CTLSWB   = $0283
INPT0    = $08
INPT1    = $09
INPT4    = $0C
BANKSEL  = $8000

CT_TEXT  = $4B          ; DMA on, border BACKGRND, 320A/C, 1-byte characters
CT_FIELD = $48          ; DMA on, border BACKGRND, 160A/B
CT_OFF   = $7F          ; DMA off

FLDPG    = $40          ; the field's first page
FONTPG   = $80          ; the font (REPL: every bank), CHARBASE
TXTPG    = $58          ; the text screen's rows: page $58 + r / 3,
TXTLO    = $78          ; byte $78 + 40 (r % 3)
NZONES   = 21           ; the field's zones
NTROWS   = 24           ; a text screen's rows
NHUD     = 3            ; the field screen's HUD rows
NSLOTZ   = 24           ; zones with display-list slots (two each)
SLOTSZ   = 64
