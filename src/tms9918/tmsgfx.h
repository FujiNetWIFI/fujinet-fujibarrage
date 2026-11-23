/*
 * tmsgfx.h -- the TMS9918 screen as the platform's code sees it
 * (src/tms9918/tms.c): set-up, the frame's work, the cell writer the text
 * screens, the HUD and the balloons share.
 */
#ifndef TMSGFX_H
#define TMSGFX_H

#include <stdint.h>

void tms_init(void);            /* registers, tables, the identity names; IE on */
void tms_card(void);            /* M0's test card (VIDEO_CARD): doesn't come back */
void tms_idle(void);            /* waiting for a frame: a changed field cell drawn */
void tms_frame(void);           /* a frame came: the sprites, then a burst while it's blank */
void tms_blank(uint8_t on);     /* the display blanked (1) or shown */

/* a cell (0-767): 8 pattern bytes and 8 colour bytes */
void tms_cell(uint16_t cell, const uint8_t *pat, const uint8_t *col);
void tms_cell_fill(uint16_t first, uint16_t n, uint8_t pat, uint8_t col);

/* field.c (the game's) */
void field_frame(void);         /* a frame: the sprites, a burst of cells */
void field_off(void);           /* a text screen comes up */
void ovl_cell(void);            /* render.asm: a cell with a tank in the bitmap */

extern uint8_t tms_blanked;     /* the display is off: VRAM goes flat out */
extern int8_t shake_dx;         /* nmi.asm: the shake's x, for the sprites */

#endif
