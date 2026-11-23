*
* chain.asm -- the menu and the game are two programs on the disk (as on
* the Atari and the Apple II): chain(name) loads the other one's low core
* (NAME.BIN, a DECB file: blocks of 00 len addr data, then FF 0000 exec)
* with DSKCON and runs it; its bank_init loads the rest. It runs from the
* caller's high core ($8000 up), which the low core's load doesn't touch,
* with its own stack there; the new program's bank_init loads its own high
* core over it only once it has jumped. Its sector buffers are DECB's own
* ($0600 a sector, $0700 the FAT). Interrupts stay masked: the new program
* sets its own up. A sector read that fails is
* tried again, 8 times (MAME's DSKCON fails now and then: bank.c's
* load_try). chain returns (0) only if the file isn't there or can't be
* read, and by then the caller's memory may be gone: it shows a message
* and stops.
*
        section hicode

        export  _chain

DCOPC   equ     $EA
DCDRV   equ     $EB
DCTRK   equ     $EC
DCSEC   equ     $ED
DCBPT   equ     $EE
DCSTA   equ     $F0
DEFDRV  equ     $095A
SEC     equ     $0600
FAT     equ     $0700

* uint8_t chain(const char *name)  (8 characters, space padded)
_chain
        orcc    #$50
        clra                    ; (DSKCON's parameters: BASIC's direct page)
        tfr     a,dp
        ldx     2,s
        ldu     #cname
        ldb     #8
@cp     lda     ,x+
        sta     ,u+
        decb
        bne     @cp
        lds     #chstk
        lda     $FFF8           ; a CoCo 3 (its IRQ vector $FEF7): slot 6
        cmpa    #$FE            ;  the Disk ROM's block (DriveWire runs there)
        bne     @c2
        lda     #$3E
        sta     $FFA6
@c2     lda     #17             ; the FAT: track 17, sector 2
        ldb     #2
        lbsr    rdsec
        ldx     #SEC
        ldu     #FAT
        ldb     #68
@fat    lda     ,x+
        sta     ,u+
        decb
        bne     @fat
        ldb     #3              ; the directory: sectors 3-11
@dsec   pshs    b
        lda     #17
        lbsr    rdsec
        ldx     #SEC
@ent    lda     ,x
        cmpa    #$FF
        lbeq    fail            ; (the directory's end)
        ldy     #cname
        ldb     #8
@cmp    lda     ,x+
        cmpa    ,y+
        bne     @next
        decb
        bne     @cmp
        lda     ,x              ; the extension: BIN
        cmpa    #'B
        bne     @next
        lda     1,x
        cmpa    #'I
        bne     @next
        lda     2,x
        cmpa    #'N
        beq     @found
@next   tfr     x,d             ; the next entry (32 bytes each)
        andb    #$E0
        addd    #32
        tfr     d,x
        cmpx    #SEC+256
        blo     @ent
        puls    b
        incb
        cmpb    #12
        blo     @dsec
        lbra    fail
@found  leas    1,s
        lda     5,x             ; (x: the extension) the first granule
        sta     gran
        ldd     6,x             ; the last sector's bytes
        bne     @lb
        ldd     #256
@lb     cmpd    #256
        bls     @lb2
        ldd     #256
@lb2    std     lastb
        clr     hn
        clr     left
        clr     left+1
* the granules: a FAT byte is the next granule, or $C0 + the sectors used
* in the last one; granule g is track g / 2 (17 skipped), sectors 1-9 or
* 10-18
@gran   ldb     gran
        cmpb    #68
        lbhs    fail
        ldx     #FAT
        lda     b,x
        sta     nxt
        ldb     #9
        cmpa    #$C0
        blo     @full
        anda    #$3F
        tfr     a,b
@full   stb     nsec
        clr     isec
@sec    lda     isec
        cmpa    nsec
        bhs     @gdone
        ldb     gran
        lsrb
        cmpb    #17
        blo     @t
        incb
@t      tfr     b,a             ; A: the track
        ldb     gran
        andb    #1
        beq     @s0
        ldb     #9
@s0     addb    isec
        incb                    ; B: the sector
        lbsr    rdsec
        lda     nxt             ; this sector's bytes: all, or the last's
        cmpa    #$C0
        blo     @all
        lda     isec
        inca
        cmpa    nsec
        bne     @all
        ldd     lastb
        bra     @n
@all    ldd     #256
@n      ldx     #SEC
        lbsr    feed
        inc     isec
        bra     @sec
@gdone  lda     nxt
        cmpa    #$C0
        lbhs    fail            ; (the file ended before its exec record)
        sta     gran
        bra     @gran

* feed: D bytes at X through the DECB stream; runs the program at its
* exec record (doesn't come back then)
feed    std     cnt
@byte   ldd     cnt
        beq     @fdone
        subd    #1
        std     cnt
        lda     ,x+
        ldu     left
        beq     @hdr
        ldy     at
        sta     ,y+
        sty     at
        leau    -1,u
        stu     left
        bra     @byte
@hdr    ldb     hn
        ldy     #hdr
        sta     b,y
        incb
        stb     hn
        cmpb    #5
        bne     @byte
        clr     hn
        lda     hdr
        cmpa    #$FF
        beq     @exec
        ldd     hdr+1
        std     left
        ldd     hdr+3
        std     at
        bra     @byte
@exec   lbra    start
@fdone  rts

* start: the new program, the machine as BASIC's LOADM leaves it (CMOC's
* start-up calls into BASIC's ROM): no interrupt source on (the vector at
* $010C is still the old program's); a CoCo 3's slot 7 its own block
* again; a CoCo 1/2 back in ROM mode -- from low RAM, as this code (the
* old high core) goes under the ROM: sta $FFDE / jmp exec at $0EF8 (the
* direct page's top, past fixed.asm's bytes)
start   lds     #$8000          ; the stack where a LOADM'd program has it (this
                                ;  one's goes under the ROM)
        lda     $FF01
        anda    #$FE            ; PIA0 CA1 (hsync) off
        sta     $FF01
        lda     $FF03
        anda    #$FE            ; PIA0 CB1 (field sync) off
        sta     $FF03
        lda     $FF02           ; (any flag cleared)
        lda     $FF00
        ldx     hdr+3
        lda     $FFF8
        cmpa    #$FE
        bne     @c12
        clr     $FF92           ; the GIME's sources off
        clr     $FF93
        lda     #$3F            ; slot 7: BASIC's block
        sta     $FFA7
        jmp     ,x
@c12    ldd     #$B7FF          ; sta $FFDE
        std     $0EF8
        ldd     #$DE7E          ;  jmp exec
        std     $0EFA
        stx     $0EFC
        jmp     $0EF8

* rdsec: track A, sector B into SEC, tried 8 times; fail if it can't
rdsec   sta     DCTRK
        stb     DCSEC
        lda     #8
        sta     tries
@try    lda     #2
        sta     DCOPC
        lda     DEFDRV
        sta     DCDRV
        ldx     #SEC
        stx     DCBPT
        pshs    u,y,x
        jsr     [$C004]
        puls    u,y,x
        lda     DCSTA
        beq     @ok
        dec     tries
        bne     @try
        leas    2,s             ; (rdsec's return: fail instead)
        bra     fail
@ok     rts

* fail: the text screen says so, and it stops (the caller's memory is gone)
fail    lda     $FFF8           ; a CoCo 3: its CoCo 1/2 video
        cmpa    #$FE
        bne     @vdg
        lda     #$CC
        sta     $FF90
        clr     $FF98
@vdg    clr     $FF22           ; the VDG: text
        sta     $FFC0           ; the SAM: mode 0, the screen at $0400
        sta     $FFC2
        sta     $FFC4
        sta     $FFC6
        sta     $FFC9
        sta     $FFCA
        sta     $FFCC
        sta     $FFCE
        sta     $FFD0
        sta     $FFD2
        ldx     #$0400
        ldd     #$6060
@clr    std     ,x++
        cmpx    #$0600
        blo     @clr
        ldx     #$0400+32*7+8
        ldu     #msg
@m      lda     ,u+
        beq     @stop
        sta     ,x+
        bra     @m
@stop   bra     @stop

msg     fcc     "CAN'T LOAD THE"
        fcb     $60
        fcc     "PROGRAM"
        fcb     0
cname   rmb     8
gran    fcb     0
nxt     fcb     0
nsec    fcb     0
isec    fcb     0
lastb   fdb     0
cnt     fdb     0
left    fdb     0
at      fdb     0
hn      fcb     0
hdr     rmb     5
tries   fcb     0
        rmb     96              ; its stack
chstk
