; p_menu.inc -- the screens before a room (src/menus.c): the title, the
; name, the rooms. FujiBlast's (its src/arcadia/src/p_menu.inc), for
; FujiBarrage: a text title (M5 may draw a logo), the name in cart RAM,
; the room records in ST (ROOMS).

        BCTA,UN MENTER          ; E_ENTER: the title; the name if none (R0 = 1: a new one)
        BCTA,UN TLSTART         ; E_STATE: the room list asked for
        BCTA,UN TLTICK          ; E_TICK: its frame: R0 0 on, 1 picked, 2 no list

; ---- title -------------------------------------------------------------------------
MENTER: ZBSR    *ZTEXT
        DCOL    WARNI
        CTXT    STITLE,3
        DCOL    TXTI
        CTXT    SFORFN,5
        CTXT    SFIRE,9
        DCOL    USI
        CTXT    STAG,12
        SND     MUS_TITLE
MT6:    ZBSR    *ZFRAME
        LODA,R0 EVBTN           ; FIRE, Enter or Start
        BCFR,EQ MT10
        LODA,R0 EVKEY
        COMI,R0 K_ENT
        BCTR,EQ MT10
        COMI,R0 K_START
        BCFR,EQ MT6
MT10:   EORZ    R0              ; the tune off
        STRA,R0 SNDPH
        STRA,R0 VOLUME
        LODA,R0 MYNAME          ; a name: done (R0 = 0)
        BCTA,EQ NAMEIN          ; else ask
        EORZ    R0
        RETC,UN

STITLE: DB      "FUJIBARRAGE",$FF
SFORFN: DB      "FOR FUJINET",$FF
SFIRE:  DB      "PRESS FIRE",$FF
STAG:   DB      "2-10 TANKS",$FF

; ---- the name (menus.c name_entry; no keyboard) ------------------------------
; Up and down turn the letter at the cursor through A-Z and 0-9 (held, it
; runs on); FIRE or right takes it, left or Clear rubs out, Enter or Start
; ends (two at least). R0 = 1 (the caller saves it: key 0). T0 the
; letters so far, T1 the letter at the cursor, T3 the repeat count.
NAMEIN: ZBSR    *ZTEXT
        CTXT    SYNAME,1
        CTXT    SNH1,7
        CTXT    SNH2,8
        CTXT    SNH3,9
        CTXT    SNH4,10
        EORZ    R0
        STRA,R0 T0
        STRA,R0 T1
        STRA,R0 T3
        CPTR    MYNAME          ; (cart RAM: streamed)
        LODI,R3 NAME_LEN+1
NM0:    EORZ    R0
        BSTA,UN RAMPUT1
        BDRR,R3 NM0
        BSTA,UN RAMPEND
NM1:    ZBSR    *ZFRAME
        LODA,R0 T3
        BCTR,EQ NM2
        SUBI,R0 1
        STRA,R0 T3
NM2:    LODA,R0 EVKEY           ; ends
        COMI,R0 K_ENT
        BCTA,EQ NMEND
        COMI,R0 K_START
        BCTA,EQ NMEND
        COMI,R0 K_CLR           ; rubs out
        BCTR,EQ NMRUB
        LODA,R0 T3
        BCFR,EQ NM3
        LODA,R0 INDIR
        TMI,R0  D_LEFT
        BCTR,EQ NMRUB
NM3:    LODA,R0 EVBTN           ; takes it
        BCFR,EQ NMTAKE
        LODA,R0 T3
        BCFA,EQ NMSHOW
        LODA,R0 INDIR
        TMI,R0  D_RIGHT
        BCTR,EQ NMTAKE
        TMI,R0  D_UP
        BCTA,EQ NMUP
        TMI,R0  D_DOWN
        BCTA,EQ NMDN
        BCTA,UN NMSHOW
NMRUB:  LODA,R0 T0
        BCTA,EQ NMSHOW
        SUBI,R0 1
        STRA,R0 T0
        STRZ    R3
        EORZ    R0
        CPUTR   MYNAME
        LODI,R0 12
        STRA,R0 T3
        SND     UI_TICK
        BCTA,UN NMSHOW
NMTAKE: LODA,R3 T0
        COMI,R3 NAME_LEN
        BCTA,EQ NMSHOW
        LODA,R2 T1
        LODA,R0 PICKSET,R2
        CPUTR   MYNAME
        LODA,R0 T0
        ADDI,R0 1
        STRA,R0 T0
        LODI,R0 12
        STRA,R0 T3
        SND     UI_TICK
        BCTR,UN NMSHOW
NMUP:   LODA,R0 T1
        SUBI,R0 1
        BCFR,LT NMUD
        LODI,R0 35
        BCTR,UN NMUD
NMDN:   LODA,R0 T1
        ADDI,R0 1
        COMI,R0 36
        BCFR,EQ NMUD
        EORZ    R0
NMUD:   STRA,R0 T1
        LODI,R0 8
        STRA,R0 T3
NMSHOW: DAT     4,4             ; the name, then the letter blinking
        DCOL    TXTI
        LODI,R3 0
NMS1:   COMA,R3 T0              ; (R3 letters shown)
        BCTR,EQ NMS2
        LODA,R0 MYNAME,R3
        ZBSR    *ZA2G
        ZBSR    *ZDPUTC
        ADDI,R3 1
        BCTR,UN NMS1
NMS2:   LODA,R0 FRML            ; the cursor: the letter, or a dash
        ANDI,R0 $10
        BCTR,EQ NMS3
        LODA,R2 T1
        LODA,R0 PICKSET,R2
        ZBSR    *ZA2G
        BCTR,UN NMS4
NMS3:   LODI,R0 GDASH
NMS4:   STRA,R0 T2
        DAT     4,4
        LODA,R0 T0              ; (R2: the cell before the cursor)
        ADDZ    R2
        STRZ    R2
        LODA,R0 T0
        COMI,R0 NAME_LEN
        BCTR,EQ NMS5
        LODA,R0 T2
        ZBSR    *ZDPUTC
NMS5:   EORZ    R0              ; and nothing after it
        ZBSR    *ZDPUTC
        BCTA,UN NM1
NMEND:  LODA,R0 T0              ; two letters at least
        COMI,R0 2
        BCTA,LT NM1
        SND     UI_TICK
        LODI,R0 1
        RETC,UN
PICKSET: ASCII
        DB      "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        GLYPHS
SYNAME: DB      "YOUR NAME",$FF
SNH1:   DB      "UP/DOWN: LETTER",$FF
SNH2:   DB      "FIRE: TAKE IT",$FF
SNH3:   DB      "LEFT: RUB OUT",$FF
SNH4:   DB      "ENTER: DONE",$FF

; ---- the rooms (menus.c room_list, net.c net_rooms) -----------------------------
; TLSTART: T asked for (again every 60 frames until the count comes);
; the list drawn once every record is in (600 frames at most); then up
; and down choose, FIRE or Enter joins: SRVTBL = its id.
TLSTART: LODI,R0 $FF
        STRA,R0 ROOMN
        EORZ    R0
        STRA,R0 ROOMHAVE
        STRA,R0 SEL
        STRA,R0 NSEL            ; (0: the list not drawn yet)
        STRA,R0 JOINTH          ; (the frames waited)
        STRA,R0 JOINTL
        LODA,R0 TXF
        IORI,R0 TX_ROOMS
        STRA,R0 TXF
        RETC,UN

TLTICK: LODA,R0 NSEL
        BCFA,EQ TLUI
        LODA,R0 JOINTL          ; waiting: 600 frames, T every 60
        ADDI,R0 1
        STRA,R0 JOINTL
        PPSL    PSLWC
        LODA,R0 JOINTH
        ADDI,R0 0
        STRA,R0 JOINTH
        CPSL    PSLWC
        COMI,R0 600>>8
        BCTR,LT TL1
        LODA,R0 JOINTL
        COMI,R0 600&$FF
        BCTR,LT TL1
TLFAIL: LODI,R0 2
        RETC,UN
TL1:    LODA,R0 ROOMN
        COMI,R0 $FF
        BCFR,EQ TL3
        LODA,R0 JOINTL          ; (every 60: its low byte, mod 60)
        STRA,R0 T0
TL2:    LODA,R0 T0
        COMI,R0 60
        BCTR,LT TL2A
        SUBI,R0 60
        STRA,R0 T0
        BCTR,UN TL2
TL2A:   COMI,R0 0
        BCFR,EQ TLON
        LODA,R0 TXF
        IORI,R0 TX_ROOMS
        STRA,R0 TXF
TLON:   EORZ    R0
        RETC,UN
TL3:    COMI,R0 0               ; the count: none is no answer
        BCTR,EQ TLFAIL
        STRZ    R1              ; every record in?
        LODA,R0 TMASK-1,R1
        ANDA,R0 ROOMHAVE
        COMA,R0 TMASK-1,R1
        BCFR,EQ TLON
        STRA,R1 NSEL            ; the list
        ZBSR    *ZTEXT
        CTXT    SPICK,0
        CTXT    SJOIN,12
        LODI,R3 0
TL4:    STRA,R3 T1              ; a room a row from row 2
        LODA,R0 TROWOFF,R3      ; its record
        STRA,R0 T0
        STRZ    R1
        LODI,R0 TXTI            ; its ink: waiting, or playing
        STRA,R0 DCOLOR
        LODA,R0 ROOMS+22,R1
        BCTR,EQ TL5
        LODI,R0 LISTI1
        STRA,R0 DCOLOR
TL5:    LODA,R1 T1
        ADDI,R1 2
        LODI,R2 0
        ZBSR    *ZDPOS
        LODA,R3 T0              ; its name, 9 of its 12
        ADDI,R3 7
        LODI,R0 9
        STRA,R0 T2
TL6:    LODA,R0 ROOMS,R3,+
        ZBSR    *ZA2G
        ZBSR    *ZDPUTC
        LODA,R0 T2
        SUBI,R0 1
        STRA,R0 T2
        BCFR,EQ TL6
        LODI,R2 10              ; seated / seats, from column 11
        LODA,R3 T0
        LODA,R0 ROOMS+20,R3
        LODI,R1 2
        ZBSR    *ZDNUM
        LODI,R0 GSLASH
        ZBSR    *ZDPUTC
        LODA,R3 T0
        LODA,R0 ROOMS+21,R3
        LODI,R1 2
        ZBSR    *ZDNUM
        LODA,R3 T1
        ADDI,R3 1
        COMA,R3 NSEL
        BCFA,EQ TL4
        BCTA,UN TLON

TLUI:   LODA,R0 INDIR           ; up, down (pushed, not held)
        STRA,R0 T0
        LODA,R0 PRVDIR
        EORI,R0 $FF
        ANDA,R0 T0
        STRA,R0 T0
        TMI,R0  D_UP
        BCFR,EQ TLU1
        LODA,R0 SEL
        BCTR,EQ TLU1
        SUBI,R0 1
        STRA,R0 SEL
        SND     UI_TICK
TLU1:   LODA,R0 T0
        TMI,R0  D_DOWN
        BCFR,EQ TLU2
        LODA,R0 SEL
        ADDI,R0 1
        COMA,R0 NSEL
        BCTR,EQ TLU2
        STRA,R0 SEL
        SND     UI_TICK
TLU2:   LODI,R3 0               ; the cursor
TLU3:   LODZ    R3
        ADDI,R0 2
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R2
        EORZ    R0
        COMA,R3 SEL
        BCFR,EQ TLU4
        LODI,R0 GCURSOR|USI
TLU4:   STRA,R0 SCR,R2
        ADDI,R3 1
        COMA,R3 NSEL
        BCFR,EQ TLU3
        LODA,R0 EVBTN           ; FIRE or Enter: that one
        BCFR,EQ TLU5
        LODA,R0 EVKEY
        COMI,R0 K_ENT
        BCFA,EQ TLON
TLU5:   CPTR    SRVTBL          ; SRVTBL = its id (cart RAM: streamed), a NUL
        LODA,R1 SEL
        LODA,R0 TROWOFF,R1
        SUBI,R0 1
        STRZ    R3
        LODI,R1 ROOM_LEN
TLU6:   LODA,R0 ROOMS,R3,+
        BSTA,UN RAMPUT1
        BDRR,R1 TLU6
        EORZ    R0
        BSTA,UN RAMPUT1
        BSTA,UN RAMPEND
        SND     UI_TICK
        LODI,R0 1
        RETC,UN

TMASK:  DB      1,3,7,$F,$1F,$3F,$7F,$FF
TROWOFF: DB     0,ROOM_REC,2*ROOM_REC,3*ROOM_REC,4*ROOM_REC,5*ROOM_REC,6*ROOM_REC,7*ROOM_REC
SPICK:  DB      "PICK A ROOM",$FF
SJOIN:  DB      "FIRE: JOIN",$FF
