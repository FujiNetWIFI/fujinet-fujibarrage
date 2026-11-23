/* fldlook.h -- the CGA build's field look for src/msdos/field.c: fld_dots
 * draws values here, and a tank is one silhouette of value 0 (tank). */
#define TK_ONE      1               /* the tank in one colour: tk_all */
#define TK_COL(s)   PX_TANK
#define TK_FLAGCOL  PX_TANK
#define BAL_INK     PX_SKY          /* the balloon's letters (its box: tank) */
#define KEEP_PPB    4               /* fld_keep's unit: a byte, 4 pixels */
#define KEEP_SIZE   (18 * 80)
