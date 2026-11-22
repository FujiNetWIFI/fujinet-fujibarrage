; p_net.inc -- the link (src/net.c over the cart's mailbox): one step a
; frame that never waits. FujiBlast's (src/astrocade/src/p_net.inc), for
; FujiBarrage's documents (docs/PROTOCOL.md).
;
; The FujiNet's TCP socket is driven one transaction at a time, each posted
; in one frame and seen answered in a later one. The order: a WRITE of
; whatever is to be sent; else, with bytes known waiting (NBW), a READ of
; them, up to RD_MAX; else a STATUS (the bytes waiting; is the socket still
; up). A READ never asks for more than is waiting: firmware before
; afe7dddaa (2026-10-30; the Go Astrocade desktop's pin) takes a short
; read's bytes off the socket and drops them with an error. Every answer
; replaces the reply window -- an empty one too -- so a READ's bytes are
; decoded (a chunk a step) before anything else is posted.
;
; What is to be sent is kept as intents (TXF) and encoded only when a WRITE
; goes out: each frame built in RAWBUF (type, payload, Fletcher-16), its
; COBS streamed into the transaction, then a 0. The server's frames are
; decoded as they come, straight from the reply window: COBS, a running
; Fletcher, the last two bytes held back (they are the sums), and applied
; as they come -- a patch's bytes into ST, a terrain frame's columns into
; the field (the PLAY page draws them), a script's bytes into SCRIPT; the
; CATALOG's dropped (ours is in ROM: p_cat.inc). A bad frame (its sums)
; asks for what it touched again (RESYNC: the state, the field, the segment).
;
; The hold (CAP_HOLD): a segment's last frame says S_HOLD and the server
; sends nothing more until our D; the main loop plays it (the PLAY page)
; with no transaction out, then E_DONE owes the D.

        JP      NSTEP           ; E_ENTER: a frame's step (NEVT: what came)
        JP      NHELLO          ; E_HELLO: in a room: the state zeroed, HELLO
        JP      NINIT           ; E_INIT: a fresh link's counters
        JP      NDONE           ; E_DONE: the segment played: D owed

PING_EVERY EQU  300             ; frames between keepalives (each is answered)
DEAD_AFTER EQU  900             ; frames without a frame: the link is gone
ASK_AGAIN  EQU  60              ; HELLO, or our D, unanswered: again
RESYNC_GAP EQU  30              ; frames between RESYNCs
NTIMEOUT   EQU  240             ; frames a transaction may be out
RD_MAX     EQU  192             ; the most a READ asks for (slice 0 holds 256)
DCHUNK     EQU  24              ; encoded bytes decoded a step
DF_BLK     EQU  1               ; DFLAG: a COBS block began (a zero before the next)
DF_255     EQU  2               ;  a 254-byte one (no zero after it)
FNRPAGE    EQU  3BH             ; the reply's page (FNRDATA)
CATLEN     EQU  1612            ; the CATALOG's bytes (p_cat.inc's, the server's)

; WORK, while sending (ram.inc): the frame being built
RAWBUF  EQU     WORK            ; 33: type, payload (HELLO's 30), the two sums
RAWN    EQU     WORK+34         ; its length
BK      EQU     WORK+35         ; its TXF bit's number
DRPOS0  EQU     WORK+36         ; (decoding) the chunk's start
; a frame's own, while it decodes (one at a time)
GNEW    EQU     QLEN            ; G: the new version; S: the shot
GFLG    EQU     QOFF+1          ; G, S: the flags
SSEG    EQU     QOFF            ; S: the segment
SOFF    EQU     GX              ; S: the offset (its low byte; a buffer < 256)

; ---- entries ---------------------------------------------------------------------
; NINIT: a fresh link: the counters, the decoder, nothing out or owed.
NINIT:  XOR     A
        LD      (NKIND),A
        LD      (NWAIT),A
        LD      HL,NEVT
        LD      B,ROOMHAVE-NEVT+1
NI1:    LD      (HL),A
        INC     HL
        DJNZ    NI1
        LD      A,0FFH
        LD      (ROOMN),A
        LD      (NTV),A
        RET

; NHELLO: sit in the room: the counters, ST zeroed (the state comes whole),
; HELLO due, no seat yet.
NHELLO: CALL    NINIT
        LD      HL,ST
        LD      DE,ST+1
        LD      BC,STATE_SIZE-1
        LD      (HL),0
        LDIR
        LD      A,(NLINK)
        AND     NL_LINK
        OR      NL_JOINED
        LD      (NLINK),A
        LD      A,TX_HELLO
        LD      (TXF),A
        LD      A,NONE
        LD      (MYSEAT),A
        LD      HL,0
        LD      (JOINT),HL
        RET

; NDONE: the segment played (or given up): D(sid, seg, taken) owed, again
; every ASK_AGAIN frames until something comes.
NDONE:  LD      A,(SEGSID)
        LD      (DSID),A
        LD      A,(SEGNO)
        LD      (DSEG),A
        XOR     A
        LD      (SEGRDY),A
        LD      A,1
        LD      (DOWED),A
        LD      A,(TXF)
        OR      TX_DONE
        LD      (TXF),A
        RET

; NSTEP: one frame of the link: the transaction out answered, a chunk
; decoded, the next posted, the clocks; our seat at a commit; no state in
; JOIN_WAIT frames is EV_NOANS.
NSTEP:  XOR     A
        LD      (NEVT),A
        LD      A,(FLAGS)       ; a test scene: nothing comes or goes
        AND     FL_TEST
        JR      Z,NS0
        XOR     A
        LD      (TXF),A
        RET
NS0:    LD      A,(NLINK)
        AND     NL_LINK
        JR      NZ,NS1
        LD      A,EV_LOST
        LD      (NEVT),A
        RET
NS1:    CALL    NTXN            ; 1. the transaction out
        LD      A,(NEVT)
        OR      A
        RET     NZ              ; (lost)
        LD      A,(DLEFT)       ; 2. a chunk decoded
        OR      A
        JR      Z,NS2
        CALL    DECODE
        LD      A,(NEVT)
        AND     EV_ERROR
        RET     NZ              ; (an E frame: the caller's)
NS2:    LD      A,(SEGRDY)      ; 3. the next transaction (none with a segment
        OR      A               ;  to play: the server waits for its D)
        CALL    Z,NPOST
        CALL    NTICK           ; 4. the clocks
        CALL    NSEGW           ; 5. a script waiting: the link flat out
        LD      A,(NEVT)        ; a commit: our seat
        AND     EV_STATE
        JR      Z,NS3
        LD      A,(ST+OFF_MY_SEAT)
        LD      (MYSEAT),A
NS3:    LD      A,(NLINK)       ; joined, nothing heard: JOIN_WAIT
        AND     NL_JOINED|NL_HEARD
        CP      NL_JOINED
        RET     NZ
        LD      HL,(JOINT)
        LD      A,(DT)
        LD      E,A
        LD      D,0
        ADD     HL,DE
        LD      (JOINT),HL
        LD      DE,JOIN_WAIT
        OR      A
        SBC     HL,DE
        RET     C
        LD      A,(NEVT)
        OR      EV_NOANS
        LD      (NEVT),A
        RET

; NSEGW: a script waits for its next segment (nothing else runs then): the
; transactions one after another, each as soon as the last is answered,
; until the segment is whole or the frame is over. (Else one a frame: a
; cut would cost a frame each for STATUS, READ and our credits.)
NSEGW:  LD      A,(SHOWN)
        CP      S_FIELD
        RET     NZ
        LD      A,(INSCR)
        OR      A
        RET     Z
        LD      A,(FRAMES)
        PUSH    AF
NSW1:   LD      A,(SEGRDY)      ; whole, or something else came: done
        OR      A
        JR      NZ,NSW9
        LD      A,(NEVT)
        AND     EV_ERROR|EV_LOST|EV_TERR
        JR      NZ,NSW9
        POP     AF              ; the frame over: done
        PUSH    AF
        LD      HL,FRAMES
        CP      (HL)
        JR      NZ,NSW9
        CALL    NTXN2
        LD      A,(NEVT)
        AND     EV_LOST
        JR      NZ,NSW9
        LD      A,(DLEFT)
        OR      A
        JR      Z,NSW2
        CALL    DECODE
NSW2:   LD      A,(CREDITED)    ; taken - credited >= 48: a credit
        LD      B,A
        LD      A,(TAKEN)
        SUB     B
        CP      48
        JR      C,NSW3
        LD      A,(TXF)
        OR      TX_CREDIT
        LD      (TXF),A
NSW3:   LD      A,(SEGRDY)
        OR      A
        CALL    Z,NPOST
        JR      NSW1
NSW9:   POP     AF
        RET

; ---- the transactions ------------------------------------------------------------
; NTXN: 1. the transaction out: answered? Its answer taken (a READ's bytes
; to decode, a STATUS's bytes waiting and its word on the socket).
NTXN:   LD      A,(NKIND)
        OR      A
        RET     Z
        LD      HL,NWAIT
        INC     (HL)
        LD      A,(HL)
        CP      NTIMEOUT
        JR      Z,NLOST
        JR      NTX1
; NTXN2: NTXN without its frame's count (NSEGW asks within a frame).
NTXN2:  LD      A,(NKIND)
        OR      A
        RET     Z
NTX1:   CALL    MDONE
        RET     NZ              ; still out
        LD      HL,NKIND
        LD      C,(HL)
        LD      (HL),0
        CALL    MACK
        JR      NZ,NT9          ; a NAK: nothing
        LD      A,C
        CP      NK_READ
        JR      NZ,NT2
        LD      A,(FNRXLL)      ; a READ's bytes: decode them
        LD      (DLEFT),A
        XOR     A
        LD      (DRPOS),A
        RET
NT2:    CP      NK_STATUS
        RET     NZ
        LD      HL,(FNRDATA)    ; STATUS: the bytes waiting, to be read
        LD      (NBW),HL
        LD      A,H
        OR      L
        RET     NZ
        LD      A,(FNRDATA+2)   ; ... or none, but connected
        OR      A
        RET     NZ
        JR      NLOST
NT9:    LD      HL,0            ; (what is waiting asked again: a STATUS)
        LD      (NBW),HL
        RET
NLOST:  XOR     A
        LD      (NLINK),A
        LD      A,(NEVT)
        OR      EV_LOST
        LD      (NEVT),A
        RET

; NPOST: 3. the next transaction, once nothing is out and the last READ is
; decoded: a WRITE, else a READ of what is waiting, else a STATUS.
NPOST:  LD      A,(NKIND)
        OR      A
        RET     NZ
        LD      A,(DLEFT)
        OR      A
        RET     NZ
        LD      A,(TXF)
        OR      A
        JP      NZ,POSTW
        LD      HL,(NBW)
        LD      A,H
        OR      L
        JR      NZ,POSTR
        LD      E,NCSTAT        ; none known waiting: STATUS
        LD      L,2
        CALL    MSTARTN
        XOR     A
        CALL    FNPARB
        XOR     A
        CALL    FNPARB
        LD      A,NK_STATUS
        JR      NPGO
POSTR:  LD      DE,RD_MAX       ; READ what is waiting, up to RD_MAX
        OR      A
        SBC     HL,DE
        JR      NC,NPR1         ; (RD_MAX of it, the rest kept)
        ADD     HL,DE           ; (all of it)
        EX      DE,HL
        LD      HL,0
NPR1:   LD      (NBW),HL        ; NBW: what is left
        PUSH    DE              ; DE: the READ's length
        LD      E,NCREAD
        LD      L,1
        CALL    MSTARTN
        POP     HL
        CALL    MPARW
        LD      A,NK_READ
NPGO:   LD      (NKIND),A       ; posted: kind A
        XOR     A
        LD      (NWAIT),A
        JP      MPOST

; POSTW: every frame TXF wants, in one WRITE: its length first (each
; frame's raw length + 2: COBS adds one, then the 0), then the frames.
POSTW:  LD      HL,FLEN
        LD      A,(TXF)
        LD      C,A
        LD      B,8
        XOR     A
PW1:    RR      C
        JR      NC,PW2
        ADD     A,(HL)
PW2:    INC     HL
        DJNZ    PW1
        LD      C,A
        LD      A,(TXF)         ; the action's: its args'
        AND     TX_ACT
        LD      A,C
        JR      Z,PW2A
        LD      A,(ANARG)
        ADD     A,1+2+2+2
        ADD     A,C
PW2A:   PUSH    AF
        LD      E,NCWRITE
        LD      L,1
        CALL    MSTARTN
        POP     AF
        LD      L,A
        LD      H,0
        CALL    MPARW
        XOR     A               ; each frame, in bit order
PW3:    LD      (BK),A
        CALL    BITOF           ; C = its bit
        LD      A,(TXF)
        AND     C
        JR      Z,PW4
        CALL    BUILD
        CALL    COBSOUT
PW4:    LD      A,(BK)
        INC     A
        CP      8
        JR      NZ,PW3
        XOR     A
        LD      (TXF),A
        LD      A,NK_WRITE
        JR      NPGO

; BITOF: C = bit A (0-7). Clobbers A.
BITOF:  LD      C,1
        OR      A
        RET     Z
BO1:    SLA     C
        DEC     A
        JR      NZ,BO1
        RET

; the frames, by TXF bit: raw length + 2 (type, payload, 2 sums; COBS's 1;
; the 0), and the type; the action's length is its args' (POSTW)
FLEN:   DB      1+30+2+2,0,1+3+2+2,1+1+2+2,1+0+2+2,1+8+2+2,1+0+2+2,1+0+2+2
FTYPES: DB      SC_HELLO,SC_ACT,SC_DONE,SC_CREDIT,SC_PING,SC_RESYNC,SC_ROOMS,SC_BYE

; BUILD: frame BK (a TXF bit's number) into RAWBUF: its type, its payload,
; then the two Fletcher sums; RAWN its length.
BUILD:  LD      A,(BK)
        LD      C,A
        LD      B,0
        LD      HL,FTYPES
        ADD     HL,BC
        LD      A,(HL)
        LD      DE,RAWBUF
        LD      (DE),A
        INC     DE
        LD      A,C
        OR      A
        JR      Z,BHELLO
        DEC     A
        JR      Z,BACT
        DEC     A
        JR      Z,BDONE
        DEC     A
        JR      Z,BCRED
        DEC     A
        JR      Z,BSUM          ; PING
        DEC     A
        JR      Z,BRESY
        JR      BSUM            ; the rest: no payload
BHELLO: LD      HL,HELLO0       ; version, flags
        LD      BC,2
        LDIR
        LD      HL,SRVTBL       ; the room's id, our name (NUL-padded)
        LD      BC,ROOM_LEN
        LDIR
        LD      HL,MYNAME
        LD      BC,NAME_LEN
        LDIR
        LD      HL,HELLO1       ; our shape
        LD      BC,12
        LDIR
        JR      BSUM
BACT:   LD      HL,ASEQ         ; seq, act, its args
        LD      A,(ANARG)
        ADD     A,2
        LD      C,A
        LD      B,0
        LDIR
        JR      BSUM
BDONE:  LD      A,(DSID)        ; sid, seg, taken (credit starts again from it)
        LD      (DE),A
        INC     DE
        LD      A,(DSEG)
        LD      (DE),A
        INC     DE
BCRED:  LD      A,(TAKEN)       ; what we've taken (and credited with it)
        LD      (CREDITED),A
        LD      (DE),A
        INC     DE
        JR      BSUM
BRESY:  LD      A,(TAKEN)       ; taken, tv, sid, seg, off16 0, cat16
        LD      (CREDITED),A
        LD      (DE),A
        INC     DE
        LD      A,(NTV)
        LD      (DE),A
        INC     DE
        LD      A,(SEGSID)
        LD      (DE),A
        INC     DE
        LD      A,(SEGNO)
        LD      (DE),A
        INC     DE
        XOR     A
        LD      (DE),A
        INC     DE
        LD      (DE),A
        INC     DE
        LD      (SEGLEN),A      ; (the segment again from its start)
        LD      HL,(ST+OFF_CAT_LEN)     ; all of the catalog: ours is in ROM
        LD      A,H
        OR      L
        JR      NZ,BRS1
        LD      HL,CATLEN
BRS1:   EX      DE,HL
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        EX      DE,HL
BSUM:   EX      DE,HL           ; Fletcher-16 over RAWBUF up to DE
        LD      DE,RAWBUF
        OR      A
        SBC     HL,DE
        LD      B,L             ; its length
        LD      A,L
        ADD     A,2
        LD      (RAWN),A        ; and the sums
        EX      DE,HL
        LD      C,0             ; s1
        LD      D,0             ; s2
BS1:    LD      A,C
        ADD     A,(HL)
        CALL    MOD255
        LD      C,A
        ADD     A,D
        CALL    MOD255
        LD      D,A
        INC     HL
        DJNZ    BS1
        LD      (HL),C
        INC     HL
        LD      (HL),D
        RET
HELLO0: DB      PROTO_VERSION,0
HELLO1: DB      FIELD_W,0,FIELD_H,PAR,60,TANK_W,TANK_H,SCRIPT_BUF,0,2,CAP_HOLD,PLAT_ASTROCADE

; MOD255: A, just added (CY its carry), mod 255: a carry is 1 (256 is 1
; mod 255), and 255 is 0.
MOD255: JR      NC,MOD1
        INC     A
MOD1:   CP      255
        RET     NZ
        XOR     A
        RET

; COBSOUT: RAWBUF's RAWN bytes as COBS into the TX stream, then a 0: each
; run up to a zero (or the end) is its length + 1, then its bytes.
COBSOUT: LD     HL,RAWBUF       ; HL: the run's start
        LD      A,(RAWN)
        LD      B,A             ; bytes left
CO1:    PUSH    HL              ; a run: up to a zero or the end
        LD      C,0
CO2:    LD      A,B
        OR      A
        JR      Z,CO3
        LD      A,(HL)
        OR      A
        JR      Z,CO3
        INC     HL
        INC     C
        DEC     B
        JR      CO2
CO3:    POP     HL
        LD      A,C             ; its code: length + 1
        INC     A
        CALL    FNTXBYT
        LD      A,C
        OR      A
        JR      Z,CO5
CO4:    LD      A,(HL)
        CALL    FNTXBYT
        INC     HL
        DEC     C
        JR      NZ,CO4
CO5:    LD      A,B             ; the end: the frame's 0
        OR      A
        JR      Z,CO6
        INC     HL              ; a zero: past it, the next run
        DEC     B
        JR      CO1
CO6:    XOR     A
        JP      FNTXBYT

; NTICK: 4. the clocks: a RESYNC owed, a credit, the keepalive, HELLO
; again, our D again, a silent link given up.
NTICK:  LD      HL,RHOLD        ; a RESYNC owed, once the gap is over
        LD      A,(HL)
        AND     7FH
        JR      Z,NTK2
        DEC     (HL)
        JR      NTK3
NTK2:   BIT     7,(HL)
        JR      Z,NTK3
        LD      (HL),RESYNC_GAP
        LD      A,(TXF)
        OR      TX_RESYNC
        LD      (TXF),A
NTK3:   LD      A,(CREDITED)    ; taken - credited >= 48: a credit
        LD      B,A
        LD      A,(TAKEN)
        SUB     B
        CP      48
        JR      C,NTK4
        LD      A,(TXF)
        OR      TX_CREDIT
        LD      (TXF),A
NTK4:   LD      HL,(QUIET)      ; quiet > DEAD_AFTER: gone
        INC     HL
        LD      (QUIET),HL
        LD      DE,DEAD_AFTER+1
        OR      A
        SBC     HL,DE
        JP      NC,NLOST
        LD      HL,(QPING)      ; every PING_EVERY: a credit and a PING
        INC     HL
        LD      (QPING),HL
        LD      DE,PING_EVERY
        OR      A
        SBC     HL,DE
        JR      NZ,NTK6
        LD      (QPING),HL
        LD      A,(TXF)
        OR      TX_CREDIT|TX_PING
        LD      (TXF),A
NTK6:   LD      A,(DOWED)       ; our D unanswered for ASK_AGAIN: again
        OR      A
        JR      Z,NTK7
        INC     A
        LD      (DOWED),A
        CP      ASK_AGAIN
        JR      C,NTK7
        LD      A,1
        LD      (DOWED),A
        LD      A,(TXF)
        OR      TX_DONE
        LD      (TXF),A
NTK7:   LD      HL,QASK         ; every ASK_AGAIN: HELLO, if unanswered
        INC     (HL)
        LD      A,(HL)
        CP      ASK_AGAIN
        RET     C
        LD      (HL),0
        LD      A,(NLINK)
        AND     NL_JOINED|NL_HEARD
        CP      NL_JOINED
        RET     NZ
        LD      A,(TXF)
        OR      TX_HELLO
        LD      (TXF),A
        RET

; ---- the decoder -----------------------------------------------------------------------
; DECODE: up to DCHUNK of the READ's bytes (all while a script waits);
; TAKEN counts them. One loop, the
; common path (a patch's byte) inline. HL walks the reply (3B00H + DRPOS),
; B counts the chunk.
DECODE: LD      B,DCHUNK        ; this chunk: DCHUNK at most; all of the READ
        LD      A,(SHOWN)       ;  while a script waits for its next segment
        CP      S_FIELD         ;  (nothing else runs then: the stall at a
        JR      NZ,DE0          ;  segment's cut is the link's alone)
        LD      A,(INSCR)
        OR      A
        JR      Z,DE0
        LD      B,0FFH
DE0:    LD      A,(DLEFT)
        CP      B
        JR      C,DE1
        LD      A,B
DE1:    LD      B,A
        LD      C,A
        LD      A,(DLEFT)
        SUB     C
        LD      (DLEFT),A
        LD      A,(DRPOS)
        LD      (DRPOS0),A
        LD      L,A
        LD      H,FNRPAGE
DC1:    LD      A,(HL)
        INC     L
        OR      A
        JP      Z,DCEND         ; 0: a frame's end
        LD      C,A
        LD      A,(DCODE)
        OR      A
        JP      Z,DCCODE
        DEC     A               ; a data byte
        LD      (DCODE),A
        LD      A,C
; emit: into the two held back (D1 the older, D2); the one pushed out is the
; frame's, once two are held
DEM:    LD      C,A
        LD      A,(DN)
        CP      2
        JP      C,DEMN
        LD      A,(D1)
        LD      E,A             ; E: the frame's byte
        LD      A,(D2)
        LD      (D1),A
        LD      A,C
        LD      (D2),A
        LD      A,(S1)          ; s1 += b, s2 += s1 (mod 255)
        ADD     A,E
        JR      NC,DP1
        INC     A
DP1:    CP      255
        JR      NZ,DP2
        XOR     A
DP2:    LD      (S1),A
        LD      C,A
        LD      A,(S2)
        ADD     A,C
        JR      NC,DP3
        INC     A
DP3:    CP      255
        JR      NZ,DP4
        XOR     A
DP4:    LD      (S2),A
        LD      A,(FIDX)
        OR      A
        JR      Z,DPTYPE
        LD      A,(FTYPE)
        CP      CS_PATCH
        JR      Z,DPATCH
        LD      A,(FIDX)        ; the rest: the payload's place + 2 (to 255)
        INC     A
        JR      Z,DPO
        LD      (FIDX),A
DPO:    PUSH    BC
        PUSH    HL
        CALL    PAYLOAD
        POP     HL
        POP     BC
        JR      DCN
DPTYPE: LD      A,1             ; its type
        LD      (FIDX),A
        LD      A,E
        LD      (FTYPE),A
        XOR     A
        LD      (QST),A
        LD      (GST),A
        LD      (GSKIP),A
        JR      DCN
DPATCH: LD      A,(QST)         ; a patch
        CP      3
        JR      Z,DPQ
        CALL    QHDR
        JR      DCN
DPQ:    PUSH    HL              ; its run's byte, into ST
        LD      HL,(QOFF)
        PUSH    DE
        LD      DE,ST
        ADD     HL,DE
        POP     DE
        LD      (HL),E
        LD      HL,(QOFF)
        INC     HL
        LD      (QOFF),HL
        POP     HL
        LD      A,(QLEN)
        DEC     A
        LD      (QLEN),A
        JR      NZ,DCN
        LD      (QST),A         ; (the run's last: the next run's header)
DCN:    DEC     B               ; the chunk's next
        JP      NZ,DC1
DCOUT:  LD      A,L             ; taken += the bytes decoded
        LD      (DRPOS),A
        LD      HL,DRPOS0
        SUB     (HL)
        LD      HL,TAKEN
        ADD     A,(HL)
        LD      (HL),A
        RET
DEMN:   INC     A               ; (not two held yet)
        LD      (DN),A
        LD      A,(D2)          ; the delay line still steps
        LD      (D1),A
        LD      A,C
        LD      (D2),A
        JR      DCN
DCCODE: LD      A,C             ; a block's code (C): its data bytes
        DEC     A
        LD      (DCODE),A
        LD      A,(DFLAG)       ; (D: the last block's)
        LD      D,A
        LD      A,C
        CP      255
        LD      A,DF_BLK
        JR      NZ,DCC1
        LD      A,DF_BLK|DF_255
DCC1:   LD      (DFLAG),A
        LD      A,D             ; the zero before it, unless it's the frame's
        CP      DF_BLK          ;  first or after a 254-byte one
        JR      NZ,DCN
        XOR     A
        JP      DEM
DCEND:  PUSH    BC              ; a frame's end
        PUSH    HL
        CALL    FREND
        POP     HL
        POP     BC
        LD      A,(NEVT)        ; an error: stop
        AND     EV_ERROR
        JR      NZ,DCOUT
        JR      DCN

; QHDR: a run's header: its offset (two bytes), its length (past the state:
; a bad run, ignored). A = QST, E the byte. Preserves BC, HL.
QHDR:   OR      A
        JR      NZ,QH1
        LD      A,E             ; its offset's low byte
        LD      (QOFF),A
        LD      A,1
        LD      (QST),A
        RET
QH1:    CP      1
        JR      NZ,QH2
        LD      A,E             ; its high byte
        LD      (QOFF+1),A
        LD      A,2
        LD      (QST),A
        RET
QH2:    CP      2
        RET     NZ              ; 4: a bad run
        LD      A,E             ; its length
        LD      (QLEN),A
        OR      A
        JR      Z,QHZ           ; (no bytes: the next run)
        PUSH    HL              ; offset + length within the state?
        PUSH    DE
        LD      HL,(QOFF)
        LD      D,0
        ADD     HL,DE
        LD      DE,STATE_SIZE+1
        OR      A
        SBC     HL,DE
        POP     DE
        POP     HL
        JR      NC,QHBAD
        LD      A,3
        LD      (QST),A
        RET
QHZ:    XOR     A
        LD      (QST),A
        RET
QHBAD:  LD      A,4
        LD      (QST),A
        RET

; PAYLOAD: a byte (E) of a frame's payload but a patch's: its place FIDX - 2.
; Preserves nothing it needn't (the caller keeps BC, HL).
PAYLOAD: LD     A,(FIDX)
        SUB     2
        LD      C,A             ; C: the payload's place (to 253)
        LD      A,(FTYPE)
        CP      CS_TERRAIN
        JP      Z,PTERR
        CP      CS_SCRIPT
        JP      Z,PSCR
        CP      CS_ROOM
        JR      Z,PROOM
        CP      CS_ERROR
        RET     NZ              ; (CATALOG, COMMIT, HOLD: nothing kept)
        LD      A,C             ; E: its code
        OR      A
        RET     NZ
        LD      A,E
        LD      (QOFF),A
        RET
; T: index, count, then the record into ROOMS at its index's place
PROOM:  LD      A,C
        OR      A
        JR      NZ,PRM2
        LD      A,E             ; the index
        CP      MAX_ROOMS
        JR      C,PRM1
        LD      A,0FFH
PRM1:   LD      (QLEN),A
        RET
PRM2:   CP      1
        JR      NZ,PRM3
        LD      A,E             ; the count, at most MAX_ROOMS
        CP      MAX_ROOMS+1
        JR      C,PRM2A
        LD      A,MAX_ROOMS
PRM2A:  LD      (QOFF),A
        RET
PRM3:   SUB     2               ; a record's byte: ROOMS + index x 24 + place - 2
        CP      ROOM_REC
        RET     NC
        LD      C,A
        LD      A,(QLEN)
        CP      0FFH
        RET     Z
        LD      L,A             ; 24 index
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      D,H
        LD      A,L
        ADD     HL,HL
        ADD     A,L
        LD      L,A
        LD      A,D
        ADC     A,H
        LD      H,A
        LD      B,0
        ADD     HL,BC
        LD      BC,ROOMS
        ADD     HL,BC
        LD      (HL),E
        RET

; G: base tv, new tv, flags, x16, then a column's entry each (net.c's
; CS_TERRAIN): drawn as they come (the PLAY page's columns). A base not
; ours (nor FF, the whole field): skipped, and asked again at its end.
PTERR:  LD      A,C
        CP      5
        JR      NC,PTE5
        OR      A
        JR      NZ,PTE1
        LD      A,E             ; base: FF the whole field (being built: FE),
        CP      0FFH            ;  ours, or not ours (skipped)
        JR      Z,PTE0
        LD      A,(NTV)
        CP      E
        JR      NZ,PTEX
        LD      A,(SHOWN)       ; ours, on the field with a script under way:
        CP      S_FIELD         ;  the server stopped waiting for our D (a
        RET     NZ              ;  skip). The shot is over for us, but our field
        LD      A,(INSCR)       ;  is part-way through it, where this diff
        OR      A               ;  from its start can't reach: the script
        RET     Z               ;  ended, the whole field again (R, tv FF)
        FAR     PG_PLAY,PSCREND
PTEX:   LD      A,1
        LD      (GSKIP),A
        RET
PTE0:   LD      A,(NTV)         ; (its first frame: a text screen blanked)
        CP      0FEH
        RET     Z
        LD      A,0FEH
        LD      (NTV),A
        LD      A,PG_PLAY
        LD      HL,E_LOAD
        JP      PGCALL
PTE1:   CP      1
        JR      NZ,PTE2
        LD      A,E             ; its new version
        LD      (GNEW),A
        RET
PTE2:   CP      2
        JR      NZ,PTE3
        LD      A,E             ; its flags
        LD      (GFLG),A
        RET
PTE3:   CP      3
        JR      NZ,PTE4
        LD      A,E             ; x: the low byte (the field is 160 wide)
        LD      (GX),A
        RET
PTE4:   LD      A,E             ; x's high byte: past the field if not 0
        OR      A
        RET     Z
        LD      A,0FFH
        LD      (GX),A
        RET
PTE5:   LD      A,(GSKIP)       ; a column's entry
        OR      A
        RET     NZ
        LD      A,(GST)
        OR      A
        JR      NZ,PTE8
        LD      A,(GX)          ; its first byte: all sky, then
        LD      B,A
        CP      FIELD_W
        JR      NC,PTE6A
        PUSH    DE
        LD      A,PG_PLAY
        LD      HL,E_COLSKY
        CALL    PGCALL
        POP     DE
PTE6A:  LD      A,E
        CP      0F0H
        JR      NC,PTE7
        LD      D,E             ; dirt from row E down
        LD      E,FIELD_H
        CALL    PCRUN
        JR      PTENEXT
PTE7:   SUB     0F0H            ; F0: no dirt; F0+n: n spans follow
        JR      Z,PTENEXT
        LD      (GSP),A
        LD      A,1
        LD      (GST),A
        RET
PTE8:   CP      1
        JR      NZ,PTE9
        LD      A,E             ; a span's top
        LD      (GTOP),A
        LD      A,2
        LD      (GST),A
        RET
PTE9:   LD      A,(GTOP)        ; its end
        LD      D,A
        CALL    PCRUN
        LD      A,1
        LD      (GST),A
        LD      HL,GSP
        DEC     (HL)
        RET     NZ
PTENEXT: XOR    A               ; the next column
        LD      (GST),A
        LD      HL,GX
        INC     (HL)
        RET     NZ
        DEC     (HL)            ; (stays past the field)
        RET
; PCRUN: column GX, rows D to E-1 dirt (none past the field).
PCRUN:  LD      A,(GX)
        CP      FIELD_W
        RET     NC
        LD      B,A
        LD      A,PG_PLAY
        LD      HL,E_COLRUN
        JP      PGCALL

; S: sid, seg, flags, off16, then its bytes into SCRIPT at off (net.c's
; CS_SCRIPT): off 0 starts the segment; a frame that isn't the next of
; ours (a gap) is asked again at its end (GSKIP).
PSCR:   LD      A,C
        CP      5
        JR      NC,PSC5
        OR      A
        JR      NZ,PSC1
        LD      A,E
        LD      (GNEW),A        ; (its shot)
        RET
PSC1:   CP      1
        JR      NZ,PSC2
        LD      A,E
        LD      (SSEG),A
        RET
PSC2:   CP      2
        JR      NZ,PSC3
        LD      A,E
        LD      (GFLG),A
        RET
PSC3:   CP      3
        JR      NZ,PSC4
        LD      A,E
        LD      (SOFF),A
        RET
PSC4:   LD      A,E             ; off's high byte: 0, or not ours
        OR      A
        JR      NZ,PSCBAD
        LD      A,(SOFF)        ; off 0: a new segment
        OR      A
        JR      NZ,PSC4A
        LD      (SEGLEN),A
        LD      (SEGRDY),A
        LD      A,(GNEW)
        LD      (SEGSID),A
        LD      A,(SSEG)
        LD      (SEGNO),A
PSC4A:  LD      A,(GNEW)        ; ours, and the next bytes of it?
        LD      HL,SEGSID
        CP      (HL)
        JR      NZ,PSCBAD
        LD      A,(SSEG)
        LD      HL,SEGNO
        CP      (HL)
        JR      NZ,PSCBAD
        LD      A,(SOFF)
        LD      HL,SEGLEN
        CP      (HL)
        RET     Z
PSCBAD: LD      A,1
        LD      (GSKIP),A
        RET
PSC5:   LD      A,(GSKIP)       ; a byte of it
        OR      A
        RET     NZ
        LD      A,(SEGLEN)
        CP      SCRIPT_BUF
        JR      NC,PSCBAD
        LD      C,A
        LD      B,0
        LD      HL,SCRIPT
        ADD     HL,BC
        LD      (HL),E
        INC     A
        LD      (SEGLEN),A
        RET

; FREND: a frame's end: its sums checked (S1, S2 against the two held
; back), then what its type does; a bad one asks for what it touched again.
; The frame's state reset.
FREND:  LD      A,(FIDX)
        OR      A
        JP      Z,FEBAD         ; nothing but the sums: malformed
        LD      A,(DN)
        CP      2
        JP      NZ,FEBAD
        LD      A,(D1)
        LD      HL,S1
        CP      (HL)
        JP      NZ,FEBAD
        LD      A,(D2)
        LD      HL,S2
        CP      (HL)
        JP      NZ,FEBAD
        LD      HL,0            ; a good frame: the link is alive, our D heard
        LD      (QUIET),HL
        LD      (QPING),HL
        XOR     A
        LD      (QASK),A
        LD      (DOWED),A
        LD      A,(FTYPE)
        CP      CS_COMMIT
        JR      Z,FECOM
        CP      CS_ERROR
        JR      Z,FEERR
        CP      CS_TERRAIN
        JR      Z,FETERR
        CP      CS_SCRIPT
        JR      Z,FESCR
        CP      CS_ROOM
        JP      NZ,FERESET
        LD      A,(QOFF)        ; a room: the count, its record in
        LD      (ROOMN),A
        LD      A,(QLEN)
        CP      MAX_ROOMS
        JP      NC,FERESET
        CALL    BITOF
        LD      A,(ROOMHAVE)
        OR      C
        LD      (ROOMHAVE),A
        JP      FERESET
FECOM:  LD      A,(NLINK)       ; a commit: the state is whole
        BIT     1,A             ; NL_JOINED
        JP      Z,FERESET
        OR      NL_HEARD
        LD      (NLINK),A
        LD      A,(NEVT)
        OR      EV_STATE
        LD      (NEVT),A
        JP      FERESET
FEERR:  LD      A,(QOFF)
        LD      (NERR),A
        LD      A,(NEVT)
        OR      EV_ERROR
        LD      (NEVT),A
        JP      FERESET
FETERR: LD      A,(GSKIP)       ; terrain: not ours, asked again (the whole field)
        OR      A
        JR      NZ,FETBAD
        LD      A,(GFLG)        ; the last of a version: it's ours
        AND     G_LAST
        JR      Z,FET1
        LD      A,(GNEW)
        LD      (NTV),A
FET1:   LD      A,1
        LD      (NTERR),A
        LD      A,(NEVT)
        OR      EV_TERR
        LD      (NEVT),A
        JP      FERESET
FESCR:  LD      A,(GSKIP)       ; script: a gap, asked again
        OR      A
        JP      NZ,FEBAD
        LD      A,(GFLG)        ; its last frame: the segment is whole
        AND     S_LAST_FRAME
        JP      Z,FERESET
        LD      A,(GFLG)
        LD      (SEGFLG),A
        LD      A,1
        LD      (SEGRDY),A
        LD      A,(NEVT)
        OR      EV_SCRIPT
        LD      (NEVT),A
        JP      FERESET
FETBAD: LD      A,0FFH          ; the field not to be trusted
        LD      (NTV),A
FEBAD:  LD      A,(FTYPE)       ; (a bad terrain frame: the whole field again)
        CP      CS_TERRAIN
        JR      NZ,FEB1
        LD      A,0FFH
        LD      (NTV),A
FEB1:   LD      HL,RHOLD        ; a RESYNC owed
        SET     7,(HL)
        LD      HL,NBAD         ; (counted, for the rigs)
        INC     (HL)
        JP      NZ,FERESET
        DEC     (HL)
FERESET: XOR    A
        LD      (DCODE),A
        LD      (DFLAG),A
        LD      (DN),A
        LD      (FIDX),A
        LD      (S1),A
        LD      (S2),A
        LD      (QST),A
        RET
