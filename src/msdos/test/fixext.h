/* fixext.h -- the PC's test scenes take their fixture from files on the
 * disk (tools/dossim/fixture.py writes them; dossim and the 86Box rig put
 * them on the game's disk): one TEST=1 build serves every weapon's run.
 * FIX.BIN goes into fix_ram (the field, the tanks, the shot's segment
 * count; the Adam's layout, src/adam/test/fixext.h); SEGn.BIN into
 * script[] and seg_len as each segment is played (src/msdos/fixfile.c). */
#define FIX_EXTERN 1
#define FIX_FILES 1
#define FIX_RAM 4096
extern uint8_t fix_ram[FIX_RAM];
#define FIX_SKY     fix_ram[0]
#define FIX_PAL     fix_ram[1]
#define FIX_NSEG    fix_ram[2]
#define fix_tx      ((const uint16_t *)(fix_ram + 3))
#define fix_ty      (fix_ram + 23)
#define fix_ta      (fix_ram + 33)
#define fix_terrain (fix_ram + 43)
void fix_load(void);
void fix_seg(uint8_t n);
void cat_load(void);                /* (TEST_CATALOG: CAT.BIN into catalog[]) */
