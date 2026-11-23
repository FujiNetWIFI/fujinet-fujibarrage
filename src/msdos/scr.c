/*
 * scr.c -- the PC's text screens (scr.h): 40 columns by 24 rows of 8x8
 * cells in the Namco font, on the same 320x200 screen as the field, both
 * builds' (the CoCo 3's src/coco3/scr3.c). This keeps what each cell
 * holds and each row's look (txt_pal: PAL_*, a seat, the HUD); each build
 * draws the cells in its own colours (text.h: src/msdos/cga/text.c,
 * src/msdos/vga/text.c). Rows 22-24 (lines 176-199) are also the field's
 * HUD (scr_hud), which draws only the cells that change.
 */
#include <string.h>
#include "scr.h"
#include "plat.h"
#include "text.h"
#include "layout.h"

uint8_t scr_ch[TXT_ROWS][TXT_COLS];
#define ch scr_ch
static uint8_t inv[TXT_ROWS][5];            /* cells shown inverse, a bit each */
uint8_t txt_pal[TXT_ROWS];
uint8_t scr_seq, field_up;

static uint8_t is_inv(uint8_t row, uint8_t col)
{
    return (inv[row][col >> 3] >> (col & 7)) & 1;
}

static void cells(uint8_t row, uint8_t col, uint8_t n)
{
    while (n-- && col < TXT_COLS) {
        txt_cell(row, col, ch[row][col], is_inv(row, col));
        ++col;
    }
}

void scr_redraw(uint8_t row)
{
    txt_blank(row);
    cells(row, 0, TXT_COLS);
}

static void blank_row(uint8_t row)
{
    memset(ch[row], ' ', TXT_COLS);
    memset(inv[row], 0, sizeof(inv[row]));
    txt_blank(row);
}

void scr_init(void)
{
    scr_begin();
}

void scr_begin(void)
{
    uint8_t r;
    ++scr_seq;
    field_up = 0;
    txt_begin();
    for (r = 0; r < TXT_ROWS; ++r) {
        txt_pal[r] = PAL_TEXT;
        blank_row(r);
    }
}

void scr_row(uint8_t row, uint8_t p)
{
    if (txt_pal[row] == p)
        return;
    txt_pal[row] = p;
    scr_redraw(row);
}

void scr_text(uint8_t row, uint8_t col, const char *s)
{
    uint8_t c0 = col;
    while (*s && col < TXT_COLS)
        ch[row][col++] = (uint8_t)*s++;
    cells(row, c0, (uint8_t)(col - c0));
}

void scr_inverse(uint8_t row, uint8_t col, uint8_t n)
{
    uint8_t c0 = col, k = n;
    while (k-- && col < TXT_COLS) {
        inv[row][col >> 3] ^= (uint8_t)(1 << (col & 7));
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
            memcpy(ch[i], ch[i + 1], TXT_COLS);
            memcpy(inv[i], inv[i + 1], sizeof(inv[i]));
            txt_pal[i] = txt_pal[i + 1];
        }
    } else {
        for (i = last; i > row; --i) {
            memcpy(ch[i], ch[i - 1], TXT_COLS);
            memcpy(inv[i], inv[i - 1], sizeof(inv[i]));
            txt_pal[i] = txt_pal[i - 1];
        }
    }
    for (i = row; i <= last; ++i)
        scr_redraw(i);
}

void scr_show(void)
{
    frame_wait();
    txt_show();
}

/* ---- the field's HUD: rows 22-24 in the HUD's look (hi: inverse) ---- */
void scr_hud(uint8_t row, uint8_t col, const char *s, uint8_t hi)
{
    uint8_t r = (uint8_t)(HUD_ROW + row), c, b, *cp, *iv;
    if (txt_pal[r] != TP_HUD) {
        txt_pal[r] = TP_HUD;
        blank_row(r);
    }
    cp = ch[r] + col;
    while ((c = (uint8_t)*s++) != 0 && col < TXT_COLS) {
        iv = inv[r] + (col >> 3);
        b = (uint8_t)(1 << (col & 7));
        if (*cp != c || ((*iv & b) != 0) != hi) {   /* (only the cells that change: OP_DAMAGE redraws it all) */
            *cp = c;
            if (hi)
                *iv |= b;
            else
                *iv &= (uint8_t)~b;
            txt_cell(r, col, c, hi);
        }
        ++cp;
        ++col;
    }
}

void scr_hud_clear(uint8_t row)
{
    uint8_t r = (uint8_t)(HUD_ROW + row);
    txt_pal[r] = TP_HUD;
    blank_row(r);
}

