; ---------------------------------------------------------------------------
; barrage.asm -- FujiBarrage for the Fairchild Channel F, over FujiNet.
;
; A hand-written F8 client: the link and the console's idioms from
; FujiBlast's src/channelf (after M.U.L.E.'s clients/channelf), the game
; from the other FujiBarrage asm ports (the Astrocade's and the Arcadia's),
; the shot player the 6502's (src/6502 ply.s, trk.s, ops.s and the
; Atari's fld.s), op for op. The screen (src/disp.inc) writes a pixel only
; when it changes.
;
; THE CART. fujinet-firmware pico/channelf at protocol 2: the fixed 16K at
; $0800-$47FF, and one 8K page at a time at $4800-$67FF, chosen by a store
; to $FD80+n (APPBANK); a 60 Hz tick at $FC0E; 30K of RAM at $8000. The
; image is the 16K and six pages: the fixed block is assembled at 0
; PHASEd to $0800, page n at $4000 + n * $2000 PHASEd to where it runs:
; $4800, or RAM for the two START copies there (the player: PLYRAM; the
; sounds: DATA), so p2bin's 0-$FFFF is the image. Only SELPG and PGPUSH
; store to $FD80 (tools/chf/checkcalls.py). Code in a page never calls
; another page; the player (RAM) and fixed code may call any page (FAR).
; ---------------------------------------------------------------------------
        CPU F3850

        INCLUDE "srcdir/src/equ.inc"
        INCLUDE "srcdir/src/macros.inc"
        INCLUDE "flags.inc"
        INCLUDE "proto.inc"
SCRIPT_BUF EQU 1536             ; the server's segments (client.ChannelF Buf)
MAX_ROOMS  EQU 8
TABLE_LEN  EQU 8
CAT_MAX    EQU 1664             ; the CATALOG's bytes at most (src/game.h)
        INCLUDE "srcdir/src/ram.inc"

; ---- pages --------------------------------------------------------------
PG_BOOT  EQU 0                  ; the start, the appkeys, the socket, messages,
                                ;  the title, the name, the rooms
PG_PLY   EQU 1                  ; the shot player: copied to PLYRAM, run there
PG_HUD   EQU 2                  ; the field's rows of text: aiming, items, talk
PG_SCR   EQU 3                  ; the waiting room, settings, shop, results
PG_DATA  EQU 4                  ; the sounds: copied to DATA
PG_TEST  EQU 5                  ; the rigs' offline scenes
NPAGES   EQU 6

; PAGEHDR -- a page's first bytes: its id twice (the second complemented),
; then its entries (E_*).
PAGEHDR MACRO id, e1, e2, e3, e4
        DB id, (~(id))&0FFH
        JMP e1
        JMP e2
        JMP e3
        JMP e4
        ENDM
E_ENTER EQU 2
E_STATE EQU 5
E_TICK  EQU 8
E_EXTRA EQU 11

; =======================================================================
; the fixed 16K: $0800-$47FB
; =======================================================================
        ORG 0
        PHASE 0800H
MB_FIX:
        DB 055H                 ; the BIOS's cart signature
        DB 000H                 ; ($0801 is skipped)
        JMP START               ; $0802

MB_START:
        INCLUDE "srcdir/src/start.inc"
MB_MAIN:
        INCLUDE "srcdir/src/main.inc"
MB_MAIL:
        INCLUDE "srcdir/src/mail.inc"
MB_NET:
        INCLUDE "srcdir/src/net.inc"
MB_DISP:
        INCLUDE "srcdir/src/disp.inc"
MB_FLD:
        INCLUDE "srcdir/src/fld.inc"
MB_TANK:
        INCLUDE "srcdir/src/tank.inc"
MB_SKY:
        INCLUDE "srcdir/src/sky.inc"
MB_CAT:
        INCLUDE "srcdir/src/cat.inc"
MB_NUM:
        INCLUDE "srcdir/src/num.inc"
MB_INPUT:
        INCLUDE "srcdir/src/input.inc"
MB_SOUND:
        INCLUDE "srcdir/src/sound.inc"
MB_HUD:
        INCLUDE "srcdir/src/hud.inc"
MB_FONT:
        INCLUDE "font.inc"
MB_FIXEND:
        IF $ > 047FCH
        ERROR "the fixed block runs into the claim"
        ENDIF
        DEPHASE

; the claim: the cart keeps the mailbox alive for an image that has it
        ORG 03FFCH
        DB "FUJI"

; =======================================================================
; the pages: $4800-$67FF
; =======================================================================
PAGE    MACRO n
        ORG 04000H+(n)*02000H
        PHASE PGWIN
        ENDM

        PAGE PG_BOOT
MB_P0:  PAGEHDR PG_BOOT, BENTER, TLSTART, TLTICK, MENTER
        JMP SAVENM              ; E_SAVENM
        JMP CONN                ; E_CONN
        JMP JOIN                ; E_JOIN
        JMP SCLOSE              ; E_CLOSE
        JMP CLRROOM             ; E_CLRRM
        JMP MSG                 ; E_MSG
        JMP FAIL                ; E_FAIL
        INCLUDE "srcdir/src/p_boot.inc"
MB_P0END:
        DEPHASE

; The player is assembled where START copies it: PLYRAM.
        ORG 04000H+PG_PLY*02000H
        PHASE PLYRAM
MB_P1:  DB PG_PLY, (~PG_PLY)&0FFH
        INCLUDE "srcdir/src/p_ply.inc"
MB_P1END:
        IF $-PLYRAM > PGSIZE
        ERROR "the player is over 8K"
        ENDIF
        DEPHASE

        PAGE PG_HUD
MB_P2:  PAGEHDR PG_HUD, HENTER, HSTATE, HTICK, STUB
        INCLUDE "srcdir/src/p_hud.inc"
MB_P2END:
        DEPHASE

        PAGE PG_SCR
MB_P3:  PAGEHDR PG_SCR, SENTER, SSTATE, STICK, STUB
        INCLUDE "srcdir/src/p_scr.inc"
MB_P3END:
        DEPHASE

; The sounds are assembled where START copies them: DATA.
        ORG 04000H+PG_DATA*02000H
        PHASE DATA
MB_P4:  DB PG_DATA, (~PG_DATA)&0FFH
        INCLUDE "snd.inc"
MB_P4END:
DATASZ  EQU ((MB_P4END-DATA+255)/256)*256
        IF DATASZ > 0800H
        ERROR "the sounds are over 2K"
        ENDIF
        DEPHASE

        PAGE PG_TEST
MB_P5:  PAGEHDR PG_TEST, TENTER, STUB, STUB, STUB
        INCLUDE "srcdir/src/p_test.inc"
MB_P5END:
        DEPHASE

        ORG 0FFFFH
        DB 0FFH                 ; (p2bin: the image is all 64K)
        END
