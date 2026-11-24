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

/* the four blocks of a byte all one value, by value */
static const uint8_t pat[4] = { 0, 0x55, 0xAA, 0xFF };
/* the blocks of a byte from the n-th on, and up to the n-th */
static const uint8_t em_from[4] = { 0xFF, 0xFC, 0xF0, 0xC0 };
static const uint8_t em_to[4] = { 0x03, 0x0F, 0x3F, 0xFF };
/* the cell bits from the n-th on of a bitmap byte, and up to it */
static const uint8_t cb_from[8] = { 0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80 };
static const uint8_t cb_to[8] = { 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF };

/* the cells of block row y over store bytes b0..b1 (a byte is two cells):
 * composed again at the next sync */
static void mark_bytes(uint8_t y, uint8_t b0, uint8_t b1)
{
    static uint8_t cy, c0, c1, k, k1, m;
    cy = y >> 1;
    c0 = b0 << 1;
    c1 = (b1 << 1) + 1;
    k = c0 >> 3;
    k1 = c1 >> 3;
    for (;; ++k) {
        m = 0xFF;
        if (k == c0 >> 3)
            m = cb_from[c0 & 7];
        if (k == k1)
            m &= cb_to[c1 & 7];
        fld_cd[cy][k] |= m;
        if (k == k1)
            break;
    }
    if (c0 < fld_lo[cy])
        fld_lo[cy] = c0;
    if (c1 >= fld_hi[cy])
        fld_hi[cy] = c1 + 1;
    fld_any = 1;
}

/* the cells of column x over block rows y0..y1-1 */
static void mark_col(uint8_t x, uint8_t y0, uint8_t y1)
{
    static uint8_t cy, cx, bit, k;
    cx = x >> 1;
    k = cx >> 3;
    bit = 1 << (cx & 7);
    for (cy = y0 >> 1; cy <= (uint8_t)(y1 - 1) >> 1; ++cy) {
        fld_cd[cy][k] |= bit;
        if (cx < fld_lo[cy])
            fld_lo[cy] = cx;
        if (cx >= fld_hi[cy])
            fld_hi[cy] = cx + 1;
    }
    fld_any = 1;
}

/* line fy, fx..fx1 (both in), clipped, by bytes: swap 0 every block becomes
 * the value P is of; swap 1 only those that are Pf's value. A changed byte
 * is written, and its cells marked once, over the changed range. */
static void rowfill(uint8_t swap, uint8_t P, uint8_t Pf)
{
    static int16_t a, b;
    static uint8_t y, ba, bb, n, em, x, z, m, first, last, any, *p, i;
    if ((uint16_t)fy >= FIELD_H)
        return;
    a = fx < 0 ? 0 : fx;
    b = fx1 >= FIELD_W ? FIELD_W - 1 : fx1;
    if (a > b)
        return;
    y = (uint8_t)fy;
    ba = (uint8_t)a >> 2;
    bb = (uint8_t)b >> 2;
    p = &fld_store[y][ba];
    any = 0;
    for (i = ba;; ++i, ++p) {
        em = 0xFF;
        if (i == ba)
            em = em_from[(uint8_t)a & 3];
        if (i == bb)
            em &= em_to[(uint8_t)b & 3];
        if (swap) {
            x = *p ^ Pf;
            z = ~(x | (x >> 1)) & 0x55;         /* a bit a block that is Pf's value */
            m = (z | (z << 1)) & em;
        } else
            m = em;
        n = (*p & ~m) | (P & m);
        if (n != *p) {
            *p = n;
            if (!any) {
                first = i;
                any = 1;
            }
            last = i;
        }
        if (i == bb)
            break;
    }
    if (any)
        mark_bytes(y, first, last);
}

void c_fld_hspan(void)
{
    rowfill(0, pat[fv], 0);
}

void c_fld_vrun(void)
{
    static int16_t a, b;
    static uint8_t x, y, i, *p, n, keep, val;
    if ((uint16_t)fx >= FIELD_W)
        return;
    a = fy < 0 ? 0 : fy;
    b = fx1 > FIELD_H ? FIELD_H : fx1;
    if (a >= b)
        return;
    x = (uint8_t)fx;
    i = x & 3;
    keep = clr[i];
    val = vin[i][fv];
    p = &fld_store[(uint8_t)a][x >> 2];
    for (y = (uint8_t)a; (int16_t)y < b; ++y, p += FIELD_W / 4) {
        n = (*p & keep) | val;
        *p = n;
    }
    mark_col(x, (uint8_t)a, (uint8_t)b);
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
        p = &fld_store[0][x >> 2];
        if (v < T_SPANS_) {
            for (y = 0; y < top; ++y, p += FIELD_W / 4)
                *p = (*p & keep) | sky;
            for (; y < FIELD_H; ++y, p += FIELD_W / 4)
                *p = (*p & keep) | dirt;
        } else {
            for (y = 0; y < FIELD_H; ++y, p += FIELD_W / 4)
                *p = (*p & keep) | sky;
            for (i = 0; i < v - T_SPANS_; ++i) {
                top = e[1 + 2 * i];
                end = e[2 + 2 * i];
                if (end > FIELD_H)
                    end = FIELD_H;
                p = &fld_store[top][x >> 2];
                for (y = top; y < end; ++y, p += FIELD_W / 4)
                    *p = (*p & keep) | dirt;
            }
        }
        mark_col(x, 0, FIELD_H);
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
    fy = y;
    fx = ex - hw;
    fx1 = ex + hw;
    if (emode == 0)
        rowfill(0, pat[fv], 0);
    else if (emode == 1)
        rowfill(1, pat[fv], pat[PX_SKY]);
    else
        rowfill(1, pat[PX_SKY], pat[PX_FX]);
}

/* A row dy is as wide as the largest hw with hw^2 ry^2 + dy^2 rx^2 <= rx^2 ry^2
 * (view.go's). Both terms are stepped by adds: hw^2 ry^2 falls by
 * (2 hw - 1) ry^2 a step, which itself falls by 2 ry^2; dy^2 rx^2 rises
 * by (2 dy - 1) rx^2. */
void c_fld_ellipse(void)
{
    static uint8_t dy, hw;
    static uint32_t full, rxs, rys, row, rinc, xx, xd;
    if (!ery)
        ery = 1;
    if (!erx)
        erx = 1;
    rxs = (uint32_t)erx * erx;
    rys = (uint32_t)ery * ery;
    full = rxs * rys;
    hw = erx;
    xx = full;                          /* hw^2 ry^2 */
    xd = (uint32_t)(2 * (uint16_t)erx - 1) * rys;   /* what it falls by when hw does */
    row = 0;                            /* dy^2 rx^2 */
    rinc = rxs;                         /* what it rises by to the next dy: (2 dy + 1) rx^2 */
    for (dy = 0; dy <= ery; ++dy) {
        while (hw && xx + row > full) {
            xx -= xd;
            xd -= rys + rys;
            --hw;
        }
        erow(ey - dy, hw);
        if (dy)
            erow(ey + dy, hw);
        row += rinc;
        rinc += rxs + rxs;
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
    static uint8_t t, u, m;
    p = &fld_store[(uint8_t)(cy << 1)][cx >> 1];
    t = p[0];
    u = p[FIELD_W / 4];
    if (cx & 1) {
        t >>= 4;
        u >>= 4;
    }
    /* (nibbles: the upper blocks' bits 0-1 and 2-3, the lower's the same) */
    if ((t & (t >> 1) & 5) | (u & (u >> 1) & 5))
        return 'i';                     /* a fire block: the checkerboard, whatever it burns */
    m = ((t >> 1) & 1) | ((t >> 2) & 2) | ((u << 1) & 4) | (u & 8);
    return fld_glyph[m];
}
