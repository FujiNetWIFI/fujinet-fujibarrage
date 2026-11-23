/*
 * text.c -- the VGA build's text cells (text.h): each glyph line's lit
 * pixels its ink's colour and the rest its ground's, a ramp of 8 for each
 * (scra.asm vga_cell), so letters and rows are shaded top to bottom. The
 * ramps are the DAC's V_UI block (vga.h), loaded by txt_show; while a
 * text screen is drawn they are black.
 */
#include <string.h>
#include <i86.h>
#include "scr.h"
#include "plat.h"
#include "text.h"
#include "vga.h"

static uint8_t ramp[8][8];          /* V_UI's ramps as indices: ramp[k][line] */
static uint8_t seat_g[8];
static uint8_t black8[8];
static uint8_t ui_dac[64 * 3];      /* (6-bit RGB) */
static uint8_t made;

/* a ramp of 8 from (r0,g0,b0) at the top to (r1,g1,b1) at the bottom */
static void ramp_dac(uint8_t k, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1)
{
    uint8_t i, *d = ui_dac + k * 24;
    for (i = 0; i < 8; ++i) {
        *d++ = (uint8_t)(r0 + (r1 - r0) * i / 7);
        *d++ = (uint8_t)(g0 + (g1 - g0) * i / 7);
        *d++ = (uint8_t)(b0 + (b1 - b0) * i / 7);
        ramp[k][i] = (uint8_t)(V_UI + k * 8 + i);
    }
}

static void make(void)
{
    made = 1;
    ramp_dac(0, 63, 63, 63, 44, 48, 56);    /* text ink: white to blue-grey */
    ramp_dac(1, 4, 8, 30, 2, 4, 18);        /* blue ground */
    ramp_dac(2, 44, 8, 8, 28, 4, 4);        /* red ground */
    ramp_dac(3, 28, 28, 32, 16, 16, 20);    /* grey ground */
    ramp_dac(4, 63, 56, 16, 56, 40, 4);     /* yellow ground */
    ramp_dac(5, 36, 40, 52, 24, 28, 40);    /* dim ink */
    ramp_dac(6, 63, 63, 40, 63, 44, 8);     /* title ink: cream to amber */
    ramp_dac(7, 6, 6, 8, 2, 2, 3);          /* the HUD's ground: near black */
}

/* TEXT, TITLE, HEAD, HILITE, DIM: ink and ground ramps */
static const uint8_t pal_ink[] = { 0, 6, 0, 0xFF, 5 };     /* (0xFF: black) */
static const uint8_t pal_ground[] = { 1, 2, 3, 4, 1 };

static void colours(uint8_t row, const uint8_t **ink, const uint8_t **ground)
{
    uint8_t p = txt_pal[row];
    if (p == TP_HUD) {
        *ink = ramp[0];
        *ground = ramp[7];
    } else if (p >= PAL_SEAT) {
        memset(seat_g, V_SEAT + (p - PAL_SEAT) * 3, 8);
        *ink = black8;
        *ground = seat_g;
    } else {
        *ink = pal_ink[p] == 0xFF ? black8 : ramp[pal_ink[p]];
        *ground = ramp[pal_ground[p]];
    }
}

void txt_cell(uint8_t row, uint8_t col, uint8_t c, uint8_t inv)
{
    const uint8_t *ink, *ground;
    if (!made)
        make();
    colours(row, &ink, &ground);
    if (c < 32 || c > 127)
        c = ' ';
    if (inv)
        vga_cell((uint16_t)row * 2560 + col * 8, font8 + (c - 32) * 8, ground, ink);
    else
        vga_cell((uint16_t)row * 2560 + col * 8, font8 + (c - 32) * 8, ink, ground);
}

void txt_blank(uint8_t row)
{
    const uint8_t *ink, *ground;
    uint8_t k;
    if (!made)
        make();
    colours(row, &ink, &ground);
    for (k = 0; k < 8; ++k)
        _fmemset(MK_FP(0xA000, (uint16_t)row * 2560 + k * 320), ground[k], 320);
}

void txt_begin(void)
{
    static uint8_t dark[64 * 3];
    if (!made)
        make();
    vga_dac(V_UI, 64, dark);            /* (drawn in the dark) */
}

void txt_show(void)
{
    vga_dac(V_UI, 64, ui_dac);
}

#ifdef MENU
/* the title (menus.c): tools/msdos/mktitle_dos.py's picture, drawn in the
 * dark, then its 192 colours; ESC: LOBBY is in it */
extern const uint8_t title_vga[], title_vga_dac[];

void scr_picture(void)
{
    static uint8_t dark[192 * 3];
    uint8_t r;
    vga_dac(0, 192, dark);
    lz_unpack(0xA000, title_vga);
    frame_wait();
    vga_dac(0, 192, title_vga_dac);
    for (r = 0; r < TXT_ROWS; ++r)
        txt_pal[r] = 0xFF;              /* (the rows aren't text now) */
}
#endif
