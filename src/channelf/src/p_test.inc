; ---------------------------------------------------------------------------
; p_test.inc -- page TEST: the rigs' offline scenes (src/test.c's numbers
; where they mean the same). TV (ram.inc) is a mark the rigs time: each
; STB TV is a write tools/chfsim's "marks" prints with its clock. The scene
; comes from the test hook (p_boot: TESTHK+2).
;
;   1  the field: the built-in fixture's TERRAIN through FCOLENT (M0 spike)
;   2  1, then a 35-column settle (the spike's blasts and tracks, SPBLS and
;      SPTRK, were 3 and 4 before those went to the shot and the screens:
;      unreached now)
;   3  a shot from FIXBUF (the rig loads tools/chf/mkfix_chf.py's packing of
;      refview's fixture, then FIXRDY): the field, the tanks, each segment
;      through the player as in a room (TV 98 at its start, 99 at its end)
;   4  the screens over a canned ST (the rig loads it, CATBUF, CATN, then
;      FIXRDY): the room's loop with the link quiet, TESTEV a commit
;   6  the sound board: every sound in turn (TV its id)
;   8  the settle sweep, for a real console: a band of stripes drawn at each
;      wait after the strobe (about 56-176 clocks), labelled: a band whose
;      wait is too short comes out wrong (MAME draws them all)
;   9  text (the font, a glyph-diff rewrite)
; ---------------------------------------------------------------------------

        INCLUDE "spike.inc"
        INCLUDE "srcdir/test/fixture.inc"

TRKS    EQU SCRIPT              ; scene 4's tracks: 16 x 16 (LE: X3 Y3 VX3 VY3 AX2 AY2)
TPOS    EQU SCRIPT+256          ;  and where each head is drawn: x, y (FF none)

TENTER: ENTER
        LR A,0
        STB TV+1                ; the scene
        CI 4                    ; 4: a canned state (the rig loads ST,
        BNZ TEN00               ;  CATBUF, CATN), then the room's loop
TEN4W:  PI FRAME                ;  (main.inc: the link quiet, TESTEV a
        LDB FIXRDY              ;  commit)
        NI 0FFH
        BZ TEN4W
        LDB ST+OFF_MY_SEAT
        STB MYSEAT
        LEAVE
TEN00:  CI 8                    ; 8: the settle sweep (never comes back)
        BNZ TEN02
        JMP TSWEEP
TEN02:  CI 6                    ; 6: the sound board
        BNZ TEN01
        PI SBOARD
        LEAVE
TEN01:  CI 3
        BNZ TEN0
        PI SFIX
        LI 99                   ; the shot's end
        STB TV
        LEAVE
TEN0:   PI SPFLD
        LDB TV+1
        CI 2
        BNZ TEN3
        PI SPSET
        BR TENX
TEN3:   CI 3
        BNZ TEN4
        PI SPBLS
        BR TENX
TEN4:   CI 4
        BNZ TEN9
        PI SPTRK
        BR TENX
TEN9:   CI 9
        BNZ TENX
        PI SPTXT
TENX:   LI 0FFH                 ; the scene's end
        STB TV
        LEAVE

; SPFLD -- the field's rows light blue, the HUD's black; the fixture's
;   TERRAIN column by column; marks 1, 2.
SPFLD:  ENTER
        LIS FY0
        LR 0,A
        LI FY0+FH-1
        LR 1,A
        LIS 1
        LR 2,A
        PI PALSET
        LIS VM_FIELD            ; (all sky on a screen all 0: nothing to show)
        STB VMODE
        LIS 1
        STB TV
        DCI FIXTERR
        XDC
        CLR
        LR 5,A
SPF1:   PI FCOLENT
        LR A,5
        INC
        LR 5,A
        CI FW-1
        BC SPF1
        LIS 2
        STB TV
        LEAVE

; SPSET -- columns 30-64 dirt from row 35 down (a settle's worth); marks 3, 4.
SPSET:  ENTER
        LIS 3
        STB TV
        LI 30
        LR 5,A
SPS1:   DCI SPSENT
        XDC
        PI FCOLENT
        LR A,5
        INC
        LR 5,A
        CI 64
        BC SPS1
        LIS 4
        STB TV
        LEAVE
SPSENT: DB 35

; SPBLS -- the fireball's rings, a frame each, then the crater; marks 5-7.
SPBLS:  ENTER
        LIS 5
        STB TV
        DCI SPELL
SPB1:   LR Q,DC
        LR A,QU
        STB TV+2
        LR A,QL
        STB TV+3
        PI FRAME
        DCI TV+2
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
SPB2:   LM                      ; x (FE: the ring's end, FF: all)
        CI 0FDH
        BNC SPB3
        LR 5,A
        LM
        LR 6,A
        LM
        LR 7,A
        LR Q,DC
        LI PX_FX
        LR 1,A
        PI FCOLW
        LR DC,Q
        BR SPB2
SPB3:   CI 0FEH
        BZ SPB1
        LIS 6
        STB TV
        DCI SPCRAT
SPB4:   LM
        CI 0FEH
        BNC SPB5
        LR 5,A
        LM
        LR 6,A
        LM
        LR 7,A
        LR Q,DC
        LI PX_SKY
        LR 1,A
        PI FCOLW
        LR DC,Q
        BR SPB4
SPB5:   LIS 7
        STB TV
        LEAVE

; SPTRK -- 16 tracks (SPTRKD, big-endian) into TRKS (little-endian), then 60
;   steps of all 16, no waiting: marks 8, 9.
SPTRK:  ENTER
        DCI SPTRKD              ; the canned tracks
        XDC
        DCI TRKS
        LI 16
        LR 8,A
SPT1:   LIS 4                   ; X, Y, VX, VY: 3 bytes each, reversed
        LR 7,A
SPT2:   XDC
        LM
        LR 2,A
        LM
        LR 1,A
        LM
        LR 0,A
        XDC
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        DS 7
        BNZ SPT2
        XDC                     ; AX, AY: 2 bytes each, reversed
        LM
        LR 4,A
        LM
        LR 3,A
        LM
        LR 6,A
        LM
        LR 5,A
        XDC
        LR A,3
        ST
        LR A,4
        ST
        LR A,5
        ST
        LR A,6
        ST
        DS 8
        BNZ SPT1
        DCI TPOS                ; nothing drawn
        LI 32
        LR 0,A
SPT4:   LI 0FFH
        ST
        DS 0
        BNZ SPT4
        LIS 8
        STB TV
        LI 60
        STB TV+4                ; steps left
SPT5:   CLR
        STB TV+5                ; the track
SPT6:   PI TSTEP
        LDB TV+5
        INC
        STB TV+5
        CI 15
        BC SPT6
        DECB TV+4
        BNZ SPT5
        LIS 9
        STB TV
        LEAVE

; TSTEP -- track TV+5 one step: X += VX, Y += VY (24-bit), VX += AX, VY +=
;   AY (sign-extended); its head (X >> 12, Y >> 12) moved: the old pixel
;   back to sky, the new one FX if it's sky.
TSTEP:  ENTER
        LDB TV+5
        SL 4
        DCI TRKS
        ADCU
        LR Q,DC
        LM                      ; X
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A
        LM                      ; Y
        LR 3,A
        LM
        LR 4,A
        LM
        LR 5,A
        LM                      ; + VX
        AS 0
        LR 0,A
        ADCM 1
        LR 1,A
        ADCM 2
        LR 2,A
        LM                      ; + VY
        AS 3
        LR 3,A
        ADCM 4
        LR 4,A
        ADCM 5
        LR 5,A
        LR DC,Q
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        LR A,4
        ST
        LR A,5
        ST
        LR A,2                  ; the head: x = X >> 12, y = Y >> 12
        SL 4
        LR 6,A
        LR A,1
        SR 4
        AS 6
        LR 6,A
        LR A,5
        SL 4
        LR 7,A
        LR A,4
        SR 4
        AS 7
        LR 7,A
        LR A,2                  ; (off the field: X2 or Y2 past it)
        CI 6
        BC TS0
        LI 0FFH
        LR 6,A
TS0:    LR A,5
        CI 2
        BC TS0A
        LI 0FFH
        LR 6,A
TS0A:   LR A,6
        STB TV+6
        LR A,7
        STB TV+7
        LR DC,Q                 ; VX, VY, AX, AY
        LI 6
        ADC
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A
        LM
        LR 3,A
        LM
        LR 4,A
        LM
        LR 5,A
        LM
        LR 6,A                  ; AX lo
        LM
        LR 7,A                  ; AX hi
        LM
        LR 8,A                  ; AY lo
        LM
        LR 10,A                 ; AY hi
        LR A,7                  ; r11 = AX's sign: 0 or FF
        AS 7
        CLR
        LNK
        COM
        INC
        LR 11,A
        LR A,0                  ; VX += AX
        AS 6
        LR 0,A
        ADC3 1,7
        LR 1,A
        ADC3 2,11
        LR 2,A
        LR A,10                 ; r11 = AY's sign
        AS 10
        CLR
        LNK
        COM
        INC
        LR 11,A
        LR A,3                  ; VY += AY
        AS 8
        LR 3,A
        ADC3 4,10
        LR 4,A
        ADC3 5,11
        LR 5,A
        LR DC,Q
        LI 6
        ADC
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        LR A,4
        ST
        LR A,5
        ST
        LDB TV+5                ; where its head is drawn
        SL 1
        DCI TPOS
        ADC
        LR Q,DC
        LM
        LR 5,A
        LM
        LR 6,A
        LDB TV+6                ; moved?
        XS 5
        BNZ TS1
        LDB TV+7
        XS 6
        BNZ TS1
        LEAVE
TS1:    LR A,5                  ; the old head back to sky
        CI 0FFH
        BZ TS2
        LI PX_SKY
        LR 1,A
        PI FSET
TS2:    LR DC,Q                 ; nothing drawn now
        LI 0FFH
        ST
        ST
        LDB TV+6                ; the new head, over sky
        LR 5,A
        CI FW-1
        BNC TSX
        LDB TV+7
        LR 6,A
        CI FH-1
        BNC TSX
        PI FGET
        CI PX_SKY
        BNZ TSX
        LI PX_FX
        LR 1,A
        PI FSET
        LR DC,Q
        LR A,5
        ST
        LR A,6
        ST
TSX:    LEAVE

; SPTXT -- scene 9: text (a white line on black, a red one on light blue).
SPTXT:  ENTER
        CLR
        LR 0,A
        PI DCLS
        DCI SPT_A
        LIS 1
        LR 1,A
        LI CV1
        LR 2,A
        PI TXCENT
        LI TY0+18
        LR 0,A
        LI TY0+23
        LR 1,A
        LIS 1
        LR 2,A
        PI PALSET
        DCI SPT_B
        LIS 3
        LR 1,A
        LI CV2
        LR 2,A
        PI TXCENT
        LEAVE
SPT_A:  DB "HELLO CHANNEL F",0
SPT_B:  DB "0123456789 ABC",0

; ---------------------------------------------------------------------------
; SFIX -- scene 3: the fixture's shot (tools/chf/refcheck.py: refview's, as
;   tools/chf/mkfix_chf.py packs it into FIXBUF, then FIXRDY): its field and
;   tanks shown, each segment played in turn as in a room; TV 98 at its
;   start, 99 at its end.
; ---------------------------------------------------------------------------
SFIX:   ENTER
SFX0:   PI FRAME                ; the rig loads it
        LDB FIXRDY
        NI 0FFH
        BZ SFX0
        LDB FIXBUF              ; the sky; ours seat 0 (the reference's)
        STB ST+OFF_SKY
        CLR
        STB MYSEAT
        STB MYTURN
        STB VMODE
        PI FCLEAR               ; the field (FLD only), then the screen
        DCI FIXBUF+35
        XDC
        CLR
        LR 5,A
SFX1:   PI FCOLENT
        LR A,5
        INC
        LR 5,A
        CI FW-1
        BC SFX1
        CLR                     ; the tanks: all ten
        STB TV+2
SFX2:   LDB TV+2
        LR 4,A
        DCI FIXBUF+3
        ADC
        LM
        LR 1,A
        LR A,4
        DCI FIXBUF+13
        ADC
        LM
        LR 2,A
        LR A,4
        DCI FIXBUF+23
        ADC
        LM
        LR 3,A
        LR A,4
        DCI TKX
        ADC
        LR A,1
        ST
        LR A,4
        DCI TKY
        ADC
        LR A,2
        ST
        LR A,4
        DCI TKA
        ADC
        LR A,3
        ST
        LR A,4
        DCI TKON
        ADC
        LIS 1
        ST
        LR A,4
        INC
        STB TV+2
        CI SEATS
        BNZ SFX2
        PI TKLOST
        CLR
        LR 0,A
        LI TROWS-1
        LR 1,A
        PI TXCLR
        PI SKYSET
        PI FSHOW
        PI TKALL
        LI 98
        STB TV
        DCI FIXBUF+33           ; the segments: past the terrain
        LM
        LR 0,A
        LM
        LR 1,A
        DCI FIXBUF+35
        LR Q,DC
        LR A,QL
        AS 0
        LR QL,A
        LR A,QU
        LNK
        AS 1
        LR QU,A
        LR A,QU
        STB TV+4                ; (the next segment's place)
        LR A,QL
        STB TV+5
        CLR
        STB TV+3                ; the segment
SFX3:   LDB TV+3
        LR 0,A
        LDB FIXBUF+1
        XS 0
        BNZ SFX4
        LEAVE                   ; (TENTER marks the end)
SFX4:   DCI TV+4                ; its length, its bytes into SCRIPT
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LM
        LR 2,A
        LM
        LR 3,A
        LR Q,DC
        DCI SEGLEN
        LR A,2
        ST
        LR A,3
        ST
        LR DC,Q
        XDC
        DCI SCRIPT
        XDC
        LR A,3                  ; (r3:r2 bytes: whole pages and the rest)
        INC
        LR 3,A
        LR A,2
        NI 0FFH
        BNZ SFX5
        DS 3
        BZ SFX6
SFX5:   LM                      ; DC0 the fixture, DC1 SCRIPT
        XDC
        ST
        XDC
        DS 2
        BNZ SFX5
        DS 3
        BNZ SFX5
SFX6:   LR Q,DC                 ; DC0: past it
        LR A,QU
        STB TV+4
        LR A,QL
        STB TV+5
        LDB TV+3                ; sid 1, seg, its last frame (and the shot's)
        STB SEGNO
        LIS 1
        STB SEGSID
        LDB TV+3
        INC
        LR 0,A
        LDB FIXBUF+1
        XS 0
        LIS S_LAST_FRAME
        BNZ SFX7
        LIS S_LAST_FRAME+S_LAST_SEG
SFX7:   STB SEGFLG
        PI PLAYSEG
        LDB TV+3
        INC
        STB TV+3
        JMP SFX3

; SBOARD -- scene 6, the sound board: every sound in turn (TV = its id at
;   its start, for the rig's marks; 255 at the end), each to its end (the
;   title's loop: 8 s) and half a second of silence after it.
SBOARD: ENTER
        LI 30                   ; (half a second first: the rig's marks set)
        STB TV+3
SBD0:   PI FRAME
        DECB TV+3
        BNZ SBD0
        CLR
        STB TV+2
SBD1:   LDB TV+2
        STB TV                  ; (its mark)
        LR 0,A
        PI SNDPLAY
        CLR
        STB TV+3                ; jiffies played (to 8 s: 480 = 2 x 240)
        STB TV+4
SBD2:   PI FRAME
        LDB SNDID
        CI 0FFH
        BZ SBD3
        LDB DT
        LR 1,A
        LDB TV+3
        AS 1
        STB TV+3
        CI 239
        BC SBD2
        CLR
        STB TV+3
        INCB TV+4
        CI 2
        BNZ SBD2
        PI SNDINIT              ; (the title's loop: stopped)
SBD3:   LI 30
        STB TV+3
SBD4:   PI FRAME
        DECB TV+3
        BNZ SBD4
        INCB TV+2
        CI NSND
        BNZ SBD1
        LI 0FFH
        STB TV
        LEAVE

; ---------------------------------------------------------------------------
; TSWEEP -- scene 8: 7 bands of stripes (blue and red by turns, columns
;   20-99, 4 rows at text row n + 1 on light grey), each band's pixels with
;   an exact wait from the strobe to the next port write (the assembler's
;   padding: 70, 80, 90, 94 (M.U.L.E.'s), 106 (PAL's 2 MHz), 120, 136 (the
;   BIOS's) clocks), labelled. On a console a band whose wait is too short
;   comes out wrong; MAME draws them all. (The settle guard fails it: it is
;   meant to.) After FujiBlast's.
; ---------------------------------------------------------------------------
; SWBAND blab, bds, bnops -- a band: r4 its first row, 4 rows, 80 pixels
;   each; the pad after the strobe is bds (0/1: a DS, 6 clocks) + bnops x 4, so the
;   wait is 56 + it (the rest of the strobe's OUTS 4, the pad, the loop's
;   44, the next LR and OUTS 1's 4 + 4).
SWBAND  MACRO blab, bds, bnops
        LIS 4
        LR 9,A                  ; rows
blab:   LR A,4                  ; the row out
        COM
        NI 03FH
        AS 8
        OUTS 5
        LI 80
        LR 3,A                  ; columns
        LI (~20)&0FFH
        LR 2,A                  ; the column, complemented
        LI 080H
        LR 7,A                  ; the value: 1 (port-1 code), then 2
        NOPS 4
blab_P: LR A,7
        OUTS 1
        LR A,2
        OUTS 4
        LI 060H
        OUTS 0
        LI 050H
        OUTS 0
        IF bds
        DS 6
        ENDIF
        NOPS bnops
        LR A,7
        XI 0C0H
        LR 7,A
        DS 2
        DS 3
        BNZ blab_P
        NOPS 20                 ; (the row's own write well after)
        LR A,4
        INC
        LR 4,A
        DS 9
        BNZ blab
        ENDM

TSWEEP: PI DCLS
        LI TY0+6                ; the bands' rows on light grey
        LR 0,A
        LI TY0+6*8-1
        LR 1,A
        LIS 2
        LR 2,A
        PI PALSET
        CLR                     ; the title
        LR 0,A
        LR 1,A
        LI CV1
        LR 2,A
        PI TXAT
        DCI TSWTTL
        PI TXSTR
        CLR
        STB TV+2
TSW1:   CLR                     ; each band's label: its wait, at row n + 1
        LR 0,A
        LDB TV+2
        INC
        LR 1,A
        LI CV1
        LR 2,A
        PI TXAT
        LDB TV+2
        SL 1
        SL 1
        DCI TSWLBL
        ADC
        PI TXSTR
        INCB TV+2
        CI 7
        BNZ TSW1
        DCI SNDB
        LM
        LR 8,A                  ; the tone bits (kept on port 5)
TSW2:   LI TY0+6                ; the bands, again and again
        LR 4,A
        SWBAND SWB0, 1, 2       ;  70
        LI TY0+12
        LR 4,A
        SWBAND SWB1, 0, 6       ;  80
        LI TY0+18
        LR 4,A
        SWBAND SWB2, 1, 7       ;  90
        LI TY0+24
        LR 4,A
        SWBAND SWB3, 1, 8       ;  94
        LI TY0+30
        LR 4,A
        SWBAND SWB4, 1, 11      ; 106
        LI TY0+36
        LR 4,A
        SWBAND SWB5, 0, 16      ; 120
        LI TY0+42
        LR 4,A
        SWBAND SWB6, 0, 20      ; 136
        JMP TSW2
TSWTTL: DB "SETTLE SWEEP",0
TSWLBL: DB "070",0, "080",0, "090",0, "094",0, "106",0, "120",0, "136",0
