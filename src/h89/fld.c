/*
 * fld.c -- the field's store and the primitives the script player draws
 * with (fld.h; the player's own asm, src/z80, calls them through fldglue.asm).
 *
 * The field is 160 x 44 blocks, two bits a block (a byte holds four,
 * leftmost in the low bits): 1 sky, 2 dirt, 3 fire (the tanks are never in
 * it: field.c draws them over it). The terminal shows
 * a cell of 2 x 2 blocks as one graphics character (field.c), so a block
 * that changes marks its cell dirty; field_sync composes the dirty cells
 * into the screen (h19.h's bg) once a frame.
 *
 * The arguments are variables, as on the Adam (src/tms9918/fld.h): fx, fy,
 * fx1 signed words, fv, fold, emode bytes, fent a pointer, ex, ey words,
 * erx, ery bytes. The ellipse is view.go's: x^2 ry^2 + y^2 rx^2 <= rx^2 ry^2.
 */
#include <stdint.h>
#include <string.h>
#include "field.h"
#include "fld.h"

#define CELLS_W (FIELD_W / 2)
#define CELLS_H (FIELD_H / 2)

uint8_t fld_store[FIELD_H][FIELD_W / 4];
int16_t fx, fy, fx1, ex, ey;
uint8_t fv, fold, erx, ery, emode, fld_ncols;
const uint8_t *fent;

/* the cells to be composed: a bit each, the row's range, any */
uint8_t fld_cd[CELLS_H][CELLS_W / 8];
uint8_t fld_lo[CELLS_H], fld_hi[CELLS_H];
uint8_t fld_any;

static void mark(uint8_t x, uint8_t y)
{
    static uint8_t cx, cy;
    cx = x >> 1;
    cy = y >> 1;
    fld_cd[cy][cx >> 3] |= (uint8_t)(1 << (cx & 7));
    if (cx < fld_lo[cy])
        fld_lo[cy] = cx;
    if (cx >= fld_hi[cy])
        fld_hi[cy] = cx + 1;
    fld_any = 1;
}

/* a cell of the screen, composed again at the next sync */
void fld_mark_cell(uint8_t cx, uint8_t cy)
{
    if (cx >= CELLS_W || cy >= CELLS_H)
        return;
    fld_cd[cy][cx >> 3] |= (uint8_t)(1 << (cx & 7));
    if (cx < fld_lo[cy])
        fld_lo[cy] = cx;
    if (cx >= fld_hi[cy])
        fld_hi[cy] = cx + 1;
    fld_any = 1;
}

void fld_mark_all(void)
{
    uint8_t y;
    memset(fld_cd, 0xFF, sizeof(fld_cd));
    for (y = 0; y < CELLS_H; ++y) {
        fld_lo[y] = 0;
        fld_hi[y] = CELLS_W;
    }
    fld_any = 1;
}

static void unmark(void)
{
    uint8_t y;
    memset(fld_cd, 0, sizeof(fld_cd));
    for (y = 0; y < CELLS_H; ++y) {
        fld_lo[y] = CELLS_W;
        fld_hi[y] = 0;
    }
    fld_any = 0;
}

void fld_clear(void)
{
    memset(fld_store, 0x55, sizeof(fld_store));    /* (sky in all four) */
    fld_mark_all();
}

void fld_init(void)
{
    fld_clear();
    unmark();
}

/* ---- a block ------------------------------------------------------------ */

/* a block's bits: the mask that clears them, and a value in place */
static const uint8_t clr[4] = { 0xFC, 0xF3, 0xCF, 0x3F };
static const uint8_t vin[4][4] = {
    { 0, 1, 2, 3 }, { 0, 4, 8, 12 }, { 0, 16, 32, 48 }, { 0, 64, 128, 192 }
};

static uint8_t unpack(uint8_t b, uint8_t i)
{
    switch (i) {
    case 0:
        return b & 3;
    case 1:
        return (b >> 2) & 3;
    case 2:
        return (b >> 4) & 3;
    }
    return b >> 6;
}

void c_fld_plot(void)
{
    static uint8_t *p, i, b;
    if ((uint16_t)fx >= FIELD_W || (uint16_t)fy >= FIELD_H)
        return;
    i = (uint8_t)fx & 3;
    p = &fld_store[(uint8_t)fy][(uint8_t)fx >> 2];
    b = (*p & clr[i]) | vin[i][fv];
    if (b != *p) {
        *p = b;
        mark((uint8_t)fx, (uint8_t)fy);
    }
}

uint8_t c_fld_get(void)
{
    if ((uint16_t)fx >= FIELD_W || (uint16_t)fy >= FIELD_H)
        return PX_SKY;
    return unpack(fld_store[(uint8_t)fy][(uint8_t)fx >> 2], (uint8_t)fx & 3);
}

void c_fld_swap(void)
{
    static uint8_t *p, i, b;
    if ((uint16_t)fx >= FIELD_W || (uint16_t)fy >= FIELD_H)
        return;
    i = (uint8_t)fx & 3;
    p = &fld_store[(uint8_t)fy][(uint8_t)fx >> 2];
    if (unpack(*p, i) == fold) {
        b = (*p & clr[i]) | vin[i][fv];
        if (b != *p) {
            *p = b;
            mark((uint8_t)fx, (uint8_t)fy);
        }
    }
}

/* ---- spans -------------------------------------------------------------- */

/* line fy, fx..fx1 (both in), clipped: every block (any), or those that are
 * fold (swap) */
static void span(uint8_t swap)
{
    static int16_t a, b;
    static uint8_t *p, y, x, i, n;
    if ((uint16_t)fy >= FIELD_H)
        return;
    a = fx < 0 ? 0 : fx;
    b = fx1 >= FIELD_W ? FIELD_W - 1 : fx1;
    if (a > b)
        return;
    y = (uint8_t)fy;
    for (x = (uint8_t)a; x <= (uint8_t)b; ++x) {
        i = x & 3;
        p = &fld_store[y][x >> 2];
        if (swap && unpack(*p, i) != fold)
            continue;
        n = (*p & clr[i]) | vin[i][fv];
        if (n != *p) {
            *p = n;
            mark(x, y);
        }
    }
}

void c_fld_hspan(void)
{
    span(0);
}

void c_fld_vrun(void)
{
    static int16_t a, b;
    static uint8_t x, y, i, *p, n;
    if ((uint16_t)fx >= FIELD_W)
        return;
    a = fy < 0 ? 0 : fy;
    b = fx1 > FIELD_H ? FIELD_H : fx1;
    x = (uint8_t)fx;
    i = x & 3;
    for (y = (uint8_t)a; (int16_t)y < b; ++y) {
        p = &fld_store[y][x >> 2];
        n = (*p & clr[i]) | vin[i][fv];
        if (n != *p) {
            *p = n;
            mark(x, y);
        }
    }
}

/* ---- a column ----------------------------------------------------------- */

#define T_SPANS_ 240

/* column fx becomes the TERRAIN entry at fent; its length */
uint8_t c_fld_col(void)
{
    static uint8_t x, y, v, n, i, top, end, *p, sky, dirt, keep;
    static const uint8_t *e;
    e = fent;
    v = *e;
    x = (uint8_t)fx;
    if (v < T_SPANS_) {
        n = 1;
        top = v > FIELD_H ? FIELD_H : v;
    } else {
        n = 1 + 2 * (v - T_SPANS_);
    }
    if ((uint16_t)fx < FIELD_W) {
        i = x & 3;
        sky = vin[i][PX_SKY];
        dirt = vin[i][PX_DIRT];
        keep = clr[i];
        if (v < T_SPANS_) {
            for (y = 0; y < top; ++y) {
                p = &fld_store[y][x >> 2];
                *p = (*p & keep) | sky;
            }
            for (; y < FIELD_H; ++y) {
                p = &fld_store[y][x >> 2];
                *p = (*p & keep) | dirt;
            }
        } else {
            for (y = 0; y < FIELD_H; ++y) {
                p = &fld_store[y][x >> 2];
                *p = (*p & keep) | sky;
            }
            for (i = 0; i < v - T_SPANS_; ++i) {
                top = e[1 + 2 * i];
                end = e[2 + 2 * i];
                if (end > FIELD_H)
                    end = FIELD_H;
                for (y = top; y < end; ++y) {
                    p = &fld_store[y][x >> 2];
                    *p = (*p & clr[x & 3]) | dirt;
                }
            }
        }
        for (y = 0; y < FIELD_H; y += 2)
            mark(x, y);
    }
    return n;
}

void c_fld_cols(void)
{
    static uint8_t k;
    for (k = fld_ncols; k; --k) {
        fent += c_fld_col();
        ++fx;
    }
}

/* ---- the ellipse -------------------------------------------------------- */

/* one row of it: line y, ex - hw .. ex + hw, by the mode (0 all of it fv,
 * 1 the sky becomes fv, 2 the fire becomes sky) */
static void erow(int16_t y, uint8_t hw)
{
    static uint8_t keep;
    fy = y;
    fx = ex - hw;
    fx1 = ex + hw;
    if (emode == 0) {
        span(0);
    } else if (emode == 1) {
        fold = PX_SKY;
        span(1);
    } else {
        fold = PX_FX;
        keep = fv;
        fv = PX_SKY;
        span(1);
        fv = keep;
    }
}

void c_fld_ellipse(void)
{
    static uint8_t dy, hw;
    static uint32_t full, rxs, rys, row;
    if (!ery)
        ery = 1;
    if (!erx)
        erx = 1;
    rxs = (uint32_t)erx * erx;
    rys = (uint32_t)ery * ery;
    full = rxs * rys;
    hw = erx;
    for (dy = 0; dy <= ery; ++dy) {
        row = (uint32_t)dy * dy * rxs;
        while (hw && (uint32_t)hw * hw * rys + row > full)
            --hw;
        erow(ey - dy, hw);
        if (dy)
            erow(ey + dy, hw);
    }
}

/* ---- the screen's glyphs ------------------------------------------------ */

/* a cell's four blocks (bit 0 upper left, 1 upper right, 2 lower left,
 * 3 lower right: set when dirt or fire) to the character that shows them:
 * quadrants l m n o, halves p q and their reverses; the two diagonals are
 * the checkerboard. (src/h89/h19.h) */
const uint8_t fld_glyph[16] = {
    ' ',        /* 0 */
    'n',        /* upper left */
    'o',        /* upper right */
    'p',        /* upper half */
    'm',        /* lower left */
    'q' | 0x80, /* left half: reverse of the right half */
    'i',        /* upper right + lower left */
    'l' | 0x80, /* all but lower right: reverse of the lower right */
    'l',        /* lower right */
    'i',        /* upper left + lower right */
    'q',        /* right half */
    'm' | 0x80, /* all but lower left */
    'p' | 0x80, /* lower half: reverse of the upper half */
    'o' | 0x80, /* all but upper right */
    'n' | 0x80, /* all but upper left */
    ' ' | 0x80  /* all */
};

uint8_t fld_quad(uint8_t cx, uint8_t cy)
{
    static const uint8_t *p;
    static uint8_t m, a, b, c, d, t, u, i;
    p = &fld_store[(uint8_t)(cy << 1)][cx >> 1];
    t = p[0];
    u = p[FIELD_W / 4];
    i = (cx & 1) ? 4 : 0;               /* the byte's left cell, or right */
    if (i) {
        t >>= 4;
        u >>= 4;
    }
    a = t & 3;
    b = (t >> 2) & 3;
    c = u & 3;
    d = (u >> 2) & 3;
    if (a == PX_FX || b == PX_FX || c == PX_FX || d == PX_FX)
        return 'i';                     /* fire: the checkerboard, whatever it burns */
    m = (a > PX_SKY) | (b > PX_SKY) << 1 | (c > PX_SKY) << 2 | (d > PX_SKY) << 3;
    return fld_glyph[m];
}
