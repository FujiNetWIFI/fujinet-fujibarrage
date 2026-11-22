/*
 * field.c -- the field (field.h) on the Apple II: HGR page 1 is the field
 * itself (src/apple2/fld.s), mixed mode's four text rows its HUD.
 *
 * HGR has no sprites, so the tanks are in the bitmap as the Atari's are:
 * pixel value 0, black. Yours carries a black pennant (tank pixels too);
 * the turn's tank, while it aims, has a white arrow over it that blinks,
 * drawn as fire over sky and taken away before any script plays (so the
 * script never meets it). A tank's talk is the HUD's fourth row.
 */
#ifdef __APPLE2__

#include <string.h>
#include "game.h"
#include "field.h"
#include "plat.h"

/* fld.s */
extern int16_t fx, fy, fx1;
extern uint8_t fv, fold, fpal;
extern const uint8_t *fent;
extern int16_t ex, ey;
extern uint8_t erx, ery, emode;
#pragma zpsym ("fx")
#pragma zpsym ("fy")
#pragma zpsym ("fx1")
#pragma zpsym ("fv")
#pragma zpsym ("fold")
#pragma zpsym ("fpal")
#pragma zpsym ("fent")
#pragma zpsym ("ex")
#pragma zpsym ("ey")
#pragma zpsym ("erx")
#pragma zpsym ("ery")
#pragma zpsym ("emode")
void fld_init(void);
void fld_clear(void);
void fld_pal(void);
void fld_plot(void);
uint8_t fld_get(void);
void fld_swap(void);
void fld_hspan(void);
uint8_t fld_col(void);
void fld_ellipse(void);
void fld_dots(void);

/* lowlevel.s, scr.c */
extern uint8_t flashn;
void hgr_on(void);
void scr_hud(uint8_t row, uint8_t col, const char *s);
void scr_hud_clear(uint8_t row);
void scr_for_field(void);

/* a tank, as fld_dots draws it: (dx, dy) from its foot (x, y), the line
 * under it. The body, 5 by 5: */
#define BODY_N 17
static const int8_t body[BODY_N * 2] = {
                     0, -5,
             -1, -4, 0, -4, 1, -4,
     -2, -3, -1, -3, 0, -3, 1, -3, 2, -3,
     -2, -2, -1, -2, 0, -2, 1, -2, 2, -2,
     -2, -1,         0, -1,         2, -1,
};
/* the barrel from the turret's top, by angle (0, 5, ... 90 degrees; past
 * 90 mirrored): 3 pixels long, its height in lines x 117/64 (PAR), a
 * point a pixel or a line; the last repeated, to make 5) */
#define BARREL_N 5
static const int8_t barrel_pts[19][BARREL_N * 2] = {
    { 1, 0, 2, 0, 3, 0, 3, 0, 3, 0 },   /*  0 */
    { 1, 0, 2, 0, 3, 0, 3, 0, 3, 0 },   /*  5 */
    { 1, 0, 2, -1, 3, -1, 3, -1, 3, -1 },   /* 10 */
    { 1, 0, 2, -1, 3, -1, 3, -1, 3, -1 },   /* 15 */
    { 1, -1, 2, -1, 3, -2, 3, -2, 3, -2 },   /* 20 */
    { 1, -1, 2, -2, 3, -2, 3, -2, 3, -2 },   /* 25 */
    { 1, -1, 2, -2, 3, -3, 3, -3, 3, -3 },   /* 30 */
    { 1, -1, 2, -2, 2, -3, 2, -3, 2, -3 },   /* 35 */
    { 1, -1, 1, -2, 2, -3, 2, -4, 2, -4 },   /* 40 */
    { 1, -1, 1, -2, 2, -3, 2, -4, 2, -4 },   /* 45 */
    { 0, -1, 1, -2, 1, -3, 2, -4, 2, -4 },   /* 50 */
    { 0, -1, 1, -2, 1, -3, 2, -4, 2, -4 },   /* 55 */
    { 0, -1, 1, -2, 1, -3, 1, -4, 2, -5 },   /* 60 */
    { 0, -1, 1, -2, 1, -3, 1, -4, 1, -5 },   /* 65 */
    { 0, -1, 0, -2, 1, -3, 1, -4, 1, -5 },   /* 70 */
    { 0, -1, 0, -2, 0, -3, 1, -4, 1, -5 },   /* 75 */
    { 0, -1, 0, -2, 0, -3, 0, -4, 1, -5 },   /* 80 */
    { 0, -1, 0, -2, 0, -3, 0, -4, 0, -5 },   /* 85 */
    { 0, -1, 0, -2, 0, -3, 0, -4, 0, -5 },   /* 90 */
};
/* yours: a pennant at the back */
#define PENNANT_N 4
static const int8_t pennant[PENNANT_N * 2] = { -2, -6, -2, -7, -2, -8, -1, -8 };
/* the round's palette by its sky (SKY_*): $80 blue over orange; 0 violet
 * over green (stars, storm, sunset) */
static const uint8_t sky_pal[8] = { 0x80, 0x80, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80 };

static int16_t drawn_x[SEATS];
static uint8_t drawn_y[SEATS], drawn_a[SEATS], drawn_on[SEATS], drawn_me[SEATS];

void field_init(void)
{
    fld_init();
    fld_clear();
}

/* text page and HGR page are apart: nothing to blank */
void field_loading(void)
{
}

void field_open(uint8_t sky, uint8_t pal)
{
    uint8_t p = sky_pal[sky & 7];
    (void)pal;
    if (p != fpal) {            /* the page in the round's palette (the terrain may
                                 * have come before: it was drawn in the last one) */
        fpal = p;
        fld_pal();
    }
    memset(drawn_on, 0, sizeof(drawn_on));
    field_turn(NONE);
    scr_for_field();
    frame_wait();
    hgr_on();
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

static uint8_t shape[1 + (BODY_N + BARREL_N + PENNANT_N) * 2];
static uint8_t shape_n;

static void add(const int8_t *pts, uint8_t n)
{
    memcpy(shape + 1 + shape_n, pts, n);
    shape_n += n;
}

static void add_barrel(uint8_t a)
{
    const int8_t *b = barrel_pts[a <= 90 ? a / 5 : (180 - a) / 5];
    uint8_t i;
    for (i = 0; i < BARREL_N * 2; i += 2) {
        shape[1 + shape_n++] = a <= 90 ? b[i] : -b[i];
        shape[1 + shape_n++] = b[i + 1] - TANK_H;
    }
}

/* the shape built at (x, y): drawn (tank pixels), or taken off (its tank
 * pixels back to sky); the clock ticked after */
static void dots(int16_t x, uint8_t y, uint8_t draw)
{
    shape[0] = shape_n >> 1;
    fx = x;
    fy = y;
    fent = shape;
    if (draw) {
        fold = 0xFF;
        fv = PX_TANK;
    } else {
        fold = PX_TANK;
        fv = PX_SKY;
    }
    fld_dots();
    frame_tick();
}

/* a whole tank at (x, y), its barrel at a */
static void sprite(int16_t x, uint8_t y, uint8_t a, uint8_t me, uint8_t draw)
{
    shape_n = 0;
    add(body, BODY_N * 2);
    add_barrel(a);
    if (me)
        add(pennant, PENNANT_N * 2);
    dots(x, y, draw);
}

void field_tank(uint8_t s, uint8_t draw)
{
    uint8_t me = s == my_seat;
    if (draw && tk[s].on && drawn_on[s] && drawn_x[s] == tk[s].x && drawn_y[s] == tk[s].y
        && drawn_me[s] == me) {
        if (drawn_a[s] != tk[s].angle) {   /* an aim step: the barrel alone */
            shape_n = 0;
            add_barrel(drawn_a[s]);
            dots(drawn_x[s], drawn_y[s], 0);
            drawn_a[s] = tk[s].angle;
            shape_n = 0;
            add_barrel(tk[s].angle);
            if (me)
                add(pennant, PENNANT_N * 2);   /* (the old barrel may have crossed it) */
            dots(tk[s].x, tk[s].y, 1);
            return;
        }
        sprite(tk[s].x, tk[s].y, tk[s].angle, me, 1);   /* (over what was: it mends) */
        return;
    }
    if (drawn_on[s]) {
        sprite(drawn_x[s], drawn_y[s], drawn_a[s], drawn_me[s], 0);
        drawn_on[s] = 0;
    }
    if (draw && tk[s].on) {
        drawn_me[s] = me;
        sprite(tk[s].x, tk[s].y, tk[s].angle, me, 1);
        drawn_x[s] = tk[s].x;
        drawn_y[s] = tk[s].y;
        drawn_a[s] = tk[s].angle;
        drawn_on[s] = 1;
    }
}

/* the tanks have no colours to follow */
void field_colours(void)
{
}

/* ---- the turn's marker ---------------------------------------------------- */

#define MK_UP (TANK_H + 9)        /* the arrow's top: over the barrel and pennant */

static uint8_t mk_seat = NONE, mk_on;
static int16_t mk_x, mk_y;
static uint16_t mk_at;

/* a down arrow, 5, 3 and 1 pixels wide: fire over sky (only sky changes) */
static void marker(uint8_t on)
{
    uint8_t from = on ? PX_SKY : PX_FX, to = on ? PX_FX : PX_SKY, r;
    int8_t c;
    for (r = 0; r < 3; ++r)
        for (c = r - 2; c <= 2 - r; ++c)
            field_swap(mk_x + c, mk_y + r, from, to);
    mk_on = on;
}

/* the turn's tank, while it aims: a blinking arrow over it (NONE: away) */
void field_turn(uint8_t s)
{
    if (s >= SEATS || !tk[s].on)
        s = NONE;
    if (mk_seat != NONE && (s != mk_seat || tk[s].x != mk_x || tk[s].y - MK_UP != mk_y)) {
        if (mk_on)
            marker(0);
        mk_seat = NONE;
    }
    if (s == NONE)
        return;
    if (mk_seat == NONE) {
        mk_seat = s;
        mk_x = tk[s].x;
        mk_y = tk[s].y - MK_UP;
        mk_at = frame_count;
        marker(1);
        return;
    }
    if ((uint16_t)(frame_count - mk_at) >= 15) {
        mk_at = frame_count;
        marker(!mk_on);
    }
}

/* ---- the HUD: text rows 20-23 ------------------------------------------- */

void field_status(uint8_t row, uint8_t col, const char *s)
{
    scr_hud(row, col, s);
    frame_tick();               /* (a whole HUD is longer than the 6522 wraps) */
}

void field_status_clear(void)
{
    scr_hud_clear(0);
    scr_hud_clear(1);
    scr_hud_clear(2);
    scr_hud_clear(3);
}

/* a tank's talk: "NAME: phrase" on the fourth row; n 0 takes it away */
void field_balloon(uint8_t s, const char *t, uint8_t n)
{
    static char line[41];
    uint8_t i = 0, k;
    scr_hud_clear(3);
    if (!n || s >= SEATS)
        return;
    for (k = 0; k < NAME_LEN && NAME(s)[k]; ++k)
        line[i++] = NAME(s)[k];
    line[i++] = ':';
    line[i++] = ' ';
    for (k = 0; k < n && i < 40; ++k)
        line[i++] = t[k];
    line[i] = 0;
    scr_hud(3, 0, line);
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

/* a flash: full text, its rows 0-19 inverse spaces (white), the HUD under
 * them; lowlevel.s's clock turns the picture back on */
void field_flash(uint8_t frames)
{
    if (!frames)
        return;
    flashn = frames;
    *(volatile uint8_t *)0xC051 = 0;
}

/* fire is white: it has no colours to cycle */
void field_fire(uint8_t on)
{
    (void)on;
}

#endif
