; p_test.inc -- the offline test scenes (tools/arc/refcheck.py; the
; Astrocade's p_test.inc scene 3): the fixture's field and tanks (refview
; -fixture-asm at the Arcadia's caps: src/arcadia/test/fixture.inc), then
; each of its segments into SCRIPT for the PLY page to play (main.inc
; TSHOT). TMARK says when the shot starts (98) and ends (99): the rig
; counts the frames between (the player's lag against the script's).

        BCTA,UN TFIX            ; E_TFIX: the field, the tanks, the shot's start
        BCTA,UN TSEG            ; E_TSEG: the next segment (R0 = 0: none left)
        BCTA,UN TEND            ; E_TEND: the shot's end

        INCLUDE "test/fixture.inc"

; TFIX: offline (FL_TEST: the link quiet); the terrain's column entries
; drawn as the decoder draws them (p_fld.inc E_COL from x 0); the seats:
; each fixture tank alive at its place (the shooter's turn, ours seat 0:
; mock_arc.py's OWN), the tanks drawn (E_TANKS); the shot's segment 0 next.
TFIX:   LODA,R0 FLAGS
        IORI,R0 FL_TEST
        CPUT    FLAGS
        LODI,R0 4               ; the decoder's column state at x 0
        STRA,R0 GMT
        EORZ    R0
        STRA,R0 GX
        STRA,R0 GST
        STRA,R0 GPN
        LODI,R0 FIXTERR>>8      ; DSTRH:DSTRL the entry byte, DLEFT:DCODE left
        STRA,R0 DSTRH
        LODI,R0 FIXTERR&$FF
        STRA,R0 DSTRL
        LODI,R0 FIXNTER&$FF
        STRA,R0 DLEFT
        LODI,R0 FIXNTER>>8
        STRA,R0 DCODE
TF1:    LODA,R0 DLEFT
        IORA,R0 DCODE
        BCTR,EQ TF3
        LODA,R0 *DSTRH
        STRA,R0 DOUT
        FARPG   PG_FLD,E_COL
        LODA,R0 DSTRL
        ADDI,R0 1
        STRA,R0 DSTRL
        BCFR,EQ TF2
        LODA,R0 DSTRH
        ADDI,R0 1
        STRA,R0 DSTRH
TF2:    LODA,R0 DLEFT
        SUBI,R0 1
        STRA,R0 DLEFT
        COMI,R0 $FF
        BCFR,EQ TF1
        LODA,R0 DCODE
        SUBI,R0 1
        STRA,R0 DCODE
        BCTR,UN TF1
TF3:    FARPG   PG_FLD,E_COLEND
        EORZ    R0
        STRA,R0 DLEFT
        STRA,R0 DCODE
        LODI,R0 0               ; the seats (EY): kind, alive, x, y, angle
        STRA,R0 EY
TF4:    LODI,R0 P_KIND
        LODI,R1 KIND_BOT
        BSTA,UN TSF
        LODI,R0 P_FLAGS
        LODI,R1 PF_ALIVE
        BSTA,UN TSF
        LODA,R3 EY
        LODA,R0 FIXTX,R3
        STRZ    R1
        LODI,R0 P_X
        BSTA,UN TSF
        LODI,R0 P_X+1
        LODI,R1 0
        BSTA,UN TSF
        LODA,R3 EY
        LODA,R0 FIXTY,R3
        STRZ    R1
        LODI,R0 P_Y
        BSTA,UN TSF
        LODA,R3 EY
        LODA,R0 FIXTA,R3
        STRZ    R1
        LODI,R0 P_ANGLE
        BSTA,UN TSF
        LODA,R0 EY
        ADDI,R0 1
        STRA,R0 EY
        COMI,R0 SEATS
        BCFA,EQ TF4
        LODI,R0 PH_SHOT         ; the phase, the turn: the shooter's
        CPUT    ST+OFF_PHASE
        LODI,R0 FIXSHOOT
        CPUT    ST+OFF_TURN
        EORZ    R0              ; ours: seat 0
        CPUT    MYSEAT
        FARPG   PG_FLD,E_TANKS
        EORZ    R0              ; segment 0 next (SEGSID: the count)
        CPUT    SEGSID
        LODI,R0 98
        CPUT    TMARK
        RETC,UN
; TSF: seat EY's byte R0 (P_*) = R1. Calls one down. Clobbers R0-R3, PTMP.
TSF:    STRA,R1 PTMP
        LODA,R3 EY
        ADDA,R0 SEAT25,R3
        ADDI,R0 OFF_SEATS       ; (ST is cart offset 0; past 256: the
        STRZ    R3              ;  pointer register's next)
        LODI,R1 FRRAMP0
        TPSL    PSLC
        BCFR,EQ TSF1
        LODI,R1 FRRAMP1
TSF1:   LODA,R0 PTMP
        BCTA,UN RAMPUT          ; tail call
        IF      ST <> $0600
        ERROR   "TSF: ST must be cart offset 0"
        ENDIF

; TSEG: the fixture's segment SEGSID into SCRIPT (SEGLEN; SEGNO, SEGFLG:
; S_LAST_SEG on its last); R0 = 1. None left: R0 = 0. Calls one down.
TSEG:   LODA,R0 SEGSID
        COMI,R0 FIXNSEG
        BCTR,LT TS1
        EORZ    R0
        RETC,UN
TS1:    LODI,R0 FIXSEG>>8       ; DSTRH:DSTRL: its first byte (the lengths
        STRA,R0 DSTRH           ;  before it added)
        LODI,R0 FIXSEG&$FF
        STRA,R0 DSTRL
        LODI,R3 0
TS2:    COMA,R3 SEGSID
        BCTR,EQ TS3
        LODZ    R3              ; (FIXSLEN: DW, low byte first)
        ADDZ    R0
        STRZ    R2
        LODA,R0 FIXSLEN,R2
        ADDA,R0 DSTRL
        STRA,R0 DSTRL
        TPSL    PSLC
        BCFR,EQ TS2A
        LODA,R0 DSTRH
        ADDI,R0 1
        STRA,R0 DSTRH
TS2A:   ADDI,R3 1
        BCTR,UN TS2
TS3:    LODZ    R3              ; its length
        ADDZ    R0
        STRZ    R2
        LODA,R0 FIXSLEN,R2
        STRA,R0 SEGLEN
        CPTR    SCRIPT          ; its bytes
        LODI,R3 0
TS4:    COMA,R3 SEGLEN
        BCTR,EQ TS5
        LODA,R0 *DSTRH,R3
        STRZ    R2
        LODA,R0 *PRDAT,R2
        ADDI,R3 1
        BCTR,UN TS4
TS5:    BSTA,UN RAMPEND
        LODA,R0 SEGSID          ; SEGNO, SEGFLG, the next
        CPUT    SEGNO
        LODI,R0 0
        LODA,R1 SEGSID
        ADDI,R1 1
        COMI,R1 FIXNSEG
        BCFR,EQ TS6
        LODI,R0 S_LAST_SEG
TS6:    CPUT    SEGFLG
        LODA,R0 SEGSID
        ADDI,R0 1
        CPUT    SEGSID
        LODI,R0 1
        RETC,UN

; TEND: the shot over: TMARK 99.
TEND:   LODI,R0 99
        CPUT    TMARK
        RETC,UN
