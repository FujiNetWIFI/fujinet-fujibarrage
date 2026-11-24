; p_net.inc -- the link (src/net.c over the cart's mailbox): one step a
; frame that never waits. The Astrocade's (src/astrocade/src/p_net.inc,
; Z80) in FujiBlast's 2650 form (its src/arcadia/src/p_net.inc), for
; FujiBarrage's documents (docs/PROTOCOL.md).
;
; The FujiNet's TCP socket is driven one transaction at a time, each posted
; in one frame and seen answered in a later one. The order: a WRITE of
; whatever is to be sent; else, with bytes known waiting (NBW), a READ of
; them, up to RD_MAX; else a STATUS (the bytes waiting; is the socket still
; up). A READ never asks for more than is waiting: firmware before
; afe7dddaa (2026-10-30) takes a short read's bytes off the socket and
; drops them with an error. Every answer replaces the reply window -- an
; empty one too -- so a READ's bytes are decoded (a chunk a step) before
; anything else is posted.
;
; What is to be sent is kept as intents (TXF) and encoded only when a WRITE
; goes out (the SEND page, p_send.inc: each frame built in RAWBUF, its COBS
; streamed into the transaction, then a 0). The
; server's frames are decoded as they come, straight from the reply window:
; COBS, a running Fletcher, the last two bytes held back (they are the
; sums), and applied as they come -- a patch's bytes streamed into ST, a
; script's into SCRIPT (cart RAM both: the stream is aimed at the run going
; on at each chunk's start, as a RAMPUT elsewhere closes it), a room's
; record into ROOMS; terrain's columns drawn into the field as they come
; (p_fld.inc PTCOL; only while the field shows, which then asks for all of
; it); the
; CATALOG's dropped (ours is in ROM: p_cat.inc). A bad frame (its sums)
; asks for what it touched again (RESYNC: the state, the field, the segment).
;
; The hold (CAP_HOLD): a segment's last frame says S_HOLD and the server
; sends nothing more until our D; the main loop plays it (E_PLAY) with no
; transaction out, then E_DONE owes the D.

        BCTA,UN NSTEP           ; E_ENTER: a frame's step (NEVT: what came)
        BCTA,UN NHELLO          ; E_HELLO: in a room: the state zeroed, HELLO
        BCTA,UN NINIT           ; E_INIT: a fresh link's counters
        BCTA,UN NDONE           ; E_DONE: the segment played: D owed
        BCTA,UN TKOFF           ; E_TKOFF: the block tanks off (fixed B's)

PING_EVERY EQU  300             ; frames between keepalives (each is answered)
DEAD_AFTER EQU  900             ; frames without a frame: the link is gone
ASK_AGAIN  EQU  60              ; HELLO, or our D, unanswered: again
RESYNC_GAP EQU  30              ; frames between RESYNCs
NTIMEOUT   EQU  240             ; frames a transaction may be out
RD_MAX     EQU  192             ; the most a READ asks for (slice 0 holds 256)
DCHUNK     EQU  16              ; encoded bytes decoded a step (~180 cycles each)
DF_BLK     EQU  1               ; DFLAG: a COBS block began (a zero before the next)
DF_255     EQU  2               ;  a 254-byte one (no zero after it)

        IF      PING_EVERY*3 <> DEAD_AFTER
        ERROR   "NTICK pings at QUIET = PING_EVERY and 2 PING_EVERY"
        ENDIF

; ---- entries ---------------------------------------------------------------------
; NINIT: a fresh link: the counters, the decoder, nothing out or owed.
; Calls one down. Clobbers R0-R3.
NINIT:  EORZ    R0
        STRA,R0 NKIND
        STRA,R0 NWAIT
        STRA,R0 NEVT
        STRA,R0 TXF
        STRA,R0 TAKEN
        LODI,R3 ZC_END-CREDITED ; zone C's link bytes
NI1:    STRA,R0 CREDITED-1,R3
        BDRR,R3 NI1
        LODA,R0 NLINK           ; (no D owed)
        ANDI,R0 ~NL_DOWED&$FF
        STRA,R0 NLINK
        CPTR    NTV             ; NTV FF, NTERR, SEGSID, SEGNO, SEGFLG, DSID, DSEG 0
        LODI,R0 $FF
        BSTA,UN RAMPUT1
        LODI,R3 DSEG-NTV
NI2:    EORZ    R0
        BSTA,UN RAMPUT1
        BDRR,R3 NI2
        BCTA,UN RAMPEND         ; tail call
        IF      DSEG-NTV <> 6
        ERROR   "NINIT: NTV .. DSEG must be one run"
        ENDIF

; NHELLO: sit in the room: the counters, ST zeroed (the state comes whole),
; HELLO due, no seat yet. Calls two down. Clobbers R0-R3.
NHELLO: BSTR,UN NINIT
        CPTR    ST
        LODI,R3 0               ; 256 bytes ...
NH1:    EORZ    R0
        BSTA,UN RAMPUT1
        BDRR,R3 NH1
        LODI,R3 STATE_SIZE-256  ; ... and the rest
NH2:    EORZ    R0
        BSTA,UN RAMPUT1
        BDRR,R3 NH2
        BSTA,UN RAMPEND
        LODA,R0 NLINK
        ANDI,R0 NL_LINK
        IORI,R0 NL_JOINED
        STRA,R0 NLINK
        LODI,R0 TX_HELLO
        STRA,R0 TXF
        LODI,R0 NONE
        CPUT    MYSEAT
        EORZ    R0
        STRA,R0 JOINTH
        STRA,R0 JOINTL
        RETC,UN

; NDONE: the segment played (or given up): D(sid, seg, taken) owed, again
; every ASK_AGAIN frames until something comes. (The PLY page used the
; decoder's bytes, PLYSCR, and left them zeroed: the decoder idle.) Calls
; one down. Clobbers R0-R3.
NDONE:  EORZ    R0
        STRA,R0 SEGRDY
        STRA,R0 QASK
        CPTR    DSID
        LODA,R0 SEGSID
        BSTA,UN RAMPUT1
        LODA,R0 SEGNO
        BSTA,UN RAMPUT1
        BSTA,UN RAMPEND
        IF      DSEG <> DSID+1
        ERROR   "NDONE: DSID, DSEG must be one run"
        ENDIF
        LODA,R0 NLINK
        IORI,R0 NL_DOWED
        STRA,R0 NLINK
        LODA,R0 TXF
        IORI,R0 TX_DONE
        STRA,R0 TXF
        RETC,UN

; NSTEP: one frame of the link: the transaction out answered, a chunk
; decoded, the next posted, the clocks; our seat at a commit; no state in
; JOIN_WAIT frames is EV_NOANS. Calls four down. Clobbers R0-R3.
NSTEP:  EORZ    R0
        STRA,R0 NEVT
        LODA,R0 FLAGS           ; a test scene: nothing comes or goes
        TMI,R0  FL_TEST
        BCFR,EQ NS0
        EORZ    R0
        STRA,R0 TXF
        RETC,UN
NS0:    LODA,R0 NLINK
        TMI,R0  NL_LINK
        BCTR,EQ NS1
        LODI,R0 EV_LOST
        STRA,R0 NEVT
        RETC,UN
NS1:    BSTA,UN NTXN            ; 1. the transaction out
        LODA,R0 NEVT
        RETC,GT                 ; (lost)
        LODA,R0 DLEFT           ; 2. a chunk decoded
        BCTR,EQ NS2
        BSTA,UN DECODE
        LODA,R0 NEVT
        TMI,R0  EV_ERROR
        RETC,EQ                 ; (an E frame: the caller's)
NS2:    LODA,R0 SEGRDY          ; 3. the next transaction (none with a segment
        BCFR,EQ NS2A            ;  to play: the server waits for its D)
        BSTA,UN NPOST
NS2A:   BSTA,UN NTICK           ; 4. the clocks
        BCTA,UN NSEGW           ; 5. a script waiting: the link flat out (a
NS2B:                           ;  flow: NPOST's far call is deep)
        LODA,R0 NEVT            ; a commit: our seat
        TMI,R0  EV_STATE
        BCFR,EQ NS3
        LODA,R0 ST+OFF_MY_SEAT
        CPUT    MYSEAT
NS3:    LODA,R0 NLINK           ; joined, nothing heard: JOIN_WAIT
        ANDI,R0 NL_JOINED|NL_HEARD
        COMI,R0 NL_JOINED
        RETC,LT
        RETC,GT
        LODA,R0 JOINTL
        ADDA,R0 DT
        STRA,R0 JOINTL
        PPSL    PSLWC
        LODA,R0 JOINTH
        ADDI,R0 0
        STRA,R0 JOINTH
        CPSL    PSLWC
        COMI,R0 JOIN_WAIT>>8
        RETC,LT
        BCTR,GT NS4
        LODA,R0 JOINTL
        COMI,R0 JOIN_WAIT&$FF
        RETC,LT
NS4:    LODA,R0 NEVT
        IORI,R0 EV_NOANS
        STRA,R0 NEVT
        RETC,UN

; NSEGW: a shot's script waits for its next segment (FL_INSCR, the field
; showing; nothing else runs then): transactions back to back, each READ's
; bytes decoded as they come, up to NSWMAX more steps this frame while the
; picture is still above its last rows (the rest of the frame is the main
; loop's) -- the Astrocade's NSEGW. A flow from NSTEP, back to NS2B.
; Clobbers R0-R3, T2, T3.
NSWMAX  EQU     6
NSEGW:  LODA,R0 FLAGS
        TMI,R0  FL_INSCR
        BCTA,LT NS2B
        LODA,R0 SHOWN
        COMI,R0 S_FIELD
        BCFA,EQ NS2B
        LODI,R0 NSWMAX
        STRA,R0 T3
        EORZ    R0              ; (T2: the picture seen since)
        STRA,R0 T2
NSW1:   LODA,R0 SEGRDY          ; whole, or something else came: done
        BCTA,GT NS2B
        LODA,R0 NEVT
        ANDI,R0 EV_ERROR|EV_LOST|EV_TERR
        BCTA,GT NS2B
        LODA,R0 CHROW           ; the picture's last rows, the next blank:
        ANDI,R0 $0F             ;  done (the blank it began in: on)
        COMI,R0 13
        BCFR,EQ NSW1A
        LODA,R0 T2
        BCTA,GT NS2B
        BCTR,UN NSW1B
NSW1A:  COMI,R0 11
        BCTA,GT NS2B
        BCTA,EQ NS2B
        LODI,R0 1
        STRA,R0 T2
NSW1B:  LODA,R0 T3
        SUBI,R0 1
        STRA,R0 T3
        BCTA,LT NS2B
        BSTA,UN NTXN2
        LODA,R0 NEVT
        TMI,R0  EV_LOST
        BCTA,EQ NS2B
        LODA,R0 DLEFT
        BCTR,EQ NSW2
        BSTA,UN DECODE
NSW2:   LODA,R0 TAKEN           ; taken - credited >= 48: a credit
        SUBA,R0 CREDITED
        COMI,R0 48
        BCTR,LT NSW3
        LODA,R0 TXF
        IORI,R0 TX_CREDIT
        STRA,R0 TXF
NSW3:   LODA,R0 SEGRDY
        BCFA,EQ NSW1
        BSTA,UN NPOST
        BCTA,UN NSW1

; ---- the transactions ------------------------------------------------------------
; NTXN: 1. the transaction out: answered? Its answer taken (a READ's bytes
; to decode, a STATUS's bytes waiting and its word on the socket). Calls
; one down. Clobbers R0-R3.
NTXN:   LODA,R0 NKIND
        RETC,EQ
        LODA,R0 NWAIT
        ADDI,R0 1
        STRA,R0 NWAIT
        COMI,R0 NTIMEOUT
        BCTA,EQ NLOST
; NTXN2: NTXN without its frame's count (NSEGW asks again within a frame).
NTXN2:  LODA,R0 NKIND
        RETC,EQ
        BSTA,UN MDONE
        RETC,LT                 ; still out
        RETC,GT
        LODA,R0 NKIND
        STRA,R0 T0
        EORZ    R0
        STRA,R0 NKIND
        BSTA,UN MACK
        BCTR,EQ NT1
        EORZ    R0              ; a NAK: nothing (what is waiting asked again)
        STRA,R0 NBWL
        STRA,R0 NBWH
        RETC,UN
NT1:    LODA,R0 T0
        COMI,R0 NK_READ
        BCFR,EQ NT2
        LODA,R0 *PRXLL          ; a READ's bytes: decode them
        STRA,R0 DLEFT
        LODI,R0 $FF
        STRA,R0 DRPOS
        RETC,UN
NT2:    COMI,R0 NK_STATUS
        RETC,LT
        RETC,GT
        LODA,R0 *PRDATA         ; STATUS: the bytes waiting (LE), to be read
        STRA,R0 NBWL
        LODI,R3 1
        LODA,R0 *PRDATA,R3
        STRA,R0 NBWH
        IORA,R0 NBWL
        RETC,LT
        RETC,GT
        LODI,R3 2               ; ... or none, but connected
        LODA,R0 *PRDATA,R3
        RETC,GT
        RETC,LT
NLOST:  EORZ    R0
        STRA,R0 NLINK
        LODA,R0 NEVT
        IORI,R0 EV_LOST
        STRA,R0 NEVT
        RETC,UN

; NPOST: 3. the next transaction, once nothing is out and the last READ is
; decoded: a WRITE, else a READ of what is waiting, else a STATUS. Calls
; three down. Clobbers R0-R3.
NPOST:  LODA,R0 NKIND
        RETC,GT
        LODA,R0 DLEFT
        RETC,GT
        RETC,LT
        LODA,R0 TXF
        BCFA,EQ NPW
        LODA,R0 NBWL
        IORA,R0 NBWH
        BCFR,EQ POSTR
        LODI,R3 NCSTAT          ; none known waiting: STATUS
        LODI,R2 2
        BSTA,UN MSTARTN
        LODI,R1 0
        BSTA,UN MPARB
        LODI,R1 0
        BSTA,UN MPARB
        LODI,R0 NK_STATUS
        BCTA,UN NPGO
POSTR:  LODA,R0 NBWH            ; READ what is waiting, up to RD_MAX
        BCFR,EQ PR1             ; (256 or more: RD_MAX of it, the rest kept)
        LODA,R0 NBWL
        COMI,R0 RD_MAX
        BCTR,GT PR1
        STRA,R0 T1              ; (all of it)
        EORZ    R0
        STRA,R0 NBWL
        BCTR,UN PR3
PR1:    LODA,R0 NBWL            ; NBW -= RD_MAX
        COMI,R0 RD_MAX
        BCTR,LT PR2
        SUBI,R0 RD_MAX
        STRA,R0 NBWL
        BCTR,UN PR2A
PR2:    ADDI,R0 256-RD_MAX      ; (a borrow from the high byte)
        STRA,R0 NBWL
        LODA,R0 NBWH
        SUBI,R0 1
        STRA,R0 NBWH
PR2A:   LODI,R0 RD_MAX
        STRA,R0 T1
PR3:    LODI,R3 NCREAD
        LODI,R2 1
        BSTA,UN MSTARTN
        LODA,R1 T1
        LODI,R2 0
        BSTA,UN MPARW
        LODI,R0 NK_READ
NPGO:   STRA,R0 NKIND           ; posted: kind R0
        EORZ    R0
        STRA,R0 NWAIT
        BCTA,UN MPOST           ; tail call
NPW:    FARPG   PG_SEND,E_ENTER ; a WRITE of the frames TXF wants (p_send.inc)
        RETC,UN

; NTICK: 4. the clocks: a RESYNC owed, a credit, the keepalive, HELLO
; again, our D again, a silent link given up. QUIET counts frames since a
; good frame: PING at PING_EVERY and twice that, gone past DEAD_AFTER;
; QASK the same mod ASK_AGAIN. Leaf. Clobbers R0.
NTICK:  LODA,R0 RHOLD           ; a RESYNC owed, once the gap is over
        ANDI,R0 $7F
        BCTR,EQ NTK2
        LODA,R0 RHOLD
        SUBI,R0 1
        STRA,R0 RHOLD
        BCTR,UN NTK3
NTK2:   LODA,R0 RHOLD
        TMI,R0  $80
        BCFR,EQ NTK3
        LODI,R0 RESYNC_GAP
        STRA,R0 RHOLD
        LODA,R0 TXF
        IORI,R0 TX_RESYNC
        STRA,R0 TXF
NTK3:   LODA,R0 TAKEN           ; taken - credited >= 48: a credit
        SUBA,R0 CREDITED
        COMI,R0 48
        BCTR,LT NTK4
        LODA,R0 TXF
        IORI,R0 TX_CREDIT
        STRA,R0 TXF
NTK4:   LODA,R0 QUIETL          ; quiet += 1
        ADDI,R0 1
        STRA,R0 QUIETL
        BCFR,EQ NTK4A
        LODA,R0 QUIETH
        ADDI,R0 1
        STRA,R0 QUIETH
NTK4A:  LODA,R0 QUIETH          ; > DEAD_AFTER: gone
        COMI,R0 DEAD_AFTER>>8
        BCTR,LT NTK5
        BCTA,GT NLOST
        LODA,R0 QUIETL
        COMI,R0 DEAD_AFTER&$FF
        BCTA,GT NLOST
NTK5:   LODA,R0 QUIETH          ; PING_EVERY or twice it: a credit and a PING
        COMI,R0 PING_EVERY>>8
        BCFR,EQ NTK5A
        LODA,R0 QUIETL
        COMI,R0 PING_EVERY&$FF
        BCTR,EQ NTKP
        BCTR,UN NTK6
NTK5A:  COMI,R0 (2*PING_EVERY)>>8
        BCFR,EQ NTK6
        LODA,R0 QUIETL
        COMI,R0 (2*PING_EVERY)&$FF
        BCFR,EQ NTK6
NTKP:   LODA,R0 TXF
        IORI,R0 TX_CREDIT|TX_PING
        STRA,R0 TXF
NTK6:   LODA,R0 QASK            ; every ASK_AGAIN: our D again, HELLO again
        ADDI,R0 1
        STRA,R0 QASK
        COMI,R0 ASK_AGAIN
        RETC,LT
        EORZ    R0
        STRA,R0 QASK
        LODA,R0 NLINK
        TMI,R0  NL_DOWED
        BCFR,EQ NTK7
        LODA,R0 TXF
        IORI,R0 TX_DONE
        STRA,R0 TXF
NTK7:   LODA,R0 NLINK
        ANDI,R0 NL_JOINED|NL_HEARD
        COMI,R0 NL_JOINED
        RETC,LT
        RETC,GT
        LODA,R0 TXF
        IORI,R0 TX_HELLO
        STRA,R0 TXF
        RETC,UN

; ---- the decoder -----------------------------------------------------------------------
; DECODE: up to DCHUNK of the READ's bytes; TAKEN counts them (all of a
; chunk: an E frame in it ends the link anyway). The cart RAM stream aimed
; at the run going on (REAIM), closed after. One loop, the common path
; inline (a patch's byte: about 180 cycles). Calls three down. Clobbers
; R0-R3.
DECODE: LODA,R0 DLEFT           ; this chunk: DCHUNK at most
        COMI,R0 DCHUNK+1
        BCTR,LT DE1
        LODI,R0 DCHUNK
DE1:    STRA,R0 DCNT
        STRZ    R1
        ADDA,R0 TAKEN
        STRA,R0 TAKEN
        LODA,R0 DLEFT
        SUBZ    R1
        STRA,R0 DLEFT
        BSTA,UN REAIM
DC1:    LODA,R3 DRPOS
        LODA,R0 *PRDATA,R3,+
        STRA,R3 DRPOS
        BCTA,EQ DCEND           ; 0: a frame's end
        LODA,R1 DCODE
        BCTA,EQ DCCODE
        SUBI,R1 1               ; a data byte
        STRA,R1 DCODE
; emit: into the two held back (D1 the older, D2); the one pushed out is the
; frame's, once two are held
DEM:    LODA,R1 D2
        STRA,R0 D2
        LODA,R0 D1
        STRA,R1 D1
        LODA,R1 DN
        COMI,R1 2
        BCTA,LT DEMN
; the frame's byte (R0): the sums run over it; its type, then its payload
; by type -- a patch's bytes streamed into ST
        STRA,R0 DOUT
        ADDA,R0 S1              ; s1 += b, s2 += s1 (mod 255: 256 is 1, 255 0)
        TPSL    PSLC
        BCFR,EQ DP1
        ADDI,R0 1
DP1:    COMI,R0 255
        BCTR,LT DP2
        EORZ    R0
DP2:    STRA,R0 S1
        ADDA,R0 S2
        TPSL    PSLC
        BCFR,EQ DP3
        ADDI,R0 1
DP3:    COMI,R0 255
        BCTR,LT DP4
        EORZ    R0
DP4:    STRA,R0 S2
        LODA,R0 FIDX
        BCTA,EQ DPTYPE
        LODA,R0 FTYPE
        COMI,R0 CS_PATCH
        BCTA,EQ DPATCH
        LODA,R0 FIDX            ; the rest: the payload's place + 2 (to 255)
        COMI,R0 255
        BCTR,EQ DPO
        ADDI,R0 1
        STRA,R0 FIDX
DPO:    BSTA,UN PAYLOAD
        BCTA,UN DCN
DPTYPE: LODI,R0 1               ; its type
        STRA,R0 FIDX
        LODA,R0 DOUT
        STRA,R0 FTYPE
        EORZ    R0
        STRA,R0 QST             ; (QST, GST)
        STRA,R0 GSKIP
        BCTA,UN DCN
DPATCH: LODA,R0 QST             ; a patch
        COMI,R0 3
        BCTR,EQ DPQ
        BSTA,UN QHDR
        BCTA,UN DCN
DPQ:    LODA,R1 DOUT            ; its run's byte into ST: the stream stores,
        LODA,R0 *PRDAT,R1       ;  steps on
        LODA,R0 QOFFL
        ADDI,R0 1
        STRA,R0 QOFFL
        BCFR,EQ DPQ1
        LODA,R0 QOFFH
        ADDI,R0 1
        STRA,R0 QOFFH
DPQ1:   LODA,R0 QLEN
        SUBI,R0 1
        STRA,R0 QLEN
        BCFR,EQ DCN
        STRA,R0 QST             ; (the run's last: the next run's header)
DCN:    LODA,R0 DCNT            ; the chunk's next
        SUBI,R0 1
        STRA,R0 DCNT
        BCFA,EQ DC1
DCOUT:  BCTA,UN RAMPEND         ; tail call: the stream closed
DEMN:   ADDI,R1 1               ; (not two held yet)
        STRA,R1 DN
        BCTR,UN DCN
DCCODE: STRZ    R2              ; a block's code (R2): its data bytes
        SUBI,R0 1
        STRA,R0 DCODE
        LODA,R1 DFLAG           ; (R1: the last block's)
        LODI,R0 DF_BLK
        COMI,R2 255
        BCFR,EQ DCC1
        LODI,R0 DF_BLK|DF_255
DCC1:   STRA,R0 DFLAG
        COMI,R1 DF_BLK          ; the zero before it, unless it's the frame's
        BCFA,EQ DCN             ;  first or after a 254-byte one
        EORZ    R0
        BCTA,UN DEM
DCEND:  BSTA,UN FREND           ; a frame's end
        LODA,R0 NEVT            ; an error: stop
        TMI,R0  EV_ERROR
        BCTA,EQ DCOUT
        BCTA,UN DCN

; REAIM: the cart RAM stream at the run going on: a patch's (ST + QOFF),
; a script's (SCRIPT + SEGLEN, once its offset is in: FIDX past place 4).
; Leaf (fixed B's). Clobbers R0, R1, R3.
REAIM:  LODA,R0 FIDX
        RETC,EQ
        LODA,R0 FTYPE
        COMI,R0 CS_PATCH
        BCFR,EQ RA2
        LODA,R0 QST
        COMI,R0 3
        RETC,LT
        RETC,GT
AIMQ:   LODA,R1 QOFFH           ; ST is cart offset 0
        ADDI,R1 FRRAMP0
        LODA,R3 QOFFL
        BCTA,UN RAMSET          ; tail call
RA2:    COMI,R0 CS_SCRIPT
        RETC,LT
        RETC,GT
        LODA,R0 FIDX            ; (FIDX 6: place 4 in, its bytes next)
        COMI,R0 2+4
        RETC,LT
        LODA,R0 GSKIP
        RETC,GT
AIMS:   LODI,R1 FRRAMP2         ; SCRIPT is cart offset $200
        LODA,R3 SEGLEN
        BCTA,UN RAMSET          ; tail call

; QHDR: a run's header: its offset (two bytes), its length (past the state:
; a bad run, ignored); then the stream aimed at it. R0 = QST.
QHDR:   COMI,R0 1
        BCTR,LT QH0
        BCTR,EQ QH1
        COMI,R0 2
        RETC,GT                 ; 4: a bad run
        LODA,R0 DOUT            ; its length
        STRA,R0 QLEN
        BCTR,EQ QHZ             ; (no bytes: the next run)
        ADDA,R0 QOFFL           ; its end: within the state?
        LODA,R1 QOFFH
        TPSL    PSLC
        BCFR,EQ QH2
        ADDI,R1 1
QH2:    COMI,R1 STATE_SIZE>>8
        BCTR,LT QHOK
        BCTR,GT QHBAD
        COMI,R0 (STATE_SIZE&$FF)+1
        BCFR,LT QHBAD
QHOK:   LODI,R0 3
        STRA,R0 QST
        BCTA,UN AIMQ            ; tail call
QH0:    LODA,R0 DOUT            ; its offset's low byte
        STRA,R0 QOFFL
        LODI,R0 1
        STRA,R0 QST
        RETC,UN
QH1:    LODA,R0 DOUT            ; its high byte
        STRA,R0 QOFFH
        LODI,R0 2
        STRA,R0 QST
        RETC,UN
QHZ:    EORZ    R0
        STRA,R0 QST
        RETC,UN
QHBAD:  LODI,R0 4
        STRA,R0 QST
        RETC,UN

; PAYLOAD: DOUT, a byte of a frame's payload but a patch's, at its place
; FIDX - 2 (R2). Calls two down.
PAYLOAD: LODA,R0 FIDX
        SUBI,R0 2
        STRZ    R2
        LODA,R0 FTYPE
        COMI,R0 CS_SCRIPT
        BCTA,EQ PSCR
        COMI,R0 CS_TERRAIN
        BCTA,EQ PTERR
        COMI,R0 CS_ROOM
        BCTA,EQ PROOM
        COMI,R0 CS_ERROR
        RETC,LT                 ; (CATALOG, COMMIT, HOLD: nothing kept)
        RETC,GT
        COMI,R2 0               ; E: its code
        RETC,GT
        LODA,R0 DOUT
        STRA,R0 QOFFL
        RETC,UN

; S: sid, seg, flags, off16, then its bytes into SCRIPT at off (net.c's
; CS_SCRIPT): off 0 starts the segment; a frame that isn't the next of
; ours (a gap) is asked again at its end (GSKIP).
PSCR:   COMI,R2 5
        BCTR,LT PSC0
        LODA,R0 GSKIP           ; a byte of it, streamed in
        RETC,GT
        LODA,R0 SEGLEN
        COMI,R0 SCRIPT_BUF
        BCFA,LT PSBAD
        ADDI,R0 1
        STRA,R0 SEGLEN
        LODA,R1 DOUT
        LODA,R0 *PRDAT,R1
        RETC,UN
PSC0:   COMI,R2 4
        BCTR,EQ PSC4
        LODA,R0 DOUT
        COMI,R2 1
        BCTR,LT PSA
        BCTR,EQ PSB
        COMI,R2 3
        BCTR,LT PSF
        STRA,R0 SOFF            ; 3: off's low byte
        RETC,UN
PSA:    STRA,R0 GNEW            ; 0: its shot
        RETC,UN
PSB:    STRA,R0 SSEG            ; 1: its segment
        RETC,UN
PSF:    STRA,R0 GFLG            ; 2: its flags
        RETC,UN
PSC4:   LODA,R0 DOUT            ; off's high byte: 0, or not ours
        BCFR,EQ PSBAD
        LODA,R0 SOFF            ; off 0: a new segment
        BCFR,EQ PSC4A
        STRA,R0 SEGLEN
        STRA,R0 SEGRDY
        CPTR    SEGSID
        LODA,R0 GNEW
        BSTA,UN RAMPUT1
        LODA,R0 SSEG
        BSTA,UN RAMPUT1
        BSTA,UN RAMPEND
        IF      SEGNO <> SEGSID+1
        ERROR   "PSCR: SEGSID, SEGNO must be one run"
        ENDIF
PSC4A:  LODA,R0 GNEW            ; ours, and the next bytes of it?
        COMA,R0 SEGSID
        BCFR,EQ PSBAD
        LODA,R0 SSEG
        COMA,R0 SEGNO
        BCFR,EQ PSBAD
        LODA,R0 SOFF
        COMA,R0 SEGLEN
        BCTA,EQ AIMS            ; tail call: the stream at SCRIPT + SEGLEN
PSBAD:  LODI,R0 1
        STRA,R0 GSKIP
        RETC,UN

; G: base tv, new tv, flags, x16, then a column's entry each (net.c's
; CS_TERRAIN), drawn as it comes: an entry under T_EMPTY is solid from
; that row down, T_EMPTY none, T_SPANS + n then n spans (top, end). A
; base not ours (nor FF, the whole field): skipped, and asked again at its
; end. The columns are drawn only while the field shows (FL_FIELD: the
; text screens share its RAM -- TZ is the field's lower rows), the block
; tanks taken off first (p_play.inc draws them again: NTERR); on a text
; screen they're dropped, and the field asks for all of it when it shows
; (p_play.inc PENTER: tv FF).
PTERR:  COMI,R2 4
        BCTA,GT PTCOLF
        BCTR,EQ PTX4
        COMI,R2 3
        BCTR,EQ PTX
        LODA,R0 DOUT
        COMI,R2 1
        BCTR,LT PTB
        BCTR,EQ PTN
        STRA,R0 GFLG            ; 2: its flags
        RETC,UN
PTN:    STRA,R0 GNEW            ; 1: its new version
        RETC,UN
PTX:    LODA,R0 DOUT            ; 3: x's low byte
        STRA,R0 GX
        RETC,UN
PTB:    COMI,R0 $FF             ; 0: base: FF the whole field (being built:
        BCTR,EQ PTB1            ;  FE), ours, or not ours (skipped)
        COMA,R0 NTV
        RETC,EQ
        LODI,R0 1
        STRA,R0 GSKIP
        RETC,UN
PTB1:   LODA,R0 NTV
        COMI,R0 $FE
        RETC,EQ
        LODI,R0 $FE
        CPUT    NTV
        RETC,UN
; 4: x's high byte: the columns from x (cell column GX, its third GMT), the
; block tanks off; or none (GST $FF)
PTX4:   LODI,R0 $FF
        STRA,R0 GST
        LODA,R0 GSKIP
        RETC,GT
        LODA,R0 DOUT
        RETC,GT
        LODA,R0 FLAGS
        TMI,R0  FL_FIELD
        BCFR,EQ PTX9
        LODA,R0 GX
        COMI,R0 FIELD_W
        BCFR,LT PTX9
        LODI,R1 0
PTX1:   COMI,R0 3
        BCTR,LT PTX2
        SUBI,R0 3
        ADDI,R1 1
        BCTR,UN PTX1
PTX2:   STRZ    R3
        LODA,R0 GMTS,R3
        STRA,R0 GMT
        STRA,R1 GX
        EORZ    R0
        STRA,R0 GST
        STRA,R0 GPN
        BCTA,UN TKOFF           ; tail call
PTX9:   RETC,UN
GMTS:   DB      4,2,1
; 5 on: a column entry's byte: the field's page draws it (PTCOL)
PTCOLF: LODA,R0 GST
        COMI,R0 $FF
        RETC,EQ
        FARPG   PG_FLD,E_COL
        RETC,UN

; T: index, count, then the record into ROOMS at its index's place.
PROOM:  LODA,R0 DOUT
        COMI,R2 1
        BCTR,LT PRM0
        BCTR,EQ PRM1
        SUBI,R2 2               ; a record's byte: ROOMS + index x 24 + place - 2
        COMI,R2 ROOM_REC
        RETC,GT
        RETC,EQ
        LODA,R1 QLEN
        COMI,R1 $FF
        RETC,EQ
        LODA,R0 REC24,R1
        ADDZ    R2
        STRZ    R3
        LODA,R0 DOUT
        LODI,R1 FRRAMP0         ; (ROOMS is ST: cart offset 0)
        BCTA,UN RAMPUT          ; tail call
PRM0:   COMI,R0 MAX_ROOMS       ; the index
        BCTR,LT PRM0A
        LODI,R0 $FF
PRM0A:  STRA,R0 QLEN
        RETC,UN
PRM1:   COMI,R0 MAX_ROOMS+1     ; the count, at most MAX_ROOMS
        BCTR,LT PRM1A
        LODI,R0 MAX_ROOMS
PRM1A:  STRA,R0 QOFFL
        RETC,UN
BITS:   DB      1,2,4,8,$10,$20,$40,$80
REC24:  DB      0,ROOM_REC,2*ROOM_REC,3*ROOM_REC,4*ROOM_REC,5*ROOM_REC,6*ROOM_REC,7*ROOM_REC

; FREND: a frame's end: its sums checked (S1, S2 against the two held
; back), then what its type does; a bad one asks for what it touched again.
; The frame's state reset. Calls one down. Clobbers R0-R3.
FREND:  LODA,R0 FIDX
        BCTA,EQ FEBAD           ; nothing but the sums: malformed
        LODA,R0 DN
        COMI,R0 2
        BCFA,EQ FEBAD
        LODA,R0 S1
        COMA,R0 D1
        BCFA,EQ FEBAD
        LODA,R0 S2
        COMA,R0 D2
        BCFA,EQ FEBAD
        EORZ    R0              ; a good frame: the link is alive, our D heard
        STRA,R0 QUIETH
        STRA,R0 QUIETL
        STRA,R0 QASK
        LODA,R0 NLINK
        ANDI,R0 ~NL_DOWED&$FF
        STRA,R0 NLINK
        LODA,R0 FTYPE
        COMI,R0 CS_COMMIT
        BCTR,EQ FECOM
        COMI,R0 CS_SCRIPT
        BCTA,EQ FESCR
        COMI,R0 CS_TERRAIN
        BCTA,EQ FETERR
        COMI,R0 CS_ERROR
        BCTR,EQ FEERR
        COMI,R0 CS_ROOM
        BCFA,EQ FERESET
        LODA,R0 QOFFL           ; a room: the count, its record in
        STRA,R0 ROOMN
        LODA,R1 QLEN
        COMI,R1 MAX_ROOMS
        BCFA,LT FERESET
        LODA,R0 ROOMHAVE
        IORA,R0 BITS,R1
        STRA,R0 ROOMHAVE
        BCTA,UN FERESET
FECOM:  LODA,R0 NLINK           ; a commit: the state is whole
        TMI,R0  NL_JOINED
        BCFA,EQ FERESET
        IORI,R0 NL_HEARD
        STRA,R0 NLINK
        LODA,R0 NEVT
        IORI,R0 EV_STATE
        STRA,R0 NEVT
        BCTA,UN FERESET
FEERR:  LODA,R0 QOFFL
        CPUT    NERR
        LODA,R0 NEVT
        IORI,R0 EV_ERROR
        STRA,R0 NEVT
        BCTA,UN FERESET
FETERR: LODA,R0 GSKIP           ; terrain: not ours, asked again (the whole field)
        BCFA,EQ FETBAD
        LODA,R0 GST             ; drawn: the columns waiting for their cell
        COMI,R0 $FF             ;  column's others, drawn (p_fld.inc)
        BCTR,EQ FET0
        FARPG   PG_FLD,E_COLEND
FET0:
        LODA,R0 GFLG            ; the last of a version: it's ours
        TMI,R0  G_LAST
        BCFR,EQ FET1
        LODA,R0 GNEW
        CPUT    NTV
FET1:   LODI,R0 1
        CPUT    NTERR
        LODA,R0 NEVT
        IORI,R0 EV_TERR
        STRA,R0 NEVT
        BCTA,UN FERESET
FESCR:  LODA,R0 GSKIP           ; script: a gap, asked again
        BCFA,EQ FEBAD
        LODA,R0 GFLG            ; its last frame: the segment is whole
        TMI,R0  S_LAST_FRAME
        BCFA,EQ FERESET
        LODA,R0 GFLG
        CPUT    SEGFLG
        LODI,R0 1
        STRA,R0 SEGRDY
        LODA,R0 NEVT
        IORI,R0 EV_SCRIPT
        STRA,R0 NEVT
        BCTA,UN FERESET
FETBAD: LODI,R0 $FF             ; the field not to be trusted
        CPUT    NTV
FEBAD:  LODA,R0 FTYPE           ; (a bad terrain frame: the whole field again)
        COMI,R0 CS_TERRAIN
        BCFR,EQ FEB1
        LODI,R0 $FF
        CPUT    NTV
FEB1:   LODA,R0 RHOLD           ; a RESYNC owed
        IORI,R0 $80
        STRA,R0 RHOLD
        LODA,R0 NBAD            ; (counted, for the rigs)
        ADDI,R0 1
        BCTR,EQ FERESET
        CPUT    NBAD
FERESET: EORZ   R0
        STRA,R0 DCODE
        STRA,R0 DFLAG
        STRA,R0 DN
        STRA,R0 FIDX
        STRA,R0 S1
        STRA,R0 S2
        STRA,R0 QST
        STRA,R0 GSKIP
        RETC,UN
