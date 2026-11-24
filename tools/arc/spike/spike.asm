; spike.asm -- the Arcadia graphics study's ROM (a plain 4K cart, no
; FujiNet): it shows one mock-up frame (scene.inc, from gen_spike.py: the
; screen, the sprites, the colour registers) for MAME's snapshot to be
; compared with arcmodel.py's, then times block plotting. MARK ($18D2)
; tells the MAME script where it is: 1 the scene is up, 5 the plots start, 2 after
; 9 x 2304 single-block plots (PLOT), 3 after 960 full-row spans (SPAN), 4 done.
        CPU     2650
        INCLUDE "scene.inc"

PTR     EQU     $18D0           ; PLOT's cell (big-endian)
MARK    EQU     $18D2
CNT     EQU     $18D3
XV      EQU     $18D4
YV      EQU     $18D5
X1V     EQU     $18D6

        ORG     $0000
        BCTA,UN START
        DB      $17             ; RETC,UN: the interrupt guard

START:  LODI,R0 $20
        LPSU
        LODI,R0 $02             ; logical compares, no carry-in
        LPSL
        EORZ    R0
        STRA,R0 MARK
        STRA,R0 $18FE           ; quiet, no shift
; the scene: rows 0-12, rows 13-25, the glyphs, the registers
        LODI,R3 208
S1:     LODA,R0 SCRA-1,R3
        STRA,R0 $1800-1,R3
        BDRR,R3 S1
        LODI,R3 208
S2:     LODA,R0 SCRB-1,R3
        STRA,R0 $1A00-1,R3
        BDRR,R3 S2
        LODI,R3 32
S3:     LODA,R0 UDCS-1,R3
        STRA,R0 $1980-1,R3
        BDRR,R3 S3
        LODI,R3 8
S4:     LODA,R0 SPRS-1,R3
        STRA,R0 $18F0-1,R3
        BDRR,R3 S4
        LODI,R0 R18FC
        STRA,R0 $18FC
        LODI,R0 R18FD
        STRA,R0 $18FD
        LODI,R0 R19F8
        STRA,R0 $19F8
        LODI,R0 R19F9
        STRA,R0 $19F9
        LODI,R0 R19FA
        STRA,R0 $19FA
        LODI,R0 R19FB
        STRA,R0 $19FB
; let it show a second, then the timings
        LODI,R3 60
W1:     TPSU    $80
        BCFR,EQ W1
W2:     TPSU    $80
        BCTR,EQ W2
        BDRR,R3 W1
        LODI,R0 1
        STRA,R0 MARK
        LODI,R3 60              ; (the snapshot's second)
W3:     TPSU    $80
        BCFR,EQ W3
W4:     TPSU    $80
        BCTR,EQ W4
        BDRR,R3 W3
        LODI,R0 5
        STRA,R0 MARK
; 9 x 256 PLOTs: every block of the 48x48 field, 9 times over (R1 x, R2 y)
        LODI,R0 9
        STRA,R0 CNT
P0:     LODI,R2 47
P1:     LODI,R1 47
P2:     BSTA,UN PLOT
        BDRR,R1 P2
        BSTA,UN PLOT            ; (x 0)
        BDRR,R2 P1
        LODI,R1 47              ; (y 0)
P3:     BSTA,UN PLOT
        BDRR,R1 P3
        BSTA,UN PLOT
        LODA,R0 CNT
        SUBI,R0 1
        STRA,R0 CNT
        BCFR,EQ P0
        LODI,R0 2
        STRA,R0 MARK
; 48 spans a pass (every block row, x 0-47), 20 passes
        LODI,R0 20
        STRA,R0 CNT
Q0:     LODI,R2 47
Q1:     BSTA,UN SPAN
        BDRR,R2 Q1
        BSTA,UN SPAN
        LODA,R0 CNT
        SUBI,R0 1
        STRA,R0 CNT
        BCFR,EQ Q0
        LODI,R0 3
        STRA,R0 MARK
        LODI,R0 4
        STRA,R0 MARK
SPIN:   BCTR,UN SPIN

; PLOT: block (R1, R2) of the field (x 0-47, y 0-47) inked: its cell's bit
; set. Keeps R1, R2; clobbers R0, R3.
PLOT:   LODA,R0 CADLO,R2        ; the row's cell address + x/3
        ADDA,R0 CELLX,R1
        STRA,R0 PTR+1
        LODA,R0 CADHI,R2
        STRA,R0 PTR
        LODA,R0 HALF,R2         ; the block's bit: (y & 1) * 48 + x
        ADDZ    R1
        STRZ    R3
        LODA,R0 MASK,R3
        LODA,R3 *PTR
        IORZ    R3
        STRA,R0 *PTR
        RETC,UN

; SPAN: block row R2, x 0-47 inked (whole cells: the half's 3 bits; a real
; span's two end cells would be PLOTs). Keeps R2; clobbers R0, R1, R3.
SPAN:   LODA,R0 CADLO,R2
        STRA,R0 PTR+1
        LODA,R0 CADHI,R2
        STRA,R0 PTR
        LODA,R0 HMASK,R2        ; $07 top, $38 bottom
        STRZ    R1
        LODI,R3 16
SP1:    LODA,R0 *PTR
        IORZ    R1
        STRA,R0 *PTR
        LODA,R0 PTR+1
        ADDI,R0 1
        STRA,R0 PTR+1
        BDRR,R3 SP1
        RETC,UN

; the tables (y 0-47: rows 0-23; rows 13+ are at $1A00)
CADLO:
        IRP     r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23
        IF      r < 13
        DB      (r*16) & $FF, (r*16) & $FF
        ELSE
        DB      ((r-13)*16) & $FF, ((r-13)*16) & $FF
        ENDIF
        ENDM
CADHI:
        IRP     r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23
        IF      r < 13
        DB      $18, $18
        ELSE
        DB      $1A, $1A
        ENDIF
        ENDM
HALF:
        REPT    24
        DB      0, 48
        ENDM
HMASK:
        REPT    24
        DB      $07, $38
        ENDM
CELLX:
        IRP     c,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        DB      c, c, c
        ENDM
MASK:
        REPT    16
        DB      $04, $02, $01
        ENDM
        REPT    16
        DB      $20, $10, $08
        ENDM
