/*
 * scr.c -- text screens (scr.h) on the Atari: 24 rows of ANTIC 2 in the OS
 * font, each row's colours from dsp.s's row tables.
 */
#ifdef __ATARI__

#include <string.h>
#include "atari.h"
#include "scr.h"
#include "field.h"
#include "plat.h"

extern uint8_t dsp_dma, dsp_mode, dl_text[];
extern uint8_t rw_bk[], rw_c1[], rw_c2[];
/* the seats' colours (the field's tanks, the rooms' rows) */
const uint8_t seat_col[10] = { 0x36, 0x86, 0xC8, 0x1C, 0x58, 0xAC, 0x2A, 0x0E, 0x4C, 0xE6 };

/* A PAL GTIA shows a hue about one step on from NTSC's: back it off. */
uint8_t hue(uint8_t c)
{
    return (GTIA_PAL & 0x0E) == 0 && c >= 0x20 ? c - 0x10 : c;
}

#define TEXT ((uint8_t *)TEXT_ADDR)

/* border, text brightness, background */
static const uint8_t pal[][3] = {
    { 0x90, 0x0E, 0x92 },       /* TEXT: white on blue */
    { 0x90, 0x0E, 0x34 },       /* TITLE: white on red */
    { 0x90, 0x0E, 0x06 },       /* HEAD: white on grey */
    { 0x90, 0x00, 0x1C },       /* HILITE: black on yellow */
    { 0x90, 0x08, 0x92 },       /* DIM */
};

void dsp_init(void);

#define PMBASE (*(volatile uint8_t *)0xD407)

void scr_init(void)
{
    memset((void *)(PMG_ADDR + 0x300), 0, 0x500);   /* missiles, players */
    PMBASE = PMG_ADDR >> 8;
    GTIA_GRACTL = 3;
    dsp_dma = 0;
    dsp_init();
    scr_begin();                        /* (field_init comes after the boot code) */
    scr_show();
}

void scr_begin(void)
{
    uint8_t r;
    dsp_dma = 0;
    frame_wait();
    memset(TEXT, 0, SCR_ROWS * SCR_COLS);
    for (r = 0; r < SCR_ROWS; ++r)
        scr_row(r, PAL_TEXT);
}

void scr_row(uint8_t row, uint8_t p)
{
    if (p >= PAL_SEAT) {
        rw_bk[row] = hue(0x90);
        rw_c1[row] = 0x0E;
        rw_c2[row] = hue(seat_col[p - PAL_SEAT]) & 0xF4;   /* darker, for white text */
        return;
    }
    rw_bk[row] = hue(pal[p][0]);
    rw_c1[row] = pal[p][1];
    rw_c2[row] = hue(pal[p][2]);
}

void scr_text(uint8_t row, uint8_t col, const char *s)
{
    uint8_t *d = TEXT + row * SCR_COLS + col;
    uint8_t *e = TEXT + (row + 1) * SCR_COLS;
    uint8_t c;
    while ((c = (uint8_t)*s++) != 0 && d < e) {
        if (c >= 'a' && c <= 'z')
            c -= 32;
        *d++ = c < 32 ? 0 : c < 96 ? c - 32 : c;
    }
}

void scr_inverse(uint8_t row, uint8_t col, uint8_t n)
{
    uint8_t *d = TEXT + row * SCR_COLS + col;
    while (n--)
        *d++ ^= 0x80;
}

void scr_scroll(uint8_t row, uint8_t n, uint8_t up)
{
    uint8_t *t = TEXT + row * SCR_COLS;
    if (up)
        memmove(t, t + SCR_COLS, (n - 1) * SCR_COLS);
    else
        memmove(t + SCR_COLS, t, (n - 1) * SCR_COLS);
}

void scr_clear_row(uint8_t row)
{
    memset(TEXT + row * SCR_COLS, 0, SCR_COLS);
}

void scr_show(void)
{
    uint8_t r, p = 0;
    uint16_t a = TEXT_ADDR;
    dsp_dma = 0;
    frame_wait();
    dl_text[p++] = 0x70;
    dl_text[p++] = 0x70;
    dl_text[p++] = 0x70;
    dl_text[p++] = 0xC2;            /* row 0, LMS, its DLI loads row 1 */
    dl_text[p++] = (uint8_t)a;
    dl_text[p++] = a >> 8;
    for (r = 1; r < SCR_ROWS; ++r)
        dl_text[p++] = r + 1 < SCR_ROWS ? 0x82 : 0x02;
    dl_text[p++] = 0x41;
    dl_text[p++] = (uint8_t)(uint16_t)dl_text;
    dl_text[p] = (uint16_t)dl_text >> 8;
    dsp_mode = 0;
    dsp_dma = 0x22;                 /* normal width, no players */
}

#endif
