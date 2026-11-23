/*
 * scr2.c -- the CoCo 1/2's text (scr.h): 32 columns by 24 rows, a cell a
 * byte (4 colour pixels) by 8 lines of the PMODE 4 screen
 * (src/coco2/video.c), in a 3x7 font (gen/coco2/font2.asm, drawn by
 * src/coco2/scr2a.asm). Each row has an ink and a ground among the four
 * artifact colours, which are the field's values: 0 black, 1 blue, 2
 * orange, 3 white (phys: through the artifact phase, coco_swap). The rows'
 * look is the Atari's (src/atari/scr.c): white on blue, white on orange for
 * a title, and so on.
 *
 * Rows 21-23 are also the field's HUD (scr2_hud: white on black), under
 * the field's 168 lines.
 */
#include <string.h>
#include "cocoplat.h"
#include "scr.h"
#include "plat.h"
#include "scr2.h"

#define SCREEN ((uint8_t *)0x1000)
#define ROWAD(r) (SCREEN + ((uint16_t)(r) << 8))    /* 32 bytes x 8 lines a row */

void scr2_cell(uint8_t *dst, uint8_t c, const uint8_t *tab);

extern uint8_t scr_ch[SCR_ROWS][SCR_COLS];         /* (src/coco/hibss.asm) */
#define ch scr_ch
static uint8_t rink[SCR_ROWS], rground[SCR_ROWS];
extern uint8_t scr_inv[SCR_ROWS][5];               /* cells shown inverse */
#define inv scr_inv

/* TEXT, TITLE, HEAD, HILITE, DIM: ink and ground (a heading white on
 * black: on white it melts into PMODE 4's buff border) */
static const uint8_t pal_ink[] = { 3, 3, 3, 0, 0 };
static const uint8_t pal_ground[] = { 1, 2, 0, 3, 1 };
/* a seat's row: black on orange, white or blue in turn */
static const uint8_t seat_ground[3] = { 2, 3, 1 };

static uint8_t tab[16], itab[16], tab_ink = 0xFF, tab_ground = 0xFF;

/* a value as the screen's bit pair, through the artifact phase */
uint8_t phys(uint8_t v)
{
    return coco_swap && (v == 1 || v == 2) ? v ^ 3 : v;
}

/* a value as a whole byte: four pixels of it */
uint8_t phys4(uint8_t v)
{
    static const uint8_t four[4] = { 0x00, 0x55, 0xAA, 0xFF };
    return four[phys(v)];
}

/* the nibble -> byte tables for ink on ground (and the inverse) */
static void make(uint8_t ink, uint8_t ground)
{
    uint8_t n, x, b, ib, f, g;
    if (ink == tab_ink && ground == tab_ground)
        return;
    tab_ink = ink;
    tab_ground = ground;
    f = phys(ink);
    g = phys(ground);
    for (n = 0; n < 16; ++n) {
        b = ib = 0;
        for (x = 0; x < 4; ++x) {
            b <<= 2;
            ib <<= 2;
            if (n & (8 >> x)) {
                b |= f;
                ib |= g;
            } else {
                b |= g;
                ib |= f;
            }
        }
        tab[n] = b;
        itab[n] = ib;
    }
}

/* the colours the tables were made for are stale (the phase changed) */
void scr2_recolour(void)
{
    tab_ink = 0xFF;
}

uint8_t scr_same(const uint8_t *cp, const char *s, uint8_t n);   /* (scr2a.asm) */
static const uint8_t bit8[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };     /* (CMOC shifts by a variable in a loop) */

static void cells(uint8_t row, uint8_t col, uint8_t n)
{
    uint8_t *d = ROWAD(row) + col;
    make(rink[row], rground[row]);
    while (n-- && col < SCR_COLS) {
        scr2_cell(d++, ch[row][col], inv[row][col >> 3] & bit8[col & 7] ? itab : tab);
        ++col;
    }
}

static void blank_row(uint8_t row)
{
    memset(ch[row], ' ', SCR_COLS);
    memset(inv[row], 0, sizeof(inv[row]));
    memset(ROWAD(row), phys4(rground[row]), 256);
}

static void set_pal(uint8_t row, uint8_t p)
{
    if (p >= PAL_SEAT) {
        rink[row] = 0;
        rground[row] = seat_ground[(uint8_t)(p - PAL_SEAT) % 3];
    } else {
        rink[row] = pal_ink[p];
        rground[row] = pal_ground[p];
    }
}

void scr_init(void)
{
    scr_begin();
}

void scr_begin(void)
{
    ++scr_seq;
    field_up = 0;
    uint8_t r;
    for (r = 0; r < SCR_ROWS; ++r) {
        set_pal(r, PAL_TEXT);
        blank_row(r);
    }
}

void scr_row(uint8_t row, uint8_t p)
{
    uint8_t ink = rink[row], ground = rground[row];
    set_pal(row, p);
    if (ink == rink[row] && ground == rground[row])
        return;
    memset(ROWAD(row), phys4(rground[row]), 256);
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
            memcpy(ROWAD(i), ROWAD(i + 1), 256);
        }
    } else {
        for (i = last; i > row; --i) {
            memcpy(ch[i], ch[i - 1], SCR_COLS);
            memcpy(inv[i], inv[i - 1], sizeof(inv[i]));
            rink[i] = rink[i - 1];
            rground[i] = rground[i - 1];
            memcpy(ROWAD(i), ROWAD(i - 1), 256);
        }
    }
}

void scr_show(void)
{
    frame_wait();
}

#ifdef MENU
/* the title picture (tools/mktitle_coco.py), drawn in the first phase */
extern const uint8_t title2[];
const uint8_t *lz_unpack(uint8_t *dst, const uint8_t *src);

void scr_picture(void)
{
    uint8_t r;
    lz_unpack(SCREEN, title2);
    if (coco_swap)
        video_flip();
    for (r = 0; r < SCR_ROWS; ++r)      /* (the rows' colours: as good as unknown) */
        rink[r] = rground[r] = 0xFF;
}
#endif

/* ---- the field's HUD: rows 21-23, white on black (hi: black on white) */
void scr2_hud(uint8_t row, uint8_t col, const char *s, uint8_t hi)
{
    uint8_t r = (uint8_t)(SCR_HUD_ROW + row), c, b, *iv, *cp, *d;
    if (rink[r] != 3 || rground[r] != 0) {
        rink[r] = 3;
        rground[r] = 0;
        blank_row(r);
    }
    make(3, 0);
    cp = ch[r] + col;
    d = ROWAD(r) + col;
    iv = inv[r];
    if (!hi && !(iv[0] | iv[1] | iv[2] | iv[3] | iv[4])) {     /* (most: plain text over plain) */
        for (;;) {
            b = scr_same(cp, s, (uint8_t)(SCR_COLS - col));
            cp += b;
            s += b;
            col += b;
            d += b;
            if (col >= SCR_COLS || !(c = (uint8_t)*s++))
                return;
            *cp++ = c;
            scr2_cell(d, c, tab);
            ++d;
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
            scr2_cell(d, c, hi ? itab : tab);
        }
        ++cp;
        ++d;
        ++col;
    }
}

void scr2_hud_clear(uint8_t row)
{
    uint8_t r = (uint8_t)(SCR_HUD_ROW + row);
    rink[r] = 3;
    rground[r] = 0;
    blank_row(r);
}
