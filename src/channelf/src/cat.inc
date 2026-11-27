; ---------------------------------------------------------------------------
; cat.inc -- the CATALOG (docs/PROTOCOL.md), as K frames brought it into
; CATBUF, indexed once it is whole (src/cat.c cat_check). Resident.
;
;   version, items (48), settings (27), pages (n), the pages' names, then
;   items: kind, arms, bundle, price24, name; settings: page, type, label,
;   then (type 0) n, names, or (1) min16, step16, count, decimals; tiers:
;   n, names.  Names are length-prefixed.
;
; The index holds each record's address, big-endian (LM QU, LM QL).
; ---------------------------------------------------------------------------

; CATCHK -- A = 1 if the catalog is whole and indexed (CATOK), else 0.
CATCHK: ENTER
        LDB CATOK
        NI 0FFH
        BZ CC0
        LIS 1
        LEAVE
CC0:    DCI ST+OFF_CAT_LEN      ; its length (LE) said, and all of it here
        LM
        LR 0,A
        LM
        LR 1,A
        AS 0
        BZ CCNO0
        DCI CATN
        LM
        XS 0
        BNZ CCNO0
        LM
        XS 1
        BZ CC0A
CCNO0:  CLR
        LEAVE
CC0A:   DCI PAGEAT              ; the pages
        XDC
        DCI CATBUF+3
        LM
        CI 4
        BC CC1
        LIS 4
CC1:    LR 6,A
        NI 0FFH
        BZ CC2A
CC2:    PI CCMARK
        PI CCSKIP
        DS 6
        BNZ CC2
CC2A:   LR Q,DC                 ; (DC0 on: kept over the DCI)
        DCI ITEMAT              ; the items: 6 bytes, then the name
        XDC
        LR DC,Q
        LI N_ITEMS
        LR 6,A
CC3:    PI CCMARK
        LIS 6
        ADC
        PI CCSKIP
        DS 6
        BNZ CC3
        LR Q,DC
        DCI SETAT               ; the settings
        XDC
        LR DC,Q
        LI N_SETTINGS
        LR 6,A
CC4:    PI CCMARK
        LM                      ; page
        LM                      ; type
        LR 7,A
        PI CCSKIP               ; the label
        LR A,7
        NI 0FFH
        BNZ CC6
        LM                      ; a choice: n names
        LR 7,A
        NI 0FFH
        BZ CC7
CC5:    PI CCSKIP
        DS 7
        BNZ CC5
        BR CC7
CC6:    LIS 6                   ; a number: min16, step16, count, decimals
        ADC
CC7:    DS 6
        BNZ CC4
        LR Q,DC
        DCI TIERAT              ; the tiers
        XDC
        LR DC,Q
        LM
        CI 8
        BC CC8
        LIS 8
CC8:    STB NTIERS              ; (STB: DC0 moves: kept)
        LR DC,Q                 ; past the count
        LM
        LR Q,DC
        LDB NTIERS
        LR 6,A
        LR DC,Q
        NI 0FFH
        BZ CC10
CC9:    PI CCMARK
        PI CCSKIP
        DS 6
        BNZ CC9
CC10:   LIS 1
        STB CATOK
        LEAVE
CCNO:   CLR
        LEAVE

; CCMARK -- DC0 stored at DC1 (big-endian), DC1 on.  Leaf.  Keeps DC0.
CCMARK: LR Q,DC
        XDC
        LR A,QU
        ST
        LR A,QL
        ST
        XDC
        POP

; CCSKIP -- DC0 past the length-prefixed string there.  Leaf.
CCSKIP: LM
        ADCU
        POP

; CATITEM -- DC0 = item r0's record (kind, arms, bundle, price24, name).
;   Leaf.  Clobbers A, Q.
CATITEM: LR A,0
        SL 1
        DCI ITEMAT
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        POP

; SEATF -- DC0 = seat r0's field r1: ST + OFF_SEATS + 25 r0 + r1.  Leaf.
;   Clobbers A, r2.
SEATF:  LR A,0
        SL 1
        SL 1
        SL 1
        LR 2,A
        SL 1
        AS 2
        AS 0
        AS 1
        DCI ST+OFF_SEATS
        ADCU
        POP
