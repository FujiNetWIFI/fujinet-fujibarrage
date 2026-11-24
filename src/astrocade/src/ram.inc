; ram.inc -- every byte of RAM the game keeps, and where.
;
; The Astrocade's only RAM is the 4K of screen RAM, 4000H-4FFFH, 40 bytes a
; line, and the cart has none (its edge has no /WR). The field screen shows
; LINES lines (the HUD and the field); past them, in order:
;   the raster's bands (their tank colours and splits),
;   the shot's tables (the script buffer, tracks, blasts, settles, paths,
;     flows, beams): the text screens show TLINES lines over all of this,
;     so nothing here may live across a text screen,
;   the state (ST, patched in place by the link) and the variables,
;   SCRATCH and the IM 2 vector (4FBDH-4FBFH),
;   the stack, 64 bytes in the BIOS's cells (4FC0H-4FFFH: the game never
;     calls the BIOS, and a RESET starts it cold).
; VAR hands bytes out upward; tools/astro/checksize.py fails the build when
; RAMEND passes RAMTOP, or the shot's tables end before the text screens'
; lines do (TXTEND).

RAMBASE EQU     SCR+LINES*BYTEPL        ; 4988H
STACK   EQU     5000H           ; grows down into 4FC0H-4FFFH
STKLOW  EQU     4FC0H
VECTOR  EQU     4FBEH           ; the IM 2 vector word (I = 4FH, INFBK = 0BEH)
SCRATCH EQU     4FBDH           ; a byte shifted magic writes may land in
RAMTOP  EQU     SCRATCH         ; the variables stop below it
TXTEND  EQU     SCR+TLINES*BYTEPL       ; the text screens' last line + 1

RAMP    DEFL    RAMBASE
VAR     MACRO   nm, sz
nm      EQU     RAMP
RAMP    DEFL    RAMP+sz
        ENDM

; ---- the raster: a band's entry
; HORCB, L0, R0 (3 bytes; a table never crosses a 256-byte page: INC L)
        VAR     BANDA,3*NBANDS

; ---- the shot (p_ply, p_ops): free between shots ---------------------------------
SHOT    EQU     RAMP
        VAR     SCRIPT,SCRIPT_BUF
        VAR     TRACKS,16*21    ; (p_ply.inc: TKSZ; then BLASTS, SETTLES: one LDIR)
        VAR     BLASTS,6*10
        VAR     SETTLES,8*5
        VAR     PATHS,4*7       ; (p_ops.inc; then FLOWS, BEAMS: one LDIR)
        VAR     FLOWS,4*10
        VAR     BEAMS,2*5
        VAR     PLYPC,1         ; the player: where it is in SCRIPT,
        VAR     PLYDONE,1       ;  OP_END reached,
        VAR     PLYWAIT,2       ;  frames to wait,
        VAR     PLAST,2         ;  FRAMES at its last pass,
        VAR     PLFIRE,1         ;  something burns,
        VAR     FALL,1          ;  the lowest falling shell's line | 1 (the whistle),
        VAR     STN,1           ;  a settle's columns left
        VAR     OCNT,1          ; the ops': a flow's cells, from and to,
        VAR     OTO,1
        VAR     OMUD,1          ;  mud, what becomes what,
        VAR     OFOLD,1
        VAR     OFV,1
        VAR     OFIRE,1         ;  a fire flow burns;
        VAR     LX0,1           ;  a beam's line
        VAR     LY0,1
        VAR     LDX,1
        VAR     LDY,1
        VAR     LSX,1
        VAR     LSY,1
        VAR     LON,1
        VAR     LERR,2
SHOTEND EQU     RAMP
; the text screens clear and show TLINES lines (to TXTEND): only the shot's
; tables may be there, so what follows starts at TXTEND at the lowest (the
; shot's shrank on 2026-11-22: ST's first 14 bytes were in them, the shop's
; TEXTSCR zeroed its phase and the waiting room came back, its bytes showed
; under the hints)
        IF      RAMP < TXTEND
RAMP    DEFL    TXTEND
        ENDIF

; ---- the state (src/proto.h's layout): ST + OFF_* ----------------------------------
        VAR     ST,STATE_SIZE

; ---- time, paging, input ------------------------------------------------------------
        VAR     FRAMES,2        ; 60 Hz, counted by the frame interrupt
        VAR     LASTF,2         ; FRAMES when the loop last came round
        VAR     DT,1            ; jiffies since the loop last came round (1-8)
        VAR     CURPG,1         ; the page at 2000H
        VAR     FCHL,2          ; FARCALL's scratch
        VAR     FCTGT,2
        VAR     FCA,1
        VAR     BANDP,2         ; the raster: the next band entry,
        VAR     SKYP,2          ;  the next band's sky,
        VAR     BANDTB,2        ;  a text screen's band table,
        VAR     NXTLN,1         ;  the next band's scanline,
        VAR     SKYTB,2         ;  this round's sky table (14 colours),
        VAR     HUDC,4          ;  the HUD's L0, L1, R0, R1,
        VAR     DIRTC,1         ;  the dirt's colour,
        VAR     FIREC,1         ;  the fire's when it is still
        VAR     TXPAL,2         ;  a text screen's colours (8, OTIR's order)
        VAR     FCSKY,1         ; FCOLW's sky and dirt, in place
        VAR     FCDIRT,1
        VAR     INDIR,1         ; directions held (D_*)
        VAR     PRVDIR,1        ; last frame's (a push is an edge)
        VAR     INBTN,1         ; the trigger held
        VAR     EVBTN,1         ; it went down this frame
        VAR     EVKEY,1         ; a key went down this frame (K_*, digits)
        VAR     KRAW,1          ; last frame's key (edges)
        VAR     RPNEXT,2        ; REP: the frame a held stick moves again
        VAR     FLAGS,1         ; FL_*
        VAR     RNDS,2          ; RAND's seed
        VAR     TESTHK,4        ; the rigs' offline scenes (src/test.c's test_hook)

; ---- text ------------------------------------------------------------------------------
        VAR     TXCOL,1         ; the expander's pair: fg*4 + bg
        VAR     TXPOS,2         ; the next glyph's magic address
        VAR     NUMV,1          ; NUMAT's value
        VAR     NUMD,1          ;  and a place's digit

; ---- sound (p_snd.inc) -----------------------------------------------------------------
        VAR     SNDSH,8         ; the chip's registers 0-7, written each tick
        VAR     SNDQ,1          ; sounds queued (0FFH none): two
        VAR     SNDQ2,1
        VAR     SNDCH,4*7       ; the channels A, B, C, N
        VAR     SNDSIL,1
        VAR     MUSBAR,1        ; the tune: its bar (0FDH-0FFH: start, stop, off),
        VAR     MUSJ,1          ;  the jiffy in it,
        VAR     MUSREC,2        ;  its record (snd.inc's ST_*)
        VAR     WHIST,1         ; the falling shell's whistle: A's period (0 none)

; ---- the boot, the menus, the room -----------------------------------------------------
        VAR     MSGID,1         ; the boot page's message
        VAR     FAILRM,1        ;  and E_FAIL's: the room forgotten too
        VAR     MYNAME,NAME_LEN+1       ; ASCII, NUL-padded
        VAR     SRVTBL,ROOM_LEN+1       ; the room's id, ASCII, NUL-padded
        VAR     MYSEAT,1        ; NONE: watching
        VAR     SHOWN,1         ; S_*: the screen at the room
        VAR     SHSEQ,1         ; the field's round seq when it was entered
        VAR     JOINT,2         ; frames waiting for the first state
        VAR     RCOUNT,1        ; a count for a loop around calls
        VAR     SEL,1           ; a menu's choice
        VAR     NSEL,1          ;  of how many
        VAR     T0,1            ; scratch: never across a call that says so
        VAR     T1,1
        VAR     T2,1
        VAR     T3,1
; the room list lives in ST before the state is live (the boot, the menus)
ROOMS   EQU     ST              ; MAX_ROOMS records of ROOM_REC

; ---- the link (p_net.inc) --------------------------------------------------------------
        VAR     NSEQ,1          ; the SEQ ACKSEQ echoes when it is done
        VAR     NKIND,1         ; NK_*: the transaction out (0 none)
        VAR     NWAIT,1         ; frames it has been out
        VAR     NLINK,1         ; NL_*
        VAR     NEVT,1          ; EV_*: what this frame's step brought
        VAR     NERR,1          ; an E frame's code
        VAR     NBW,2           ; bytes a STATUS said are waiting, not yet read
        VAR     TXF,1           ; TX_*: frames waiting to be sent
        VAR     ASEQ,1          ; the action: seq, kind, 6 args (its frame's payload)
        VAR     AK,1
        VAR     AARGS,6
        VAR     ANARG,1         ;  and its args' count
        VAR     TAKEN,1         ; bytes taken from the link (mod 256)
        VAR     CREDITED,1      ; and told the server
        VAR     QUIET,2         ; frames since a good frame
        VAR     QPING,2         ; the same, mod PING_EVERY
        VAR     QASK,1          ; and mod ASK_AGAIN
        VAR     RHOLD,1         ; b0-6 frames to the next RESYNC, b7 one owed
        VAR     DLEFT,1         ; the READ's bytes not yet decoded
        VAR     DRPOS,1         ; the reply's offset of the next one
        VAR     DCODE,1         ; the COBS block's data bytes left
        VAR     DFLAG,1         ; DF_*
        VAR     D1,1            ; the two decoded bytes held back (the sums at
        VAR     D2,1            ;  a frame's end)
        VAR     DN,1            ; how many are held (0-2)
        VAR     FIDX,1          ; the frame's byte: 0 its type, then payload + 1
        VAR     FTYPE,1
        VAR     S1,1            ; the running Fletcher sums
        VAR     S2,1
        VAR     QST,1           ; a patch run: 0-1 its offset, 2 its length, 3 its
                                ;  bytes, 4 a bad run
        VAR     QOFF,2          ; the run's offset (E: the code; T: the count)
        VAR     QLEN,1          ; its bytes left (T: the index)
        VAR     GX,1            ; a TERRAIN frame: the column, its entry's state,
        VAR     GST,1           ;  the spans left, a span's top; GSKIP: not ours
        VAR     GSP,1
        VAR     GTOP,1
        VAR     GSKIP,1
        VAR     NTV,1           ; the field's version (FF: not to be trusted, FE: coming)
        VAR     NTERR,1         ; terrain came since the field looked
        VAR     SEGLEN,1        ; the segment so far (bytes), its shot, its number,
        VAR     SEGSID,1        ;  its flags; SEGRDY: whole, to be played
        VAR     SEGNO,1
        VAR     SEGFLG,1
        VAR     SEGRDY,1
        VAR     DOWED,1         ; our D unanswered: its frames (ASK_AGAIN)
        VAR     DSID,1          ;  and what it said
        VAR     DSEG,1
        VAR     ROOMN,1         ; T frames: the count (0FFH: none yet)
        VAR     ROOMHAVE,1      ;  and the records in (a bit each)
        VAR     NBAD,1          ; the rigs': bad frames

; ---- the game (p_play.inc) -------------------------------------------------------------
        VAR     TKX,10          ; each tank as drawn: x, y, angle, on
        VAR     TKY,10
        VAR     TKA,10
        VAR     TKON,10
        VAR     TKDX,1          ; TANKD's tank: x, y, angle, the value drawn
        VAR     TKDY,1
        VAR     TKDA,1
        VAR     TKDV,1
        VAR     BARPW,1         ; the bars' lengths (bytes, 0-40): power, energy
        VAR     BARENG,1
        VAR     SROT,1          ; the shimmer's order (turned every half second)
        VAR     SHAKE,1         ; the screen shakes: frames left
        VAR     FLASHN,1        ; the field flashes white: frames left
        VAR     GAMEV,34        ; aim, the items line, the HUD as drawn, talk

; ---- scratch: never two users at once (a TX frame, the HUD's line, the band
; solver, the name wheel)
        VAR     WORK,42
; the ellipse's (ell.inc: never while anything else has WORK)
ELX      EQU     WORK            ; its centre (16-bit, signed)
ELY      EQU     WORK+2
ELRX     EQU     WORK+4          ; its radii
ELRY     EQU     WORK+5
ELV      EQU     WORK+6          ; the value, the mode
ELMODE   EQU     WORK+7
ELREM    EQU     WORK+8          ; its rows' stepping
ELDY     EQU     WORK+9
ELI      EQU     WORK+10
ELHW     EQU     WORK+11         ; a row's half width (2)
ELLM     EQU     WORK+13         ; a span's end bytes' masks,
ELRM     EQU     WORK+14
ELPD     EQU     WORK+15         ; the swap's from and to (a value in each pixel)
ELPE     EQU     WORK+16

RAMEND  EQU     RAMP
