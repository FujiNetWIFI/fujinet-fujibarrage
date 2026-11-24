/*
 * scr.c -- the text screens (scr.h) on the H19: 24 rows of 80 characters
 * in bg (h19.h), whose difference from what the terminal shows h19_flush
 * sends. No colours: a header, the title and the selected row are reverse
 * video (scr_row), the rest plain.
 */
#include <string.h>
#include "scr.h"
#include "game.h"
#include "h19.h"
#include "h89.h"
#include "fld.h"

uint8_t scr_seq;
uint8_t field_up;               /* (here, not field.c: resident under HDOS's overlays) */

/* the text screen gives way: the field is drawn again when it is opened */
void field_down(void)
{
    field_up = 0;
}

static uint8_t rowrev[SCR_ROWS];        /* the row's cells are reverse video */

void scr_init(void)
{
}

void scr_begin(void)
{
    uint8_t y;
    ++scr_seq;
    field_down();
    for (y = 0; y < SCR_ROWS; ++y)
        rowrev[y] = 0;
    h19_blank(0, H19_ROWS);
    h19_cleared();
}

/* the row's palette: titles, headers and the chosen row are reverse video */
void scr_row(uint8_t row, uint8_t pal)
{
    static uint8_t x, r, c, *p;
    if (row >= SCR_ROWS)
        return;
    r = pal == PAL_TITLE || pal == PAL_HEAD || pal == PAL_HILITE;
    if (r == rowrev[row])
        return;
    rowrev[row] = r;
    p = bg[row];
    for (x = 0; x < H19_COLS; ++x) {
        c = p[x];
        p[x] = r ? (c | REV) : (c & ~REV);
    }
    h19_span(row, 0, H19_COLS);
}

void scr_text(uint8_t row, uint8_t col, const char *s)
{
    if (row < SCR_ROWS && col < H19_COLS)
        h19_str(col, row, s, rowrev[row] ? REV : 0);
}

void scr_inverse(uint8_t row, uint8_t col, uint8_t n)
{
    static uint8_t x, *p;
    if (row >= SCR_ROWS)
        return;
    p = bg[row];
    for (x = col; n && x < H19_COLS; ++x, --n)
        p[x] |= REV;
    h19_span(row, col, x);
}

void scr_clear_row(uint8_t row)
{
    if (row >= SCR_ROWS)
        return;
    memset(bg[row], rowrev[row] ? (' ' | REV) : ' ', H19_COLS);
    h19_span(row, 0, H19_COLS);
}

/* rows row..row+n-1 move a row: up (the second takes the first's place) or
 * down; the row left over is the caller's to draw */
void scr_scroll(uint8_t row, uint8_t n, uint8_t up)
{
    static uint8_t y;
    if (n < 2 || row + n > SCR_ROWS)
        return;
    if (up) {
        for (y = row; y + 1 < row + n; ++y) {
            memcpy(bg[y], bg[y + 1], H19_COLS);
            rowrev[y] = rowrev[y + 1];
        }
    } else {
        for (y = row + n - 1; y > row; --y) {
            memcpy(bg[y], bg[y - 1], H19_COLS);
            rowrev[y] = rowrev[y - 1];
        }
    }
    h19_scroll(row, n, up);
}

void scr_show(void)
{
}
