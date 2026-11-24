; p_ops.inc -- the script's paths (OP_PATH) and beams (OP_BEAM), view.go's,
; op for op (the flows: p_set.inc) (the Astrocade's src/astrocade/src/
; p_ops.inc in 2650 form). Paths and flows read their bytes where they lie
; in SCRIPT; at a segment's end they finish (E_OSEGEND: View.EndSegment).
; Coordinates are bytes (the low byte of a 16-bit one: wrapped past an
; edge, simply off the field). The PLY page calls these (FARPG): an op's
; arguments at PPC, its bytes taken in R2.

        BCTA,UN OSTEP           ; E_OSTEP: a frame of each
        BCTA,UN OPATH           ; E_OPATH: OP_PATH's arguments at PPC
        BCTA,UN OBEAM           ; E_OBEAM
        BCTA,UN OSEGEND         ; E_OSEGEND: the segment's end (the paths)

; RAM-H through pointer words (R3 the record's offset)
PA0:    DWBE    PATHS           ; on
PA1:    DWBE    PATHS+1         ; style
PA2:    DWBE    PATHS+2         ; x, y
PA3:    DWBE    PATHS+3
PA4:    DWBE    PATHS+4         ; steps left
PA5:    DWBE    PATHS+5         ; drawn
PA6:    DWBE    PATHS+6         ; the next step's pc
BE0:    DWBE    BEAMS           ; frames left (0: off)
BE1:    DWBE    BEAMS+1         ; x0, y0, x1, y1
BE2:    DWBE    BEAMS+2
BE3:    DWBE    BEAMS+3
BE4:    DWBE    BEAMS+4
POFF:   DB      0,PASZ,2*PASZ,3*PASZ

; ---- paths ---------------------------------------------------------------------------
; PHEAD: path PTMP's head drawn (view.go pathHead): a roller over air, a
; digger over air or dirt; drawn if so. Calls three down. Clobbers R0-R3.
PHEAD:  LODA,R3 PTMP
        LODA,R0 *PA2,R3
        STRA,R0 BX
        LODA,R0 *PA3,R3
        STRA,R0 BY
        STRZ    R1
        STRZ    R2
        BSTA,UN KTANK
        LODI,R0 PXFX
        STRA,R0 BTO
        LODI,R1 FR_SKY
        LODA,R3 PTMP
        LODA,R0 *PA1,R3
        COMI,R0 ST_DIGGER
        BCFR,EQ PH1
        LODI,R1 FR_SKY|FR_DIRT
PH1:    STRA,R1 BFROM
        BSTA,UN KPLOT
        COMI,R0 0
        BCTR,EQ PH2
        LODI,R0 1
PH2:    LODA,R3 PTMP
        CPUTR   PATHS+5
        RETC,UN

; PLEAVE: path PTMP's head moves on: if drawn, air again where it's still
; fire (a digger bores). Calls three down. Clobbers R0-R3.
PLEAVE: LODA,R3 PTMP
        LODA,R0 *PA5,R3
        RETC,EQ
        EORZ    R0
        CPUTR   PATHS+5
        LODA,R3 PTMP
        LODA,R0 *PA2,R3
        STRA,R0 BX
        LODA,R0 *PA3,R3
        STRA,R0 BY
        STRZ    R1
        STRZ    R2
        BSTA,UN KTANK
        LODI,R0 FR_FX
        STRA,R0 BFROM
        LODI,R0 PXSKY
        STRA,R0 BTO
        BCTA,UN KPLOT           ; tail call

; PEND: path PTMP stops (its head away). Calls three down (PLEAVE's).
PEND:   LODA,R3 PTMP
        LODA,R0 *PA0,R3
        RETC,EQ
        EORZ    R0
        CPUTR   PATHS
        BCTA,UN PLEAVE          ; tail call

; OPATH: slot, style, x16, y16, n, n steps: a path (one live in its slot
; ends first), its head drawn. R2 = 7 + n.
OPATH:  LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3
        ANDI,R0 NPATH-1
        STRZ    R1
        LODA,R0 POFF,R1
        STRA,R0 PTMP
        BSTR,UN PEND
        LODI,R1 FRRAMP2+((PATHS-$2900)>>8)
        LODA,R3 PTMP
        ADDI,R3 (PATHS-$2900)&$FF
        BSTA,UN RAMSET
        LODI,R2 1               ; on
        LODA,R0 *PRDAT,R2
        LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3,+   ; style
        STRZ    R2
        LODA,R0 *PRDAT,R2
        LODA,R0 *PSCRIPT,R3,+   ; x (its low byte)
        STRZ    R2
        LODA,R0 *PRDAT,R2
        ADDI,R3 1
        LODA,R0 *PSCRIPT,R3,+   ; y
        STRZ    R2
        LODA,R0 *PRDAT,R2
        ADDI,R3 1
        LODA,R0 *PSCRIPT,R3,+   ; n
        STRA,R0 EX
        STRZ    R2
        LODA,R0 *PRDAT,R2
        LODI,R2 0               ; drawn
        LODA,R0 *PRDAT,R2
        ADDI,R3 1               ; its steps' pc
        LODZ    R3
        STRZ    R2
        LODA,R0 *PRDAT,R2
        BSTA,UN RAMPEND
        BSTA,UN PHEAD
        LODA,R2 EX
        ADDI,R2 7
        RETC,UN

; PSTEP: every live path a step: its head moves by the next byte's signed
; nibbles (dx << 4 | dy); none left: it stops. Calls four down.
PSTEP:  LODI,R0 0
        STRA,R0 PTMP
PS1:    LODA,R3 PTMP
        LODA,R0 *PA0,R3
        BCTA,EQ PS9
        LODA,R0 *PA4,R3
        BCFR,EQ PS2
        BSTA,UN PEND
        BCTA,UN PS9
PS2:    BSTA,UN PLEAVE
        LODA,R3 PTMP            ; the step: n - 1, pc + 1
        LODA,R0 *PA6,R3
        STRA,R0 EX
        LODA,R0 *PA4,R3
        SUBI,R0 1
        CPUTR   PATHS+4
        LODA,R0 EX
        ADDI,R0 1
        LODA,R3 PTMP
        CPUTR   PATHS+6
        LODA,R3 EX              ; dx: the high nibble, signed
        LODA,R0 *PSCRIPT,R3
        STRA,R0 EX
        RRR,R0
        RRR,R0
        RRR,R0
        RRR,R0
        BSTR,UN SNIB
        LODA,R3 PTMP
        ADDA,R0 *PA2,R3
        CPUTR   PATHS+2
        LODA,R0 EX              ; dy: the low
        BSTR,UN SNIB
        LODA,R3 PTMP
        ADDA,R0 *PA3,R3
        CPUTR   PATHS+3
        BSTA,UN PHEAD
PS9:    LODA,R0 PTMP
        ADDI,R0 PASZ
        STRA,R0 PTMP
        COMI,R0 NPATH*PASZ
        BCTA,LT PS1
        BCTA,UN BSTEP
; SNIB: R0's low nibble, signed (-8..7). Leaf.
SNIB:   ANDI,R0 $0F
        TMI,R0  $08
        RETC,LT
        IORI,R0 $F0
        RETC,UN

; ---- beams --------------------------------------------------------------------------
; BMLINE: beam PTMP's line (view.go line(): Bresenham) by BFROM, BTO.
; Calls four down. Clobbers R0-R3, the E* bytes.
BMLINE: LODA,R3 PTMP
        LODA,R0 *BE1,R3         ; ECX, ECY: the point
        STRA,R0 ECX
        LODA,R0 *BE2,R3
        STRA,R0 ECY
        LODA,R0 *BE3,R3         ; ERX: dx = |x1 - x0|, EX: sx
        LODI,R1 1
        SUBA,R0 ECX
        TMI,R0  $80
        BCFR,EQ BL1
        EORI,R0 $FF
        ADDI,R0 1
        LODI,R1 $FF
BL1:    STRA,R0 ERX
        STRA,R1 EX
        LODA,R0 *BE4,R3         ; ERY: dy, EY: sy
        LODI,R1 1
        SUBA,R0 ECY
        TMI,R0  $80
        BCFR,EQ BL2
        EORI,R0 $FF
        ADDI,R0 1
        LODI,R1 $FF
BL2:    STRA,R0 ERY
        STRA,R1 EY
        LODA,R0 ECY             ; the block tanks on its rows off
        STRZ    R1
        LODA,R0 *BE4,R3
        STRZ    R2
        COMZ    R1
        BCFR,LT BL2A
        STRZ    R1              ; (the lower first)
        LODA,R0 ECY
        STRZ    R2
BL2A:   BSTA,UN KTANK
        LODA,R0 ERX             ; ERYO: err = dx - dy
        SUBA,R0 ERY
        STRA,R0 ERYO
BL3:    LODA,R0 ECX             ; the point
        STRA,R0 BX
        LODA,R0 ECY
        STRA,R0 BY
        BSTA,UN KPLOT
        LODA,R3 PTMP            ; the end?
        LODA,R0 ECX
        COMA,R0 *BE3,R3
        BCFR,EQ BL4
        LODA,R0 ECY
        COMA,R0 *BE4,R3
        RETC,EQ
BL4:    LODA,R0 ERYO            ; EPASS: e2 = 2 err (signed compares)
        ADDZ    R0
        STRA,R0 EPASS
        CPSL    PSLBASE
        LODA,R0 ERY             ; e2 > -dy: x moves
        EORI,R0 $FF
        ADDI,R0 1
        STRZ    R1
        LODA,R0 EPASS
        COMZ    R1
        BCFR,GT BL5
        LODA,R0 ERYO
        SUBA,R0 ERY
        STRA,R0 ERYO
        LODA,R0 ECX
        ADDA,R0 EX
        STRA,R0 ECX
BL5:    LODA,R0 EPASS           ; e2 < dx: y moves
        COMA,R0 ERX
        BCFR,LT BL6
        LODA,R0 ERYO
        ADDA,R0 ERX
        STRA,R0 ERYO
        LODA,R0 ECY
        ADDA,R0 EY
        STRA,R0 ECY
BL6:    PPSL    PSLBASE
        BCTA,UN BL3

; BOFF: beam PTMP's line back to air where it's fire; it's off. Calls
; five down.
BOFF:   LODA,R3 PTMP
        LODA,R0 *BE0,R3
        RETC,EQ
        EORZ    R0
        CPUTR   BEAMS
        LODI,R0 FR_FX
        STRA,R0 BFROM
        LODI,R0 PXSKY
        STRA,R0 BTO
        BCTA,UN BMLINE          ; tail call

; OBEAM: x0 16, y0 16, x1 16, y1 16, kind, frames: a beam over air and dirt
; (the second slot if the first is live; a live one there goes first).
; R2 = 10. Calls six down.
OBEAM:  LODI,R0 0
        STRA,R0 PTMP
        LODA,R0 *BE0
        BCTR,EQ OB1
        LODI,R0 BESZ
        STRA,R0 PTMP
        BSTR,UN BOFF
OB1:    LODI,R1 FRRAMP2+((BEAMS-$2900)>>8)
        LODA,R3 PTMP
        ADDI,R3 (BEAMS-$2900)&$FF
        BSTA,UN RAMSET
        LODA,R3 PPC             ; frames (0: 1)
        ADDI,R3 9
        LODA,R0 *PSCRIPT,R3
        BCFR,EQ OB2
        LODI,R0 1
OB2:    STRZ    R2
        LODA,R0 *PRDAT,R2
        LODA,R3 PPC             ; x0, y0, x1, y1: their low bytes
        LODI,R1 4
OB3:    LODA,R0 *PSCRIPT,R3
        STRZ    R2
        LODA,R0 *PRDAT,R2
        ADDI,R3 2
        BDRR,R1 OB3
        BSTA,UN RAMPEND
        LODI,R0 FR_SKY|FR_DIRT
        STRA,R0 BFROM
        LODI,R0 PXFX
        STRA,R0 BTO
        BSTA,UN BMLINE
        LODI,R2 10
        RETC,UN

; BSTEP: every live beam a frame; its last: its line back to air.
BSTEP:  LODI,R0 0
        STRA,R0 PTMP
BS1:    LODA,R3 PTMP
        LODA,R0 *BE0,R3
        BCTR,EQ BS2
        SUBI,R0 1
        BCTR,EQ BS1A
        CPUTR   BEAMS
        BCTR,UN BS2
BS1A:   BSTA,UN BOFF            ; (still 1: BOFF takes it off)
BS2:    LODA,R0 PTMP
        ADDI,R0 BESZ
        STRA,R0 PTMP
        COMI,R0 NBEAM*BESZ
        BCTR,LT BS1
        RETC,UN

; ---- a frame, a segment's end ----------------------------------------------------
; OSTEP: a frame of the paths, the flows, the beams (view.go animate's
; order).
OSTEP:  BCTA,UN PSTEP           ; (PSTEP flows to BSTEP: a level less under
                                ;  every plot)

; OSEGEND: the paths stop.
OSEGEND: LODI,R0 0
        STRA,R0 PTMP
OSE1:   BSTA,UN PEND
        LODA,R0 PTMP
        ADDI,R0 PASZ
        STRA,R0 PTMP
        COMI,R0 NPATH*PASZ
        BCTR,LT OSE1
        RETC,UN

        INCLUDE "src/blk.inc"
