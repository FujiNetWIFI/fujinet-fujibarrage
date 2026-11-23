;
; dsp.s -- the display's interrupts. Two kinds of screen:
;
;   text   24 rows of ANTIC 2 (scr.c); each row its own colours from the
;          rw_* tables, loaded by a DLI on the row before (prepared ahead,
;          little before the WSYNC: FujiBlast's lesson)
;   field  3 status rows of ANTIC 2, then 168 lines of ANTIC E (fld.s). Its
;          DLIs are compiled (dlc.c) into DLICODE: each sets a few colour or
;          player registers and points VDSLST at the next. The VBI starts the
;          chain afresh each frame, and swaps in a rebuilt chain (with its
;          display list) only between frames, so a frame never shows half
;          of each.
;
; The VBI stays short (FujiBlast's traps): IRQs go back on first unless it
; came in over an IRQ handler; it writes the registers the OS's second
; stage would copy, and skips that stage (CRITIC).
;
        .export _dsp_init, _dsp_mode, _dsp_dma, _dsp_flip, _dsp_flash
        .export _dl_text, _rw_bk, _rw_c1, _rw_c2, _fx_col, _fx_cycle
        .export _st_bk, _st_c1, _st_c2, _dsp_chain, _dsp_dl, _dsp_prior
        .export _vbi_count, _dl_field, _dsp_front, _dsp_shake
        .import _snd_vbi
.ifdef __ATARI5200__
        .import vbi_tail        ; (crt0.s: the clock, the pots, then out)
.ifndef MENU
        .import _dlc_sky
.endif
.else
        .constructor clrlow, 30
.endif

        .include "hw.inc"

ROWS    = 24

        .segment "PMGFREE"
_dl_text: .res 32       ; the text screen's display list
_dl_field: .res 2*184   ; the field's, front and back (in one 1K block)
_rw_bk:  .res ROWS      ; each text row's border, text brightness, background
_rw_c1:  .res ROWS
_rw_c2:  .res ROWS
dli_idx: .res 1
nx_bk:   .res 1
nx_c1:   .res 1
nx_c2:   .res 1

        .segment "BSS2"
_dsp_dma:   .res 1      ; SDMCTL: 0 while a screen is built
_dsp_mode:  .res 1      ; 0 text, 1 field
_dsp_chain: .res 4      ; the field's chain start, display list: front, back
_dsp_dl:    .res 4
_dsp_flip:  .res 1      ; 1: the back pair is ready, show it next frame
_dsp_front:
front:      .res 1      ; 0 or 2: which chain and list are shown
_dsp_flash: .res 1      ; frames of a white field left
_dsp_prior: .res 1
_fx_col:    .res 1      ; COLPF2 in the field: fire, shells, text
_fx_cycle:  .res 1      ; 1: fx_col cycles through the fire colours
_st_bk:     .res 1      ; the status rows' colours
_st_c1:     .res 1
_st_c2:     .res 1
_vbi_count: .res 1
fire_t:     .res 1
_dsp_shake: .res 1      ; frames of a shaking screen left

        .data
fire:   .byte   $36, $38, $3A, $1C, $1E, $1C, $3A, $38   ; (a hue back on PAL)

        .code

.ifndef __ATARI5200__          ; (the 5200's crt0 clears all its RAM)
; Low memory ($0480-$09FF: LOWBSS, BSS2) holds variables cc65 doesn't
; clear; picoboot used part of it while loading.
clrlow: lda     #0
        tax
@a:     sta     $0480,x
        sta     $0580,x
        sta     $0680,x
        sta     $0780,x
        sta     $0880,x
        inx
        bne     @a
        ldx     #$7F
@b:     sta     $0980,x
        dex
        bpl     @b
        rts
.endif

; void dsp_init(void)
_dsp_init:
.ifdef __ATARI5200__
        lda     #0
        sta     NMIEN
        lda     #$F8            ; the BIOS's font (the crt0 left CHBASE 0)
        sta     CHBASE
        lda     #<tdli
        sta     VDSLST
        lda     #>tdli
        sta     VDSLST+1
        lda     #<vbi
        sta     VVBLKI
        lda     #>vbi
        sta     VVBLKI+1
        lda     #$C0
        sta     NMIEN
        rts
.else
        lda     PAL             ; PAL: the fire's hues a step back (scr.c: hue)
        and     #$0E
        bne     @ntsc
        ldx     #7
@h:     lda     fire,x
        cmp     #$20
        bcc     @hn
        sbc     #$10
        sta     fire,x
@hn:    dex
        bpl     @h
@ntsc:  lda     #0
        sta     NMIEN
        lda     #<tdli
        sta     VDSLST
        lda     #>tdli
        sta     VDSLST+1
        lda     #6              ; immediate VBI
        ldy     #<vbi
        ldx     #>vbi
        jsr     SETVBV
        lda     #$C0
        sta     NMIEN
        rts
.endif

; ---- the text screen's DLI: the next row's colours, ready since the last --
tdli:   pha
        txa
        pha
        lda     nx_c2
        ldx     nx_bk
        sta     WSYNC
        sta     COLPF2
        stx     COLBK
        lda     nx_c1
        sta     COLPF1
        jsr     prep
        pla
        tax
        pla
        rti

prep:   ldx     dli_idx
        inx
        stx     dli_idx
        cpx     #ROWS
        bcc     @ok
        ldx     #ROWS-1
@ok:    lda     _rw_bk,x
        sta     nx_bk
        lda     _rw_c1,x
        sta     nx_c1
        lda     _rw_c2,x
        sta     nx_c2
        rts

; ---- the VBI ----------------------------------------------------------------
vbi:
.ifdef __ATARI5200__
        pha                     ; (the 5200's BIOS pushes nothing: as the OS does)
        txa
        pha
        tya
        pha
.endif
        tsx                     ; the stack: Y X A P PCL PCH
        lda     $0104,x
        and     #$04
        bne     @irq
        cli
@irq:   lda     #0
        sta     ATRACT
        inc     _vbi_count
        lda     _dsp_flip       ; a rebuilt field chain: shown from now on
        beq     @nf0            ; (whatever the screen: dlc_build waits for it)
        lda     front
        eor     #2
        sta     front
        lda     #0
        sta     _dsp_flip
@nf0:   lda     _dsp_dma
        sta     SDMCTL
        sta     DMACTL
        lda     _dsp_mode
        bne     @field
        ; text: row 0's colours, the DLI from row 1 on
        sta     dli_idx
        lda     #<_dl_text
        sta     DLISTL
        lda     #>_dl_text
        sta     DLISTL+1
        lda     #<tdli
        sta     VDSLST
        lda     #>tdli
        sta     VDSLST+1
        lda     _rw_bk
        sta     COLBK
        lda     _rw_c1
        sta     COLPF1
        lda     _rw_c2
        sta     COLPF2
        lda     #0
        sta     PRIOR
.ifndef __ATARI5200__
        sta     GPRIOR
.endif
        sta     GRACTL          ; no players on text (they'd keep their last byte)
        jsr     prep
        jmp     @tail
@field: ldx     front
        lda     _dsp_dl,x
        sta     DLISTL
        lda     _dsp_dl+1,x
        sta     DLISTL+1
        lda     _dsp_chain,x
        sta     VDSLST
        lda     _dsp_chain+1,x
        sta     VDSLST+1
        ldy     #$70            ; a shake: all of it 4 lines up, 2 frames
        lda     _dsp_shake      ; in 4 (the DLIs go with their lines)
        beq     @sk
        dec     _dsp_shake
        and     #2
        beq     @sk
        ldy     #$30
@sk:    sty     _dl_field
        sty     _dl_field+184
        lda     _st_bk
        sta     COLBK
        lda     _st_c1
        sta     COLPF1
        lda     _st_c2
        sta     COLPF2
.if .defined(__ATARI5200__) .and .not .defined(MENU)
        lda     _dlc_sky        ; the first line's sky now: the 5200's DLI on the
        sta     COLPF0          ; last text row is late enough that line 0 can
.endif                          ; show (the status rows don't use COLPF0)
        lda     _dsp_prior
        sta     PRIOR
.ifndef __ATARI5200__
        sta     GPRIOR
.endif
        lda     #3
        sta     GRACTL
        lda     _fx_cycle       ; the fire colours turn over
        beq     @fl
        inc     fire_t
        lda     fire_t
        lsr
        and     #7
        tax
        lda     fire,x
        sta     _fx_col
@fl:    lda     _dsp_flash      ; a flash: the DLIs off, the field white
        beq     @dli
        dec     _dsp_flash
        lda     #$40
        sta     NMIEN
        lda     #$0E
        sta     COLPF0
        sta     COLPF1
        sta     COLPF2
        sta     COLBK
        bne     @tail
@dli:   lda     #$C0
        sta     NMIEN
@tail:  jsr     _snd_vbi
.ifdef __ATARI5200__
        lda     #1              ; the crt0's shadows not copied (see above)
        sta     CRITIC
        jmp     vbi_tail
.else
        lda     KEYDEL          ; the keyboard's debounce, stage 2's job
        beq     @kd
        dec     KEYDEL
@kd:    lda     #1              ; no stage 2 (see above)
        sta     CRITIC
        jmp     SYSVBV
.endif
