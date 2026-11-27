; ---------------------------------------------------------------------------
; ram.inc -- the cart's RAM ($8000-$EFFF used; $F000 on is left alone: a DC0
; run there reaches the mailbox), directly read and written.
;
; It survives a console RESET (the cart has no reset line), so START sets
; every byte it relies on (it zeroes VARS..RAMEND).
;
; The regions are laid down one after another by RGN (no two can overlap);
; tools/chf/checksize.py prints them and fails the build past RAMTOP.
; ---------------------------------------------------------------------------

RAMTOP  EQU 0F000H

RAMPTR  SET 08000H
RGN     MACRO name, size
name    EQU RAMPTR
RAMPTR  SET RAMPTR+(size)
        ENDM

; FLD: the field, a byte a pixel, column-major: pixel (x, y) at FLD + 64x + y
; (x 0-101, y 0-45), the port-1 code of its value (PX_*). While the field
; is on the screen it is what the screen shows there too (disp.inc).
; Columns 102-127 are the catalog's (CAT_MAX 1664 bytes, from K frames).
        RGN FLD, 128*64
CATBUF  EQU FLD+FW*64           ; = $9980: 1664 bytes to $9FFF

; PLYRAM: the shot player, page PLY assembled here and copied at START.
        RGN PLYRAM, PGSIZE
; DATA: page DATA (the sounds) assembled here and copied at START.
        RGN DATA, 0800H

        RGN ST, STATE_SIZE      ; the state, patched in place (Q frames)
        RGN SCRIPT, SCRIPT_BUF  ; a held segment (S frames)
        RGN TBLS, MAX_ROOMS*ROOM_REC ; the room list (T frames)
        RGN TXS, NTROWS*TCOLS*2 ; the text shadow: code, ink a cell
        RGN PALS, 64            ; each screen row's palette shown (0-3)
        RGN FBUF, MAX_FRAME+4   ; a frame being decoded
        RGN RAWBUF, 40          ; a frame being built (HELLO is the longest)

; --- globals: VARS..RAMEND zeroed at START ---------------------------------
        RGN VARS, 0
VAR     MACRO name, size
name    EQU RAMPTR
RAMPTR  SET RAMPTR+(size)
        ENDM

; core: the clock and the pages
        VAR FRAMES, 2           ; the cart's tick when the loop last woke
        VAR CURPG, 1            ; page mapped at $4800
        VAR PGSP, 1             ; page stack depth (CURPG, PGSP, PGSTK in order)
        VAR PGSTK, 4            ; pages under a FAR call
        VAR TESTHK, 4           ; the rigs' test hook (p_boot)
        VAR MENU, 1             ; the field's UI (hud.inc, p_hud.inc): the items line
        VAR ITEM, 1             ;   its item
        VAR GUIDE, 1            ;   guidance for the next shot (I_NONE)
        VAR TARGET, 1           ;   its target (NO_SEAT)
        VAR DPREV, 1            ;   the stick last pass (its new presses)
        VAR DRIVN, 1            ;   passes driven
        VAR DEFOPN, 1           ;   the defence window seen open
        VAR HDEF, 1             ;   defending (this pass)
        VAR IREDR, 1            ;   row 2 to draw again
        VAR NXI, 1              ;   NEXTI, NEXTT: from, step, tries, the window
        VAR NXD, 1
        VAR NXT, 1
        VAR NXN, 1
        VAR NXDEF, 1
        VAR FLK, 1              ;   what flashes on row 1 (FL_*)
        VAR FLJ, 1              ;   YOUR TURN!'s jiffies left
        VAR IDLEJ, 2            ;   jiffies idle on our turn
        VAR MSGJ, 1             ;   a message's jiffies left (the rows wait)
        VAR MSGK, 1             ;   its kind (MK_*)
        VAR TALKS, 1            ;   a talk: its seat
        VAR TALKN, 1            ;     its letters left
        VAR TALKI, 1            ;     the next one's index
        VAR TALKP, 2            ;     its words (in SCRIPT)
        VAR TALKX, 1            ;     the next one's cell
        VAR TALKY, 1
        VAR TALKB, 1            ;     the index row 2 starts at (FF none)
        VAR MLCH, 1
        VAR PDT, 1              ; the player: a pass's jiffies
        VAR INSCR, 1            ;   a shot under way (its segments to come)
        VAR FLASHN, 1           ;   a flash's frames left
        VAR EVPEND, 1           ;   an event line queued: its op, arguments
        VAR EVKIND, 1
        VAR EVARG, 3
        VAR EVJ, 1              ;   jiffies to the next one drawn
        VAR SHKJ, 1             ;   the shake's jiffies to its next flicker
        VAR HB, 48              ; hud.inc: a row (24 + NUL), scratch from 40
        VAR PXR0, 1             ; PALX
        VAR PXR1, 1
        VAR PXM, 1
        VAR FIXRDY, 1           ; the rig has loaded FIXBUF (scene 3), ST (4)
        VAR TESTEV, 1           ; scene 4: the next pass's NEVT (a commit)
        VAR DBGAT, 1            ; where the loop is (the harness peeks it)
        VAR JIFFY, 2            ; the game's clock (jiffies run)
        VAR SNDB, 1             ; the tone bits now out on port 5 (7-6)
        VAR DT, 1               ; jiffies this pass runs (FRAME; at most 8)
        VAR PASSES, 2           ; the main loop's passes (the rigs read it)
        VAR FLAGS, 1            ; FL_*
        VAR RCOUNT, 1           ; a count for a loop around calls
        VAR T2, 1

; the display (disp.inc)
        VAR VMODE, 1            ; VM_*: what the screen's field area shows
        VAR TXX, 1              ; text: the next cell's column (0-23)
        VAR TXY, 1              ;   its row (0-10)
        VAR TXINK, 1            ;   the ink code
        VAR TXPTR, 2            ;   a string being drawn
        VAR TXOLD, 2            ;   a cell's old code, ink
        VAR DS0, 16             ; disp scratch (never across a call)
        VAR NWRIT, 2            ; pixels written (the rigs read it)

; input and sound (input.inc, sound.inc)
        VAR INPDIR, 1           ; held directions (IN_*)
        VAR INPBTN, 1           ; button held
        VAR EVBTN, 1            ; taken: the button was pressed
        VAR EVKEY, 1            ; taken: a key
        VAR INLBTN, 1           ; latched until taken
        VAR INLKEY, 1
        VAR INPREV, 1           ; last scan: controller
        VAR INPREV0, 1          ; last scan: console buttons
        VAR INPHK, 1            ; test hook: nonzero = use the next three
        VAR INPHKD, 1
        VAR INPHKB, 1
        VAR INPHKK, 1
        VAR SNDID, 1            ; playing (255 none)
        VAR SNDT, 1             ; frames left in this note
        VAR SNDN, 1             ; note number
        VAR SNDMUTE, 1
        VAR SNDTONE, 1          ; tone bits held (port 5 bits 7-6)
        VAR SNDAGE, 1           ; frames into the note
        VAR SNDMODE, 1          ; its retrigger rule
        VAR SNDPTR, 2           ; the note list

; the link (net.inc): NINIT zeroes NZERO up to NZEND
        VAR NSEQ, 1             ; the SEQ ACKSEQ echoes when it is done
        VAR NKIND, 1            ; NK_*: the transaction out (0 none)
        VAR NWAIT, 1            ; passes it has been out
        VAR NLINK, 1            ; NL_*
        VAR MYSEAT, 1           ; our seat (FF: watching)
        VAR MYNAME, NAME_LEN+1  ; ASCII, NUL-padded
        VAR SRVTBL, TABLE_LEN+1 ; the table's id, NUL-padded
        VAR MSGID, 1            ; the boot page's message
        VAR FAILRM, 1           ;  and E_FAIL's: the room forgotten too
        VAR NBAD, 1             ; frames that failed their sums (the rigs)
        VAR SHOWS, 1            ; S_*: the screen up (main.inc)
        VAR SHSEQ, 1            ;  and the round's seq when it was entered
        VAR NZERO, 0            ; NINIT zeroes NZERO..NZEND
        VAR NEVT, 1             ; EV_*: what this pass's step brought
        VAR NERR, 1             ; an E frame's code
        VAR NBW, 2              ; bytes a STATUS said are waiting, not yet read
        VAR TXF, 1              ; TX_*: frames waiting to be sent
        VAR ASEQ, 1             ; the action: seq, kind, args (its payload)
        VAR AK, 1
        VAR AARGS, 7
        VAR ANARG, 1            ;  its args' count
        VAR TAKEN, 1            ; bytes taken from the link (mod 256)
        VAR CREDITED, 1         ;  and told the server
        VAR QUIET, 2            ; jiffies since a good frame
        VAR QPING, 2            ;  the same, to the next PING
        VAR QASK, 1             ;  and to the next HELLO
        VAR RHOLD, 1            ; b0-6 passes to the next RESYNC, b7 one owed
        VAR DLEFT, 1            ; the READ's bytes not yet decoded
        VAR DRPOS, 1            ; the reply's offset of the next one
        VAR DCODE, 1            ; (DCODE, FBN, DFLAG in this order: DECODE)
        VAR FBN, 1              ; FBUF's bytes
        VAR DFLAG, 1
        VAR TBLN, 1             ; T frames: the count (FF: none yet)
        VAR TBLHAVE, 1          ;  and the records in (a bit each)
        VAR GMID, 1             ; a TERRAIN version half applied
        VAR SEGRDY, 1           ; a segment whole in SCRIPT, not yet played
        VAR DSID, 1             ; the D owed: sid, seg
        VAR DSEG, 1
        VAR CATN, 2             ; the catalog's bytes held whole (CATBUF)
        VAR NZEND, 0
        VAR NTV, 1              ; our field's terrain version (FF: not to be trusted)
        VAR SEGSID, 1           ; the segment in SCRIPT: sid, seg, flags (in order)
        VAR SEGNO, 1
        VAR SEGFLG, 1
        VAR JOINT, 2            ; jiffies waiting for the first state
        VAR DRP2, 3             ; DECODE's place across FREND
        VAR RAWN, 1             ; the frame being built: its length
        VAR BK, 1               ;  its TXF bit's number

; the tanks (tank.inc): wanted, then drawn (TD* = TK* + 40)
        VAR TKX, SEATS
        VAR TKY, SEATS
        VAR TKA, SEATS
        VAR TKON, SEATS
        VAR TDX, SEATS
        VAR TDY, SEATS
        VAR TDA, SEATS
        VAR TDON, SEATS
        VAR TKS, 1              ; the seat TKSHOW/TKPLOT draw
        VAR TKS2, 1             ; TKALL's, TKSTATE's
        VAR TKOFS, 1            ; 0: the wanted tank, 40: the drawn one
        VAR TKMODE, 1           ; 1 drawn, 0 taken off
        VAR TKPX, 1
        VAR TKPY, 1
        VAR TKBRL, 1            ; 4 x the barrel's row in BARL
        VAR TKN, 1              ; TKPIX: pixels left
        VAR TKB, 1              ;  1: the barrel's (dy up from the turret)
        VAR TKSN, SEATS         ; each tank's pixels kept (TKSAVE)
        VAR TKSP, 2             ; the next entry (big-endian)
        VAR TKSAVE, SEATS*54    ; x, y, what it covered: 18 a tank
; aiming (p_hud.inc)
        VAR MYTURN, 1           ; we aim now
        VAR AIMA, 1             ; our angle, power, weapon
        VAR AIMP, 2
        VAR AIMW, 1
        VAR SENTA, 1            ; what the last ACT_AIM said
        VAR SENTP, 2
        VAR SENTT, 1            ; JIFFY's low byte when it went
        VAR HELDN, 1            ; passes the stick has been held
        VAR BLINKT, 1           ; the turn's tank: shown (b7) and its count
; the catalog's index (cat.inc)
        VAR CATOK, 1
        VAR ITEMAT, 2*N_ITEMS   ; each item's record (big-endian addresses)
        VAR SETAT, 2*N_SETTINGS
        VAR PAGEAT, 2*4
        VAR TIERAT, 2*8
        VAR NTIERS, 1
        VAR FCTOP, 1            ; FCOLENT: the column's top
        VAR SKY0, 1             ; SKYSET's band
        VAR SKY1, 1
        VAR SKYP, 1             ;  a night sky's palette below its horizon

; the shot player (p_ply.inc): records, a slot a record
NBLAST  EQU 10
NSETTLE EQU 8
NTRK    EQU 16
NPATH   EQU 4
NFLOW   EQU 4
NBEAM   EQU 2
        VAR PP, 2               ; the next op's address (big-endian)
        VAR PEND, 2             ; the segment's end (big-endian)
        VAR PWAIT, 2            ; frames to wait (LE)
        VAR PDONE, 1            ; OP_END (or not ours): the segment is done
        VAR PLAST, 2            ; the tick the timeline has stepped to (LE)
        VAR PSTEPS, 1           ; steps this pass
        VAR PFALL, 1            ; the lowest falling shell's line | 1 (the whistle)
        VAR PSHAKE, 1           ; shake frames left (FXTICK: the sky flickers)
        VAR PI0, 1              ; the record in hand
        VAR QSV, 2              ; QSAVE's
        VAR PT, 16              ; the player's scratch (never across a call of its own)
        VAR EL, 16              ; the ellipse's
        VAR PREC0, 0            ; (PRESET zeroes PREC0..PRECEND)
        VAR BLS, NBLAST*16      ; on, kind, n, tot, rx, ry, dn, xl, xh, yl, yh, drx, dry
        VAR SETS, NSETTLE*8     ; on, left, n, xl, xh, ph, pl
        VAR TRK, NTRK*32        ; on, style, X3, Y3, VX3, VY3, AX2, AY2, n2, px2, py2, under, drawn
        VAR PATHS, NPATH*16     ; on, sty, x, y, n, dr, ph, pl
        VAR FLOWS, NFLOW*16     ; on, kind, frm, f, hold, n, sh, accl, acch, ph, pl
        VAR BEAMS, NBEAM*8      ; left, x0, y0, x1, y1
        VAR PRECEND, 0
        VAR SEGLEN, 2           ; the segment's bytes in SCRIPT (LE: FSCRIPT)

; the text screens at a room (p_scr.inc)
        VAR SSET, 1             ; the settings showing
        VAR SPAGE, 1            ;  their page
        VAR SSEL, 1             ; a list's cursor (settings, shop)
        VAR SCTOP, 1             ;  its first row shown
        VAR SN, 1               ;  its entries
        VAR SPREV, 1            ; the stick last pass (presses)
        VAR SREP, 1             ; jiffies to a held stick's next move
        VAR SMSG, 1             ; the shop's last row: 0 the keys, 1 no cash
        VAR SK, 16              ; a settings page's settings
        VAR SHOPIDS, N_ITEMS    ; the shop's items
        VAR SVTXT, 11           ; a setting's value as text
        VAR SVN, 1              ;  its length
; numbers (num.inc)
        VAR N32, 4              ; DEC32's value (spent)
        VAR NBUF, 8             ;  its digits

; scratch for the game logic (any routine; never across a call)
        VAR GT, 32
; the test scenes' own (p_test)
        VAR TV, 64

; a test fixture (tools/chf/refcheck.py loads it: p_test.inc's layout)
        RGN FIXBUF, 1000H
RAMEND  EQU RAMPTR
        IF RAMEND > RAMTOP
        ERROR "RAM past RAMTOP"
        ENDIF
