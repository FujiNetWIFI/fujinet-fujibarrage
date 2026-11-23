/* fld.h -- the field engines' arguments (src/6809/dp.inc: the direct page
 * at $0E00) and entries: src/coco2/fld2.asm on the CoCo 1/2,
 * src/coco3/fld3.asm on the CoCo 3. The 6809 is big-endian: the words are
 * as C has them. */
#ifndef FLD_H
#define FLD_H

#include <stdint.h>

#define fx    (*(int16_t *)0x0E00)
#define fy    (*(int16_t *)0x0E02)
#define fx1   (*(int16_t *)0x0E04)
#define fv    (*(uint8_t *)0x0E06)
#define fold  (*(uint8_t *)0x0E07)
#define fent  (*(const uint8_t **)0x0E08)
#define ex    (*(int16_t *)0x0E0A)
#define ey    (*(int16_t *)0x0E0C)
#define erx   (*(uint8_t *)0x0E0E)
#define ery   (*(uint8_t *)0x0E0F)
#define emode (*(uint8_t *)0x0E10)
#define fswap (*(uint8_t *)0x0E38)      /* CoCo 1/2: the artifact phase swapped */

void fld_clear(void);
void fld_plot(void);
uint8_t fld_get(void);
void fld_swap(void);
void fld_hspan(void);
void fld_vrun(void);
uint8_t fld_col(void);
void fld_ellipse(void);
void fld_dots(void);
/* a box of whole bytes: lines fy.., bytes fx.., fx1 bytes a line, fv lines;
 * fold 0 kept at fent, 1 put back from it */
void fld_keep(void);
#ifdef COCO3
extern uint8_t rows[];                  /* a line's sky pattern, mask, dirt pattern, mask */
#else
void fld_phase(void);                   /* fswap changed: the tables */
#endif

#endif
