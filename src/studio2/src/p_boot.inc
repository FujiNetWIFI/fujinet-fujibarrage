; p_boot.inc -- the lobby's appkeys (src/appkey.c), the socket, the title,
; the name and the room list (src/menus.c). FujiBlast's src/studio2
; p_boot.inc and p_menu.inc, for FujiBarrage.
;
; Creator 1, app 1: key 0 our name, key 0x13 FujiBarrage's server
; "tcp://host:port/?table=id" (the lobby's, or ours: kept when we join,
; emptied when the room refuses us). The socket's spec "N:TCP://host:port/"
; lives in the cart's RAM (CR_URL: the key's, else the build's DEFSPEC);
; SRVTBL is the room (empty: the room list).
;
; These calls wait (fujilib's fn_go): they run between screens, never while
; the link steps.

; Bank BK_BOOT: entered only through these (equ.inc E_*).
        FJMP    ak_load                 ; E_AKLOAD
        FJMP    title                   ; E_TITLE
        FJMP    ak_save_name            ; E_SAVENM
        FJMP    ak_save_room            ; E_SAVERM
        FJMP    ak_clear_room           ; E_CLRRM
        FJMP    sock_open               ; E_SOPEN
        FJMP    sock_close              ; E_SCLOSE
        FJMP    rooms                   ; E_ROOMS
        FJMP    msg_show                ; E_MSG
        FJMP    msg_wait                ; E_MSGW

AKREAD  EQU     0
AKWRITE EQU     1

; ak_open: OPEN_APPKEY key R9.0, mode D (the payload is all six bytes).
; D = 0 if it worked.
        RFIT    51
ak_open:
        PLO     RC
        GLO     R9
        PHI     RC
        LDI     FNDEVF
        PHI     R9
        LDI     FCOKEY
        PLO     R9
        LDI     0
        FCALL   fn_beg
        LDI     AK_CREATOR              ; creator (LE)
        FCALL   fn_txb
        LDI     0
        FCALL   fn_txb
        LDI     AK_APP
        FCALL   fn_txb
        GHI     RC                      ; the key
        FCALL   fn_txb
        GLO     RC                      ; the mode
        FCALL   fn_txb
        LDI     0
        FCALL   fn_txb
; ak_go: run it; D = 0 if it worked (and the server said yes).
ak_go:  FCALL   fn_go
        BNZ     akg9
        FJMP    fn_ack
akg9:   FRET

; ak_close: CLOSE_APPKEY.
        RFIT    14
ak_close:
        LDI     FNDEVF
        PHI     R9
        LDI     FCCKEY
        PLO     R9
        LDI     0
        FCALL   fn_beg
        FJMP    ak_go

; ak_read: READ_APPKEY key D: the reply window holds the u16 length, then
; the value (FN_REPLY+2). D = the length (0: none, or no appkeys; 64 at
; most). The key stays open: ak_close after reading the value.
        RFIT    47
ak_read:
        PLO     R9
        LDI     AKREAD
        FCALL   ak_open
        BNZ     akr0
        LDI     FNDEVF
        PHI     R9
        LDI     FCRKEY
        PLO     R9
        LDI     0
        FCALL   fn_beg
        FCALL   ak_go
        BNZ     akr0
        SLD     FN_REPLY+1              ; the length, at most 64
        BNZ     akr1
        DEC     R8
        LDN     R8
        SMI     65
        BDF     akr1
        LDN     R8
        FRET
akr1:   LDI     64
        FRET
akr0:   LDI     0
        FRET

; ak_write: WRITE_APPKEY key R9.0 = the RC.0 bytes at R8 (any page).
        RFIT    70
ak_write:
        GLO     RC
        PUTV    T7
        GHI     R8
        PUTV    T5
        GLO     R8
        PUTV    T6
        LDI     AKWRITE
        FCALL   ak_open
        BNZ     akw9
        LDI     FNDEVF
        PHI     R9
        LDI     FCWKEY
        PLO     R9
        LDI     0
        FCALL   fn_beg
        GETV    T5
        PHI     R8
        GETV    T6
        PLO     R8
        GETV    T7
        PLO     RC
akw1:   GLO     RC
        BZ      akw2
        DEC     RC
        LDA     R8
        FCALL   fn_txb
        BR      akw1
akw2:   FCALL   ak_go
akw9:   FJMP    ak_close

; ---------------------------------------------------------------------------
; ak_load -- our name from key 0 (letters, upper-cased, and digits, 8 at
;   most); the server and room from key 0x13, else the build's server and
;   no room.
; ---------------------------------------------------------------------------
        RFIT    100
ak_load:
        LDR     R8, MYNAME              ; no name, no room yet
        LDI     NAME_LEN+1
        PLO     R9
akl0:   LDI     0
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     akl0
        LDI     0
        PUT9    SRVTBL
        LDI     AK_NAME
        FCALL   ak_read
        BZ      akl3
        PLO     RC                      ; the value's bytes left
        LDR     R8, FN_REPLY+2
        LDR     R9, MYNAME
akl1:   GLO     RC
        BZ      akl3
        DEC     RC
        LDA     R8
        BZ      akl3
        FCALL   alnum                   ; a letter (upper) or a digit, else 0
        BZ      akl1
        SMI     'a'                     ; lower: upper
        BNF     akl2
        SMI     32
akl2:   ADI     'a'
        STR     R9
        INC     R9
        GLO     R9
        XRI     LO(MYNAME+NAME_LEN)
        BNZ     akl1
akl3:   FCALL   ak_close
        FCALL   spec_default
        LDI     AK_ROOM
        FCALL   ak_read
        SMI     6
        BNF     akl9                    ; too short to be a URL
        FCALL   ak_room
akl9:   FJMP    ak_close

; alnum: D if a letter or a digit (either case), else 0. Keeps R8, R9.
        RFIT    25
alnum:  PHI     RC
        SMI     '0'
        BNF     aln9
        SMI     10
        BNF     aln8
        GHI     RC
        ANI     0DFH
        SMI     'A'
        BNF     aln9
        SMI     26
        BDF     aln9
aln8:   GHI     RC
        FRET
aln9:   LDI     0
        FRET

; spec_default: CR_URL = the build's DEFSPEC.
        RFIT    55
spec_default:
        POKEAT  CR_URL
        LDR     R8, DEFSPEC
spd1:   LDA     R8
        PKD
        DEC     R8
        LDA     R8
        BNZ     spd1
        PKEND
        FRET

; ak_room: key 0x13's value (in the reply window): "...://host[:port]/..."
; becomes CR_URL "N:TCP://host:port/" (":9913" if it has none); "table=id"
; SRVTBL (letters and digits; 8 at most). No "://": the default stays.
        RFIT    218
ak_room:
        LDR     R8, FN_REPLY+2          ; "://"
akm1:   LDA     R8
        BZ      akm9j
        XRI     ':'
        BNZ     akm1
        LDA     R8
        XRI     '/'
        BNZ     akm1
        LDA     R8
        XRI     '/'
        BNZ     akm1
        LDN     R8                      ; no host: the default
        BZ      akm9j
        XRI     '/'
        BZ      akm9j
        SETV    T7, 0                   ; a port seen
        POKEAT  CR_URL                  ; "N:TCP://", the host and port, "/"
        LDR     R9, u_ntcp
akm2:   LDA     R9
        BZ      akm3
        PKD
        BR      akm2
akm9j:  BR      akm9
akm3:   LDA     R8
        BZ      akm4
        XRI     '/'
        BZ      akm4
        XRI     '/'!'?'
        BZ      akm4
        XRI     '?'!':'
        BNZ     akm3a
        SETV    T7, 1
        LDI     0
akm3a:  XRI     ':'
        PKD
        BR      akm3
akm4:   GETV    T7                      ; no port: ours
        BNZ     akm4b
        LDR     R9, u_port
akm4a:  LDA     R9
        BZ      akm4b
        PKD
        BR      akm4a
akm4b:  LDI     '/'
        PKD
        LDI     0
        PKD
        PKEND
        DEC     R8                      ; "table=" after it
akm5:   LDA     R8
        BZ      akm9
        XRI     't'
        BNZ     akm5
        LDR     R9, u_table
akm6:   LDA     R9
        BZ      akm7
        STR     R2
        LDA     R8
        XOR
        BNZ     akm5
        BR      akm6
akm7:   LDR     R9, SRVTBL              ; the id
        LDI     ROOM_LEN
        PLO     RB
akm8:   LDA     R8
        FCALL   alnum
        BZ      akm8a
        STR     R9
        INC     R9
        DEC     RB
        GLO     RB
        BNZ     akm8
akm8a:  LDI     0
        STR     R9
akm9:   FRET
u_ntcp: DB      "N:TCP://", 0
u_table: DB     "able=", 0
u_port: DB      ":9913", 0

; ak_save_name: key 0 = MYNAME.
        RFIT    30
ak_save_name:
        LDR     R8, MYNAME
        LDI     0
        PLO     RC
asn1:   LDA     R8
        BZ      asn2
        INC     RC
        GLO     RC
        XRI     NAME_LEN
        BNZ     asn1
asn2:   LDR     R8, MYNAME
        LDI     AK_NAME
        PLO     R9
        FJMP    ak_write

; ak_save_room: key 0x13 = "tcp://" host:port "/?table=" SRVTBL, built in
; RAWBUF (idle between rooms: 40 bytes is enough for the lobby's).
        RFIT    83
ak_save_room:
        LDR     R9, RAWBUF
        LDR     R8, u_tcp
        FCALL   strcat
        LDR     R8, CR_URL+8            ; after "N:TCP://": to the '/'
asr1:   LDA     R8
        BZ      asr2
        XRI     '/'
        BZ      asr2
        XRI     '/'
        STR     R9
        INC     R9
        BR      asr1
asr2:   LDR     R8, u_room
        FCALL   strcat
        LDR     R8, SRVTBL
        FCALL   strcat
        GLO     R9                      ; its length
        SMI     LO(RAWBUF)
        PLO     RC
        LDR     R8, RAWBUF
        LDI     AK_ROOM
        PLO     R9
        FJMP    ak_write
u_tcp:  DB      "tcp://", 0
u_room: DB      "/?table=", 0

; strcat: the string at R8 copied to R9 (R9 left on its NUL).
        RFIT    8
strcat: LDA     R8
        STR     R9
        BZ      sct9
        INC     R9
        BR      strcat
sct9:   FRET

; ak_clear_room: key 0x13 written empty; no room.
        RFIT    24
ak_clear_room:
        LDI     0
        PUT9    SRVTBL
        LDI     0
        PLO     RC
        LDI     AK_ROOM
        PLO     R9
        FJMP    ak_write

; ---------------------------------------------------------------------------
; sock_open -- the socket to CR_URL (read and write, no translation),
;   closed first (a RESET leaves the cart's open): NLINK = NL_LINK if it
;   opened (D = 0), else 0 (D != 0).
; sock_close -- closed (BYE sent by the caller first).
; ---------------------------------------------------------------------------
        RFIT    84
sock_open:
        FCALL   sock_close
        LDI     NETDEV
        PHI     R9
        LDI     NCOPEN
        PLO     R9
        LDI     2
        FCALL   fn_beg
        LDI     NMRW                    ; 12: read and write
        FCALL   fn_pb
        LDI     0                       ; no translation
        FCALL   fn_pb
        LDR     R8, CR_URL
so1:    LDA     R8
        BZ      so2
        FCALL   fn_txb
        BR      so1
so2:    SETV    V_TMO, 30               ; (fn_go: 30 s)
        FCALL   fn_go
        BNZ     so9
        FCALL   fn_ack
        BNZ     so9
        SETV    NLINK, NL_LINK
        SETV    TAKEN, 0                ; a new socket: nothing taken yet
        SETV    CREDITED, 0
        LDI     0
        FRET
so9:    SETV    NLINK, 0
        LDI     1
        FRET

        RFIT    21
sock_close:
        SETV    NLINK, 0
        LDI     NETDEV
        PHI     R9
        LDI     NCCLOSE
        PLO     R9
        LDI     0
        FCALL   fn_beg
        FCALL   fn_go
        FRET

; ---------------------------------------------------------------------------
; title -- FUJIBARRAGE (M2: a field scene), then 5. A name if there is none
;   (D = 1: a new one, the caller saves it).
; ---------------------------------------------------------------------------
        RFIT    15
title:  GETV    MYNAME                  ; (the scene: p_room.inc title_scene)
        BZ      ti2
        LDI     0
        FRET
ti2:    FCALL   name_entry
        LDI     1
        FRET

; ---------------------------------------------------------------------------
; name_entry -- the family's name wheel: 2 and 8 turn the letter at the
;   cursor through A-Z and 0-9, 5 or 6 takes it, 4 rubs one out, 9 ends
;   (two letters at least). MYNAME, NUL-padded. T0 the cursor, T1 the
;   letter (PICKSET's).
; ---------------------------------------------------------------------------
        RFIT    90
name_entry:
        OPN     TOP_CLS
        SETV    TXM, TM_SET
        LDR     R8, s_yourname
        LDI     TXROW(2)
        PHI     R9
        FCALL   txt_ctr
        LDR     R8, s_namehelp
        SETV    TXM, TM_SET+TM_SHORT
        LDI     TXROW(7)
        PHI     R9
        FCALL   txt_ctr
        LDR     R8, s_namehelp2
        LDI     TXROW(7)+8
        PHI     R9
        FCALL   txt_ctr
        LDR     R8, MYNAME              ; empty
        LDI     NAME_LEN+1
        PLO     R9
ne0:    LDI     0
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     ne0
        SETV    T0, 0                   ; the cursor
        SETV    T1, 0                   ; the letter
        FJMP    ne1

        RFIT    133
ne1:    FCALL   ne_show
ne2:    FCALL   frame
        FCALL   in_poll
        FCALL   key_take
        PHI     RD
        XRI     2                       ; 2: the letter before
        BNZ     ne3
        GETV    T1
        BNZ     ne2a
        LDI     36
ne2a:   SMI     1
        STR     RA
        BR      ne1
ne3:    GHI     RD                      ; 8: the next
        XRI     8
        BNZ     ne4
        GETV    T1
        ADI     1
        STR     RA
        XRI     36
        BNZ     ne1
        STR     RA
        BR      ne1
ne4:    GHI     RD                      ; 5 or 6: taken
        XRI     5
        BZ      ne4a
        GHI     RD
        XRI     6
        BNZ     ne5
ne4a:   GETV    T0
        XRI     NAME_LEN
        BZ      ne2                     ; (full)
        GETV    T1
        ADI     LO(PICKSET)
        PLO     R8
        LDI     HI(PICKSET)
        PHI     R8
        LDN     R8
        PHI     RD
        GETV    T0
        ADI     LO(MYNAME)
        PLO     RA
        GHI     RD
        STR     RA
        INCV    T0
        BR      ne1
ne5:    GHI     RD                      ; 4: one rubbed out
        XRI     4
        BNZ     ne6
        GETV    T0
        BZ      ne2
        SMI     1
        STR     RA
        ADI     LO(MYNAME)
        PLO     RA
        LDI     0
        STR     RA
        BR      ne1
ne6:    GHI     RD                      ; 9: done, two letters at least
        XRI     9
        BNZ     ne2
        GETV    T0
        SMI     2
        BNF     ne2
        FRET

; ne_show: the name, then the letter at the cursor (if there is room), the
; cursor's cell inverted: row 4, x 16 (eight cells).
        RFIT    171
ne_show:
        LDR     R9, RAWBUF
        LDR     R8, MYNAME
        LDI     NAME_LEN
        PLO     RC
nes1:   LDA     R8
        BZ      nes2
        STR     R9
        INC     R9
        DEC     RC
        GLO     RC
        BNZ     nes1
        BR      nes3
nes2:   GETV    T1                      ; the letter offered
        ADI     LO(PICKSET)
        PLO     R8
        LDI     HI(PICKSET)
        PHI     R8
        LDN     R8
        STR     R9
        INC     R9
        DEC     RC
nes2a:  GLO     RC                      ; spaces to the end
        BZ      nes3
        LDI     ' '
        STR     R9
        INC     R9
        DEC     RC
        BR      nes2a
nes3:   LDI     0
        STR     R9
        SETV    TXM, TM_SET
        LDR     R8, RAWBUF
        LDI     16
        PLO     R9
        LDI     TXROW(4)
        PHI     R9
        FCALL   txt_at16
        GETV    T0                      ; the cursor's cell
        XRI     NAME_LEN
        BZ      nes9
        PARS
        GETV    T0
        SHL
        SHL
        ADI     16
        PD
        P       TXROW(4)
        P       4
        P       12
        P       TM_XOR
        GOP     TOP_RECT
nes9:   FRET
s_yourname: DB  "YOUR NAME", 0
s_namehelp: DB  "2 8 LETTER 5 ADD", 0
s_namehelp2: DB "4 BACK 9 DONE", 0
PICKSET: DB     "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

; ---------------------------------------------------------------------------
; rooms -- the room list: T asked for (again every 60 jiffies until the
;   count comes), drawn once every record is in (600 jiffies at most); 2
;   and 8 choose, 5 joins: SRVTBL = its id. D = 1 picked, 0 none (no
;   answer, no rooms, the link lost). T0 the choice, T1-T2 jiffies
;   waited, T3 to the next ask, T4 the list drawn.
; ---------------------------------------------------------------------------
        RFIT    175
rooms:  OPN     TOP_CLS
        SETV    TXM, TM_SET
        LDR     R8, s_pick
        LDI     TXROW(0)
        PHI     R9
        FCALL   txt_ctr
        SETV    T0, 0
        SETV    T1, 0
        SETV    T2, 0
        SETV    T3, 0
        SETV    T4, 0
        SETV    TBLHAVE, 0
        SETV    TBLN, NONE
rm1:    FCALL   frame
        FCALL   in_poll
        FCALL   nstep
        GETV    NEVT
        ANI     EV_LOST
        BZ      rm2
rm0:    LDI     0
        FRET
rm2:    GETV    TBLN                    ; no count yet: ask, every 60
        XRI     NONE
        BNZ     rm4
        GETV    DT
        STR     R2
        GETV    T1                      ; (600 jiffies: give up)
        ADD
        STR     RA
        INC     RA
        LDN     RA
        ADCI    0
        STR     RA
        GETV    T1
        SMI     LO(600)
        GETV    T2
        SMBI    HI(600)
        BDF     rm0
        GETV    T3
        BNZ     rm3
        SETV    T3, 60
        GETV    TXF
        ORI     TX_ROOMS
        STR     RA
        BR      rm1
rm3:    GETV    DT
        STR     R2
        GETV    T3
        SM
        BDF     rm3a
        LDI     0
rm3a:   STR     RA
        BR      rm1
rm4:    GETV    TBLN                    ; none: nothing to pick
        BZ      rm0
        FJMP    rm5

        RFIT    151
rm5:    GETV    TBLN                    ; every record in?
        ADI     LO(TMASK-1)
        PLO     R8
        LDI     HI(TMASK)
        PHI     R8
        LDN     R8
        STR     R2
        GETV    TBLHAVE
        XOR
        BNZ     rm1j
        GETV    T4                      ; drawn once
        BNZ     rm6
        SETV    T4, 1
        FCALL   rm_draw
rm6:    FCALL   key_take
        PHI     RD
        XRI     2
        BNZ     rm7
        GETV    T0
        BZ      rm1j
        SMI     1
        BR      rm8
rm7:    GHI     RD
        XRI     8
        BNZ     rm9
        GETV    T0
        ADI     1
        STR     R2
        GETV    TBLN
        SM
        BZ      rm1j
        LDN     R2
rm8:    PUTV    T0
        FCALL   rm_draw
rm1j:   FJMP    rm1
rm9:    GHI     RD                      ; 5: that one
        XRI     5
        BNZ     rm1j
        GETV    T0                      ; SRVTBL = its id (TBLS + 24 i)
        SHL
        SHL
        SHL
        PLO     R9
        SHL
        STR     R2
        GLO     R9
        ADD
        ADI     LO(TBLS)
        PLO     R8
        LDI     HI(TBLS)
        ADCI    0
        PHI     R8
        LDR     R9, SRVTBL
        LDI     ROOM_LEN
        PLO     RC
rm10:   LDA     R8
        STR     R9
        INC     R9
        DEC     RC
        GLO     RC
        BNZ     rm10
        LDI     0
        STR     R9
        LDI     1
        FRET
s_pick: DB      "PICK A ROOM", 0
TMASK:  DB      1, 3, 7, 0FH, 1FH, 3FH, 7FH, 0FFH

; rm_draw: each room a row from 2: the cursor, its name (12), '*' if it
;   plays, its people.
        RFIT    174
rm_draw:
        SETV    T5, 0                   ; the room
rd1:    LDR     R9, RAWBUF
        GETV    T0
        STR     R2
        GETV    T5
        XOR
        BNZ     rd2
        LDI     '>'
        BR      rd3
rd2:    LDI     ' '
rd3:    STR     R9
        INC     R9
        GETV    T5                      ; R8 = its record + 8: the name
        SHL
        SHL
        SHL
        PLO     R8
        SHL
        STR     R2
        GLO     R8
        ADD
        ADI     LO(TBLS+8)
        PLO     R8
        LDI     HI(TBLS)
        ADCI    0
        PHI     R8
        LDI     12
        PLO     RC
rd4:    LDA     R8
        BNZ     rd5
        DEC     R8
        LDI     ' '
rd5:    STR     R9
        INC     R9
        DEC     RC
        GLO     RC
        BNZ     rd4
        GETV    T5                      ; + 22: playing
        SHL
        SHL
        SHL
        PLO     R8
        SHL
        STR     R2
        GLO     R8
        ADD
        ADI     LO(TBLS+20)
        PLO     R8
        LDI     HI(TBLS)
        ADCI    0
        PHI     R8
        INC     R8
        INC     R8
        LDN     R8
        BZ      rd6
        LDI     '*'
        BR      rd7
rd6:    LDI     ' '
rd7:    STR     R9
        INC     R9
        DEC     R8                      ; + 20: the people, two places
        DEC     R8
        LDN     R8
        PLO     RC
        SMI     10
        BNF     rd8
        PLO     RC
        LDI     '1'
        BR      rd9
rd8:    LDI     ' '
rd9:    STR     R9
        INC     R9
        GLO     RC
        ADI     '0'
        STR     R9
        INC     R9
        LDI     0
        STR     R9
        SETV    TXM, TM_SET
        LDR     R8, RAWBUF
        GETV    T5
        ADI     2
        PLO     R9                      ; (row = T5 + 2: R9.1 = 12 row)
        SHL
        PLO     RC
        SHL
        STR     R2
        GLO     RC
        ADD
        SHL
        PHI     R9
        LDI     0
        PLO     R9
        FCALL   txt_at16
        INCV    T5
        STR     R2
        GETV    TBLN
        XOR
        BNZ     rd1
        FRET

; ---------------------------------------------------------------------------
; msg_show -- message MSGID (b6-0) on a cleared screen: its lines (MSGTAB),
;   the room's id under M_JOIN's.
; msg_wait -- the message shown, then its time (MSGT: 0 until 5, 1 none).
; ---------------------------------------------------------------------------

        RFIT    91
msg_show:
        OPN     TOP_CLS
        SETV    TXM, TM_SET
        GETV    MSGID
        ANI     7FH
        SHL
        SHL
        ADI     LO(MSGTAB)
        PLO     RC
        LDI     HI(MSGTAB)
        PHI     RC
        LDA     RC                      ; the first line
        PHI     R8
        LDA     RC
        PLO     R8
        LDA     RC                      ; (the second: T0, T1)
        PUTV    T0
        LDN     RC
        PUTV    T1
        LDI     TXROW(3)
        PHI     R9
        FCALL   txt_ctr
        GETV    MSGID
        ANI     7FH
        XRI     M_JOIN
        BNZ     msg1
        LDR     R8, SRVTBL              ; JOINING: the room's id
        BR      msg2
msg1:   GETV    T0
        BZ      msg3
        PHI     R8
        GETV    T1
        PLO     R8
msg2:   LDI     TXROW(5)
        PHI     R9
        FCALL   txt_ctr
msg3:   FRET

        RFIT    93
msg_wait:
        FCALL   msg_show
        GETV    MSGID
        ANI     7FH
        ADI     LO(MSGT)
        PLO     R8
        LDI     HI(MSGT)
        PHI     R8
        LDN     R8
        PUTV    MRC
        XRI     1
        BZ      mw9
        GETV    MRC
        BNZ     mw2
        LDR     R8, s_press5            ; until 5
        LDI     TXROW(8)
        PHI     R9
        FCALL   txt_ctr
        SETV    KEV, NONE
mw1:    FCALL   frame
        FCALL   in_poll
        GETV    KEV
        XRI     5
        BNZ     mw1
        SETV    KEV, NONE
        FRET
mw2:    FCALL   frame                   ; MRC jiffies
        FCALL   in_poll
        GETV    DT
        STR     R2
        GETV    MRC
        SM
        STR     RA
        BZ      mw9
        BDF     mw2
mw9:    FRET

        PFIT    48
MSGTAB: DW      s_join, 0, s_noans, s_from, s_lost, s_again, s_said, s_noroom
        DW      s_said, s_update, s_conn, 0, 0, 0, s_said, s_full
        DW      s_said, s_caps
MSGT:   DB      1, 0, 120, 0, 0, 1, 1, 0, 0
s_join: DB      "JOINING", 0
s_noans: DB     "NO ANSWER", 0
s_from: DB      "FROM THE SERVER", 0
s_lost: DB      "CONNECTION LOST", 0
s_again: DB     "TRYING AGAIN", 0
s_said: DB      "SERVER SAID NO", 0
s_noroom: DB    "NO SUCH ROOM", 0
s_update: DB    "UPDATE THE GAME", 0
s_full: DB      "THE ROOM IS FULL", 0
s_caps: DB      "WRONG MACHINE", 0
s_conn: DB      "CONNECTING", 0
s_press5: DB    "PRESS 5", 0
