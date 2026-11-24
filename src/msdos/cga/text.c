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
#include <dos.h>
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

static void nib_cells(uint8_t row, uint8_t col, uint8_t n, uint8_t inv, uint8_t ink);

/* A label's ink (text.h; labels in another colour than values: the user,
 * 2026-11-23): cyan on the text's and the title's rows, the fire's (index
 * 2) on a heading's and the HUD's; the other looks' labels plain. No names
 * in colour: the tanks are all one colour. */
static uint8_t lab_ink(uint8_t p)
{
    return p == PAL_TEXT || p == PAL_TITLE ? 1 : p == PAL_HEAD || p == TP_HUD ? 2 : 0xFF;
}

static void colours_k(uint8_t row, uint8_t kind, uint8_t *ink, uint8_t *ground)
{
    uint8_t l;
    colours(row, ink, ground);
    if (kind == INK_LABEL && (l = lab_ink(txt_pal[row])) != 0xFF)
        *ink = l;
}

/* The text's own looks (TEXT, TITLE, HEAD, HILITE, DIM and the HUD's),
 * and their labels', with all 96 glyphs drawn, 1.5K each (far, each got
 * at first use): a cell is then 8 word loads and stores (cga_grun). Seat
 * rows and inverse cells keep the nibble tables. (TODO 2026-11-23.) */
#define LOOKS (PAL_SEAT + 1)            /* (the last: the HUD's) */
static uint16_t gseg[LOOKS * 2];
static uint8_t gfail;

static uint16_t glyphs(uint8_t k, uint8_t row, uint8_t kind)
{
    uint8_t __far *d;
    const uint8_t *g;
    uint8_t ink, ground, b;
    uint16_t c;
    unsigned seg;
    if (gseg[k] || gfail)
        return gseg[k];
    if (_dos_allocmem(96, &seg)) {
        gfail = 1;
        return 0;
    }
    colours_k(row, kind, &ink, &ground);
    make(ink, ground);
    d = MK_FP(seg, 0);
    for (g = font8, c = 0; c < 96 * 8; ++c) {
        b = *g++;
        *d++ = tab[b >> 4];
        *d++ = tab[b & 15];
    }
    return gseg[k] = seg;
}

void txt_cells(uint8_t row, uint8_t col, uint8_t n, uint8_t inv, uint8_t ink)
{
    uint8_t p = txt_pal[row];
    uint16_t seg;
    if (ink != INK_LABEL || lab_ink(p) == 0xFF)
        ink = INK_PLAIN;                /* (no names in colour here) */
    if (!inv && (p < PAL_SEAT || p == TP_HUD)) {
        seg = glyphs((uint8_t)((p == TP_HUD ? PAL_SEAT : p) + (ink ? LOOKS : 0)), row, ink);
        if (seg) {
            cga_grun((uint16_t)row * 320 + col * 2, scr_ch[row] + col, n, seg);
            return;
        }
    }
    nib_cells(row, col, n, inv, ink);
}

static void nib_cells(uint8_t row, uint8_t col, uint8_t n, uint8_t inv, uint8_t ink)
{
    uint8_t in, ground;
    colours_k(row, ink, &in, &ground);
    if (inv)
        make(ground, in);
    else
        make(in, ground);
    cga_run((uint16_t)row * 320 + col * 2, scr_ch[row] + col, n, tab);
}

/* rows from.. (n of them) moved to to.. on the screen (scr_scroll): a
 * row is 320 bytes in each bank, one after another. (On a CGA moving them
 * is quicker than redrawing the cells that differ: its memory is slow to
 * write, an 8088's C slower to compare.) */
void txt_move(uint8_t to, uint8_t from, uint8_t n)
{
    if (to < from)
        cga_move((uint16_t)to * 320, (uint16_t)from * 320, (uint16_t)n * 320);
    else
        while (n--)                     /* (down: a row at a time, from the bottom) */
            cga_move((uint16_t)(to + n) * 320, (uint16_t)(from + n) * 320, 320);
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
    memset(scr_ch, 0, sizeof(scr_ch));  /* (nor their cells: scr_text draws what it's given) */
}
#endif
