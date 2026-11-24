/* scr.h -- text screens (menus, rooms, the waiting room, results): 24 rows
 * of 40 characters, each row its own colours. */
#ifndef SCR_H
#define SCR_H

#include <stdint.h>

#define SCR_ROWS 24
#if defined(__H89__)
#define SCR_COLS 80             /* the H19: 80 characters (layout.h has its own block) */
#elif (defined(_COCO_BASIC_) && !defined(COCO3)) || defined(__ADAM__)
#define SCR_COLS 32             /* the CoCo 1/2: 4 colour pixels a column (src/coco2/scr2.c); the TMS9918 */
#else
#define SCR_COLS 40
#endif

/* a text line's buffer (a 40-column line's has always been 41 bytes) */
#if SCR_COLS > 40
#define LINE_BUF (SCR_COLS + 1)
#else
#define LINE_BUF 41
#endif

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
#if defined(_COCO_BASIC_) || defined(__ADAM__) || defined(__MSDOS__)
/* Text in colour (TODO 2026-11-23): a label's characters have bit 7 set
 * and are drawn in the row's label ink (labels yellow or the nearest,
 * values white); SCR_NAMES: a name can be drawn in its seat's colour
 * (field_name, field.h). Elsewhere a label is plain text. */
#define SCR_INK 1
void scr_label(uint8_t row, uint8_t col, const char *s);
#if (defined(_COCO_BASIC_) && defined(COCO3)) || (defined(__MSDOS__) && defined(DOS_VGA))
#define SCR_NAMES 1
#endif
#else
#define scr_label scr_text
#endif
#if defined(_COCO_BASIC_) || defined(__ADAM__) || defined(__MSDOS__)
#define SCR_PICTURE 1
void scr_picture(void);                         /* the title: a picture, its words in it */
#endif

#endif
