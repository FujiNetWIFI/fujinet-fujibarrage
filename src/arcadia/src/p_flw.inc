; p_flw.inc -- the shot's flows (OP_FLOW, view.go's: their cells in over
; their frames, fire over air and mud over air and fire; a fire out after
; its hold; all of it at the segment's end), a page of their own with the
; field's blocks (blk.inc). The PLY page calls them (FARPG).

        BCTA,UN FSTEP           ; E_FSTEP: every flow a frame
        BCTA,UN OFLOW           ; E_OFLOW: OP_FLOW (its arguments at PPC; R2
                                ;  the bytes it took)
        BCTA,UN FSEGEND         ; E_FSEGEND: the segment's end: the flows run out

FL0:    DWBE    FLOWS           ; on
FL1:    DWBE    FLOWS+1         ; kind
FL2:    DWBE    FLOWS+2         ; frames (1 at least)
FL3:    DWBE    FLOWS+3         ; f: frames so far
FL4:    DWBE    FLOWS+4         ; hold
FL5:    DWBE    FLOWS+5         ; cells
FL6:    DWBE    FLOWS+6         ; shown
FL7:    DWBE    FLOWS+7         ; acc (len f mod frames)
FL9:    DWBE    FLOWS+9         ; the cells' pc

; ---- flows --------------------------------------------------------------------------
; FSHOW: flow PTMP's cells up to R0 in (fire over air, mud as dirt over air
; and fire). FEND: all of them in, then a fire goes out; it's off. Both
; run FCELLS: cells EY to ERX - 1 by BFROM, BTO, then by EPASS: 0 done; 1
; (FEND's) the fire out (2) or, mud, off; 2 off. Calls three down
; (inline, for the depth: FEND is called a level deeper than FSHOW).
; Clobbers R0-R3, EY, ERX, EPASS.
FEND:   LODI,R0 1
        STRA,R0 EPASS
        LODA,R3 PTMP
        LODA,R0 *FL5,R3
        BCTR,UN FSH0
FSHOW:  STRA,R0 ERX             ; (R0: to)
        EORZ    R0
        STRA,R0 EPASS
        LODA,R0 ERX
FSH0:   STRA,R0 ERX             ; ERX: to; EY: the cell
        LODA,R3 PTMP
        LODA,R0 *FL6,R3
        STRA,R0 EY
        LODA,R0 ERX
        CPUTR   FLOWS+6         ; (shown: all of them, after)
        LODI,R0 FR_SKY
        LODI,R1 PXFX
        LODA,R3 PTMP
        LODA,R0 *FL1,R3
        STRZ    R2
        LODI,R0 FR_SKY
        COMI,R2 FLOW_MUD
        BCFR,EQ FSH1
        LODI,R0 FR_SKY|FR_FX
        LODI,R1 PXDIRT
FSH1:   STRA,R0 BFROM
        STRA,R1 BTO
FCELLS: LODA,R0 EY
        COMA,R0 ERX
        BCFA,LT FC3
        STRZ    R1              ; its bytes: pc + 3 cell
        ADDZ    R0
        ADDZ    R1
        LODA,R3 PTMP
        ADDA,R0 *FL9,R3
        STRZ    R3
        LODA,R0 *PSCRIPT,R3,+   ; x16's high byte: on the field?
        BCFR,EQ FC2
        LODA,R0 *PSCRIPT,R3,-
        STRA,R0 BX
        ADDI,R3 2
        LODA,R0 *PSCRIPT,R3
        STRA,R0 BY
        STRZ    R1
        STRZ    R2
        BSTA,UN KTANK
        BSTA,UN KPLOT
FC2:    LODA,R0 EY
        ADDI,R0 1
        STRA,R0 EY
        BCTR,UN FCELLS
FC3:    LODA,R0 EPASS           ; then
        RETC,EQ                 ; (FSHOW's: done)
        COMI,R0 1
        BCFR,EQ FC4
        LODA,R3 PTMP            ; FEND's all in: a fire goes out
        LODA,R0 *FL1,R3
        COMI,R0 FLOW_MUD
        BCTR,EQ FC4
        LODI,R0 2
        STRA,R0 EPASS
        EORZ    R0
        STRA,R0 EY
        LODA,R0 *FL5,R3
        STRA,R0 ERX
        LODI,R0 FR_FX
        STRA,R0 BFROM
        LODI,R0 PXSKY
        STRA,R0 BTO
        BCTA,UN FCELLS
FC4:    EORZ    R0              ; off
        LODA,R3 PTMP
        CPUTR   FLOWS
        RETC,UN

; OFLOW: kind, frames, hold, n, n cells (x16, y8): a flow (the first free;
; none: the last ends now and is taken). R2 = 4 + 3n.
OFLOW:  LODI,R3 0
OF1:    LODA,R0 *FL0,R3
        BCTR,EQ OF2
        ADDI,R3 FLSZ
        COMI,R3 NFLOW*FLSZ
        BCTR,LT OF1
        LODI,R0 (NFLOW-1)*FLSZ
        STRA,R0 PTMP
        BSTA,UN FEND
        LODI,R3 (NFLOW-1)*FLSZ
OF2:    STRA,R3 PTMP
        LODI,R1 FRRAMP2+((FLOWS-$2900)>>8)
        ADDI,R3 (FLOWS-$2900)&$FF
        BSTA,UN RAMSET
        LODI,R2 1               ; on
        LODA,R0 *PRDAT,R2
        LODA,R3 PPC
        LODA,R0 *PSCRIPT,R3     ; kind
        STRZ    R2
        LODA,R0 *PRDAT,R2
        LODA,R0 *PSCRIPT,R3,+   ; frames (0: 1)
        BCFR,EQ OF3
        LODI,R0 1
OF3:    STRZ    R2
        LODA,R0 *PRDAT,R2
        LODI,R2 0               ; f
        LODA,R0 *PRDAT,R2
        LODA,R0 *PSCRIPT,R3,+   ; hold
        STRZ    R2
        LODA,R0 *PRDAT,R2
        LODA,R0 *PSCRIPT,R3,+   ; n
        STRA,R0 EX
        STRZ    R2
        LODA,R0 *PRDAT,R2
        LODI,R2 0               ; shown, acc
        LODA,R0 *PRDAT,R2
        LODA,R0 *PRDAT,R2
        LODA,R0 *PRDAT,R2
        ADDI,R3 1               ; its cells' pc
        LODZ    R3
        STRZ    R2
        LODA,R0 *PRDAT,R2
        BSTA,UN RAMPEND
        LODA,R0 EX              ; took 4 + 3n
        STRZ    R2
        ADDZ    R0
        ADDZ    R2
        ADDI,R0 4
        STRZ    R2
        RETC,UN

; FSTEP: every live flow a frame (view.go): while f < frames, f + 1 and its
; cells in to n f / frames (acc: n f mod frames); then its hold; then it
; ends. Calls six down.
FSTEP:  LODI,R0 0
        STRA,R0 PTMP
FT1:    LODA,R3 PTMP
        LODA,R0 *FL0,R3
        BCTA,EQ FT9
        LODA,R0 *FL3,R3
        COMA,R0 *FL2,R3
        BCFA,LT FT4
        ADDI,R0 1               ; f + 1
        CPUTR   FLOWS+3
        LODA,R3 PTMP            ; acc += n; each frames of it: a cell more
        LODA,R0 *FL6,R3
        STRA,R0 ERY
        LODA,R0 *FL7,R3
FT2:    ADDA,R0 *FL5,R3
        TPSL    PSLC
        BCTR,EQ FT3A            ; (past 255: past frames too)
FT3:    COMA,R0 *FL2,R3
        BCTR,LT FT3B
FT3A:   SUBA,R0 *FL2,R3
        STRZ    R1
        LODA,R0 ERY
        ADDI,R0 1
        STRA,R0 ERY
        LODZ    R1
        BCTR,UN FT3
FT3B:   CPUTR   FLOWS+7
        LODA,R0 ERY
        BSTA,UN FSHOW
        BCTR,UN FT5
FT4:    LODA,R0 *FL4,R3         ; its hold
        BCTR,EQ FT5
        SUBI,R0 1
        CPUTR   FLOWS+4
FT5:    LODA,R3 PTMP            ; all in, the hold over: it ends
        LODA,R0 *FL3,R3
        COMA,R0 *FL2,R3
        BCTR,LT FT9
        LODA,R0 *FL4,R3
        BCFR,EQ FT9
        BSTA,UN FEND
FT9:    LODA,R0 PTMP
        ADDI,R0 FLSZ
        STRA,R0 PTMP
        COMI,R0 NFLOW*FLSZ
        BCTA,LT FT1
        RETC,UN

; FSEGEND: the segment's end: every flow runs out (all in, a fire out).
FSEGEND: LODI,R0 0
        STRA,R0 PTMP
FSE2:   LODA,R3 PTMP
        LODA,R0 *FL0,R3
        BCTR,EQ FSE3
        BSTA,UN FEND
FSE3:   LODA,R0 PTMP
        ADDI,R0 FLSZ
        STRA,R0 PTMP
        COMI,R0 NFLOW*FLSZ
        BCTR,LT FSE2
        RETC,UN

        INCLUDE "src/blk.inc"
