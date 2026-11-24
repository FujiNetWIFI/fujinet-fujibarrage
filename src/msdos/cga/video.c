/*
 * video.c -- the CGA build's screen: 320x200 in 4 colours (mode 4), the
 * palette set through the BIOS only (FujiBlast's rule): on a CGA INT 10h
 * AH=0Bh (BH=0 the background and the bright set, BH=1 the set); on an
 * EGA or a PCjr the four colours in the palette registers (AH=10h:
 * AH=0Bh there puts CGA's palette 1 back in registers 1-3). A Tandy
 * takes AH=0Bh.
 *
 * Colour 0 is the background, any of 16; 1-3 are the set's: 0 green, red,
 * brown (bright: light green, light red, yellow), 1 cyan, magenta, light
 * grey (bright: light cyan, light magenta, white). The field's values are
 * the colours (src/msdos/cga/fld.asm): sky 0, tank 1, fire 2, dirt 3.
 */
#include <dos.h>
#include <i86.h>
#include <conio.h>
#include "plat.h"
#include "msdos.h"
#include "cga.h"

uint8_t shake_n;
static uint8_t cur_bg, cur_set, cur_bright;

/* the set's colours, RGBI: [set][bright][1..3] */
static const uint8_t set_rgbi[2][2][3] = {
    { { 2, 4, 6 }, { 10, 12, 14 } },
    { { 3, 5, 7 }, { 11, 13, 15 } },
};

static uint8_t ega(uint8_t c)
{
    return plat_pal == PAL_EGA ? (c & 7) | ((c & 8) << 1) : c;  /* (the PCjr's: RGBI) */
}

static void set_reg(uint8_t reg, uint8_t c)
{
    union REGS r;
    r.w.ax = 0x1000;
    r.h.bl = reg;
    r.h.bh = ega(c);
    int86(0x10, &r, &r);
}

/* The four colours: the background (and the border), the set, bright. */
void cga_palette(uint8_t bg, uint8_t set, uint8_t bright)
{
    union REGS r;
    uint8_t i;
    cur_bg = bg;
    cur_set = set;
    cur_bright = bright;
    if (plat_pal == PAL_CGA || plat_pal == PAL_TANDY) {
        r.h.ah = 0x0B;
        r.h.bh = 1;
        r.h.bl = set;
        int86(0x10, &r, &r);
        r.h.ah = 0x0B;
        r.h.bh = 0;
        r.h.bl = bg | (bright ? 0x10 : 0);
        int86(0x10, &r, &r);
        return;
    }
    set_reg(0, bg);
    for (i = 0; i < 3; ++i)
        set_reg(i + 1, set_rgbi[set][bright][i]);
    r.w.ax = 0x1001;            /* the border */
    r.h.bh = ega(bg);
    int86(0x10, &r, &r);
}

void video_init(void)
{
    union REGS r;
    r.w.ax = 0x0004;            /* 320x200, 4 colours */
    int86(0x10, &r, &r);
    cga_palette(0, 1, 1);
}

/* ---- the field's look ------------------------------------------------- */

/* a round: the sky (SKY_*) the background's colour, the dirt's palette
 * (0-7) the set and its brightness: 0 green tanks, red fire, brown dirt
 * (bright: light green, light red, yellow); 1 cyan, magenta, light grey
 * (bright: light cyan, light magenta, white). Checkpoint 1 settles them. */
static const uint8_t sky_bg[8] = { 9, 1, 0, 8, 5, 6, 0, 1 };
static const uint8_t dirt_set[8] = { 0, 0, 1, 0, 0, 1, 1, 0 };
static const uint8_t dirt_bright[8] = { 0, 1, 0, 0, 1, 1, 1, 1 };
static uint8_t f_bg, f_set, f_bright, flash_n, shaking;

void vid_field_init(void)
{
}

void vid_field_loading(void)
{
}

void vid_field_open(uint8_t sky, uint8_t pal)
{
    uint8_t i;
    f_bg = sky_bg[sky];
    f_set = dirt_set[pal];
    f_bright = dirt_bright[pal];
    for (i = 0; i < 3; ++i)             /* the sky never a colour of the set's */
        if (set_rgbi[f_set][f_bright][i] == f_bg)
            f_bg = 0;
    cga_palette(f_bg, f_set, f_bright);
}

/* a flash: everything bright (white sky, white dirt) for its frames */
void vid_flash(uint8_t frames)
{
    flash_n = frames;
    if (frames)
        cga_palette(15, 1, 1);
}

void vid_fire(uint8_t on)
{
    (void)on;                           /* (one colour: nothing to cycle) */
}

/* the CRTC's start address (in words: 8 pixels): a shake moves the picture
 * a word left or right. A CGA's and an EGA's 6845 only: the PCjr's and the
 * Tandy's video pages work otherwise. */
static void start_at(uint16_t w)
{
    if (plat_pal != PAL_CGA && plat_pal != PAL_EGA)
        return;
    outp(0x3D4, 12);
    outp(0x3D5, w >> 8);
    outp(0x3D4, 13);
    outp(0x3D5, w & 0xFF);
}

/* each pass (frame_wait, vbi_now): the flash's and the shake's frames, by
 * the jiffies gone since (a slow pass doesn't stretch them) */
void video_frame(void)
{
    static uint16_t seen;
    uint16_t now = *(volatile uint16_t *)&frame_count, d = now - seen;
    if (!d)
        return;
    seen = now;
    if (flash_n) {
        flash_n = d < flash_n ? (uint8_t)(flash_n - d) : 0;
        if (!flash_n)
            cga_palette(f_bg, f_set, f_bright);
    }
    if (shake_n) {
        shake_n = d < shake_n ? (uint8_t)(shake_n - d) : 0;
        shaking = 1;
        start_at(shake_n & 2 ? 1 : 0);
    } else if (shaking) {
        shaking = 0;
        start_at(0);
    }
}

#ifdef VIDEO_CARD
/* M0's test card: the four colours in bands, an ellipse that is a circle
 * at the field's aspect (PAR 55: rx 80, ry 69), a bar that moves with the
 * clock, and the palettes in turn every 2 s (bg, set, bright). */
static void pset(int16_t x, int16_t y, uint8_t c)
{
    uint8_t __far *p;
    uint8_t sh;
    if (x < 0 || x > 319 || y < 0 || y > 199)
        return;
    p = MK_FP(0xB800, (y & 1) * 0x2000 + (y >> 1) * 80 + (x >> 2));
    sh = 6 - ((x & 3) << 1);
    *p = (*p & ~(3 << sh)) | (c << sh);
}

void video_card(void)
{
    static const uint8_t pals[][3] = {
        { 9, 0, 0 }, { 9, 0, 1 }, { 1, 1, 1 }, { 0, 1, 0 }, { 5, 0, 1 }, { 8, 1, 1 },
    };
    int16_t x, y;
    uint16_t t, last = 0;
    uint8_t k = 0;
    for (y = 0; y < 200; ++y) {
        uint8_t __far *p = MK_FP(0xB800, (y & 1) * 0x2000 + (y >> 1) * 80);
        for (x = 0; x < 80; ++x)
            p[x] = y < 176 ? (uint8_t)(x / 20 * 0x55) : 0;
    }
    for (y = 0; y <= 69; ++y) {             /* x = 80 sqrt(1 - (y/69)^2) */
        uint32_t w = 6400ul - 6400ul * y * y / (69 * 69);
        for (x = 0; (uint32_t)(x + 1) * (x + 1) <= w; ++x)
            ;
        pset(160 + x, 88 + y, 3);
        pset(160 - x, 88 + y, 3);
        pset(160 + x, 88 - y, 3);
        pset(160 - x, 88 - y, 3);
    }
    for (;;) {
        t = frame_count;
        x = (t >> 1) % 320;
        for (y = 178; y < 198; ++y) {
            pset(x, y, 3);
            pset(x ? x - 1 : 319, y, 0);
        }
        if ((uint16_t)(t - last) >= 120) {
            last = t;
            cga_palette(pals[k][0], pals[k][1], pals[k][2]);
            k = (k + 1) % (sizeof(pals) / 3);
        }
        while (*(volatile uint16_t *)&frame_count == t)
            ;
    }
}
#endif
