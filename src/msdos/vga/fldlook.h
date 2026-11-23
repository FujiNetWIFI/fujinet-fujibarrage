/* fldlook.h -- the VGA build's field look for src/msdos/field.c: fld_dots
 * draws colours here (vga.h): a tank is its seat's body, black treads, a
 * white highlight. */
#include "vga.h"
#define TK_COL(s)   ((uint8_t)(V_SEAT + (s) * 3))
#define TK_DARKCOL  0
#define TK_LITECOL  (V_SEAT + 2)    /* (seat 0's highlight: white, the tank class) */
#define TK_FLAGCOL  (V_SEAT + 2)
#define BAL_INK     (V_SEAT + 2)
#define KEEP_PPB    1               /* fld_keep's unit: a pixel */
#define KEEP_SIZE   (18 * 320)
