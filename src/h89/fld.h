/* fld.h -- the field's store and its primitives (fld.c) and the glue the
 * script player (src/z80) calls them by (fldglue.asm). The arguments are
 * variables, as src/tms9918/fld.h's. */
#ifndef FLD_H
#define FLD_H

#include <stdint.h>
#include "field.h"

extern int16_t fx, fy, fx1, ex, ey;
extern uint8_t fv, fold, erx, ery, emode, fld_ncols;
extern const uint8_t *fent;

extern uint8_t fld_store[FIELD_H][FIELD_W / 4]; /* 1 sky, 2 dirt, 3 fire: two bits a block */
extern uint8_t fld_cd[FIELD_H / 2][FIELD_W / 16];
extern uint8_t fld_lo[FIELD_H / 2], fld_hi[FIELD_H / 2];
extern uint8_t fld_any;
extern const uint8_t fld_glyph[16];

void fld_init(void);
void fld_clear(void);
void fld_mark_all(void);
void fld_mark_cell(uint8_t cx, uint8_t cy);
uint8_t fld_quad(uint8_t cx, uint8_t cy);       /* a cell's blocks as a character */

/* the primitives (C: the player reaches them through fldglue.asm) */
void c_fld_plot(void);
uint8_t c_fld_get(void);
void c_fld_swap(void);
void c_fld_hspan(void);
void c_fld_vrun(void);
uint8_t c_fld_col(void);
void c_fld_cols(void);
void c_fld_ellipse(void);

/* field.c */
void field_sync(void);          /* the dirty cells composed into the screen */
extern uint8_t field_up;
void field_down(void);          /* a text screen takes the screen */

#endif
