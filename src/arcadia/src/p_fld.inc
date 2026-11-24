; p_fld.inc -- the field's drawing (its own page, reached by FARPG): a
; TERRAIN column entry's bytes (the decoder's: p_net.inc PTERR), the tanks
; from the state (p_play.inc's: which seats the sprites show, the block
; tanks), the block tanks again (after terrain). The column primitives are
; fld.inc's; the cells' address and TKOFF fixed B's. M3's player draws
; through here too (or includes fld.inc on its own page).

        BCTA,UN PTCOL           ; E_COL: a TERRAIN column entry's byte (DOUT)
        BCTA,UN TKSTATE         ; E_TANKS: the tanks as the state has them
        BCTA,UN TKDRAW          ; E_TKDRAW: the block tanks off, then again
        BCTA,UN PFLUSH          ; E_COLEND: a G frame's end: waiting columns drawn
        BCTA,UN TKSEL           ; E_TKSEL: the shot moved tanks: the sprites'
                                ;  seats again, the block tanks drawn again

; ---- the tanks -------------------------------------------------------------------------
; STX: seat R1 as the state has it: R0 its x on the field ($FF: none -- not
; a tank, dead, or off the field), R2 its y, R3 its offset in the seats.
; Leaf. Keeps R1.
STX:    LODA,R0 SEAT25,R1
        STRZ    R3
        LODA,R0 ST+OFF_SEATS+P_Y,R3
        STRZ    R2
        LODA,R0 ST+OFF_SEATS+P_KIND,R3
        BCTR,EQ STX0
        LODA,R0 ST+OFF_SEATS+P_FLAGS,R3
        TMI,R0  PF_ALIVE
        BCFR,EQ STX0
        LODA,R0 ST+OFF_SEATS+P_X+1,R3
        BCFR,EQ STX0
        LODA,R0 ST+OFF_SEATS+P_X,R3
        COMI,R0 FIELD_W
        RETC,LT
STX0:   LODI,R0 $FF
        RETC,UN

; TKSTATE: the tanks as the state has them (play.c tanks_from_state): if
; any moved (or came, or went), the block tanks off, TKX/TKY/TKA from the
; state, the sprites' seats, the block tanks drawn; if only the sprites go
; to other seats, the block tanks drawn again; else the angles only. The sprites then (the vblank). Calls three down. Clobbers
; R0-R3, T0-T3.
TKSTATE: EORZ   R0              ; T0: moved
        STRA,R0 T0
        LODI,R1 0
TKS1:   BSTR,UN STX
        COMA,R0 TKX,R1
        BCFR,EQ TKS2
        COMI,R0 $FF
        BCTR,EQ TKS3            ; (none and none)
        LODA,R0 TKY,R1
        ANDI,R0 $7F
        COMZ    R2
        BCTR,EQ TKS3
TKS2:   LODI,R0 1
        STRA,R0 T0
TKS3:   ADDI,R1 1
        COMI,R1 SEATS
        BCFR,EQ TKS1
        LODA,R0 T0
        BCTR,EQ TKS6
        BSTA,UN TKOFF           ; moved: the block tanks off, all of it again
        CPTR    TKX
        LODI,R1 0
TKS4:   BSTA,UN STX
        BSTA,UN RAMPUT1
        ADDI,R1 1
        COMI,R1 SEATS
        BCFR,EQ TKS4
        LODI,R1 0
TKS5:   BSTA,UN STX
        LODZ    R2
        BSTA,UN RAMPUT1
        ADDI,R1 1
        COMI,R1 SEATS
        BCFR,EQ TKS5
        BSTR,UN TKANG
        BSTA,UN RAMPEND
; TKSEL: the sprites' seats from TKX, then the block tanks drawn.
TKSEL:  BSTA,UN SELECT
        BCTA,UN TKDRAW          ; tail call
TKS6:   BSTA,UN SELECT          ; not moved: the sprites' seats (T0 = 1 if
        LODA,R0 T0              ;  they change: drawn again)
        BCFA,EQ TKDRAW
TKS7:   CPTR    TKA             ; the angles only
        BSTR,UN TKANG
        BCTA,UN RAMPEND         ; tail call
; TKANG: the angles into the stream (TKA's place). Clobbers R0-R3.
TKANG:  LODI,R1 0
TKA1:   BSTA,UN STX
        LODA,R0 ST+OFF_SEATS+P_ANGLE,R3
        BSTA,UN RAMPUT1
        ADDI,R1 1
        COMI,R1 SEATS
        BCFR,EQ TKA1
        RETC,UN

; SELECT: the sprites' seats (look B, mock_arc.py sprites_for): the turn's,
; ours, a guidance target being chosen (the items line, only while we aim:
; AIMV is the shot's BEAMS while a shot plays), then the seats
; after the turn's (ours if no turn), on the field
; (TKX) only; the rest NONE. T0 = 1 if any changed. Calls two down. Clobbers
; R0-R3, T1-T3.
SELECT: EORZ    R0              ; T2: sprites chosen
        STRA,R0 T2
        LODA,R1 ST+OFF_TURN
        BSTA,UN ADDSEL
        LODA,R1 MYSEAT
        BSTA,UN ADDSEL
        LODA,R0 MYTURN          ; a guidance target being chosen: the next
        BCTR,EQ SEL0            ;  (p_hud.inc: it blinks)
        LODA,R0 *PIMENU
        COMI,R0 M_TARGET
        BCFR,EQ SEL0
        LODA,R0 *PTARGF
        STRZ    R1
        BSTA,UN ADDSEL
SEL0:   LODA,R0 ST+OFF_TURN     ; T3: the seat before the next looked at
        COMI,R0 SEATS
        BCTR,LT SEL1
        LODA,R0 MYSEAT
        COMI,R0 SEATS
        BCTR,LT SEL1
        LODI,R0 SEATS-1
SEL1:   STRA,R0 T3
        LODI,R0 SEATS           ; T1: seats left to look at
        STRA,R0 T1
SEL2:   LODA,R1 T3
        ADDI,R1 1
        COMI,R1 SEATS
        BCFR,EQ SEL3
        LODI,R1 0
SEL3:   STRA,R1 T3
        BSTR,UN ADDSEL
        LODA,R0 T1
        SUBI,R0 1
        STRA,R0 T1
        BCFR,EQ SEL2
SEL4:   LODA,R3 T2              ; the rest off
        COMI,R3 4
        RETC,EQ
        LODI,R0 NONE
        COMA,R0 SPRSEL,R3
        BCTR,EQ SEL5
        STRA,R0 SPRSEL,R3
        LODI,R0 1
        STRA,R0 T0
SEL5:   ADDI,R3 1
        STRA,R3 T2
        BCTR,UN SEL4
; ADDSEL: seat R1 the next sprite's, if it's a seat on the field not yet
; chosen and a sprite is left. Calls one down. Keeps R1.
ADDSEL: COMI,R1 SEATS
        RETC,GT
        RETC,EQ
        LODA,R3 T2
        COMI,R3 4
        RETC,EQ
        COMI,R3 0
        BCTR,EQ ADS2
ADS1:   LODA,R0 SPRSEL-1,R3     ; chosen already?
        COMZ    R1
        RETC,EQ
        BDRR,R3 ADS1
ADS2:   LODA,R0 TKX,R1          ; (TKX: the state's, or the shot's)
        COMI,R0 $FF
        RETC,EQ
        LODA,R3 T2
        LODZ    R1
        COMA,R0 SPRSEL,R3
        BCTR,EQ ADS3
        STRA,R0 SPRSEL,R3
        LODI,R0 1
        STRA,R0 T0
ADS3:   ADDI,R3 1
        STRA,R3 T2
        RETC,UN

; TKDRAW: the block tanks off, then drawn again from TKX/TKY: every tank
; on the field that no sprite shows, at y > 0, as 2 HOT blocks over its
; row (x - 1 and x, at y - 1), the cells under them kept first (TBC, TBN);
; TKY's b7 says which. The sprites again (the vblank). Calls three down.
; Clobbers R0-R3, T0-T3.
TKDRAW: BSTA,UN TKOFF
        LODA,R0 FDIRTY
        IORI,R0 FD_SPR
        STRA,R0 FDIRTY
        CPTR    TKROWS          ; no block tank's cell rows yet
        EORZ    R0
        BSTA,UN RAMPUT1
        EORZ    R0
        BSTA,UN RAMPUT1
        EORZ    R0
        BSTA,UN RAMPUT1
        BSTA,UN RAMPEND
        EORZ    R0              ; T1: the seat
        STRA,R0 T1
TD1:    LODA,R1 T1
        LODA,R0 TKY,R1          ; T3: its y as drawn (b7 a block tank's)
        ANDI,R0 $7F
        STRA,R0 T3
        LODA,R0 TKX,R1
        COMI,R0 $FF
        BCTA,EQ TD8
        LODA,R0 T3              ; (y 1-48)
        BCTA,EQ TD8
        COMI,R0 FIELD_H+1
        BCTA,GT TD8
        BCTA,EQ TD8
        LODI,R3 4               ; a sprite's?
TD2:    LODA,R0 SPRSEL-1,R3
        COMZ    R1
        BCTA,EQ TD8
        BDRR,R3 TD2
        LODA,R0 TBN             ; T2: its cells' place in TBC
        COMI,R0 SEATS-4
        BCTA,EQ TD8             ; (no room: can't be, 4 are sprites)
        RRL,R0
        STRA,R0 T2
        LODA,R1 T1              ; the cells as they are: the left's (none at
        LODA,R0 TKX,R1          ;  x 0)
        BCTR,EQ TD3
        SUBI,R0 1
        STRZ    R1
        BSTA,UN CELLP
        LODA,R0 *DROWH
        LODA,R3 T2
        CPUTR   TBC
TD3:    LODA,R1 T1              ; the right's
        LODA,R0 TKX,R1
        STRZ    R1
        BSTA,UN CELLP
        LODA,R0 *DROWH
        LODA,R3 T2
        ADDI,R3 1
        CPUTR   TBC
        LODA,R1 T1              ; then the HOT blocks
        LODA,R0 TKX,R1
        BCTR,EQ TD4
        SUBI,R0 1
        STRZ    R1
        BSTA,UN CELLP
        BSTA,UN HOTBLK
TD4:    LODA,R1 T1
        LODA,R0 TKX,R1
        STRZ    R1
        BSTA,UN CELLP
        BSTA,UN HOTBLK
        LODA,R0 T3              ; its cell row ((y - 1) / 2) in TKROWS
        SUBI,R0 1
        ANDI,R0 $FE
        RRR,R0
        STRA,R0 T2
        ANDI,R0 7
        STRZ    R1
        LODA,R0 T2
        RRR,R0
        RRR,R0
        RRR,R0
        ANDI,R0 $1F
        STRZ    R3
        LODA,R0 *PTKR,R3
        IORA,R0 TBIT8,R1
        CPUTR   TKROWS
        LODA,R0 TBN             ; one more
        ADDI,R0 1
        CPUT    TBN
        LODA,R0 T3              ; y's b7: a block tank
        IORI,R0 $80
        BCTR,UN TD9
TD8:    LODA,R0 T3              ; not one: b7 clear
TD9:    LODA,R3 T1
        COMA,R0 TKY,R3
        BCTR,EQ TD10
        CPUTR   TKY
TD10:   LODA,R0 T1
        ADDI,R0 1
        STRA,R0 T1
        COMI,R0 SEATS
        BCFA,EQ TD1
        RETC,UN

PTKR:   DWBE    TKROWS
PTARGF: DWBE    TARGET
TBIT8:  DB      1,2,4,8,$10,$20,$40,$80

; CELLP: the cell of block (R1 x, row T3 - 1) at DROWH:DROWL; R1 the
; block's bit in it. Calls one down. Clobbers R0, R2, R3.
CELLP:  LODA,R2 T3
        SUBI,R2 1
        BSTA,UN CELLAD
        STRA,R3 DROWL
        LODI,R0 SCR>>8
        COMI,R2 0
        BCTR,EQ CLP1
        LODI,R0 SCR2>>8
CLP1:   STRA,R0 DROWH
        RETC,UN

; HOTBLK: the block R1 (a bit) of the cell at DROWH:DROWL made HOT: a cell
; with HOT keeps its ground; a sky/dirt one gets the majority of its
; blocks left as its ground (mock_arc.py: all three can't show). Calls one
; down. Clobbers R0, R2, R3, T0.
HOTBLK: LODA,R0 *DROWH
        TMI,R0  $40
        BCFR,EQ HB2
        STRZ    R3              ; HOT already: its HOT and this one, the ground kept
        ANDI,R0 $3F
        IORZ    R1
        COMI,R0 $3F
        BCTA,EQ HB9
        STRZ    R1
        LODZ    R3
        ANDI,R0 $80
        IORI,R0 $40
        IORZ    R1
        STRA,R0 *DROWH
        RETC,UN
HB2:    COMI,R0 0               ; sky and dirt: T0 its dirt ($00: all), R1 HOT
        BCFR,EQ HB3
        LODI,R0 $3F
HB3:    ANDI,R0 $3F
        STRA,R0 T0
        LODZ    R1              ; the dirt left, the sky left: counted
        EORI,R0 $FF
        ANDA,R0 T0
        BSTR,UN POPC
        LODZ    R2              ; (R3: dirt blocks)
        STRZ    R3
        LODA,R0 T0
        EORI,R0 $3F
        STRA,R0 T0
        LODZ    R1
        EORI,R0 $FF
        ANDA,R0 T0
        STRA,R3 T0
        BSTR,UN POPC            ; (R2: sky blocks)
        LODZ    R1
        IORI,R0 $C0             ; sky as many or more: the ground sky
        COMA,R2 T0
        BCFR,LT HB4
        ANDI,R0 $7F             ; (else dirt)
HB4:    STRA,R0 *DROWH
        RETC,UN
HB9:    LODI,R0 $FF             ; all HOT
        STRA,R0 *DROWH
        RETC,UN
; POPC: R2 = the bits set in R0. Leaf. Clobbers R0.
POPC:   LODI,R2 0
PC1:    COMI,R0 0
        RETC,EQ
        TMI,R0  1
        BCFR,EQ PC2
        ADDI,R2 1
PC2:    ANDI,R0 $FE
        RRR,R0
        BCTR,UN PC1

; ---- the link's: terrain ------------------------------------------------------------
; PTCOL: a G frame's column entry's byte (DOUT; p_net.inc PTERR's, by FARPG
; from the decoder: the field showing, the block tanks off), by GST: an
; entry's first byte (under T_EMPTY: solid from that row; T_EMPTY: none;
; T_SPANS + n: n spans of top, end), a span's top, its end; then the next
; column (GMT, GX). Whole cells where it can: a cell column's three
; columns all simple (solid or none) in the frame are drawn together when
; the third comes (WCOL: each cell written once, no reads); the first two
; wait meanwhile (GPN of them, their tops GTOP, GSP). Any other column
; (spans, or a cell column the frame began inside) is drawn on its own
; into the cells (fld.inc), the waiting ones first (PFLUSH); so is a
; frame's end inside a cell column (E_COLEND).
; FLUSHC: PFLUSH's body (below), then on to label lab.
FLUSHC  MACRO   lab
        LODA,R0 GPN
        COMI,R0 1
        BCTA,LT lab
        COMI,R0 2
        BCTA,GT lab             ; ($FF: none)
        LODI,R0 4               ; the first's
        STRA,R0 GMT
        LODI,R1 0
        LODA,R2 GTOP
        BSTA,UN FILLS
        LODA,R1 GTOP
        LODI,R2 FIELD_H
        BSTA,UN FILLD
        LODI,R0 2
        STRA,R0 GMT
        LODA,R0 GPN
        COMI,R0 2
        BCFA,EQ lab
        LODI,R1 0               ; the second's
        LODA,R2 GSP
        BSTA,UN FILLS
        LODA,R1 GSP
        LODI,R2 FIELD_H
        BSTA,UN FILLD
        LODI,R0 1
        STRA,R0 GMT
        ENDM
PTCOL:  LODA,R0 GST
        COMI,R0 1
        BCTR,LT PCE
        BCTR,EQ PCT
        COMI,R0 2
        RETC,GT                 ; ($FF: dropped)
        LODA,R1 GCUR            ; 2: a span's end: sky down to its top, dirt
        LODA,R2 GTOP            ;  to its end
        BSTA,UN FILLS
        LODA,R1 GTOP
        LODA,R2 DOUT
        STRA,R2 GCUR
        BSTA,UN FILLD
        LODI,R0 1
        STRA,R0 GST
        LODA,R0 GSP
        SUBI,R0 1
        STRA,R0 GSP
        RETC,GT
        LODA,R1 GCUR            ; (the last: sky below it)
        LODI,R2 FIELD_H
        BSTA,UN FILLS
        BCTA,UN PCNEXT
PCT:    LODA,R0 DOUT            ; 1: a span's top
        STRA,R0 GTOP
        LODI,R0 2
        STRA,R0 GST
        RETC,UN
PCE:    LODA,R0 DOUT            ; 0: an entry
        COMI,R0 T_EMPTY
        BCTA,LT PCSOL
        BCTA,EQ PCEMP
        FLUSHC  PCE1            ; spans: the waiting columns first; this
PCE1:   LODI,R0 $FF             ;  cell column's whole cells are lost
        STRA,R0 GPN
        LODA,R0 DOUT            ; n of them
        ANDI,R0 $0F
        STRA,R0 GSP
        EORZ    R0
        STRA,R0 GCUR
        LODI,R0 1
        STRA,R0 GST
        RETC,UN
PCEMP:  LODI,R0 FIELD_H         ; none: solid from 48
PCSOL:  COMI,R0 FIELD_H         ; solid from R0 (48 at most)
        BCTR,LT PCS1
        LODI,R0 FIELD_H
PCS1:   STRZ    R2              ; (R2: the top)
        LODA,R0 GMT             ; R3: its third
        LODI,R3 0
        COMI,R0 4
        BCTR,EQ PCS2
        LODI,R3 1
        COMI,R0 2
        BCTR,EQ PCS2
        LODI,R3 2
PCS2:   COMA,R3 GPN             ; the thirds before it all waiting?
        BCFR,EQ PCS5
        COMI,R3 1
        BCTR,LT PCS3
        BCTR,EQ PCS4
        LODA,R0 GTOP            ; the third: the cell column, whole cells
        STRA,R0 T0
        LODA,R0 GSP
        STRA,R0 T1
        STRA,R2 T2
        BSTA,UN WCOL
        EORZ    R0
        STRA,R0 GPN
        BCTR,UN PCNEXT
PCS3:   STRA,R2 GTOP            ; the first: it waits
        LODI,R0 1
        STRA,R0 GPN
        BCTR,UN PCNEXT
PCS4:   STRA,R2 GSP             ; the second: it waits
        LODI,R0 2
        STRA,R0 GPN
        BCTR,UN PCNEXT
PCS5:   STRA,R2 GCUR            ; on its own: sky above, dirt below (GCUR:
        LODI,R1 0               ;  the top -- FILLS takes T0-T3)
        BSTA,UN FILLS
        LODA,R1 GCUR
        LODI,R2 FIELD_H
        BSTA,UN FILLD
PCNEXT: EORZ    R0              ; the next column: its third, its cell
        STRA,R0 GST
        LODA,R0 GMT
        RRR,R0
        STRA,R0 GMT
        COMI,R0 $80
        RETC,LT
        LODI,R0 4               ; (1 became $80: the next cell's left)
        STRA,R0 GMT
        EORZ    R0              ;  none waiting
        STRA,R0 GPN
        LODA,R0 GX
        ADDI,R0 1
        STRA,R0 GX
        COMI,R0 FIELD_W/3
        RETC,LT
        LODI,R0 $FF             ; (past the field's edge)
        STRA,R0 GST
        RETC,UN

; PFLUSH: the waiting columns (GPN: 1 or 2; their tops GTOP, GSP) drawn
; each on its own; none waiting after; GMT the column's after them again.
; E_COLEND: at a frame's end (p_net.inc FETERR); PCE has its own copy (a
; call less deep: FLUSHC). Calls two down. Clobbers R0-R3, T0-T3.
PFLUSH: FLUSHC  PFL1
PFL1:   EORZ    R0
        STRA,R0 GPN
        RETC,UN

; WCOL: cell column GX whole, from its thirds' tops T0, T1, T2 (each solid
; from its top): each cell written once -- the cells above every top sky
; and those under every top dirt in runs (WRUN), the few between worked
; out block by block. Calls one down. Clobbers R0-R3, T3, NUMV, NUMD.
WCOL:   LODA,R0 T0              ; NUMV: the highest top, NUMD: the lowest
        STRA,R0 NUMV
        STRA,R0 NUMD
        LODA,R0 T1
        BSTA,UN WMM
        LODA,R0 T2
        BSTA,UN WMM
        LODA,R0 NUMD            ; NUMD: the first cell row all dirt ((lowest + 1) / 2)
        ADDI,R0 1
        ANDI,R0 $FE
        RRR,R0
        STRA,R0 NUMD
        LODA,R0 NUMV            ; NUMV: the first not all sky (highest / 2)
        ANDI,R0 $FE
        RRR,R0
        STRA,R0 NUMV
        STRZ    R2              ; sky: rows 0 to NUMV - 1
        EORZ    R0
        STRA,R0 T3
        LODI,R1 CSKY
        BSTA,UN WRUN
WC1:    LODA,R0 T3              ; between: cell row T3, block by block
        COMA,R0 NUMD
        BCFA,LT WC9
        RRL,R0                  ; R2: its top row
        STRZ    R2
        LODI,R1 CSKY
        COMA,R2 T0
        BCTR,LT WC2A
        IORI,R1 4
WC2A:   COMA,R2 T1
        BCTR,LT WC2B
        IORI,R1 2
WC2B:   COMA,R2 T2
        BCTR,LT WC2C
        IORI,R1 1
WC2C:   ADDI,R2 1               ; (the bottom row)
        COMA,R2 T0
        BCTR,LT WC2D
        IORI,R1 $20
WC2D:   COMA,R2 T1
        BCTR,LT WC2E
        IORI,R1 $10
WC2E:   COMA,R2 T2
        BCTR,LT WC2F
        IORI,R1 8
WC2F:   LODA,R2 T3              ; one cell
        ADDI,R2 1
        BSTR,UN WRUN
        BCTA,UN WC1
WC9:    LODI,R1 CDIRT           ; dirt: the rest
        LODI,R2 FROWS
        ; (tail: WRUN)
; WRUN: cell rows T3 to R2 - 1 of cell column GX = R1 (none if T3 >= R2);
; T3 = R2 after. Leaf. Clobbers R0, R2, R3.
WRUN:   LODA,R0 T3              ; rows 0-12 (SCR)
        COMI,R0 13
        BCFR,LT WR3
        LODZ    R2              ; up to min(end, 13)
        COMI,R0 13
        BCTR,LT WR1
        LODI,R0 13
WR1:    SUBA,R0 T3
        BCTA,LT WR9
        BCTA,EQ WR9
        STRA,R2 T3              ; (T3: the end, for after; R2: the count)
        STRZ    R2
        LODA,R0 T3
        COMI,R0 13
        BCTR,LT WR1A
        LODI,R0 13
WR1A:   SUBZ    R2              ; R3: the first one's index
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        ADDA,R0 GX
        STRZ    R3
        LODZ    R1
WR2:    STRA,R0 SCR,R3
        ADDI,R3 16
        BDRR,R2 WR2
        LODA,R2 T3              ; the rest past row 13 (else T3 = the end)
        COMI,R2 13
        BCFR,GT WR9
        LODI,R0 13
        STRA,R0 T3
WR3:    LODZ    R2              ; rows 13-23 (SCR2)
        SUBA,R0 T3
        BCTR,LT WR9
        BCTR,EQ WR9
        STRA,R2 T3
        STRZ    R2
        LODA,R0 T3
        SUBZ    R2
        SUBI,R0 13
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        ADDA,R0 GX
        STRZ    R3
        LODZ    R1
WR4:    STRA,R0 SCR2,R3
        ADDI,R3 16
        BDRR,R2 WR4
        RETC,UN
WR9:    STRA,R2 T3
        RETC,UN
WMM:    COMA,R0 NUMV            ; R0 into NUMV (the lowest number: the highest
        BCFR,LT WMM1            ;  top) and NUMD (the highest)
        STRA,R0 NUMV
WMM1:   COMA,R0 NUMD
        RETC,LT
        STRA,R0 NUMD
        RETC,UN

        INCLUDE "src/fld.inc"
