;
; dlc.s -- the field's display list and its DLIs, compiled (dsp.s runs them).
;
; Colour: the sky (COLPF0) and the dirt (COLPF1) change every 11 lines, a
; gradient per sky and per dirt palette. The tanks are drawn in the bitmap
; in colour 0 (black, COLBK); with PRIOR 4 the playfield covers the players,
; so a player standing behind a tank as a solid block shows its colour only
; through that tank's own pixels. Four players are reused down the screen:
; a DLI moves one (HPOS, COLPM) on a line between two of its tanks, where it
; isn't drawn. The four missiles make a fifth (PRIOR $10, COLPF3), used only
; when no player is free; it covers the playfield, so its shape is the
; tank's own black pixels, read from the field, a missile to two pixels.
; More tanks than that on the same lines stay black: this client's own and
; the shooter's are given colours first.
;
; Each DLI line gets a routine of LDA #/STA pairs, compiled into the back
; half of DLICODE with a display list to match; dsp.s shows the pair from
; the next frame. A routine that moves players does so before any WSYNC
; (the players aren't drawn there), so a line with players and a gradient
; step is avoided: the gradient step moves up a line or two instead.
;
; void dlc_build(void)
; void __fastcall__ dlc_palette(uint16_t a)   sky (SKY_*) low, dirt palette high
;
        .export _dlc_build, _dlc_palette
.ifdef __ATARI5200__
        .export _dlc_sky        ; (dsp.s: the first line's sky, set in the VBI)
.endif
        .import _tk, _seat_col, _hue, _my_seat, _st, _fld_get
        .import _dl_field, _dsp_chain, _dsp_dl, _dsp_flip, _dsp_front, _dsp_prior
        .import _fx_col
        .importzp _fx, _fy

        .include "proto.inc"

BAND    = 11            ; 16 bands: the worst chain (10 tanks on 10 lines)
NBANDS  = 16            ; stays under CODE_MAX
CODE_MAX = $2F0         ; a 768-byte buffer, less the last routine
DL_SIZE = 184
F_LINES = 168
TANK_H  = 5             ; the body's rows: the player's block
        .include "hw.inc"       ; (DLICODE, PLAYERS, VDSLST, WSYNC, GTIA: the 5200's too)
FIELD_ADDR = FIELD
STATUS_ADDR = STATUS

        .segment "ZEROPAGE"
cp:     .res 2          ; where the code goes
dp:     .res 2          ; the display list being built
pp:     .res 2          ; a player's memory; a routine's head (to link it)

        .segment "HIGH"
_dlc_sky:  .res NBANDS
_dlc_dirt: .res NBANDS
is_tank: .res F_LINES   ; line -> 1: a players' DLI is there
is_grad: .res F_LINES   ; line -> band + 1: a gradient step
ev_line: .res SEATS     ; a player moved: on this line,
ev_slot: .res SEATS     ; this one (4: the missiles),
ev_hp:  .res SEATS      ; to here,
ev_col: .res SEATS      ; this colour
first_hp: .res 5        ; where they start
first_col: .res 5
tt:     .res SEATS      ; a tank's top + 8
rk:     .res SEATS      ; 0 this client's, 1 the shooter's, 2 the rest
order:  .res SEATS      ; the tanks, top first

        .segment "BSS"
se:     .res 5          ; where a slot's last tank ends + 8 (0: none yet)
nt:     .res 1
nev:    .res 1
j:      .res 1
s:      .res 1
best:   .res 1
lim:    .res 1
hp:     .res 1
col:    .res 1
kk:     .res 1
row:    .res 1
r:      .res 1
g:      .res 1
fx0:    .res 2
l:      .res 1
evi:    .res 1
gb:     .res 1
rh:     .res 1          ; a register's high byte (sti)
bf:     .res 1          ; the back pair: 0 or 2
cbh:    .res 1          ; its code's page
sky:    .res 1
pal:    .res 1

        .rodata
dltop:  .byte   $70, $70, $70, $42, <STATUS_ADDR, >STATUS_ADDR
        .byte   $82, $82        ; status rows 1 (the players' start), 2 (the colours)
        .byte   $4E, <FIELD_ADDR, >FIELD_ADDR
gbit:   .byte   $02, $01, $08, $04, $20, $10, $80   ; pixel p's GRAFM bit
sunset: .byte   $52, $54, $44, $44, $46, $36, $38, $28, $2A, $2A, $1C, $1C
        .byte   $1E, $1E, $1E, $1E
dirt_hue: .byte $F0, $C0, $00, $30, $10, $90, $60, $E0
dirt_l: .byte   12, 12, 11, 10, 10, 9, 8, 8, 7, 6, 6, 5, 4, 4, 3, 2  ; 12 - 2k/3

        .code

; ---- the gradients -----------------------------------------------------------

_dlc_palette:
        sta     sky
        txa
        and     #7
        sta     pal
        ldx     #NBANDS-1
@k:     stx     j
        lda     sky
        cmp     #SKY_PLAIN
        bne     @n1
        lda     #$96
        bne     @set
@n1:    cmp     #SKY_STARS
        bne     @n2
        txa
        sbc     #8              ; (carry set)
        bcc     @blk            ; the top half black
        ora     #$80
        bne     @set
@n2:    cmp     #SKY_STORM
        bne     @n3
        txa
        lsr
        clc
        adc     #2
        bne     @set
@n3:    cmp     #SKY_SUNSET
        bne     @n4
        lda     sunset,x
        bne     @set
@n4:    cmp     #SKY_CAVERN
        bne     @n5
        lda     #$F0
        cpx     #8
        bcc     @set
        lda     #$F2
        bne     @set
@n5:    cmp     #SKY_BLACK
        beq     @blk
        inx                     ; the gradient: night blue to day
        txa
        cpx     #9
        bcs     @day
        ora     #$80
        bne     @set
@day:   cmp     #16
        bcc     @d
        lda     #15
@d:     ora     #$90
        bne     @set
@blk:   lda     #0
@set:   jsr     _hue
        ldx     j
        sta     _dlc_sky,x
        ldy     pal
        lda     dirt_hue,y
        ora     dirt_l,x
        jsr     _hue
        ldx     j
        sta     _dlc_dirt,x
        dex
        bpl     @k
        rts

; ---- emitting code -------------------------------------------------------------

; sti: LDA #A, STA rh:X
sti:    pha
        lda     #$A9
        jsr     emit
        pla
        jsr     emit
; sta_: STA rh:X
sta_:   lda     #$8D
        jsr     emit
        txa
        jsr     emit
        lda     rh
; emit: A at cp (X, as it was)
emit:   ldy     #0
        sta     (cp),y
        inc     cp
        bne     @x
        inc     cp+1
@x:     rts

; head: PHA, then VDSLST to the next routine (tail fills it in) first thing
; -- a DLI that came while this one ran would otherwise find VDSLST half
; changed
head:   lda     cp
        sta     pp
        lda     cp+1
        sta     pp+1
        lda     #$48
        jsr     emit
        lda     #>VDSLST
        sta     rh
        ldx     #<VDSLST
        jsr     sti
        ldx     #<VDSLST+1
        jsr     sti
        lda     #>GTIA
        sta     rh
        rts

; tail: PLA, RTI; the head's VDSLST gets where the next one starts
tail:   lda     #$68
        jsr     emit
        lda     #$40
        jsr     emit
        ldy     #2
        lda     cp
        sta     (pp),y
        ldy     #7
        lda     cp+1
        sta     (pp),y
        rts

; put: slot A (4: the missiles) to hp, colour col
put:    cmp     #4
        beq     @m
        tax                     ; HPOSP0 + slot
        lda     hp
        jsr     sti
        txa
        clc
        adc     #$12            ; COLPM0 + slot
        tax
        lda     col
        jmp     sti
@m:     ldx     #4              ; HPOSM0-3: two pixels apart
        lda     hp
@mm:    pha
        jsr     sti
        pla
        clc
        adc     #2
        inx
        cpx     #8
        bne     @mm
        lda     col
        ldx     #$19            ; COLPF3
        jmp     sti

; grad: band X's sky and dirt: LDA, WSYNC, COLPF0, LDA/STA COLPF1
grad:   stx     gb
        lda     #$A9
        jsr     emit
        lda     _dlc_sky,x
        jsr     emit
        lda     #>WSYNC
        sta     rh
        ldx     #<WSYNC
        jsr     sta_
        lda     #>GTIA
        sta     rh
        ldx     #$16
        jsr     sta_
        ldx     gb
        lda     _dlc_dirt,x
        ldx     #$17
        jmp     sti

; ---- the build -------------------------------------------------------------------

_dlc_build:
@w:     lda     _dsp_flip       ; the last build not shown yet: its buffer
        bne     @w              ; is the one we'd write
        lda     _dsp_front
        eor     #2
        sta     bf
        beq     @f0
        lda     #<(_dl_field+DL_SIZE)
        sta     dp
        lda     #>(_dl_field+DL_SIZE)
        sta     dp+1
        lda     #>(DLICODE+$300)
        bne     @f1
@f0:    lda     #<_dl_field
        sta     dp
        lda     #>_dl_field
        sta     dp+1
        lda     #>DLICODE
@f1:    sta     cp+1
        sta     cbh
        lda     #0
        sta     cp

        ; the tanks, top first; on the same lines, this client's own, then
        ; the shooter's
        lda     #0
        sta     nt
        ldx     #0
@ts:    stx     s
        txa
        asl
        asl
        adc     s
        tay                     ; tk + 5 s
        lda     _tk,y
        beq     @tn
        lda     _tk+3,y
        clc
        adc     #8-TANK_H
        sta     tt,x
        lda     #0
        cpx     _my_seat
        beq     @rk
        lda     #1
        cpx     _st+OFF_TURN
        beq     @rk
        lda     #2
@rk:    sta     rk,x
        ldy     nt
@ins:   cpy     #0
        beq     @put
        ldx     order-1,y
        lda     tt,x
        ldx     s
        cmp     tt,x
        bcc     @put
        bne     @shift
        ldx     order-1,y
        lda     rk,x
        ldx     s
        cmp     rk,x
        bcc     @put
        beq     @put
@shift: lda     order-1,y
        sta     order,y
        dey
        jmp     @ins
@put:   lda     s
        sta     order,y
        inc     nt
@tn:    ldx     s
        inx
        cpx     #SEATS
        bne     @ts

        lda     #0              ; nothing drawn, no DLIs yet
        tax
@cl:    sta     MISSILES,x
        sta     PLAYERS,x
        sta     PLAYERS+$100,x
        sta     PLAYERS+$200,x
        sta     PLAYERS+$300,x
        inx
        bne     @cl
        ldx     #F_LINES
@cl2:   sta     is_tank-1,x
        sta     is_grad-1,x
        dex
        bne     @cl2
        ldx     #4
@cl3:   sta     se,x
        sta     first_hp,x
        sta     first_col,x
        dex
        bpl     @cl3
        sta     nev
        sta     j

        ; each tank to a player whose last tank ended above the line before
        ; its top: the move goes on that line
@tank:  lda     j
        cmp     nt
        bne     @t1
        jmp     @grads
@t1:    tax
        lda     order,x
        sta     s
        tax
        lda     tt,x
        sec
        sbc     #1
        sta     lim             ; a slot must have ended above this
        lda     #$FF
        sta     best
        ldy     #0
@sl:    lda     se,y            ; the player free longest
        cmp     lim
        bcs     @sn
        ldx     best
        bmi     @take
        cmp     se,x
        bcs     @sn
@take:  sty     best
@sn:    iny
        cpy     #4
        bne     @sl
        lda     best
        bpl     @have
        lda     se+4            ; the missiles only when no player's free
        cmp     lim
        bcc     @msl
        jmp     @next
@msl:   lda     #4
        sta     best
@have:  lda     s
        asl
        asl
        adc     s
        tay
        lda     _tk+1,y         ; its left edge: 48 + x - 3
        clc
        adc     #45
        sta     hp
        lda     _tk+1,y
        sec
        sbc     #3
        sta     fx0
        lda     _tk+2,y
        sbc     #0
        sta     fx0+1
        ldx     s
        lda     _seat_col,x
        jsr     _hue
        sta     col
        ldx     best
        lda     se,x
        beq     @first
        sec
        sbc     #7
        sta     kk              ; the first line the slot is free on
        ldx     s
        lda     tt,x
        cmp     #10
        bcc     @first          ; (near the top: from the start)
        sbc     #9              ; the line before its top
        tax
        ; the latest line the slot is free on with no DLI next to it (one
        ; there already: share it)
@ll:    cpx     kk
        beq     @lg
        lda     is_tank,x
        bne     @lg
        lda     is_tank-1,x
        ora     is_tank+1,x
        beq     @lg
        dex
        jmp     @ll
@lg:    lda     #1
        sta     is_tank,x
        txa
        ldy     nev
        sta     ev_line,y
        lda     best
        sta     ev_slot,y
        lda     hp
        sta     ev_hp,y
        lda     col
        sta     ev_col,y
        inc     nev
        jmp     @gfx
@first: ldx     best
        lda     hp
        sta     first_hp,x
        lda     col
        sta     first_col,x
@gfx:   ldx     s
        lda     tt,x
        clc
        adc     #TANK_H-1       ; its bottom + 8 - 1: the slot's end + 8
        ldy     best
        sta     se,y
        lda     tt,x
        clc
        adc     #48             ; its first player line (F_TOP - 8)
        sta     row
        cpy     #4
        beq     @mis
        tya
        clc
        adc     #>PLAYERS
        sta     pp+1
        lda     #0
        sta     pp
        ldy     row
        ldx     #TANK_H
        lda     #$FE
@pr:    sta     (pp),y
        iny
        dex
        bne     @pr
        beq     @next
@mis:   lda     #0              ; the missiles: the tank's black pixels
        sta     r
@mr:    ldx     s
        lda     tt,x
        clc
        adc     r
        sec
        sbc     #8
        sta     _fy
        lda     #0
        sbc     #0
        sta     _fy+1
        lda     #0
        sta     g
        sta     kk
@mp:    lda     fx0
        clc
        adc     kk
        sta     _fx
        lda     fx0+1
        adc     #0
        sta     _fx+1
        jsr     _fld_get
        cmp     #0
        bne     @mn
        ldx     kk
        lda     gbit,x
        ora     g
        sta     g
@mn:    inc     kk
        lda     kk
        cmp     #7
        bne     @mp
        lda     row
        clc
        adc     r
        tax
        lda     g
        sta     MISSILES,x
        inc     r
        lda     r
        cmp     #TANK_H
        bne     @mr
@next:  inc     j
        jmp     @tank

        ; the gradient steps: band k from line 11k, the DLI on the line
        ; before, moved up a line or more to keep a line free between DLIs,
        ; or onto a players' DLI (its moves first, then the WSYNC)
@grads: lda     #1
        sta     j               ; the band
        lda     #BAND-1
        sta     kk              ; its line
@gk:    ldx     kk
        ldy     #6
@gi:    lda     is_tank,x
        bne     @gs
        lda     is_tank-1,x
        ora     is_tank+1,x
        ora     is_grad-1,x
        ora     is_grad+1,x
        ora     is_grad,x
        beq     @gs
        dex
        dey
        bne     @gi
        ldx     kk              ; nowhere better: as it falls
@gs:    ldy     j
        iny
        tya
        sta     is_grad,x
        lda     kk
        clc
        adc     #BAND
        sta     kk
        inc     j
        lda     j
        cmp     #NBANDS
        bne     @gk

        ; the display list
        ldy     #10
@dt:    lda     dltop,y
        sta     (dp),y
        dey
        bpl     @dt
        ldy     #11
        lda     #$0E
@df:    sta     (dp),y
        iny
        cpy     #180
        bne     @df
        lda     #$41            ; (Y 180) JVB to itself
        sta     (dp),y
        iny
        lda     dp
        sta     (dp),y
        iny
        lda     dp+1
        sta     (dp),y
        ldy     #112            ; line 102: at the 4K boundary
        lda     #$4E
        sta     (dp),y
        iny
        lda     #<FIELD2
        sta     (dp),y
        iny
        lda     #>FIELD2
        sta     (dp),y

        ; the routines: R0, the players' first places
        jsr     head
        lda     #0
@r0:    sta     j
        tax
        lda     first_hp,x
        sta     hp
        lda     first_col,x
        sta     col
        txa
        jsr     put
        ldx     j
        inx
        txa
        cmp     #5
        bne     @r0
        jsr     tail
        ; R1: the field's colours, line 0
        jsr     head
        ldx     #0
        jsr     grad
        lda     #$AD            ; LDA fx_col (the VBI cycles it)
        jsr     emit
        lda     #<_fx_col
        jsr     emit
        lda     #>_fx_col
        jsr     emit
        ldx     #$18            ; COLPF2
        jsr     sta_
        lda     #0
        ldx     #$1A            ; COLBK
        jsr     sti
        jsr     tail
        ; a routine for each line with a DLI
        lda     #0
@ln:    sta     l
        tax
        lda     is_tank,x
        ora     is_grad,x
        beq     @lx
        lda     cp+1            ; (can't overflow: the bound holds)
        sec
        sbc     cbh
        cmp     #>CODE_MAX
        bcc     @ok
        bne     @full
        lda     cp
        cmp     #<CODE_MAX+1
        bcc     @ok
@full:  lda     is_tank,x       ; a gradient step goes first
        beq     @lx
        jmp     @done
@ok:    txa                     ; the line's instruction gets its DLI
        beq     @i0
        cmp     #103
        bcc     @i1
        adc     #11             ; (carry set: 12 + l)
        bne     @i2
@i1:    adc     #10
        bne     @i2
@i0:    lda     #8
@i2:    tay
        lda     (dp),y
        ora     #$80
        sta     (dp),y
        jsr     head
        ldx     l
        lda     is_tank,x
        beq     @ng
        ldy     #0
@ev:    cpy     nev
        beq     @ng
        lda     ev_line,y
        cmp     l
        bne     @en
        sty     evi
        lda     ev_hp,y
        sta     hp
        lda     ev_col,y
        sta     col
        lda     ev_slot,y
        jsr     put
        ldy     evi
@en:    iny
        bne     @ev
@ng:    ldx     l
        ldy     is_grad,x
        beq     @nt
        dey
        tya
        tax
        jsr     grad
@nt:    jsr     tail
@lx:    ldx     l
        inx
        txa
        cpx     #F_LINES
        beq     @done
        jmp     @ln
@done:  lda     #$40            ; past the last: a bare RTI
        jsr     emit
        ldx     bf
        lda     #0
        sta     _dsp_chain,x
        lda     cbh
        sta     _dsp_chain+1,x
        lda     dp
        sta     _dsp_dl,x
        lda     dp+1
        sta     _dsp_dl+1,x
        lda     #$14            ; playfield over players; the fifth player
        sta     _dsp_prior
        lda     #1
        sta     _dsp_flip
        rts
