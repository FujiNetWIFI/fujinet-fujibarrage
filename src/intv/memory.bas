' ===========================================================================
' memory.bas -- the cartridge RAM map, colours, and the variables and
' helpers every file shares. IntyBASIC is single-pass: a CONST, DIM or
' DEF FN comes before its first use, so this file is included first.
'
' IntyBASIC has about 220 8-bit and 50 16-bit variables and 24 words of
' stack shared with the frame handler. Everything tabular lives in the
' FujiNet cartridge's RAM, $8000-$9BFF, asked for 16 bits wide (fujinet.bas):
' a byte of a document is a word here (its upper bits 0), the catalog two
' bytes a word, the field's planes the GRAM's layout (4 words a cell, an
' even row in the low byte). PEEK/POKE from BASIC, the asm directly. A
' console reset leaves it as it was: boot clears what must start clear.
' ===========================================================================

    ' ---- cartridge RAM (16-bit words) ------------------------------------------
    CONST ST       = $8000      ' 378: the state (STATE_SIZE), patched as it comes
    CONST NBUF     = $8180      ' 136: the frame decoded (type, payload, sums),
    CONST NBUF_END = $8208      ' and 256 words of slack after, for a garbled one
    CONST TXQ      = $8308      ' 64: encoded frames waiting to go out
    CONST RAWB     = $8348      ' 40: a frame being encoded: type, payload
    CONST ROOMS    = $8370      ' 192: the room list, ROOM_REC a room
    CONST AKBUF    = $8430      ' 72: an AppKey read or written
    CONST MYNAME   = $8478      ' 9: our name, 0 ended
    CONST SRVTBL   = $8481      ' 9: the room, 0 ended
    CONST SRVHOST  = $848A      ' 64: the server's host, 0 ended
    CONST STXT     = $84CA      ' 48: text built for the screen, 0 ended
    CONST CATALOG  = $8500      ' 832: the catalog, two bytes a word (CAT_MAX bytes)
    CONST CAT_MAX  = 1664
    CONST CATIDX   = $8840      ' 88: where its items, settings, pages, tiers start (cat.bas)
    CONST SCRIPT   = $88A0      ' 1024: a script segment, a byte a word
    CONST SCRIPT_BUF = 1024
    CONST DIRT     = $8CA0      ' 800: the field's dirt, 200 cells x 4 words
    CONST FXP      = $8FC0      ' 800: its fire (the effects' colour), the same
    CONST CELL     = $92E0      ' 200: each cell's card and class (cell.asm)
    CONST DIRTYQ   = $93A8      ' 208: the cells to draw again (cell.asm)
    CONST POOLM    = $9478      ' 424: the GRAM pool (cell.asm)
    CONST GRAMQ    = $9620      ' 192: cards and BACKTAB words for the frame handler (cell.asm)
    CONST TRACKS   = $96E0      ' 256: the script's tracks (trk.asm)
    CONST OPSM     = $97E0      ' 266: its blasts, settles, paths, flows, beams, the player (below)
    CONST TANKS    = $98EA      ' 174: the tanks as drawn (play.bas, mob.asm, ply.asm)
    CONST NV_RXP   = $9998      ' the link's (nstrans.bas, DECODE): the bytes read and not decoded,
    CONST NV_RXE   = $9999
    CONST NV_DECP  = $999A      ' the frame being decoded, where this pass stops,
    CONST NV_DECE  = $999B
    CONST NV_S1    = $999C      ' its sums
    CONST NV_S2    = $999D
    CONST DEF_OPEN = $999E      ' the defence window's items line opened (play.bas)
    CONST BAN_ON   = $999F      ' YOUR TURN! flashing on row 1 (play.bas)
    CONST CELLW    = $99A0      ' 200: each field cell's BACKTAB word as last drawn (cell.asm)
    CONST SKYW     = $9A68      ' 200: each cell's word when it is all sky (a star or not)
    CONST FLDV     = $9B30      ' 72: fld.asm's and cell.asm's variables (below)
    CONST MBV      = $9B78      ' 4: mob.asm's: tanks alive, the turn's seat, those with a MOB each,
    CONST MB_CNT   = MBV        ' and the MOBs' words to be worked out again (play.bas sets it)
    CONST MB_T     = MBV + 1
    CONST MB_KEEP  = MBV + 2
    CONST MB_DIRTY = MBV + 3
    CONST MB_ME    = $9B7C      ' our turn: seat + 1 of the tank that blinks (mob.asm), 0 none
    CONST CO_MM    = $9B7D      ' COLUMN's: the column's bits in a word, both rows
    CONST TURN_AT  = $9B7E      ' our turn began (#frame_count), and our last press
    CONST IDLE_AT  = $9B7F
    CONST CSV      = $9B80      ' 48: COLS's (fld.asm):
    CONST CS_P0    = CSV        ' where the entries start, where it is,
    CONST CS_P     = CSV + 1
    CONST CS_X     = CSV + 2    ' the column, the last + 1, this cell column's last + 1,
    CONST CS_END   = CSV + 3
    CONST CS_GE    = CSV + 4
    CONST CS_CM    = CSV + 5    ' its columns built a cell at a time (a bit each, bit 7 the left),
    CONST CS_SLOW  = CSV + 6    ' those with spans (COLUMN's),
    CONST CS_TMIN  = CSV + 7    ' the highest and lowest tops,
    CONST CS_TMAX  = CSV + 8
    CONST CS_CMW   = CSV + 9    ' CM in a word (both rows), and not it,
    CONST CS_NCMW  = CSV + 10
    CONST CS_CELL  = CSV + 11   ' the cell, its first word, its top row,
    CONST CS_W     = CSV + 12
    CONST CS_BASE  = CSV + 13
    CONST CS_CHG   = CSV + 14   ' a change seen
    CONST CS_TOP   = CSV + 16   ' 8: each column's top
    CONST CS_SLOWP = CSV + 24   ' 8: each spans column's entry
    CONST CS_ROW   = CSV + 32   ' 8: the cell's rows (from the columns that start in each, then all)
    CONST CS_NW    = CSV + 40   ' 4: the cell's new words (the columns' bits only)
    CONST LNV      = $9BB0      ' 12: ops.asm's line (a beam): from, to, steps, signs, error, on
    CONST LN_X0    = LNV
    CONST LN_Y0    = LNV + 1
    CONST LN_X1    = LNV + 2
    CONST LN_Y1    = LNV + 3
    CONST LN_DX    = LNV + 4
    CONST LN_DY    = LNV + 5
    CONST LN_SX    = LNV + 6
    CONST LN_SY    = LNV + 7
    CONST LN_ERR   = LNV + 8
    CONST LN_OP    = LNV + 9
    CONST LN_CNT   = LNV + 10
    CONST SHK_X    = $9BBC      ' the screen's shake: the STIC's delays (GRAM_FLUSH writes them)
    CONST SHK_Y    = $9BBD
    CONST SX_P0    = $9BBE      ' sfx.asm's: each channel's next step (B, C),
    CONST SX_P1    = $9BBF
    CONST SX_N0    = $9BC0      ' the jiffies left of its step,
    CONST SX_N1    = $9BC1
    CONST SX_NZ    = $9BC2      ' its noise (bit 0 B, bit 1 C), the noise's period
    CONST SX_NP    = $9BC3
    CONST TALK_TXT = $9BC4      ' 40: a tank's phrase (a copy: the script moves on)
    CONST TALK_N   = $9BEC      ' its length, its seat, the name's length,
    CONST TALK_S   = $9BED
    CONST TALK_L   = $9BEE
    CONST TALK_K   = $9BEF      ' the scroll: the first character in the row

    ' ---- the script player (ply.asm, trk.asm, ops.asm) ------------------------
    ' TRACKS: 16 tracks, 16 words each: on, style, X (lo, hi), Y (lo, hi),
    ' VX (lo, hi), VY (lo, hi), AX, AY, steps left, its pixel x, y
    CONST TR_ON = 0
    CONST TR_ST = 1
    CONST TR_XL = 2
    CONST TR_XH = 3
    CONST TR_YL = 4
    CONST TR_YH = 5
    CONST TR_VXL = 6
    CONST TR_VXH = 7
    CONST TR_VYL = 8
    CONST TR_VYH = 9
    CONST TR_AX = 10
    CONST TR_AY = 11
    CONST TR_N = 12
    CONST TR_PX = 13
    CONST TR_PY = 14
    ' OPSM: the blasts (10), settles (8), paths (4), flows (4), beams (2),
    ' the player's own, the head cards'
    CONST NBLAST   = 10
    CONST B_ON     = OPSM       ' (each NBLAST long)
    CONST B_KIND   = OPSM + 10  ' EX_*, or DIRTBALL
    CONST B_N      = OPSM + 20  ' frames so far, of
    CONST B_TOT    = OPSM + 30
    CONST B_RX     = OPSM + 40
    CONST B_RY     = OPSM + 50
    CONST B_DRX    = OPSM + 60  ' the size last drawn (0: none yet)
    CONST B_X      = OPSM + 70
    CONST B_Y      = OPSM + 80
    CONST B_DRY    = OPSM + 90
    CONST NSETTLE  = 8
    CONST S_ON     = OPSM + 100 ' (each NSETTLE long)
    CONST S_LEFT   = OPSM + 108
    CONST S_N      = OPSM + 116
    CONST S_X      = OPSM + 124
    CONST S_P      = OPSM + 132 ' its columns' entries in SCRIPT
    CONST NPATH    = 4
    CONST PA_ON    = OPSM + 140 ' (each NPATH long)
    CONST PA_ST    = OPSM + 144
    CONST PA_X     = OPSM + 148
    CONST PA_Y     = OPSM + 152
    CONST PA_N     = OPSM + 156
    CONST PA_DR    = OPSM + 160
    CONST PA_P     = OPSM + 164
    CONST NFLOW    = 4
    CONST FL_ON    = OPSM + 168 ' (each NFLOW long)
    CONST FL_KIND  = OPSM + 172
    CONST FL_FRM   = OPSM + 176
    CONST FL_F     = OPSM + 180
    CONST FL_HOLD  = OPSM + 184
    CONST FL_N     = OPSM + 188
    CONST FL_SH    = OPSM + 192
    CONST FL_ACC   = OPSM + 196
    CONST FL_P     = OPSM + 200
    CONST NBEAM    = 2
    CONST BE_LEFT  = OPSM + 204 ' (each NBEAM long)
    CONST BE_X0    = OPSM + 206
    CONST BE_Y0    = OPSM + 208
    CONST BE_X1    = OPSM + 210
    CONST BE_Y1    = OPSM + 212
    CONST PL_P     = OPSM + 214 ' the next op (in SCRIPT), the segment's end,
    CONST PL_END   = OPSM + 215
    CONST PL_WAIT  = OPSM + 216 ' frames to wait, OP_END reached,
    CONST PL_DONE  = OPSM + 217
    CONST PL_FRAME = OPSM + 218 ' frames played in this script,
    CONST PL_EV    = OPSM + 219 ' what play.bas has to do (EV_*),
    CONST PL_NSND  = OPSM + 220 ' sounds to cue (id + size * 256),
    CONST PL_SND   = OPSM + 221 ' (4)
    CONST PL_TALK_S = OPSM + 225 ' a phrase: its seat, text, length,
    CONST PL_TALK_P = OPSM + 226
    CONST PL_TALK_N = OPSM + 227
    CONST PL_SHAKE = OPSM + 228 ' frames of shaking, of flashing, its colour,
    CONST PL_FLASH = OPSM + 229
    CONST PL_FLASHC = OPSM + 230
    CONST PL_FALL  = OPSM + 231 ' the lowest falling shell's row | 1 (the whistle), or 0
    CONST PL_I     = OPSM + 232 ' (the player's scratch)
    CONST PL_J     = OPSM + 233
    CONST PL_K     = OPSM + 234
    CONST HD_N     = OPSM + 235 ' head cards shown, the cell each shows, their patterns
    CONST HD_CELL  = OPSM + 236 ' (6)
    CONST HD_PAT   = OPSM + 242 ' (24: to OPSM + 265)
    CONST EV_HUD   = 1
    CONST EV_TALK  = 2
    CONST EV_FIRE  = 4
    CONST EV_SND   = 8
    CONST DIRTBALL = $80        ' an OP_DIRT, kept with the blasts

    ' fld.asm and cell.asm (FLDV)
    CONST FV_OP    = FLDV + 0   ' the pixel op (PO_*)
    CONST FV_DQH   = FLDV + 1   ' DIRTYQ's head and tail (a ring of 208 cells)
    CONST FV_DQT   = FLDV + 2
    CONST FV_GQH   = FLDV + 3   ' GRAMQ's (192 words, 2 an entry): the frame handler takes from the head
    CONST FV_GQT   = FLDV + 4
    CONST FV_CLOCK = FLDV + 5   ' the pool's next card to look at for a free one
    CONST FV_DIRTW = FLDV + 6   ' an all-dirt cell's word
    CONST FV_DIRTFG = FLDV + 7  ' the dirt as a foreground, and as a background (BACKTAB bits)
    CONST FV_DIRTBG = FLDV + 8
    CONST FV_FIREFG = FLDV + 9  ' the fire's colour now
    CONST FV_NFIRE = FLDV + 10  ' cells with fire in them
    CONST FV_NAPPROX = FLDV + 11 ' cells drawn with a level card (the pool was full)
    CONST FV_RETRY = FLDV + 12  ' a pool card came free: try those again
    CONST RV_N     = FLDV + 13  ' RENDER's: cells left this call, the cell,
    CONST RV_IDX   = FLDV + 14
    CONST RV_D0    = FLDV + 15  ' its dirt (4 words), its fire (4)
    CONST RV_F0    = FLDV + 19
    CONST RV_P0    = FLDV + 23  ' the pattern for the pool (4), its hash and bucket
    CONST RV_P1    = FLDV + 24
    CONST RV_P2    = FLDV + 25
    CONST RV_P3    = FLDV + 26
    CONST RV_H     = FLDV + 27
    CONST RV_B     = FLDV + 28
    CONST RV_W     = FLDV + 29  ' the word, and the cell's new CELL entry
    CONST RV_CELL  = FLDV + 30
    CONST EL_X     = FLDV + 31  ' ELLIPSE's: the centre, radii, the inner's radii (0: none),
    CONST EL_Y     = FLDV + 32
    CONST EL_RX    = FLDV + 33
    CONST EL_RY    = FLDV + 34
    CONST EL_IRX   = FLDV + 35
    CONST EL_IRY   = FLDV + 36
    CONST EL_DY    = FLDV + 37  ' the row, the steps through q32, the half widths
    CONST EL_I     = FLDV + 38
    CONST EL_REM   = FLDV + 39
    CONST EL_I2    = FLDV + 40
    CONST EL_REM2  = FLDV + 41
    CONST EL_HW    = FLDV + 42
    CONST EL_IHW   = FLDV + 43
    CONST EL_ROW   = FLDV + 44
    CONST SP_RM    = FLDV + 45  ' SPAN's: the last cell's mask, a whole cell's, cells left
    CONST SP_FULL  = FLDV + 46
    CONST SP_N     = FLDV + 47
    CONST CO_X     = FLDV + 48  ' COLUMN's: x, its spread bits' row, its first cell and word, the mask's
    CONST CO_EXP   = FLDV + 49  ' complement, a change seen, the cell row
    CONST CO_CELL  = FLDV + 50
    CONST CO_W     = FLDV + 51
    CONST CO_NM    = FLDV + 52
    CONST CO_CHG   = FLDV + 53
    CONST CO_CY    = FLDV + 54
    CONST TR_X     = FLDV + 55  ' TERRAIN's: x, where it is, where the frame ends
    CONST TR_P     = FLDV + 56
    CONST TR_END   = FLDV + 57
    CONST FV_FIREALT = FLDV + 58 ' the fire's colour on dirt of its own colour
    CONST COLB     = FLDV + 59  ' 10: COLUMN's image, a byte a cell row (bit r: its row r)
    CONST RV_K     = FLDV + 69  ' the pool's card in hand
    CONST RV_FAND  = FLDV + 70  ' RCELL's: the fire's words ANDed, and whether any is not 0
    CONST RV_FANY  = FLDV + 71

    ' the pool (cell.asm), in POOLM
    CONST PPAT     = POOLM      ' 176: each card's pattern (4 words)
    CONST PREF     = POOLM + 176 ' 44: the cells showing it
    CONST PHASH    = POOLM + 220 ' 44: its pattern's hash
    CONST PNEXT    = POOLM + 264 ' 44: $8000 + the next card in its bucket + 1 (0: in none)
    CONST PBUCK    = POOLM + 308 ' 64: each bucket's first card + 1 (0: empty)
    CONST PPEND    = POOLM + 372 ' 44: not 0 while its pattern waits in GRAMQ

    ' a CELL entry: its pool card (0: none), what it is (known only when
    ' not queued), queued, fire in it, drawn with a level card
    CONST CF_CARD  = $3F
    CONST CF_CLASS = $C0
    CONST CL_SKY   = $40
    CONST CL_DIRT  = $80
    CONST CF_DIRTY = $100
    CONST CF_FIRE  = $200
    CONST CF_APPROX = $400

    ' the pixel ops (FV_OP)
    CONST PO_SKY   = 0
    CONST PO_DIRT  = 1
    CONST PO_FX    = 2
    CONST PO_AIRFX = 3          ' fire where there is no dirt
    CONST PO_CLRFX = 4          ' fire to sky
    CONST TEST_HOOK = $9BF0     ' 16: the rigs' (input.bas, test.bas)

    ' ---- the field --------------------------------------------------------------
    CONST FIELD_W  = 160
    CONST FIELD_H  = 80
    CONST FIELD_ROW = 2         ' its first screen row (rows 0-1: the HUD)
    CONST TANK_W   = 7
    CONST TANK_H   = 5
    CONST PAR      = 54         ' a pixel's width / height, 64ths
    CONST BANDS    = 8

    ' ---- Intellivision colours ----------------------------------------------
    CONST C_BLACK    = 0
    CONST C_BLUE     = 1
    CONST C_RED      = 2
    CONST C_TAN      = 3
    CONST C_DKGREEN  = 4
    CONST C_GREEN    = 5
    CONST C_YELLOW   = 6
    CONST C_WHITE    = 7
    CONST C_GREY     = 8
    CONST C_CYAN     = 9
    CONST C_ORANGE   = 10
    CONST C_BROWN    = 11
    CONST C_PINK     = 12
    CONST C_LTBLUE   = 13
    CONST C_YELGREEN = 14
    CONST C_PURPLE   = 15

    ' A card's colours in foreground/background mode: foreground 0-7, any
    ' background (bit 13 is background bit 2, bit 12 bit 3: stic.txt).
    DEF FN fgbg(f, b) = ((f) + ((b) AND 3) * $200 + ((b) AND 4) * $800 + ((b) AND 8) * $200)
    ' A cartridge RAM byte (and a mailbox one: their upper bits aren't 0).
    DEF FN rb(a) = (PEEK(a) AND 255)

    CONST NONE = 255

    ' shared between files (IntyBASIC is single-pass: declared before any use)
    DIM my_seat, pr_ret, res_final, s
    DIM offline                 ' a test scene: no link (no polls, no D)
    DIM #srv_port

    ' Main-loop scratch (never the frame handler's)
    DIM i, j, k, n, c, r, t, v
    DIM #a, #v, #w
    UNSIGNED #v, #w
