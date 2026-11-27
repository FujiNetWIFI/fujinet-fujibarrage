/*
 * title.c -- the 7800's title (scr.h's scr_picture, menus.c's title): a
 * field scene, as the NES's and the Channel F's: a sunset over the hills
 * (gen/atari7800/title.s), FUJIBARRAGE in fire letters twice as tall as
 * the font's (gen/atari7800/font.s, REPL's: MARIA's own text font), a
 * shell's arc from one tank to the other and its burst; the words on the
 * HUD's rows. The menus' bank's (bank 2: MENUCODE, its data MENURO).
 */
#ifdef __ATARI7800__

#include "game.h"
#include "field.h"
#include "scr.h"
#include "layout.h"
#include "proto.h"

extern const uint8_t title_hills[160];
extern const uint8_t font_a78[];        /* font.s: glyph line l at + (7 - l) * 256 */
extern int16_t fx, fy, fx1;
extern uint8_t fv;
#pragma zpsym ("fx")
#pragma zpsym ("fy")
#pragma zpsym ("fx1")
#pragma zpsym ("fv")
void fld_clear(void);
void fld_vrun(void);

/* a letter as tall again as the font's (8 x 16), in fire */
static void big(uint8_t x, uint8_t y, char ch)
{
    uint8_t i, j, b;
    for (i = 0; i < 7; ++i) {
        b = font_a78[(7 - i) * 256 + (uint8_t)(ch - ' ')];
        for (j = 0; j < 8; ++j, b <<= 1)
            if (b & 0x80) {
                field_plot(x + j, y + i * 2, PX_FX);
                field_plot(x + j, y + i * 2 + 1, PX_FX);
            }
    }
}

void scr_picture(void)
{
    static const char name[] = "FUJIBARRAGE";
    uint8_t i, x;
    int16_t y, k;
    fld_clear();
    for (x = 0; x < 160; ++x) {
        fx = x;
        fy = title_hills[x];
        fx1 = FIELD_H;
        fv = PX_DIRT;
        fld_vrun();
    }
    for (i = 0; i < 11; ++i)
        big((uint8_t)(31 + i * 9), 18, name[i]);
    for (k = 0; k <= 96; k += 2) {      /* a parabola from one tank to the other */
        x = (uint8_t)(32 + k);
        y = 56 + ((k - 48) * (k - 48)) / 40;
        if (y < (int16_t)title_hills[x] - 6)
            field_plot(x, y, PX_FX);
    }
    field_ellipse(132, (int16_t)title_hills[132] - 7, 8, 9, PX_FX, 0);
    field_open(SKY_SUNSET, 4);
    tk[0].on = tk[1].on = 1;
    tk[0].x = 28;
    tk[0].y = title_hills[28];
    tk[0].angle = 60;
    tk[1].x = 136;
    tk[1].y = title_hills[136];
    tk[1].angle = 120;
    field_tank(0, 1);
    field_tank(1, 1);
    field_fire(1);
    field_status(0, 2, TXT_TAGLINE);
    field_status(1, (SCR_COLS - sizeof(TXT_PRESS) + 1) / 2, TXT_PRESS);
#ifdef QUIT_HINT
    field_status(2, (SCR_COLS - sizeof(QUIT_HINT) + 1) / 2, QUIT_HINT);
#endif
}

#endif
