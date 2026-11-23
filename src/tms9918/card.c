/*
 * card.c -- M0's test card (make adam VIDEO_CARD=1, src/adam/plat.c): the
 * colours, a circle for the pixel aspect, five sprites on a line, the clock.
 */
#include <string.h>
#include "tms.h"
#include "tmsgfx.h"
#include "plat.h"

#ifdef VIDEO_CARD
/* M0's test card: the 15 colours as bars (top third), a circle 64 pixels
 * across by 64 lines (the pixel aspect: on a 4:3 picture it is round when
 * PAR is 64/64ths... it looks 1.14 times wider at PAR 73), five sprites on
 * one line (the fifth must vanish), and the clock: frame_count's bytes as
 * the bits of two cells. */
void tms_card(void)
{
    static uint8_t pat[8], col[8], spr[8], sat[21];
    uint8_t c, i, x, y;
    int16_t dx, dy;
    /* bars: 2 cells a colour, 8 rows */
    for (c = 0; c < 16; ++c)
        tms_cell_fill(c * 2, 2, 0xFF, (uint8_t)(c << 4 | C_BLACK));
    for (c = 0; c < 16; ++c)
        for (i = 1; i < 8; ++i)
            tms_cell_fill(i * 32 + c * 2, 2, 0xFF, (uint8_t)(c << 4 | C_BLACK));
    /* the circle: radius 32 (dy) and 32 (dx), centred at (128, 112), white on dark blue */
    tms_cell_fill(256, 512, 0, C_WHITE << 4 | C_DBLUE);
    for (y = 80; y < 145; ++y) {
        for (c = 12; c < 20; ++c) {
            uint8_t b = 0;
            for (i = 0; i < 8; ++i) {
                dx = (int16_t)(c * 8 + i) - 128;
                dy = (int16_t)y - 112;
                if (dx * dx + dy * dy <= 32 * 32)
                    b |= 0x80 >> i;
            }
            pat[0] = b;
            va_addr = VR_PAT + ((uint16_t)(y >> 3) * 32 + c) * 8 + (y & 7);
            va_src = pat;
            va_len = 1;
            va_writef();
        }
    }
    /* five sprites on line 160 (y-1 in the SAT) */
    for (i = 0; i < 8; ++i)
        spr[i] = 0xFF;
    vput(VR_SPAT, spr, 8);
    for (i = 0; i < 5; ++i) {
        sat[i * 4] = 159;
        sat[i * 4 + 1] = 40 + i * 40;
        sat[i * 4 + 2] = 0;
        sat[i * 4 + 3] = 2 + i * 2;
    }
    sat[20] = 0xD0;
    vput(VR_SAT, sat, 21);
    tms_blank(0);
    for (;;) {
        x = (uint8_t)frame_count;
        y = (uint8_t)(frame_count >> 8);
        for (i = 0; i < 8; ++i) {
            pat[i] = x;
            col[i] = C_LYELLOW << 4 | C_DRED;
        }
        tms_cell(21 * 32 + 1, pat, col);
        for (i = 0; i < 8; ++i)
            pat[i] = y;
        tms_cell(21 * 32 + 2, pat, col);
        frame_wait();
    }
}
#endif
