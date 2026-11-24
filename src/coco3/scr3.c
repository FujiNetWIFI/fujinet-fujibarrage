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
#include "scr3i.h"

void scr3_fill(uint8_t y, uint8_t n, uint8_t b);
extern const uint8_t pal3_text[32];         /* gen/coco3/pal3.asm: RGB, then composite */

#define ch scr_ch
uint8_t scr3_rink[ROWS], scr3_rground[ROWS];  /* (scr3i.h: scr3k.c's too) */
#define rink scr3_rink
#define rground scr3_rground
#define inv scr_inv

/* TEXT, TITLE, HEAD, HILITE, DIM: ink and ground (gen/coco3/pal3.asm) */
static const uint8_t pal_ink[] = { C3_WHITE, C3_WHITE, C3_WHITE, C3_BLACK, C3_DIM };
static const uint8_t pal_ground[] = { C3_BLUE, C3_RED, C3_GREY, C3_YELLOW, C3_BLUE };

uint16_t scr3_tab[16], scr3_itab[16];         /* the row's, inverse (scr3i.h) */
#define tab scr3_tab
#define itab scr3_itab
static uint8_t tab_ink = 0xFF, tab_ground = 0xFF;

static void make(uint8_t ink, uint8_t ground)
{
    if (ink == tab_ink && ground == tab_ground)
        return;
    tab_ink = ink;
    tab_ground = ground;
    scr3_tabfill(tab, ink, ground);
    scr3_tabfill(itab, ground, ink);
}

uint8_t scr_same(const uint8_t *cp, const char *s, uint8_t n, uint8_t m);   /* (scr3a.asm) */
static const uint8_t bit8[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };     /* (CMOC shifts by a variable in a loop) */

/* cells col.. (n) of a row drawn from what they hold: a run a call (a
 * cell a call, the C cost more than the drawing; scr3_run takes a label's
 * table by its bit 7); a row with inverse cells or a name a cell at a
 * time (scr3k.c) */
void scr3_cells(uint8_t row, uint8_t col, uint8_t n)
{
    uint8_t *ip = inv[row], h = (uint8_t)(row - SCR_HUD_ROW);
    make(rink[row], rground[row]);
    if (!(ip[0] | ip[1] | ip[2] | ip[3] | ip[4]) && (h >= 3 || !scr3_nmn[h]))
        scr3_run(row, col, ch[row] + col, n, tab,
                 scr3_tabfor(row, rink[row] == C3_WHITE ? C3_LABEL : 0xFF));
    else
        scr3_cellk(row, col, n);
}
#define cells scr3_cells

/* n characters from s at (row, col), each OR m (80h: a label's): only the
 * cells that change drawn, a run at a time (the unchanged passed over by
 * scr_same) */
static void put_n(uint8_t row, uint8_t col, const uint8_t *s, uint8_t n, uint8_t m)
{
    uint8_t *cp = ch[row], k, from;
    if (col >= SCR_COLS)
        return;
    if (n > SCR_COLS - col)
        n = (uint8_t)(SCR_COLS - col);
    while (n) {
        k = scr_same(cp + col, (const char *)s, n, m);
        col += k;
        s += k;
        n -= k;
        from = col;
        for (; n && cp[col] != (uint8_t)(*s | m); --n)
            cp[col++] = (uint8_t)(*s++ | m);
        if (col != from)
            cells(row, from, (uint8_t)(col - from));
    }
}

static uint8_t two(uint8_t c)
{
    return (uint8_t)(c << 4) | c;
}

static void blank_row(uint8_t row)
{
    if ((uint8_t)(row - SCR_HUD_ROW) < 3)
        scr3_nmn[row - SCR_HUD_ROW] = 0;
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
    cells(row, 0, SCR_COLS);            /* (the 40 cells cover the row) */
}

/* only the cells that change are drawn (a row is often written again as
 * it was: the shop's each second, a row after scr_row redrew it) */
void scr_text(uint8_t row, uint8_t col, const char *s)
{
    put_n(row, col, (const uint8_t *)s, (uint8_t)strlen(s), 0);
}

void scr_label(uint8_t row, uint8_t col, const char *s)     /* (scr.h) */
{
    put_n(row, col, (const uint8_t *)s, (uint8_t)strlen(s), 0x80);
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

/* row to shows what row from holds (scr_scroll): only the cells that
 * differ are drawn (a list's neighbouring rows share their look and about
 * two thirds of their cells: the shop's 13 of 40 differ, on average) */
static void row_from(uint8_t to, uint8_t from)
{
    if (rink[to] != rink[from] || rground[to] != rground[from]
        || memcmp(inv[to], inv[from], sizeof(inv[to]))) {
        memcpy(ch[to], ch[from], SCR_COLS);
        memcpy(inv[to], inv[from], sizeof(inv[to]));
        rink[to] = rink[from];
        rground[to] = rground[from];
        cells(to, 0, SCR_COLS);
    } else
        put_n(to, 0, ch[from], SCR_COLS, 0);
}

void scr_scroll(uint8_t row, uint8_t n, uint8_t up)
{
    uint8_t i, last;
    if (n < 2)
        return;
    last = (uint8_t)(row + n - 1);
    if (up)
        for (i = row; i < last; ++i)
            row_from(i, (uint8_t)(i + 1));
    else
        for (i = last; i > row; --i)
            row_from(i, (uint8_t)(i - 1));
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
    memset(ch, 0, sizeof(ch));          /* (nor their cells: scr_text draws what it's given) */
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
    cp = ch[r] + col;
    iv = inv[r];
    if (!hi && !(iv[0] | iv[1] | iv[2] | iv[3] | iv[4])) {     /* (most: plain text over plain) */
        for (;;) {
            b = scr_same(cp, s, (uint8_t)(SCR_COLS - col), 0);
            cp += b;
            s += b;
            col += b;
            if (col >= SCR_COLS || !(c = (uint8_t)*s++))
                return;
            *cp++ = c;
            cells(r, col, 1);
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
            cells(r, col, 1);
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
