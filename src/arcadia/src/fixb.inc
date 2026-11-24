; fixb.inc -- fixed B, $2000-$28FF (2650 page 1, with the mailbox and RAM-H):
; the same in every page's assembly and always mapped.
;
; What needs the mailbox close at hand: its transactions, cart RAM's
; writes, the tick; and the tables fixed A reads through pointer words
; (sound, keys, the build's server). Page-1 code reaches console RAM
; through the pointer words at the end.

; ---- start-up ---------------------------------------------------------------------
; STBAD: no FujiNet cart, or one too old for APPBANK images: say so, stop.
STBAD:  LODI,R0 LOOKBG
        STRA,R0 *PBGCOL
        DAT     5,0
        TXTAT   SNEEDFW
        DAT     7,0
        TXTAT   SNEEDF2
STHALT: BCTR,UN STHALT
SNEEDFW: DB     "NEEDS A FUJINET",$FF
SNEEDF2: DB     "CART, PROTO 2",$FF

; CARTCLR: all 1K of cart RAM zeroed (a stream: the pointer wraps at 1K).
; Leaf. Clobbers R0-R3.
CARTCLR: LODI,R1 FRRAMP0
        LODI,R3 0
        BSTR,UN RAMSET
        LODI,R1 4
CC1:    LODI,R3 0
CC2:    LODA,R0 FNRDAT          ; a zero, stored, the pointer on
        BDRR,R3 CC2
        BDRR,R1 CC1
        BCTR,UN RAMPEND

; ---- cart RAM ---------------------------------------------------------------------
; RAMPUT: cart RAM = R0 at pointer register R1 (FRRAMP0-3) + R3 (see CPUT).
; Leaf. Clobbers R0, R2; closes a stream.
RAMPUT: STRZ    R2
        LODA,R0 FNRSEL,R1       ; the pointer
        LODA,R0 FNRDAT,R3
        LODA,R0 FNRSEL+FRRAMPK  ; one byte
        LODA,R0 FNRDAT,R2
        LODA,R0 FNRSEL+FRRAMP0  ; closed: a stray REGDATA can't store
        RETC,UN
; RAMSET: the pointer at R1 + R3, a burst open (CPTR). RAMPUT1: R0 there,
; the pointer on. RAMPEND: the burst closed. Leaves. Clobber R0 (RAMPUT1 R2).
RAMSET: LODA,R0 FNRSEL,R1
        LODA,R0 FNRDAT,R3
        LODA,R0 FNRSEL+FRRAMPK
        RETC,UN
RAMPUT1: STRZ   R2
        LODA,R0 FNRDAT,R2
        RETC,UN
RAMPEND: LODA,R0 FNRSEL+FRRAMP0
        RETC,UN

; ---- the mailbox ------------------------------------------------------------------------
; MSTARTN: an N: transaction: TX rewound, command R3, R2 parameters.
; Leaf. Clobbers R0.
MSTARTN: LODA,R0 FNRSEL+FRDATRS
        LODA,R0 FNRDAT
        LODA,R0 FNRSEL+FRDEVIC
        LODA,R0 FNRDAT+NETDEV
MST1:   LODA,R0 FNRSEL+FRCMD
        LODA,R0 FNRDAT,R3
        LODA,R0 FNRSEL+FRNPARM
        LODA,R0 FNRDAT,R2
        RETC,UN
; MSTARTF: the same on the Fuji device.
MSTARTF: LODA,R0 FNRSEL+FRDATRS
        LODA,R0 FNRDAT
        LODA,R0 FNRSEL+FRDEVIC
        LODA,R0 FNRDAT+FNDFUJI
        BCTR,UN MST1
; MPARB: a 1-byte parameter, value R1. MPARW: a 2-byte one, R1 low, R2
; high. Leaves. Clobber R0.
MPARB:  LODA,R0 FNDATA+1
        LODA,R0 FNDATA,R1
        RETC,UN
MPARW:  LODA,R0 FNDATA+2
        LODA,R0 FNDATA,R1
        LODA,R0 FNDATA,R2
        RETC,UN
; MPOST: SEQ = ACKSEQ + 1 (255 wraps to 1): the transaction goes. The
; SEQ, which ACKSEQ echoes when it is done, is kept. Leaf. Clobbers R0, R2.
MPOST:  LODA,R0 FNACKSQ
        ADDI,R0 1
        BCFR,EQ MPO1
        LODI,R0 1
MPO1:   STRZ    R2
        LODA,R0 FNRSEL+FRSEQ
        LODA,R0 FNRDAT,R2
        LODZ    R2
        STRA,R0 *PNSEQ
        RETC,UN
; MDONE: CC EQ when the posted transaction has answered. Leaf. Clobbers R0.
MDONE:  LODA,R0 FNACKSQ
        COMA,R0 *PNSEQ
        RETC,UN
; MACK: CC EQ when it worked and was ACKed. Leaf. Clobbers R0.
MACK:   LODA,R0 FNERR
        RETC,GT                 ; (not EQ)
        RETC,LT
        LODA,R0 FNREPLY
        COMI,R0 FCACK
        RETC,UN
; MIDLE: the link's transaction, if one is out, answered first (a new one
; is never posted over it): up to 4 s of frames. Calls two down. Clobbers
; R0-R3.
MIDLE:  LODA,R0 *PNKIND
        RETC,EQ
        LODI,R0 240
        STRA,R0 *PMGOL
MID1:   BSTR,UN MDONE
        BCTR,EQ MID2
        BSTA,UN VBSYNC
        BSTA,UN TICKRD          ; (DT: the sound's jiffies)
        LODA,R0 *PMGOL
        SUBI,R0 1
        STRA,R0 *PMGOL
        BCFR,EQ MID1
MID2:   EORZ    R0
        STRA,R0 *PNKIND
        RETC,UN
; MGO: post, then wait for the answer a frame at a time (the sound
; running), about 21 s at most (an OPEN may dial for seconds; the same SEQ
; is polled all along). CC EQ when it was ACKed. Calls two down.
; Clobbers R0-R3.
MGO:    BSTA,UN MPOST
        LODI,R0 5
        STRA,R0 *PMGOH
MGO1:   EORZ    R0
        STRA,R0 *PMGOL
MGO2:   BSTA,UN MDONE
        BCTR,EQ MGO4
        BSTA,UN VBSYNC
        BSTA,UN TICKRD          ; (DT: the sound's jiffies)
        LODA,R0 *PMGOL
        SUBI,R0 1
        STRA,R0 *PMGOL
        BCFR,EQ MGO2
        LODA,R0 *PMGOH
        SUBI,R0 1
        STRA,R0 *PMGOH
        BCFR,EQ MGO1
        LODI,R0 1               ; never came: (not EQ)
        RETC,UN
MGO4:   BCTA,UN MACK            ; tail call

; ---- the clock ----------------------------------------------------------------
; TICKRD: DT = the cart's ticks since the last loop (1-8), added to our
; frame clock. The tick and the vblank are not locked together, so a frame
; can see 0 or 2: 0 counts as 1. Leaf. Clobbers R0-R2.
TICKRD: LODA,R0 FNTICKH         ; HI, LO, HI: the bytes change together
        STRZ    R1
        LODA,R2 FNTICKL
        LODA,R0 FNTICKH
        COMZ    R1
        BCFR,EQ TICKRD
        LODZ    R2
        SUBA,R0 *PTICKL
        STRA,R1 *PTICKH
        STRA,R2 *PTICKL
        BCFR,EQ TKR1
        LODI,R0 1
TKR1:   COMI,R0 8
        BCTR,LT TKR2
        LODI,R0 8
TKR2:   STRA,R0 *PDT
        ADDA,R0 *PFRML          ; (C: the carry)
        STRA,R0 *PFRML
        PPSL    PSLWC
        LODA,R0 *PFRMH
        ADDI,R0 0
        STRA,R0 *PFRMH
        CPSL    PSLWC
        RETC,UN

; ---- the field's cells (every page's: the decoder's, p_play.inc's, M3's) ----------
; CELLAD: block (R1 x 0-47, R2 row 0-47)'s cell: R2 its half (0 SCR, 1
; SCR2), R3 its index there, R1 the block's bit in it (top row b2-b0, the
; bottom's b5-b3, left to right). Leaf. Clobbers R0.
CELLAD: LODA,R0 CADIV3,R1       ; the cell column (x / 3), the third's bit
        STRZ    R3
        LODA,R0 CABITX,R1
        STRZ    R1
        LODZ    R2              ; the bottom row: its bit 3 higher
        ANDI,R0 1
        BCTR,EQ CA3
        LODZ    R1
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R1
CA3:    LODZ    R2              ; the cell row
        ANDI,R0 $FE
        RRR,R0
        LODI,R2 0
        COMI,R0 13
        BCTR,LT CA4
        SUBI,R0 13
        LODI,R2 1
CA4:    RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        ADDZ    R3
        STRZ    R3
        RETC,UN
CADIV3: DB      0,0,0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,6,7,7,7
        DB      8,8,8,9,9,9,10,10,10,11,11,11,12,12,12,13,13,13,14,14,14,15,15,15
CABITX: DB      4,2,1,4,2,1,4,2,1,4,2,1,4,2,1,4,2,1,4,2,1,4,2,1
        DB      4,2,1,4,2,1,4,2,1,4,2,1,4,2,1,4,2,1,4,2,1,4,2,1

; TKOFF: the block tanks off the field: each one's cells as they were
; (TBC), the last drawn first (two may share a cell); TBN 0. The decoder's
; before it draws terrain, p_play.inc's before the tanks move. Calls one
; down. Clobbers R0-R3, T0-T3.
TKOFF:  LODA,R0 *PTBN
        RETC,EQ
        RRL,R0                  ; T0: TBC's bytes in use, T1: the seat
        STRA,R0 *PWT0
        LODI,R0 SEATS
        STRA,R0 *PWT1
TO1:    LODA,R0 *PWT1
        BCTA,EQ TO9
        SUBI,R0 1
        STRA,R0 *PWT1
        STRZ    R3
        LODA,R0 *PTKY,R3        ; a block tank? (y's b7)
        TMI,R0  $80
        BCFA,EQ TO1
        ANDI,R0 $7F             ; its blocks' row: y - 1 (T3), x (T2)
        SUBI,R0 1
        STRA,R0 *PMGOL
        STRZ    R2
        LODA,R0 *PTKX,R3
        STRA,R0 *PMGOH
        STRZ    R1
        BSTA,UN CELLAD          ; the right block's cell: TBC's odd byte
        LODA,R1 *PWT0
        SUBI,R1 1
        STRA,R1 *PWT0
        LODA,R0 *PTBC,R1
        BSTA,UN TOPUT
        LODA,R1 *PWT0            ; the left's (x - 1, none at x 0): the even
        SUBI,R1 1
        STRA,R1 *PWT0
        LODA,R1 *PMGOH
        BCTA,EQ TO1
        SUBI,R1 1
        LODA,R2 *PMGOL
        BSTA,UN CELLAD
        LODA,R1 *PWT0
        LODA,R0 *PTBC,R1
        BSTA,UN TOPUT
        BCTA,UN TO1
TO9:    EORZ    R0
        CPUT    TBN
        RETC,UN
TOPUT:  COMI,R2 0               ; R0 into cell R3 of half R2
        BCFR,EQ TOP1
        STRA,R0 *PWSCR,R3
        RETC,UN
TOP1:   STRA,R0 *PWSCR2,R3
        RETC,UN

; TSHOT: test scene 3 (tools/arc/refcheck.py): the field, the fixture's
; terrain and tanks (p_test.inc), each of its segments played (the PLY
; page, as in a room), then frames for ever (the rig reads the field).
; Fixed B's (fixed A is full; fixed code may switch pages).
TSHOT:  LODA,R0 *PTESTA          ; (TZ: before the field takes it)
        CPUT    TLOCK
        GOPAGE  PG_PLAY,E_ENTER
        GOPAGE  PG_TEST,E_TFIX
TSH1:   GOPAGE  PG_TEST,E_TSEG
        COMI,R0 0
        BCTR,EQ TSH2
        GOPAGE  PG_PLY,E_ENTER
        BCTR,UN TSH1
TSH2:   GOPAGE  PG_TEST,E_TEND
TSH3:   BSTA,UN FRAME
        GOPAGE  PG_PLAY,E_VBL
        BCTR,UN TSH3

PTESTA: DWBE    TESTHK+3
PTESTS: DWBE    TESTHK+2

; TSCRN: test scenes 4 on (M4's screens; tools/arcsim/scenes/t*.txt): the
; scene's state (p_tscr.inc E_TSET: ST, our seat; R0 the screen, S_*;
; the field's scenes their terrain and tanks too), the screen entered as a
; room enters it, then frames for ever: its E_TICK (and the field's
; sprites), the scene's own frame (a ticking timer: R0 1 = a new state for
; the screen's E_STATE). Offline (FL_TEST): actions go nowhere.
TSCRN:  LODA,R0 *PTESTS         ; (11 on: the sound's, p_snd.inc)
        COMI,R0 11
        BCTR,LT TSCR0
        GOPAGE  PG_SND,E_TSND
TSCR0:  GOPAGE  PG_TSCR,E_TSET
        COMI,R0 S_FIELD
        BCTR,EQ TSCF
        BSTA,UN ENTERS
        BCTR,UN TSC1
TSCF:   CPUT    SHOWN           ; the field: entered, the fixture's terrain
        GOPAGE  PG_PLAY,E_ENTER ;  and tanks (scene 3's), the scene's seats,
        GOPAGE  PG_TEST,E_TFIX  ;  then its state; scene 8 plays a segment
        GOPAGE  PG_TSCR,E_TFLD
        GOPAGE  PG_PLAY,E_STATE
        LODA,R0 TMARK
        BCTR,EQ TSC1
        GOPAGE  PG_PLY,E_ENTER
TSC1:   BSTA,UN FRAME
        LODA,R0 *PSHOWN
        COMI,R0 S_FIELD
        BCFR,EQ TSC2
        GOPAGE  PG_PLAY,E_VBL
TSC2:   GOPAGE  PG_TSCR,E_TTICK
        COMI,R0 0
        BCTR,EQ TSC3
        LODI,R3 E_STATE
        BSTR,UN TSCPG
TSC3:   LODI,R3 E_TICK
        BSTR,UN TSCPG
        BCTR,UN TSC1
TSCPG:  LODA,R2 *PSHOWN         ; entry R3 of the screen's page
        LODA,R0 *PSCRPG,R2
        STRZ    R1
        BCTA,UN PGCALL
PSHOWN: DWBE    SHOWN
PSCRPG: DWBE    SCRPG

; ---- the text screens' user glyphs (fixed A's TEXTSCR: UDC 0-7) ---------------------
TXTGLY: DB      $00,$32,$32,$04,$08,$10,$26,$26 ; %
        DB      $00,$04,$0C,$1C,$3C,$1C,$0C,$04 ; < (the cursor's mirror)
        DB      $00,$08,$08,$08,$08,$08,$00,$08 ; !
        DB      $00,$1C,$22,$02,$04,$08,$00,$08 ; ?
        DB      $00,$20,$30,$38,$3C,$38,$30,$20 ; > the cursor
        DB      $00,$00,$00,$00,$3E,$00,$00,$00 ; -
        DB      $00,$00,$18,$18,$00,$18,$18,$00 ; :
        DB      $00,$02,$04,$08,$10,$20,$40,$00 ; /

; ---- the items' short names --------------------------------------------------------------
        INCLUDE "src/names.inc"

; ---- tables fixed A reads ---------------------------------------------------------
        INCLUDE "sndtab.inc"    ; SNDTH, SNDTL, SNDPRT (tools/arc/mksnd_arc.py)

; the lowest set bit wins, so two keys at once never read as a third
KT0:    DB      K_NONE,K_CLR,7,K_CLR,4,K_CLR,7,K_CLR
        DB      1,K_CLR,7,K_CLR,4,K_CLR,7,K_CLR
KT1:    DB      K_NONE,0,8,0,5,0,8,0
KT2:    DB      K_NONE,K_ENT,9,K_ENT,6,K_ENT,9,K_ENT
        DB      3,K_ENT,9,K_ENT,6,K_ENT,9,K_ENT
KTP:    DB      K_NONE,K_START,K_OPT,K_START,K_SEL,K_START,K_OPT,K_START

        ASCII
        INCLUDE "config.inc"    ; DEFSPEC, DEFURL: the build's server (build.sh)
        GLYPHS

; ---- pointer words: page-1 code's way into page 0 ----------------------------------------
PNSEQ:  DWBE    NSEQ
PNKIND: DWBE    NKIND
PMGOH:  DWBE    T2
PMGOL:  DWBE    T3
PTICKH: DWBE    TICKH
PTICKL: DWBE    TICKL
PFRMH:  DWBE    FRMH
PFRML:  DWBE    FRML
PDT:    DWBE    DT
PBGCOL: DWBE    BGCOL
PWT0:   DWBE    T0
PWT1:   DWBE    T1
PWSCR:  DWBE    SCR
PWSCR2: DWBE    SCR2
PTKX:   DWBE    TKX
PTKY:   DWBE    TKY
PTBN:   DWBE    TBN
PTBC:   DWBE    TBC
