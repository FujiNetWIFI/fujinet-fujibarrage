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

static uint8_t nm_c[TXT_ROWS], nm_n[TXT_ROWS], nm_s[TXT_ROWS];  /* a row's name: seat ink */

static uint8_t is_inv(uint8_t row, uint8_t col)
{
    return (inv[row][col >> 3] >> (col & 7)) & 1;
}

/* a cell's ink (text.h): its row's name's seat, a label (bit 7) or plain */
static uint8_t ink_of(uint8_t row, uint8_t col)
{
    if (nm_n[row] && col >= nm_c[row] && col < nm_c[row] + nm_n[row])
        return (uint8_t)(INK_SEAT + nm_s[row]);
    return ch[row][col] & 0x80 ? INK_LABEL : INK_PLAIN;
}

/* cells col.. (n) of a row drawn from what they hold, in runs of one look
 * and ink (a call each: a cell a call, the C cost more than the drawing) */
static void cells(uint8_t row, uint8_t col, uint8_t n)
{
    uint8_t k, iv, ik, *ip = inv[row], *cp;
    if (col >= TXT_COLS)
        return;
    if (n > TXT_COLS - col)
        n = (uint8_t)(TXT_COLS - col);
    if (!(ip[0] | ip[1] | ip[2] | ip[3] | ip[4]) && !nm_n[row]) {
        for (cp = ch[row] + col, k = 0; k < n && !(cp[k] & 0x80); ++k)
            ;
        if (k == n) {                   /* (most: plain text) */
            if (n)
                txt_cells(row, col, n, 0, INK_PLAIN);
            return;
        }
    }
    while (n) {
        iv = is_inv(row, col);
        ik = ink_of(row, col);
        for (k = 1; k < n && is_inv(row, (uint8_t)(col + k)) == iv
                    && ink_of(row, (uint8_t)(col + k)) == ik; ++k)
            ;
        txt_cells(row, col, k, iv, ik);
        col += k;
        n -= k;
    }
}

/* n characters from s at (row, col): only the cells that change drawn, a
 * run at a time (one unchanged between two changes is drawn with them:
 * about what another call costs); the unchanged passed over by txt_same */
static void put_n(uint8_t row, uint8_t col, const uint8_t *s, uint8_t n)
{
    uint8_t *cp = ch[row], k, from;
    if (col >= TXT_COLS)
        return;
    if (n > TXT_COLS - col)
        n = (uint8_t)(TXT_COLS - col);
    while (n) {
        k = txt_same(cp + col, s, n);
        col += k;
        s += k;
        n -= k;
        if (!n)
            return;
        from = col;
        for (;;) {                      /* a change, and the few unchanged after it */
            cp[col++] = *s++;
            if (!--n)
                break;
            k = txt_same(cp + col, s, n);
            if (k > 1 || k == n)
                break;
            col += k;
            s += k;
            n -= k;
        }
        cells(row, from, (uint8_t)(col - from));
    }
}

void scr_redraw(uint8_t row)           /* (the 40 cells cover the row) */
{
    cells(row, 0, TXT_COLS);
}

static void blank_row(uint8_t row)
{
    nm_n[row] = 0;
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

/* only the cells that change are drawn (a row is often written again as
 * it was: the shop's each second, a row after scr_row redrew it) */
void scr_text(uint8_t row, uint8_t col, const char *s)
{
    put_n(row, col, (const uint8_t *)s, (uint8_t)strlen(s));
}

/* a label: its characters' bit 7 set (scr.h) */
void scr_label(uint8_t row, uint8_t col, const char *s)
{
    uint8_t b[TXT_COLS], n = 0;
    while (*s && n < TXT_COLS)
        b[n++] = (uint8_t)(*s++ | 0x80);
    put_n(row, col, b, n);
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

#ifdef DOS_VGA
/* row to shows what row from holds (scr_scroll): only the cells that
 * differ are drawn (a list's neighbouring rows share their look and about
 * two thirds of their cells: the shop's 13 of 40 differ, on average; moving
 * the pixels took longer over the ISA bus) */
static void row_from(uint8_t to, uint8_t from)
{
    if (txt_pal[to] != txt_pal[from] || memcmp(inv[to], inv[from], sizeof(inv[to]))) {
        memcpy(ch[to], ch[from], TXT_COLS);
        memcpy(inv[to], inv[from], sizeof(inv[to]));
        txt_pal[to] = txt_pal[from];
        cells(to, 0, TXT_COLS);
    } else
        put_n(to, 0, ch[from], TXT_COLS);
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
#else
/* the CGA's: the rows and their pixels move (text.c txt_move: 89 ms a
 * shop scroll on a 4.77 MHz PC, the cells that differ 123) */
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
        txt_move(row, (uint8_t)(row + 1), (uint8_t)(n - 1));
    } else {
        for (i = last; i > row; --i) {
            memcpy(ch[i], ch[i - 1], TXT_COLS);
            memcpy(inv[i], inv[i - 1], sizeof(inv[i]));
            txt_pal[i] = txt_pal[i - 1];
        }
        txt_move((uint8_t)(row + 1), row, (uint8_t)(n - 1));
    }
}
#endif

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
            txt_cells(r, col, 1, hi, ink_of(r, col));
        }
        ++cp;
        ++col;
    }
}

/* a HUD row's name, in its seat's colour (field.c's field_name): the cells
 * it leaves and the ones it takes drawn again */
void scr_hud_name(uint8_t row, uint8_t col, uint8_t n, uint8_t seat)
{
    uint8_t r = (uint8_t)(HUD_ROW + row), oc = nm_c[r], on = nm_n[r];
    if (oc == col && on == n && (!n || nm_s[r] == seat))
        return;
    nm_c[r] = col;
    nm_n[r] = n;
    nm_s[r] = seat;
    if (txt_pal[r] != TP_HUD)
        return;
    if (on)
        cells(r, oc, on);
    if (n)
        cells(r, col, n);
}

void scr_hud_clear(uint8_t row)
{
    uint8_t r = (uint8_t)(HUD_ROW + row);
    txt_pal[r] = TP_HUD;
    blank_row(r);
}

