/*
 * text.c -- the CGA build's text cells (text.h): a glyph's line is 8
 * pixels, 2 bytes, each of its 4-pixel halves a byte through a 16-entry
 * table for the row's ink and ground (scra.asm cga_cell). The text
 * screens' colours are palette 1 bright on blue: 0 blue, 1 light cyan,
 * 2 light magenta, 3 white. The HUD's rows are the field's colours: sky
 * (0) letters on a band of dirt (3), the ground going on under the field.
 */
#include <string.h>
#include <i86.h>
#include "scr.h"
#include "plat.h"
#include "text.h"
#include "cga.h"

#define T_BG 1                      /* the text screens' background: blue */

/* TEXT, TITLE, HEAD, HILITE, DIM: ink and ground among 0-3 */
static const uint8_t pal_ink[] = { 3, 3, 0, 0, 1 };
static const uint8_t pal_ground[] = { 0, 2, 1, 3, 0 };

static uint8_t tab[16], tab_ink = 0xFF, tab_ground = 0xFF;

static void make(uint8_t ink, uint8_t ground)
{
    uint8_t n, x, b;
    if (ink == tab_ink && ground == tab_ground)
        return;
    tab_ink = ink;
    tab_ground = ground;
    for (n = 0; n < 16; ++n) {
        b = 0;
        for (x = 0; x < 4; ++x)
            b = (uint8_t)(b << 2 | (n & (8 >> x) ? ink : ground));
        tab[n] = b;
    }
}

static void colours(uint8_t row, uint8_t *ink, uint8_t *ground)
{
    uint8_t p = txt_pal[row];
    if (p == TP_HUD) {
        *ink = 0;
        *ground = 3;
    } else if (p >= PAL_SEAT) {
        *ink = 0;
        *ground = (uint8_t)(1 + (p - PAL_SEAT) % 3);
    } else {
        *ink = pal_ink[p];
        *ground = pal_ground[p];
    }
}

void txt_cell(uint8_t row, uint8_t col, uint8_t c, uint8_t inv)
{
    uint8_t ink, ground;
    colours(row, &ink, &ground);
    if (inv)
        make(ground, ink);
    else
        make(ink, ground);
    if (c < 32 || c > 127)
        c = ' ';
    cga_cell((uint16_t)row * 320 + col * 2, font8 + (c - 32) * 8, tab);
}

void txt_blank(uint8_t row)
{
    uint8_t ink, ground, k;
    colours(row, &ink, &ground);
    for (k = 0; k < 8; ++k)
        _fmemset(MK_FP(0xB800, (uint16_t)row * 320 + (k >> 1) * 80 + (k & 1) * 0x2000),
                 ground * 0x55, 80);
}

void txt_begin(void)
{
    cga_palette(T_BG, 1, 1);
}

void txt_show(void)
{
}

#ifdef MENU
/* the title (menus.c): tools/msdos/mktitle_dos.py's picture, palette 0
 * bright on blue; ESC: LOBBY is in it */
extern const uint8_t title_cga[];

void scr_picture(void)
{
    uint8_t r;
    frame_wait();
    cga_palette(1, 0, 1);
    lz_unpack(0xB800, title_cga);
    for (r = 0; r < TXT_ROWS; ++r)
        txt_pal[r] = 0xFF;              /* (the rows aren't text now) */
}
#endif
