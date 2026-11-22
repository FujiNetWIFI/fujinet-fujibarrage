;
; xload.s -- the game loads its menu (MENU.COM) from this disk when the
; appkeys don't say who is playing where. MENU.COM is a DOS 2 / MyDOS file
; on a single-density disk: the directory at sectors 361-368, 125 data
; bytes a sector and the next sector's number in the last three. Its Atari
; binary segments go where they say (each INITAD called, as DOS does), then
; RUNAD runs it.
;
; Boot code (the field's memory, until the first round). Nothing of the
; game survives the load: its VBI and DLIs are put back to the OS's first.
;
; uint8_t xload(void)   0 if MENU.COM isn't on the disk; else it never returns
;
        .export _xload

DDEVIC  = $0300
DUNIT   = $0301
DCOMND  = $0302
DSTATS  = $0303
DBUFLO  = $0304
DBUFHI  = $0305
DTIMLO  = $0306
DBYTLO  = $0308
DBYTHI  = $0309
DAUX1   = $030A
DAUX2   = $030B
SIOV    = $E459
RUNAD   = $02E0
INITAD  = $02E2
VVBLKI  = $0222
VVBLKD  = $0224
VDSLST  = $0200
SYSVBV  = $E45F
XITVBV  = $E462
SDMCTL  = $022F
NMIEN   = $D40E
BUF     = $B900             ; a sector (the boot area, past this code)

        .segment "ZEROPAGE"
dst:    .res 2              ; where the next byte goes
last:   .res 2              ; a segment's last address
secn:   .res 2
next:   .res 2              ; the file's next sector (0: no more)
pos:    .res 1
cnt:    .res 1
tries:  .res 1

        .segment "BOOTCODE"

name:   .byte   "MENU    COM"

_xload:
        sei                     ; the game's display interrupts: the OS's again
        lda     #$40            ; VBIs only
        sta     NMIEN
        lda     #<SYSVBV
        sta     VVBLKI
        lda     #>SYSVBV
        sta     VVBLKI+1
        lda     #<XITVBV
        sta     VVBLKD
        lda     #>XITVBV
        sta     VVBLKD+1
        lda     #<done
        sta     VDSLST
        lda     #>done
        sta     VDSLST+1
        cli
        lda     #0
        sta     SDMCTL          ; the screen off while it loads
        lda     #<361
        sta     secn
        lda     #>361
        sta     secn+1
@dir:   jsr     readsec
        bmi     fail
        ldx     #0
@ent:   lda     BUF,x           ; in use, not deleted?
        and     #$C0
        cmp     #$40
        bne     @nx
        ldy     #0
@cmp:   lda     BUF+5,x
        cmp     name,y
        bne     @nx
        inx
        iny
        cpy     #11
        bne     @cmp
        txa                     ; found: back to the entry's start
        sec
        sbc     #11
        tax
        lda     BUF+3,x
        sta     next
        lda     BUF+4,x
        sta     next+1
        jmp     found
@nx:    txa                     ; the next entry (16 bytes each)
        and     #$F0
        clc
        adc     #16
        tax
        bpl     @ent
        inc     secn
        lda     secn
        cmp     #<369
        bne     @dir
fail:   lda     #0              ; (nothing of the game written yet)
        tax
        rts
done:   rti                     ; a DLI that does nothing

found:  lda     #0
        sta     pos
        sta     cnt
        sta     RUNAD
        sta     RUNAD+1
@seg:   jsr     get             ; a segment: start, last, bytes
        bcs     run
        sta     dst
        jsr     get
        bcs     run
        sta     dst+1
        and     dst
        cmp     #$FF            ; $FFFF: a binary file's mark, again
        beq     @seg
        jsr     get
        sta     last
        jsr     get
        sta     last+1
        lda     #<noinit
        sta     INITAD
        lda     #>noinit
        sta     INITAD+1
@copy:  jsr     get
        bcs     run
        ldy     #0
        sta     (dst),y
        lda     dst
        cmp     last
        bne     @inc
        lda     dst+1
        cmp     last+1
        beq     @end
@inc:   inc     dst
        bne     @copy
        inc     dst+1
        jmp     @copy
@end:   jsr     init
        jmp     @seg
run:    jmp     (RUNAD)

init:   jmp     (INITAD)
noinit: rts

; get: the file's next byte in A, carry clear; carry set at its end
get:    ldy     pos
        cpy     cnt
        bcc     @have
        lda     next
        ora     next+1
        bne     @more
        sec
        rts
@more:  lda     next
        sta     secn
        lda     next+1
        sta     secn+1
        jsr     readsec
        bpl     @rd
        jmp     $E477           ; half loaded and the disk failed: start again
@rd:    lda     BUF+127         ; bytes in this sector
        sta     cnt
        lda     BUF+125         ; the next sector's high bits
        and     #3
        sta     next+1
        lda     BUF+126
        sta     next
        ldy     #0
@have:  lda     BUF,y
        iny
        sty     pos
        clc
        rts

; readsec: sector secn into BUF (tried four times); N set on an error
readsec:
        lda     #4
        sta     tries
@try:   lda     #$31
        sta     DDEVIC
        lda     #1
        sta     DUNIT
        lda     #$52            ; read
        sta     DCOMND
        lda     #$40
        sta     DSTATS
        lda     #<BUF
        sta     DBUFLO
        lda     #>BUF
        sta     DBUFHI
        lda     #7
        sta     DTIMLO
        lda     #128
        sta     DBYTLO
        lda     #0
        sta     DBYTHI
        lda     secn
        sta     DAUX1
        lda     secn+1
        sta     DAUX2
        jsr     SIOV
        lda     DSTATS
        bpl     @ok
        dec     tries
        bne     @try
        lda     #$FF            ; N set
@ok:    rts
