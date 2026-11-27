/* fld.h -- the NES's field store (src/nes/fld.s): its arguments in zero
 * page (the Atari's names), its entries, and what the renderer reads. */
#ifndef NES_FLD_H
#define NES_FLD_H

#include <stdint.h>

extern int16_t fx, fy, fx1;
extern uint8_t fv, fold;
extern const uint8_t *fent;
extern int16_t ex, ey;
extern uint8_t erx, ery, emode;
#pragma zpsym ("fx")
#pragma zpsym ("fy")
#pragma zpsym ("fx1")
#pragma zpsym ("fv")
#pragma zpsym ("fold")
#pragma zpsym ("fent")
#pragma zpsym ("ex")
#pragma zpsym ("ey")
#pragma zpsym ("erx")
#pragma zpsym ("ery")
#pragma zpsym ("emode")

void fld_init(void);
void fld_clear(void);
void fld_plot(void);
uint8_t fld_get(void);
void fld_swap(void);
void fld_hspan(void);
void fld_vrun(void);
uint8_t fld_col(void);
uint8_t fld_cols8(void);        /* columns fx..fx+7 (fx % 8 == 0), 8 plain entries: 1 if one has spans */
void fld_ellipse(void);

extern uint8_t fmap[21 * 32];           /* a cell's code: 1 sky, 2 dirt, 3 fire, else a block */
extern uint8_t fld_dirty[21 * 4];       /* a bit a cell: changed */
extern uint8_t fld_rowd[21];
extern uint16_t fld_nd, fld_lossy;
extern uint8_t fld_live, fld_blocks;

#endif
