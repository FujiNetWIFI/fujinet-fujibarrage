/*
 * scr.c -- text screens (scr.h) on the 7800: 24 rows of 40 in MARIA's
 * 320A, the Namco font (gen/atari7800/font.s: a glyph's code is its Atari
 * screen code, + 64 inverse, + 128 a label's), each row its own background
 * and inks (maria.s's DLI: P0C2 text, P1C2 labels, P2C2 a name).
 *
 * A row's 40 bytes (TXT_ROW) are its characters as MARIA reads them; its
 * display list is the runs of one ink (row_dl), built into the slot not
 * shown and shown from the next frame. The field screen's HUD rows are the
 * same (src/atari7800/fieldm.c), with a name's run in P2.
 */
#ifdef __ATARI7800__

#include <string.h>
#include "a78.h"
#include "scr.h"
#include "field.h"
#include "plat.h"

/* the seats' colours (the tanks, the rooms' rows: the Atari's) */
const uint8_t seat_col[10] = { 0x36, 0x86, 0xC8, 0x1C, 0x58, 0xAC, 0x2A, 0x0E, 0x4C, 0xE6 };

/* background, text, labels */
static const uint8_t pal[][3] = {
    { 0x92, 0x0E, 0x1C },       /* TEXT: white on blue, labels yellow */
    { 0x34, 0x0E, 0x1C },       /* TITLE: white on red */
    { 0x06, 0x0E, 0x1C },       /* HEAD: white on grey */
    { 0x1C, 0x00, 0x00 },       /* HILITE: black on yellow */
    { 0x92, 0x08, 0x1C },       /* DIM */
};

#define MAXRUNS 12              /* 5 bytes each: a 64-byte slot */

/* Zone z's list from row map m (40 bytes): its runs of one ink (bit 7: a
 * label's; cells nc..nc+nn-1 a name's), 32 cells at most, blanks and all:
 * so a row's list changes only with its inks, not its words, and a scroll
 * (the rows' bytes moved) needs none (m_put leaves a list that's the same).
 * A row all blank has none. */
uint8_t row_kind[NTROWS];          /* a text row's list: 0 none (blank), 1 one ink, 2 more */

void row_dl(uint8_t z, const uint8_t *m, uint8_t nc, uint8_t nn)
{
    uint8_t *d = m_nd, c, s, cls, k, w, runs = 0;
    uint16_t a;
    for (c = 0; c < 40 && !(m[c] & 0x7F) && (uint8_t)(c - nc) >= nn; ++c)
        ;
    if (c < 40) {
        c = 0;
        while (c < 40 && runs < MAXRUNS) {
            cls = (uint8_t)(c - nc) < nn ? 2 : m[c] >> 7;
            s = c;
            for (++c; c < 40 && c - s < 32; ++c) {
                k = (uint8_t)(c - nc) < nn ? 2 : m[c] >> 7;
                if (k != cls)
                    break;
            }
            w = c - s;
            a = (uint16_t)(m + s);
            d[0] = (uint8_t)a;
            d[1] = 0x60;                /* indirect, 1-byte characters */
            d[2] = a >> 8;
            d[3] = (uint8_t)(cls << 5) | (uint8_t)(32 - w);
            d[4] = s << 2;
            d += 5;
            ++runs;
        }
    }
    d[0] = 0;
    d[1] = 0;
    m_ndlen = (uint8_t)(d - m_nd) + 2;
    m_put(z);
    if (z < NTROWS)
        row_kind[z] = !runs ? 0 : runs <= 2 && !nn && !(m[0] & 0x80) && !(m[39] & 0x80) ? 1 : 2;
}

/* A row's colours are staged, and shown at the next frame_wait (or
 * scr_show) all at once: the shop's bar leaves one row and reaches the next
 * in the same frame, though the new row's words come between. */
static uint8_t s_bg[NTROWS], s_ink[NTROWS], s_lab[NTROWS], s_new;

static void colours(uint8_t r, uint8_t p)
{
    s_new = 1;
    if (p >= PAL_SEAT) {
        s_bg[r] = hue(seat_col[p - PAL_SEAT]) & 0xF4;   /* darker, for white text */
        s_ink[r] = 0x0E;
        s_lab[r] = 0x0E;
        return;
    }
    s_bg[r] = hue(pal[p][0]);
    s_ink[r] = pal[p][1];
    s_lab[r] = hue(pal[p][2]);
}

/* the staged colours shown (plat.c's frame_wait, scr_show) */
void scr_commit(void)
{
    if (!s_new)
        return;
    s_new = 0;
    memcpy(r_bg, s_bg, NTROWS);
    memcpy(r_ink, s_ink, NTROWS);
    memcpy(r_lab, s_lab, NTROWS);
}

void scr_init(void)
{
    m_init();
    scr_begin();                        /* (field_init comes after the boot code) */
    scr_show();
}

extern uint8_t m_fshown;                /* fieldm.c: the HUD's rows are the text's now */

void scr_begin(void)
{
    uint8_t r;
    m_show(M_BLANK);
    m_fshown = 0;
    for (r = 0; r < SCR_ROWS; ++r) {
        memset(TXT_ROW(r), 0, SCR_COLS);
        colours(r, PAL_TEXT);
        row_dl(r, TXT_ROW(r), 0, 0);
    }
}

void scr_row(uint8_t row, uint8_t p)
{
    colours(row, p);
}

static void put(uint8_t row, uint8_t col, const char *s, uint8_t lab)
{
    uint8_t *d = TXT_ROW(row) + col;
    uint8_t *e = TXT_ROW(row) + SCR_COLS;
    uint8_t c;
    while ((c = (uint8_t)*s++) != 0 && d < e) {
        c &= 0x7F;
        if (c >= 'a' && c <= 'z')
            c -= 32;
        *d++ = (c < 32 || c >= 96 ? 0 : c - 32) | lab;
    }
    row_dl(row, TXT_ROW(row), 0, 0);
}

void scr_text(uint8_t row, uint8_t col, const char *s)
{
    put(row, col, s, 0);
}

void scr_label(uint8_t row, uint8_t col, const char *s)
{
    put(row, col, s, 0x80);
}

void scr_inverse(uint8_t row, uint8_t col, uint8_t n)
{
    uint8_t *d = TXT_ROW(row) + col;
    while (n--)
        *d++ ^= 0x40;
    row_dl(row, TXT_ROW(row), 0, 0);    /* (an inverse blank isn't one) */
}

void txt_scroll(uint8_t row, uint8_t n, uint8_t up);   /* hud.s */

/* The rows' bytes move ahead of the raster (hud.s, just after the frame's
 * DLI), the row left over is blank (not the row it came from twice, while
 * the caller works out what goes there); a row's list is built again only
 * if its kind changed (one-ink rows all have the same two runs). */
void scr_scroll(uint8_t row, uint8_t n, uint8_t up)
{
    uint8_t i, e = row + n - 1, v = up ? e : row, was;
    if (n < 2)
        return;
    txt_scroll(row, n, up);
    for (i = row; i <= e; ++i) {
        was = row_kind[i];
        if (i == v)
            row_kind[i] = 0xFF;             /* (cleared below) */
        else
            row_kind[i] = row_kind[up ? i + 1 : i - 1];
        if (row_kind[i] == 2 || row_kind[i] != was)
            row_dl(i, TXT_ROW(i), 0, 0);
    }
}

void scr_clear_row(uint8_t row)
{
    memset(TXT_ROW(row), 0, SCR_COLS);
    row_dl(row, TXT_ROW(row), 0, 0);
}

void scr_show(void)
{
    scr_commit();
    m_show(M_TEXT);
}

#endif
