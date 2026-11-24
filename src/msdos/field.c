/*
 * field.c -- the field (field.h) on the PC, both builds: the engines
 * (src/msdos/cga/fld.asm, src/msdos/vga/fld.asm, through fld.h) draw it
 * into the screen itself, the text's rows 22-24 under it are the HUD
 * (scr_hud). The CoCo's src/coco/field.c.
 *
 * The tanks are in the bitmap, as the Apple's and the CoCo's: their pixels
 * are value 0 (tank). Each is the CoCo 3's 12 by 6 (gen/msdos/gfx.c): on
 * CGA a silhouette in the tank colour, yours with a pennant; on VGA in its
 * seat's colour with black treads and a white highlight, yours with a
 * white pennant. They are shape lists for fld_dots, drawn in their
 * colours and taken off by turning their tank pixels back into sky.
 *
 * A round's look (sky, dirt, fire) is each build's video.c: CGA's palette
 * registers, VGA's DAC and line patterns. A tank's talk is a balloon over
 * it: a box in the text's font, what it covers kept and put back
 * (fld_keep).
 */
#include <string.h>
#include "game.h"
#include "field.h"
#include "plat.h"
#include "msdos.h"
#include "text.h"
#include "fld.h"
#include "fldlook.h"

/* gen/msdos/gfx.c */
extern const uint8_t tk_body[], tk_dark[], tk_lite[], tk_all[], tk_flag[], tk_barrel[];
#define BARREL_N 8
#define MIRROR   ((uint8_t)-1)          /* past 90 degrees: x becomes -1 - x */
#define CW       8                      /* the balloon's font: a cell */
#define LMAX     39                     /*  and the characters a line */

static int16_t drawn_x[SEATS];
static uint8_t drawn_y[SEATS], drawn_a[SEATS], drawn_on[SEATS], drawn_me[SEATS];
static uint8_t barrel[1 + BARREL_N * 2];

void field_init(void)
{
    fld_tables();
    vid_field_init();
    fld_clear();
}

/* a whole field is coming (net.c says so at each of its frames): over a
 * text screen, the first frame starts it on sky; over the field (a
 * resync) it is simply drawn again */
void field_loading(void)
{
    static uint8_t seen;
    if (field_up || seen == scr_seq)
        return;
    seen = scr_seq;
    vid_field_loading();
    fld_clear();
}

void field_open(uint8_t sky, uint8_t pal)
{
    field_up = 1;
    memset(drawn_on, 0, sizeof(drawn_on));
    scr_hud_clear(0);
    scr_hud_clear(1);
    scr_hud_clear(2);
    frame_wait();
    vid_field_open(sky & 7, pal & 7);
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

/* a shape list at (x, y): drawn in colour c (the CGA: a value), or its
 * tank pixels back to sky */
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
    const uint8_t *b = tk_barrel + (uint16_t)(a <= 90 ? a / 5 : (180 - a) / 5) * (BARREL_N * 2);
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
#ifdef TK_ONE
    (void)s;
    dots(tk_all, x, y, draw, TK_COL(s));
#else
    dots(tk_body, x, y, draw, TK_COL(s));
    dots(tk_dark, x, y, draw, TK_DARKCOL);
    dots(tk_lite, x, y, draw, TK_LITECOL);
#endif
    dots(barrel, x, y, draw, TK_COL(s));
    if (me)
        dots(tk_flag, x, y, draw, TK_FLAGCOL);
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
    scr_hud(row, col, s, 0);
}

void field_status_hi(uint8_t row, uint8_t col, const char *s)
{
    scr_hud(row, col, s, 1);
}

#ifdef SCR_NAMES
void field_name(uint8_t row, uint8_t col, uint8_t n, uint8_t seat)
{
    scr_hud_name(row, col, n, seat);
}
#endif

void field_status_clear(void)
{
    uint8_t r;
    for (r = 0; r < 3; ++r)
        scr_hud_clear(r);
}

/* ---- the balloon -------------------------------------------------------- */

static uint8_t kept, k_y, k_h;
static int16_t k_x, k_n;
static uint8_t keep[KEEP_SIZE];         /* two lines of text: 18 lines of the screen */
static uint8_t gl[1 + 64 * 2];

static void keep_box(uint8_t back)
{
    fy = k_y;
    fx = k_x;
    fx1 = k_n;
    fv = k_h;
    fold = back;
    fent = keep;
    fld_keep();
}

/* character c at (x, y): its lit pixels in the ink */
static void glyph(int16_t x, uint8_t y, uint8_t c)
{
    const uint8_t *g;
    uint8_t r, b, k, n = 0;
    if (c < 32 || c > 127)
        c = ' ';
    g = font8 + (uint16_t)(c - 32) * 8;
    for (r = 0; r < 8; ++r) {
        b = g[r];
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
    dots(gl, x, y, 1, BAL_INK);
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
    x0 &= ~(KEEP_PPB - 1);              /* (whole units) */
    i = (uint8_t)(TANK_H + 6 + h);
    k_y = tk[s].y > i ? (uint8_t)(tk[s].y - i) : 0;
    k_x = x0 / KEEP_PPB;
    k_n = (x0 + w - 1) / KEEP_PPB - k_x + 1;
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

void field_flash(uint8_t frames)
{
    vid_flash(frames);
}

void field_fire(uint8_t on)
{
    vid_fire(on);
}
