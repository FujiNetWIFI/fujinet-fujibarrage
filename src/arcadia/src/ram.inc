; ram.inc -- every byte of RAM the game keeps, and where: the RAM LEDGER.
;
; The Arcadia has two kinds of RAM, and the field's screen mode takes most
; of one:
;
; CONSOLE RAM (the 2637's): fast, read and written by page-0 code (fixed A
;   and every page) directly, by fixed B through pointer words.
;   26-row mode (the field: 24 rows of 3x2-block cells + 2 HUD rows) shows
;   $1800-$18CF and $1A00-$1ACF, which leaves 84 bytes:
;     zone A   $18D0-$18EF   32   time, input, display pointers, scratch, link
;     island   $18F8-$18FB    4   sound      ($18F0-$18F7: the sprites)
;     zone C   $1AD0-$1AFF   48   the link's decoder and clocks; the rest free
;   13-row mode (every text screen: the boot, the menus, the room, M4's shop
;   and results) doesn't show $1A00-$1ACF:
;     TZ       $1A00-$1ACF  208   a text screen's own. DEAD ON THE FIELD: the
;                                 field draws its lower 13 rows there, so TZ
;                                 is written only while a text screen shows
;                                 (TEXTSCR first), and holds nothing across
;                                 the field (a message's id goes to the boot
;                                 page in R2, not through TZ).
;   Nothing persistent may lie in $1800-$18CF or $1A00-$1ACF: a screen
;   clears them (the Astrocade's bug of 2026-11-23: a screen cleared ST).
;
; CART RAM (the FujiNet cart's, 1K, protocol 2): read like ROM, by any code
;   (RAM-L directly from page 0; RAM-H through pointer words), written ONLY
;   through the mailbox: RAMPUT (CPUT/CPUTR, ~100 clocks, closes a stream)
;   or a stream (CPTR then RAMPUT1, ~39 clocks a byte; the link streams
;   patches into ST and segments into SCRIPT). So it holds what is read
;   often but written seldom, or written in runs.
;     RAM-L    $0600-$07FF  512   ST (the STATE, 378), then the link's and the
;                                 UI's slow variables, then reserves
;     RAM-H    $2900-$2AFF  512   SCRIPT (160), then the shot's reserve
;   A RESET restarts the 2650 but not the cart: START zeroes all 1K.
;
; THE LEDGER (M4, 2026-11-24; tools/arc/checkram.py prints it from the
; listing at every build and fails on an overflow; VAR fails the assembly):
;   zone A   32 of 32 used
;   island    4 of 4
;   zone C   48 of 48 used (M2: the column's GMT, GCUR, GPN; our aim MYTURN
;            AIMA AIMPH AIMPL WEAPON SENTT HELDN; FDIRTY; SPRSEL 4; M3:
;            SHAKEB, the shake's VOLUME bits)
;   TZ       80 of 208 used: the boot's and menus' 13, every text screen's
;            15 (N32, DIG, REPN), then one screen's at a time (TZSCR: the
;            room's W* 24, the shop's SH* 52); 128 free
;   RAM-L   378 ST + 85 variables + C3N 1 + CELL3 48 = 512 of 512
;   RAM-H   160 SCRIPT + the shot's 352 = 512 of 512: blasts 10x8, the
;            player's counters 12, the HUD's message 4 (TALK*: M4, the old
;            pad), tracks 6x21, settles 4x5, paths 4x7, flows 4x10, beams
;            2x5 (M4: AIMV, the field's UI, overlays them), TMPW 25 (an
;            ellipse's widths), TKROWS 3; the test scene's TMARK TLOCK
;            STEPC 4 (not the game's: free to take)
; Free: console 0, cart 0 (+ the test scene's 4). M4's text screens (the
; room's settings, the shop, the results) keep their state in TZ (TZFREE);
; the field's UI overlays the shot's records (AIMV) and the old pad (TALK*).
;
; OVERLAYS (tools/arc/checkram.py proves them): two names on the same
; bytes must never be live in the same phase. Each is declared with the
; phases it is live in (PB_*), and checkram fails if two that share a byte
; share a phase:
;   PB_TEXT  a text screen shows (TZ is its own)
;   PB_AIM   the field, no shot under way (FL_INSCR clear, no segment):
;            aiming, the defence window, the others aiming
;   PB_SEG   a held segment plays (PLAYSEG: the decoder idle)
;   PB_SHOT  a shot under way: its first segment to its last (the waits
;            between included)
;   PB_LINK  the decoder runs (every frame but PB_SEG's)
;   CELL3    48 the field's 3-colour cells (HOT, sky and dirt in one: the
;               display keeps the HOT blocks and the majority ground), their
;               true dirt kept to come back when the fire goes (24 x 2 B:
;               the index, the half | the dirt; the study saw <= 11)
; The decoder's working set (DLEFT..GPN, 23 B of zone C: PLYSCRN) is idle
; while a held segment plays (CAP_HOLD: the server sends nothing until our
; D, and the decoder stands at a frame boundary): the player uses it as
; scratch then (PLYSCR) and zeroes it after (the decoder's idle state).

VAR     MACRO   name,n
name    EQU     RAMNEXT
RAMNEXT SET     RAMNEXT+(n)
        IF      RAMNEXT > RAMEND
        ERROR   "RAM zone full"
        ENDIF
        ENDM

ZONE    MACRO   lo,hi
RAMNEXT SET     lo
RAMEND  SET     hi
        ENDM

; OVERLAY base,n,phases: bytes base..base+n-1 are live in these phases
; (PB_*): tools/arc/checkram.py reads OVLk_LO/_N/_PH from the listing and
; fails if two declared ranges share a byte and a phase.
PB_MENU EQU     $01             ; the boot, the menus, joining, leaving (TZ's own)
PB_ROOM EQU     $02             ; the waiting room (a text screen)
PB_SHOP EQU     $04             ; the shop (a text screen)
PB_RES  EQU     $08             ; a round's results, the game's (a text screen)
PB_AIM  EQU     $10             ; the field, no shot under way
PB_SEG  EQU     $20             ; a held segment plays (the decoder idle)
PB_SHOT EQU     $40             ; a shot under way (its segments and the waits)
PB_LINK EQU     $80             ; the decoder runs (all but PB_SEG)
OVN     SET     0
OVERLAY MACRO   base,n,ph
OVL{"\{OVN}"}_LO EQU base
OVL{"\{OVN}"}_N EQU n
OVL{"\{OVN}"}_PH EQU ph
OVN     SET     OVN+1
        ENDM

; ---- zone A: time, input, the display's pointers, scratch, the link's hottest --
ZA_LO   EQU     $18D0
ZA_HI   EQU     $18F0
        ZONE    ZA_LO,ZA_HI
        VAR     TICKH,1         ; the cart's tick at the last frame
        VAR     TICKL,1
        VAR     FRMH,1          ; our frame clock (jiffies), big-endian
        VAR     FRML,1
        VAR     DT,1            ; jiffies since the loop last came round (1-8)
        VAR     CURPG,1         ; the page in the window
        VAR     PGSAVE,1        ; PGFAR: the page to give back
        VAR     AXSEL,1         ; the axis the pot reads next vblank (0 vertical)
        VAR     AXV,1           ; the disc, both axes as last read
        VAR     AXH,1
        VAR     INDIR,1         ; directions held (D_*)
        VAR     PRVDIR,1        ; last frame's (a push is an edge)
        VAR     INBTN,1         ; FIRE held
        VAR     EVBTN,1         ; FIRE went down this frame
        VAR     EVKEY,1         ; a key went down this frame (K_*, digits)
        VAR     KRAW,1          ; last frame's key (edges)
        VAR     DROWH,1         ; the display row's base (DAT, DPOS)
        VAR     DROWL,1
        VAR     DCOLOR,1        ; ink bits for DPUTC
        VAR     DSTRH,1         ; the string DTXT reads
        VAR     DSTRL,1
        VAR     T0,1            ; scratch: never across a call that says so
        VAR     T1,1
        VAR     T2,1            ; (T2, T3: MGO's frame counts too)
        VAR     T3,1
        VAR     NSEQ,1          ; the SEQ ACKSEQ echoes when it is done
        VAR     NKIND,1         ; NK_*: the link's transaction out (0 none)
        VAR     NWAIT,1         ; frames it has been out
        VAR     NLINK,1         ; NL_*
        VAR     NEVT,1          ; EV_*: what this frame's step brought
        VAR     TXF,1           ; TX_*: frames waiting to be sent
        VAR     TAKEN,1         ; bytes taken from the link (mod 256)
ZA_END  EQU     RAMNEXT

; ---- the island ($18F0-$18F7 are the sprite registers) -------------------------
ZI_LO   EQU     $18F8
ZI_HI   EQU     $18FC
        ZONE    ZI_LO,ZI_HI
        VAR     SNDPH,1         ; the sound's next note (big-endian, in its page:
        VAR     SNDPL,1         ;  b7 the title's; 0 none plays)
        VAR     SNDCNT,1        ; the note's jiffies left (signed: owed past 0)
        VAR     SNDPRI,1        ; the sound's priority (SNDQUE)
ZI_END  EQU     RAMNEXT

; ---- zone C: the link's decoder and clocks (p_net.inc) ---------------------------
ZC_LO   EQU     $1AD0
ZC_HI   EQU     $1B00
        ZONE    ZC_LO,ZC_HI
        VAR     CREDITED,1      ; TAKEN when the server was last told
        VAR     DLEFT,1         ; the READ's bytes not yet decoded
        VAR     DRPOS,1         ; the reply's offset of the last one decoded
        VAR     DCNT,1          ; this chunk's bytes left
        VAR     DCODE,1         ; the COBS block's data bytes left
        VAR     DFLAG,1         ; DF_*
        VAR     D1,1            ; the two decoded bytes held back (the sums at
        VAR     D2,1            ;  a frame's end)
        VAR     DN,1            ; how many are held (0-2)
        VAR     FIDX,1          ; the frame's byte: 0 its type, then payload + 2
        VAR     FTYPE,1
        VAR     S1,1            ; the running Fletcher sums
        VAR     S2,1
        VAR     DOUT,1          ; the frame's byte being taken
        VAR     FR,7            ; a frame's own (one frame at a time: below)
        VAR     GMT,1           ; G: the column's top-row block bit (4, 2, 1:
                                ;  its third of the cell; the bottom row's is x8)
        VAR     GCUR,1          ; G: the column's row reached (spans)
        VAR     GPN,1           ; G: the cell column's columns waiting (0-2;
                                ;  $FF none can: p_fld.inc PTCOL)
DECEND  EQU     RAMNEXT
        VAR     SEGLEN,1        ; the segment so far (bytes in SCRIPT)
        VAR     SEGRDY,1        ;  whole, to be played
        VAR     QUIETH,1        ; frames since a good frame
        VAR     QUIETL,1
        VAR     QASK,1          ; the same, mod ASK_AGAIN (HELLO again, D again)
        VAR     RHOLD,1         ; b0-6 frames to the next RESYNC, b7 one owed
        VAR     NBWL,1          ; bytes a STATUS said are waiting, not yet read
        VAR     NBWH,1
        VAR     NUMV,1          ; DNUM's value
        VAR     NUMD,1          ;  and a place's digit
        VAR     VSEEN,1         ; the display seen since the last vblank's work
; the field's (p_play.inc): our aim, the sprites
        VAR     MYTURN,1        ; we aim now
        VAR     AIMA,1          ; our aim: the angle, the power (16, big-endian),
        VAR     AIMPH,1         ;  the weapon
        VAR     AIMPL,1
        VAR     WEAPON,1
        VAR     SENTT,1         ; FRML when our aim last went (ACT_AIM)
        VAR     HELDN,1         ; frames the disc has been held (the step)
        VAR     FDIRTY,1        ; FD_*: what to draw again
        VAR     SPRSEL,4        ; the seat each sprite shows (NONE: off)
        VAR     SHAKEB,1        ; VOLUME's b5-7 (the picture's shift: OP_SHAKE),
                                ;  ORed in by every VOLUME write
ZC_END  EQU     RAMNEXT
ZCFREE  EQU     RAMNEXT
; a frame's own bytes, by type (one frame decodes at a time)
QST     EQU     FR              ; Q: a run: 0-1 its offset, 2 its length, 3 its bytes,
QOFFL   EQU     FR+1            ;  4 a bad run; the offset (16), the bytes left
QOFFH   EQU     FR+2
QLEN    EQU     FR+3
GST     EQU     FR              ; G: a column entry's state, the column, the spans
GX      EQU     FR+1            ;  left, a span's top (M2)
GSP     EQU     FR+2
GTOP    EQU     FR+3
SSEG    EQU     FR+1            ; S: the segment, its offset (a buffer < 256)
SOFF    EQU     FR+2
GNEW    EQU     FR+4            ; G: the new version; S: the shot
GFLG    EQU     FR+5            ; G, S: the flags
GSKIP   EQU     FR+6            ; G, S: not ours (asked again at its end)
                                ; (G: GX the cell column, GST the entry's byte:
                                ;  0 its first, 1 a span's top, 2 its end, $FF
                                ;  the columns dropped -- not ours, or no field
                                ;  showing; GSP spans left, GTOP a span's top)
; E: its code in QOFFL; T: the index in QLEN ($FF out of range), the count in QOFFL
PLYSCR  EQU     DLEFT           ; the player's scratch while a held segment
PLYSCRN EQU     DECEND-DLEFT    ;  plays: the decoder idle (see above); the
                                ;  player zeroes it at the end (the decoder's
                                ;  idle state)
; the player's (p_ply.inc, p_ops.inc, blk.inc): PLYSCR's 23 bytes
PPC     EQU     PLYSCR          ; the script's pc (the op's next byte)
PFLG    EQU     PLYSCR+1        ; PF_*
BX      EQU     PLYSCR+2        ; a block: x, y (0-47)
BY      EQU     PLYSCR+3
BIDX    EQU     PLYSCR+4        ;  its cell: the index in its half, the half
BHALF   EQU     PLYSCR+5        ;  (0 SCR, 1 SCR2)
BMSK    EQU     PLYSCR+6        ;  the blocks of it worked on
BH      EQU     PLYSCR+7        ; the cell decoded: its HOT blocks, its dirt ones
BD      EQU     PLYSCR+8        ;  (the rest sky), its CELL3 entry (2k; $FF none)
BC3     EQU     PLYSCR+9
BFROM   EQU     PLYSCR+10       ; what changes (FR_*), into what (PX*)
BTO     EQU     PLYSCR+11
SX0     EQU     PLYSCR+12       ; a span's ends (signed)
SX1     EQU     PLYSCR+13
EX      EQU     PLYSCR+14       ; the ellipse: the row's half width, the row,
EY      EQU     PLYSCR+15
ERX     EQU     PLYSCR+16       ;  the radii walked
ERY     EQU     PLYSCR+17
ECX     EQU     PLYSCR+18       ;  the centre (signed, -32..95)
ECY     EQU     PLYSCR+19
ERYO    EQU     PLYSCR+20       ;  the old ellipse's ry ($FF: none -- a ring's inside)
EPASS   EQU     PLYSCR+21       ;  0 the old one's widths kept, 1 the ring drawn
PTMP    EQU     PLYSCR+22       ; an op's scratch
        IF      PTMP-PLYSCR+1 > PLYSCRN
        ERROR   "the player's scratch runs past the decoder's bytes"
        ENDIF
        OVERLAY DLEFT,PLYSCRN,PB_LINK   ; the decoder's working set
        OVERLAY PLYSCR,PLYSCRN,PB_SEG   ;  the player's scratch on it
; and while the player's ellipse runs (nothing drawn on the HUD meanwhile):
EFL     EQU     T0              ; F, 16-bit (lo, hi): the row's slack
EFH     EQU     T1
EDRL    EQU     T2              ; dR: F's drop to the next row
EDRH    EQU     T3
EDLL    EQU     NUMV            ; dL: F's rise as x goes in
EDLH    EQU     NUMD
E2AL    EQU     DROWH           ; 2 rx^2, 2 ry^2
E2AH    EQU     DROWL
E2BL    EQU     DCOLOR
E2BH    EQU     DSTRH
PF_TK   EQU     $01             ; PFLG: the block tanks were taken off (drawn again)
PF_BEHIND EQU   $02             ;  this pass catches up 8 steps (the growth undrawn)
FR_SKY  EQU     1               ; BFROM: the kinds that change
FR_DIRT EQU     2
FR_FX   EQU     4
PXSKY   EQU     0               ; BTO, and a block's kind (view.go's Px*)
PXDIRT  EQU     1
PXFX    EQU     2

; ---- TZ: a text screen's own (dead on the field) ---------------------------------
TZ_LO   EQU     $1A00
TZ_HI   EQU     $1AD0
        ZONE    TZ_LO,TZ_HI
        VAR     TESTHK,4        ; the rigs' test hook (the boot's 20 frames)
        VAR     MSGID,1         ; the boot page's message
        VAR     FAILRM,1        ;  and E_FAIL's: the room forgotten too
        VAR     RCOUNT,1        ; a count for a loop around calls
        VAR     JOINTH,1        ; frames waiting for the first state (the JOINING
        VAR     JOINTL,1        ;  message's; the room list's wait)
        VAR     ROOMN,1         ; T frames: the count ($FF: none yet)
        VAR     ROOMHAVE,1      ;  and the records in (a bit each)
        VAR     SEL,1           ; a menu's choice
        VAR     NSEL,1          ;  of how many
        OVERLAY MSGID,RAMNEXT-MSGID,PB_MENU
; every text screen's (p_room.inc, p_shop.inc: numbers, the disc's repeat)
        VAR     N32,4           ; a number (32-bit, little-endian) to draw
        VAR     DIG,10          ;  its digits (NUMX)
        VAR     REPN,1          ; frames the disc has been held (the repeat)
        OVERLAY N32,15,PB_ROOM|PB_SHOP|PB_RES
TZSCR   EQU     RAMNEXT         ; the screens' own, from here (one at a time)
; the waiting room's (p_room.inc)
WSET    EQU     TZSCR           ; the settings shown (else the seats)
WSEL    EQU     TZSCR+1         ;  the one chosen (in WKS)
WPAGE   EQU     TZSCR+2         ;  their page
WTOP    EQU     TZSCR+3         ;  the first shown
WN      EQU     TZSCR+4         ;  how many on the page
WKS     EQU     TZSCR+5         ;  the page's settings (16 at most)
WI      EQU     TZSCR+21        ; a row being drawn (the seats', the settings')
WMV     EQU     TZSCR+22        ; this frame's disc (REP)
WOLD    EQU     TZSCR+23        ; the choice before it
        OVERLAY TZSCR,24,PB_ROOM
; the shop's (p_shop.inc)
SHSEL   EQU     TZSCR           ; the one chosen (in SHIDS)
SHTOP   EQU     TZSCR+1         ;  the first shown
SHN     EQU     TZSCR+2         ;  how many
SHMSG   EQU     TZSCR+3         ;  row 12's message (0: the hint)
SHIDS   EQU     TZSCR+4         ;  the list: the items the arms level allows
        OVERLAY TZSCR,4+N_ITEMS,PB_SHOP
RAMNEXT SET     TZSCR+4+N_ITEMS
TZ_END  EQU     RAMNEXT
TZFREE  EQU     RAMNEXT         ; the rest: free
TZFREEN EQU     TZ_HI-RAMNEXT

; ---- cart RAM-L: ST, the slow variables, reserves ------------------------------
ST      EQU     $0600           ; the state (proto.h's layout): ST + OFF_*
ROOMS   EQU     ST              ; the room list lives in ST before the state is
                                ;  live (MAX_ROOMS records of ROOM_REC)
CL_LO   EQU     ST+STATE_SIZE
CL_HI   EQU     $0800
        ZONE    CL_LO,CL_HI
        VAR     MYNAME,NAME_LEN+1       ; ASCII, NUL-padded (key 0)
        VAR     SRVTBL,ROOM_LEN+1       ; the room's id, ASCII, NUL-padded (key 0x13)
        VAR     ASEQ,1          ; the action: seq, kind, 6 args (its frame's payload)
        VAR     AK,1
        VAR     AARGS,6
        VAR     ANARG,1         ;  and its args' count
        VAR     NERR,1          ; an E frame's code
        VAR     NBAD,1          ; the rigs': bad frames
        VAR     NTV,1           ; the field's version (FF: not to be trusted, FE: coming)
        VAR     NTERR,1         ; terrain came since the field looked
        VAR     SEGSID,1        ; the segment being received: its shot, its number,
        VAR     SEGNO,1         ;  its flags
        VAR     SEGFLG,1
        VAR     DSID,1          ; our D: what it said (again if unanswered)
        VAR     DSEG,1
        VAR     FLAGS,1         ; FL_*
        VAR     BGCB,1          ; BGCOL without the pot mux
        VAR     MYSEAT,1        ; NONE: watching (ST's OFF_MY_SEAT at each commit)
        VAR     SHOWN,1         ; S_*: the screen in the room
        VAR     SHSEQ,1         ; OFF_SEQ when it was entered
        VAR     NSEGS,1         ; the rigs': segments played (mod 256)
; the tanks as drawn (p_play.inc; M3's ops move them): x ($FF: none), y
; (b7: drawn as a 2x1 block tank, not a sprite), the barrel's angle; the
; block tanks' cells as they were under them (TKOFF puts them back, the
; last drawn first): 2 a tank, in the order drawn
        VAR     TKX,SEATS
        VAR     TKY,SEATS
        VAR     TKA,SEATS
        VAR     TBN,1           ; block tanks drawn (their cells in TBC)
        VAR     TBC,2*(SEATS-4) ; (at most 6: 4 tanks are sprites)
CLVARS  EQU     RAMNEXT-CL_LO
        VAR     C3N,1           ; CELL3's entries in use
        VAR     CELL3,48        ; the 3-colour cells (HOT, sky and dirt): 2 bytes
                                ;  each -- the cell's index, its half (b7) | its
                                ;  dirt blocks -- as many as C3N, packed
CL_END  EQU     RAMNEXT

; ---- cart RAM-H: SCRIPT, the shot's reserve --------------------------------------
CH_LO   EQU     $2900
CH_HI   EQU     $2B00
        ZONE    CH_LO,CH_HI
        VAR     SCRIPT,SCRIPT_BUF       ; a segment (the stream's: cart offset $200)
; the shot's state (p_ply.inc, p_ops.inc), SHOTRES to SHOTEND zeroed at a
; shot's start: the blasts and the player's counters in $29A0-$29FF, the
; tracks and the rest from $2A00 (each table within one 256 of cart RAM:
; CPUTR indexes them)
SHOTRES EQU     RAMNEXT
NTRACK  EQU     6               ; the server caps the Arcadia at 6 (project.PlatSlots)
TKSZ    EQU     21              ; a track: TK_* (p_ply.inc)
NBLAST  EQU     10              ; blasts and dirt balls at once (more: dropped)
BLSZ    EQU     8               ; kind, n, tot (0: free), rx, ry, dn, x, y
NSETTLE EQU     4
STSZ    EQU     5               ; left, n (0: free), x16, at (its entries' pc)
NPATH   EQU     4
PASZ    EQU     7               ; on, style, x, y, n, drawn, its next step (pc)
NFLOW   EQU     4
FLSZ    EQU     10              ; on, kind, frames, f, hold, n, shown, acc16, cells (pc)
NBEAM   EQU     2
BESZ    EQU     5               ; frames left (0: off), x0, y0, x1, y1
        VAR     BLASTS,NBLAST*BLSZ
        OVERLAY BLASTS,NBLAST*BLSZ,PB_SHOT|PB_SEG
        VAR     PWAITL,1        ; frames to wait (OP_WAIT), 16-bit
        VAR     PWAITH,1
        VAR     PDONE,1         ; the segment's OP_END reached
        VAR     PLAST,2         ; the cart's tick the timeline has reached (lo, hi)
        VAR     PSTEPS,1        ; this pass's steps left
        VAR     PBLK,1          ; the blast being worked on (its offset)
        VAR     FLASHN,1        ; OP_FLASH's frames left
        VAR     SHAKEN,1        ; OP_SHAKE's
        VAR     ERXN,1          ; KELL's new radii (blk.inc)
        VAR     ERYN,1
; the HUD's message (p_hud.inc; M4, the 4 bytes that were a pad): a talk
; (OP_TALK: "NAME text" typed a letter a step over both HUD rows) or an
; event line (OP_DAMAGE, OP_SHIELD over row 24); while TALKT runs the HUD
; isn't drawn. Zeroed with the shot's state at a shot's start (PLYRESET);
; TALKT outlives the shot (the main loop counts it down after).
        VAR     TALKT,1         ; the message stays: eighths of a second (0: none)
        VAR     TALKP,1         ; the talk's next letter (its offset in SCRIPT)
        VAR     TALKN,1         ;  letters left to type (0: all shown; flushed at
                                ;  the segment's end: SCRIPT's bytes go)
        VAR     TALKC,1         ;  the cell it types into (0-29: row 24's 15, 25's)
        IF      RAMNEXT > $2A00
        ERROR   "the shot's first part runs past $2A00"
        ENDIF
RAMNEXT SET     $2A00
        VAR     TRACKS,NTRACK*TKSZ
        VAR     SETTLES,NSETTLE*STSZ
        VAR     PATHS,NPATH*PASZ
        VAR     FLOWS,NFLOW*FLSZ
        VAR     BEAMS,NBEAM*BESZ
SHOTEND EQU     RAMNEXT
        OVERLAY BEAMS,NBEAM*BESZ,PB_SHOT|PB_SEG
; AIMV: the field's UI while no script plays (p_hud.inc, p_play.inc): the
; items line, the guidance's target, the turn's banner and reminder, the
; defence window. OVERLAYS BEAMS: live only in PB_AIM (the field with no
; shot under way: our aim, the defence window, waiting for the others),
; when the shot's records are dead. PLYRESET zeroes it at a shot's start
; (the items line shut by the shot), PSEND at a script's last segment and
; PENTER on a new field (the records' leftovers are not ours).
AIMV    EQU     BEAMS
IMENU   EQU     AIMV            ; the items line: M_* (0 shut)
ITEM    EQU     AIMV+1          ;  the item it shows
GUIDE   EQU     AIMV+2          ; our next shot's guidance (I_NONE) and target
TARGET  EQU     AIMV+3          ;  (NO_SEAT): ACT_FIRE's last two
DRIVEN  EQU     AIMV+4          ; jiffies to the next ACT_DRIVE while the disc drives
TURNJ   EQU     AIMV+5          ; YOUR TURN!'s frames left (the banner)
IDLEL   EQU     AIMV+6          ; frames our turn has gone with no input (the
IDLEH   EQU     AIMV+7          ;  reminder at REMINDJ)
DEFOPN  EQU     AIMV+8          ; the defence window seen (its line opened)
AIMVN   EQU     9
        OVERLAY AIMV,AIMVN,PB_AIM
        IF      AIMVN > NBEAM*BESZ
        ERROR   "AIMV runs past BEAMS"
        ENDIF
M_ITEM  EQU     1               ; IMENU: the items
M_TARGET EQU    2               ;  the guidance's target
M_DRIVE EQU     3               ;  driving (fuel)
EMAXR   EQU     24              ; the radii an ellipse walks at most (TMPW's rows - 1)
        VAR     TMPW,EMAXR+1    ; an ellipse's half widths (the old one's, a ring)
        VAR     TKROWS,3        ; the block tanks' cell rows (a bit each: p_fld.inc
                                ;  TKDRAW; not the shot's: it outlives one)
        VAR     TMARK,1         ; the test scene's mark (98 the shot starts, 99 ends)
        VAR     TLOCK,1         ;  its lockstep (a step a pass: its argument 1)
        VAR     STEPC,2         ; the rigs': the player's steps (16-bit)
CH_END  EQU     RAMNEXT

; ---- the checks the code relies on ------------------------------------------------
        IF      ST+STATE_SIZE > CL_LO
        ERROR   "ST runs into the variables"
        ENDIF
        IF      SCRIPT <> $2900
        ERROR   "SCRIPT must start RAM-H: the decoder aims at cart offset $200 + SEGLEN"
        ENDIF
        IF      ((MYNAME-$0600)&$FF)+NAME_LEN > $FF
        ERROR   "MYNAME crosses a 256: CPUTR can't index it"
        ENDIF
        IF      ((TKX-$0600)&$FF)+3*SEATS+1+2*(SEATS-4) > $100
        ERROR   "TKX..TBC cross a 256: CPUTR can't index them"
        ENDIF
        IF      (TKY <> TKX+SEATS) || (TKA <> TKY+SEATS)
        ERROR   "TKX, TKY, TKA must be one run (TKSTATE streams them)"
        ENDIF
        IF      ((ROOM_REC*MAX_ROOMS) > STATE_SIZE)
        ERROR   "the room list doesn't fit in ST"
        ENDIF
        IF      ((CELL3-$0600)&$FF)+47 > $FF
        ERROR   "CELL3 crosses a 256: CPUTR can't index it"
        ENDIF
