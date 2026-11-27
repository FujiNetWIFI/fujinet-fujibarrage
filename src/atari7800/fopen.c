/*
 * fopen.c -- the 7800's field screen set up (src/atari/field.c's
 * field_open: then src/atari7800/fieldm.c's m_field_show): the screen
 * blank, the HUD's colours, each zone's sky and dirt (the Atari's gradients,
 * src/atari/dlc.s, a step a zone), the seats' colours. Bank 5's (FOPEN),
 * through its far wrapper (tools/atari7800/far.txt).
 */
#ifdef __ATARI7800__

#include "a78.h"
#include "game.h"
#include "field.h"
#include "proto.h"

extern const uint8_t seat_col[10];          /* scr.c */

/* the Atari's gradients (src/atari/dlc.s), 16 bands: zone z is band zb[z] */
static const uint8_t sunset[16] = { 0x52, 0x54, 0x44, 0x44, 0x46, 0x36, 0x38, 0x28, 0x2A, 0x2A, 0x1C, 0x1C,
                                    0x1E, 0x1E, 0x1E, 0x1E };
static const uint8_t dirt_hue[8] = { 0xF0, 0xC0, 0x00, 0x30, 0x20, 0x90, 0x60, 0xE0 };  /* (4: the 800's $10, olive on the 7800 as 0 is) */
static const uint8_t dirt_l[16] = { 12, 12, 11, 10, 10, 9, 8, 8, 7, 6, 6, 5, 4, 4, 3, 2 };
static const uint8_t zb[NZONES] = { 0, 0, 1, 2, 3, 3, 4, 5, 6, 6, 7, 8, 9, 9, 10, 11, 12, 12, 13, 14, 15 };

static uint8_t sky_of(uint8_t sky, uint8_t j)
{
    switch (sky) {
    case SKY_PLAIN:
        return 0x96;
    case SKY_STARS:
        return j < 8 ? 0 : (j - 8) | 0x80;
    case SKY_STORM:
        return (j >> 1) + 2;
    case SKY_SUNSET:
        return sunset[j];
    case SKY_CAVERN:
        return j < 8 ? 0xF0 : 0xF2;
    case SKY_BLACK:
        return 0;
    }
    ++j;                                    /* the gradient: night blue to day */
    return j < 9 ? j | 0x80 : (j > 15 ? 15 : j) | 0x90;
}

void m_field_open(uint8_t sky, uint8_t pal)
{
    uint8_t z, r, s;
    m_show(M_BLANK);
    for (r = NTROWS; r < NTROWS + NHUD; ++r) {
        r_bg[r] = hue(0x92);
        r_ink[r] = 0x0E;
        r_lab[r] = hue(0x1C);
        r_nam[r] = 0x0E;
    }
    for (z = 0; z < NZONES; ++z) {
        zb_sky[z] = z_sky[z] = hue(sky_of(sky, zb[z]));
        zb_dirt[z] = z_dirt[z] = hue(dirt_hue[pal & 7] | dirt_l[zb[z]]);
    }
    for (s = 0; s < SEATS; ++s)
        m_seat[s] = hue(seat_col[s]);
    m_seatnew = 1;
    fx_col = 0x0E;
    fx_cycle = 0;
}

#endif
