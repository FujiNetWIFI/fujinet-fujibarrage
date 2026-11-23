/* fixext.h -- the Adam's test scenes take their fixture from the rig
 * (tools/adamsim: adamrun pokes it), not from the build: one TEST=1 build
 * serves every weapon's run. fix_ram is the field, the tanks and the
 * shot's segment count (tools/adamsim/fixture.py writes it); each segment
 * is asked for (test_hook[0] = 80h + its number) and the rig puts it in
 * script[] and seg_len, then test_hook[0] back to 0. */
#define FIX_EXTERN 1
#define FIX_RAM 2048
extern uint8_t fix_ram[FIX_RAM];
#define FIX_SKY     fix_ram[0]
#define FIX_PAL     fix_ram[1]
#define FIX_NSEG    fix_ram[2]
#define fix_tx      ((const uint16_t *)(fix_ram + 3))
#define fix_ty      (fix_ram + 23)
#define fix_ta      (fix_ram + 33)
#define fix_terrain (fix_ram + 43)
