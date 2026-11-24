; barrage.asm -- FujiBarrage for the Emerson Arcadia 2001, on the FujiNet cart.
;
; Assembled once per page (build.sh writes bb_pN.asm: PAGE EQU N, then
; this): every assembly holds the fixed halves, identically (the build
; checks), and its own page in the window at $0800-$0FFF. So a page can
; call fixed code but never another page -- another page's labels don't
; exist in its assembly. The image (protocol 2, APPBANK) is page 0's 8K,
; the mailbox empty and claimed, then pages 1.. (tools/arc/mkblast.py).
;
;   $0000-$05FF  fixed A   src/fixa.inc   start, vblank, input, display, main loop
;   $0600-$07FF  RAM-L     cart RAM: ST, the slow variables (src/ram.inc)
;   $0800-$0FFF  window    a 2K page: entries at $0800 + 3k, its id at $0FFF
;   $2000-$28FF  fixed B   src/fixb.inc   the mailbox, cart RAM writes, tables
;   $2900-$2AFF  RAM-H     cart RAM: SCRIPT, the shot's state
;   $2B00-$2FFF  mailbox
;
; FujiBlast's Arcadia port (the page model, the hardware layer, the boot
; and menus) with the Astrocade's link (src/astrocade/src/p_net.inc) for
; FujiBarrage's documents (docs/PROTOCOL.md).

        CPU     2650
        INCLUDE "src/macros.inc"
        GLYPHS
        INCLUDE "flags.inc"
        INCLUDE "proto.inc"
        INCLUDE "catequ.inc"
        INCLUDE "sndequ.inc"
        INCLUDE "src/equ.inc"
        INCLUDE "src/look.inc"
        INCLUDE "src/ram.inc"
        INCLUDE "src/pages.inc"

        INCLUDE "src/fixa.inc"

        ORG     WIN
        IF      PAGE == PG_BOOT
        INCLUDE "src/p_boot.inc"
        ELSEIF  PAGE == PG_MENU
        INCLUDE "src/p_menu.inc"
        ELSEIF  PAGE == PG_NET
        INCLUDE "src/p_net.inc"
        ELSEIF  PAGE == PG_ROOM
        INCLUDE "src/p_room.inc"
        ELSEIF  PAGE == PG_SHOP
        INCLUDE "src/p_shop.inc"
        ELSEIF  PAGE == PG_CAT
        INCLUDE "src/p_cat.inc"
        ELSEIF  PAGE == PG_SEND
        INCLUDE "src/p_send.inc"
        ELSEIF  PAGE == PG_PLAY
        INCLUDE "src/p_play.inc"
        ELSEIF  PAGE == PG_FLD
        INCLUDE "src/p_fld.inc"
        ELSEIF  PAGE == PG_PLY
        INCLUDE "src/p_ply.inc"
        ELSEIF  PAGE == PG_OPS
        INCLUDE "src/p_ops.inc"
        ELSEIF  PAGE == PG_TEST
        INCLUDE "src/p_test.inc"
        ELSEIF  PAGE == PG_TRK
        INCLUDE "src/p_trk.inc"
        ELSEIF  PAGE == PG_BLS
        INCLUDE "src/p_bls.inc"
        ELSEIF  PAGE == PG_SET
        INCLUDE "src/p_set.inc"
        ELSEIF  PAGE == PG_FLW
        INCLUDE "src/p_flw.inc"
        ELSEIF  PAGE == PG_HUD
        INCLUDE "src/p_hud.inc"
        ELSEIF  PAGE == PG_TSCR
        INCLUDE "src/p_tscr.inc"
        ELSEIF  PAGE == PG_SETS
        INCLUDE "src/p_sets.inc"
        ELSEIF  PAGE == PG_MSG
        INCLUDE "src/p_msg.inc"
        ELSEIF  PAGE == PG_SND
        INCLUDE "src/p_snd.inc"
        ELSEIF  PAGE == PG_SNDT
        INCLUDE "sndtit.inc"
        ELSE
        INCLUDE "src/p_stub.inc"
        ENDIF
        IF      $ > $0FFF
        ERROR   "this page is full"
        ENDIF
        ORG     $0FFF
        DB      PAGE            ; the page's id

        ORG     $2000
        INCLUDE "src/fixb.inc"
        IF      $ > $2900
        ERROR   "fixed B runs into RAM-H"
        ENDIF
