;
; crt0.s -- FujiBarrage on the NES: the NES 2.0 header, start-up, the
; frame's interrupts and the PPU queue (src/nes/nes.h). From M.U.L.E.'s
; src/nes/crt0.s (a tile per cell in CHR-RAM, banks switched in windows,
; the queue drained with the display off) and FujiBlast's (a palette split).
;
; The field (mode DM_FIELD): 21 rows of 32 cells, each cell its own tile.
; Three rows are a band, and a band's tiles are a 2K CHR bank of their own
; (bands 0-6: banks 0-13 in 1K units): its 96 cells' tiles, then 32 the
; renderer keeps the same (all sky, all dirt, the stars: nes.h's T_*). A
; band's cells use tiles 0-127 if it is even (MMC3's R0, $0000) and
; 128-255 if odd (R1, $0800), and each register is switched while the
; other's band is on the screen: anywhere in its 24 lines, so neither the
; interrupt's lateness nor a line more or less in MMC3's count (hardware
; and MesenCE clock it on the pre-render line, MAME's cart doesn't) can
; put a wrong tile up. Under the field a row of black (the gap), where
; rendering goes off for the HUD's palettes, then the HUD (band 7, banks
; 14-15, R1), 3 rows of text drawn into tiles. With the vertical scroll at
; 12 the field's name rows 4-24 are lines 20-187, the gap row 25 lines
; 188-195 and the HUD's rows 26-28 lines 196-219:
;
;   NMI        R0 band 0, R1 band 1; the field's palettes; everything on
;   line  55   R0 band 2           (band 1 shows: lines 44-67)
;         79   R1 band 3           (band 2: 68-91)
;        103   R0 band 4           (band 3: 92-115)
;        127   R1 band 5           (band 4: 116-139)
;        151   R0 band 6           (band 5: 140-163)
;        175   R1 band 7, the HUD  (band 6: 164-187)
;        187   the split: rendering off, the HUD's palettes (the gap is
;              black), rendering on for line 196 at name row 26
;        219   off: from here to the end of the vertical blank the PPU is
;              free, and the queue is drained (then again in the NMI)
;
; (The interrupts' lines are hardware's and MesenCE's; MAME's are a line
; later, the HUD too.)
;
; Text (DM_TEXT): name rows 3-26 (24 rows, the vertical scroll 0) in the
; font's tables (R0, R1: banks 20-23), one interrupt at line 215 for the
; drain. DM_OFF: the display off, no interrupts.
;
; Rendering goes off mid-screen at a dot that leaves OAM alone: ~150, in
; the next line's sprite evaluation, with no sprite on that line (M.U.L.E.'s
; rule: off where the interrupt lands, a 2C02 and MesenCE copy OAM row 0
; over another row at the pre-render line). The sprites are 8x8 from $1000
; (banks 16-19), so MMC3's counter is clocked once a line.
;
; The NMI counts the frames (the server's 60 Hz: one NES frame in
; CLK_EVERY isn't counted), and when the IRQ's drain isn't running, drains
; the queue (a budget's worth), writes the palettes, sends the sprites and
; puts the display back for the next frame; when it is, the IRQ does that
; after. Game code never touches the PPU while it shows a picture: drawing
; goes into the queue. While a whole screen is drawn, ppu_direct is set:
; the NMI turns the display off and the game drains the queue itself.
;
; The sprites have two pages (oam, oam2): the game builds the one not
; shown and names it in oam_next; the NMI sends that one from then on.
;
; MMC3's bank select register is shared: the game sets R6 (prg_r6), the
; interrupts R0/R1. The game's choice is kept in mmc3_sel, written before
; the register, and whatever the interrupts change they put back.
;
        .export         __STARTUP__ : absolute = 1
        .export         _exit
        .export         _ppu_drain, _prg_r6, _nmi_hook
        .exportzp       _qhead, _qtail, _ppu_direct, _mmc3_sel, _ppu_off
        .exportzp       _oam_show, _oam_next, _disp_mode, _scroll_x, _r6_bank
        .export         _frame_count, _nes_frames, _ppuq, _oam, _oam2
        .export         _pal_bg, _pal_hud, _pal_spr, _pal_spr_dirty
        .export         _irq_hits, _drain_left
        .import         initlib, donelib, callmain, zerobss, copydata
        .include        "zeropage.inc"

PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
OAMADDR   = $2003
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
OAMDMA    = $4014

CTRL_ON   = $88                 ; NMI, 8x8 sprites at $1000, background at $0000
MASK_ALL  = $1E                 ; background and sprites, no clipping

DM_OFF    = 0
DM_TEXT   = 1
DM_FIELD  = 2

FIELD_Y   = 12                  ; the field's vertical scroll
TEXT_Y    = 0
FONT_BANK = 20                  ; the font's 4K (1K units)
HUD_BANK  = 14
HUD_ROW   = 26                  ; the HUD's first name row

; The first interrupt: the counter is reloaded with the latch at the first
; line it is clocked on (the pre-render line, or MAME's line 0), and goes
; off when it has counted down to 0 (the end of that line); each sets the
; next (lines to it, less one).
FIELD_FIRST = 56
TEXT_FIRST  = 215

; Drain budgets, in units of 16 cycles, counted from the code below at its
; slowest: a copy of n bytes is 15n + 120 cycles, a fill 9n + 130, a tile
; 160 + 130. The IRQ's starts at line 220 and the display must be back on
; by the pre-render line's dot 280 (or it isn't scrolled to the top): its
; budget is what leaves time for the NMI's work (the palettes, the DMA,
; show: ~1000 cycles) after. The NMI's own has the vertical blank before
; that work.
IRQ_BUDGET = 200
NMI_BUDGET = 70
OFF_DELAY  = 8                  ; the IRQ's wait before rendering goes off (dot ~150)
OFF_DELAY_F = 1                 ; the same in the field's steps (more before it)
CLK_EVERY  = 608                ; 60.0988 * 607 / 608 = 60.000 Hz

Q_TILE    = $00                 ; queue ops: a tile (16 bytes)
Q_MAP     = 1                   ; op $80's: a read, remap, write of cells
Q_MOVE    = 2                   ; and a copy of VRAM
QBUF      = $0100               ; (the stack page's bottom: their buffer, 32 bytes)
                                ; $01-$7F: a copy of n bytes; $81-$FF: a fill of n & $7F

; ---------------------------------------------------------------------------
; The header: NES 2.0, mapper 4, 256K PRG, 32K CHR-RAM, 8K PRG-RAM.
.segment "HEADER"
        .byte   "NES", $1A
        .byte   16              ; PRG: 16 x 16K
        .byte   0               ; CHR-ROM: none (RAM)
        .byte   $40             ; mapper 4 (low); MMC3 sets the mirroring
        .byte   $08             ; NES 2.0
        .byte   0, 0            ; mapper high, submapper; ROM size high
        .byte   $07             ; PRG-RAM 64 << 7 = 8K
        .byte   $09             ; CHR-RAM 64 << 9 = 32K
        .byte   0, 0, 0, 0

; ---------------------------------------------------------------------------
.segment "ZEROPAGE"
_qhead:      .res 1             ; the queue: where the game writes next
_qtail:      .res 1             ; where the drain reads next
_ppu_direct: .res 1
_mmc3_sel:   .res 1             ; the game's bank select (6)
_r6_bank:    .res 1             ; and R6's bank
_disp_mode:  .res 1             ; DM_*, from the next frame
_scroll_x:   .res 1             ; the field's horizontal scroll (the shake)
_ppu_off:    .res 1             ; the NMI has turned the display off
_oam_show:   .res 1             ; the sprites' page the NMI sends
_oam_next:   .res 1             ; the page to send from now on (0: none)
budget:      .res 1
irq_step:    .res 1
in_drain:    .res 1             ; the IRQ's drain is going
nmi_late:    .res 1             ; and the NMI came meanwhile
cur_mode:    .res 1             ; the mode this frame shows
op:          .res 1
tmp:         .res 1
ahi:         .res 1
alo:         .res 1
cnt:         .res 1
qx:          .res 1
clk_n:       .res 2             ; frames to the next one not counted

.segment "BSS"
_frame_count: .res 2            ; the game's 60 Hz (cleared by zerobss)
_nes_frames:  .res 2            ; every NMI
_pal_bg:      .res 16           ; the field's (or text's) background palettes
_pal_hud:     .res 16           ; the HUD's, from the split
_pal_spr:     .res 16           ; the sprites'
_pal_spr_dirty: .res 1          ; written at the next NMI
_irq_hits:    .res 1            ; interrupts this frame (tools/nessim)
_drain_left:  .res 1            ; queue bytes left when the last drain stopped
_nmi_hook:    .res 2            ; called each NMI, after the PPU (the sound), if set

.segment "PPUQ"
_ppuq:       .res 256

.segment "OAM"
_oam:        .res 256

.segment "OAM2"
_oam2:       .res 256

; ---------------------------------------------------------------------------
.segment "STARTUP"

start:
        sei
        cld
        ldx     #$FF
        txs
        inx
        stx     PPUCTRL         ; NMI off
        stx     PPUMASK
        stx     $4010           ; no DMC interrupt
        lda     #$40
        sta     $4017           ; no APU frame interrupt
        stx     $E000           ; no MMC3 interrupt
        bit     PPUSTATUS
@v1:    bit     PPUSTATUS
        bpl     @v1

        ; MMC3: PRG mode 0 ($8000 R6, $A000 R7, $C000 fixed), CHR mode 0
        ldy     #0
@mm:    sty     $8000
        lda     mmc3_init,y
        sta     $8001
        iny
        cpy     #8
        bne     @mm
        lda     #$80
        sta     $A001           ; WRAM on, writable
        lda     #1
        sta     $A000           ; horizontal mirroring (a shake wraps the field round)

        ; RAM: all of it zero (the stack's page too: nothing is on it yet)
        lda     #0
        tax
@clr:   sta     $00,x
        sta     $0100,x
        sta     $0200,x
        sta     $0300,x
        sta     $0400,x
        sta     $0500,x
        sta     $0600,x
        sta     $0700,x
        inx
        bne     @clr
        lda     #6
        sta     _mmc3_sel
        sta     $8000
        lda     mmc3_init+6
        sta     _r6_bank

        ; sprites all off the screen
        lda     #$F8
@ofs:   sta     _oam,x
        sta     _oam2,x
        inx
        bne     @ofs
        lda     #>_oam
        sta     _oam_show

@v2:    bit     PPUSTATUS
        bpl     @v2

        jsr     zerobss
        jsr     copydata
        lda     #<(__CSTACK_START__ + __CSTACK_SIZE__)
        ldx     #>(__CSTACK_START__ + __CSTACK_SIZE__)
        sta     c_sp
        stx     c_sp+1
        lda     #1
        sta     _ppu_direct     ; the display stays off until the game shows one
        lda     #CTRL_ON
        sta     PPUCTRL         ; the frame clock
        cli                     ; (the scanline interrupts)
        jsr     initlib
        jsr     callmain
_exit:  jsr     donelib
        jmp     start

        .import __CSTACK_START__, __CSTACK_SIZE__

mmc3_init:
        .byte   0, 2, 16, 17, 18, 19    ; CHR: bands 0, 1; the sprites' 4K
        .byte   1, 0                    ; PRG: R6, R7 (barrage-nes.cfg)

; ---------------------------------------------------------------------------
.segment "CODE"

; prg_r6(bank): the window at $8000 gets PRG bank A.
_prg_r6:
        ldx     #6
        stx     _mmc3_sel
        sta     _r6_bank
        stx     $8000
        sta     $8001
        rts

; chr(reg, bank): an interrupt's bank switch, the game's select put back
.macro  chr_set reg, bank
        lda     reg
        sta     $8000
        lda     bank
        sta     $8001
        lda     _mmc3_sel
        sta     $8000
.endmacro

; ---------------------------------------------------------------------------
; The display for the next frame (in the vertical blank, or after the
; IRQ's drain): the mode's banks, palettes, scroll and first interrupt.
show:
        lda     _disp_mode
        sta     cur_mode
        bne     @on
        lda     #0              ; DM_OFF
        sta     PPUMASK
        sta     $E000
        rts
@on:    ; the palettes: the background's (the split wrote the HUD's over
        ; them), the sprites' if they changed
        bit     PPUSTATUS
        lda     #$3F
        sta     PPUADDR
        lda     #0
        sta     PPUADDR
        ldx     #0
@pb:    lda     _pal_bg,x
        sta     PPUDATA
        inx
        cpx     #16
        bne     @pb
        lda     _pal_spr_dirty
        beq     @ps
        lda     #0
        sta     _pal_spr_dirty
        ldx     #0
@sp:    lda     _pal_spr,x
        sta     PPUDATA
        inx
        cpx     #16
        bne     @sp
@ps:    lda     #0              ; the interrupt off (acknowledged)
        sta     $E000
        sta     irq_step
        sta     _irq_hits
        lda     cur_mode
        cmp     #DM_FIELD
        bne     @text
        chr_set #0, #0          ; R0 band 0, R1 band 1
        chr_set #1, #2
        bit     PPUSTATUS
        lda     #CTRL_ON
        sta     PPUCTRL
        lda     _scroll_x
        sta     PPUSCROLL
        lda     #FIELD_Y
        sta     PPUSCROLL
        lda     #FIELD_FIRST
        bne     @go
@text:  chr_set #0, #FONT_BANK
        chr_set #1, #FONT_BANK+2
        bit     PPUSTATUS
        lda     #CTRL_ON
        sta     PPUCTRL
        lda     #0
        sta     PPUSCROLL
        lda     #TEXT_Y
        sta     PPUSCROLL
        lda     #TEXT_FIRST
@go:    sta     $C000
        sta     $C001
        sta     $E001
        lda     #MASK_ALL
        sta     PPUMASK
        rts

; The sprites' DMA, from the page the game last finished.
oam_send:
        lda     _oam_next
        beq     @dma
        sta     _oam_show
        lda     #0
        sta     _oam_next
@dma:   lda     #0
        sta     OAMADDR
        lda     _oam_show
        sta     OAMDMA
        rts

nmi:
        pha
        txa
        pha
        tya
        pha
        inc     _nes_frames
        bne     @nf
        inc     _nes_frames+1
@nf:    lda     clk_n           ; (one frame in CLK_EVERY isn't counted)
        bne     @cl
        lda     clk_n+1
        bne     @ch
        lda     #<(CLK_EVERY - 1)
        sta     clk_n
        lda     #>(CLK_EVERY - 1)
        sta     clk_n+1
        jmp     @c
@ch:    dec     clk_n+1
@cl:    dec     clk_n
        inc     _frame_count
        bne     @c
        inc     _frame_count+1
@c:     lda     _ppu_direct
        bne     @direct
        lda     in_drain
        bne     @late
        lda     #NMI_BUDGET
        jsr     _ppu_drain
        jsr     oam_send
        jsr     show
        jmp     @hook
@late:  lda     #1              ; the IRQ's drain shows it when done
        sta     nmi_late
        jmp     @hook
@direct:
        lda     #0
        sta     PPUMASK
        sta     $E000
        lda     #1
        sta     _ppu_off
@hook:  lda     _nmi_hook+1
        beq     @x
        jsr     @call
@x:     pla
        tay
        pla
        tax
        pla
        rti
@call:  jmp     (_nmi_hook)

; ---------------------------------------------------------------------------
; The scanline interrupt's steps (see the top). The field's: a bank
; register, its bank and the latch for the next; then the split, then off.
FIELD_SWAPS = 6
irq_reg:  .byte 0, 1, 0, 1, 0, 1
irq_bank: .byte 4, 6, 8, 10, 12, HUD_BANK
irq_next: .byte 23, 23, 23, 23, 23, 11

irq:
        pha
        txa
        pha
        tya
        pha
        inc     _irq_hits
        sta     $E000           ; acknowledged, off
        lda     cur_mode
        cmp     #DM_FIELD
        bne     @off
        ldx     irq_step
        inc     irq_step
        cpx     #FIELD_SWAPS
        bcs     @split
        chr_set {irq_reg,x}, {irq_bank,x}
        lda     irq_next,x
        sta     $C000
        sta     $C001
        sta     $E001
        jmp     @ret
@split: bne     @off            ; (step 7: the drain)
        jmp     split
        ; the end of the picture: off at dot ~150 of the next line, and
        ; the queue
@off:   ldy     #OFF_DELAY
        lda     cur_mode
        cmp     #DM_FIELD
        bne     @dl
        ldy     #OFF_DELAY_F
@dl:    dey
        bne     @dl
        lda     #0
        sta     PPUMASK
        sta     $E000
        lda     #1
        sta     in_drain
        lda     #IRQ_BUDGET
        jsr     _ppu_drain
        lda     #0
        sta     in_drain
        lda     nmi_late
        beq     @ret
        lda     #0
        sta     nmi_late
        jsr     oam_send
        jsr     show
@ret:   pla
        tay
        pla
        tax
        pla
        rti

; ---------------------------------------------------------------------------
; The split (see the top). The interrupt comes at dot ~260 of the field's
; last line (187: MesenCE and hardware; 188 in MAME). Rendering goes off at
; dot ~150 of the next, then video address is parked on palette entry 4,
; which no tile shows and is always black (with rendering off the screen
; shows the colour the address points at, or the backdrop's). The HUD's
; palettes go in one burst a line, each in a horizontal blank: the
; backdrop (then the address parked again), then a palette's three
; colours, which leave the address on entry 8, 12 or 16 (the backdrop:
; black by then); a burst shows colours for 16 cycles at most, of the
; blank's 28. The bursts are a line apart (113.67 cycles: 114, 113, 114,
; 114 between their first $2006 low writes); then the address for name
; row 26 (fine Y 0) and rendering on in line 195's blank. The counter isn't
; clocked meanwhile: the drain's interrupt is 24 lines after.

; delay n: exactly n cycles (n >= 2), a loop that stays in its page
.macro  delay n
        .local  lp
        .if n = 2
        nop
        .elseif n = 3
        bit     $00
        .elseif n = 4
        nop
        nop
        .elseif n = 5
        nop
        bit     $00
        .elseif n = 6
        nop
        nop
        nop
        .elseif n = 7
        nop
        nop
        bit     $00
        .else
        ; 5k + 1 cycles, k >= 1, then 2..6 more
        .local  k, r
        k = (n - 3) / 5
        r = n - (5 * k + 1)
        ldy     #k
lp:     dey
        bne     lp
        .assert >lp = >*, error, "delay: a loop across a page"
        .if r = 2
        nop
        .elseif r = 3
        bit     $00
        .elseif r = 4
        nop
        nop
        .elseif r = 5
        nop
        bit     $00
        .elseif r = 6
        nop
        nop
        nop
        .endif
        .endif
.endmacro

; Tuned in MesenCE (tools/nessim/nesrun, the test card: 60 frames each):
; the bursts are all in the blank for SPLIT_D0 110-120 (a dot or two of
; the line's ends shows outside that, in the overscan), the HUD's first
; line is line 196 for SPLIT_DR 176-188.
.ifndef SPLIT_DP
SPLIT_DP = 33                   ; rendering off to the park (in the blank)
.endif
.ifndef SPLIT_D0
SPLIT_D0 = 116 - SPLIT_DP       ; the park to the first burst
.endif
.ifndef SPLIT_DR
SPLIT_DR = 182                  ; the last burst to the resume
.endif

.segment "SPLIT"                ; (a page of its own: no loop branches across one)
split:
        ldy     #OFF_DELAY_F
@d1:    dey
        bne     @d1
        lda     #0
        sta     PPUMASK         ; rendering off, dot ~190 (the gap row's tile
        delay   SPLIT_DP        ; is the backdrop's colour: the line is one)
        bit     PPUSTATUS       ; then in the blank,
        lda     #$3F
        sta     PPUADDR
        lda     #$04
        sta     PPUADDR         ; parked on entry 4
        lda     #23             ; the drain's interrupt: 24 lines after the resume
        sta     $C000
        sta     $C001
        sta     $E001
        delay   SPLIT_D0        ; (it takes Y)
        lda     #$3F
        ldx     #$00
        ldy     _pal_hud
        ; burst 0: the backdrop (14 cycles from its address to parked)
        sta     PPUADDR
        stx     PPUADDR         ; <- the reference: v on entry 0
        sty     PPUDATA
        sta     PPUADDR
        lda     #$04
        sta     PPUADDR         ; parked
        ; 14 + d + 20 = 114
        delay   80
        .repeat 4, p
        ldx     _pal_hud + 4 * p + 1
        ldy     _pal_hud + 4 * p + 2
        lda     #$3F
        sta     PPUADDR
        lda     #4 * p + 1
        sta     PPUADDR         ; <- the reference
        stx     PPUDATA
        sty     PPUDATA
        lda     _pal_hud + 4 * p + 3
        sta     PPUDATA         ; v on entry 4p+4: black
        .if p = 0
        delay   (113 - 36)
        .elseif p < 3
        delay   (114 - 36)
        .endif
        .endrepeat
        delay   SPLIT_DR
        bit     PPUSTATUS
        lda     #0
        ldx     #>(HUD_ROW * 32)  ; (v, not an address: $2000 would be fine Y 2)
        ldy     #<(HUD_ROW * 32)
        sta     PPUSCROLL       ; (fine X 0; both, the toggle is $2006's too)
        sta     PPUSCROLL
        stx     PPUADDR
        sty     PPUADDR
        lda     #MASK_ALL
        sta     PPUMASK
        pla
        tay
        pla
        tax
        pla
        rti
        .assert >split = >*, error, "split: more than a page"
.segment "CODE"

; ---------------------------------------------------------------------------
; ppu_drain(budget): run queued commands while the budget lasts (255: all
; of them). Only with the PPU free: from the interrupts, or the game while
; ppu_direct is set. A command: its op (Q_TILE, a copy's length, or a
; fill's with bit 7), a CHR bank for R0 ($FF: none), the address (high,
; low), then the bytes (a fill's: one). Op $80 is one of two that read
; VRAM first (its "bank" says which; the text screens', src/nes/scr.c),
; through a buffer at the stack page's bottom (QBUF):
;   Q_MAP   hi, lo, n, map[4]: n cells' tiles t become (t & 63) | map[t >> 6]
;   Q_MOVE  hi, lo, n, shi, slo: n bytes from shi:slo to hi:lo
_ppu_drain:
        sta     budget
@next:  ldx     _qtail
        cpx     _qhead
        bne     @have
        lda     #0
        sta     _drain_left
        rts
@have:  lda     _ppuq,x
        beq     @ctile
        bmi     @cfill
        sta     tmp             ; a copy: (15n + 120) / 16 ~ n + 8
        clc
        adc     #8
        bcc     @chk
@cfill: and     #$7F            ; a fill: (9n + 130) / 16 ~ n/2 + 9
        beq     @cread
        lsr     a
        adc     #9
        bne     @chk
@cread: lda     #72             ; a map or a move of 32: ~1100 cycles at most
@ctile: lda     #19             ; a tile: 290 / 16
@chk:   ldy     budget
        cpy     #255
        beq     @go
        cmp     budget
        beq     @take
        bcc     @take
        txa                     ; not this frame
        eor     #$FF
        sec
        adc     _qhead
        sta     _drain_left
        rts
@take:  sta     tmp
        lda     budget
        sec
        sbc     tmp
        sta     budget
@go:    lda     _ppuq,x
        sta     op
        inx
        lda     _ppuq,x         ; the bank, for R0
        inx
        cmp     #$FF
        beq     @addr
        ldy     #0
        sty     $8000
        sta     $8001
        lda     _mmc3_sel
        sta     $8000
@addr:  bit     PPUSTATUS
        lda     _ppuq,x
        sta     ahi
        sta     PPUADDR
        inx
        lda     _ppuq,x
        sta     alo
        sta     PPUADDR
        inx
        lda     op
        bne     @nt
        jmp     @tile
@nt:    bmi     @fill
@copy:  tay
@cl:    lda     _ppuq,x
        sta     PPUDATA
        inx
        dey
        bne     @cl
        stx     _qtail
        lda     ahi             ; a palette's: the address off it (with the
        cmp     #$3F            ; display off, the colour it points at is
        bne     @cn             ; what the screen shows)
        sty     PPUADDR
        sty     PPUADDR
@cn:    jmp     @next
@fill:  and     #$7F
        beq     @read
        tay
        lda     _ppuq,x
        inx
@fl:    sta     PPUDATA
        dey
        bne     @fl
        stx     _qtail
        jmp     @next
@read:  dex                     ; (back to the "bank": which)
        dex
        dex
        lda     _ppuq,x
        inx
        inx
        inx
        cmp     #Q_MOVE
        beq     @move
        ; Q_MAP: n cells read, remapped, written back
        lda     _ppuq,x         ; n
        sta     cnt
        inx
        stx     qx              ; (the map)
        lda     PPUDATA         ; (the first read is the old buffer's)
        ldy     #0
@mr:    lda     PPUDATA
        sta     QBUF,y
        iny
        cpy     cnt
        bne     @mr
        ldy     #0
@mm:    lda     QBUF,y          ; t & 63 | map[t >> 6]
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        clc
        adc     qx
        tax
        lda     _ppuq,x
        sta     tmp
        lda     QBUF,y
        and     #$3F
        ora     tmp
        sta     QBUF,y
        iny
        cpy     cnt
        bne     @mm
        bit     PPUSTATUS       ; and back
        lda     ahi
        sta     PPUADDR
        lda     alo
        sta     PPUADDR
        ldy     #0
@mw:    lda     QBUF,y
        sta     PPUDATA
        iny
        cpy     cnt
        bne     @mw
        lda     qx              ; past the map
        clc
        adc     #4
        sta     _qtail
        jmp     @next
@move:  lda     _ppuq,x         ; n
        sta     cnt
        inx
        bit     PPUSTATUS       ; from shi:slo
        lda     _ppuq,x
        sta     PPUADDR
        inx
        lda     _ppuq,x
        sta     PPUADDR
        inx
        stx     _qtail
        lda     PPUDATA
        ldy     #0
@vr:    lda     PPUDATA
        sta     QBUF,y
        iny
        cpy     cnt
        bne     @vr
        bit     PPUSTATUS       ; to hi:lo
        lda     ahi
        sta     PPUADDR
        lda     alo
        sta     PPUADDR
        ldy     #0
@vw:    lda     QBUF,y
        sta     PPUDATA
        iny
        cpy     cnt
        bne     @vw
        jmp     @next
@tile:  .repeat 16
        lda     _ppuq,x
        sta     PPUDATA
        inx
        .endrepeat
        stx     _qtail
        jmp     @next

; ---------------------------------------------------------------------------
.segment "VECTORS"
        .word   nmi
        .word   start
        .word   irq

.segment "CLAIM"
        .byte   "FUJI"
