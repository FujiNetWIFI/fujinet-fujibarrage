/*
 * scr.c -- text screens (scr.h) on the Apple II: text page 1, 40x24, the
 * machine's own characters. No colours: a row's palette is normal (white
 * on black) or inverse (TITLE, HEAD and HILITE: black on white, so a
 * title is a white bar); scr_inverse swaps cells between the two.
 */
#ifdef __APPLE2__

#include <string.h>
#include "scr.h"
#include "plat.h"

void text_on(void);             /* lowlevel.s */
/* txt.s */
void __fastcall__ txt_put(const char *s);
extern uint8_t *txt_dst;
extern uint8_t txt_max, txt_inv;
#pragma zpsym ("txt_dst")
#pragma zpsym ("txt_max")
#pragma zpsym ("txt_inv")

/* the rows' addresses: $0400 + (r & 7) * $80 + (r >> 3) * $28 */
static const uint16_t rowad[SCR_ROWS] = {
    0x400, 0x480, 0x500, 0x580, 0x600, 0x680, 0x700, 0x780,
    0x428, 0x4A8, 0x528, 0x5A8, 0x628, 0x6A8, 0x728, 0x7A8,
    0x450, 0x4D0, 0x550, 0x5D0, 0x650, 0x6D0, 0x750, 0x7D0,
};
static uint8_t inv[SCR_ROWS];   /* 1: the row is inverse */

#define ROW(r) ((uint8_t *)rowad[r])

/* a cell the other way: normal <-> inverse */
static uint8_t flip(uint8_t c)
{
    if (c & 0x80)
        return c & 0x3F;
    return c < 0x20 ? c | 0xC0 : c | 0x80;
}

void scr_init(void)
{
    scr_begin();
    scr_show();
}

void scr_begin(void)
{
    uint8_t r;
    for (r = 0; r < SCR_ROWS; ++r) {
        inv[r] = 0;
        memset(ROW(r), 0xA0, SCR_COLS);
    }
}

void scr_row(uint8_t row, uint8_t p)
{
    uint8_t want = p == PAL_TITLE || p == PAL_HEAD || p == PAL_HILITE, i;
    uint8_t *d;
    if (want == inv[row])
        return;
    inv[row] = want;
    d = ROW(row);
    for (i = 0; i < SCR_COLS; ++i)
        d[i] = flip(d[i]);
}

/* s at (row, col), as the row is (normal or inverse): txt.s */
static void put(uint8_t row, uint8_t col, const char *s, uint8_t v)
{
    if (col >= SCR_COLS)
        return;
    txt_dst = ROW(row) + col;
    txt_max = SCR_COLS - col;
    txt_inv = v;
    txt_put(s);
}

void scr_text(uint8_t row, uint8_t col, const char *s)
{
    put(row, col, s, inv[row]);
}

void scr_inverse(uint8_t row, uint8_t col, uint8_t n)
{
    uint8_t *d = ROW(row);
    while (n-- && col < SCR_COLS) {
        d[col] = flip(d[col]);
        ++col;
    }
}

void scr_scroll(uint8_t row, uint8_t n, uint8_t up)
{
    uint8_t i;
    if (n < 2)
        return;
    if (up) {
        for (i = row; i < row + n - 1; ++i) {
            memcpy(ROW(i), ROW(i + 1), SCR_COLS);
            inv[i] = inv[i + 1];
        }
    } else {
        for (i = row + n - 1; i > row; --i) {
            memcpy(ROW(i), ROW(i - 1), SCR_COLS);
            inv[i] = inv[i - 1];
        }
    }
}

void scr_clear_row(uint8_t row)
{
    memset(ROW(row), inv[row] ? 0x20 : 0xA0, SCR_COLS);
}

void scr_show(void)
{
    frame_wait();
    text_on();
}

/* the field's HUD (src/apple2/field.c): text rows 20-23 under mixed mode */
void scr_hud(uint8_t row, uint8_t col, const char *s)
{
    put(20 + row, col, s, 0);
}

void scr_hud_clear(uint8_t row)
{
    memset(ROW(20 + row), 0xA0, SCR_COLS);
}

/* the field's text page: rows 0-19 inverse spaces (white: a flash shows
 * them, full text for a moment), rows 20-23 the HUD, blank */
void scr_for_field(void)
{
    uint8_t r;
    for (r = 0; r < SCR_ROWS; ++r) {
        inv[r] = r < 20;
        memset(ROW(r), r < 20 ? 0x20 : 0xA0, SCR_COLS);
    }
}

#endif
