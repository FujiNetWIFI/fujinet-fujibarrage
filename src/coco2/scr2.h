/* scr2.h -- the CoCo 1/2's display, inside: the artifact colours and the
 * HUD rows (src/coco2/scr2.c) */
#ifndef SCR2_H
#define SCR2_H

#include <stdint.h>

#define SCR_HUD_ROW 21              /* the HUD's rows: 21-23 (lines 168-191) */

uint8_t phys(uint8_t v);            /* a value (0 black, 1 blue, 2 orange, 3 white) as its bit pair */
uint8_t phys4(uint8_t v);           /* the same, a byte of four */
void scr2_recolour(void);           /* the artifact phase changed */
void scr2_hud(uint8_t row, uint8_t col, const char *s, uint8_t hi);
void scr2_hud_clear(uint8_t row);
void video_flip(void);              /* the screen in the other artifact phase (video.c) */

#endif
