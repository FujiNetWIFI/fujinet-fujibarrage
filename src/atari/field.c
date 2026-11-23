/*
 * field.c -- the field (field.h) on the Atari: fld.s draws the pixels,
 * dlc.c colours the sky, the dirt and the tanks.
 *
 * A tank is a 7x5 body drawn in pixel value 0 (black: a player's colour
 * shows through it) with its barrel, 4 pixels from the turret, also 0. Its
 * balloon is a black box with the phrase in COLPF2 (white while it shows),
 * the pixels under it kept and put back.
 */
#if defined(__ATARI__) || defined(__ATARI5200__)

#include <string.h>
#include "atari.h"
#include "game.h"
#include "field.h"
#include "plat.h"
#include "font35.h"

/* fld.s */
extern int16_t fx, fy, fx1;
extern uint8_t fv, fold;
extern const uint8_t *fent;
extern int16_t ex, ey;
extern uint8_t erx, ery, emode;
#pragma zpsym ("ex")
#pragma zpsym ("ey")
#pragma zpsym ("erx")
#pragma zpsym ("ery")
#pragma zpsym ("emode")
void fld_ellipse(void);
#pragma zpsym ("fx")
#pragma zpsym ("fy")
#pragma zpsym ("fx1")
#pragma zpsym ("fv")
#pragma zpsym ("fold")
#pragma zpsym ("fent")
void fld_init(void);
void fld_clear(void);
void fld_plot(void);
uint8_t fld_get(void);
void fld_swap(void);
void fld_hspan(void);
uint8_t fld_col(void);
extern uint8_t rowlo[], rowhi[];

/* dsp.s, dlc.c */
extern uint8_t dsp_dma, dsp_mode, dsp_flash, fx_col, fx_cycle, st_bk, st_c1, st_c2;
void __fastcall__ dlc_palette(uint16_t sky_pal);    /* (dlc.s) */
uint8_t hue(uint8_t c);                             /* (scr.c) */
void dlc_build(void);

#define STATUS ((uint8_t *)STATUS_ADDR)

/* the body: rows top to bottom, bit 6 the left pixel */
static const uint8_t body[TANK_H] = { 0x1C, 0x3E, 0x7F, 0x7F, 0x2A };
/* cos of 0, 5, ... 90 degrees, x64 */
static const uint8_t cos64[19] = { 64, 64, 63, 62, 60, 58, 55, 52, 49, 45, 41, 37, 32, 27, 22, 17, 11, 6, 0 };

#pragma bss-name (push, "HIGH")
static int16_t drawn_x[SEATS];
static uint8_t drawn_y[SEATS], drawn_a[SEATS], drawn_on[SEATS];
#pragma bss-name (pop)

/* at start-up (scr_init): after that the field is only ever written by
 * TERRAIN and SETTLE columns (and a text screen borrows its top) */
void field_init(void)
{
    fld_init();
    fld_clear();
}

/* a text screen borrows the field's memory: it goes dark while the field
 * is written over it */
void field_loading(void)
{
    if (!dsp_mode)
        dsp_dma = 0;
}

void field_open(uint8_t sky, uint8_t pal)
{
    dsp_dma = 0;
    frame_wait();
    memset(STATUS, 0, 120);
    memset(drawn_on, 0, sizeof(drawn_on));
    st_bk = 0x00;
    st_c1 = 0x0E;
    st_c2 = hue(0x92);
    fx_col = 0x0E;
    fx_cycle = 0;
    dlc_palette(sky | pal << 8);
    dlc_build();
    dsp_mode = 1;
    dsp_dma = 0x3E;             /* normal width, players and missiles, one line */
}

void field_column(uint16_t x, const uint8_t *entry)
{
    fx = x;
    fent = entry;
    fld_col();
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

/* the barrel's k-th pixel from the turret at angle a */
static void barrel(int16_t x, int16_t y, uint8_t a, uint8_t k, int16_t *bx, int16_t *by)
{
    uint8_t i = a <= 90 ? a / 5 : (180 - a) / 5;
    int16_t dx = (int16_t)(k * cos64[i] + 32) >> 6;
    int16_t dy = (int16_t)(k * cos64[18 - i] * 3 / 2 + 32) >> 6;    /* lines are narrow */
    *bx = a <= 90 ? x + dx : x - dx;
    *by = y - TANK_H - 1 - dy + (dy > 0 ? 0 : 0);
}

static void sprite(int16_t x, uint8_t y, uint8_t a, uint8_t draw)
{
    uint8_t r, c, bits;
    int16_t bx, by;
    for (r = 0; r < TANK_H; ++r) {
        bits = body[r];
        for (c = 0; c < 7; ++c, bits <<= 1)
            if (bits & 0x40) {
                if (draw)
                    field_plot(x - 3 + c, y - TANK_H + r, PX_TANK);
                else
                    field_swap(x - 3 + c, y - TANK_H + r, PX_TANK, PX_SKY);
            }
    }
    for (r = 1; r <= 4; ++r) {
        barrel(x, y, a, r, &bx, &by);
        if (draw)
            field_plot(bx, by + 1, PX_TANK);
        else
            field_swap(bx, by + 1, PX_TANK, PX_SKY);
    }
}

void field_tank(uint8_t s, uint8_t draw)
{
    if (drawn_on[s]) {
        sprite(drawn_x[s], drawn_y[s], drawn_a[s], 0);
        drawn_on[s] = 0;
    }
    if (draw && tk[s].on) {
        sprite(tk[s].x, tk[s].y, tk[s].angle, 1);
        drawn_x[s] = tk[s].x;
        drawn_y[s] = tk[s].y;
        drawn_a[s] = tk[s].angle;
        drawn_on[s] = 1;
    }
}

void field_colours(void)
{
    dlc_build();
}

/* ---- the balloon --------------------------------------------------------- */

/* bln.s draws it (and keeps what it covers) */
extern uint8_t keeping, bl_s, bl_n;
extern const char *bl_t;
void balloon(void);

void field_balloon(uint8_t s, const char *t, uint8_t n)
{
    bl_s = s;
    bl_t = t;
    bl_n = n;
    balloon();
}

/* ---- the status rows ----------------------------------------------------- */

void field_status(uint8_t row, uint8_t col, const char *s)
{
    uint8_t *d = STATUS + row * 40 + col;
    uint8_t c;
    while ((c = (uint8_t)*s++) != 0 && d < STATUS + 120) {
        if (c >= 'a' && c <= 'z')
            c -= 32;
        *d++ = c < 32 ? 0 : c < 96 ? c - 32 : c;   /* ASCII to screen code */
    }
}

/* in inverse video (the status rows are ANTIC mode 2: bit 7) */
void field_status_hi(uint8_t row, uint8_t col, const char *s)
{
    uint8_t *d = STATUS + row * 40 + col;
    field_status(row, col, s);
    while (*s++)
        *d++ |= 0x80;
}

void field_status_clear(void)
{
    memset(STATUS, 0, 120);
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

void field_flash(uint8_t frames)
{
    dsp_flash = frames;
}

void field_fire(uint8_t on)
{
    if (!keeping) {
        fx_cycle = on;
        if (!on)
            fx_col = 0x0E;
    }
}

#endif
