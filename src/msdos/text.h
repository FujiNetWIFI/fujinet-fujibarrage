/* text.h -- the text cells each build draws (src/msdos/cga/text.c, src/msdos/vga/text.c
 * and their scra.asm), under the shared text screens of src/msdos/scr.c:
 * 25 rows of 40 cells of 8x8 pixels, rows 22-24 also the field's HUD. */
#ifndef TEXT_H
#define TEXT_H

#include <stdint.h>

#define TXT_ROWS 25
#define TXT_COLS 40
#define HUD_ROW  22                 /* the HUD's rows: 22-24 (lines 176-199) */
#define TP_HUD   0xF0               /* txt_pal: the field's HUD */

extern uint8_t txt_pal[TXT_ROWS];   /* each row's look: PAL_* (scr.h), PAL_SEAT + seat, TP_HUD */
extern const uint8_t font8[];       /* the Namco font: 8 bytes a glyph, 20h-7Fh (gen/msdos/font.c) */

void txt_cell(uint8_t row, uint8_t col, uint8_t c, uint8_t inv);
void txt_blank(uint8_t row);        /* the row all its ground */
void txt_begin(void);               /* a text screen comes: the text's colours, the screen hidden if it can be */
void txt_show(void);                /* it is drawn: shown */

/* scr.c's: the text screens' cells, a text screen's number and whether the
 * field has been put up over it since (field.c's field_loading) */
extern uint8_t scr_ch[TXT_ROWS][TXT_COLS];
extern uint8_t scr_seq, field_up;
void scr_hud(uint8_t row, uint8_t col, const char *s, uint8_t hi);
void scr_hud_clear(uint8_t row);
void scr_redraw(uint8_t row);       /* a row drawn again, all of it */

/* lzd.asm: a title picture onto the screen at seg:0 (the menus') */
void lz_unpack(uint16_t seg, const uint8_t *src);
#pragma aux lz_unpack "*_" parm [ax] [dx] modify [ax bx cx dx es]

#endif
