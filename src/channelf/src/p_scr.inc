; ---------------------------------------------------------------------------
; p_scr.inc -- page SCR: the text screens at a room (src/room.c): the
; waiting room (and its settings), the shop, a round's end, the game's.
; Nine text rows of 24 cells; row 0 a white title on black; the lists on a
; light grey band, names in their seat's ink (blue, red, green by seat).
;
; Entries: E_ENTER (drawn afresh), E_STATE (a new state on it), E_TICK
; (its pass), each by the screen up (SHOWS). Text is written only where it
; changes (disp.inc TXCH), so a screen is redrawn whole on a new state.
;
; Keys: the waiting room: push starts (the host), HOLD the settings (the
; host: up/down choose, left/right change, twist the page); the shop: up/
; down choose (it scrolls), push buys, left sells, START is done.
; ---------------------------------------------------------------------------

SCRL    EQU TV                  ; a line being built: 24 cells and a NUL
SBAND0  EQU TY0+6               ; the grey band: text rows 1-7
SBAND1  EQU TY0+6*8-1

; this page's own: SSET, SPAGE, SSEL, SCTOP, SN, SPREV, SREP, SMSG, SK,
; SHOPIDS, SVTXT, SVN (ram.inc); GT+0..15 its routines' scratch

SENTER: ENTER
        CLR
        LR 0,A
        PI DCLS
        CLR
        STB SSET
        STB SPAGE
        STB SSEL
        STB SCTOP
        STB SMSG
        LDB INPDIR
        STB SPREV
        PI CATCHK
        LDB SHOWS
        CI S_SHOP
        BNZ SEN1
        PI SHOPLIST
        SND MUS_SHOP
        BR SEN3
SEN1:   CI S_ROUND
        BNZ SEN2
        SND MUS_WIN
        BR SEN3
SEN2:   CI S_OVER
        BNZ SEN3
        SND MUS_OVER
SEN3:   PI SDRAW
        LEAVE

SSTATE: ENTER
        PI SDRAW
        LEAVE

; SDRAW -- the screen up, whole.
SDRAW:  ENTER
        LDB SHOWS
        CI S_WAIT
        BNZ SD1
        LDB SSET
        NI 0FFH
        BNZ SD0
        PI WROOM
        LEAVE
SD0:    PI WSETS
        LEAVE
SD1:    CI S_SHOP
        BNZ SD2
        PI SHOPDRAW
        LEAVE
SD2:    PI RESULTS
        LEAVE

; ---- lines ------------------------------------------------------------------------
; LCLR -- SCRL: 24 spaces and a NUL.  Leaf.
LCLR:   DCI SCRL
        LI TCOLS
        LR 0,A
LCL1:   LI ' '
        ST
        DS 0
        BNZ LCL1
        CLR
        ST
        POP

; LCOPY -- up to r1 characters of the string at DC1 (NUL ends it) into
;   SCRL at column r0.  Leaf.  Clobbers A, r1, DCs.
LCOPY:  LR A,0
        DCI SCRL
        ADC
LCP1:   LR A,1
        NI 0FFH
        BZ LCPX
        XDC
        LM
        XDC
        NI 0FFH
        BZ LCPX
        ST
        DS 1
        BR LCP1
LCPX:   POP

; LNAME -- seat r0's name (8, NULs as spaces) into SCRL at column r2.
;   Leaf.  Clobbers A, r1, r3, DCs.
LNAME:  LR A,0
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
        LR A,2
        DCI SCRL
        ADC
        LIS NAME_LEN
        LR 3,A
LNM1:   XDC
        LM
        XDC
        NI 0FFH
        BNZ LNM2
        LI ' '
LNM2:   ST
        DS 3
        BNZ LNM1
        POP

; LSHOW -- SCRL on text row r1 in ink r2.
LSHOW:  ENTER
        CLR
        LR 0,A
        PI TXAT
        DCI SCRL
        PI TXSTR
        LEAVE

; SEATINK -- A = seat r0's ink: blue, red, green by seat (mod 3).  Leaf.
SEATINK: LR A,0
SI1:    CI 2
        BC SI2
        AI -3
        BR SI1
SI2:    DCI INKS
        ADC
        LM
        POP
INKS:   DB CV1, CV2, CV3

; KIND -- A = seat r0's P_KIND.  Leaf.
KIND:   LR A,0
        SL 1
        SL 1
        SL 1
        LR 1,A
        SL 1
        AS 1
        AS 0
        DCI ST+OFF_SEATS+P_KIND
        ADCU
        LM
        POP

; ---- the waiting room ----------------------------------------------------------
; WROOM -- "WAITING ROOM: id"; the seats two a row on the band (a marker:
;   * the host; the name in its ink; B and the tier a bot's, ? a person's
;   link down); the hint.
WROOM:  ENTER
        PI LCLR
        DCI S_ROOM
        XDC
        CLR
        LR 0,A
        LI 14
        LR 1,A
        PI LCOPY
        DCI SRVTBL
        XDC
        LI 14
        LR 0,A
        LIS 8
        LR 1,A
        PI LCOPY
        CLR
        LR 1,A
        LI CV1
        LR 2,A
        PI LSHOW
        LI SBAND0               ; the seats' band
        LR 0,A
        LI SBAND1
        LR 1,A
        LIS 2
        LR 2,A
        PI PALSET
        CLR
        STB GT+8
WR1:    PI WSEAT
        LDB GT+8
        INC
        STB GT+8
        CI SEATS
        BNZ WR1
        PI WHINT
        LEAVE

; WSEAT -- seat GT+8's cell: rows 2-6, columns 0 and 12.
WSEAT:  ENTER
        LDB GT+8                ; its place
        CI 4
        LR 1,A
        LIS 0
        BC WS1
        LR A,1
        AI -5
        LR 1,A
        LI 12
WS1:    STB GT+9                ; column
        LR A,1
        AI 2
        STB GT+10               ; row
        DCI SCRL                ; 12 cells: marker, name, kind
        LI 12
        LR 0,A
WS2:    LI ' '
        ST
        DS 0
        BNZ WS2
        CLR
        ST
        LDB GT+8
        LR 0,A
        PI KIND
        CI KIND_EMPTY
        BNZ WS3
        DCI S_OPEN
        XDC
        LIS 1
        LR 0,A
        LI 10
        LR 1,A
        PI LCOPY
        LI CV1                  ; (blue: the band's own)
        STB GT+11
        BR WS9
WS3:    STB GT+12               ; the kind
        LDB GT+8                ; the host: *
        LR 0,A
        LDB ST+OFF_HOST
        XS 0
        BNZ WS4
        LI '*'
        STB SCRL
WS4:    LDB GT+8
        LR 0,A
        LIS 1
        LR 2,A
        PI LNAME
        LDB GT+12               ; a bot: B and its tier
        CI KIND_BOT
        BNZ WS5
        LI 'B'
        STB SCRL+10
        LDB GT+8
        LR 0,A
        LI P_TIER
        LR 1,A
        PI SEATF
        LM
        INC
        AI '0'
        STB SCRL+11
        BR WS8
WS5:    LDB GT+8                ; a person, the link down: ?
        LR 0,A
        LI P_FLAGS
        LR 1,A
        PI SEATF
        LM
        NI PF_CONN
        BNZ WS8
        LI '?'
        STB SCRL+11
WS8:    LDB GT+8
        LR 0,A
        PI SEATINK
        STB GT+11
WS9:    LDB GT+9
        LR 0,A
        LDB GT+10
        LR 1,A
        LDB GT+11
        LR 2,A
        PI TXAT
        DCI SCRL
        PI TXSTR
        LEAVE

; WHINT -- row 8: what the keys do here.
WHINT:  ENTER
        LDB ST+OFF_HOST
        LR 0,A
        LDB MYSEAT
        XS 0
        DCI S_WHOST
        BZ WH1
        DCI S_WWAIT
WH1:    LR Q,DC
        PI LCLR
        LR DC,Q
        XDC
        CLR
        LR 0,A
        LI TCOLS
        LR 1,A
        PI LCOPY
        LIS 8
        LR 1,A
        LI CV1
        LR 2,A
        PI LSHOW
        LEAVE

; ---- the settings (the waiting room's HOLD) ------------------------------------
; WSETS -- "SETTINGS: page"; its settings (up to 7 shown from SCTOP): label,
;   value; the host's cursor; the hint.
WSETS:  ENTER
        PI SKS                  ; SK: the page's settings, SN of them
        PI LCLR
        DCI S_SETT
        XDC
        CLR
        LR 0,A
        LI 10
        LR 1,A
        PI LCOPY
        LDB SPAGE               ; the page's name
        SL 1
        DCI PAGEAT
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LM                      ; (its length)
        LR 1,A
        LR Q,DC
        LR DC,Q
        XDC
        LI 10
        LR 0,A
        PI LCOPY
        CLR
        LR 1,A
        LI CV1
        LR 2,A
        PI LSHOW
        LI SBAND0
        LR 0,A
        LI SBAND1
        LR 1,A
        LIS 2
        LR 2,A
        PI PALSET
        CLR
        STB GT+8                ; the row
WSE1:   PI WSLINE
        LDB GT+8
        INC
        STB GT+8
        CI 7
        BNZ WSE1
        PI LCLR                 ; the hint
        LDB ST+OFF_HOST
        LR 0,A
        LDB MYSEAT
        XS 0
        DCI S_SHOST
        BZ WSE2
        DCI S_SLOOK
WSE2:   XDC
        CLR
        LR 0,A
        LI TCOLS
        LR 1,A
        PI LCOPY
        LIS 8
        LR 1,A
        LI CV1
        LR 2,A
        PI LSHOW
        LEAVE

; SKS -- SK = the settings of page SPAGE (up to 16), SN their number.
SKS:    ENTER
        CLR
        STB SN
        LR 4,A                  ; the setting
SKP1:   LR A,4
        SL 1
        DCI SETAT
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LM                      ; its page
        LR 1,A
        LDB SPAGE
        XS 1
        BNZ SKP2
        LDB SN
        LR 2,A
        CI 15
        BNC SKP2
        DCI SK
        ADC
        LR A,4
        ST
        LR A,2
        INC
        STB SN
SKP2:   LR A,4
        INC
        LR 4,A
        CI N_SETTINGS
        BNZ SKP1
        LEAVE

; WSLINE -- settings row GT+8 (text row 1 + it): SK[SCTOP + it].
WSLINE: ENTER
        PI LCLR
        LDB SCTOP
        LR 0,A
        LDB GT+8
        AS 0
        STB GT+9                ; the entry
        LR 0,A
        LDB SN
        COM                     ; (past the list: a blank row)
        AS 0                    ; (entry + ~SN: C when entry >= SN)
        BC WSL8
        LDB GT+9
        DCI SK
        ADC
        LM
        STB GT+10               ; the setting
        LDB GT+9                ; the cursor
        LR 0,A
        LDB SSEL
        XS 0
        BNZ WSL1
        LI '>'
        STB SCRL
WSL1:   LDB GT+10               ; its label: page, type, label
        SL 1
        DCI SETAT
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LM
        LM
        LM                      ; the label's length
        LR 1,A
        CI 12
        BC WSL2
        LI 12
        LR 1,A
WSL2:   XDC
        LIS 1
        LR 0,A
        PI LCOPY
        LDB GT+10               ; its value, right-aligned to column 23
        LR 0,A
        PI SETVAL               ; (SVTXT: the text, SVN its length)
        LDB SVN
        COM
        INC
        AI TCOLS
        LR 0,A
        DCI SVTXT
        XDC
        LDB SVN
        LR 1,A
        PI LCOPY
WSL8:   LDB GT+8
        INC
        LR 1,A
        LI CV1
        LR 2,A
        PI LSHOW
        LEAVE

; SETVAL -- setting r0's value as text: SVTXT (10 at most), SVN long.
;   A choice: its name; a number: min + value x step, its decimals.
SETVAL: ENTER
        LR A,0
        STB GT+11
        DCI ST+OFF_SETTINGS     ; its value
        ADC
        LM
        STB GT+12
        LDB GT+11               ; its record: page, type, label, ...
        SL 1
        DCI SETAT
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LM
        LM
        STB GT+13               ; type
        LR DC,Q                 ; past the label
        LIS 2
        ADC
        LM
        ADCU
        LR Q,DC                 ; (the type's own: kept over the LDB)
        LDB GT+13
        NI 0FFH
        BNZ SV5
        LR DC,Q                 ; a choice: n, names: the value-th
        LM                      ; (n)
        LDB GT+12
        LR 4,A
        LR DC,Q
        LM
SV1:    LR A,4
        NI 0FFH
        BZ SV2
        LM
        ADCU
        DS 4
        BR SV1
SV2:    LM                      ; its length (10 at most)
        CI 10
        BC SV3
        LI 10
SV3:    LR 1,A
        LR Q,DC                 ; (its characters: kept over the STB)
        LR A,1
        STB SVN
        LR DC,Q
        XDC
        DCI SVTXT
        LR A,1
        NI 0FFH
        BZ SVX
SV4:    XDC
        LM
        XDC
        ST
        DS 1
        BNZ SV4
SVX:    LEAVE
SV5:    LR DC,Q                 ; a number: min16, step16, count, decimals
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A
        LM
        LR 3,A
        LM                      ; (count)
        LM
        STB GT+14               ; decimals
        CLR                     ; N32 = min + value x step
        LR 5,A
        LDB GT+12
        LR 4,A
SV6:    LR A,4
        NI 0FFH
        BZ SV7
        LR A,0
        AS 2
        LR 0,A
        LR A,1
        LNK
        LR 1,A
        LR A,5                  ; (a third byte: past 65535)
        LNK
        LR 5,A
        LR A,1
        AS 3
        LR 1,A
        LR A,5
        LNK
        LR 5,A
        DS 4
        BR SV6
SV7:    DCI N32
        LR A,0
        ST
        LR A,1
        ST
        LR A,5
        ST
        CLR
        ST
        PI DEC32                ; NBUF: 8, leading spaces
        LDB GT+14               ; decimals: at least d + 1 digits, a point
        NI 0FFH
        BZ SV9
        LR 4,A
        DCI NBUF+7              ; the leading spaces of the last d + 1: 0
        LR A,4
        COM
        INC
        ADC                     ; NBUF + 7 - d
        LR A,4
        INC
        LR 3,A
SV8:    LM
        CI ' '
        BNZ SV8A
        LI 0FFH
        ADC
        LI '0'
        ST
SV8A:   DS 3
        BNZ SV8
SV9:    LDB GT+14               ; (the decimals: r7, the loop's DC0 kept)
        LR 7,A
        DCI NBUF                ; copy the digits (no spaces), the point d
        LR Q,DC                 ;  from the end
        LIS 8
        LR 3,A
        DCI SVTXT
        XDC
        LR DC,Q
        CLR
        LR 6,A                  ; the length
SV10:   LM
        CI ' '
        BZ SV11
        XDC
        ST
        XDC
        LR A,6
        INC
        LR 6,A
SV11:   LR A,7                  ; (the point before the last d)
        NI 0FFH
        BZ SV12
        LR A,3
        AI -1
        XS 7
        BNZ SV12
        LI '.'
        XDC
        ST
        XDC
        LR A,6
        INC
        LR 6,A
SV12:   DS 3
        BNZ SV10
        LR A,6
        STB SVN
        LEAVE

; ---- the waiting room's keys ----------------------------------------------------
; WTICK -- HOLD: the settings or the seats; push: the host starts; on the
;   settings: up/down the cursor, left/right (the host) the value, twist
;   the page.
WTICK:  ENTER
        LDB EVKEY
        CI KEY_HOLD
        BNZ WT1
        LDB SSET
        XI 1
        STB SSET
        CLR
        STB SSEL
        STB SCTOP
        SND UI_TICK
        CLR
        LR 0,A
        LI TROWS-1
        LR 1,A
        PI TXCLR
        PI SDRAW
        LEAVE
WT1:    LDB EVBTN               ; push: the host starts
        NI 0FFH
        BZ WT2
        LDB ST+OFF_HOST
        LR 0,A
        LDB MYSEAT
        XS 0
        BNZ WT2
        LIS ACT_START
        LR 1,A
        CLR
        LR 2,A
        PI ACTQ
        SND UI_GO
        LEAVE
WT2:    LDB SSET
        NI 0FFH
        BNZ WT3
        LEAVE
WT3:    LDB EVKEY               ; twist: the next page (3)
        CI KEY_TWR
        BZ WT4
        CI KEY_TWL
        BNZ WT5
WT4:    LDB SPAGE
        INC
        CI 3
        BC WT4A
        CLR
WT4A:   STB SPAGE
        CLR
        STB SSEL
        STB SCTOP
        SND UI_TICK
        PI SDRAW
        LEAVE
WT5:    PI SMOVES               ; the stick: GT+0 the moves this pass
        LDB GT
        NI IN_UP+IN_DOWN
        BZ WT8
        LDB SSEL
        LR 1,A
        LDB GT
        NI IN_UP
        BZ WT6
        LR A,1
        NI 0FFH
        BZ WT8
        AI -1
        BR WT7
WT6:    LR A,1                  ; down: within the page
        INC
        LR 1,A
        LDB SN
        COM
        AS 1                    ; (sel + ~SN: C when sel >= SN)
        BC WT8
        LR A,1
WT7:    STB SSEL
        PI SSCROLL
        SND UI_TICK
        PI SDRAW
WT8:    LDB GT                  ; left/right: the host's change
        NI IN_LEFT+IN_RIGHT
        BNZ WT9
        LEAVE
WT9:    LDB ST+OFF_HOST
        LR 0,A
        LDB MYSEAT
        XS 0
        BZ WT10
        LEAVE
WT10:   LDB SSEL                ; the setting, its value, its count
        DCI SK
        ADC
        LM
        STB AARGS
        LR 0,A
        PI SETCNT
        STB GT+3
        LDB AARGS
        DCI ST+OFF_SETTINGS
        ADC
        LM
        LR 1,A
        LDB GT
        NI IN_LEFT
        BZ WT11
        LR A,1                  ; left: one less, the last after 0
        NI 0FFH
        BNZ WT10A
        LDB GT+3
        LR 1,A
WT10A:  LR A,1
        AI -1
        BR WT12
WT11:   LR A,1                  ; right: one more, 0 after the last
        INC
        LR 1,A
        LDB GT+3
        XS 1
        LR A,1
        BNZ WT12
        CLR
WT12:   STB AARGS+1
        LIS ACT_SET
        LR 1,A
        LIS 2
        LR 2,A
        PI ACTQ
        SND UI_TICK
        LEAVE

; SETCNT -- A = setting r0's choices (a choice's n, a number's count).
SETCNT: ENTER
        LR A,0
        SL 1
        DCI SETAT
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LM
        LM
        LR 4,A                  ; type
        LM
        ADCU                    ; past the label
        LR A,4
        NI 0FFH
        BNZ SCN1
        LM                      ; a choice: n
        LEAVE
SCN1:    LIS 4                   ; a number: past min, step: count
        ADC
        LM
        LEAVE

; SSCROLL -- SCTOP so that SSEL shows (7 rows).  Leaf.
SSCROLL: LDB SCTOP
        LR 0,A
        LDB SSEL
        LR 1,A
        COM                     ; sel < top: top = sel
        AS 0                    ; (top + ~sel: C when top > sel)
        BNC SSC1
        LR A,1
        STB SCTOP
        POP
SSC1:   LR A,0                  ; sel >= top + 7: top = sel - 6
        AI 6
        COM
        AS 1                    ; (sel + ~(top + 6): C when sel > top + 6)
        BNC SSCX
        LR A,1
        AI -6
        STB SCTOP
SSCX:   POP

; SMOVES -- GT = the stick's moves this pass: a press, then every 4
;   jiffies once held 16 (src/room.c rep).
SMOVES: ENTER
        LDB SPREV
        COM
        LR 1,A
        LDB INPDIR
        LR 2,A
        STB SPREV
        NS 1                    ; pressed now
        STB GT
        NI 0FFH
        BZ SM1
        LI 16                   ; a press: the next move after 16
        STB SREP
        LEAVE
SM1:    LR A,2                  ; held: every 4
        NI 0FFH
        BNZ SM2
        LEAVE
SM2:    LDB DT
        COM
        INC
        LR 1,A
        LDB SREP
        AS 1
        BC SM3                  ; (no borrow: not yet)
        LI 4
        STB SREP
        LR A,2
        STB GT
        LEAVE
SM3:    STB SREP
        LEAVE

; ---- the shop ---------------------------------------------------------------------
; SHOPLIST -- SK: the items the arms level allows (not the Mini Missile),
;   SN of them (the shop's list; 47 at most: SK holds 7, so the list is
;   built in SHOPIDS).
SHOPLIST: ENTER
        CLR
        STB SN
        LIS 1
        LR 4,A                  ; the item (from 1)
SHI1:    LR A,4
        LR 0,A
        PI CATITEM
        LM                      ; (kind)
        LM                      ; arms level
        LR 1,A
        LDB ST+OFF_SETTINGS+SET_ARMS
        COM
        AS 1                    ; (level + ~arms: C when level > arms)
        BC SHI2
        LDB SN
        DCI SHOPIDS
        ADC
        LR A,4
        ST
        LDB SN
        INC
        STB SN
SHI2:    LR A,4
        INC
        LR 4,A
        CI N_ITEMS
        BNZ SHI1
        LEAVE

; SHOPDRAW -- "SHOP RND n  CASH n"; 7 items from SCTOP; the last row.
SHOPDRAW: ENTER
        PI LCLR
        DCI S_SHOPR
        XDC
        CLR
        LR 0,A
        LIS 9
        LR 1,A
        PI LCOPY
        LDB ST+OFF_ROUND        ; (the round: from 1)
        INC
        LR 1,A
        DCI SCRL+9
        PI DIG2S
        DCI S_CASH
        XDC
        LI 12
        LR 0,A
        LIS 4
        LR 1,A
        PI LCOPY
        DCI ST+OFF_CASH         ; the cash: 32-bit
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A
        LM
        LR 3,A
        DCI N32
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        PI DEC32
        DCI SCRL+16
        LIS 8
        LR 0,A
        PI NUMPUT
        CLR
        LR 1,A
        LI CV1
        LR 2,A
        PI LSHOW
        LI SBAND0
        LR 0,A
        LI SBAND1
        LR 1,A
        LIS 2
        LR 2,A
        PI PALSET
        CLR
        STB GT+8
SHD1:   PI SHLINE
        LDB GT+8
        INC
        STB GT+8
        CI 7
        BNZ SHD1
        PI SHLAST
        LEAVE

; SHLINE -- shop row GT+8 (text row 1 + it): ">NAME9 PRICE6 xB OWN".
SHLINE: ENTER
        PI LCLR
        LDB SCTOP
        LR 0,A
        LDB GT+8
        AS 0
        STB GT+9                ; the entry
        LR 0,A
        LDB SN
        COM
        AS 0
        BNC SHL0                ; (past the list: blank)
        JMP SHL8
SHL0:   LDB GT+9                ; the cursor
        LR 0,A
        LDB SSEL
        XS 0
        BNZ SHL1
        LI '>'
        STB SCRL
SHL1:   LDB GT+9
        DCI SHOPIDS
        ADC
        LM
        STB GT+10               ; the item
        LR 0,A
        PI CATITEM
        LR Q,DC
        LIS 6                   ; its name (9)
        ADC
        LM
        LR 1,A
        CI 9
        BC SHL2
        LIS 9
        LR 1,A
SHL2:   XDC
        LIS 1
        LR 0,A
        PI LCOPY
        LDB GT+10               ; its price (24-bit)
        LR 0,A
        PI CATITEM
        LIS 3
        ADC
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A
        DCI N32
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        CLR
        ST
        PI DEC32
        DCI SCRL+11
        LIS 6
        LR 0,A
        PI NUMPUT
        LDB GT+10               ; its bundle: xN
        LR 0,A
        PI CATITEM
        LIS 2
        ADC
        LM
        LR 1,A
        DCI SCRL+18
        LI 'X'
        ST
        PI DIG2S
        LDB GT+10               ; what we own of it
        DCI ST+OFF_INV
        ADC
        LM
        NI 0FFH
        BZ SHL8
        CI 99
        BC SHL3
        LI 99
SHL3:   LR 1,A
        DCI SCRL+22
        PI DIG2S
SHL8:   LDB GT+8
        INC
        LR 1,A
        LI CV1
        LR 2,A
        PI LSHOW
        LEAVE

; SHLAST -- row 8: done and waiting, or no cash, or the keys; the timer.
SHLAST: ENTER
        PI LCLR
        LDB SMSG
        NI 0FFH
        DCI S_SHINT
        BZ SLA1
        DCI S_NOCASH
SLA1:   LR Q,DC
        LDB MYSEAT
        CI SEATS-1
        BNC SLA2
        LR 0,A
        LI P_FLAGS
        LR 1,A
        PI SEATF
        LM
        NI PF_DONE
        BZ SLA2
        DCI S_SHDONE
        LR Q,DC
SLA2:   LR DC,Q
        XDC
        CLR
        LR 0,A
        LI 20
        LR 1,A
        PI LCOPY
        LDB ST+OFF_TIMER
        LR 1,A
        CI 99
        BC SLA3
        LI 99
        LR 1,A
SLA3:   DCI SCRL+22
        PI DIG2S
        LIS 8
        LR 1,A
        LI CV1
        LR 2,A
        PI LSHOW
        LEAVE

; SHTICK -- the shop's keys: up/down (it scrolls), push buys, left sells
;   (a press each), START is done.
SHTICK: ENTER
        LDB MYSEAT              ; (watching, or done: nothing)
        CI SEATS-1
        BNC SHTX
        LR 0,A
        LI P_FLAGS
        LR 1,A
        PI SEATF
        LM
        NI PF_DONE
        BNZ SHTX
        LDB SN
        NI 0FFH
        BNZ SHT1
SHTX:   LEAVE
SHT1:   LDB INPDIR              ; (a fresh left: sell)
        LR 1,A
        LDB SPREV
        COM
        NS 1
        STB GT+5
        PI SMOVES
        LDB GT
        NI IN_UP+IN_DOWN
        BZ SHT4
        LDB SSEL
        LR 1,A
        LDB GT
        NI IN_UP
        BZ SHT2
        LR A,1
        NI 0FFH
        BZ SHT4
        AI -1
        BR SHT3
SHT2:   LR A,1
        INC
        LR 1,A
        LDB SN
        COM
        AS 1
        BC SHT4
        LR A,1
SHT3:   STB SSEL
        PI SSCROLL
        CLR
        STB SMSG
        SND UI_TICK
        PI SHOPDRAW
SHT4:   LDB SSEL                ; the item under the cursor
        DCI SHOPIDS
        ADC
        LM
        STB AARGS
        LIS 1
        STB AARGS+1
        LDB EVBTN               ; push: buy (cash for it?)
        NI 0FFH
        BZ SHT6
        PI AFFORD
        LR A,0
        NI 0FFH
        BNZ SHT5
        LIS 1
        STB SMSG
        SND UI_ERROR
        PI SHLAST
        BR SHT6
SHT5:   LIS ACT_BUY
        LR 1,A
        LIS 2
        LR 2,A
        PI ACTQ
        SND UI_SELECT
SHT6:   LDB GT+5                ; a fresh left: sell one (if owned)
        NI IN_LEFT
        BZ SHT7
        LDB AARGS
        DCI ST+OFF_INV
        ADC
        LM
        NI 0FFH
        BZ SHT7
        LIS ACT_SELL
        LR 1,A
        LIS 2
        LR 2,A
        PI ACTQ
        SND UI_TICK
SHT7:   LDB EVKEY               ; START: done
        CI KEY_END
        BNZ SHT8
        LIS ACT_DONE
        LR 1,A
        CLR
        LR 2,A
        PI ACTQ
        SND UI_GO
SHT8:   LEAVE

; AFFORD -- r0 = 1 if the cash covers item AARGS's price.
AFFORD: ENTER
        LDB AARGS
        LR 0,A
        PI CATITEM
        LIS 3
        ADC
        LM
        LR 4,A                  ; the price (24-bit), complemented ...
        LM
        LR 5,A
        LM
        LR 6,A
        LR A,4
        COM
        LR 4,A
        LR A,5
        COM
        LR 5,A
        LR A,6
        COM
        LR 6,A
        DCI ST+OFF_CASH         ; ... + the cash + 1: C when cash >= price
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A
        LM
        LR 3,A
        LI 0FFH
        LR 7,A                  ; (the price's top byte: 0, complemented)
        AI 1
        ADC3 0,4
        ADC3 1,5
        ADC3 2,6
        ADC3 3,7
        LIS 1
        BC AF1
        CLR
AF1:    LR 0,A
        LEAVE

; ---- a round's end, the game's ---------------------------------------------------
; RESULTS -- ROUND OVER (or GAME OVER); the winner; the seats and their wins.
RESULTS: ENTER
        PI LCLR
        LDB SHOWS
        CI S_OVER
        DCI S_ROVER
        BNZ RS1
        DCI S_GOVER
RS1:    LR Q,DC
        LR DC,Q
        XDC
        CLR
        LR 0,A
        LI TCOLS
        LR 1,A
        PI LCOPY
        CLR
        LR 1,A
        LI CV1
        LR 2,A
        PI LSHOW
        LI SBAND0               ; the band
        LR 0,A
        LI SBAND1
        LR 1,A
        LIS 2
        LR 2,A
        PI PALSET
        PI LCLR                 ; the winner
        LDB ST+OFF_RESULT
        CI SEATS-1
        BNC RS3
        LR 0,A
        CLR
        LR 2,A
        PI LNAME
        LDB SHOWS
        CI S_OVER
        DCI S_WROUND
        BNZ RS2
        DCI S_WGAME
RS2:    XDC
        LI 9
        LR 0,A
        LI 15
        LR 1,A
        PI LCOPY
        LDB ST+OFF_RESULT
        LR 0,A
        PI SEATINK
        LR 2,A
        LIS 2
        LR 1,A
        PI LSHOW
RS3:    CLR                     ; the seats: two a row, rows 4-8: name, wins
        STB GT+8
RS4:    LDB GT+8
        LR 0,A
        PI KIND
        CI KIND_EMPTY
        BZ RS6
        DCI SCRL                ; 12 cells
        LI 12
        LR 0,A
RS5:    LI ' '
        ST
        DS 0
        BNZ RS5
        CLR
        ST
        LDB GT+8
        LR 0,A
        CLR
        LR 2,A
        PI LNAME
        LDB GT+8                ; its wins
        LR 0,A
        LI P_WINS
        LR 1,A
        PI SEATF
        LM
        LR 1,A
        DCI SCRL+9
        PI DIG2S
        LDB GT+8                ; its place
        CI 4
        LR 1,A
        LIS 0
        BC RS5A
        LR A,1
        AI -5
        LR 1,A
        LI 12
RS5A:   LR 0,A
        LR A,1
        AI 4
        LR 1,A
        LDB GT+8
        LR 2,A
        LR A,0
        STB GT+9
        LR A,2
        LR 0,A
        PI SEATINK
        LR 2,A
        LDB GT+9
        LR 0,A
        PI TXAT
        DCI SCRL
        PI TXSTR
RS6:    LDB GT+8
        INC
        STB GT+8
        CI SEATS
        BZ RS7
        JMP RS4
RS7:    LEAVE

; ---- each pass --------------------------------------------------------------------
STICK:  ENTER
        LDB SHOWS
        CI S_WAIT
        BNZ STK1
        PI WTICK
        LEAVE
STK1:   CI S_SHOP
        BNZ STK2
        PI SHTICK
STK2:   LEAVE

S_ROOM:  DB "WAITING ROOM: ",0
S_OPEN:  DB "- OPEN -",0
S_WHOST: DB "PUSH START  HOLD SETUP",0
S_WWAIT: DB "THE HOST STARTS  HOLD",0
S_SETT:  DB "SETTINGS: ",0
S_SHOST: DB "<> CHANGE TWIST PAGE",0
S_SLOOK: DB "TWIST PAGE  HOLD BACK",0
S_SHOPR: DB "SHOP RND ",0
S_CASH:  DB "CASH",0
S_SHINT: DB "PUSH BUY <SELL START",0
S_NOCASH: DB "NOT ENOUGH CASH",0
S_SHDONE: DB "DONE: WAITING",0
S_ROVER: DB "ROUND OVER",0
S_GOVER: DB "GAME OVER",0
S_WROUND: DB "WINS THE ROUND",0
S_WGAME: DB "WINS THE GAME!",0
