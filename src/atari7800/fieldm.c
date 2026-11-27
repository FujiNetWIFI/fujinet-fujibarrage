/*
 * fieldm.c -- the field screen on MARIA (src/atari/field.c draws its
 * pixels): the HUD's three rows of 320A text, the field's 21 zones of
 * 160A, each zone's sky and dirt (the Atari's gradients, a step a zone), and
 * the tanks' colours.
 *
 * A tank is drawn into the bitmap in pixel value 0, which 160A leaves
 * transparent: its underlay, a box exactly over its pixels in its seat's
 * colour, is drawn first in each zone it crosses, and shows through. The
 * box is 2 bytes (8 pixels) of a strip in the field's spare bytes (maria.s:
 * L for a run that meets a zone's top or bottom, F5 and F6 for one inside a
 * zone), at the header's high byte that puts its lines on the tank's.
 * Seat s is colour 1 + s % 3 of palette 3 + s / 3.
 */
#ifdef __ATARI7800__

#include <string.h>
#include "a78.h"
#include "game.h"
#include "field.h"
#include "plat.h"
#include "proto.h"

extern const uint8_t seat_col[10];          /* scr.c */
void row_dl(uint8_t z, const uint8_t *m, uint8_t nc, uint8_t nn);

uint8_t m_fshown;                           /* the field screen is up (its lists are) */

extern uint8_t m_hud[NHUD][40];             /* hud.s (field_status, field_status_hi) */
#define hud m_hud
static uint8_t nm_col[NHUD], nm_n[NHUD];
static uint8_t bx_on[SEATS], bx_top[SEATS], bx_h[SEATS], bx_x[SEATS];

/* ---- the zones ------------------------------------------------------------- */

static void zone_dl(uint8_t z)
{
    uint8_t *d, s, y0 = z << 3, t, a, b, n, lo, hi;
    m_busy = 1;
    d = m_back(NHUD + z);
    for (s = 0; s < SEATS; ++s) {
        if (!bx_on[s])
            continue;
        t = bx_top[s];
        if (t >= y0 + 8 || t + bx_h[s] <= y0)
            continue;
        a = t > y0 ? t - y0 : 0;
        b = t + bx_h[s] - y0;
        if (b > 8)
            b = 8;
        if (a == 0 || b == 8) {             /* L: zeros $40-$46, value $47-$4E */
            lo = 0xF0;
            hi = b == 8 ? 0x47 + a : 0x3F + b;
        } else {
            n = b - a;
            if (n < 5)
                continue;                   /* (a box is 5 lines or more) */
            lo = 0xF6;                      /* F5: value $42-$46; F6: $4A-$4F */
            hi = (n == 5 ? 0x46 : 0x4F) - 7 + a;
        }
        d[0] = lo + (s % 3) * 2;
        d[1] = (uint8_t)((3 + s / 3) << 5) | 30;    /* 2 bytes */
        d[2] = hi;
        d[3] = bx_x[s];
        d += 4;
    }
    lo = (z % 6) * 40;
    hi = FLDPG + (z / 6) * 8;
    d[0] = lo;
    d[1] = (uint8_t)((z & 1) << 5) | 12;            /* 20 bytes */
    d[2] = hi;
    d[3] = 0;
    d[4] = lo + 20;
    d[5] = d[1];
    d[6] = hi;
    d[7] = 80;
    d[8] = 0;
    d[9] = 0;
    m_flip(NHUD + z);
    m_busy = 0;
}

static void zones(uint8_t top, uint8_t h)
{
    uint8_t z = top >> 3, e = (uint8_t)(top + h - 1) >> 3;
    if (!m_fshown)
        return;
    if (e >= NZONES)
        e = NZONES - 1;
    for (; z <= e; ++z)
        zone_dl(z);
}

/* seat s's box: x its left pixel, top its first line, h lines (h 0: none) */
void m_box(uint8_t s, int16_t x, int16_t top, int8_t h)
{
    uint8_t ot = bx_top[s], oh = bx_h[s], was = bx_on[s];
    if (top < 0) {
        h += (int8_t)top;
        top = 0;
    }
    if (h <= 0 || top >= FIELD_H) {
        bx_on[s] = 0;
        if (was)
            zones(ot, oh);
        return;
    }
    if (was && bx_x[s] == (uint8_t)x && ot == (uint8_t)top && oh == (uint8_t)h)
        return;
    bx_on[s] = 1;
    bx_x[s] = (uint8_t)x;
    bx_top[s] = (uint8_t)top;
    bx_h[s] = (uint8_t)h;
    if (was)
        zones(ot, oh);
    zones((uint8_t)top, (uint8_t)h);
}

/* ---- the screen -------------------------------------------------------------- */

/* A HUD row's list: its 40 cells in runs of 32 at most, the name's its
 * own (P2), blanks and all, so its text can change with no new list (only
 * the name's place makes one). */
static uint8_t *hrun(uint8_t *d, uint8_t r, uint8_t s, uint8_t e, uint8_t pal)
{
    uint8_t w;
    uint16_t a;
    while (s < e) {
        w = e - s > 32 ? 32 : e - s;
        a = (uint16_t)(hud[r] + s);
        d[0] = (uint8_t)a;
        d[1] = 0x60;                    /* indirect, 1-byte characters */
        d[2] = a >> 8;
        d[3] = (uint8_t)(pal << 5) | (uint8_t)(32 - w);
        d[4] = s << 2;
        d += 5;
        s += w;
    }
    return d;
}

static void hud_dl(uint8_t r)
{
    uint8_t *d, c = nm_col[r], e = nm_col[r] + nm_n[r];
    if (!m_fshown)
        return;
    if (!nm_n[r])
        c = e = 40;
    m_busy = 1;
    d = m_back(r);
    d = hrun(d, r, 0, c, 0);
    d = hrun(d, r, c, e, 2);
    d = hrun(d, r, e, 40, 0);
    d[0] = 0;
    d[1] = 0;
    m_flip(r);
    m_busy = 0;
}

void field_loading(void)
{
}

/* the field screen on (src/atari/field.c's field_open, after fopen.c's
 * m_field_open has set its colours and forgotten its tanks): the HUD's rows
 * blank, every zone's list */
void m_field_show(void)
{
    uint8_t z;
    m_fshown = 1;
    memset(hud, 0, sizeof(hud));
    memset(nm_n, 0, sizeof(nm_n));
    memset(bx_on, 0, sizeof(bx_on));
    for (z = 0; z < NHUD; ++z)
        hud_dl(z);
    for (z = 0; z < NZONES; ++z)
        zone_dl(z);
    m_show(M_FIELD);
}

void field_colours(void)
{
}

/* ---- the HUD (field_status, field_status_hi: hud.s) ---------------------------- */

void field_status_clear(void)
{
    uint8_t r;
    memset(hud, 0, sizeof(hud));
    for (r = 0; r < NHUD; ++r)
        if (nm_n[r]) {
            nm_n[r] = 0;
            hud_dl(r);
        }
}

void field_name(uint8_t row, uint8_t col, uint8_t n, uint8_t seat)
{
    r_nam[NTROWS + row] = hue(seat_col[seat < SEATS ? seat : 0]);
    if (nm_col[row] == col && nm_n[row] == n)
        return;
    nm_col[row] = col;
    nm_n[row] = n;
    hud_dl(row);
}

#endif
