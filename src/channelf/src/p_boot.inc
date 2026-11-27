; ---------------------------------------------------------------------------
; p_boot.inc -- page BOOT: the rigs' test hook (src/test.c test_check), the
; lobby's appkeys (src/appkey.c), the socket (net_open, net_close), the
; messages; the title, the name and the room list (src/menus.c). FujiBlast's
; src/channelf p_boot.inc and p_menu.inc, for FujiBarrage. Each transaction
; here is waited for (FNGO); the link's own, if one is out, is let finish
; first (MIDLE).
;
; Key 0 is the player's name, key 0x13 FujiBarrage's server
; ("tcp://host:port/?table=id"). We keep only the room's id (SRVTBL) and
; FL_ROOM (the key holds our server); the OPEN reads the key again and
; stores "N:TCP://HOST:PORT/" from its reply straight into the transaction.
; Without the key, the server is the build's (DEFSPEC), and the room one
; picked from its list (E_JOIN then writes the key).
;
; Entries (barrage.asm, main.inc): E_ENTER the test hook, then the keys (r0
; = the scene, 0: none); E_STATE the room list asked for; E_TICK its pass
; (r0: 0 on, 1 picked, 2 no list); E_MENU the title, and the name if none
; (r0 = 1: a new one); E_SAVENM; E_CONN and E_JOIN (r0 = 1: the socket is
; open); E_CLOSE; E_CLRRM; E_MSG (message MSGID); E_FAIL (the socket
; closed, message MSGID, b7 the room forgotten).
; ---------------------------------------------------------------------------

AKNAME  EQU 0
AKROOM  EQU 13H

; the build's server (gen/server, or DEFSRV): DEFSPEC, DEFURL
        INCLUDE "config.inc"

; ---- test_check, then the keys ---------------------------------------------------
; A rig pokes TESTHK[0] = $5A (the scene in [2], its argument in [3]) within
; 20 frames of seeing [0], [1] = $3C, $C3.
BENTER: ENTER
        DCI TESTHK
        LI 03CH
        ST
        LI 0C3H
        ST
        LI 20
        STB RCOUNT
BEN1:   PI FRWAIT
        LDB TESTHK
        CI 05AH
        BZ BEN2
        DECB RCOUNT
        BNZ BEN1
        CLR
        STB TESTHK
        STB TESTHK+1
        PI AKLOAD
        CLR
        LR 0,A
        LEAVE
BEN2:   LI 0A5H
        STB TESTHK
        LDB TESTHK+2
        LR 0,A
        LEAVE

; ---- the appkeys -----------------------------------------------------------------
; Results come back in A (0: it worked): LEAVE keeps A but not the flags,
; so a caller tests A again (NI 0FFH).
;
; AKOPEN -- key r3 opened, mode r4 (0 read, 1 write): OPEN_APPKEY, no
;   parameters, a 6-byte payload (creator lo, hi, app, key, mode, 0).
;   A = 0 if it opened.
AKOPEN: ENTER
        PI MIDLE
        LI FD_FUJI
        LR 0,A
        LI FC_AKOPN
        LR 1,A
        CLR
        LR 2,A
        PI FNBEG
        LIS 1
        ST                      ; creator 1, LE
        CLR
        ST
        LIS 1
        ST                      ; app 1
        LR A,3
        ST                      ; the key
        LR A,4
        ST                      ; the mode
        CLR
        ST                      ; reserved: it must be sent
        PI FNGO
        LEAVE

; AKCLOSE -- the key closed.
AKCLOSE: ENTER
        LI FD_FUJI
        LR 0,A
        LI FC_AKCLS
        LR 1,A
        CLR
        LR 2,A
        PI FNBEG
        PI FNGO
        LEAVE

; AKREAD -- key r3 opened and read: the reply is its length (LE), then the
;   data (kept in the window until the next transaction: the caller closes
;   the key after).  r5 = the length, at most 64 (0, Z: none or it failed:
;   closed); A = r5.
AKREAD: ENTER
        CLR
        LR 4,A
        PI AKOPEN
        NI 0FFH
        BNZ AKR8
        LI FD_FUJI
        LR 0,A
        LI FC_AKRD
        LR 1,A
        CLR
        LR 2,A
        PI FNBEG
        PI FNGO
        NI 0FFH
        BNZ AKR8
        LDB FNREPLY+1           ; the length's high byte: 0, or it isn't a key
        NI 0FFH
        BNZ AKR8
        LDB FNREPLY
        CI 64
        BC AKR9
        LI 64
AKR9:   LR 5,A                  ; (A = the length: LEAVE keeps A, not the flags)
        LEAVE
AKR8:   PI AKCLOSE
        CLR
        LR 5,A
        LEAVE

; AKWRITE -- key r3 opened to write, the WRITE begun (DC0 on the TX: the
;   caller stores the data, then AKGO).  A = 0 if it opened.
AKWRITE: ENTER
        LIS 1
        LR 4,A
        PI AKOPEN
        NI 0FFH
        BNZ AKW9
        LI FD_FUJI
        LR 0,A
        LI FC_AKWR
        LR 1,A
        CLR
        LR 2,A
        PI FNBEG
        CLR                     ; A = 0: open
AKW9:   LEAVE

; AKGO -- the WRITE sent, the key closed.
AKGO:   ENTER
        PI FNGO
        PI AKCLOSE
        LEAVE

; AKLOAD -- MYNAME = key 0, letters and digits up to 8, upper-cased (NUL
;   padded); SRVTBL and FL_ROOM from key 0x13 (appkey_load).
AKLOAD: ENTER
        DCI MYNAME              ; both cleared
        LI NAME_LEN+1
        LR 0,A
AKL0:   CLR
        ST
        DS 0
        BNZ AKL0
        DCI SRVTBL
        LI TABLE_LEN+1
        LR 0,A
AKL0A:  CLR
        ST
        DS 0
        BNZ AKL0A
        LDB FLAGS
        NI 0FFH-FL_ROOM
        STB FLAGS
        LIS AKNAME              ; the name
        LR 3,A
        PI AKREAD
        NI 0FFH
        BZ AKL3
        DCI FNREPLY+2
        XDC
        DCI MYNAME
        LI NAME_LEN
        LR 6,A                  ; room left
AKL1:   XDC                     ; r5 the key's bytes left
        LM
        XDC
        LR 4,A
        PI UPALNUM
        BZ AKL2
        ST
        DS 6
        BZ AKL2A
AKL2:   DS 5
        BNZ AKL1
AKL2A:  PI AKCLOSE
AKL3:   LI AKROOM           ; the server: its table, if it has one
        LR 3,A
        PI AKREAD
        LR A,5
        CI 4
        BNC AKL3A
        LEAVE                   ; (4 bytes or fewer: none; closed)
AKL3A:  DCI FNREPLY+2
AKL4:   LM                      ; to the "="
        CI '='
        BZ AKL5
        DS 5
        BNZ AKL4
        BR AKL8
AKL5:   XDC
        DCI SRVTBL
        LI TABLE_LEN
        LR 6,A
AKL6:   DS 5
        BZ AKL7
        XDC
        LM
        XDC
        LR 4,A
        PI UPALNUM
        BZ AKL7
        CI 'A'-1                ; a letter: lower case
        BC AKL6A
        AI 20H
AKL6A:  ST
        DS 6
        BNZ AKL6
AKL7:   LDB SRVTBL
        NI 0FFH
        BZ AKL8
        LDB FLAGS
        OI FL_ROOM
        STB FLAGS
AKL8:   PI AKCLOSE
        LEAVE

; UPALNUM -- A = r4 upper-cased if a letter; 0 (Z) if not a letter or a
;   digit.  Leaf.
UPALNUM: LR A,4
        CI 'a'-1
        BC UPA1
        CI 'z'
        BNC UPA9
        AI -20H
        NI 0FFH
        POP
UPA1:   CI 'A'-1
        BC UPA2
        CI 'Z'
        BNC UPA9
        NI 0FFH
        POP
UPA2:   CI '0'-1
        BC UPA9
        CI '9'
        BNC UPA9
        NI 0FFH
        POP
UPA9:   CLR
        NI 0FFH
        POP

; SAVENM -- key 0 = MYNAME (appkey_save_name).
SAVENM: ENTER
        LIS AKNAME
        LR 3,A
        PI AKWRITE
        NI 0FFH
        BNZ SVN9
        XDC
        DCI MYNAME
        XDC
        LI NAME_LEN
        LR 6,A
SVN1:   XDC
        LM
        XDC
        NI 0FFH
        BZ SVN2
        ST
        DS 6
        BNZ SVN1
SVN2:   PI AKGO
SVN9:   LEAVE

; SAVEROOM -- key 0x13 = the build's server + SRVTBL, if the key isn't ours
;   already (appkey_save_room).
SAVEROOM: ENTER
        LDB FLAGS
        NI FL_ROOM
        BNZ SVR9
        LI AKROOM
        LR 3,A
        PI AKWRITE
        NI 0FFH
        BNZ SVR9
        DCI DEFURL              ; "tcp://host:port/?table="
        XDC
        PI FNTXS
        DCI SRVTBL
        XDC
        PI FNTXS
        PI AKGO
        LDB FLAGS
        OI FL_ROOM
        STB FLAGS
SVR9:   LEAVE

; CLRROOM -- key 0x13 emptied; no table (appkey_clear_room).
CLRROOM: ENTER
        LI AKROOM
        LR 3,A
        PI AKWRITE
        NI 0FFH
        BNZ CLR1
        PI AKGO
CLR1:   CLR
        STB SRVTBL
        LDB FLAGS
        NI 0FFH-FL_ROOM
        STB FLAGS
        LEAVE

; ---- the socket -------------------------------------------------------------------
; SCLOSE -- CLOSE (a NAK is fine: nothing was open), the link down.
SCLOSE: ENTER
        PI MIDLE
        LI FD_NET
        LR 0,A
        LI NC_CLOS
        LR 1,A
        CLR
        LR 2,A
        PI FNBEG
        PI FNGO
        CLR
        STB NLINK
        STB NKIND
        LEAVE

; SOPEN -- the socket: CLOSE first (a RESET leaves the cart's open), then
;   OPEN "N:TCP://host:port/" (read and write) -- from key 0x13 (FL_ROOM),
;   else DEFSPEC.  r0 = 1 open (NLINK), 0 not.
SOPEN:  ENTER
        PI SCLOSE
        LDB FLAGS
        NI FL_ROOM
        BNZ SOK
        PI SOBEGIN              ; the build's
        DCI DEFSPEC
        XDC
        PI FNTXS
        BR SOGO
SOK:    LI AKROOM           ; the key again: its reply holds the URL
        LR 3,A
        PI AKREAD
        NI 0FFH
        BZ SOFAIL
        LR A,5
        STB T2                  ; its length
        PI SOBEGIN
        PI SPECKEY
SOGO:   PI FNGO
        NI 0FFH
        BNZ SOFAIL1
        LDB FLAGS               ; (the key, if read, closed after)
        NI FL_ROOM
        BZ SOG1
        PI AKCLOSE
SOG1:   LIS NL_LINK
        STB NLINK
        LIS 1
        LR 0,A
        LEAVE
SOFAIL1: LDB FLAGS
        NI FL_ROOM
        BZ SOFAIL
        PI AKCLOSE
SOFAIL: CLR
        STB NLINK
        LR 0,A
        LEAVE

; SOBEGIN -- OPEN: read and write, no translation.  Leaf.  DC0 on the TX.
SOBEGIN: LI FD_NET
        LR 0,A
        LI NC_OPEN
        LR 1,A
        LIS 2
        LR 2,A
        DCI FNREG+FR_DEV        ; (FNBEG, inline: a leaf)
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        DCI FNREG+FR_DRST
        CLR
        ST
        DCI FNTX
        LIS 1
        ST
        LI NM_RW
        ST
        LIS 1
        ST
        CLR
        ST
        POP

; SPECKEY -- "N:" + the key's URL (in the reply from 2, T2 bytes) up to its
;   "?" (a "/" before it dropped), upper-cased; ":9913" if it has no port;
;   then "/".  Leaf.  Clobbers A, r0-r3, DCs; DC0 on the TX.
SPECKEY: DCI FNREPLY+2          ; r1: its characters, to T2 or the "?"
        LDB T2
        LR 0,A
        CLR
        LR 1,A
        DCI FNREPLY+2
SK1:    LR A,0
        NI 0FFH
        BZ SK2
        LM
        NI 0FFH
        BZ SK2
        CI '?'
        BZ SK2
        LR A,1
        INC
        LR 1,A
        DS 0
        BR SK1
SK2:    LR A,1                  ; a "/" last: dropped
        NI 0FFH
        BZ SK3
        DCI FNREPLY+1
        ADC
        LM
        CI '/'
        BNZ SK3
        DS 1
SK3:    DCI FNREPLY+2
        XDC
        DCI FNTX
        LI 'N'
        ST
        LI ':'
        ST
        CLR
        LR 2,A                  ; its colons
        LR A,1
        NI 0FFH
        BZ SK6
SK4:    XDC
        LM
        XDC
        CI ':'
        BNZ SK5
        LR 3,A
        LR A,2
        INC
        LR 2,A
        LR A,3
SK5:    CI 'a'-1                ; a..z up
        BC SK5A
        CI 'z'
        BNC SK5A
        AI -20H
SK5A:   ST
        DS 1
        BNZ SK4
SK6:    LR A,2                  ; "tcp://host": one colon, no port
        CI 1
        BNC SK8
        XDC
        DCI SPORT
        XDC
SK7:    XDC
        LM
        XDC
        NI 0FFH
        BZ SK8
        ST
        BR SK7
SK8:    LI '/'
        ST
        POP
SPORT:  DB ":9913",0

; CONN -- "CONNECTING", the socket (the build's server: the room list).
;   r0 = 1 open.
CONN:   ENTER
        LIS M_CONN
        STB MSGID
        PI MSG
        PI SOPEN
        LEAVE

; JOIN -- the room kept, "JOINING <room>", the socket.  r0 = 1 open.
JOIN:   ENTER
        PI SAVEROOM
        LIS M_JOIN
        STB MSGID
        PI MSG
        PI SOPEN
        LEAVE

; FAIL -- the socket closed, message MSGID (M_QUIET: none); its b7: the room
;   forgotten too.
FAIL:   ENTER
        PI SCLOSE
        LDB MSGID
        NI 080H
        STB FAILRM
        LDB MSGID
        NI 07FH
        STB MSGID
        CI M_QUIET
        BZ FAI1
        PI MSG
FAI1:   LDB FAILRM
        NI 0FFH
        BZ FAI9
        PI CLRROOM
FAI9:   LEAVE

; ---- the messages ------------------------------------------------------------------
; MSG -- message MSGID on a clear screen, white on black: its two lines (the
;   second the room's id for M_JOIN); then its time (1: none) or FIRE.
MSG:    ENTER
        CLR
        LR 0,A
        PI DCLS
        LDB MSGID               ; its first line
        SL 1
        DCI MSGA
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LIS 3
        LR 1,A
        LI CV1
        LR 2,A
        PI TXCENT
        LDB MSGID
        CI M_JOIN
        BZ MSG2
        SL 1                    ; its second
        DCI MSGB
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        BR MSG2A
MSG2:   DCI SRVTBL              ; the room: its id
MSG2A:  LIS 4
        LR 1,A
        LI CV1
        LR 2,A
        PI TXCENT
        LDB MSGID               ; its time
        DCI MSGT
        ADC
        LM
        STB RCOUNT
        CI 1
        BZ MSG9
        NI 0FFH
        BNZ MSG5
        DCI S_PFIRE             ; ... or FIRE
        LIS 7
        LR 1,A
        LI CV1
        LR 2,A
        PI TXCENT
MSG4:   PI FRWAIT
        PI INPOLL
        PI INTAKE
        LDB EVBTN
        NI 0FFH
        BNZ MSG9
        LDB EVKEY
        CI KEY_END
        BNZ MSG4
        BR MSG9
MSG5:   PI FRWAIT
        PI SNDTICK
        DECB RCOUNT
        BNZ MSG5
MSG9:   LEAVE

; the messages (M_*): each one's lines and time (0: FIRE, 1: none)
MSGA:   DW SJOIN, SNOANS, SLOST, SSAIDNO, SSAIDNO, SCONN, SNONE, SSAIDNO, SSAIDNO
MSGB:   DW SNONE, SFROM, SAGAIN, SNOROOM, SUPDATE, SNONE, SNONE, SFULL, SCAPS
MSGT:   DB 1, 0, 120, 0, 0, 1, 1, 0, 0
SJOIN:  DB "JOINING",0
SNOANS: DB "NO ANSWER",0
SFROM:  DB "FROM THE SERVER",0
SLOST:  DB "CONNECTION LOST",0
SAGAIN: DB "TRYING AGAIN",0
SSAIDNO: DB "SERVER SAID NO",0
SNOROOM: DB "NO SUCH ROOM",0
SUPDATE: DB "UPDATE FUJIBARRAGE",0
SFULL:  DB "THE ROOM IS FULL",0
SCAPS:  DB "NOT FOR THIS MACHINE",0
SCONN:  DB "CONNECTING",0
SNONE:  DB 0
S_PFIRE: DB "PRESS FIRE",0

;
; ---- the title (menus.c title) -----------------------------------------------------
; A field scene (checkpoint 1's: tools/chf/mktitle_chf.py): the hills drawn
; into the field as TERRAIN columns, the tanks, a shell's arc and its burst
; and FUJIBARRAGE in 2x red letters set over them, all shown at once
; (FSHOW); FOR FUJINET and PRESS FIRE on the HUD's rows. FIRE or START
; goes on, to the name if there is none.
MT0     EQU GT+16               ; the menus' scratch (GT: never across a call
MT1     EQU GT+17               ;  of theirs that uses GT: TXCLR spends GT+0,1)
MT3     EQU GT+18
MSEL    EQU GT+19
MNSEL   EQU GT+20
MPREV   EQU GT+21
MWAIT   EQU GT+22               ; 2
MBLINK  EQU GT+24
MNAME   EQU TV                  ; a string being built (TV: the test page's,
                                ;  never in a menu)
MENTER: ENTER
        CLR
        LR 0,A
        PI DCLS
        PI FCLEAR               ; the scene into the field (not on the screen)
        DCI TTERR
        XDC
        CLR
        LR 5,A
MEN1:   PI FCOLENT
        LR A,5
        INC
        LR 5,A
        CI FW-1
        BC MEN1
        DCI TTPIX               ; the pixels over it
MEN2:   LM
        CI 0FFH
        BZ MEN3
        LR 5,A
        LM
        LR 6,A
        LM
        LR 1,A
        LR Q,DC
        PI FSET
        LR DC,Q
        BR MEN2
MEN3:   LIS FY0                 ; light blue sky, then all of it at once
        LR 0,A
        LI FY0+FH-1
        LR 1,A
        LIS 1
        LR 2,A
        PI PALSET
        PI FSHOW
        DCI S_FORFN
        LI HROW0
        LR 1,A
        LI CV1
        LR 2,A
        PI TXCENT
        DCI S_PFIRE
        LI HROW0+1
        LR 1,A
        LI CV1
        LR 2,A
        PI TXCENT
        SND MUS_TITLE
MT1L:   PI FRAME
        LDB EVBTN
        NI 0FFH
        BNZ MT2
        LDB EVKEY
        CI KEY_END
        BNZ MT1L
MT2:    PI SNDINIT
        SND UI_SELECT
        LDB MYNAME              ; a name already: on
        NI 0FFH
        BZ MT3A
        CLR
        LR 0,A
        LEAVE
MT3A:   PI NAMEIN
        LEAVE

        INCLUDE "title.inc"
S_TITLE: DB "FUJIBARRAGE",0
S_FORFN: DB "FOR FUJINET",0

; ---- the name (menus.c name_entry: the stick's wheel) ---------------------------------
; Up and down turn the letter at the cursor through A-Z and 0-9 (held, it
; runs on); FIRE (push) or right takes it, left or pull rubs out, START ends
; (two letters at least).  r0 = 1 (the caller saves it: key 0).
NAMEIN: ENTER
        CLR
        LR 0,A
        PI DCLS
        DCI S_YNAME
        LIS 0
        LR 1,A
        LI CV1
        LR 2,A
        PI TXCENT
        LI TY0+12               ; the name's row on light grey
        LR 0,A
        LI TY0+12+5
        LR 1,A
        LIS 2
        LR 2,A
        PI PALSET
        DCI S_NH1
        LIS 5
        LR 1,A
        LI CV1
        LR 2,A
        PI TXCENT
        DCI S_NH2
        LIS 6
        LR 1,A
        LI CV1
        LR 2,A
        PI TXCENT
        DCI S_NH3
        LIS 7
        LR 1,A
        LI CV1
        LR 2,A
        PI TXCENT
        DCI S_NH4
        LIS 8
        LR 1,A
        LI CV1
        LR 2,A
        PI TXCENT
        CLR
        STB MT0                 ; letters so far
        STB MT1                 ; the letter at the cursor (0-35)
        STB MT3                 ; the repeat count
        DCI MYNAME
        LI NAME_LEN+1
        LR 0,A
NM0:    CLR
        ST
        DS 0
        BNZ NM0
NM1:    PI FRAME
        LDB MT3
        NI 0FFH
        BZ NM2
        AI -1
        STB MT3
NM2:    LDB EVKEY               ; ends
        CI KEY_END
        BNZ NM2A
        JMP NMEND
NM2A:   CI KEY_BACK             ; rubs out
        BZ NMRUB
        LDB MT3
        NI 0FFH
        BNZ NM3
        LDB INPDIR
        NI IN_LEFT
        BNZ NMRUB
NM3:    LDB EVBTN               ; takes it
        NI 0FFH
        BNZ NMTAKE
        LDB MT3
        NI 0FFH
        BNZ NMSHOW0
        LDB INPDIR
        LR 4,A
        NI IN_RIGHT
        BNZ NMTAKE
        LR A,4
        NI IN_UP
        BNZ NMUP
        LR A,4
        NI IN_DOWN
        BNZ NMDN
NMSHOW0: JMP NMSHOW
NMRUB:  LDB MT0
        NI 0FFH
        BZ NMSHOW0
        AI -1
        STB MT0
        DCI MYNAME
        ADC
        CLR
        ST
        LI 12
        STB MT3
        SND UI_SELECT
        JMP NMSHOW
NMTAKE: LDB MT0
        CI NAME_LEN
        BZ NMSHOW0
        LDB MT1
        DCI PICKSET
        ADC
        LM
        LR 1,A
        LDB MT0
        DCI MYNAME
        ADC
        LR A,1
        ST
        INCB MT0
        LI 12
        STB MT3
        SND UI_SELECT
        JMP NMSHOW
NMUP:   LDB MT1
        AI -1
        BC NMUD
        LI 35
        BR NMUD
NMDN:   LDB MT1
        INC
        CI 36
        BNZ NMUD
        CLR
NMUD:   STB MT1
        LIS 8
        STB MT3
NMSHOW: DCI MNAME               ; the name, the letter at the cursor blinking,
        XDC                     ;  spaces to 9 (blue on grey)
        DCI MYNAME
        LI NAME_LEN
        LR 2,A
NMS1:   LM
        NI 0FFH
        BZ NMS2
        XDC
        ST
        XDC
        DS 2
        BNZ NMS1
NMS2:   LDB MT0
        CI NAME_LEN
        BZ NMS4
        LDB JIFFY               ; the cursor: the letter, or a dash
        NI 10H
        LI '-'
        BZ NMS3
        LDB MT1
        DCI PICKSET
        ADC
        LM
NMS3:   XDC
        ST
        XDC
NMS4:   XDC                     ; spaces to the end
        LR Q,DC
        XDC
        LR A,QL
        AI (100H-(MNAME&0FFH))&0FFH
        COM
        INC
        AI NAME_LEN+1
        LR 2,A
        NI 0FFH
        BZ NMS6
NMS5:   LI ' '
        XDC
        ST
        XDC
        DS 2
        BNZ NMS5
NMS6:   XDC
        CLR
        ST
        LIS 8
        LR 0,A
        LIS 2
        LR 1,A
        LI CV1
        LR 2,A
        PI TXAT
        DCI MNAME
        PI TXSTR
        JMP NM1
NMEND:  LDB MT0                 ; two letters at least
        CI 1
        BNC NME1
        JMP NM1
NME1:   SND UI_GO
        LIS 1
        LR 0,A
        LEAVE

PICKSET: DB "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
S_YNAME: DB "YOUR NAME",0
S_NH1:  DB "UP/DOWN: A LETTER",0
S_NH2:  DB "PUSH: TAKE IT",0
S_NH3:  DB "PULL: RUB OUT",0
S_NH4:  DB "START: DONE",0

; ---- the rooms (menus.c room_list, net.c net_rooms) -----------------------------------
; TLSTART: T asked for (again every 60 jiffies until the count comes); the
; list drawn once every record is in (600 jiffies at most); then up and
; down choose, FIRE or START joins: SRVTBL = its id.
TLSTART: ENTER
        LI 0FFH
        STB TBLN
        STB MT3                 ; (no cursor drawn)
        CLR
        STB TBLHAVE
        STB MSEL
        STB MNSEL
        STB MWAIT
        STB MWAIT+1
        LDB INPDIR
        STB MPREV
        LI 60
        STB MBLINK
        LDB TXF
        OI TX_ROOMS
        STB TXF
        LEAVE

TLTICK: ENTER
        LDB MNSEL
        NI 0FFH
        BZ TL0
        JMP TLUI
TL0:    LDB DT                  ; waiting: 600 jiffies, T every 60
        LR 0,A
        DCI MWAIT
        LM
        AS 0
        LR 1,A
        LM
        LNK
        LR 2,A
        DCI MWAIT
        LR A,1
        ST
        LR A,2
        ST
        LR A,1                  ; >= 600: no answer
        AI 256-(600&0FFH)
        LR A,2
        LNK
        AI 255-(600>>8)
        BNC TL1
TLFAIL: LIS 2
        LR 0,A
        LEAVE
TL1:    LDB TBLN
        CI 0FFH
        BNZ TL3
        LDB DT                  ; T again every 60 jiffies until the count comes
        COM
        INC
        LR 0,A
        LDB MBLINK
        AS 0
        BZ TL2
        BC TL2X                 ; (no borrow: still counting)
TL2:    LDB TXF
        OI TX_ROOMS
        STB TXF
        LI 60
TL2X:   STB MBLINK
        BR TLON
TL3:    NI 0FFH                 ; the count: none is no answer
        BZ TLFAIL
        LR 3,A                  ; every record in?
        DCI TMASK-1
        ADC
        LM
        LR 4,A
        LDB TBLHAVE
        NS 4
        XS 4
        BNZ TLON
        LR A,3
        STB MNSEL               ; the list
        PI TLDRAW
TLON:   CLR
        LR 0,A
        LEAVE

; TLDRAW -- the list: PICK A ROOM white on black, each room a text row on
;   light grey (its name, its people seated / seats: green waiting, red
;   playing).
TLDRAW: ENTER
        CLR
        LR 0,A
        PI DCLS
        DCI S_PICK
        CLR
        LR 1,A
        LI CV1
        LR 2,A
        PI TXCENT
        LI TY0+6                ; the rooms' rows on light grey
        LR 0,A
        LI TY0+6*TROWS-1
        LR 1,A
        LIS 2
        LR 2,A
        PI PALSET
        CLR
        STB MT0
TL4:    LDB MT0                 ; its record: TBLS + 24 i
        SL 1
        SL 1
        SL 1
        LR 1,A
        SL 1
        AS 1
        DCI TBLS
        ADCU
        LR Q,DC
        LR A,QU
        STB MT1+10
        LR A,QL
        STB MT1+11
        LR DC,Q                 ; its ink: waiting green, playing red
        LI 22
        ADC
        LM
        NI 0FFH
        LI CV3
        BZ TL5
        LI CV2
TL5:    LR 2,A
        LDB MT0                 ; its row: 1 + i, column 2
        INC
        LR 1,A
        LIS 2
        LR 0,A
        PI TXAT
        DCI MT1+10              ; its name: 12, then seated/seats
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LIS 8
        ADC
        XDC
        DCI MNAME
        LI 12
        LR 2,A
TL5A:   XDC
        LM
        XDC
        NI 0FFH
        BNZ TL5B
        LI ' '
TL5B:   ST
        DS 2
        BNZ TL5A
        LI ' '
        ST
        LR Q,DC                 ; (where the counts go)
        DCI MT1+10
        LM
        LR 10,A
        LM
        LR 11,A
        LR DC,H
        LI 20
        ADC
        LM
        LR 3,A                  ; seated
        LM
        LR 4,A                  ; seats
        LR DC,Q
        LR A,3
        LR 1,A
        PI DIG2S
        LI '/'
        ST
        LR A,4
        LR 1,A
        PI DIG2S
        CLR
        ST
        DCI MNAME
        PI TXSTR
        INCB MT0
        LR 1,A
        LDB MNSEL
        XS 1
        BZ TL6
        JMP TL4
TL6:    LEAVE

; TLUI: up, down (pushed, not held); the cursor; FIRE or START: that one.
TLUI:   LDB MPREV
        COM
        LR 4,A
        LDB INPDIR
        STB MPREV
        NS 4
        LR 4,A
        NI IN_UP
        BZ TLU1
        LDB MSEL
        NI 0FFH
        BZ TLU1
        AI -1
        STB MSEL
        SND UI_SELECT
TLU1:   LR A,4
        NI IN_DOWN
        BZ TLU2
        LDB MSEL
        INC
        LR 1,A
        LDB MNSEL
        XS 1
        BZ TLU2
        LR A,1
        STB MSEL
        SND UI_SELECT
TLU2:   LDB MSEL                ; the cursor, when it moved: off the old, on the new
        LR 1,A
        LDB MT3
        XS 1
        BZ TLU5
        LDB MT3
        CI 0FFH
        BZ TLU3
        LR 0,A
        LI ' '
        LR 2,A
        PI TLCUR
TLU3:   LDB MSEL
        STB MT3
        LR 0,A
        LI '>'
        LR 2,A
        PI TLCUR
TLU5:   LDB EVBTN               ; FIRE or START: that one
        NI 0FFH
        BNZ TLU6
        LDB EVKEY
        CI KEY_END
        BZ TLU6
        JMP TLON
TLU6:   LDB MSEL                ; SRVTBL = its id
        SL 1
        SL 1
        SL 1
        LR 1,A
        SL 1
        AS 1
        DCI TBLS
        ADCU
        XDC
        DCI SRVTBL
        LI TABLE_LEN
        LR 2,A
TLU7:   XDC
        LM
        XDC
        ST
        DS 2
        BNZ TLU7
        CLR
        ST
        SND UI_GO
        LIS 1
        LR 0,A
        LEAVE

; TLCUR -- the character r2 at room row r0's cursor place, white.
TLCUR:  ENTER
        LR A,2
        STB MT1
        LR A,0
        INC
        LR 1,A
        CLR
        LR 0,A
        LI CV1
        LR 2,A
        PI TXAT
        LDB MT1
        LR 0,A
        PI TXCH
        LEAVE

TMASK:  DB 1, 3, 7, 0FH, 1FH, 3FH, 7FH, 0FFH
S_PICK: DB "PICK A ROOM",0
