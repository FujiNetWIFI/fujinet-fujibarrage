; p_send.inc -- the link's sending side (p_net.inc's NPOST reaches it by
; FARPG when TXF wants frames sent): every frame TXF wants, in one WRITE.
; What is to be sent is kept as intents (TXF) and encoded only when a WRITE
; goes out, each frame straight from its fields (no buffer: cart RAM is
; short): GETB gives its byte i (type, payload, then the two Fletcher sums,
; worked out first), and COBSOUT streams its COBS into the transaction,
; then a 0. The Astrocade's (src/astrocade/src/p_net.inc POSTW..COBSOUT)
; in FujiBlast's 2650 form.

        BCTA,UN POSTW           ; E_ENTER: the WRITE posted (NKIND NK_WRITE)

; POSTW's scratch (T0-T3; NUMV, NUMD: DNUM's, never used under NSTEP)
CFROM   EQU     T0              ; COBSOUT: a run's start and end
CI      EQU     T1
BK      EQU     T2              ; the frame's TXF bit's number
RAWN    EQU     T3              ; its bytes before the sums (type, payload)
SUM1    EQU     NUMV            ; its Fletcher sums
SUM2    EQU     NUMD

; POSTW: every frame TXF wants, in one WRITE: its length first (each
; frame's raw length + 2: COBS adds one, then the 0), then the frames.
POSTW:  LODI,R1 0               ; the length
        LODI,R3 7
PW1:    LODA,R0 TXF
        ANDA,R0 BITS,R3
        BCTR,EQ PW2
        LODZ    R1
        ADDA,R0 FLEN,R3
        STRZ    R1
PW2:    SUBI,R3 1
        BCFR,LT PW1
        LODA,R0 TXF             ; the action's: its args' too
        TMI,R0  TX_ACT
        BCFR,EQ PW2A
        LODZ    R1
        ADDA,R0 ANARG
        STRZ    R1
PW2A:   STRA,R1 T1
        LODI,R3 NCWRITE
        LODI,R2 1
        BSTA,UN MSTARTN
        LODA,R1 T1
        LODI,R2 0
        BSTA,UN MPARW
        LODI,R0 0               ; each frame, in bit order
PW3:    STRA,R0 BK
        STRZ    R3
        LODA,R0 TXF
        ANDA,R0 BITS,R3
        BCTR,EQ PW4
        BSTA,UN PREP
        BSTA,UN COBSOUT
PW4:    LODA,R0 BK
        ADDI,R0 1
        COMI,R0 8
        BCFR,EQ PW3
        EORZ    R0
        STRA,R0 TXF
        LODI,R0 NK_WRITE        ; posted
        STRA,R0 NKIND
        EORZ    R0
        STRA,R0 NWAIT
        BCTA,UN MPOST           ; tail call

; the frames, by TXF bit: raw length + 2 (type, payload, 2 sums; COBS's 1;
; the 0), and the type; the action's length is its args' too (POSTW)
FLEN:   DB      1+30+2+2,1+2+2+2,1+3+2+2,1+1+2+2,1+0+2+2,1+8+2+2,1+0+2+2,1+0+2+2
FTYPES: DB      SC_HELLO,SC_ACT,SC_DONE,SC_CREDIT,SC_PING,SC_RESYNC,SC_ROOMS,SC_BYE
BITS:   DB      1,2,4,8,$10,$20,$40,$80

; PREP: frame BK about to go: what sending it does (D, a credit and RESYNC
; say what we've taken: the credit counts from it; RESYNC asks for the
; segment from its start), RAWN its bytes before the sums, SUM1 and SUM2
; its Fletcher-16. Calls one down. Clobbers R0-R3.
; (GETB's CC is not its byte's: callers test R0.)
PREP:   LODA,R3 BK
        COMI,R3 2               ; D, a credit, RESYNC: CREDITED = TAKEN
        BCTR,EQ PP1
        COMI,R3 3
        BCTR,EQ PP1
        COMI,R3 5
        BCFR,EQ PP2
        EORZ    R0              ; (RESYNC: the segment again from its start)
        STRA,R0 SEGLEN
PP1:    LODA,R0 TAKEN
        STRA,R0 CREDITED
PP2:    LODA,R0 FLEN,R3         ; its bytes: type, payload
        SUBI,R0 4
        COMI,R3 1
        BCFR,EQ PP3
        ADDA,R0 ANARG
PP3:    STRA,R0 RAWN
        LODI,R1 0               ; s1 += b, s2 += s1 (mod 255) over them
        LODI,R2 0
        LODI,R3 0
PS1:    COMA,R3 RAWN
        BCTR,EQ PS2
        BSTR,UN GETB
        ADDZ    R1
        BSTA,UN MOD255
        STRZ    R1
        ADDZ    R2
        BSTA,UN MOD255
        STRZ    R2
        ADDI,R3 1
        BCTR,UN PS1
PS2:    STRA,R1 SUM1
        STRA,R2 SUM2
        RETC,UN

; GETB: R0 = byte R3 of frame BK: its type, its payload from the fields it
; says, then (R3 = RAWN, RAWN + 1) its two sums. Leaf. Clobbers R0 only.
GETB:   COMA,R3 RAWN
        BCTR,LT GB1
        BCFR,EQ GB0
        LODA,R0 SUM1
        RETC,UN
GB0:    LODA,R0 SUM2
        RETC,UN
GB1:    LODA,R0 BK
        COMI,R3 0               ; 0: the type
        BCFR,EQ GB2
        STRZ    R3              ; (R3 0 again after)
        LODA,R0 FTYPES,R3
        LODI,R3 0
        RETC,UN
GB2:    COMI,R0 1
        BCTR,LT GBHEL
        BCFR,EQ GB3
        LODA,R0 ASEQ-1,R3       ; ACT: seq, act, its args
        RETC,UN
GB3:    COMI,R0 3
        BCTR,LT GBDONE
        BCFR,EQ GBRESY
        LODA,R0 TAKEN           ; a credit: what we've taken
        RETC,UN
GBDONE: COMI,R3 3               ; D: sid, seg, taken (credit starts again from it)
        BCTR,LT GBD1
        LODA,R0 TAKEN
        RETC,UN
GBD1:   LODA,R0 DSID-1,R3
        RETC,UN
        IF      DSEG <> DSID+1
        ERROR   "GETB: DSID, DSEG must be one run"
        ENDIF
GBHEL:  COMI,R3 1               ; HELLO: version, flags, the room's id, our
        BCFR,EQ GBH1            ;  name, our shape
        LODI,R0 PROTO_VERSION
        RETC,UN
GBH1:   EORZ    R0
        COMI,R3 2
        RETC,EQ
        COMI,R3 3+ROOM_LEN
        BCFR,LT GBH2
        LODA,R0 SRVTBL-3,R3
        RETC,UN
GBH2:   COMI,R3 3+ROOM_LEN+NAME_LEN
        BCFR,LT GBH3
        LODA,R0 MYNAME-(3+ROOM_LEN),R3
        RETC,UN
GBH3:   LODA,R0 HELLO1-(3+ROOM_LEN+NAME_LEN),R3
        RETC,UN
GBRESY: COMI,R0 5               ; (PING, ROOMS, BYE have no payload)
        RETC,GT
        COMI,R3 1               ; RESYNC: taken, tv, sid, seg, off16 0, cat16
        BCFR,EQ GBR1
        LODA,R0 TAKEN
        RETC,UN
GBR1:   COMI,R3 2
        BCFR,EQ GBR2
        LODA,R0 NTV
        RETC,UN
GBR2:   COMI,R3 5
        BCFR,LT GBR3
        LODA,R0 SEGSID-3,R3
        RETC,UN
        IF      SEGNO <> SEGSID+1
        ERROR   "GETB: SEGSID, SEGNO must be one run"
        ENDIF
GBR3:   EORZ    R0
        COMI,R3 7
        RETC,LT
        LODA,R0 ST+OFF_CAT_LEN  ; all of the catalog: ours is in ROM (the
        IORA,R0 ST+OFF_CAT_LEN+1 ;  server's length when it has said it)
        BCTR,EQ GBR4
        LODA,R0 ST+OFF_CAT_LEN-7,R3
        RETC,UN
GBR4:   LODI,R0 CATLEN&$FF
        COMI,R3 7
        RETC,EQ
        LODI,R0 CATLEN>>8
        RETC,UN

; our shape (HELLO bytes 18-29): width (LE), height, aspect, Hz, the tank's
; sprite, the script buffer (LE), colour objects a band, caps, platform
HELLO1: DB      FIELD_W,0,FIELD_H,PAR,60,TANK_W,TANK_H,SCRIPT_BUF,0,0,CAP_HOLD,PLAT_ARCADIA
HELLO1N EQU     $-HELLO1
        IF      1+2+ROOM_LEN+NAME_LEN+HELLO1N <> 1+HELLO_LEN
        ERROR   "HELLO's length"
        ENDIF

; MOD255: R0, just added (C its carry), mod 255: a carry is 1 (256 is 1
; mod 255), and 255 is 0. Leaf.
MOD255: TPSL    PSLC
        BCFR,EQ MOD1
        ADDI,R0 1
MOD1:   COMI,R0 255
        RETC,LT
        EORZ    R0
        RETC,UN

; COBSOUT: frame BK's RAWN + 2 bytes (GETB) as COBS into the TX stream,
; then a 0: each run up to a zero (or the end) is its length + 1, then its
; bytes. Calls one down. Clobbers R0, R1, R3.
COBSOUT: EORZ   R0
        STRA,R0 CFROM           ; the run's start
        LODI,R3 0
CO1:    LODA,R0 RAWN            ; the end?
        ADDI,R0 2
        COMZ    R3
        BCTR,EQ CO3
        BSTA,UN GETB
        COMI,R0 0
        BCTR,EQ CO2
        ADDI,R3 1
        BCTR,UN CO1
CO2:    BSTR,UN CORUN           ; a zero: the run, then on past it
        ADDI,R3 1
        STRA,R3 CFROM
        BCTR,UN CO1
CO3:    BSTR,UN CORUN           ; the end: the last run, then the 0
        LODI,R1 0
        LODA,R0 *PTXD,R1
        RETC,UN
CORUN:  STRA,R3 CI              ; length + 1, then bytes [CFROM, R3)
        LODZ    R3
        SUBA,R0 CFROM
        ADDI,R0 1
        STRZ    R1
        LODA,R0 *PTXD,R1
        LODA,R3 CFROM
CR1:    COMA,R3 CI
        BCTR,EQ CR2
        BSTA,UN GETB
        STRZ    R1
        LODA,R0 *PTXD,R1
        ADDI,R3 1
        BCTR,UN CR1
CR2:    LODA,R3 CI
        RETC,UN
