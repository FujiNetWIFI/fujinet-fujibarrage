/*
 * scr3.c -- the CoCo 3's text (scr.h): 40 columns by 24 rows of 8x8
 * pixels on the GIME's 320x200 screen (src/coco3/video.c), in the Namco
 * font (gen/coco3/font3.asm, drawn by src/coco3/scr3a.asm). Each row has an
 * ink and a ground among the palette's 16 colours; the rows' look is the
 * Atari's (src/atari/scr.c): white on blue, white on red for a title,
 * white on grey for a heading, black on yellow highlighted, a seat's row
 * black on its colour. While a screen is drawn the palette is black
 * (scr_begin); scr_show puts the text palette up (RGB or composite).
 *
 * Rows 22-24 (lines 176-199) are also the field's HUD (scr3_hud), on the
 * field's own palette: white (seat 7's colour) on black.
 */
#include <string.h>
#include "cocoplat.h"
#include "scr.h"
#include "plat.h"
#include "scr3.h"

void scr3_cell(uint8_t row, uint8_t col, uint8_t c, const uint16_t *tab);
void scr3_fill(uint8_t y, uint8_t n, uint8_t b);
extern const uint8_t pal3_text[32];         /* gen/coco3/pal3.asm: RGB, then composite */

#define ROWS (SCR_ROWS + 1)                 /* (the 25th: the HUD's last) */
extern uint8_t scr_ch[ROWS][SCR_COLS];         /* (src/coco/hibss.asm) */
#define ch scr_ch
static uint8_t rink[ROWS], rground[ROWS];
extern uint8_t scr_inv[ROWS][5];            /* cells shown inverse */
#define inv scr_inv

/* TEXT, TITLE, HEAD, HILITE, DIM: ink and ground (gen/coco3/pal3.asm) */
static const uint8_t pal_ink[] = { C3_WHITE, C3_WHITE, C3_WHITE, C3_BLACK, C3_DIM };
static const uint8_t pal_ground[] = { C3_BLUE, C3_RED, C3_GREY, C3_YELLOW, C3_BLUE };

static uint16_t tab[16], itab[16];
static uint8_t tab_ink = 0xFF, tab_ground = 0xFF;

static void make(uint8_t ink, uint8_t ground)
{
    uint8_t n, x;
    uint16_t w, iw;
    if (ink == tab_ink && ground == tab_ground)
        return;
    tab_ink = ink;
    tab_ground = ground;
    for (n = 0; n < 16; ++n) {
        w = iw = 0;
        for (x = 0; x < 4; ++x) {
            w <<= 4;
            iw <<= 4;
            if (n & (8 >> x)) {
                w |= ink;
                iw |= ground;
            } else {
                w |= ground;
                iw |= ink;
            }
        }
        tab[n] = w;
        itab[n] = iw;
    }
}

uint8_t scr_same(const uint8_t *cp, const char *s, uint8_t n);   /* (scr3a.asm) */
static const uint8_t bit8[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };     /* (CMOC shifts by a variable in a loop) */

static void cells(uint8_t row, uint8_t col, uint8_t n)
{
    make(rink[row], rground[row]);
    while (n-- && col < SCR_COLS) {
        scr3_cell(row, col, ch[row][col], inv[row][col >> 3] & bit8[col & 7] ? itab : tab);
        ++col;
    }
}

static uint8_t two(uint8_t c)
{
    return (uint8_t)(c << 4) | c;
}

static void blank_row(uint8_t row)
{
    memset(ch[row], ' ', SCR_COLS);
    memset(inv[row], 0, sizeof(inv[row]));
    scr3_fill((uint8_t)(row << 3), 8, two(rground[row]));
}

static void set_pal(uint8_t row, uint8_t p)
{
    if (p >= PAL_SEAT) {
        rink[row] = C3_BLACK;
        rground[row] = (uint8_t)(C3_SEAT + (p - PAL_SEAT));
    } else {
        rink[row] = pal_ink[p];
        rground[row] = pal_ground[p];
    }
}

/* the palette: all black (a screen being drawn), or the text's */
static void text_palette(uint8_t on)
{
    const uint8_t *p = pal3_text + (coco_swap ? 16 : 0);
    uint8_t i;
    for (i = 0; i < 16; ++i)
        ((uint8_t *)0xFFB0)[i] = on ? p[i] : 0;
    *(uint8_t *)0xFF9A = on ? p[C3_BLUE] : 0;     /* the border: the text's ground */
}

void scr_init(void)
{
    scr_begin();
}

void scr_begin(void)
{
    uint8_t r;
    video_hook = 0;                     /* (CLEAR: the text's palette now) */
    ++scr_seq;
    field_up = 0;
    text_palette(0);
    for (r = 0; r < ROWS; ++r)
        set_pal(r, PAL_TEXT);
    scr3_fill(0, 200, two(C3_BLUE));
    for (r = 0; r < ROWS; ++r) {
        memset(ch[r], ' ', SCR_COLS);
        memset(inv[r], 0, sizeof(inv[r]));
    }
}

void scr_row(uint8_t row, uint8_t p)
{
    uint8_t ink = rink[row], ground = rground[row];
    set_pal(row, p);
    if (ink == rink[row] && ground == rground[row])
        return;
    scr3_fill((uint8_t)(row << 3), 8, two(rground[row]));
    cells(row, 0, SCR_COLS);
}

void scr_text(uint8_t row, uint8_t col, const char *s)
{
    uint8_t c0 = col;
    while (*s && col < SCR_COLS)
        ch[row][col++] = (uint8_t)*s++;
    cells(row, c0, (uint8_t)(col - c0));
}

void scr_inverse(uint8_t row, uint8_t col, uint8_t n)
{
    uint8_t c0 = col, k = n;
    while (k-- && col < SCR_COLS) {
        inv[row][col >> 3] ^= (uint8_t)bit8[col & 7];
        ++col;
    }
    cells(row, c0, n);
}

void scr_clear_row(uint8_t row)
{
    blank_row(row);
}

void scr_scroll(uint8_t row, uint8_t n, uint8_t up)
{
    uint8_t i, last;
    if (n < 2)
        return;
    last = (uint8_t)(row + n - 1);
    if (up) {
        for (i = row; i < last; ++i) {
            memcpy(ch[i], ch[i + 1], SCR_COLS);
            memcpy(inv[i], inv[i + 1], sizeof(inv[i]));
            rink[i] = rink[i + 1];
            rground[i] = rground[i + 1];
        }
    } else {
        for (i = last; i > row; --i) {
            memcpy(ch[i], ch[i - 1], SCR_COLS);
            memcpy(inv[i], inv[i - 1], sizeof(inv[i]));
            rink[i] = rink[i - 1];
            rground[i] = rground[i - 1];
        }
    }
    for (i = row; i <= last; ++i) {             /* (drawn again: the screen can't be copied a row at a time cheaply) */
        scr3_fill((uint8_t)(i << 3), 8, two(rground[i]));
        cells(i, 0, SCR_COLS);
    }
}

void scr_show(void)
{
    frame_wait();
    text_palette(1);
}

/* the palette's monitor changed (video_swap): the text's again, if shown */
#ifdef MENU
/* the title picture (tools/mktitle_coco.py): an 8K block a stream, its own
 * palette (RGB, then composite) */
extern const uint8_t title3[], title3_pal[];
const uint8_t *lz_unpack(uint8_t *dst, const uint8_t *src);

static void title_palette(void)
{
    const uint8_t *p = title3_pal + (coco_swap ? 16 : 0);
    uint8_t i;
    for (i = 0; i < 16; ++i)
        ((uint8_t *)0xFFB0)[i] = p[i];
    *(uint8_t *)0xFF9A = p[0];
}

void scr_picture(void)
{
    const uint8_t *s = title3;
    uint8_t b, r;
    text_palette(0);
    asm { orcc #$50 }
    for (b = 0; b < 7; ++b) {
        *(uint8_t *)0xFFA6 = (uint8_t)(0x30 + b);
        s = lz_unpack((uint8_t *)0xC000, s);
    }
    *(uint8_t *)0xFFA6 = 0x3E;
    asm { andcc #$EF }
    for (r = 0; r < ROWS; ++r)
        rink[r] = rground[r] = 0xFF;
    frame_wait();
    title_palette();
    video_hook = title_palette;
}
#endif

void scr3_repalette(void)
{
    text_palette(1);
}

/* ---- the field's HUD: rows 22-24, white on black (hi: black on white) */
void scr3_hud(uint8_t row, uint8_t col, const char *s, uint8_t hi)
{
    uint8_t r = (uint8_t)(SCR_HUD_ROW + row), c, b, *iv, *cp;
    if (rink[r] != C3_WHITE || rground[r] != C3_BLACK) {
        rink[r] = C3_WHITE;
        rground[r] = C3_BLACK;
        blank_row(r);
    }
    make(C3_WHITE, C3_BLACK);
    cp = ch[r] + col;
    iv = inv[r];
    if (!hi && !(iv[0] | iv[1] | iv[2] | iv[3] | iv[4])) {     /* (most: plain text over plain) */
        for (;;) {
            b = scr_same(cp, s, (uint8_t)(SCR_COLS - col));
            cp += b;
            s += b;
            col += b;
            if (col >= SCR_COLS || !(c = (uint8_t)*s++))
                return;
            *cp++ = c;
            scr3_cell(r, col, c, tab);
            ++col;
        }
    }
    while ((c = (uint8_t)*s++) != 0 && col < SCR_COLS) {
        iv = inv[r] + (col >> 3);
        b = bit8[col & 7];
        if (*cp != c || ((*iv & b) != 0) != hi) {   /* (only the cells that change: OP_DAMAGE redraws it all) */
            *cp = c;
            if (hi)
                *iv |= b;
            else
                *iv &= (uint8_t)~b;
            scr3_cell(r, col, c, hi ? itab : tab);
        }
        ++cp;
        ++col;
    }
}

void scr3_hud_clear(uint8_t row)
{
    uint8_t r = (uint8_t)(SCR_HUD_ROW + row);
    rink[r] = C3_WHITE;
    rground[r] = C3_BLACK;
    blank_row(r);
}
