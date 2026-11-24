/* scr3.h -- the CoCo 3's display, inside: its palette indices and the HUD
 * rows (src/coco3/scr3.c; the colours: tools/mkgfx_coco.py pal3) */
#ifndef SCR3_H
#define SCR3_H

#include <stdint.h>

/* the palette's indices: 0 black, on the text screens 1-5 the rows'
 * grounds and the dim ink, on the field 1-2 sky, 3-4 dirt, 5 fire; 6-15
 * the seats' colours on both (seat 7 white) */
#define C3_BLACK  0
#define C3_BLUE   1
#define C3_RED    2
#define C3_GREY   3
#define C3_YELLOW 4
#define C3_DIM    5
#define C3_SEAT   6
#define C3_WHITE  (C3_SEAT + 7)

#define SCR_HUD_ROW 22              /* the HUD's rows: 22-24 (lines 176-199) */

void scr3_repalette(void);          /* the monitor changed: the text's palette again */
void scr3_hud(uint8_t row, uint8_t col, const char *s, uint8_t hi);
void scr3_hud_clear(uint8_t row);
void scr3_hud_name(uint8_t row, uint8_t col, uint8_t n, uint8_t seat);

#endif
