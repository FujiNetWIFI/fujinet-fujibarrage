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

void scr2_run(uint8_t *dst, const uint8_t *chars, uint8_t n, const uint8_t *tab, const uint8_t *ltab);
void scr2_move(uint8_t *dst, const uint8_t *src, uint16_t n);

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
static uint8_t ltab[16], l_ground = 0xFF;          /* a label's (cells) */

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

/* t: nibble -> a byte, each pixel ink f or ground g (screen values) */
static void fill2(uint8_t *t, uint8_t f, uint8_t g)
{
    uint8_t n, x, b;
    for (n = 0; n < 16; ++n) {
        for (b = 0, x = 0; x < 4; ++x)
            b = (uint8_t)(b << 2 | (n & (8 >> x) ? f : g));
        t[n] = b;
    }
}

/* the nibble -> byte tables for ink on ground (and the inverse) */
static void make(uint8_t ink, uint8_t ground)
{
    if (ink == tab_ink && ground == tab_ground)
        return;
    tab_ink = ink;
    tab_ground = ground;
    fill2(tab, phys(ink), phys(ground));
    fill2(itab, phys(ground), phys(ink));
}

/* the colours the tables were made for are stale (the phase changed) */
void scr2_recolour(void)
{
    tab_ink = l_ground = 0xFF;
}

uint8_t scr_same(const uint8_t *cp, const char *s, uint8_t n, uint8_t m);   /* (scr2a.asm) */
static const uint8_t bit8[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };     /* (CMOC shifts by a variable in a loop) */

/* A label (bit 7, scr.h) in orange, the nearest to yellow, on a row of
 * white ink but an orange one (labels yellow, values white: the user,
 * 2026-11-23); no names in colour: the tanks are black. (ltab: scr2_recolour) */

/* cells col.. (n) of a row drawn from what they hold: a run a call (a
 * cell a call, the C cost more than the drawing; scr2_run takes a
 * label's table by its bit 7); a row with inverse cells (a cursor) a cell
 * at a time, its labels plain */
static void cells(uint8_t row, uint8_t col, uint8_t n)
{
    uint8_t *ip = inv[row], *lt = tab;
    make(rink[row], rground[row]);
    if (ip[0] | ip[1] | ip[2] | ip[3] | ip[4]) {
        for (; n; --n, ++col)
            scr2_run(ROWAD(row) + col, ch[row] + col, 1, ip[col >> 3] & bit8[col & 7] ? itab : tab, tab);
        return;
    }
    if (rink[row] == 3 && rground[row] != 2) {
        if (rground[row] != l_ground) {
            l_ground = rground[row];
            fill2(ltab, phys(2), phys(l_ground));
        }
        lt = ltab;
    }
    scr2_run(ROWAD(row) + col, ch[row] + col, n, tab, lt);
}

/* s at (row, col), each character OR m (80h: a label's): only the cells
 * that change drawn, a run at a time (the unchanged passed over by
 * scr_same) */
static void put_s(uint8_t row, uint8_t col, const char *s, uint8_t m)
{
    uint8_t *cp = ch[row], k, from;
    while (col < SCR_COLS && *s) {
        k = scr_same(cp + col, s, (uint8_t)(SCR_COLS - col), m);
        col += k;
        s += k;
        from = col;
        while (col < SCR_COLS && *s && cp[col] != (uint8_t)(*s | m))
            cp[col++] = (uint8_t)(*s++ | m);
        if (col != from)
            cells(row, from, (uint8_t)(col - from));
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
    cells(row, 0, SCR_COLS);            /* (the 32 cells cover the row) */
}

/* only the cells that change are drawn (a row is often written again as
 * it was: the shop's each second, a row after scr_row redrew it) */
void scr_text(uint8_t row, uint8_t col, const char *s)
{
    put_s(row, col, s, 0);
}

void scr_label(uint8_t row, uint8_t col, const char *s)     /* (scr.h) */
{
    put_s(row, col, s, 0x80);
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
        scr2_move(ROWAD(row), ROWAD(row + 1), (uint16_t)(n - 1) << 8);
    } else {
        for (i = last; i > row; --i) {
            memcpy(ch[i], ch[i - 1], SCR_COLS);
            memcpy(inv[i], inv[i - 1], sizeof(inv[i]));
            rink[i] = rink[i - 1];
            rground[i] = rground[i - 1];
            scr2_move(ROWAD(i), ROWAD(i - 1), 256);     /* (a row at a time, from the bottom) */
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
    memset(ch, 0, sizeof(ch));          /* (and their cells: scr_text draws what it's given) */
}
#endif

/* ---- the field's HUD: rows 21-23, white on black (hi: black on white) */
void scr2_hud(uint8_t row, uint8_t col, const char *s, uint8_t hi)
{
    uint8_t r = (uint8_t)(SCR_HUD_ROW + row), c, b, *iv, *cp;
    if (rink[r] != 3 || rground[r] != 0) {
        rink[r] = 3;
        rground[r] = 0;
        blank_row(r);
    }
    cp = ch[r] + col;
    iv = inv[r];
    if (!hi && !(iv[0] | iv[1] | iv[2] | iv[3] | iv[4])) {     /* (most: plain text over plain) */
        put_s(r, col, s, 0);
        return;
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

void scr2_hud_clear(uint8_t row)
{
    uint8_t r = (uint8_t)(SCR_HUD_ROW + row);
    rink[r] = 3;
    rground[r] = 0;
    blank_row(r);
}
