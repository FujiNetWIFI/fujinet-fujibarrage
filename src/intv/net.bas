' ===========================================================================
' net.bas -- the stream protocol (docs/PROTOCOL.md) over the link
' (nstrans.bas), as src/net.c, after FujiBlast's net.bas.
'
' The server pushes the state as patches, then a commit, when the state is
' whole. The patches go straight into ST (PATCH, netasm.asm: src/net.c
' stages them, but copying 378 words costs an Intellivision a frame): the
' screen is picked only at a commit. The catalog comes once, in pieces
' (CATPUT packs it); the field as TERRAIN columns (fld.asm's TERRAIN draws
' them); a shot as SCRIPT segments, one at a time, each after our D for the
' last. We drain the link every pass and say what we took (SC_CREDIT), so
' it never sends more than we hold.
'
' We are the server's first client without CAP_HOLD: the link stays up
' while a segment plays (we just decode little then). If the server gives
' up waiting and skips us to a shot's end, the field comes as TERRAIN while
' our script still plays: we stop it and ask for the whole field again.
' ===========================================================================

    CONST PING_EVERY = 300      ' passes between keepalives (each is answered)
    CONST DEAD_AFTER = 900      ' passes without a frame: the link is gone
    CONST ASK_AGAIN  = 60       ' HELLO, or our D, unanswered: again
    CONST RESYNC_GAP = 30       ' passes between RESYNCs
    CONST DECODE_MOST = 160     ' bytes decoded a pass off the field,
    CONST DECODE_PLAY = 24      ' and while a script plays
    CONST NET_NONE   = 0
    CONST NET_STATE  = 1
    CONST NET_ERROR  = 2
    CONST NET_LOST   = 3
    CONST NET_SCRIPT = 4
    CONST MAX_ROOMS  = 8

    DIM link, joined, seq, credited, resync_hold, resync_owed
    DIM d_owed, d_sid, d_seg, q_d
    DIM room_n, room_have
    DIM net_err, net_heard, net_r, net_tv, net_terrain, net_skipped
    DIM seg_ready, seg_sid, seg_no, seg_flags, in_script, dec_budget
    DIM #quiet, #q_ping, q_ask
    DIM #cat_len, #seg_len
    DIM ns_n, ns_t              ' a frame's payload length and type (net_send)

' net_send: frame ns_t with the ns_n bytes at RAWB + 1 into TXQ.
net_send: PROCEDURE
    ns_n = USR SEND(ns_t, ns_n)
END

send_credit: PROCEDURE
    credited = taken
    POKE RAWB + 1, credited
    ns_t = SC_CREDIT : ns_n = 1 : GOSUB net_send
END

' net_resync: what we have (taken, terrain version, the segment we hold, the
' catalog's length): the server sends the rest again.
net_resync: PROCEDURE
    resync_owed = 1
    IF resync_hold THEN RETURN
    POKE RAWB + 1, taken
    POKE RAWB + 2, net_tv
    POKE RAWB + 3, seg_sid
    POKE RAWB + 4, seg_no
    POKE RAWB + 5, 0
    POKE RAWB + 6, 0
    POKE RAWB + 7, #cat_len AND 255
    POKE RAWB + 8, #cat_len / 256
    credited = taken
    ns_t = SC_RESYNC : ns_n = 8 : GOSUB net_send
    #seg_len = 0
    resync_hold = RESYNC_GAP
    resync_owed = 0
END

' net_open: link = 1 if the server answered.
net_open: PROCEDURE
    IF link THEN RETURN
    joined = 0
    credited = 0
    #quiet = 0 : #q_ping = 0 : q_ask = 0
    GOSUB nst_open
    link = 1 - nst_dead
END

' HELLO: version, flags, room, name, then our shape (docs/PROTOCOL.md).
hello_frame: PROCEDURE
    FOR i = 1 TO HELLO_LEN
        POKE RAWB + i, 0
    NEXT i
    POKE RAWB + 1, PROTO_VERSION
    i = 0
    WHILE i < ROOM_LEN
        c = rb(SRVTBL + i)
        IF c = 0 THEN EXIT WHILE
        POKE RAWB + 3 + i, c
        i = i + 1
    WEND
    i = 0
    WHILE i < NAME_LEN
        c = rb(MYNAME + i)
        IF c = 0 THEN EXIT WHILE
        POKE RAWB + 11 + i, c
        i = i + 1
    WEND
    POKE RAWB + 19, FIELD_W
    POKE RAWB + 21, FIELD_H
    POKE RAWB + 22, PAR
    IF NTSC THEN POKE RAWB + 23, 60 ELSE POKE RAWB + 23, 50
    POKE RAWB + 24, TANK_W
    POKE RAWB + 25, TANK_H
    POKE RAWB + 26, SCRIPT_BUF AND 255
    POKE RAWB + 27, SCRIPT_BUF / 256
    POKE RAWB + 28, BANDS
    POKE RAWB + 29, 0                   ' no CAP_HOLD
    POKE RAWB + 30, PLAT_INTV
    ns_t = SC_HELLO : ns_n = HELLO_LEN : GOSUB net_send
END

net_hello: PROCEDURE
    joined = 1
    net_heard = 0
    net_err = 0
    #quiet = 0 : #q_ping = 0 : q_ask = 0
    resync_owed = 0
    #cat_len = 0
    net_tv = $FF
    #seg_len = 0 : seg_ready = 0 : seg_sid = 0 : in_script = 0
    FOR #a = ST TO ST + STATE_SIZE - 1
        POKE #a, 0
    NEXT #a
    GOSUB hello_frame
END

net_drop: PROCEDURE
    GOSUB nst_close
    link = 0 : joined = 0
END

' send_d: segment d_seg of shot d_sid played (d_sid 0: a W hold over),
' and what we took: the server's credit starts again from there.
send_d: PROCEDURE
    POKE RAWB + 1, d_sid
    POKE RAWB + 2, d_seg
    POKE RAWB + 3, taken
    credited = taken
    ns_t = SC_DONE : ns_n = 3 : GOSUB net_send
    d_owed = 1 : q_d = 0 : #quiet = 0
END

' net_done: the segment in SCRIPT is played.
net_done: PROCEDURE
    d_sid = seg_sid : d_seg = seg_no
    seg_ready = 0
    GOSUB send_d
END

' net_poll: a pass of the link. net_r = NET_NONE, NET_STATE (a commit came:
' ST is whole), NET_SCRIPT (a segment is whole in SCRIPT), NET_ERROR
' (net_err) or NET_LOST. Decodes dec_budget bytes at most.
net_poll: PROCEDURE
    net_r = NET_NONE
    IF link = 0 THEN net_r = NET_LOST : RETURN
    GOSUB nst_step
    IF nst_dead THEN GOSUB net_drop : net_r = NET_LOST : RETURN
    IF resync_hold THEN
        resync_hold = resync_hold - 1
    ELSEIF resync_owed THEN
        GOSUB net_resync
    END IF
    dec_left = dec_budget
    WHILE 1
        n = USR DECODE
        IF n = 0 THEN EXIT WHILE
        #quiet = 0 : #q_ping = 0 : q_ask = 0
        d_owed = 0
        IF n = 255 THEN
            net_tv = $FF
            GOSUB net_resync
        ELSE
            GOSUB net_frame
            IF net_r = NET_ERROR OR net_r = NET_SCRIPT THEN EXIT WHILE
        END IF
    WEND
    IF net_r = NET_ERROR THEN RETURN
    IF ((taken - credited) AND 255) >= 48 THEN GOSUB send_credit
    #quiet = #quiet + 1
    #q_ping = #q_ping + 1
    IF #q_ping = PING_EVERY THEN
        #q_ping = 0
        GOSUB send_credit
        ns_t = SC_PING : ns_n = 0 : GOSUB net_send
    END IF
    q_ask = q_ask + 1
    IF q_ask = ASK_AGAIN THEN
        q_ask = 0
        IF joined AND net_heard = 0 THEN GOSUB hello_frame
    END IF
    IF d_owed THEN
        q_d = q_d + 1
        IF q_d = ASK_AGAIN THEN GOSUB send_d   ' our D may have been lost
    END IF
    IF #quiet > DEAD_AFTER THEN GOSUB net_drop : net_r = NET_LOST
END

' net_frame: the frame in NBUF (n bytes: type, payload).
net_frame: PROCEDURE
    c = rb(NBUF)
    IF c = CS_PATCH THEN
        CALL PATCH(n)
    ELSEIF c = CS_COMMIT THEN
        IF joined THEN
            net_heard = 1
            IF net_r = NET_NONE THEN net_r = NET_STATE
        END IF
    ELSEIF c = CS_CATALOG THEN
        CALL CATPUT(n)
    ELSEIF c = CS_TERRAIN THEN
        GOSUB net_terrain_frame
    ELSEIF c = CS_SCRIPT THEN
        GOSUB net_script_frame
    ELSEIF c = CS_ROOM THEN
        GOSUB net_room
    ELSEIF c = CS_ERROR THEN
        net_err = rb(NBUF + 1)
        net_r = NET_ERROR
    END IF
END

' TERRAIN: base tv, tv, flags, x16, column entries (fld.asm's TERRAIN draws
' them). One whose base isn't ours: all of it again.
net_terrain_frame: PROCEDURE
    IF in_script THEN                   ' the server skipped us to the shot's end
        in_script = 0 : net_skipped = 1
        net_tv = $FF
    END IF
    c = rb(NBUF + 1)
    IF c <> $FF AND c <> net_tv THEN
        net_tv = $FF
        GOSUB net_resync
        RETURN
    END IF
    IF c = $FF THEN net_tv = $FE : GOSUB field_loading
    CALL TERRAIN(n)
    IF rb(NBUF + 3) AND G_LAST THEN net_tv = rb(NBUF + 2)
    net_terrain = 1
END

' SCRIPT: sid, seg, flags, off16, bytes, into SCRIPT. NET_SCRIPT when the
' segment is whole.
net_script_frame: PROCEDURE
    #v = rb(NBUF + 4) + rb(NBUF + 5) * 256
    IF #v = 0 THEN
        #seg_len = 0
        seg_sid = rb(NBUF + 1)
        seg_no = rb(NBUF + 2)
        seg_ready = 0
    END IF
    #w = n - 6
    IF rb(NBUF + 1) <> seg_sid OR rb(NBUF + 2) <> seg_no OR #v <> #seg_len OR #seg_len + #w > SCRIPT_BUF THEN
        GOSUB net_resync                ' a frame of it missing
        RETURN
    END IF
    CALL COPYB(NBUF + 6, SCRIPT + #seg_len, #w)
    #seg_len = #seg_len + #w
    IF rb(NBUF + 3) AND S_LAST_FRAME THEN
        seg_ready = 1
        seg_flags = rb(NBUF + 3)
        net_r = NET_SCRIPT
    END IF
END

' A room's record (CS_ROOM: index, count, record) into ROOMS.
net_room: PROCEDURE
    IF n >= 3 + ROOM_REC THEN
        c = rb(NBUF + 1)
        IF c < MAX_ROOMS THEN
            CALL COPYB(NBUF + 3, ROOMS + c * ROOM_REC, ROOM_REC)
            room_have = room_have OR tbit(c)
        END IF
    END IF
    IF n >= 3 THEN
        room_n = rb(NBUF + 2)
        IF room_n > MAX_ROOMS THEN room_n = MAX_ROOMS
    END IF
END

' net_rooms: the room list (waited for, asked again until it comes):
' room_n rooms, 0 if none or no answer.
net_rooms: PROCEDURE
    GOSUB net_open
    IF link = 0 THEN room_n = 0 : RETURN
    room_n = NONE
    room_have = 0
    dec_budget = DECODE_MOST
    FOR #a = 0 TO 599
        IF room_n = NONE AND (#a % ASK_AGAIN) = 0 THEN
            ns_t = SC_ROOMS : ns_n = 0 : GOSUB net_send
        END IF
        WAIT
        GOSUB net_poll
        IF net_r = NET_LOST THEN room_n = 0 : RETURN
        IF room_n <> NONE THEN
            v = tbit(room_n) - 1
            IF (room_have AND v) = v THEN RETURN
        END IF
    NEXT #a
    room_n = 0
END

' net_act: act na_act with the na_n bytes at RAWB + 3 (the caller puts
' them there); seq the action's.
    DIM na_act, na_n
net_act: PROCEDURE
    seq = seq + 1
    IF seq = 0 THEN seq = 1
    POKE RAWB + 1, seq
    POKE RAWB + 2, na_act
    ns_t = SC_ACT : ns_n = 2 + na_n : GOSUB net_send
END

net_leave: PROCEDURE
    IF joined THEN na_act = ACT_LEAVE : na_n = 0 : GOSUB net_act
    joined = 0
END

' net_close: goodbye, sent before the link is closed.
net_close: PROCEDURE
    IF link THEN
        IF joined THEN
            ns_t = SC_BYE : ns_n = 0 : GOSUB net_send
            #a = FRAME
            WHILE txq_len AND FRAME - #a < 30
                WAIT
                GOSUB nst_step
            WEND
        END IF
        GOSUB net_drop
    END IF
END

tbit:
    DATA 1, 2, 4, 8, 16, 32, 64, 128, 256
