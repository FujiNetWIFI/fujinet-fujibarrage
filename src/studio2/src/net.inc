; net.inc -- the link (src/net.c over the cart's mailbox): one step a pass
; that never waits. FujiBlast's src/studio2 net.inc (its transactions,
; COBS, Fletcher-16) with FujiBarrage's frames (src/channelf net.inc).
;
; The FujiNet's TCP socket (N1:) is driven one transaction at a time, each
; posted in one pass and seen answered in a later one. The order: a WRITE
; of whatever is to be sent; else, with bytes known waiting (NBW), a READ
; of them, up to RD_MAX; else a STATUS (the bytes waiting; is the socket
; still up), STATUS_GAP jiffies apart at most. A READ never asks for more
; than is waiting (firmware before afe7dddaa drops a short read's bytes).
;
; While a transaction is out only drawing and POKE touch the cart (a
; mailbox read would stall the cart's drawing until the reply), and nothing
; is posted while a READ's bytes are undecoded: every finished transaction
; repaints the reply window. The bytes are decoded from the window itself.
;
; What is to be sent is kept as intents (TXF) and encoded only when a WRITE
; goes: each frame built in RAWBUF (type, payload, Fletcher-16), COBS'd
; straight into the transaction, then a 0. The server's frames: a chunk of
; the READ's bytes a pass COBS-decoded into FBUF (the cart's RAM, by a poke
; stream); at a frame's 0 its sums checked, then applied (frend): a patch's
; runs into ST (Q), a commit (C), a room (T), an error (E), the catalog (K),
; TERRAIN (G: the model, fld.inc), a segment (S into SCRIPT). A bad frame
; asks for everything again (R).

PING_EVERY EQU  300             ; jiffies between keepalives (each is answered)
DEAD_AFTER EQU  900             ; jiffies without a frame: the link is gone
ASK_AGAIN  EQU  60              ; HELLO unanswered: again
RESYNC_GAP EQU  30              ; passes between RESYNCs
NTIMEOUT   EQU  240             ; passes a transaction may be out
JOIN_WAIT  EQU  600             ; jiffies at a table with no state: no answer
RD_MAX     EQU  240             ; the most a READ asks for
DCHUNK     EQU  48              ; encoded bytes decoded a pass
FBMAX      EQU  MAX_FRAME+4     ; FBUF: a decoded frame
STATUS_GAP EQU  3               ; jiffies between STATUSes
MAX_TABLES EQU  MAX_ROOMS

NK_READ  EQU    1               ; NKIND: the transaction out
NK_STATUS EQU   2
NK_WRITE EQU    3
NL_LINK  EQU    1               ; NLINK: the socket is open
NL_JOINED EQU   2               ;   HELLO sent: at a table
NL_HEARD EQU    4               ;   a commit came since
EV_STATE EQU    1               ; NEVT: what this pass's step brought
EV_ERROR EQU    2
EV_LOST  EQU    4
EV_NOANS EQU    8               ;   no state in JOIN_WAIT jiffies
EV_SCRIPT EQU   10H             ;   a segment is whole in SCRIPT (SEGRDY)
EV_TERR  EQU    20H             ;   a TERRAIN frame changed the model
TX_HELLO EQU    01H             ; TXF: what the next WRITE carries
TX_ACT   EQU    02H
TX_DONE  EQU    04H
TX_CREDIT EQU   08H
TX_PING  EQU    10H
TX_RESYNC EQU   20H
TX_ROOMS EQU    40H
TX_BYE   EQU    80H
DF_BLK   EQU    1               ; DFLAG: a COBS block began (a zero before the next)
DF_255   EQU    2               ;  a 254-byte one (no zero after it)

; TXB: D appended to the transaction's TX stream (RF.1 put back: parked).
TXB     MACRO
        HOT     HI(FN_TXDATA)
        ENDM

; ---- the mailbox (M.U.L.E.'s net.inc) -------------------------------------------
; mstart: a transaction on device R9.1, command R9.0, D parameters: TX
; rewound. Clobbers R8.0 (and RF's low byte).
        RFIT    79
mstart: PLO     R8
        REG     FN_REG_DATA_RST, 0
        LDI     FN_REG_DEVICE
        HOT     HI(FN_REGSEL)
        GHI     R9
        HOT     HI(FN_REGDATA)
        LDI     FN_REG_CMD
        HOT     HI(FN_REGSEL)
        GLO     R9
        HOT     HI(FN_REGDATA)
        LDI     FN_REG_NPARAM
        HOT     HI(FN_REGSEL)
        GLO     R8
        HOT     HI(FN_REGDATA)
        FRET

; mpost: SEQ = ACKSEQ + 1 (255 wraps to 1): the transaction goes; NSEQ
; keeps it, ACKSEQ echoes it when the reply is in. NWAIT from 0.
        RFIT    53
mpost:  PEEK    FN_ACKSEQ
        ADI     1
        BNZ     mpo1
        LDI     1
mpo1:   PUTV    NSEQ
        LDI     FN_REG_SEQ
        HOT     HI(FN_REGSEL)
        GETV    NSEQ
        HOT     HI(FN_REGDATA)
        SETV    NWAIT, 0
        FRET

; ninit: a fresh link: nothing out, owed or half-decoded.
        RFIT    28
ninit:  LDR     R8, NZERO
ni1:    LDI     0
        STR     R8
        INC     R8
        GLO     R8
        XRI     LO(NZEND)
        BNZ     ni1
        SETV    TBLN, NONE
        SETV    NKIND, 0
        FRET

; nhello: sit at the table: the link's counters, ST zeroed (the state comes
; whole), HELLO due, no seat yet, the field's version unknown.
        RFIT    141
nhello: GETV    TAKEN                   ; (the same socket: the server's count
        PUTV    T6                      ;  of what we took goes on)
        GETV    CREDITED
        PUTV    T7
        FCALL   ninit
        GETV    T6
        PUTV    TAKEN
        GETV    T7
        PUTV    CREDITED
        POKEAT  ST
        LDR     R9, STATE_SIZE
nh1:    LDI     0
        PKD
        DEC     R9
        GLO     R9
        BNZ     nh1
        GHI     R9
        BNZ     nh1
        PKEND
        GETV    NLINK
        ANI     NL_LINK
        ORI     NL_JOINED
        STR     RA
        SETV    TXF, TX_HELLO
        SETV    MYSEAT, NONE
        SETV    NTV, NONE
        SETV    JOINT, 0
        SETV    JOINT+1, 0
        FRET

; act_q: an action queued: D the kind, its ANARG args already in AARGS; its
; seq never 0. (One a pass: a second replaces the first.)
        RFIT    25
act_q:  PUTV    AK
        GETV    ASEQ
        ADI     1
        BNZ     acq1
        LDI     1
acq1:   STR     RA
        GETV    TXF
        ORI     TX_ACT
        STR     RA
        FRET

; ---------------------------------------------------------------------------
; nstep -- a pass of the link: the transaction out answered, a chunk
;   decoded, the next posted, the clocks (DT jiffies); our seat at a
;   commit; no state in JOIN_WAIT jiffies is EV_NOANS. NEVT: what came.
; ---------------------------------------------------------------------------
        RFIT    145
nstep:  SETV    NEVT, 0
        GETV    FLAGS                   ; a test scene: nothing comes or goes
        ANI     FL_TEST                 ;  (TESTEV: what a rig pokes, once)
        BZ      ns0
        SETV    TXF, 0
        GETV    TESTEV
        PUTV    NEVT
        SETV    TESTEV, 0
        FRET
ns0:    GETV    NLINK
        ANI     NL_LINK
        BNZ     ns1
        SETV    NEVT, EV_LOST
        FRET
ns1:    FCALL   ntxn                    ; 1. the transaction out
        GETV    NEVT
        BZ      ns1a
        FRET                            ; (lost)
ns1a:   GETV    DLEFT                   ; 2. a chunk decoded
        BZ      ns2
        FCALL   decode
        GETV    NEVT
        ANI     EV_ERROR
        BZ      ns2
        FRET                            ; (an E frame: the caller's)
ns2:    FCALL   npost                   ; 3. the next transaction
        FCALL   ntick                   ; 4. the clocks
        GETV    NEVT                    ; a commit: our seat (the server's)
        ANI     EV_STATE
        BZ      ns3
        GETS    OFF_MY_SEAT
        PUTV    MYSEAT
ns3:    GETV    NLINK                   ; joined, nothing heard: JOIN_WAIT
        ANI     NL_JOINED+NL_HEARD
        XRI     NL_JOINED
        BNZ     ns9
        GETV    DT
        STR     R2
        GETV    JOINT
        ADD
        STR     RA
        INC     RA
        LDN     RA
        ADCI    0
        STR     RA
        SMI     HI(JOIN_WAIT)           ; (to a multiple of 256: near enough)
        BNF     ns9
        GETV    NEVT
        ORI     EV_NOANS
        STR     RA
ns9:    FRET

; ---------------------------------------------------------------------------
; ntxn -- 1. the transaction out: answered? Its answer taken (a READ's
;   bytes to decode; a STATUS's bytes waiting, and is the socket up).
; ---------------------------------------------------------------------------
        RFIT    209
ntxn:   GETV    NKIND
        BNZ     nt0
        FRET
nt0:    INCV    NWAIT
        XRI     NTIMEOUT
        BZ      nlost
        PEEK    FN_ACKSEQ               ; answered?
        STR     R2
        GETV    NSEQ
        XOR
        BZ      nt1
        FRET                            ; still out
nt1:    GETV    NKIND
        PLO     R9
        SETV    NKIND, 0
        PEEK    FN_ERR                  ; a NAK or an error: nothing
        BNZ     nt9
        PEEK    FN_REPLYCMD
        XRI     FCACK
        BNZ     nt9
        GLO     R9
        XRI     NK_READ
        BNZ     nt2
        PEEK    FN_RXLEN                ; a READ's bytes: decode them
        PUTV    DLEFT
        SETV    DRPOS, 0
        FRET
nt2:    GLO     R9
        XRI     NK_STATUS
        BZ      nt3
        FRET
nt3:    PEEK    FN_REPLY                ; STATUS: the bytes waiting
        PUTV    NBW
        PEEK    FN_REPLY+1
        PUTV    NBW+1
        BNZ     ntx
        GETV    NBW
        BNZ     ntx
        PEEK    FN_REPLY+2              ; none waiting, but connected
        BNZ     ntx
        BR      nlost
nt9:    SETV    NBW, 0                  ; (what is waiting asked again)
        SETV    NBW+1, 0
ntx:    FRET
nlost:  SETV    NLINK, 0
        GETV    NEVT
        ORI     EV_LOST
        STR     RA
        SETV    NKIND, 0
        FRET

; ---------------------------------------------------------------------------
; npost -- 3. the next transaction, once nothing is out and the last READ
;   is decoded: a WRITE, else a READ of what is waiting (RD_MAX at most),
;   else a STATUS (STATUS_GAP jiffies apart).
; ---------------------------------------------------------------------------
        RFIT    186
npost:  GETV    NKIND
        BNZ     np9
        GETV    DLEFT
        BNZ     np9
        GETV    TXF
        BZ      np1
        FJMP    postw
np1:    GETV    NBW+1                   ; waiting: READ it
        BNZ     npr1
        GETV    NBW
        BNZ     npr
        GETV    SGAP                    ; none known: STATUS, if its time
        BNZ     np9
        SETV    SGAP, STATUS_GAP
        LDI     NETDEV
        PHI     R9
        LDI     NCSTAT
        PLO     R9
        LDI     2
        FCALL   mstart
        LDI     1                       ; two 1-byte parameters, 0
        TXB
        LDI     0
        TXB
        LDI     1
        TXB
        LDI     0
        TXB
        LDI     NK_STATUS
        BR      npgo
npr:    SMI     RD_MAX+1                ; all of it, up to RD_MAX
        BDF     npr1
        GETV    NBW
        PLO     R9
        SETV    NBW, 0
        BR      npr2
npr1:   LDI     RD_MAX                  ; RD_MAX of it, the rest kept
        PLO     R9
        GETV    NBW
        SMI     RD_MAX
        STR     RA
        INC     RA
        LDN     RA
        SMBI    0
        STR     RA
npr2:   GLO     R9
        PHI     R8                      ; (mstart spends R8.0)
        LDI     NETDEV
        PHI     R9
        LDI     NCREAD
        PLO     R9
        LDI     1
        FCALL   mstart
        LDI     2                       ; a 2-byte parameter: the length
        TXB
        GHI     R8
        TXB
        LDI     0
        TXB
        LDI     NK_READ
npgo:   PUTV    NKIND                   ; posted: kind D
        FJMP    mpost
np9:    FRET

; ---------------------------------------------------------------------------
; postw -- every frame TXF wants, in one WRITE: its length first (each
;   frame's raw length + 2: COBS adds one, then the 0; an action's args
;   ANARG more), then the frames.
; ---------------------------------------------------------------------------
        RFIT    153
postw:  GETV    TXF                     ; the length
        PLO     R9
        LDI     0
        PHI     R9
        LDR     R8, FLEN
pw1:    GLO     R9
        SHR
        PLO     R9
        BNF     pw2
        LDN     R8
        STR     R2
        GHI     R9
        ADD
        PHI     R9
pw2:    INC     R8
        GLO     R9
        BNZ     pw1
        GETV    TXF
        ANI     TX_ACT
        BZ      pw2a
        GETV    ANARG
        STR     R2
        GHI     R9
        ADD
        PHI     R9
pw2a:   GHI     R9
        PUTV    RAWN
        LDI     NETDEV
        PHI     R9
        LDI     NCWRITE
        PLO     R9
        LDI     1
        FCALL   mstart
        LDI     2
        TXB
        GETV    RAWN
        TXB
        LDI     0
        TXB
        SETV    BK, 0                   ; each frame, in bit order
pw3:    GETV    BK
        ADI     LO(BITS)
        PLO     R8
        LDI     HI(BITS)
        PHI     R8
        LDN     R8
        STR     R2
        GETV    TXF
        AND
        BZ      pw4
        FCALL   build
        FCALL   cobsout
pw4:    INCV    BK
        XRI     8
        BNZ     pw3
        SETV    TXF, 0
        LDI     NK_WRITE
        PUTV    NKIND
        FJMP    mpost

        PFIT    16
; the frames, by TXF bit: raw length + 2 (type, payload, 2 sums; COBS's 1;
; the 0), and the type
FLEN:   DB      1+30+2+2, 1+2+2+2, 1+3+2+2, 1+1+2+2, 1+0+2+2, 1+8+2+2, 1+0+2+2, 1+0+2+2
FTYPES: DB      SC_HELLO, SC_ACT, SC_DONE, SC_CREDIT, SC_PING, SC_RESYNC, SC_ROOMS, SC_BYE

; build -- frame BK (a TXF bit's number) into RAWBUF: its type, its
;   payload, then the two Fletcher sums; RAWN its length.
        RFIT    229
build:  LDR     R8, RAWBUF
        GETV    BK
        ADI     LO(FTYPES)
        PLO     RC
        LDI     HI(FTYPES)
        PHI     RC
        LDN     RC
        STR     R8
        INC     R8
        GETV    BK
        BZ      bhello
        SMI     1
        BZ      bact
        SMI     1
        BZ      bdone
        SMI     1
        BZ      bcred
        SMI     2
        BZ      bresy
        BR      bsumj                   ; PING, ROOMS, BYE: no payload
bhello: LDI     PROTO_VERSION           ; version, flags, the room's id, our
        STR     R8                      ;  name, our shape
        INC     R8
        LDI     0
        STR     R8
        INC     R8
        LDR     RC, SRVTBL
        LDI     ROOM_LEN
        PLO     R9
bh1:    LDA     RC
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     bh1
        LDR     RC, MYNAME
        LDI     NAME_LEN
        PLO     R9
bh1a:   LDA     RC
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     bh1a
        LDR     RC, HELLO1
        LDI     HELLO1N
        PLO     R9
bh2:    LDA     RC
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     bh2
        BR      bsumj
bact:   LDR     RC, ASEQ                ; seq, act, its args
        GETV    ANARG
        ADI     2
        PLO     R9
ba1:    LDA     RC
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     ba1
        BR      bsumj
bdone:  GETV    DSID                    ; D: sid, seg, taken (a credit too)
        STR     R8
        INC     R8
        GETV    DSEG
        STR     R8
        INC     R8
bcred:  GETV    TAKEN                   ; what we've taken (and credited)
        PUTV    CREDITED
        STR     R8
        INC     R8
bsumj:  FJMP    bsum
bresy:  GETV    TAKEN                   ; R: taken, tv, sid, seg, off16 0, cat16
        PUTV    CREDITED
        STR     R8
        INC     R8
        GETV    GMID                    ; (a version half applied: FF)
        BZ      brs1
        LDI     NONE
        BR      brs2
brs1:   GETV    NTV
brs2:   STR     R8
        INC     R8
        GETV    SEGSID
        STR     R8
        INC     R8
        GETV    SEGNO
        STR     R8
        INC     R8
        LDI     0                       ; (the segment again from its start)
        STR     R8
        INC     R8
        STR     R8
        INC     R8
        SETV    SEGRDY, 0
        SETV    SEGLEN, 0
        SETV    SEGLEN+1, 0
        GETV    CATN
        STR     R8
        INC     R8
        GETV    CATN+1
        STR     R8
        INC     R8
        FJMP    bsum

; bsum -- the Fletcher-16 over RAWBUF up to R8, after it; RAWN the length.
        RFIT    74
bsum:   GLO     R8
        SMI     LO(RAWBUF)
        PUTV    RAWN                    ; (+ 2 below)
        PLO     R9                      ; its length
        LDR     RC, RAWBUF
        LDI     0
        PHI     R9                      ; s2
        PLO     RA                      ; s1 (RA.0: no GETV meanwhile)
bs1:    LDA     RC                      ; s1 += b, s2 += s1 (mod 255: 255 is
        STR     R2                      ;  0, made canonical at the end)
        GLO     RA
        ADD
        ADCI    0
        PLO     RA
        STR     R2
        GHI     R9
        ADD
        ADCI    0
        PHI     R9
        DEC     R9
        GLO     R9
        BNZ     bs1
        GLO     RA                      ; the sums after it
        XRI     0FFH
        BZ      bs2
        XRI     0FFH
bs2:    STR     R8
        INC     R8
        GHI     R9
        XRI     0FFH
        BZ      bs3
        XRI     0FFH
bs3:    STR     R8
        GETV    RAWN
        ADI     2
        STR     RA
        FRET

; our shape (HELLO bytes 18-29): width (LE), height, aspect, Hz, the tank,
; the script buffer (LE), colour bands, caps, platform
HELLO1: DB      FW, 0, FH, PAR, HZ, TANK_W, TANK_H, LO(SCRIPT_BUF), HI(SCRIPT_BUF), 0
        DB      CAP_HOLD, PLAT_STUDIO2
HELLO1N EQU     $-HELLO1
        IF      2+ROOM_LEN+NAME_LEN+HELLO1N <> HELLO_LEN
        ERROR   "HELLO's length"
        ENDIF

; cobsout -- RAWBUF's RAWN bytes as COBS into the TX, then a 0: each run up
;   to a zero (or the end) is its length + 1, then its bytes. RF.1 stays on
;   the TX page meanwhile.
        RFIT    67
cobsout:
        LDR     R8, RAWBUF
        GETV    RAWN
        PLO     R9                      ; bytes left
        LDI     HI(FN_TXDATA)
        PHI     RF
co1:    GLO     R8                      ; a run: RC its start, R9.1 its length
        PLO     RC
        GHI     R8
        PHI     RC
        LDI     0
        PHI     R9
co2:    GLO     R9
        BZ      co3
        LDN     R8
        BZ      co3
        INC     R8
        DEC     R9
        GHI     R9
        ADI     1
        PHI     R9
        BR      co2
co3:    GHI     R9                      ; its code: length + 1
        ADI     1
        PLO     RF
        LDN     RF
        GHI     R9
        BZ      co5
co4:    LDA     RC                      ; its bytes
        PLO     RF
        LDN     RF
        GHI     R9
        SMI     1
        PHI     R9
        BNZ     co4
co5:    GLO     R9                      ; the end?
        BZ      co6
        INC     R8                      ; a zero: past it, the next run
        DEC     R9
        BR      co1
co6:    LDI     0                       ; the frame's 0
        PLO     RF
        LDN     RF
        LDI     FN_PARK
        PHI     RF
        FRET

; ---------------------------------------------------------------------------
; ntick -- 4. the clocks, by DT: a RESYNC owed, a credit, the keepalive,
;   HELLO again, a silent link given up, the STATUS gap.
; ---------------------------------------------------------------------------
        RFIT    202
ntick:  GETV    RHOLD                   ; a RESYNC owed, once the gap is over
        ANI     7FH
        BZ      ntk2
        LDN     RA
        SMI     1
        STR     RA
        BR      ntk3
ntk2:   LDN     RA
        ANI     80H
        BZ      ntk3
        SETV    RHOLD, RESYNC_GAP
        GETV    TXF
        ORI     TX_RESYNC
        STR     RA
ntk3:   GETV    CREDITED                ; taken - credited >= 48: a credit
        STR     R2
        GETV    TAKEN
        SM
        SMI     48
        BNF     ntk4
        GETV    TXF
        ORI     TX_CREDIT
        STR     RA
ntk4:   GETV    NLINK                   ; (before HELLO nothing answers a
        ANI     NL_JOINED               ;  PING: no dead-link clock)
        BZ      ntk5
        GETV    DT                      ; quiet += DT; > DEAD_AFTER: gone
        STR     R2
        VAR     QUIET
        LDN     RA
        ADD
        STR     RA
        INC     RA
        LDN     RA
        ADCI    0
        STR     RA
        VAR     QUIET
        LDA     RA
        SMI     LO(DEAD_AFTER+1)
        LDN     RA
        SMBI    HI(DEAD_AFTER+1)
        BNF     ntk5
        SETV    NLINK, 0
        GETV    NEVT
        ORI     EV_LOST
        STR     RA
        FRET
ntk5:   GETV    DT                      ; every PING_EVERY: a credit and a PING
        STR     R2
        VAR     QPING
        LDN     RA
        ADD
        STR     RA
        INC     RA
        LDN     RA
        ADCI    0
        STR     RA
        VAR     QPING
        LDA     RA
        SMI     LO(PING_EVERY)
        LDN     RA
        SMBI    HI(PING_EVERY)
        BNF     ntk6
        SETV    QPING, 0
        SETV    QPING+1, 0
        GETV    TXF
        ORI     TX_CREDIT+TX_PING
        STR     RA
ntk6:   GETV    DT                      ; every ASK_AGAIN: HELLO, if unanswered
        STR     R2
        GETV    QASK
        ADD
        STR     RA
        SMI     ASK_AGAIN
        BNF     ntk7
        SETV    QASK, 0
        GETV    NLINK
        ANI     NL_JOINED+NL_HEARD
        XRI     NL_JOINED
        BNZ     ntk7
        GETV    TXF
        ORI     TX_HELLO
        STR     RA
ntk7:   GETV    DT                      ; the STATUS gap, to 0
        STR     R2
        GETV    SGAP
        SM
        BDF     ntk8
        LDI     0
ntk8:   STR     RA
        FRET

; ---------------------------------------------------------------------------
; decode -- up to DCHUNK of the READ's bytes COBS-decoded into FBUF (the
;   cart's RAM: a poke stream), read from the reply window itself; at a
;   frame's 0, frend applies it. TAKEN counts the bytes. In the loop: R8
;   the reply, R9.0 the chunk left, R9.1 the byte, RB.0 FBUF's count,
;   RD.1 the COBS block's data bytes left, RF.1 on REGDATA.
; ---------------------------------------------------------------------------
        RFIT    174
decode: GETV    DLEFT                   ; this chunk: DCHUNK at most
        SMI     DCHUNK+1
        BNF     de0
        LDI     DCHUNK
        BR      de1
de0:    GETV    DLEFT
de1:    PLO     R9
        STR     R2
        GETV    DLEFT
        SM
        STR     RA
        GLO     R9                      ; taken += the chunk
        STR     R2
        GETV    TAKEN
        ADD
        STR     RA
        GETV    DRPOS                   ; R8 = the reply + DRPOS
        PLO     R8
        LDI     HI(FN_REPLY)
        PHI     R8
        GLO     R9
        STR     R2
        LDN     RA
        ADD
        STR     RA
        GETV    FBN                     ; the stream at FBUF + FBN
        PLO     RB
        ADI     LO(FBUF)
        PLO     RC
        LDI     HI(FBUF)
        ADCI    0
        PHI     RC
        POKER   RC
        GETV    DCODE
        PHI     RD
dl:     LDA     R8                      ; the next byte
        BZ      dzero                   ; 0: a frame's end
        PHI     R9
        GHI     RD
        BZ      dcode
        SMI     1                       ; a data byte
        PHI     RD
demit:  GLO     RB                      ; into FBUF (past FBMAX: dropped, and
        SMI     FBMAX                   ;  the frame will fail its sums)
        BDF     dn
        GHI     R9
        PLO     RF
        LDN     RF
        INC     RB
dn:     DEC     R9
        GLO     R9
        BNZ     dl
dsave:  PKEND                           ; the decoder's state kept
        GHI     RD
        STR     R2
        VAR     DCODE
        LDN     R2
        STR     RA
        GLO     RB
        STR     R2
        VAR     FBN
        LDN     R2
        STR     RA
        FRET
dcode:  GHI     R9                      ; a block's code: its data bytes
        SMI     1
        PHI     RD
        GETV    DFLAG                   ; the zero before it, unless it's the
        STR     R2                      ;  frame's first or after a 254-byte one
        GHI     R9
        XRI     0FFH
        BZ      dc255
        LDI     DF_BLK
        BR      dc1
dc255:  LDI     DF_BLK+DF_255
dc1:    STR     RA                      ; (RA is on DFLAG)
        LDN     R2
        XRI     DF_BLK
        BNZ     dn
        LDI     0
        PHI     R9
        BR      demit
dzero:  FJMP    dzend                   ; (D = 0: RD.1 = 0 after)

; dzend: a frame's end in decode: applied, the decoder reset, then the loop
; again at dn (the stream at FBUF).
        RFIT    119
dzend:  PKEND
        GLO     R8
        PUTV    DSV1
        GLO     R9
        PUTV    DSV2
        GLO     RB
        PUTV    FBN
        FCALL   frend
        SETV    FBN, 0
        SETV    DFLAG, 0
        GETV    NEVT                    ; an error: stop
        ANI     EV_ERROR
        BZ      dz1
        SETV    DLEFT, 0
        SETV    DCODE, 0
        FRET
dz1:    GETV    DSV1
        PLO     R8
        LDI     HI(FN_REPLY)
        PHI     R8
        GETV    DSV2
        PLO     R9
        LDI     0
        PLO     RB                      ; FBN 0
        POKEAT  FBUF
        LDI     0                       ; (RD.1 = DCODE = 0 after the jump)
        FJMP    dn

; ---------------------------------------------------------------------------
; frend -- a frame in FBUF (FBN bytes): its sums checked, then what its type
;   does; a bad one asks for everything again.
; ---------------------------------------------------------------------------
        RFIT    205
frend:  GETV    FBN
        SMI     3
        BNF     febad                   ; the sums and next to nothing: bad
        ADI     1
        PUTV    FBN                     ; the bytes summed (type, payload)
        PLO     R9
        LDR     RC, FBUF
        LDI     0
        PHI     R9                      ; s2
        PLO     RA                      ; s1
fe1:    LDA     RC                      ; (ones' complement: 255 is 0, made
        STR     R2                      ;  canonical once, at the end)
        GLO     RA
        ADD
        ADCI    0
        PLO     RA
        STR     R2
        GHI     R9
        ADD
        ADCI    0
        PHI     R9
        DEC     R9
        GLO     R9
        BNZ     fe1
        GLO     RA                      ; against the two that follow
        XRI     0FFH
        BZ      fe2
        XRI     0FFH
fe2:    STR     R2
        LDA     RC
        XOR
        BNZ     febad
        GHI     R9
        XRI     0FFH
        BZ      fe3
        XRI     0FFH
fe3:    STR     R2
        LDN     RC
        XOR
        BNZ     febad
        SETV    QUIET, 0                ; a good frame: the link is alive
        SETV    QUIET+1, 0
        SETV    QPING, 0
        SETV    QPING+1, 0
        SETV    QASK, 0
        LDR     R8, FBUF                ; its type
        LDN     R8
        XRI     CS_PATCH
        BNZ     fe4
        FJMP    fpatch
fe4:    XRI     CS_PATCH!CS_COMMIT
        BNZ     fe5
        GETV    NLINK                   ; a commit: the state is whole
        ANI     NL_JOINED
        BZ      fex
        LDN     RA
        ORI     NL_HEARD
        STR     RA
        GETV    NEVT
        ORI     EV_STATE
        STR     RA
        FRET
fe5:    XRI     CS_COMMIT!CS_ERROR
        BNZ     fe6
        INC     R8
        LDN     R8
        PUTV    NERR
        GETV    NEVT
        ORI     EV_ERROR
        STR     RA
        FRET
fe6:    XRI     CS_ERROR!CS_ROOM
        BNZ     fe7
        FJMP    ftable
fe7:    XRI     CS_ROOM!CS_TERRAIN
        BNZ     fe8
        FJMP    fterr
fe8:    XRI     CS_TERRAIN!CS_SCRIPT
        BNZ     fe9
        FJMP    fscript
fe9:    XRI     CS_SCRIPT!CS_CATALOG
        BNZ     fex
        FJMP    fcat
fex:    FRET
febad:  GETV    RHOLD                   ; a RESYNC owed
        ORI     80H
        STR     RA
        INCV    NBAD                    ; (counted, for the rigs)
        BNZ     feb1
        LDI     0FFH
        STR     RA
feb1:   FRET

; ---------------------------------------------------------------------------
; fpatch -- a patch's runs (offset LE, length, bytes) poked into ST. A run
;   past the state or the frame ends it. R8 the frame, T0 its bytes left,
;   R9 the offset, RC.0 the run's bytes.
; ---------------------------------------------------------------------------
        RFIT    118
fpatch: LDR     R8, FBUF+1
        GETV    FBN
        SMI     1
        PUTV    T0                      ; its bytes after the type
fp1:    GETV    T0                      ; a run needs three
        SMI     3
        BNF     fpx
        STR     RA
        LDA     R8
        PLO     R9                      ; the offset
        LDA     R8
        PHI     R9
        LDA     R8
        PLO     RC                      ; the length
        BZ      fp1
        STR     R2                      ; within the frame
        GETV    T0
        SM
        BNF     fpx
        STR     RA
        GLO     RC                      ; and the state: off + len <= STATE_SIZE
        STR     R2
        GLO     R9
        ADD
        PLO     RB
        GHI     R9
        ADCI    0
        PHI     RB
        GLO     RB
        SMI     LO(STATE_SIZE+1)
        GHI     RB
        SMBI    HI(STATE_SIZE+1)
        BDF     fpx
        GLO     R9                      ; the stream at ST + off
        ADI     LO(ST)
        PLO     R9
        GHI     R9
        ADCI    HI(ST)
        PHI     R9
        POKER   R9
fp2:    LDA     R8
        PLO     RF
        LDN     RF
        DEC     RC
        GLO     RC
        BNZ     fp2
        PKEND
        BR      fp1
fpx:    FRET

; ftable -- a room's record (index, count, ROOM_REC bytes) into TBLS; TBLN
;   the count (MAX_TABLES at most), TBLHAVE its bit.
        RFIT    116
ftable: LDR     R8, FBUF+2              ; the count
        LDN     R8
        SMI     MAX_TABLES+1
        BNF     ftb1
        LDI     MAX_TABLES
        BR      ftb2
ftb1:   LDN     R8
ftb2:   PUTV    TBLN
        DEC     R8                      ; the index
        LDN     R8
        SMI     MAX_TABLES
        BDF     ftbx
        LDN     R8
        ADI     LO(BITS)
        PLO     RC
        LDI     HI(BITS)
        PHI     RC
        LDN     RC
        STR     R2
        GETV    TBLHAVE
        OR
        STR     RA
        LDN     R8                      ; TBLS + 24 index
        SHL
        SHL
        SHL
        PLO     R9
        SHL
        STR     R2
        GLO     R9
        ADD
        ADI     LO(TBLS)
        PLO     R9
        LDI     HI(TBLS)
        ADCI    0
        PHI     R9
        POKER   R9
        LDR     R8, FBUF+3
        LDI     ROOM_REC
        PLO     R9
ftb3:   LDA     R8
        PLO     RF
        LDN     RF
        DEC     R9
        GLO     R9
        BNZ     ftb3
        PKEND
ftbx:   FRET

; fcat -- a catalog frame (offset LE, bytes) into CATBUF, if it follows
;   what we hold whole (CATN; R asks from there) and fits.
        RFIT    133
fcat:   GETV    FBN                     ; its bytes: FBN - 3
        SMI     3
        BZ      fcx
        BNF     fcx
        PLO     RC
        LDR     R8, FBUF+1              ; the offset must be CATN
        LDA     R8
        STR     R2
        GETV    CATN
        XOR
        BNZ     fcx
        LDA     R8
        STR     R2
        GETV    CATN+1
        XOR
        BNZ     fcx
        GLO     RC                      ; the end: CATN + n, within CAT_MAX
        STR     R2
        GETV    CATN
        ADD
        PLO     R9
        GETV    CATN+1
        ADCI    0
        PHI     R9
        GLO     R9
        SMI     LO(CAT_MAX+1)
        GHI     R9
        SMBI    HI(CAT_MAX+1)
        BDF     fcx
        GETV    CATN                    ; the stream at CATBUF + CATN
        ADI     LO(CATBUF)
        PLO     RB
        GETV    CATN+1
        ADCI    HI(CATBUF)
        PHI     RB
        GLO     R9                      ; CATN = the end
        PUTV    CATN
        GHI     R9
        PUTV    CATN+1
        POKER   RB
fc1:    LDA     R8
        PLO     RF
        LDN     RF
        DEC     RC
        GLO     RC
        BNZ     fc1
        PKEND
fcx:    FRET

; ---------------------------------------------------------------------------
; fterr -- a TERRAIN frame (base tv, new tv, flags, x LE, column entries):
;   applied when its base is FF (the whole field) or our NTV; at G_LAST,
;   NTV = its new tv (GMID: a version half applied, which a RESYNC reports
;   as FF). Else NTV = FF and a RESYNC is owed. EV_TERR.
; ---------------------------------------------------------------------------
        RFIT    165
fterr:  LDR     R8, FBUF+1              ; the base
        LDN     R8
        XRI     NONE
        BZ      fte1
        GETV    NTV
        STR     R2
        LDN     R8
        XOR
        BZ      fte1
        SETV    NTV, NONE               ; not ours: the whole field again
        GETV    RHOLD
        ORI     80H
        STR     RA
        FRET
fte1:   SETV    GMID, 1                 ; (mid-version until G_LAST)
        GETV    FBN                     ; its entries' bytes: FBN - 6
        SMI     6
        BZ      fte5
        BNF     fte5
        PLO     RC
        LDR     R8, FBUF+6              ; count the entries in them
        LDI     0
        PHI     R9
fte2:   LDA     R8
        SMI     E_NONE+1
        BNF     fte3                    ; one byte
        ADI     1                       ; F0 + n: 2n more
        SHL
        STR     R2
        GLO     R8
        ADD
        PLO     R8
        GHI     R8
        ADCI    0
        PHI     R8
        LDN     R2
        ADI     1
        BR      fte4
fte3:   LDI     1
fte4:   STR     R2                      ; bytes left -= the entry's
        GLO     RC
        SM
        PLO     RC
        GHI     R9
        ADI     1
        PHI     R9
        GLO     RC
        BZ      fte4a
        ANI     80H
        BZ      fte2
fte4a:  LDR     R8, FBUF+4              ; x (< FW), and x + n <= FW
        LDN     R8
        SMI     FW
        BDF     fte5
        LDN     R8
        PLO     R9
        GHI     R9
        STR     R2
        GLO     R9
        ADD
        SDI     FW
        BDF     fte4b
        GLO     R9                      ; (cut to the field)
        SDI     FW
        PHI     R9
fte4b:  LDR     R8, FBUF+6
        FCALL   fld_puts
fte5:   LDR     R8, FBUF+3              ; the version whole: ours
        LDN     R8
        ANI     G_LAST
        BZ      fte6
        DEC     R8
        LDN     R8
        PUTV    NTV
        SETV    GMID, 0
fte6:   GETV    NEVT
        ORI     EV_TERR
        STR     RA
        FRET

; ---------------------------------------------------------------------------
; fscript -- a segment's frame (sid, seg, flags, offset LE, bytes) into
;   SCRIPT. Offset 0 starts a segment; a frame that doesn't follow (or
;   overflows SCRIPT_BUF) asks again. At S_LAST_FRAME it is whole: SEGRDY,
;   SEGFLG, EV_SCRIPT.
; ---------------------------------------------------------------------------
        RFIT    241
fscript:
        GETV    FBN                     ; its bytes: FBN - 6
        SMI     6
        BNF     fsx
        PLO     RC
        LDR     R8, FBUF+4              ; offset 0: a new segment
        LDA     R8
        BNZ     fs1
        LDN     R8
        BNZ     fs1
        LDR     R8, FBUF+1
        LDA     R8
        PUTV    SEGSID
        LDN     R8
        PUTV    SEGNO
        SETV    SEGLEN, 0
        SETV    SEGLEN+1, 0
        SETV    SEGRDY, 0
fs1:    LDR     R8, FBUF+1              ; ours, and where we are?
        LDA     R8
        STR     R2
        GETV    SEGSID
        XOR
        BNZ     fsbad
        LDA     R8
        STR     R2
        GETV    SEGNO
        XOR
        BNZ     fsbad
        INC     R8
        LDA     R8
        STR     R2
        GETV    SEGLEN
        XOR
        BNZ     fsbad
        LDA     R8
        STR     R2
        GETV    SEGLEN+1
        XOR
        BNZ     fsbad
        GLO     RC                      ; the end within SCRIPT_BUF
        STR     R2
        GETV    SEGLEN
        ADD
        PLO     R9
        GETV    SEGLEN+1
        ADCI    0
        PHI     R9
        GLO     R9
        SMI     LO(SCRIPT_BUF+1)
        GHI     R9
        SMBI    HI(SCRIPT_BUF+1)
        BDF     fsbad
        GLO     RC
        BZ      fs3
        GETV    SEGLEN                  ; the stream at SCRIPT + SEGLEN
        ADI     LO(SCRIPT)
        PLO     RB
        GETV    SEGLEN+1
        ADCI    HI(SCRIPT)
        PHI     RB
        GLO     R9                      ; SEGLEN = the end
        PUTV    SEGLEN
        GHI     R9
        PUTV    SEGLEN+1
        POKER   RB
fs2:    LDA     R8
        PLO     RF
        LDN     RF
        DEC     RC
        GLO     RC
        BNZ     fs2
        PKEND
fs3:    LDR     R8, FBUF+3              ; the segment's last frame: whole
        LDN     R8
        PUTV    SEGFLG
        ANI     S_LAST_FRAME
        BZ      fsx
        SETV    SEGRDY, 1
        GETV    NEVT
        ORI     EV_SCRIPT
        STR     RA
fsx:    FRET
fsbad:  GETV    RHOLD                   ; a frame of it missing: R
        ORI     80H
        STR     RA
        FRET

; ndone -- the segment in SCRIPT played: D(sid, seg, taken) owed, nothing
;   ready.
        RFIT    34
ndone:  GETV    SEGSID
        PUTV    DSID
        GETV    SEGNO
        PUTV    DSEG
        SETV    SEGRDY, 0
        GETV    TXF
        ORI     TX_DONE
        STR     RA
        FRET
