/*
 * field.c -- the NES's field screen (field.h): the store (src/nes/fld.s)
 * drawn by src/nes/render.s into the field's seven bands of tiles
 * (src/nes/crt0.s), the tanks as sprites, the HUD's three rows of text in
 * its own tiles under the gap.
 *
 * The round's look: four palettes, one a band of attribute rows down the
 * field, each the band's sky, its dirt and the fire; the backdrop (colour
 * 0) is the crust over every surface. The fire's colour moves on a band
 * every 8 frames while it burns.
 *
 * The tanks are 8x8 sprites in their seats' colours (a sprite palette's
 * three each: seats 0-2, 3-5, 6-8, 9), the foot at the sprite's bottom,
 * yours with your pennant. A line shows 8 sprites: yours and the turn's go
 * first, the rest in an order that turns each frame, so a ninth blinks.
 *
 * The screen's set-up, the HUD and the talk are src/nes/fieldo.c's (an
 * overlay: bank 7).
 */
#include <string.h>
#include "nes.h"
#include "game.h"
#include "field.h"
#include "plat.h"
#include "scr.h"
#include "fld.h"

#define PPUADDR (*(volatile uint8_t *)0x2006)
#define PPUDATA (*(volatile uint8_t *)0x2007)
#define PPUSTAT (*(volatile uint8_t *)0x2002)

#ifndef RENDER_MIN
#define RENDER_MIN 6            /* cells drawn each frame, however late */
#endif

uint8_t fld_flush1(void);       /* render.s */
void fld_mark_all(void);        /* fld.s */
extern uint8_t rnd_stars;
extern uint8_t bln_cells[21 * 4];
extern uint8_t dsp_shake;       /* the player's (pt.s) */

uint8_t fires[3];               /* the round's fire's three, cycled (fieldo.c sets them) */

uint8_t band_pal[16];            /* the round's palettes */
uint8_t drawn[SEATS];            /* field_tank's: shown */
uint8_t fire_on, fire_ph, fire_t;
uint8_t flash_n;
uint8_t field_shown;

/* ---- the store's calls ---- */

void field_init(void)
{
    fld_init();
    fld_clear();
}

void field_loading(void) {}

void field_column(uint16_t x, const uint8_t *entry)
{
    fx = x;
    fent = entry;
    fld_col();
}

void field_columns(uint16_t x, const uint8_t *entry, uint16_t n)
{
    while (n) {
        if (!((uint8_t)x & 7) && n >= 8) {      /* a cell's 8 columns at once */
            fx = x;
            fent = entry;
            if (!fld_cols8()) {
                x += 8;
                entry += 8;
                n -= 8;
                continue;
            }
        }
        fx = x++;
        fent = entry;
        entry += fld_col();
        --n;
    }
}

void field_plot(int16_t x, int16_t y, uint8_t v)
{
    fx = x;
    fy = y;
    fv = v;
    fld_plot();
}

uint8_t field_get(int16_t x, int16_t y)
{
    fx = x;
    fy = y;
    return fld_get();
}

void field_swap(int16_t x, int16_t y, uint8_t from, uint8_t to)
{
    fx = x;
    fy = y;
    fold = from;
    fv = to;
    fld_swap();
}

void field_span(int16_t y, int16_t x0, int16_t x1, uint8_t v)
{
    fy = y;
    fx = x0;
    fx1 = x1;
    fv = v;
    fld_hspan();
}

void field_ellipse(int16_t x, int16_t y, uint8_t rx, uint8_t ry, uint8_t v, uint8_t mode)
{
    ex = x;
    ey = y;
    erx = rx ? rx : 1;
    ery = ry > 200 ? 200 : ry ? ry : 1;
    fv = v;
    emode = mode;
    fld_ellipse();
}

/* ---- the screen ---- */

/* (each entry written once, with its value: the NMI may copy pal_bg any
 * time) */
void field_palettes(void)
{
    uint8_t i, v;
    for (i = 0; i < 16; ++i) {
        v = band_pal[i];
        if (flash_n)
            v = 0x30;
        else if (fire_on && (i & 3) == 3)
            v = fires[(uint8_t)(fire_ph + 3 - (i >> 2)) % 3];
        pal_bg[i] = v;
    }
}

/* the field's changed cells, while the game waits for a frame */
void field_idle(void)
{
    if (field_shown)
        fld_flush1();
}

void field_sprites(void);       /* pt.s: the tanks' and sparks' sprites */

/* a frame: some changed cells however late, the sprites, the colours */
void field_frame(void)
{
    uint8_t i;
    if (!field_shown)
        return;
    for (i = 0; i < RENDER_MIN && fld_flush1(); ++i)
        ;
    field_sprites();
    if (fire_on && ++fire_t >= 8) {
        fire_t = 0;
        fire_ph = (uint8_t)((fire_ph + 1) % 3);
    }
    if (flash_n)
        --flash_n;
    field_palettes();
    if (dsp_shake) {
        --dsp_shake;
        scroll_x = (dsp_shake & 2) ? 2 : 254;
    } else
        scroll_x = 0;
}

void field_tank(uint8_t seat, uint8_t draw)
{
    if (seat < SEATS)
        drawn[seat] = draw;
}

void field_colours(void) {}

void field_flash(uint8_t frames)
{
    flash_n = frames;
}

void field_fire(uint8_t on)
{
    fire_on = on;
}

