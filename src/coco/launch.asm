*
* launch.asm -- BARRAGE.BIN: the disk's launcher. The CoCo lobby doesn't
* boot the disk (AUTOEXEC.BAS never runs): it types RUNM"<the disk's
* name>", so the program named after the disk must pick the machine's own
* build. This one loads BARRAGE3 on a CoCo 3 (its IRQ vector $FEF7: $FFF8
* reads $FE) and BARRAG2 (the CoCo 1/2's game) on anything else, with
* chainld.asm, and runs it as BASIC's LOADM:EXEC would have: BASIC's stack
* and condition codes back. AUTOEXEC.BAS LOADMs this too.
*
* At $1000, under both low files (the CoCo 3's loads at $1F00, the CoCo
* 1/2's at $2800); a DECB binary of its own (lwasm --decb).
*
        org     $1000

launch  sts     bas_s           ; BASIC's (EXEC's return address on it)
        tfr     cc,a
        sta     bas_cc
        orcc    #$50
        clra                    ; (DSKCON's parameters: BASIC's direct page)
        tfr     a,dp
        ldx     #name2
        lda     $FFF8
        cmpa    #$FE
        bne     @cp
        ldx     #name3
@cp     ldu     #cname
        ldb     #8
@cpl    lda     ,x+
        sta     ,u+
        decb
        bne     @cpl
        lds     #chstk
        include "chainld.asm"

* start: the game, the machine as LOADM:EXEC leaves it
start   ldx     hdr+3
        lds     bas_s
        lda     bas_cc
        tfr     a,cc
        jmp     ,x

name2   fcc     "BARRAG2 "
name3   fcc     "BARRAGE3"
bas_s   fdb     0
bas_cc  fcb     0

        end     launch
