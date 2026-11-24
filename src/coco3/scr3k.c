/*
 * scr3k.c -- the CoCo 3's text in colour (scr.h; src/coco3/scr3.c draws
 * the rest): a label (bit 7) in seat 3's yellow on a row of white ink, a
 * HUD row's name (field_name) in its seat's colour (labels yellow, values
 * white, names in their seat's colour: the user, 2026-11-23). The game's
 * is in the art block: its high core is full (text is never drawn while
 * fld_keep has another block in slot 7); the menu's in its high core.
 */
#include "cocoplat.h"
#include "scr3.h"
#include "scr3i.h"

uint8_t scr3_nmc[3], scr3_nmn[3], scr3_nms[3];
static uint16_t ktab[16];                   /* a label's or a name's */
static uint8_t k_ink = 0xFF, k_ground;
static const uint8_t bit8[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };     /* (CMOC shifts by a variable in a loop) */

/* t: nibble -> 4 pixels of ink on ground */
void scr3_tabfill(uint16_t *t, uint8_t ink, uint8_t ground)
{
    uint8_t n, x;
    uint16_t w;
    for (n = 0; n < 16; ++n) {
        for (w = 0, x = 0; x < 4; ++x)
            w = (w << 4) | (n & (8 >> x) ? ink : ground);
        t[n] = w;
    }
}

/* the table for ink k on the row (0xFF: the row's own, 0xFE: its inverse) */
const uint16_t *scr3_tabfor(uint8_t row, uint8_t k)
{
    if (k >= 0xFE)
        return k & 1 ? scr3_tab : scr3_itab;
    if (k != k_ink || scr3_rground[row] != k_ground) {
        k_ink = k;
        k_ground = scr3_rground[row];
        scr3_tabfill(ktab, k, k_ground);
    }
    return ktab;
}

/* cells col.. (n) of a row with inverse cells or a name: a cell at a
 * time, each in its ink (inverse, a name's, a label's or the row's) */
void scr3_cellk(uint8_t row, uint8_t col, uint8_t n)
{
    uint8_t h = (uint8_t)(row - SCR_HUD_ROW), k;
    const uint16_t *t;
    for (; n; --n, ++col) {
        if (scr_inv[row][col >> 3] & bit8[col & 7])
            k = 0xFE;
        else if (h < 3 && scr3_nmn[h] && col >= scr3_nmc[h] && col < scr3_nmc[h] + scr3_nmn[h])
            k = (uint8_t)(C3_SEAT + scr3_nms[h]);
        else if ((scr_ch[row][col] & 0x80) && scr3_rink[row] == C3_WHITE)
            k = C3_LABEL;
        else
            k = 0xFF;
        t = scr3_tabfor(row, k);
        scr3_run(row, col, scr_ch[row] + col, 1, t, t);
    }
}

/* a HUD row's name, in its seat's colour (field.c's field_name): the cells
 * it leaves and the ones it takes drawn again */
void scr3_hud_name(uint8_t row, uint8_t col, uint8_t n, uint8_t seat)
{
    uint8_t r = (uint8_t)(SCR_HUD_ROW + row), oc = scr3_nmc[row], on = scr3_nmn[row];
    if (oc == col && on == n && (!n || scr3_nms[row] == seat))
        return;
    scr3_nmc[row] = col;
    scr3_nmn[row] = n;
    scr3_nms[row] = seat;
    if (scr3_rink[r] != C3_WHITE || scr3_rground[r] != C3_BLACK)
        return;                         /* (not the HUD yet: drawn when it is) */
    if (on)
        scr3_cells(r, oc, on);
    if (n)
        scr3_cells(r, col, n);
}
