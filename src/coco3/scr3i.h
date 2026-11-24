/* scr3i.h -- the CoCo 3's text, inside: what src/coco3/scr3.c and
 * src/coco3/scr3k.c (the text in colour, in the art block) share */
#ifndef SCR3I_H
#define SCR3I_H

#include <stdint.h>
#include "scr.h"

#define ROWS (SCR_ROWS + 1)                 /* (the 25th: the HUD's last) */
extern uint8_t scr_ch[ROWS][SCR_COLS];      /* (src/coco/hibss.asm) */
extern uint8_t scr_inv[ROWS][5];            /* cells shown inverse */
extern uint8_t scr3_rink[ROWS], scr3_rground[ROWS];    /* each row's ink and ground */
extern uint16_t scr3_tab[16], scr3_itab[16];           /* the row's tables, and inverse */
extern uint8_t scr3_nmc[3], scr3_nmn[3], scr3_nms[3];  /* the HUD rows' names */

/* a label (bit 7, scr.h) in seat 3's yellow on a row of white ink (on the
 * field's palette too: the HUD) */
#define C3_LABEL (C3_SEAT + 3)

void scr3_run(uint8_t row, uint8_t col, const uint8_t *chars, uint8_t n, const uint16_t *tab,
              const uint16_t *ltab);                    /* scr3a.asm */
void scr3_cells(uint8_t row, uint8_t col, uint8_t n);  /* scr3.c */
void scr3_tabfill(uint16_t *t, uint8_t ink, uint8_t ground);   /* scr3k.c */
const uint16_t *scr3_tabfor(uint8_t row, uint8_t ink);
void scr3_cellk(uint8_t row, uint8_t col, uint8_t n);

#endif
