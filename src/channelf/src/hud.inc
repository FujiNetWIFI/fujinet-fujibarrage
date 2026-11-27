; ---------------------------------------------------------------------------
; hud.inc -- the field's two HUD rows (text rows 9 and 10, under the field)
; and what takes them over (src/play.c's hud, items_line, the flashes and
; talk; the Arcadia's p_hud.inc and p_msg.inc): resident, so the HUD page
; and the shot player (in RAM) both draw them.
;
; Row 1: the turn's (or our) name, A angle, P power, E energy; our turn's
;   first BANJ jiffies (to the first input) YOUR TURN!; the defence window
;   SET UP DEFENCES START and its timer. Those two flash: the row's palette,
;   black and light grey, 16 jiffies each (p_hud.inc FLTICK).
; Row 2: the weapon, our count of it, the wind, the timer; or the items
;   line and the timer (an item: its name, X count, ON or OFF the
;   parachute's and the impact fuse's; a target: TARGET name; driving:
;   DRIVE < > FUEL n).
; A message (MSGJ jiffies; the rows wait) on a light grey band: a talk
;   (OP_TALK: the tank's name and a colon in its seat's ink, its words typed a
;   letter a pass over both rows, broken at a space; the rest at the
;   segment's end, SCRIPT's bytes going), or an event line over row 1
;   (OP_DAMAGE: name HIT -amount E energy; OP_SHIELD: name, what the shield
;   did, its hp).
; ---------------------------------------------------------------------------

M_ITEM   EQU 1                  ; MENU: the items line's state
M_TARGET EQU 2
M_DRIVE  EQU 3
FL_BANNER  EQU 1                ; FLK: what flashes on row 1
FL_DEFENCE EQU 2
BANJ    EQU 180                 ; YOUR TURN! (or to the first input)
TALKJ   EQU 120                 ; a talk stays (src/play.c talk_left)
EVTJ    EQU 96                  ; an event line stays
MSGPAL  EQU 2                   ; the messages' band, the lit flash: light grey
MK_TALK EQU 1                   ; MSGK
MK_EVT  EQU 2

; SEATAT -- DC0 = seat r0's record: ST + OFF_SEATS + 25 s.  Leaf.
;   Clobbers A, r1.
SEATAT: LR A,0
        SL 1
        SL 1
        SL 1
        LR 1,A
        SL 1
        AS 1
        AS 0
        DCI ST+OFF_SEATS
        ADCU
        POP

; ---- the HUD -------------------------------------------------------------------
; HDRAW -- both rows.
HDRAW:  ENTER
        PI HROW1
        PI HROW2
        LEAVE

; HSEAT -- r0 = the seat the HUD shows: the turn's, else ours, else 0.  Leaf.
HSEAT:  LDB ST+OFF_TURN
        CI SEATS-1
        BC HSE1
        LDB MYSEAT
        CI SEATS-1
        BC HSE1
        CLR
HSE1:   LR 0,A
        POP

; HMINE -- r1 = 1 if seat r0 is ours and we aim (the numbers are ours).  Leaf.
HMINE:  CLR
        LR 1,A
        LDB MYTURN
        NI 0FFH
        BZ HMI9
        LDB MYSEAT
        XS 0
        BNZ HMI9
        LIS 1
        LR 1,A
HMI9:   POP

; HROW1 -- "NAME8 A 90 P1000 E100"
HROW1:  ENTER
        LDB MSGJ                ; a message holds the rows
        NI 0FFH
        BZ HR10
        LEAVE
HR10:   PI DEFING               ; the defence window: what to do, the timer
        LR A,0
        NI 0FFH
        BZ HR10B
        PI TVCLR
        DCI S_DEF
        CLR
        LR 0,A
        PI TVSTR
        LDB ST+OFF_TIMER
        LR 0,A
        DCI HB+21
        PI DECB3
        BR HR1S
HR10B:  LDB FLK                 ; our turn's first moments: YOUR TURN!
        CI FL_BANNER
        BNZ HR10C
        PI TVCLR
        DCI S_YTURN
        LIS 7
        LR 0,A
        PI TVSTR
HR1S:   LI HROW0
        LR 1,A
        PI HRSHOW
        LEAVE
HR10C:  PI HSEAT
        LR A,0
        STB T2
        PI SEATAT               ; its name, 8 padded
        LR Q,DC
        DCI HB
        XDC
        LR DC,Q
        LIS NAME_LEN
        LR 2,A
HR11:   LM
        NI 0FFH
        BNZ HR12
        LI ' '
HR12:   XDC
        ST
        XDC
        DS 2
        BNZ HR11
        LDB T2                  ; the angle, power, energy
        LR 0,A
        PI SEATAT
        LR Q,DC
        LI P_ANGLE
        ADC
        LM
        LR 3,A
        LR DC,Q
        LI P_POWER
        ADC
        LM
        LR 4,A
        LM
        LR 5,A
        LR DC,Q
        LI P_ENERGY
        ADC
        LM
        LR 6,A
        LDB T2                  ; ours, aiming: AIMA, AIMP
        LR 0,A
        PI HMINE
        LR A,1
        NI 0FFH
        BZ HR13
        LDB AIMA
        LR 3,A
        LDB AIMP
        LR 4,A
        LDB AIMP+1
        LR 5,A
HR13:   LR A,6
        STB HB+20               ; (the energy, over the number calls)
        LR A,4
        STB HB+21
        LR A,5
        STB HB+22
        XDC
        LI ' '
        ST
        LI 'A'
        ST
        LR A,3
        LR 0,A
        PI DECB3
        LI ' '
        ST
        LI 'P'
        ST
        LR Q,DC
        LDB HB+21
        LR 0,A
        LDB HB+22
        LR 1,A
        LR DC,Q
        PI DECW4
        LI ' '
        ST
        LI 'E'
        ST
        LR Q,DC
        LDB HB+20
        LR 0,A
        LR DC,Q
        PI DECB3
        CLR
        ST
        CLR
        LR 0,A
        LI HROW0
        LR 1,A
        LI CV1
        LR 2,A
        PI TXAT
        DCI HB
        PI TXSTR
        LEAVE

; HROW2 -- "WEAPON10 X99 <1000 120": the weapon (ours while we aim), our
;   count of it, the wind, the timer.
HROW2:  ENTER
        LDB MSGJ                ; a message holds the rows
        NI 0FFH
        BZ HR20
        LEAVE
HR20:   LDB MENU                ; the items line open: it instead
        NI 0FFH
        BZ HR20N
        PI IROW
        JMP HR2T
HR20N:  PI HSEAT
        LR A,0
        STB T2
        PI HMINE
        LR A,1
        NI 0FFH
        BZ HR21
        LDB AIMW
        BR HR22
HR21:   LDB T2
        LR 0,A
        PI SEATAT
        LI P_WEAPON
        ADC
        LM
HR22:   STB HB+30               ; the weapon
        PI TVCLR                ; 24 spaces first
        PI CATCHK               ; its name (the catalog whole)
        LDB CATOK
        NI 0FFH
        BZ HR26
        LDB HB+30
        CI N_ITEMS-1
        BNC HR26
        LR 0,A
        PI CATITEM
        LIS 6
        ADC
        LM                      ; its length: 10 at most
        CI 10
        BC HR24
        LI 10
HR24:   LR 2,A
        NI 0FFH
        BZ HR26
        XDC
        DCI HB
        XDC
HR25:   LM
        XDC
        ST
        XDC
        DS 2
        BNZ HR25
HR26:   LDB T2                  ; ours, not the Mini Missile: X count
        LR 0,A
        LDB MYSEAT
        XS 0
        BNZ HR27
        LDB HB+30
        NI 0FFH
        BZ HR27
        DCI ST+OFF_INV
        ADC
        LM
        LR 3,A
        DCI HB+11
        LI 'X'
        ST
        LR A,3
        CI 99
        BC HR26A
        LI 99
HR26A:  LR 1,A
        PI DIG2S
HR27:   DCI ST+OFF_WIND         ; the wind: < or >, its strength
        LM
        LR 0,A
        LM
        LR 1,A
        LI ' '
        LR 2,A
        LR A,1
        NI 0FFH
        BM HR28
        BNZ HR27A
        LR A,0
        NI 0FFH
        BZ HR29
HR27A:  LI '>'
        LR 2,A
        BR HR29
HR28:   LR A,1                  ; negative: its magnitude (~hi first: COM
        COM                     ;  clears the carry INC gives)
        LR 1,A
        LR A,0
        COM
        INC
        LR 0,A
        LR A,1
        LNK
        LR 1,A
        LI '<'
        LR 2,A
HR29:   DCI HB+15
        LR A,2
        ST
        PI DECW4
HR2T:   LDB ST+OFF_TIMER        ; the timer
        LR 0,A
        DCI HB+21
        PI DECB3
        CLR
        LR 0,A
        LI HROW0+1
        LR 1,A
        LI CV1
        LR 2,A
        PI TXAT
        DCI HB
        PI TXSTR
        LEAVE

; DEFING -- r0 = 1 in our defence window (src/play.c defending).  Leaf.
;   Clobbers A, r1, r2, DC0.
DEFING: CLR
        LR 0,A
        LDB ST+OFF_PHASE
        CI PH_DEFENSE
        BNZ DFG9
        LDB MYSEAT
        CI SEATS-1
        BNC DFG9
        LR 1,A
        SL 1
        SL 1
        SL 1
        LR 2,A
        SL 1
        AS 2
        AS 1
        DCI ST+OFF_SEATS+P_FLAGS
        ADCU
        LM
        NI PF_DEFENSE+PF_DONE
        CI PF_DEFENSE
        BNZ DFG9
        LIS 1
        LR 0,A
DFG9:   POP

; ---- a row in HB ---------------------------------------------------------------
; TVCLR -- HB: TCOLS spaces, a NUL.  Leaf.  Clobbers A, r2, DC0.
TVCLR:  DCI HB
        LI TCOLS
        LR 2,A
TVC1:   LI ' '
        ST
        DS 2
        BNZ TVC1
        CLR
        ST
        POP

; TVSTR -- the string at DC0 (NUL ends it) into HB from column r0; r0 past
;   it.  Leaf.  Clobbers A, DCs.
TVSTR:  XDC
        DCI HB
        LR A,0
        ADC
        XDC
TVS1:   LM
        NI 0FFH
        BZ TVS9
        XDC
        ST
        XDC
        LR A,0
        INC
        LR 0,A
        BR TVS1
TVS9:   POP

; TVNAME -- seat r0's name (8 at most: a NUL ends it) into HB from column
;   r2; r2 past it.  Leaf.  Clobbers A, r1, r3, DCs.
TVNAME: LR A,0
        SL 1
        SL 1
        SL 1
        LR 1,A
        SL 1
        AS 1
        AS 0
        DCI ST+OFF_SEATS+P_NAME
        ADCU
        XDC
        DCI HB
        LR A,2
        ADC
        XDC
        LIS NAME_LEN
        LR 3,A
TVNM1:  LM
        NI 0FFH
        BZ TVNM9
        XDC
        ST
        XDC
        LR A,2
        INC
        LR 2,A
        DS 3
        BNZ TVNM1
TVNM9:  POP

; TVNUM -- r1 (0-255) as its digits into HB from column r0; r0 past them.
;   Clobbers A, r1-r3, DCs.
TVNUM:  ENTER
        LR A,0
        STB HB+47
        LR A,1
        LR 0,A
        DCI HB+44
        PI DECB3
        LDB HB+47
        LR 0,A
        DCI HB
        ADC
        XDC
        DCI HB+44
        LIS 3
        LR 3,A
TVM1:   LM
        CI ' '
        BZ TVM2
        XDC
        ST
        XDC
        LR A,0
        INC
        LR 0,A
TVM2:   DS 3
        BNZ TVM1
        LEAVE

; HRSHOW -- HB on text row r1 in CV1.
HRSHOW: ENTER
        CLR
        LR 0,A
        LI CV1
        LR 2,A
        PI TXAT
        DCI HB
        PI TXSTR
        LEAVE

; IROW -- the items line into HB (src/play.c items_line), the timer's
;   cells blank.
IROW:   ENTER
        PI TVCLR
        LDB MENU
        CI M_ITEM
        BZ IR1
        CI M_TARGET
        BNZ IR0
        JMP IR5
IR0:    DCI S_DRIVE             ; driving: DRIVE < > FUEL n
        CLR
        LR 0,A
        PI TVSTR
        DCI ST+OFF_INV
        LI I_FUEL
        ADC
        LM
        LR 0,A
        DCI HB+14
        PI DECB3
        LEAVE
IR1:    PI CATCHK               ; an item: its name (12 at most)
        LDB CATOK
        NI 0FFH
        BZ IR3
        LDB ITEM
        CI N_ITEMS-1
        BNC IR9
        LR 0,A
        PI CATITEM
        LIS 6
        ADC
        LM
        CI 12
        BC IR2
        LI 12
IR2:    LR 2,A
        NI 0FFH
        BZ IR3
        XDC
        DCI HB
        XDC
IR2A:   LM
        XDC
        ST
        XDC
        DS 2
        BNZ IR2A
IR3:    LDB ITEM                ; X count
        CI N_ITEMS-1
        BNC IR9
        LR 3,A
        LI 'X'
        STB HB+13
        DCI ST+OFF_INV
        LR A,3
        ADC
        LM
        CI 99
        BC IR3A
        LI 99
IR3A:   LR 1,A
        DCI HB+14
        PI DIG2S
        LR A,3                  ; the parachute, the impact fuse: ON, OFF
        CI I_PARACHUTE
        LIS PF_CHUTE
        BZ IR4
        LR A,3
        CI I_IMPACT_FUSE
        LIS PF_FUSE
        BNZ IR9
IR4:    LR 4,A
        LDB MYSEAT
        CI SEATS-1
        BNC IR9
        LR 0,A
        PI SEATAT
        LI P_FLAGS
        ADC
        LM
        NS 4
        DCI S_OFF
        BZ IR4A
        DCI S_ON
IR4A:   LI 17
        LR 0,A
        PI TVSTR
IR9:    LEAVE
IR5:    DCI S_TARGET            ; a target: TARGET name
        CLR
        LR 0,A
        PI TVSTR
        LR A,0
        LR 2,A
        LDB TARGET
        CI SEATS-1
        BNC IR9
        LR 0,A
        PI TVNAME
        LEAVE

S_DEF:    DB "SET UP DEFENCES START",0
S_YTURN:  DB "YOUR TURN!",0
S_DRIVE:  DB "DRIVE < > FUEL",0
S_TARGET: DB "TARGET ",0
S_ON:     DB "ON",0
S_OFF:    DB "OFF",0
S_HIT:    DB " HIT -",0
S_E:      DB " E ",0
; what a shield did (SH_HIT ... SH_LIFT), 10 a word
SHWORDS:  DB "SHIELD",0,0,0,0
          DB "BROKEN",0,0,0,0
          DB "DEFLECTS",0,0
          DB "SHIELD UP",0
          DB "LIFTED",0,0,0,0

; ---- the messages -----------------------------------------------------------
; MTALK -- OP_TALK's arguments at Q (seat, length, words): a tank on the
;   field speaks: the rows blank on the band, its name in its seat's ink,
;   then its words a letter a pass (MSTEP).  Clobbers everything.
MTALK:  ENTER
        LR DC,Q
        LM
        LR 0,A                  ; the seat
        LM
        LR 1,A                  ; its words' length
        LR Q,DC                 ; Q = its words
        LR A,0
        CI SEATS-1
        BNC MTK9
        STB TALKS
        DCI TKON                ; (its tank on the field)
        ADC
        LM
        NI 0FFH
        BZ MTK9
        LR A,1
        STB TALKN
        CLR
        STB TALKI
        LR A,QU
        STB TALKP
        LR A,QL
        STB TALKP+1
        LIS MK_TALK
        STB MSGK
        LI TALKJ
        STB MSGJ
        PI MBAND
        PI TVCLR                ; its name alone (a NUL after it)
        LDB TALKS
        LR 0,A
        CLR
        LR 2,A
        PI TVNAME
        LR A,2                  ; (no name: the words from the first cell)
        NI 0FFH
        BZ MTK1
        DCI HB                  ; a colon after it, then a NUL
        ADC
        LI ':'
        ST
        CLR
        ST
        LR A,2                  ; its words a cell after that
        INC
        INC
MTK1:   STB TALKX
        LDB TALKS
        LR 0,A
        PI INKOF
        LR 2,A
        CLR
        LR 0,A
        LI HROW0
        LR 1,A
        STB TALKY
        PI TXAT
        DCI HB
        PI TXSTR
        PI TBREAK
MTK9:   LEAVE

; INKOF -- A = seat r0's ink: blue, red, green by seat (mod 3).  Leaf.
INKOF:  LR A,0
INK0:   CI 2
        BC INK1
        AI -3
        BR INK0
INK1:   DCI INKT
        ADC
        LM
        POP
INKT:   DB CV1, CV2, CV3

; TBREAK -- TALKB: where the words go on to row 2 (the last space that fits
;   row 1 after the name, skipped; none: the first cell past it; FF if all
;   of them fit).  Leaf.  Clobbers A, r4-r7, Q, DC0.
TBREAK: LDB TALKX               ; the row's room after the name
        COM
        INC
        AI TCOLS
        LR 4,A
        LI 0FFH
        LR 6,A
        LDB TALKN               ; all of them fit: no break
        COM
        INC
        AS 4
        BC TBR9
        DCI TALKP
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        CLR
        LR 7,A                  ; i: 0 .. room
TBR1:   LM
        CI ' '
        BNZ TBR2
        LR A,7
        LR 6,A
TBR2:   LR A,7
        XS 4
        BZ TBR3
        LR A,7
        INC
        LR 7,A
        BR TBR1
TBR3:   LR A,6
        CI 0FFH
        BNZ TBR9
        LR A,4
        LR 6,A
TBR9:   LR A,6
        STB TALKB
        POP

; MSTEP -- a pass of a talk: its next letter (a blip every other one).
MSTEP:  ENTER
        LDB TALKN
        NI 0FFH
        BZ MSP9
        LDB TALKI
        NI 1
        BNZ MSP1
        SND UI_TALK
MSP1:   PI MLETTER
MSP9:   LEAVE

; MFLUSH -- a talk's letters left, all now (the segment's end).
MFLUSH: ENTER
MFL1:   LDB TALKN
        NI 0FFH
        BZ MFL9
        PI MLETTER
        BR MFL1
MFL9:   LEAVE

; MLETTER -- a talk's next letter at (TALKX, TALKY): at the break, row 2
;   from its first cell (a space there not shown); past a row's end, none.
MLETTER: ENTER
        LDB TALKI
        LR 3,A
        INC
        STB TALKI
        DECB TALKN
        DCI TALKP
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LR A,3
        ADC
        LM
        STB MLCH
        LDB TALKB
        XS 3
        BNZ MLT1
        CLR
        STB TALKX
        LI HROW0+1
        STB TALKY
        LDB MLCH
        CI ' '
        BZ MLT9
MLT1:   LDB TALKX
        CI TCOLS-1
        BNC MLT9
        LR 0,A
        INC
        STB TALKX
        LDB TALKY
        LR 1,A
        LI CV1
        LR 2,A
        PI TXAT
        LDB MLCH
        LR 0,A
        PI TXCH
MLT9:   LEAVE

; MBAND -- both rows blank on the messages' band.
MBAND:  ENTER
        LI HROW0
        LR 0,A
        LI HROW0+1
        LR 1,A
        PI TXCLR
        LI HUDY0
        LR 0,A
        LI HUDY1+5
        LR 1,A
        LIS MSGPAL
        LR 2,A
        PI PALSET
        LEAVE

; MEVT -- an event line to come on row 1: a talk's second row blank, its
;   letters dropped; row 1 on the band; EVTJ jiffies at least.
MEVT:   ENTER
        LDB MSGK
        CI MK_TALK
        BNZ MEV1
        LI HROW0+1
        LR 0,A
        LR 1,A
        PI TXCLR
        LI HUDY1
        LR 0,A
        LI HUDY1+5
        LR 1,A
        CLR
        LR 2,A
        PI PALSET
MEV1:   CLR
        STB TALKN
        LIS MK_EVT
        STB MSGK
        LDB MSGJ
        CI EVTJ-1
        BNC MEV2
        LI EVTJ
        STB MSGJ
MEV2:   LI HUDY0
        LR 0,A
        LI HUDY0+5
        LR 1,A
        LIS MSGPAL
        LR 2,A
        PI PALSET
        LEAVE

; MEVARGS -- an event op's arguments at Q (seat, a, b) into HB+40..42;
;   r0 = 1 if the seat is one.  Leaf.  Clobbers A, r1, r2, DC0.
MEVARGS: LR DC,Q
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A
        DCI HB+40
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        LR A,0
        CI SEATS-1
        LIS 1
        BC MVA1
        CLR
MVA1:   LR 0,A
        POP

; MEVNAME -- HB: the event's seat's name, a space; r0 past them.
MEVNAME: ENTER
        PI MEVT
        PI TVCLR
        LDB HB+40
        LR 0,A
        CLR
        LR 2,A
        PI TVNAME
        LR A,2
        LR 0,A
        LEAVE

; MDMG -- OP_DAMAGE at Q (seat, amount, energy left): row 1 "name HIT
;   -amount E energy".  Clobbers everything.
MDMG:   ENTER
        PI MEVARGS
        LR A,0
        NI 0FFH
        BZ MDG9
        PI MEVNAME
        DCI S_HIT
        PI TVSTR
        LDB HB+41
        LR 1,A
        PI TVNUM
        DCI S_E
        PI TVSTR
        LDB HB+42
        LR 1,A
        PI TVNUM
        LI HROW0
        LR 1,A
        PI HRSHOW
MDG9:   LEAVE

; MSHLD -- OP_SHIELD at Q (seat, event, hp): row 1 "name what hp".
;   Clobbers everything.
MSHLD:  ENTER
        PI MEVARGS
        LR A,0
        NI 0FFH
        BZ MSH9
        PI MEVNAME
        LDB HB+41               ; its word: 10 x the event (SH_HIT ...)
        CI SH_LIFT
        BC MSH1
        CLR
MSH1:   SL 1
        LR 1,A
        SL 1
        SL 1
        AS 1
        DCI SHWORDS
        ADC
        PI TVSTR
        LR A,0
        INC
        LR 0,A
        LDB HB+42
        LR 1,A
        PI TVNUM
        LI HROW0
        LR 1,A
        PI HRSHOW
MSH9:   LEAVE

; MTICK -- a message's time less r0 jiffies (not while a talk types); over:
;   MEND, or (r1 = 1: in a script) 1 left (the rows are drawn after it).
MTICK:  ENTER
        LR A,1
        STB HB+43
        LDB MSGJ
        NI 0FFH
        BZ MTI9
        LR 1,A
        LDB TALKN
        NI 0FFH
        BNZ MTI9
        LR A,0
        COM
        INC
        AS 1
        BNC MTI8
        BZ MTI8
        STB MSGJ
        LEAVE
MTI8:   LDB HB+43
        NI 0FFH
        BZ MTI8A
        LIS 1
        STB MSGJ
        LEAVE
MTI8A:  PI MEND
MTI9:   LEAVE

; MEND -- the message over: the rows black again and (on the field) the
;   HUD's.
MEND:   ENTER
        CLR
        STB MSGJ
        STB MSGK
        STB TALKN
        LI HUDY0
        LR 0,A
        LI HUDY1+5
        LR 1,A
        CLR
        LR 2,A
        PI PALSET
        LDB SHOWS
        CI S_FIELD
        BNZ MEN9
        PI HDRAW
MEN9:   LEAVE
