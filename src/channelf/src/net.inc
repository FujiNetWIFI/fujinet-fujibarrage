; ---------------------------------------------------------------------------
; net.inc -- the link (src/net.c over the cart's mailbox): one step a pass
; that never waits. FujiBlast's src/channelf net.inc (the Astrocade's
; p_net.inc, transcribed), with FujiBarrage's frames. Resident.
;
; The FujiNet's TCP socket is driven one transaction at a time, each posted
; in one pass and seen answered in a later one. The order: a WRITE of
; whatever is to be sent; else, with bytes known waiting (NBW), a READ of
; them, up to RD_MAX; else a STATUS (the bytes waiting; is the socket still
; up). A READ never asks for more than is waiting: firmware before
; afe7dddaa drops a short read's bytes.
;
; What is to be sent is kept as intents (TXF: HELLO, an action, a D, a
; credit...) and encoded only when a WRITE goes out: each frame built in
; RAWBUF (type, payload, Fletcher-16), its COBS stored into the
; transaction, then a 0.
;
; The server's frames are decoded in two steps: a chunk of the READ's bytes
; a pass is COBS-decoded into FBUF; at a frame's 0 its sums are checked and
; only then is it applied (FREND): a patch's runs into ST (Q), a commit (C),
; a room record (T), an error (E), the catalog (K into CATBUF), TERRAIN (G:
; the columns into the field, fld.inc), a segment (S into SCRIPT; whole at
; S_LAST_FRAME: EV_SCRIPT). A bad frame asks for everything again (R).
; ---------------------------------------------------------------------------

PING_EVERY EQU 300              ; jiffies between keepalives (each is answered)
DEAD_AFTER EQU 900              ; jiffies without a frame: the link is gone
ASK_AGAIN  EQU 60               ; HELLO unanswered: again
RESYNC_GAP EQU 30               ; jiffies between RESYNCs
NTIMEOUT   EQU 240              ; passes a transaction may be out
JOIN_WAIT  EQU 600              ; jiffies at a table with no state: no answer
RD_MAX     EQU 192              ; the most a READ asks for
DCHUNK     EQU 32               ; encoded bytes decoded a pass
FBMAX      EQU MAX_FRAME+4      ; FBUF: a decoded frame

NK_READ  EQU 1                  ; NKIND: the transaction out
NK_STATUS EQU 2
NK_WRITE EQU 3
NL_LINK  EQU 1                  ; NLINK: the socket is open
NL_JOINED EQU 2                 ;   HELLO sent: at a table
NL_HEARD EQU 4                  ;   a commit came since
EV_STATE EQU 1                  ; NEVT: what this pass's step brought
EV_ERROR EQU 2
EV_LOST  EQU 4
EV_NOANS EQU 8                  ;   no state in JOIN_WAIT jiffies
EV_SCRIPT EQU 10H               ;   a segment is whole in SCRIPT (SEGRDY)
EV_TERR  EQU 20H                ;   a TERRAIN frame changed the field
TX_HELLO EQU 01H                ; TXF: what the next WRITE carries
TX_ACT   EQU 02H
TX_DONE  EQU 04H
TX_CREDIT EQU 08H
TX_PING  EQU 10H
TX_RESYNC EQU 20H
TX_ROOMS EQU 40H
TX_BYE   EQU 80H
MAX_TABLES EQU MAX_ROOMS

; ---------------------------------------------------------------------------
; NINIT -- a fresh link: nothing out, owed or half-decoded.  Leaf.
;   Clobbers A, r0, DC0.
; ---------------------------------------------------------------------------
NINIT:  DCI NZERO
        LI NZEND-NZERO
        LR 0,A
NI1:    CLR
        ST
        DS 0
        BNZ NI1
        LI 0FFH
        STB TBLN
        POP

; NHELLO -- sit at the table: the link's counters, ST zeroed (the state comes
;   whole), HELLO due, no seat yet.  Clobbers A, r0, DC0.
NHELLO: ENTER
        PI NINIT
        DCI ST
        LI (STATE_SIZE+255)>>8
        LR 1,A
        LI STATE_SIZE&0FFH
        LR 0,A
NH1:    CLR
        ST
        DS 0
        BNZ NH1
        DS 1
        BNZ NH1
        LDB NLINK
        NI NL_LINK
        OI NL_JOINED
        STB NLINK
        LIS TX_HELLO
        STB TXF
        LI 0FFH
        STB MYSEAT
        STB NTV                 ; (the field: whatever comes first, whole)
        CLR
        STB JOINT
        STB JOINT+1
        LEAVE

; ---------------------------------------------------------------------------
; NSTEP -- a pass of the link: the transaction out answered, a chunk
;   decoded, the next posted, the clocks (DT jiffies); our seat found at a
;   commit; no state in JOIN_WAIT jiffies is EV_NOANS.  NEVT: what came.
; ---------------------------------------------------------------------------
NSTEP:  ENTER
        CLR
        STB NEVT
        LDB FLAGS               ; a test scene: nothing comes or goes (its
        NI FL_TEST              ;  TESTEV: what a rig pokes, once)
        BZ NS0
        CLR
        STB TXF
        LDB TESTEV
        STB NEVT
        CLR
        STB TESTEV
        LEAVE
NS0:    LDB NLINK
        NI NL_LINK
        BNZ NS1
        LIS EV_LOST
        STB NEVT
        LEAVE
NS1:    PI NTXN                 ; 1. the transaction out
        LDB NEVT
        NI 0FFH
        BZ NS1A
        LEAVE                   ; (lost)
NS1A:   LDB DLEFT               ; 2. a chunk decoded
        NI 0FFH
        BZ NS2
        PI DECODE
        LDB NEVT
        NI EV_ERROR
        BZ NS2
        LEAVE                   ; (an E frame: the caller's)
NS2:    PI NPOST                ; 3. the next transaction
        PI NTICK                ; 4. the clocks
        LDB NEVT                ; a commit: which seat is ours
        NI EV_STATE
        BZ NS3
        LDB ST+OFF_MY_SEAT
        STB MYSEAT
NS3:    LDB NLINK               ; joined, nothing heard: JOIN_WAIT
        NI NL_JOINED+NL_HEARD
        CI NL_JOINED
        BNZ NS9
        LDB DT
        LR 0,A
        DCI JOINT
        LM
        AS 0
        LR 1,A
        LM
        LNK
        LR 2,A
        DCI JOINT
        LR A,1
        ST
        LR A,2
        ST
        LR A,1                  ; JOINT >= JOIN_WAIT: no answer (low: C
        AI 256-(JOIN_WAIT&0FFH) ;  when no borrow; high: C when r2 + C >
        LR A,2                  ;  JOIN_WAIT's high)
        LNK
        AI 255-(JOIN_WAIT>>8)
        BNC NS9
        LDB NEVT
        OI EV_NOANS
        STB NEVT
NS9:    LEAVE

; ---------------------------------------------------------------------------
; NTXN -- 1. the transaction out: answered?  Its answer taken (a READ's
;   bytes to decode, a STATUS's bytes waiting and its word on the socket).
; ---------------------------------------------------------------------------
NTXN:   LDB NKIND               ; (nothing out: out before ENTER)
        NI 0FFH
        BNZ NT0
        POP
NT0:    ENTER
        INCB NWAIT
        CI NTIMEOUT
        BZ NLOST
        PI FNDONE
        BZ NT1
        LEAVE                   ; still out
NT1:    LDB NKIND
        LR 3,A
        CLR
        STB NKIND
        PI FNACK
        BNZ NT9                 ; a NAK: nothing
        LR A,3
        CI NK_READ
        BNZ NT2
        LDB FNRXLO              ; a READ's bytes: decode them
        STB DLEFT
        CLR
        STB DRPOS
        LEAVE
NT2:    CI NK_STATUS
        BZ NT3
        LEAVE
NT3:    DCI FNREPLY             ; STATUS: the bytes waiting, to be read
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A                  ; connected
        DCI NBW
        LR A,0
        ST
        LR A,1
        ST
        LR A,0                  ; some waiting
        NI 0FFH
        BNZ NTX
        LR A,1
        NI 0FFH
        BNZ NTX
        LR A,2                  ; none waiting, but connected
        NI 0FFH
        BNZ NTX
        BR NLOST
NT9:    CLR                     ; (what is waiting asked again: a STATUS)
        STB NBW
        STB NBW+1
NTX:    LEAVE
NLOST:  CLR
        STB NLINK
        LDB NEVT
        OI EV_LOST
        STB NEVT
        LEAVE

; ---------------------------------------------------------------------------
; NPOST -- 3. the next transaction, once nothing is out and the last READ
;   is decoded: a WRITE, else a READ of what is waiting (RD_MAX at most),
;   else a STATUS.
; ---------------------------------------------------------------------------
NPOST:  ENTER
        LDB NKIND
        NI 0FFH
        BNZ NPQ
        LDB DLEFT
        NI 0FFH
        BZ NP0
NPQ:    LEAVE
NP0:
        LDB TXF
        NI 0FFH
        BZ NP1
        PI POSTW
        LEAVE
NP1:    DCI NBW                 ; r4:r5 waiting
        LM
        LR 4,A
        LM
        LR 5,A
        NI 0FFH
        BNZ NPR
        LR A,4
        NI 0FFH
        BNZ NPR
        LI FD_NET               ; none known waiting: STATUS
        LR 0,A
        LI NC_STAT
        LR 1,A
        LIS 2
        LR 2,A
        PI FNBEG
        LIS 1                   ; two 1-byte parameters, 0
        ST
        CLR
        ST
        LIS 1
        ST
        CLR
        ST
        LIS NK_STATUS
        BR NPGO
NPR:    LR A,5                  ; READ what is waiting, up to RD_MAX
        NI 0FFH
        BNZ NPR1
        LR A,4
        CI RD_MAX
        BNC NPR1
        LR 6,A                  ; all of it
        CLR
        LR 4,A
        BR NPR2
NPR1:   LI RD_MAX               ; RD_MAX of it, the rest kept
        LR 6,A
        LR A,4
        AI 256-RD_MAX
        LR 4,A
        LR A,5
        LNK
        AI 255
        LR 5,A
NPR2:   DCI NBW
        LR A,4
        ST
        LR A,5
        ST
        LI FD_NET
        LR 0,A
        LI NC_READ
        LR 1,A
        LIS 1
        LR 2,A
        PI FNBEG
        LIS 2                   ; a 2-byte parameter: the length
        ST
        LR A,6
        ST
        CLR
        ST
        LIS NK_READ
NPGO:   STB NKIND               ; posted: kind A
        CLR
        STB NWAIT
        PI FNPOST
NPX:    LEAVE

; ---------------------------------------------------------------------------
; POSTW -- every frame TXF wants, in one WRITE: its length first (each
;   frame's raw length + 2: COBS adds one, then the 0), then the frames.
; ---------------------------------------------------------------------------
POSTW:  ENTER
        LDB TXF                 ; the length
        LR 3,A
        CLR
        LR 4,A
        DCI FLEN
        LIS 8
        LR 5,A
PW1:    LR A,3
        NI 1
        LM
        BZ PW2
        AS 4
        LR 4,A
PW2:    LR A,3
        SR 1
        LR 3,A
        DS 5
        BNZ PW1
        LDB TXF                 ; the action's: its args' too
        NI TX_ACT
        BZ PW2A
        LDB ANARG
        AS 4
        LR 4,A
PW2A:   LI FD_NET
        LR 0,A
        LI NC_WRIT
        LR 1,A
        LIS 1
        LR 2,A
        PI FNBEG
        LIS 2
        ST
        LR A,4
        ST
        CLR
        ST
        CLR                     ; each frame, in bit order
        STB BK
PW3:    LDB BK
        DCI SLBIT
        ADC
        LM
        LR 3,A
        LDB TXF
        NS 3
        BZ PW4
        PI BUILD
        PI COBSOUT
PW4:    INCB BK
        CI 8
        BNZ PW3
        CLR
        STB TXF
        LIS NK_WRITE
        STB NKIND
        CLR
        STB NWAIT
        PI FNPOST
        LEAVE

; the frames, by TXF bit: raw length + 2 (type, payload, 2 sums; COBS's 1;
; the 0), and the type; an action's args are ANARG more (POSTW)
FLEN:   DB 1+30+2+2, 1+2+2+2, 1+3+2+2, 1+1+2+2, 1+0+2+2, 1+8+2+2, 1+0+2+2, 1+0+2+2
FTYPES: DB SC_HELLO, SC_ACT, SC_DONE, SC_CREDIT, SC_PING, SC_RESYNC, SC_ROOMS, SC_BYE

; BUILD -- frame BK (a TXF bit's number) into RAWBUF: its type, its
;   payload, then the two Fletcher sums; RAWN its length.  Leaf.
;   Clobbers A, r0-r3, DCs.
BUILD:  LDB BK
        LR 3,A
        DCI FTYPES
        ADC
        LM
        DCI RAWBUF
        ST
        LR A,3
        NI 0FFH
        BZ BHELLO
        CI 1
        BZ BACT
        CI 2
        BZ BDONE
        CI 3
        BZ BCRED
        CI 5
        BNZ BSUMJ               ; PING, ROOMS, BYE: no payload
        JMP BRESY
BSUMJ:  JMP BSUM
BHELLO: LI PROTO_VERSION        ; version, flags, the room's id, our name,
        ST                      ;  our shape
        CLR
        ST
        XDC
        DCI SRVTBL
        LI ROOM_LEN
        LR 0,A
BH1:    LM
        XDC
        ST
        XDC
        DS 0
        BNZ BH1
        DCI MYNAME
        LI NAME_LEN
        LR 0,A
BH2:    LM
        XDC
        ST
        XDC
        DS 0
        BNZ BH2
        DCI HELLO1
        LI HELLO1N
        LR 0,A
BH3:    LM
        XDC
        ST
        XDC
        DS 0
        BNZ BH3
        XDC
        JMP BSUM
BACT:   XDC                     ; seq, act, its args
        DCI ASEQ
        LDB ANARG               ; (DCI moved DC0: start again)
        INC
        INC
        LR 0,A
        DCI ASEQ
BA1:    LM
        XDC
        ST
        XDC
        DS 0
        BNZ BA1
        XDC
        BR BSUM
BDONE:  LDB DSID                ; D: sid, seg, taken (credit again from it)
        LR 0,A
        LDB DSEG
        LR 1,A
        LDB TAKEN
        LR 2,A
        STB CREDITED
        DCI RAWBUF+1
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        BR BSUM
BCRED:  LDB TAKEN               ; what we've taken (and credited with it)
        STB CREDITED
        DCI RAWBUF+1
        ST
        BR BSUM
BRESY:  LDB TAKEN               ; R: taken, tv, sid, seg, off16 0, cat16
        STB CREDITED
        DCI RAWBUF+1
        ST
        LDB GMID                ; (a version half applied: FF)
        NI 0FFH
        LI 0FFH
        BNZ BRS1
        LDB NTV
BRS1:   DCI RAWBUF+2
        ST
        LDB SEGSID
        DCI RAWBUF+3
        ST
        LDB SEGNO
        DCI RAWBUF+4
        ST
        CLR                     ; (the segment again from its start)
        ST
        ST
        STB SEGRDY
        LDB CATN
        DCI RAWBUF+7
        ST
        LDB CATN+1
        DCI RAWBUF+8
        ST
BSUM:   LR Q,DC                 ; Fletcher-16 over RAWBUF up to DC0
        LR A,QL
        AI (100H-(RAWBUF&0FFH))&0FFH
        LR 2,A                  ; its length
        INC
        INC
        STB RAWN                ; and the sums
        DCI RAWBUF
        CLR
        LR 0,A                  ; s1
        LR 1,A                  ; s2
BS1:    LR A,0
        AM
        LNK                     ; mod 255: a carry is 1 ...
        CI 0FEH
        BC BS2
        AI 1                    ; ... and 255 is 0
BS2:    LR 0,A
        AS 1
        LNK
        CI 0FEH
        BC BS3
        AI 1
BS3:    LR 1,A
        DS 2
        BNZ BS1
        LR A,0
        ST
        LR A,1
        ST
        POP

; our shape (HELLO bytes 18-29): width (LE), height, aspect, Hz, the tank's
; sprite, the script buffer (LE), colour objects a band, caps, platform
HELLO1: DB FW, 0, FH, PAR, 60, TANK_W, TANK_H, SCRIPT_BUF&0FFH, SCRIPT_BUF>>8, 0
        DB CAP_HOLD, PLAT_CHANNELF
HELLO1N EQU $-HELLO1
        IF 2+ROOM_LEN+NAME_LEN+HELLO1N <> HELLO_LEN
        ERROR "HELLO's length"
        ENDIF

; COBSOUT -- RAWBUF's RAWN bytes as COBS into the TX, then a 0: each run up
;   to a zero (or the end) is its length + 1, then its bytes.  Leaf.
;   Clobbers A, r0-r4, Q, DCs.
COBSOUT: LDB RAWN
        LR 2,A                  ; bytes left
        DCI RAWBUF
        XDC                     ; DC1: the run's start
        DCI FNTX                ; DC0: the TX (it appends)
CO1:    XDC                     ; a run: up to a zero or the end
        LR Q,DC
        XDC
        CLR
        LR 3,A                  ; its length
CO2:    LR A,2
        NI 0FFH
        BZ CO3
        XDC
        LM
        XDC
        NI 0FFH
        BZ CO3
        LR A,3
        INC
        LR 3,A
        DS 2
        BR CO2
CO3:    LR A,3                  ; its code: length + 1
        INC
        ST
        XDC                     ; back to the run's start
        LR DC,Q
        XDC
        LR A,3
        NI 0FFH
        BZ CO5
CO4:    XDC
        LM
        XDC
        ST
        DS 3
        BNZ CO4
CO5:    LR A,2                  ; the end: the frame's 0
        NI 0FFH
        BZ CO6
        XDC                     ; a zero: past it, the next run
        LM
        XDC
        DS 2
        BR CO1
CO6:    CLR
        ST
        POP

; ---------------------------------------------------------------------------
; NTICK -- 4. the clocks, by DT: a RESYNC owed, a credit, the keepalive,
;   HELLO again, a silent link given up.
; ---------------------------------------------------------------------------
NTICK:  ENTER
        LDB DT
        LR 6,A
        LDB RHOLD               ; a RESYNC owed, once the gap is over
        LR 0,A
        NI 07FH
        BZ NTK2
        LR A,0                  ; (the gap counts passes)
        AI -1
        STB RHOLD
        BR NTK3
NTK2:   LR A,0
        NI 080H
        BZ NTK3
        LI RESYNC_GAP
        STB RHOLD
        LDB TXF
        OI TX_RESYNC
        STB TXF
NTK3:   LDB CREDITED            ; taken - credited >= 48: a credit
        COM
        INC
        LR 0,A
        LDB TAKEN
        AS 0
        CI 47
        BC NTK4
        LDB TXF
        OI TX_CREDIT
        STB TXF
NTK4:   DCI QUIET               ; quiet += DT; > DEAD_AFTER: gone
        LM
        AS 6
        LR 0,A
        LM
        LNK
        LR 1,A
        DCI QUIET
        LR A,0
        ST
        LR A,1
        ST
        LR A,0
        AI 256-((DEAD_AFTER+1)&0FFH)
        LR A,1
        LNK
        AI 255-((DEAD_AFTER+1)>>8)
        BNC NTK5
        JMP NLOSTT
NTK5:   DCI QPING               ; every PING_EVERY: a credit and a PING
        LM
        AS 6
        LR 0,A
        LM
        LNK
        LR 1,A
        LR A,0
        AI 256-(PING_EVERY&0FFH)
        LR 2,A
        LR A,1
        LNK
        AI 255-(PING_EVERY>>8)
        BNC NTK6
        CLR                     ; (reached: again from 0)
        LR 0,A
        LR 1,A
        LDB TXF
        OI TX_CREDIT+TX_PING
        STB TXF
NTK6:   DCI QPING
        LR A,0
        ST
        LR A,1
        ST
        LDB QASK                ; every ASK_AGAIN: HELLO, if unanswered
        AS 6
        STB QASK
        CI ASK_AGAIN-1
        BC NTK9
        CLR
        STB QASK
        LDB NLINK
        NI NL_JOINED+NL_HEARD
        CI NL_JOINED
        BNZ NTK9
        LDB TXF
        OI TX_HELLO
        STB TXF
NTK9:   LEAVE
NLOSTT: CLR
        STB NLINK
        LDB NEVT
        OI EV_LOST
        STB NEVT
        LEAVE

; ---------------------------------------------------------------------------
; DECODE -- up to DCHUNK of the READ's bytes COBS-decoded into FBUF; at a
;   frame's 0, FREND applies it.  TAKEN counts the bytes.
;   In the loop: r7 the chunk, r0 DCODE (the block's data bytes left), r1
;   FBN (FBUF's length), r2 DFLAG; DC1 the reply, DC0 FBUF.
; ---------------------------------------------------------------------------
DF_BLK  EQU 1                   ; DFLAG: a COBS block began (a zero before the next)
DF_255  EQU 2                   ;  a 254-byte one (no zero after it)
DECODE: ENTER
        LDB DLEFT               ; this chunk: DCHUNK at most
        CI DCHUNK
        BC DE1
        LI DCHUNK
DE1:    LR 7,A
        COM
        INC
        LR 0,A
        LDB DLEFT
        AS 0
        STB DLEFT
        LDB TAKEN               ; taken += the chunk
        AS 7
        STB TAKEN
        LDB DRPOS               ; DC1 = the reply + DRPOS
        DCI FNREPLY
        ADCU
        XDC
        LDB DRPOS
        AS 7
        STB DRPOS
DE2:    DCI DCODE               ; the decoder's state
        LM
        LR 0,A
        LM
        LR 1,A                  ; FBN
        LM
        LR 2,A                  ; DFLAG
        DCI FBUF
        LR A,1
        ADC
DCL:    XDC
        LM                      ; the next byte
        XDC
        NI 0FFH
        BZ DCZERO               ; 0: a frame's end
        LR 3,A
        LR A,0
        NI 0FFH
        BZ DCCODE
        DS 0                    ; a data byte
        LR A,3
DCEM:   LR 3,A                  ; emit A into FBUF (past FBMAX: dropped,
        LR A,1                  ;  and the frame will fail its sums)
        CI FBMAX-1
        BNC DCN
        LR A,3
        ST
        LR A,1
        INC
        LR 1,A
DCN:    DS 7
        BNZ DCL
DCOUT:  DCI DCODE               ; the state kept for the next chunk
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        LEAVE
DCCODE: LR A,3                  ; a block's code: its data bytes
        AI -1
        LR 0,A
        LR A,2                  ; the zero before it, unless it's the frame's
        LR 4,A                  ;  first or after a 254-byte one
        LR A,3
        CI 0FFH
        LIS DF_BLK
        BNZ DCC1
        LIS DF_BLK+DF_255
DCC1:   LR 2,A
        LR A,4
        CI DF_BLK
        BNZ DCN
        CLR
        BR DCEM
DCZERO: LR A,1                  ; a frame's end: applied, the state reset
        STB FBN
        XDC
        LR Q,DC
        LR A,QU                 ; (the reply's place, across FREND)
        STB DRP2
        LR A,QL
        STB DRP2+1
        LR A,7
        STB DRP2+2
        PI FREND
        DCI DRP2
        LM
        LR QU,A
        LM
        LR QL,A
        LM
        LR 7,A
        LR DC,Q
        XDC
        CLR
        LR 0,A
        LR 1,A
        LR 2,A
        DCI FBUF
        LDB NEVT                ; an error: stop
        NI EV_ERROR
        BNZ DCERR
        DCI FBUF
        BR DCN
DCERR:  CLR
        STB DLEFT
        BR DCOUT

; ---------------------------------------------------------------------------
; FREND -- a frame in FBUF (FBN bytes): its sums checked, then what its type
;   does; a bad one asks for everything again.
; ---------------------------------------------------------------------------
FREND:  ENTER
        LDB FBN
        CI 2
        JC FEBAD                ; the sums and nothing else: malformed
        AI -2
        LR 2,A                  ; the bytes summed
        STB FBN
        DCI FBUF
        CLR
        LR 0,A
        LR 1,A
FE1:    LR A,0                  ; s1 += b, s2 += s1 (mod 255)
        AM
        LNK
        CI 0FEH
        BC FE2
        AI 1
FE2:    LR 0,A
        AS 1
        LNK
        CI 0FEH
        BC FE3
        AI 1
FE3:    LR 1,A
        DS 2
        BNZ FE1
        LR A,0                  ; against the two that follow
        CM
        BNZ FEBADJ
        LR A,1
        CM
        BZ FEOK
FEBADJ: JMP FEBAD
FEOK:   CLR                     ; a good frame: the link is alive
        STB QUIET
        STB QUIET+1
        STB QPING
        STB QPING+1
        STB QASK
        LDB FBUF                ; its type
        CI CS_PATCH
        BNZ FE4
        PI FPATCH
        LEAVE
FE4:    CI CS_COMMIT
        BNZ FE5
        LDB NLINK               ; a commit: the state is whole
        NI NL_JOINED
        BZ FEX
        LDB NLINK
        OI NL_HEARD
        STB NLINK
        LDB NEVT
        OI EV_STATE
        STB NEVT
        LEAVE
FE5:    CI CS_ERROR
        BNZ FE6
        LDB FBUF+1
        STB NERR
        LDB NEVT
        OI EV_ERROR
        STB NEVT
        LEAVE
FE6:    CI CS_ROOM
        BNZ FE7
        PI FTABLE
        LEAVE
FE7:    CI CS_TERRAIN
        BNZ FE8
        PI FTERR
        LEAVE
FE8:    CI CS_SCRIPT
        BNZ FE9
        PI FSCRIPT
        LEAVE
FE9:    CI CS_CATALOG
        BNZ FEX
        PI FCAT
FEX:    LEAVE
FEBAD:  LDB RHOLD               ; a RESYNC owed
        OI 080H
        STB RHOLD
        INCB NBAD               ; (counted, for the rigs)
        BNZ FEB1
        LI 0FFH
        STB NBAD
FEB1:   LEAVE

; FPATCH -- a patch's runs (offset LE, length, bytes) written into ST.  A
;   run past the state or the frame ends it.  Leaf.
;   Clobbers A, r0-r3, r6, r8, H, Q, DCs.
FPATCH: DCI FBUF+1
        XDC                     ; DC1: the frame
        LDB FBN
        AI -1
        LR 8,A                  ; its bytes left after the type
FP1:    LR A,8
        CI 2
        BC FPX                  ; (a run needs three)
        XDC
        LM
        LR 11,A                 ; the offset: H = its 16 bits
        LM
        LR 10,A
        LM
        LR 6,A                  ; the length
        XDC
        LR A,8
        AI -3
        LR 8,A
        LR A,6                  ; within the frame ...
        NI 0FFH
        BZ FP1
        COM
        INC
        AS 8
        BNC FPX                 ; (more than the frame has)
        LR 8,A
        LR A,6                  ; ... and the state: offset + length <= STATE_SIZE
        AS 11
        LR 0,A
        LR A,10
        LNK
        LR 1,A                  ; r1:r0 = the end
        LR A,0
        AI 256-((STATE_SIZE+1)&0FFH)
        LR A,1
        LNK
        AI 255-((STATE_SIZE+1)>>8)
        BC FPX                  ; (end > STATE_SIZE)
        DCI ST                  ; DC0 = ST + offset
        LR Q,DC
        LR A,QL
        AS 11
        LR QL,A
        LR A,QU
        LNK
        AS 10
        LR QU,A
        LR DC,Q
FP2:    XDC
        LM
        XDC
        ST
        DS 6
        BNZ FP2
        BR FP1
FPX:    POP

; FTABLE -- a room's record (index, count, ROOM_REC bytes) into TBLS; TBLN
;   the count (MAX_TABLES at most), TBLHAVE its bit.  Leaf.
;   Clobbers A, r0-r2, DCs.
FTABLE: LDB FBUF+2              ; the count
        CI MAX_TABLES
        BC FTB1
        LIS MAX_TABLES
FTB1:   STB TBLN
        LDB FBUF+1              ; the index
        CI MAX_TABLES-1
        BNC FTBX
        LR 0,A
        DCI SLBIT
        ADC
        LM
        LR 1,A
        LDB TBLHAVE             ; (no OR with a register: add it if clear)
        NS 1
        BNZ FTB2
        LDB TBLHAVE
        AS 1
        STB TBLHAVE
FTB2:   LR A,0                  ; TBLS + 24 index
        SL 1
        SL 1
        SL 1
        LR 1,A
        SL 1
        AS 1                    ; 24 index (< 192)
        DCI TBLS
        ADCU
        XDC
        DCI FBUF+3
        LI ROOM_REC
        LR 2,A
FTB3:   LM
        XDC
        ST
        XDC
        DS 2
        BNZ FTB3
FTBX:   POP
SLBIT:  DB 1, 2, 4, 8, 10H, 20H, 40H, 80H

; FCAT -- a catalog frame (offset LE, bytes) into CATBUF, if it follows
;   what we hold whole (CATN; R asks from there).  Leaf.
;   Clobbers A, r0-r3, H, Q, DCs.
FCAT:   LDB FBN                 ; its bytes: FBN - 3
        AI -3
        BZ FCX
        BM FCX
        LR 3,A
        DCI FBUF+1              ; the offset must be CATN
        LM
        LR 0,A
        LM
        LR 1,A
        DCI CATN
        LM
        XS 0
        BNZ FCX
        LM
        XS 1
        BNZ FCX
        LR A,3                  ; and the end within CATBUF
        AS 0
        LR 2,A
        LR A,1
        LNK
        LR 10,A                 ; (the end's high byte)
        LR A,2
        AI 256-((CAT_MAX+1)&0FFH)
        LR A,10
        LNK
        AI 255-((CAT_MAX+1)>>8)
        BC FCX
        DCI CATN                ; CATN = the end
        LR A,2
        ST
        LR A,10
        ST
        DCI CATBUF              ; DC0 = CATBUF + offset
        LR Q,DC
        LR A,QL
        AS 0
        LR QL,A
        LR A,QU
        LNK
        AS 1
        LR QU,A
        LR DC,Q
        XDC
        DCI FBUF+3
        XDC
FC1:    XDC
        LM
        XDC
        ST
        DS 3
        BNZ FC1
FCX:    POP

; FTERR -- a TERRAIN frame (base tv, new tv, flags, x LE, column entries):
;   applied when its base is FF (the whole field) or our NTV; then, at
;   G_LAST, NTV = its new tv (GMID: a version half applied, which a RESYNC
;   reports as FF).  Else NTV = FF and a RESYNC is owed (the server sends
;   the field whole).  EV_TERR.
FTERR:  ENTER
        LDB FBUF+1              ; the base
        CI 0FFH
        BZ FTE1
        LR 0,A
        LDB NTV
        XS 0
        BZ FTE1
        LI 0FFH                 ; not ours: the whole field again
        STB NTV
        LDB RHOLD
        OI 080H
        STB RHOLD
        LEAVE
FTE1:   LIS 1                   ; (mid-version until G_LAST)
        STB GMID
        LDB FBUF+4              ; the first column (x < FW)
        LR 5,A
        LDB FBN                 ; its entries' bytes: FBN - 6
        AI -6
        STB DS0+11
        DCI FBUF+6
        XDC
FTE2:   LDB DS0+11              ; (each entry 1 byte, or 1 + 2n)
        NI 0FFH
        BZ FTE5
        BM FTE5
        LR A,5
        CI FW-1
        BNC FTE5
        XDC                     ; this entry's size
        LR Q,DC
        LM
        XDC
        CI 0F0H
        BC FTE3                 ; (00-F0: one byte)
        AI 010H                 ; F0 + n: 1 + 2n
        SL 1
        INC
        BR FTE4
FTE3:   LIS 1
FTE4:   LR 0,A
        LDB DS0+11
        COM
        INC
        AS 0
        COM
        INC
        STB DS0+11              ; left -= the entry's size
        XDC
        LR DC,Q
        XDC
        PI FCOLENT              ; the column (keeps r5, DC1 past it)
        LR A,5
        INC
        LR 5,A
        BR FTE2
FTE5:   LDB FBUF+3              ; the version whole: ours
        NI G_LAST
        BZ FTE6
        LDB FBUF+2
        STB NTV
        CLR
        STB GMID
FTE6:   LDB NEVT
        OI EV_TERR
        STB NEVT
        LEAVE

; FSCRIPT -- a segment's frame (sid, seg, flags, offset LE, bytes) into
;   SCRIPT; at S_LAST_FRAME it is whole: SEGRDY, EV_SCRIPT, and its sid,
;   seg and flags kept (SEGSID, SEGNO, SEGFLG) for the player and our D.
;   Leaf.  Clobbers A, r0-r3, H, Q, DCs.
FSCRIPT: LDB FBN                ; its bytes: FBN - 6
        AI -6
        BM FSCX
        LR 3,A
        DCI FBUF+1
        LM
        LR 0,A                  ; sid
        LM
        LR 1,A                  ; seg
        LM
        LR 2,A                  ; flags
        LM
        LR 11,A                 ; offset
        LM
        LR 10,A
        LR A,3                  ; within SCRIPT_BUF
        AS 11
        LR 8,A
        LR A,10
        LNK
        LR 7,A
        LR A,8
        AI 256-((SCRIPT_BUF+1)&0FFH)
        LR A,7
        LNK
        AI 255-((SCRIPT_BUF+1)>>8)
        BC FSCX
        DCI SEGSID
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        DCI SEGLEN              ; its length so far: offset + bytes
        LR A,8
        ST
        LR A,7
        ST
        LR A,3
        NI 0FFH
        BZ FS2
        DCI SCRIPT              ; DC0 = SCRIPT + offset
        LR Q,DC
        LR A,QL
        AS 11
        LR QL,A
        LR A,QU
        LNK
        AS 10
        LR QU,A
        LR DC,Q
        XDC
        DCI FBUF+6
        XDC
FS1:    XDC
        LM
        XDC
        ST
        DS 3
        BNZ FS1
FS2:    LR A,2                  ; the segment's last frame: whole
        NI S_LAST_FRAME
        BZ FSCX
        LIS 1
        STB SEGRDY
        LDB NEVT
        OI EV_SCRIPT
        STB NEVT
FSCX:    POP

; NDONE -- the segment in SCRIPT played: D(sid, seg, taken) owed, nothing
;   ready.  Leaf.  Clobbers A, DC0.
NDONE:  LDB SEGSID
        STB DSID
        LDB SEGNO
        STB DSEG
        CLR
        STB SEGRDY
        LDB TXF
        OI TX_DONE
        STB TXF
        POP
