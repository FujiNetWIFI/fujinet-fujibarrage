/*
 * field.c -- the field (field.h) on the H19: the store is fld.c's, a cell
 * (2 x 2 blocks) one graphics character; the tanks, their barrels and the
 * talk balloons are drawn over it when a cell is composed, never in the
 * store. The three status rows are the screen's rows 22, 23 and the 25th
 * line.
 *
 * A change marks cells dirty (fld.c, or here); field_sync (once a frame, from
 * src/h89/plat.c) composes the dirty cells into bg (h19.h), and h19_flush
 * sends what the terminal doesn't show yet.
 */
#include <string.h>
#include "game.h"
#include "field.h"
#include "plat.h"
#include "fld.h"
#include "h19.h"
#include "h89.h"

#define CW (FIELD_W / 2)
#define CH (FIELD_H / 2)
#define HUD_Y 22                        /* the status rows' first screen row */
#define BAL_MAX (MAX_TALK + 2)

/* the tank's look: 0 a reverse seat digit and a block (a bright box), 1 the
 * hull a lower half and a block (turret toward the aim) with the digit over
 * it, 2 the same hull hatched */
#ifndef H89_LOOK
#define H89_LOOK 2
#endif

uint8_t shake_n;                        /* (the player counts a shake: nothing shakes here) */

/* a tank: its body cells (cx, cx + 1 on row r), the barrel's cell (bx, by,
 * character bg; 0 none) and, in the looks that have one, its seat's digit
 * over the body (gx, gy, gg) */
typedef struct {
    uint8_t on, cx, r, me, bx, by, bg, gx, gy, gg, face;
    int16_t x;
    uint8_t y, a;
} tank_ov;
static tank_ov ovt[SEATS];

static struct { uint8_t on, c, r, n; char s[MAX_TALK + 1]; } bal;      /* (one tank talks at a time) */

/* cells that carry an overlay: a bit each */
static uint8_t ovm[CH][CW / 8];

static uint8_t ov_at(uint8_t cx, uint8_t cy)
{
    return ovm[cy][cx >> 3] & (1 << (cx & 7));
}

static void ov_set(int8_t cx, int8_t cy)
{
    if (cx < 0 || cy < 0 || cx >= CW || cy >= CH)
        return;
    ovm[cy][(uint8_t)cx >> 3] |= (uint8_t)(1 << (cx & 7));
}

/* what is over the field's cells again, and those cells composed again */
static void ov_rebuild(void)
{
    static uint8_t s, i, y, c;
    memset(ovm, 0, sizeof(ovm));
    for (s = 0; s < SEATS; ++s) {
        if (ovt[s].on) {
            ov_set(ovt[s].cx, ovt[s].r);
            ov_set(ovt[s].cx + 1, ovt[s].r);
            if (ovt[s].bg)
                ov_set(ovt[s].bx, ovt[s].by);
            if (ovt[s].gg)
                ov_set(ovt[s].gx, ovt[s].gy);
        }
        if (bal.on) {
            y = bal.r;
            for (i = 0; i < bal.n + 2; ++i) {
                c = bal.c + i;
                ov_set(c, y);
            }
        }
    }
}

static void ov_mark_tank(uint8_t s)
{
    fld_mark_cell(ovt[s].cx, ovt[s].r);
    fld_mark_cell(ovt[s].cx + 1, ovt[s].r);
    if (ovt[s].bg)
        fld_mark_cell(ovt[s].bx, ovt[s].by);
    if (ovt[s].gg)
        fld_mark_cell(ovt[s].gx, ovt[s].gy);
}

static void ov_mark_bal(void)
{
    static uint8_t i;
    for (i = 0; i < bal.n + 2; ++i)
        fld_mark_cell(bal.c + i, bal.r);
}

/* ---- the screen ---------------------------------------------------------- */

void field_init(void)
{
    fld_init();
    memset(ovt, 0, sizeof(ovt));
    memset(&bal, 0, sizeof(bal));
    memset(ovm, 0, sizeof(ovm));
}

/* a whole field is coming (net.c says so at each of its frames): over a text
 * screen, the first frame blanks it; over the field (a resync) it is
 * drawn again */
void field_loading(void)
{
    static uint8_t seen;
    if (field_up || seen == scr_seq)
        return;
    seen = scr_seq;
    h19_blank(0, 25);
    h19_cleared();
    fld_clear();
}

void field_open(uint8_t sky, uint8_t pal)
{
    (void)sky;
    (void)pal;
    field_up = 1;
    field_coming = 0;
    memset(ovt, 0, sizeof(ovt));
    memset(&bal, 0, sizeof(bal));
    memset(ovm, 0, sizeof(ovm));
    h19_blank(HUD_Y, 3);
    h19_cleared();
    fld_mark_all();
}

void field_column(uint16_t x, const uint8_t *entry)
{
    fx = (int16_t)x;
    fent = entry;
    c_fld_col();
}

void field_plot(int16_t x, int16_t y, uint8_t v)
{
    fx = x;
    fy = y;
    fv = v;
    c_fld_plot();
}

uint8_t field_get(int16_t x, int16_t y)
{
    fx = x;
    fy = y;
    return c_fld_get();
}

void field_swap(int16_t x, int16_t y, uint8_t from, uint8_t to)
{
    fx = x;
    fy = y;
    fold = from;
    fv = to;
    c_fld_swap();
}

void field_span(int16_t y, int16_t x0, int16_t x1, uint8_t v)
{
    fy = y;
    fx = x0;
    fx1 = x1;
    fv = v;
    c_fld_hspan();
}

void field_ellipse(int16_t x, int16_t y, uint8_t rx, uint8_t ry, uint8_t v, uint8_t mode)
{
    ex = x;
    ey = y;
    erx = rx ? rx : 1;
    ery = ry > 200 ? 200 : ry ? ry : 1;
    fv = v;
    emode = mode;
    c_fld_ellipse();
}

/* ---- the tanks ------------------------------------------------------------ */

/* the barrel's cell and character by angle (0 right, 90 up, 180 left) */
static void barrel(tank_ov *t, uint8_t a)
{
    int8_t dx = 1, dy = -1;
    uint8_t g = '`';
    if (a < 12) {
        dx = 2; dy = 0; g = 'a';
    } else if (a < 40) {
        dx = 2; g = 'x';
    } else if (a < 70) {
        g = 'x';
    } else if (a < 91) {
        ;
    } else if (a < 110) {
        dx = 0;
    } else if (a < 140) {
        dx = 0; g = 'y';
    } else if (a < 168) {
        dx = -1; g = 'y';
    } else {
        dx = -1; dy = 0; g = 'a';
    }
    if ((int8_t)(t->cx + dx) < 0 || (int8_t)(t->r + dy) < 0 || t->cx + dx >= CW) {
        t->bg = 0;
        return;
    }
    t->bx = t->cx + dx;
    t->by = t->r + dy;
    t->bg = g;
}

/* the seat's digit over the body, on the side the barrel is not (the looks
 * that have one) */
static void digit(tank_ov *t, uint8_t s)
{
    t->gg = 0;
#if H89_LOOK > 0
    if (t->r == 0)
        return;
    t->gx = t->cx + (t->bg && t->bx == t->cx + 1 ? 0 : t->bg && t->bx == t->cx ? 1 : t->face ? 1 : 0);
    t->gy = t->r - 1;
    t->gg = (uint8_t)('0' + s) | (t->me ? REV : 0);
#else
    (void)s;
#endif
}

void field_tank(uint8_t s, uint8_t draw)
{
    static int16_t x;
    static uint8_t me;
    tank_ov *t = &ovt[s];
    me = s == my_seat;
    if (draw && tk[s].on && t->on && t->x == tk[s].x && t->y == tk[s].y && t->a == tk[s].angle
        && t->me == me)
        return;
    if (t->on) {
        ov_mark_tank(s);
        t->on = 0;
    }
    if (draw && tk[s].on) {
        x = tk[s].x;
        t->x = x;
        t->y = tk[s].y;
        t->a = tk[s].angle;
        t->me = me;
        t->cx = x < 2 ? 0 : x >= FIELD_W - 1 ? CW - 2 : (uint8_t)((x - 1) >> 1);
        t->r = tk[s].y ? (tk[s].y - 1) >> 1 : 0;       /* (the cell over the ground) */
        if (t->r >= CH)
            t->r = CH - 1;
        t->on = 1;
        t->face = tk[s].angle > 90;
        barrel(t, tk[s].angle);
        digit(t, s);
        ov_mark_tank(s);
    }
    ov_rebuild();
}

/* monochrome: nothing to recolour */
void field_colours(void)
{
}

/* a tank's talk, over it: reverse video, a blank each side */
void field_balloon(uint8_t seat, const char *s, uint8_t n)
{
    int16_t c;
    if (bal.on) {
        ov_mark_bal();
        bal.on = 0;
    }
    if (n && ovt[seat].on) {
        if (n > MAX_TALK)
            n = MAX_TALK;
        memcpy(bal.s, s, n);
        bal.s[n] = 0;
        bal.n = n;
        c = (int16_t)ovt[seat].cx + 1 - n / 2 - 1;
        if (c + n + 2 > CW)
            c = CW - n - 2;
        if (c < 0)
            c = 0;
        bal.c = (uint8_t)c;
        bal.r = ovt[seat].r >= 3 ? ovt[seat].r - 3 : 0;
        bal.on = 1;
        ov_mark_bal();
    }
    ov_rebuild();
}

void field_flash(uint8_t frames)
{
    (void)frames;
}

void field_fire(uint8_t on)
{
    (void)on;
}

/* ---- the status rows -------------------------------------------------------- */

void field_status(uint8_t row, uint8_t col, const char *s)
{
    h19_str(col, HUD_Y + row, s, 0);
}

void field_status_hi(uint8_t row, uint8_t col, const char *s)
{
    h19_str(col, HUD_Y + row, s, REV);
}

void field_status_clear(void)
{
    h19_blank(HUD_Y, 3);
}

/* ---- the screen composed ---------------------------------------------------- */

static uint8_t seat_glyph(const tank_ov *t, uint8_t s, uint8_t cx, uint8_t cy)
{
    if (cy == t->r && (cx == t->cx || cx == t->cx + 1)) {
#if H89_LOOK == 0
        if (cx == t->cx)
            return (uint8_t)('0' + s) | REV;
        return t->me ? (' ' | REV) : ('p' | REV);
#elif H89_LOOK == 1
        /* the hull a lower half and a block, the block (the turret) toward the aim */
        return (cx == t->cx) == t->face ? (' ' | REV) : ('p' | REV);
#else
        (void)s;
        return 'w';
#endif
    }
    if (t->gg && cy == t->gy && cx == t->gx)
        return t->gg;
    return t->bg && cy == t->by && cx == t->bx ? t->bg : 0;
}

static uint8_t overlay(uint8_t cx, uint8_t cy)
{
    static uint8_t s, g, i;
    for (s = 0; s < SEATS; ++s)
        if (bal.on && cy == bal.r && cx >= bal.c && cx < bal.c + bal.n + 2) {
            i = cx - bal.c;
            if (i == 0 || i == bal.n + 1)
                return ' ' | REV;
            return h19_char((uint8_t)bal.s[i - 1]) | REV;
        }
    for (s = 0; s < SEATS; ++s)
        if (ovt[s].on && (g = seat_glyph(&ovt[s], s, cx, cy)) != 0)
            return g;
    return 0;
}

void field_sync(void)
{
    static uint8_t cy, cx, g, lo, hi, a, b, *row;
    if (!fld_any)
        return;
    fld_any = 0;
    for (cy = 0; cy < CH; ++cy) {
        lo = fld_lo[cy];
        hi = fld_hi[cy];
        if (lo >= hi)
            continue;
        fld_lo[cy] = CW;
        fld_hi[cy] = 0;
        a = 0xFF;
        b = 0;
        row = bg[cy];
        for (cx = lo; cx < hi; ++cx) {
            if (!(fld_cd[cy][cx >> 3] & (1 << (cx & 7))))
                continue;
            fld_cd[cy][cx >> 3] &= ~(1 << (cx & 7));
            g = 0;
            if (ov_at(cx, cy))
                g = overlay(cx, cy);
            if (!g)
                g = fld_quad(cx, cy);
            if (row[cx] != g) {
                row[cx] = g;
                if (a == 0xFF)
                    a = cx;
                b = cx;
            }
        }
        if (a != 0xFF)
            h19_span(cy, a, b + 1);
    }
}
