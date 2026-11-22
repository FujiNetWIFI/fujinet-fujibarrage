/* scr.h -- text screens (menus, rooms, the waiting room, results): 24 rows
 * of 40 characters, each row its own colours. */
#ifndef SCR_H
#define SCR_H

#include <stdint.h>

#define SCR_ROWS 24
#define SCR_COLS 40

/* palettes for a row */
enum { PAL_TEXT, PAL_TITLE, PAL_HEAD, PAL_HILITE, PAL_DIM, PAL_SEAT };

void scr_init(void);                            /* the display, at start-up */
void scr_begin(void);                           /* clear, all rows PAL_TEXT; off */
void scr_row(uint8_t row, uint8_t pal);         /* PAL_SEAT + seat: in its colour */
void scr_text(uint8_t row, uint8_t col, const char *s);
void scr_inverse(uint8_t row, uint8_t col, uint8_t n);  /* highlight n cells */
void scr_clear_row(uint8_t row);
void scr_scroll(uint8_t row, uint8_t n, uint8_t up);    /* rows row..row+n-1 move a row */
void scr_show(void);                            /* on, from the next frame */

#endif
