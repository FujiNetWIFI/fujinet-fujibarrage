/* fld.h -- the field engine's arguments and entries (each build's
 * fld.asm: src/msdos/cga, src/msdos/vga); the script player (src/8086)
 * uses the same variables. Every entry keeps SI, DI, BP. */
#ifndef FLD_H
#define FLD_H

#include <stdint.h>

extern int16_t fx, fy, fx1, ex, ey;
extern uint8_t fv, fold, erx, ery, emode, fld_ncols;
extern const uint8_t *fent;

#pragma aux FLDCALL "*_" parm [] modify [ax bx cx dx es]
void fld_tables(void);          /* at start-up */
void fld_clear(void);           /* all sky */
void fld_plot(void);
uint8_t fld_get(void);
void fld_swap(void);
void fld_hspan(void);
void fld_vrun(void);
uint8_t fld_col(void);
void fld_cols(void);
void fld_ellipse(void);
/* a shape list at (fx, fy): fent its count, then (dx, dy) pairs; fold FFh
 * each pixel fv (the CGA: a value; the VGA: a colour), else each pixel
 * that is fold becomes fv */
void fld_dots(void);
/* a box: lines fy.. (fv of them), from fx (the CGA: a byte; the VGA: a
 * pixel), fx1 a line; fold 0 kept at fent, else put back from there */
void fld_keep(void);
#pragma aux (FLDCALL) fld_tables
#pragma aux (FLDCALL) fld_clear
#pragma aux (FLDCALL) fld_plot
#pragma aux (FLDCALL) fld_get value [al]
#pragma aux (FLDCALL) fld_swap
#pragma aux (FLDCALL) fld_hspan
#pragma aux (FLDCALL) fld_vrun
#pragma aux (FLDCALL) fld_col value [al]
#pragma aux (FLDCALL) fld_cols
#pragma aux (FLDCALL) fld_ellipse
#pragma aux (FLDCALL) fld_dots
#pragma aux (FLDCALL) fld_keep

#endif
