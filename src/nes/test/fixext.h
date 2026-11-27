/* fixext.h -- the NES's test scenes take their fixture from the rig
 * (tools/nessim: nesrun pokes it), not from the build (the Adam's): one TEST=1 build
 * serves every weapon's run. fix_ram is the field, the tanks and the
 * shot's segment count (tools/adamsim/fixture.py writes it); each segment
 * is asked for (test_hook[0] = 80h + its number) and the rig puts it in
 * script[] and seg_len, then test_hook[0] back to 0. */
#define FIX_EXTERN 1
#ifdef TEST_CATALOG
#define FIX_RAM 256                 /* (scene 10's build: no shot, room for the catalog) */
#else
#define FIX_RAM 512                 /* (the NES's RAM: the gate's fixtures are 299-339 bytes) */
#endif
extern uint8_t fix_ram[FIX_RAM];
#define FIX_SKY     fix_ram[0]
#define FIX_PAL     fix_ram[1]
#define FIX_NSEG    fix_ram[2]
#define fix_tx      ((const uint16_t *)(fix_ram + 3))
#define fix_ty      (fix_ram + 23)
#define fix_ta      (fix_ram + 33)
#define fix_terrain (fix_ram + 43)
