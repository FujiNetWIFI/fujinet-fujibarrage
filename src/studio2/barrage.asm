; ---------------------------------------------------------------------------
; barrage.asm -- FujiBarrage for the RCA Studio II, over FujiNet.
;
; CDP1802 assembly on the FujiNet cartridge's protocol 2 (fujinet-firmware
; pico/studio2): the cart composes the 64x128 picture from a background,
; 16 sprites and 96 images; the 1802 runs the game and the link and sends
; it small drawing operations (hotspot reads). FujiBlast's src/studio2 is
; the runtime and the link's transactions; the frames, the flow and the
; shot player come from src/channelf, the 16-column screens from
; src/arcadia.
;
; THE FIELD. The model is the server's TERRAIN column entries themselves
; (fld.inc): written only by G frames and OP_SETTLE, exact because every
; change to the dirt reaches the client as a settle. The picture is its
; cache: a column is drawn as a RECT cleared, then one RECT a dirt span.
;
; MEMORY. The runtime at $0400-$07FB ("FUJI" at $07FC); the code at
; $0C00-$9FFF, never in $x000-$x3FF (RFIT/DFIT); the cart's RAM, image
; pages written by POKE, at $A400-$AFFF, $B400-$BFFF and $D400-$DFFF
; (ram.inc). Console RAM ($0800-$09FF): the ledger (ram.inc) and the stack.
;
; REGISTERS. R0 DMA, R1 the ISR, R2 the stack (X = 2), R3 the PC, R4-R6
; SCRT (FCALL/FRET), R7 FJMP, R8-R9 scratch, RA.1 = $08 (GETV/PUTV), RB a
; pointer into the cart's RAM (read only), RC the leaves' PC (LCALL), RD.0
; the beep (ISR) and RD.1 SCRT's, RE the frame count (ISR), RF the
; hotspots (macros only).
;
; RULES (tools/s2): no 3-cycle opcode ($C0-$CF) and no IDL (checkrom), a
; short branch stays on its page (RFIT, fitcheck), RF only read with LDN,
; nothing stored through a cart pointer (checkrom), a value in D not lost
; to a pointer load (checkldr), the RAM ledger (checkram).
; ---------------------------------------------------------------------------
        CPU     1802
        INCLUDE "fujinet.inc"
        INCLUDE "s2macro.inc"
        INCLUDE "macros.inc"
        INCLUDE "flags.inc"
        INCLUDE "proto.inc"
        INCLUDE "equ.inc"
        INCLUDE "msgequ.inc"
        INCLUDE "ram.inc"

; ---- the runtime: $0400-$07FB ----------------------------------------------------
        ORG     0400H
        INCLUDE "s2call.inc"
        INCLUDE "calls.inc"
        INCLUDE "fujilib.inc"
RT_SEGEND:
        SEGEND  FN_CLAIM
        S2CLAIM

; ---- fixed code: $0C00-$0FFF, then $x400-$xFFF up to $9FFF ------------------------
        ORG     0C00H
        INCLUDE "main.inc"
        INCLUDE "input.inc"
        INCLUDE "ui.inc"
        INCLUDE "fld.inc"
        INCLUDE "cat.inc"
        INCLUDE "tank.inc"
        INCLUDE "hud.inc"
        INCLUDE "net.inc"
        INCLUDE "p_scr.inc"
        INCLUDE "p_hud.inc"
        INCLUDE "p_ply.inc"
        INCLUDE "fieldui.inc"
        INCLUDE "msg.inc"
        INCLUDE "sound.inc"
        IF      TEST
        INCLUDE "p_test.inc"
        ENDIF
CODE_SEGEND:                            ; (data from here: no page rules)
        DFIT    64
        INCLUDE "config.inc"
        INCLUDE "tankart.inc"
        INCLUDE "snd.inc"
DATA_SEGEND:
        SEGEND  0A000H

; ---- the bank window: $C400-$CFFF, one assembly a bank (build.sh) ----------------
        IF      BANK == BK_BOOT
        ORG     0C400H
        INCLUDE "p_boot.inc"
BK0_SEGEND:
        SEGEND  0D000H
        ENDIF
        IF      BANK == BK_ROOM
        ORG     0C400H
        INCLUDE "p_room.inc"
BK1_SEGEND:
        SEGEND  0D000H
        ENDIF
        IF      BANK == BK_SHOP
        ORG     0C400H
        INCLUDE "p_shop.inc"
BK2_SEGEND:
        SEGEND  0D000H
        ENDIF
        IF      TEST && (BANK == BK_FIX)
        ORG     FIXBUF                  ; the rigs' fixture (tools/s2/mkfix_s2.py)
        REPT    FIXLEN
        DB      0
        ENDM
        ENDIF

; ---- the cart's RAM: image pages the client writes (ram.inc) -----------------------
        ORG     CRAM1
        REPT    CRAM1E-CRAM1
        DB      0
        ENDM
        ORG     CRAM2
        REPT    CRAM2E-CRAM2
        DB      0
        ENDM
        ORG     CRAM3
        REPT    CRAM3E-CRAM3
        DB      0
        ENDM
