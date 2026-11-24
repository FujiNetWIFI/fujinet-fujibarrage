/*
 * field.c -- the field (field.h) on the CoCo 1/2 and the CoCo 3: the
 * engines (src/coco2/fld2.asm, src/coco3/fld3.asm, through fld.h) draw it,
 * the text's own rows under it are the HUD (scr2_hud, scr3_hud).
 *
 * The tanks are in the bitmap, as the Apple's: their pixels are value 0
 * (tank). On the CoCo 1/2 they are black silhouettes, the Apple's 5 by 5
 * (gen/coco2/tank2.asm); yours carries a pennant. On the CoCo 3 each is 12
 * by 6 in its seat's colour (palette 6-15) with black treads and a white
 * highlight (gen/coco3/fld3d.asm); yours flies a white pennant. Both are
 * shape lists for fld_dots: drawn in their colours, and taken off by
 * turning their tank pixels back into sky.
 *
 * The CoCo 3's sky and dirt are patterns a line (fld3.asm's rows): the
 * sky a dither between its two colours down to the horizon, or black with
 * stars; the dirt strata, lighter at the top. Each round has its palette:
 * the sky's colours (SKY_*), the dirt's (its palette), the fire's, the
 * seats'; RGB or composite (coco_swap).
 *
 * A tank's talk is a balloon over it: black, in the text's font, what it
 * covers kept and put back (fld_keep).
 */
#ifdef _COCO_BASIC_

#include <string.h>
#include "game.h"
#include "field.h"
#include "plat.h"
#include "cocoplat.h"
#include "fld.h"
#ifdef COCO3
#include "scr3.h"
#else
#include "scr2.h"
#endif

#ifdef COCO3
/* gen/coco3/fld3d.asm, gen/coco3/pal3.asm, gen/coco3/font3.asm */
extern const uint8_t sky3[], dirt3[], fire3[], dith3[], dithd3[], star3[];
extern const uint8_t tk3_body[], tk3_dark[], tk3_lite[], tk3_flag[], tk3_barrel[];
extern const uint8_t pal3_text[], font3[];
void fld_repat(void);
void fld_tables(void);
#define BARREL_N 8
#define MIRROR   ((uint8_t)-1)          /* past 90 degrees: x becomes -1 - x */
#define TK_BARREL tk3_barrel
#define N_STARS  16
#define CW       8                      /* the balloon's font: a cell */
#define LMAX     39                     /*  and the characters a line */
#define PPB      2                      /* pixels a byte */
#define INK      C3_WHITE
#else
/* gen/coco2/tank2.asm, gen/coco2/font2.asm */
extern const uint8_t tk2_body[], tk2_flag[], tk2_barrel[], font2[];
#define BARREL_N 5
#define MIRROR   0                      /* past 90 degrees: x becomes -x */
#define TK_BARREL tk2_barrel
#define CW       4
#define LMAX     31
#define PPB      4
#define INK      PX_FX
#endif

static int16_t drawn_x[SEATS];
static uint8_t drawn_y[SEATS], drawn_a[SEATS], drawn_on[SEATS], drawn_me[SEATS];
static uint8_t barrel[1 + BARREL_N * 2];

/* ---- the CoCo 3's rows and palettes ------------------------------------- */
#ifdef COCO3
static uint8_t rows_stars = 0xFF;       /* the rows are the stars' (1) or a dither's (0) */
static uint8_t pal_sky, pal_dirt, fpal[16];

/* line y's dither level (0 the sky's top colour, 16 its horizon's), the
 * dirt's (tools/mkgfx_coco.py's preview has the same) */
static uint8_t dirt_level(uint8_t y)
{
    uint8_t l = (uint8_t)((uint16_t)y * 12 / FIELD_H);
    if ((y / 9) % 3 == 1)
        l += 4;
    return l > 16 ? 16 : l;
}

/* the rows for a sky: its dither down to the horizon, or the stars: black
 * with a row of stars here and there, each from somewhere else in the star
 * patterns (they run on into each other: star3 has 63 bytes past its last) */
static void build_rows(uint8_t stars)
{
    uint8_t y, *r = rows;
    uint16_t h = (uint16_t)plat_rand() << 8 | plat_rand();
    const uint8_t *p;
    for (y = 0; y < FIELD_H; ++y, r += 6) {
        if (stars) {
            h = h * 75 + 74;
            if ((uint8_t)(h >> 8) < 64) {
                p = star3 + (h & 0x3FF);
                r[2] = 63;
            } else {
                p = dith3 + ((y & 3) << 1);
                r[2] = 1;
            }
        } else {
            p = dith3 + ((uint16_t)((uint16_t)y * 17 / FIELD_H) << 3) + ((y & 3) << 1);
            r[2] = 1;
        }
        *(const uint8_t **)r = p;
        *(const uint8_t **)(r + 3) = dithd3 + ((uint16_t)dirt_level(y) << 3) + ((y & 3) << 1);
        r[5] = 1;
    }
    rows_stars = stars;
}

/* the round's palette, RGB or composite */
static void field_palette(void)
{
    uint8_t i, c = coco_swap ? 16 : 0;
    fpal[0] = pal3_text[c];             /* black */
    fpal[1] = sky3[c + pal_sky * 2];
    fpal[2] = sky3[c + pal_sky * 2 + 1];
    fpal[3] = dirt3[c + pal_dirt * 2];
    fpal[4] = dirt3[c + pal_dirt * 2 + 1];
    fpal[5] = fire3[coco_swap ? 8 : 0];
    for (i = C3_SEAT; i < 16; ++i)
        fpal[i] = pal3_text[c + i];
    for (i = 0; i < 8; ++i)
        fire_cyc[i] = fire3[(coco_swap ? 8 : 0) + i];
    for (i = 0; i < 16; ++i)
        ((volatile uint8_t *)0xFFB0)[i] = fpal[i];
    *(volatile uint8_t *)0xFF9A = fpal[0];
}

/* the field's palette all black (being built) */
static void palette_off(void)
{
    uint8_t i;
    for (i = 0; i < 16; ++i)
        ((volatile uint8_t *)0xFFB0)[i] = 0;
}
#endif

/* the artifact phase or the monitor changed (CLEAR: video_swap) */
static void field_reswap(void)
{
#ifdef COCO3
    field_palette();
#else
    fswap = coco_swap;
    fld_phase();
#endif
}

void field_init(void)
{
#ifdef COCO3
    fld_tables();
    build_rows(0);
#else
    fswap = coco_swap;
    fld_phase();
    video_hook = field_reswap;          /* (the phase is the field's, whatever shows) */
#endif
    fld_clear();
}

/* a whole field is coming (net.c says so at each of its frames): over a
 * text screen, the first frame blanks it; over the field (a resync) it is
 * simply drawn again */
void field_loading(void)
{
    static uint8_t seen;
    if (field_up || seen == scr_seq)
        return;
    seen = scr_seq;
#ifdef COCO3
    palette_off();                      /* (dark while it is drawn) */
    build_rows(st[OFF_SKY] == SKY_STARS);
#else
    fld_clear();                        /* (the field drawn over sky, not text) */
#endif
}

void field_open(uint8_t sky, uint8_t pal)
{
    field_up = 1;
    memset(drawn_on, 0, sizeof(drawn_on));
#ifdef COCO3
    {
        uint8_t stars = (sky & 7) == SKY_STARS;
        if (stars != rows_stars) {      /* drawn on another sky's rows */
            build_rows(stars);
            fld_repat();
        }
    }
    pal_sky = sky & 7;
    pal_dirt = pal & 7;
    fire_on = 0;
    scr3_hud_clear(0);
    scr3_hud_clear(1);
    scr3_hud_clear(2);
    frame_wait();
    field_palette();
    video_hook = field_reswap;
#else
    (void)sky;
    (void)pal;
    scr2_hud_clear(0);
    scr2_hud_clear(1);
    scr2_hud_clear(2);
    frame_wait();
#endif
}

void field_column(uint16_t x, const uint8_t *entry)
{
    fx = (int16_t)x;
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

/* ---- the tanks ---------------------------------------------------------- */

/* a shape list at (x, y): drawn in colour c (the CoCo 1/2: value c), or
 * its tank pixels back to sky */
static void dots(const uint8_t *list, int16_t x, uint8_t y, uint8_t draw, uint8_t c)
{
    fx = x;
    fy = y;
    fent = list;
    if (draw) {
        fold = 0xFF;
        fv = c;
    } else {
        fold = PX_TANK;
        fv = PX_SKY;
    }
    fld_dots();
}

/* the barrel at angle a (past 90 mirrored) */
static void barrel_at(uint8_t a)
{
    const uint8_t *b = TK_BARREL + (uint16_t)(a <= 90 ? a / 5 : (180 - a) / 5) * (BARREL_N * 2);
    uint8_t i;
    barrel[0] = BARREL_N;
    for (i = 0; i < BARREL_N * 2; i += 2) {
        barrel[1 + i] = a <= 90 ? b[i] : (uint8_t)(MIRROR - b[i]);
        barrel[2 + i] = b[i + 1];
    }
}

/* a whole tank at (x, y), its barrel at a: drawn or taken off */
static void sprite(uint8_t s, int16_t x, uint8_t y, uint8_t a, uint8_t me, uint8_t draw)
{
    barrel_at(a);
#ifdef COCO3
    dots(tk3_body, x, y, draw, (uint8_t)(C3_SEAT + s));
    dots(tk3_dark, x, y, draw, C3_BLACK);
    dots(tk3_lite, x, y, draw, C3_WHITE);
    dots(barrel, x, y, draw, (uint8_t)(C3_SEAT + s));
    if (me)
        dots(tk3_flag, x, y, draw, C3_WHITE);
#else
    (void)s;
    dots(tk2_body, x, y, draw, PX_TANK);
    dots(barrel, x, y, draw, PX_TANK);
    if (me)
        dots(tk2_flag, x, y, draw, PX_TANK);
#endif
}

void field_tank(uint8_t s, uint8_t draw)
{
    uint8_t me = s == my_seat;
    if (draw && tk[s].on && drawn_on[s] && drawn_x[s] == tk[s].x && drawn_y[s] == tk[s].y
        && drawn_me[s] == me) {
        if (drawn_a[s] != tk[s].angle) {   /* an aim step: the barrel off, the tank again */
            barrel_at(drawn_a[s]);
            dots(barrel, drawn_x[s], drawn_y[s], 0, 0);
            drawn_a[s] = tk[s].angle;
        }
        sprite(s, tk[s].x, tk[s].y, tk[s].angle, me, 1);   /* (over what was: it mends) */
        return;
    }
    if (drawn_on[s]) {
        sprite(s, drawn_x[s], drawn_y[s], drawn_a[s], drawn_me[s], 0);
        drawn_on[s] = 0;
    }
    if (draw && tk[s].on) {
        drawn_me[s] = me;
        sprite(s, tk[s].x, tk[s].y, tk[s].angle, me, 1);
        drawn_x[s] = tk[s].x;
        drawn_y[s] = tk[s].y;
        drawn_a[s] = tk[s].angle;
        drawn_on[s] = 1;
    }
}

/* the tanks' colours are in the bitmap */
void field_colours(void)
{
}

/* ---- the HUD: the text's last three rows -------------------------------- */

void field_status(uint8_t row, uint8_t col, const char *s)
{
#ifdef COCO3
    scr3_hud(row, col, s, 0);
#else
    scr2_hud(row, col, s, 0);
#endif
}

#ifdef SCR_NAMES
void field_name(uint8_t row, uint8_t col, uint8_t n, uint8_t seat)
{
    scr3_hud_name(row, col, n, seat);
}
#endif

void field_status_hi(uint8_t row, uint8_t col, const char *s)
{
#ifdef COCO3
    scr3_hud(row, col, s, 1);
#else
    scr2_hud(row, col, s, 1);
#endif
}

void field_status_clear(void)
{
    uint8_t r;
    for (r = 0; r < 3; ++r)
#ifdef COCO3
        scr3_hud_clear(r);
#else
        scr2_hud_clear(r);
#endif
}

/* ---- the balloon -------------------------------------------------------- */

static uint8_t kept, k_y, k_b, k_nb, k_h;
#ifndef COCO3
static uint8_t keep[17 * 32];           /* two lines of text: 17 lines of the screen */
#endif                                  /* (the CoCo 3's: in the screen's memory, fld3.asm) */
static uint8_t gl[1 + 64 * 2];

static void keep_box(uint8_t back)
{
    fy = k_y;
    fx = k_b;
    fx1 = k_nb;
    fv = k_h;
    fold = back;
#ifndef COCO3
    fent = keep;
#endif
    fld_keep();
}

/* character c at (x, y): its lit pixels in the ink */
static void glyph(int16_t x, uint8_t y, uint8_t c)
{
    const uint8_t *g;
    uint8_t r, b, k, n = 0;
#ifdef COCO3
    if (c < 32 || c > 127)
        c = ' ';
    g = font3 + (uint16_t)(c - 32) * 8;
#else
    if (c >= 'a' && c <= 'z')
        c -= 32;
    if (c < 32 || c > 95)
        c = ' ';
    g = font2 + (uint16_t)(c - 32) * 8;
#endif
    for (r = 0; r < 8; ++r) {
#ifdef COCO3
        b = g[r];
#else
        b = (uint8_t)(g[r] << 4);
#endif
        for (k = 0; b; ++k, b <<= 1)
            if (b & 0x80) {
                gl[1 + n * 2] = k;
                gl[2 + n * 2] = r;
                ++n;
            }
    }
    if (!n)
        return;
    gl[0] = n;
    dots(gl, x, y, 1, INK);
}

void field_balloon(uint8_t s, const char *t, uint8_t n)
{
    uint8_t lines, len, h, i, line;
    int16_t x0, w;
    if (kept) {
        keep_box(1);
        kept = 0;
    }
    if (!n || s >= SEATS)
        return;
    if (n > MAX_TALK)
        n = MAX_TALK;
    lines = n > LMAX ? 2 : 1;
    len = lines == 2 ? (uint8_t)((n + 1) >> 1) : n;
    w = (int16_t)len * CW + 2;
    h = (uint8_t)(lines * 8 + 2);
    x0 = tk[s].x - (w >> 1);
    if (x0 < 0)
        x0 = 0;
    if (x0 + w > FIELD_W)
        x0 = FIELD_W - w;
    x0 &= ~(PPB - 1);                   /* (whole bytes) */
    i = (uint8_t)(TANK_H + 6 + h);
    k_y = tk[s].y > i ? (uint8_t)(tk[s].y - i) : 0;
    k_b = (uint8_t)(x0 / PPB);
    k_nb = (uint8_t)((x0 + w - 1) / PPB - k_b + 1);
    k_h = h;
    keep_box(0);
    kept = 1;
    for (i = 0; i < h; ++i)             /* the box */
        field_span((int16_t)k_y + i, x0, x0 + w - 1, PX_TANK);
    for (line = 0, i = 0; line < lines; ++line) {
        uint8_t k;
        for (k = 0; k < len && i < n; ++k, ++i)
            glyph(x0 + 1 + (int16_t)k * CW, (uint8_t)(k_y + 1 + line * 8), (uint8_t)t[i]);
    }
}

/* ---- flash, fire --------------------------------------------------------- */

/* a flash: frames of it (video_frame counts them off) */
void field_flash(uint8_t frames)
{
    video_flash(frames);
}

void field_fire(uint8_t on)
{
#ifdef COCO3
    fire_on = on;
    if (!on)
        *(volatile uint8_t *)0xFFB5 = fpal[5];
#else
    (void)on;                           /* (white: it has no colours to cycle) */
#endif
}

#endif
