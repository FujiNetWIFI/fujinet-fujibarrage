/* fld.h -- the field engine's arguments and entries (fld.asm, render.asm):
 * variables, little-endian as C has them. The player (src/z80) uses the
 * same names. */
#ifndef FLD_H
#define FLD_H

#include <stdint.h>

extern int16_t fx, fy, fx1, ex, ey;
extern uint8_t fv, fold, erx, ery, emode;
extern const uint8_t *fent;
extern uint8_t fld_live;        /* the field is shown: changes mark cells dirty */
extern uint8_t fld_dirty[21 * 32];      /* a cell's flags: 1 dirty, 2 the balloon's, 4 a tank in it */
extern uint8_t fld_rowd[21];
extern uint16_t fld_nd;
extern uint8_t fld_ncols, fld_budget, rnd_fast;
extern uint8_t fld_shadow[];

void fld_clear(void);
void fld_mark_all(void);
void fld_plot(void);
uint8_t fld_get(void);
void fld_swap(void);
void fld_hspan(void);
void fld_vrun(void);
uint8_t fld_col(void);
void fld_cols(void);
void fld_ellipse(void);

/* render.asm: the line styles (field.c builds them), the cell being drawn */
extern uint8_t st_line[168 * 5];          /* a line's styles: style.inc */
extern uint8_t st_fire[8], st_stars, fire_ph;
extern uint8_t rc_pat[8], rc_col[8], rc_y;
extern uint16_t rc_cell;        /* H the row, L the column * 8 */
uint8_t fld_flush1(void);       /* a dirty cell drawn: 0 none */
void fld_flushn(void);          /* up to fld_budget of them */

#define CF_DIRTY 1
#define CF_COVER 2
#define CF_TANK  4

#endif
