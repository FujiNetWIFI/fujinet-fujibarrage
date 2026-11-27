/*
 * nes.h -- the NES's hardware as the client uses it: src/nes/crt0.s (the
 * frame's interrupts, the display modes, the PPU queue) and the
 * cartridge's banks (src/nes/barrage-nes.cfg).
 */
#ifndef NES_H
#define NES_H

#include <stdint.h>

/* the display modes (crt0.s: from the next frame) */
#define DM_OFF   0
#define DM_TEXT  1              /* 24 text rows: name rows 3-26, the font's tables */
#define DM_FIELD 2              /* the field's 7 bands, the gap, the HUD */

/* CHR-RAM, in MMC3's 1K units: a band's tiles are 2K */
#define CHR_BAND(b)  ((b) * 2)  /* bands 0-6 the field's, 7 the HUD's */
#define CHR_SPR      16         /* the sprites' 4K ($1000) */
#define CHR_FONT     20         /* the text screens' 4K */

/* the name table's rows: the field's, the gap, the HUD's, text */
#define NT_FIELD     4
#define NT_GAP       25
#define NT_HUD       26
#define NT_TEXT      3

/* A band's 128 tiles: its 96 cells' (3 rows of 32), then the ones the
 * renderer keeps the same in every band (T_*); an odd band's are 128 on. */
#define BAND_CELLS   96
#define T_SKY        96         /* every pixel colour 1 */
#define T_DIRT       97         /* 2 */
#define T_CRUST      98         /* 0 */
#define T_FIRE       99         /* 3 */

/* The PPU queue (crt0.s's ppu_drain): commands of an op (Q_TILE, a copy's
 * length, a fill's | 0x80), a CHR bank for R0 (0xFF: none), the address
 * (high, low), then the bytes (a fill's one). */
#define Q_TILE   0x00
#define Q_NOBANK 0xFF

extern uint8_t qhead, qtail, ppu_direct, mmc3_sel, ppu_off, r6_bank;
extern uint8_t oam_show, oam_next, disp_mode, scroll_x;
#pragma zpsym ("qhead")
#pragma zpsym ("qtail")
#pragma zpsym ("ppu_direct")
#pragma zpsym ("mmc3_sel")
#pragma zpsym ("ppu_off")
#pragma zpsym ("r6_bank")
#pragma zpsym ("oam_show")
#pragma zpsym ("oam_next")
#pragma zpsym ("disp_mode")
#pragma zpsym ("scroll_x")
extern uint8_t nes_region;              /* 0 NTSC, 1 PAL, 2 a Dendy (crt0.s: measured at start-up) */
#pragma zpsym ("nes_region")
extern volatile uint16_t nes_frames;   /* every NMI (frame_count, the 60 Hz clock: plat.h) */
extern uint8_t ppuq[256], oam[256], oam2[256];
void __fastcall__ mmc3_chr(uint16_t rb);    /* CHR register rb & 255 gets bank rb >> 8 (bank.s) */
extern uint8_t pal_bg[16], pal_hud[16], pal_spr[16], pal_spr_dirty;
extern uint8_t irq_hits, drain_left;
extern void (*nmi_hook)(void);

void __fastcall__ ppu_drain(uint8_t budget);
void __fastcall__ prg_r6(uint8_t bank);

/* the queue's free bytes */
#define Q_ROOM() ((uint8_t)(qtail - qhead - 1))

#endif
