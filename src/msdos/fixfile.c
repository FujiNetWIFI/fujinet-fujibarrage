/*
 * fixfile.c -- a TEST=1 build's fixture, from files on the game's disk
 * (src/msdos/test/fixext.h; tools/dossim/fixture.py writes them):
 * FIX.BIN into fix_ram, SEGn.BIN into script[] as segment n plays.
 */
#ifdef TEST_SCENES
#include <fcntl.h>
#include <dos.h>
#include "game.h"
#include "msdos/test/fixext.h"

static uint16_t load(const char *name, uint8_t *to, uint16_t max)
{
    int fd;
    unsigned n = 0;
    if (_dos_open(name, O_RDONLY, &fd))
        return 0;
    _dos_read(fd, to, max, &n);
    _dos_close(fd);
    return (uint16_t)n;
}

void fix_load(void)
{
    load("FIX.BIN", fix_ram, FIX_RAM);
}

#ifdef TEST_CATALOG
void cat_load(void)                     /* (scene 10's: barraged -catalog) */
{
    cat_len = load("CAT.BIN", catalog, CAT_MAX);
}
#endif

void fix_seg(uint8_t n)
{
    static char name[] = "SEG00.BIN";
    name[3] = '0' + n / 10;
    name[4] = '0' + n % 10;
    seg_len = load(name, script, SCRIPT_BUF);
}
#endif
