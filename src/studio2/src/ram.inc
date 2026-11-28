; ram.inc -- the ledger. Console RAM ($0800-$09FF): $0800-$080F the
; runtime's (rt/s2macro.inc), then the variables laid by CVAR on page $08
; (reached through RA: VAR/GETV/PUTV), page $09's buffers, the stack from
; $09FF down. The cart's RAM: image pages, read directly and written by
; POKE, laid by RGN in three areas; only the pages used are in the image.
; Nothing overlaps by construction; each area's end is checked.

CPTR    SET     V_APP
CVAR    MACRO   name, size
name    EQU     CPTR
CPTR    SET     CPTR+(size)
        ENDM

RPTR    SET     0
RAREA   MACRO   start
RPTR    SET     start
        ENDM
RGN     MACRO   name, size
name    EQU     RPTR
RPTR    SET     RPTR+(size)
        ENDM
RPAGE   MACRO                   ; the next region page-aligned
RPTR    SET     (RPTR+0FFH)&0FF00H
        ENDM
RAEND   MACRO   lim, name
        IF      RPTR > lim
        ERROR   "cart RAM area overflows"
        ENDIF
name    EQU     (RPTR+0FFH)&0FF00H
        ENDM

; ---- console RAM: core --------------------------------------------------------
        CVAR    TESTHK, 4       ; the harness's hook (3C C3, 5A ..., A5)
        CVAR    CURBANK, 1      ; the bank in the window (calls.inc)
        CVAR    JIFFY, 2        ; the game's clock (jiffies run), lo hi
        CVAR    LASTRE, 1       ; RE.0 when the last pass began
        CVAR    DT, 1           ; jiffies this pass runs (at most 8)
        CVAR    PASSES, 2       ; the main loop's passes (the rigs read it)
        CVAR    FLAGS, 1        ; FL_*
        CVAR    KHELD, 2        ; the keys held now: bit k key k (either pad)
        CVAR    KNEW, 2         ;  pressed since a screen took them
        CVAR    KTMP, 1         ; the key number OUT 2 latches
        CVAR    T0, 1           ; scratch for one routine at a time
        CVAR    T1, 1
        CVAR    T2, 1
        CVAR    T3, 1
        CVAR    T4, 1
        CVAR    T5, 1
        CVAR    T6, 1
        CVAR    T7, 1
        CVAR    KEV, 1          ; a key pressed or repeating (NONE: taken)
        CVAR    KLAST, 1        ;  the lowest key held at the last scan
        CVAR    KREPT, 1        ;  JIFFY lo when it repeats next
        CVAR    TXM, 1          ; txt_*'s mode (TM_SET, + TM_SHORT)
        CVAR    MSGID, 1        ; the message (b7: the room forgotten)
        CVAR    MRC, 1          ; a count for a loop around calls
        CVAR    MYNAME, NAME_LEN+1 ; ASCII, NUL-padded
        CVAR    SHOWS, 1        ; S_*: the screen up
        CVAR    SHSEQ, 1        ; the state's seq when it was entered
        CVAR    MYSEAT, 1       ; our seat (NONE: watching)
        CVAR    TESTEV, 1       ; a test scene's link events (the rig pokes)
FL_TEST EQU     01H             ; FLAGS: an offline test scene: the link is quiet
FL_MUTE EQU     02H

; ---- console RAM: the link (net.inc) ------------------------------------------------
        CVAR    NSEQ, 1         ; the SEQ ACKSEQ echoes when it is done
        CVAR    NKIND, 1        ; NK_*: the transaction out (0 none)
        CVAR    NWAIT, 1        ; passes it has been out
        CVAR    NLINK, 1        ; NL_*
        CVAR    NZERO, 0        ; (ninit zeroes NZERO up to NZEND)
        CVAR    NEVT, 1         ; EV_*: what this pass's step brought
        CVAR    NERR, 1         ; an E frame's code
        CVAR    NBW, 2          ; bytes a STATUS said are waiting, not read
        CVAR    TXF, 1          ; TX_*: frames waiting to be sent
        CVAR    ASEQ, 1         ; the action: seq, kind, args (its payload)
        CVAR    AK, 1
        CVAR    AARGS, 6
        CVAR    ANARG, 1        ;  how many args
        CVAR    TAKEN, 1        ; bytes taken from the link (mod 256)
        CVAR    CREDITED, 1     ;  and told the server
        CVAR    QUIET, 2        ; jiffies since a good frame
        CVAR    QPING, 2        ;  the same, to the next PING
        CVAR    QASK, 1         ;  and to the next HELLO
        CVAR    RHOLD, 1        ; b0-6 passes to the next RESYNC, b7 one owed
        CVAR    DLEFT, 1        ; the READ's bytes not yet decoded
        CVAR    DRPOS, 1        ; the reply's offset of the next one
        CVAR    DCODE, 1        ; the COBS block's data bytes left
        CVAR    FBN, 1          ; FBUF's bytes
        CVAR    DFLAG, 1        ; DF_*
        CVAR    TBLN, 1         ; T frames: the count (FF: none yet)
        CVAR    TBLHAVE, 1      ;  and the records in (a bit each)
        CVAR    GMID, 1         ; a TERRAIN version half applied
        CVAR    SEGSID, 1       ; the segment in SCRIPT: its script, number,
        CVAR    SEGNO, 1        ;  flags, length so far; whole (SEGRDY)
        CVAR    SEGFLG, 1
        CVAR    SEGLEN, 2
        CVAR    SEGRDY, 1
        CVAR    CATN, 2         ; the catalog's bytes held whole
        CVAR    CATOK, 1        ;  and indexed (cat.inc)
        CVAR    NZEND, 0
        CVAR    NBAD, 1         ; frames that failed their sums (the rigs)
        CVAR    JOINT, 2        ; jiffies waiting for the first state
        CVAR    RAWN, 1         ; the frame being built: its length
        CVAR    BK, 1           ;  its TXF bit's number
        CVAR    SGAP, 1         ; jiffies before the next STATUS
        CVAR    DSV1, 1         ; decode's place across frend: the reply's
        CVAR    DSV2, 1         ;  pointer (low), the chunk's bytes left
        CVAR    NTV, 1          ; the field's TERRAIN version (FF: none)
        CVAR    DSID, 1         ; our D: the segment played
        CVAR    DSEG, 1

; ---- console RAM: the field (fld.inc) ---------------------------------------------
        CVAR    DIRTY, 8        ; the columns to draw again, a bit each
        CVAR    FSV, 2          ; fld_run's registers across fld_spans
        CVAR    FTT, 1          ;  its column's entry across fld_crust
        CVAR    DIRTM, 1        ; how dirt is drawn: RECT mode + pattern
        CVAR    DCRUST, 1       ;  a solid top line on it (1)

; ---- console RAM: the tanks (tank.inc) -----------------------------------------------
        CVAR    TKON, SEATS     ; each seat as wanted: seated and alive,
        CVAR    TKX, SEATS      ;  the hull's bottom centre (x, y),
        CVAR    TKY, SEATS
        CVAR    TKST, SEATS     ;  the barrel's step (15 degrees)
        CVAR    TDST, SEATS     ; the step its image holds (NONE: none sent)
        CVAR    TDON, SEATS     ;  its sprite shown (TDST, TDON in order)
        CVAR    TKDRT, 2        ; the seats to see to, a bit each
        CVAR    TKBL, 1         ; the seat blinking (NONE: none)
        CVAR    TKBLP, 1        ;  its phase now

; ---- console RAM: the aim and the HUD (p_hud.inc, hud.inc) ------------------------------
        CVAR    AIMING, 1       ; ours to aim
        CVAR    AIMA, 1         ; the aim: angle, power, weapon
        CVAR    AIMP, 2
        CVAR    AWPN, 1
        CVAR    SENTA, 1        ;  as the server last had it
        CVAR    SENTP, 2
        CVAR    AIMT, 1         ;  RE.0 when it was sent
        CVAR    AHT, 1          ; jiffies a way has been held
        CVAR    FIRED, 1        ; we fired: not our turn again until it ends
        CVAR    IDLEAT, 1       ; JIFFY's high byte at our last key (the cue again)
        CVAR    SNDID, 1        ; the sound playing (sound.inc)
        CVAR    SNDPH, 1        ;  its next pair (0: none)
        CVAR    SNDPL, 1
        CVAR    SNDT, 1         ;  JIFFY's low byte when it's due
        CVAR    HUDDRT, 1       ; the HUD's rows to build again
        CVAR    HSEAT, 1        ; the HUD's seat
        CVAR    HBARE, 1        ; its bars as drawn: energy, timer
        CVAR    HBART, 1
; the field's own lines (fieldui.inc, msg.inc)
        CVAR    MENU, 1         ; the items line: M_OFF, M_ITEM, M_TARGET, M_DRIVE
        CVAR    ITEM, 1         ;  the item on it
        CVAR    GUIDE, 1        ;  the guidance our fire carries (I_NONE)
        CVAR    TARGET, 1       ;  its target (NO_SEAT)
        CVAR    DRIVN, 1        ;  jiffies to the next ACT_DRIVE
        CVAR    DEFOPN, 1       ; the defence window has opened
        CVAR    BANNERT, 1      ; jiffies YOUR TURN! stays (0: none)
        CVAR    HMSGT, 1         ; jiffies the message stays (0: none)
        CVAR    MSGK, 1         ;  MK_*
        CVAR    MSGS, 1         ;  the seat
        CVAR    MSGA, 1         ;  talk: the page's first letter; else the energy,
        CVAR    MSGB, 1         ;  talk: its length; shield: the event
        CVAR    MSGC, 1         ;  talk: the next page's; shield: the hp

; ---- console RAM: the shot player (p_ply.inc) -----------------------------------------
        CVAR    PNOW, 2         ; the shot's frame (LE)
        CVAR    PWAIT, 2        ;  the frame the next ops run
        CVAR    PPC, 2          ; the script's next op (the cart's address)
        CVAR    PLAYING, 1      ; a segment is being played
        CVAR    POPSTOP, 1      ;  the frame's ops are done (WAIT, END)
        CVAR    PSTEPS, 1       ;  frames left this pass
        CVAR    PSHAKE, 1       ; frames of shaking left
        CVAR    PFLASH, 1       ;  of flashing
        CVAR    VIEWF, 1        ; the VIEW's flags: the sky's (TV_INVERT: day)
        CVAR    VIEWD, 2        ;  the VIEW shown: dy, flags
        CVAR    EX, 1           ; ell_draw's ellipse: centre (signed),
        CVAR    EY, 1
        CVAR    ERX, 1          ;  radii,
        CVAR    ERY, 1
        CVAR    EMODE, 1        ;  mode; a dot's (dot_r9) too
        CVAR    EH, 1           ;  a column's half height,
        CVAR    ESTEP, 2        ;  the step across QT (8.8), where it is,
        CVAR    EACC, 2
        CVAR    EDX, 1          ;  the column
BLX1    EQU     EX              ; be_line's end, steps, sizes, error: over
BLY1    EQU     EY              ;  ell_draw's (never at once; EMODE is
BLSX    EQU     ERX             ;  both's, the dots' mode)
BLSY    EQU     ERY
BLDX    EQU     EH
BLDY    EQU     ESTEP
BLERR   EQU     EACC            ; (2)
        CVAR    PCT, 1          ; records on: tracks, blasts, settles, paths,
        CVAR    PCB, 1          ;  flows, beams (a table with none is skipped)
        CVAR    PCS, 1
        CVAR    PCP, 1
        CVAR    PCF, 1
        CVAR    PCE, 1

PHEND   EQU     CPTR
        IF      PHEND > 0900H
        ERROR   "RAM ledger runs off page $08"
        ENDIF

; ---- page $09: the frame being built, the hot buffers, the stack -------------------
RAWBUF  EQU     0900H           ; 40: a frame being built (HELLO is the longest)
VALBUF  EQU     RAWBUF+28       ; 12: a setting's value as text (cat_value:
                                ;  RAWBUF is the TX's only inside postw)
NUMB    EQU     RAWBUF+22       ; 6: num_s's digits, NUL-ended (the same)
CPTR    SET     RAWBUF+40
        CVAR    HLINE, 17       ; a HUD row being built (hud.inc)
        CVAR    SRVTBL, ROOM_LEN+1 ; the room's id, NUL-padded ("": none; GET9)
        CVAR    PTRK, 12*NTRK   ; the tracks' ARCs: X3 Y3 VX3 VY3 (p_ply.inc)
        CVAR    PHNX, NTRK      ;  their heads now (NONE: none), x
        CVAR    PHNY, NTRK      ;   y
        CVAR    PHX, NTRK       ;  and as their sprites show them
        CVAR    PHY, NTRK
        CVAR    PBSTG, NBLAST   ; each blast's stage drawn
        CVAR    PFSHN, NFLOW    ; each flow's cells shown (after PBSTG)
        CVAR    PAX, NPATH      ; each path's head: x, y, step, drawn
        CVAR    PAY, NPATH
        CVAR    PAI, NPATH
        CVAR    PAD, NPATH
        IF      TEST
; the test scenes' own (p_test.inc)
        CVAR    TSCN, 1         ; the scene
        CVAR    TMARK, 16       ; marks: RE.0 at each step (the harness peeks)
        CVAR    TK, 1           ; a loop's count
TARG    EQU     TESTHK+2        ; the scene's argument (GET9)
        ENDIF
STKLOW  EQU     09D0H           ; the stack grows down from $09FF to here
        IF      CPTR > STKLOW
        ERROR   "page $09 runs into the stack"
        ENDIF

; ---- the cart's RAM -----------------------------------------------------------
        RAREA   0A400H
        RGN     CATBUF, CAT_MAX ; the catalog (K frames)
        RGN     TBLS, MAX_ROOMS*ROOM_REC ; the room list (T frames)
        RGN     FBUF, MAX_FRAME+4 ; a frame being decoded
        RGN     CR_URL, 66      ; the socket's spec, "N:TCP://host:port/"
        RGN     CATIDX, 2*(4+N_ITEMS+N_SETTINGS+1) ; the catalog's index (cat.inc)
        RAEND   0B000H, CRAM1E
CRAM1   EQU     0A400H

        RAREA   0B400H
        RGN     SPN, 8*256      ; the field's spans: page HI(SPN)+x/8, 32 (x mod 8)
        RGN     TOPS, 64        ; the field's entries' first bytes, a column each
        RGN     WKS, 16         ; the settings page's settings (p_room.inc)
        RGN     SHIDS, N_ITEMS  ; the shop's items (p_shop.inc)
        RGN     TALKB, MAX_TALK ; the talk shown (msg.inc)
        RAEND   0C000H, CRAM2E
CRAM2   EQU     0B400H

        RAREA   0D400H
        RGN     ST, STATE_SIZE  ; the state, patched in place (Q frames)
        RPAGE
        RGN     SCRIPT, SCRIPT_BUF ; a held segment (S frames)
        RGN     HSHAD, 32       ; the HUD's rows as drawn (hud.inc)
        RPAGE
        RGN     PTREC, NTRK*8   ; the player's records (p_ply.inc), in order
        RGN     PBREC, NBLAST*16
        RGN     PSREC, NSETTLE*8
        RGN     PPREC, NPATH*8
        RGN     PFREC, NFLOW*16
        RGN     PEREC, NBEAM*8
        RGN     PRECEND, 0
        RAEND   0E000H, CRAM3E
CRAM3   EQU     0D400H
        IF      LO(SPN) <> 0
        ERROR   "SPN must be page-aligned"
        ENDIF

FIXBUF  EQU     0C400H          ; (TEST) bank BK_FIX's window: the rigs' fixture
FIXLEN  EQU     3072
