; p_cat.inc -- the CATALOG (src/cat.c), in ROM: the server's names for the
; arsenal, the host's settings and the bots' tiers, as the server sends
; them, indexed at build time (tools/arc/mkcat_arc.py: cat.inc). The link
; drops the K frames; p_room.inc's CATOURS checks the state's OFF_CAT_LEN
; and OFF_CAT_SUM against this one (catequ.inc). M4's accessors (an item's
; name and price, a setting's label and value, a tier's name) go here,
; reached by FARPG, copying into TZ.

        BCTA,UN CINAME          ; E_INAME (E_ENTER): an item's name
        RETC,UN                 ; E_STATE
        NOP
        NOP
        RETC,UN                 ; E_TICK

        INCLUDE "cat.inc"

; CINAME: item T0's name in T1 cells at the display's next cells (DPUTC:
; R2, DCOLOR): its first T1 letters, then spaces (an item past the
; catalog's: all spaces). Letters A2G turns into glyphs ('-' and the like
; are spaces). Calls two down. Clobbers R0, R1, R3, DSTRH:DSTRL; advances R2.
CINAME: LODA,R3 T0
        COMI,R3 CNITEMS
        BCFR,LT CIN3
        LODZ    R3              ; its record (CIDX: DWBE each): kind, arms,
        RRL,R0                  ;  bundle, price24, the name's length, its
        STRZ    R3              ;  letters
        LODA,R0 CIDX,R3
        STRA,R0 DSTRH
        LODA,R0 CIDX+1,R3
        STRA,R0 DSTRL
        LODI,R3 6
        LODA,R0 *DSTRH,R3       ; its length, at most T1
        COMA,R0 T1
        BCTR,LT CIN1
        LODA,R0 T1
CIN1:   STRZ    R1
        LODA,R0 T1              ; T1: the spaces after
        SUBZ    R1
        STRA,R0 T1
        COMI,R1 0
        BCTR,EQ CIN3
CIN2:   LODA,R0 *DSTRH,R3,+
        ZBSR    *ZA2G
        ZBSR    *ZDPUTC
        BDRR,R1 CIN2
CIN3:   LODA,R1 T1
        BCTR,EQ CIN9
CIN4:   EORZ    R0
        ZBSR    *ZDPUTC
        BDRR,R1 CIN4
CIN9:   RETC,UN
