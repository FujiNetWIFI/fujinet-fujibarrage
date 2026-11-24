/*
 * video.c -- the VGA build's screen: mode 13h, 320x200 in 256 colours, on
 * a VGA or an MCGA. Its DAC is set through the BIOS only (INT 10h
 * AX=1010h/1012h): an MCGA has no attribute controller, so nothing of
 * AX=1000h-1003h. A machine with neither is handed to the CGA build's
 * program (msdos.h TWIN_*), typed at DOS's prompt, as FujiBlast's
 * FUJIBVGA does.
 */
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <i86.h>
#include <conio.h>
#include "game.h"
#include "field.h"
#include "plat.h"
#include "msdos.h"
#include "text.h"
#include "vga.h"



uint8_t shake_n;
static uint8_t vga_only;               /* a VGA (not an MCGA): its CRTC shakes */

/* The vertical retrace's start (3DAh bit 3, a VGA's and an MCGA's): the
 * IBM VGA BIOS blacks out lines while it loads the DAC (86Box shows it,
 * FujiNet Go's too, with a frame 80 pixels wide now and then), so each
 * BIOS load starts here, off the screen as far as it can. (Bounded: a
 * card that never says.) */
static void retrace(void)
{
    uint16_t n = 0;
    while ((inp(0x3DA) & 8) && --n)
        ;
    while (!(inp(0x3DA) & 8) && --n)
        ;
}

/* DAC entries first.. from 6-bit RGB in DGROUP */
void vga_dac(uint16_t first, uint16_t n, const uint8_t *tab)
{
    union REGS r;
    struct SREGS s;
    retrace();
    segread(&s);
    s.es = s.ds;
    r.w.ax = 0x1012;
    r.w.bx = first;
    r.w.cx = n;
    r.w.dx = (uint16_t)tab;
    int86x(0x10, &r, &r, &s);
}

/* A VGA or an MCGA: INT 10h AX=1A00h answers AL=1Ah, the display in BL
 * (07h/08h a VGA, 0Bh/0Ch an MCGA). */
static uint8_t vga(void)
{
    union REGS r;
    r.w.ax = 0x1A00;
    int86(0x10, &r, &r);
    vga_only = r.h.al == 0x1A && (r.h.bl == 7 || r.h.bl == 8);
    return r.h.al == 0x1A && (r.h.bl == 7 || r.h.bl == 8 || r.h.bl == 0x0B || r.h.bl == 0x0C);
}

void video_init(void)
{
    union REGS r;
    if (!vga()) {
#ifdef MENU
        plat_chain(TWIN_MENU);  /* (the CGA build's, then: CGA, EGA, PCjr, Tandy) */
#else
        plat_chain(TWIN_GAME);
#endif
    }
    r.w.ax = 0x0013;            /* 320x200, 256 colours */
    int86(0x10, &r, &r);
    {   /* the ten seats: body, shade, highlight (M2's art settles them) */
        static const uint8_t body[10][3] = {
            { 60, 60, 60 }, { 60, 12, 12 }, { 16, 28, 63 }, { 63, 36, 4 }, { 12, 52, 12 },
            { 63, 60, 8 }, { 8, 56, 60 }, { 56, 16, 56 }, { 40, 30, 20 }, { 40, 40, 44 } };
        static uint8_t dac[30 * 3];
        uint8_t i, k;
        for (i = 0; i < 10; ++i)
            for (k = 0; k < 3; ++k) {
                dac[i * 9 + k] = body[i][k];
                dac[i * 9 + 3 + k] = (uint8_t)(body[i][k] * 5 / 8);
                dac[i * 9 + 6] = dac[i * 9 + 7] = dac[i * 9 + 8] = 63;
            }
        vga_dac(V_SEAT, 30, dac);
    }
}

/* ---- the field's look ------------------------------------------------- */
#ifndef MENU
/* The engine's tables (src/msdos/vga/fld.asm): a value's colour by line
 * and x; and a colour's value. Built once: a round's look is the DAC. */
uint8_t v_sky[176], v_dirt[176], v_fire[176], v_toff[176];
uint8_t v_tex[64], v_rand[256], v_class[256], v_stars;

/* the skies (SKY_*: plain, gradient, stars, storm, sunset, cavern, black,
 * random as gradient): the top's colour and the horizon's, 6-bit RGB */
static const uint8_t skies[8][6] = {
    { 18, 34, 58, 30, 46, 63 }, { 4, 10, 40, 40, 52, 63 }, { 0, 0, 4, 6, 4, 16 },
    { 10, 10, 14, 30, 30, 34 }, { 16, 4, 28, 63, 36, 8 }, { 10, 6, 3, 3, 2, 1 },
    { 0, 0, 0, 0, 0, 0 }, { 4, 10, 40, 40, 52, 63 } };
/* the dirt's palettes: the top stratum's colour (deeper ones darker) */
static const uint8_t dirts[8][3] = {
    { 44, 26, 10 }, { 18, 38, 12 }, { 34, 34, 34 }, { 50, 20, 8 },
    { 54, 42, 12 }, { 16, 22, 42 }, { 34, 16, 38 }, { 58, 38, 8 } };
/* the fire's 16, cycled: yellow, orange, red and back */
static const uint8_t fires[16][3] = {
    { 63, 63, 40 }, { 63, 58, 20 }, { 63, 50, 8 }, { 63, 40, 4 }, { 63, 30, 0 }, { 60, 20, 0 },
    { 54, 12, 0 }, { 48, 6, 0 }, { 54, 12, 0 }, { 60, 20, 0 }, { 63, 30, 0 }, { 63, 40, 4 },
    { 63, 50, 8 }, { 63, 58, 20 }, { 63, 63, 40 }, { 63, 63, 52 } };
static uint8_t fdac[143 * 3];           /* the field's DAC now: entries 1-143 */
static uint8_t flash_n, fire_on, fire_ph, shaking;

void vid_field_init(void)
{
    uint16_t i, h = 0x1234;
    uint8_t y;
    for (y = 0; y < 176; ++y) {
        v_sky[y] = (uint8_t)(V_SKY + (uint16_t)y * V_NSKY / 176);
        v_dirt[y] = (uint8_t)(V_DIRT + ((uint16_t)y * 8 / 176) * 4);
        v_fire[y] = (uint8_t)(V_FIRE + ((y >> 2) & 15));
        h = h * 75 + 74;
        v_toff[y] = (uint8_t)(h >> 8);
    }
    for (i = 0; i < 64; ++i) {          /* the dirt's grain: mostly the middle shades */
        static const uint8_t grain[8] = { 1, 1, 2, 2, 1, 2, 0, 3 };
        h = h * 75 + 74;
        v_tex[i] = grain[(h >> 8) & 7];
    }
    for (i = 0; i < 256; ++i) {         /* the stars: where a line's hash of x is 0 */
        h = h * 75 + 74;
        v_rand[i] = (uint8_t)(h >> 8);
        v_class[i] = i == 0 ? PX_TANK : i < 64 ? PX_SKY : i < 128 ? PX_DIRT : i < 144 ? PX_FX : PX_TANK;
    }
}

/* the stars on or off in a field already drawn: only where they can be
 * (fld.asm's vcol: line y's star where v_rand[(v_rand[y] + x) & 255] is
 * 0), each that is sky now drawn as the sky it is */
static void stars_turn(uint8_t on)
{
    uint8_t __far *v = MK_FP(0xA000, 0);
    uint16_t k, x;
    uint8_t y, c;
    v_stars = on;
    for (k = 0; k < 256; ++k) {
        if (v_rand[k])
            continue;
        for (y = 0; y < 176; ++y)
            for (x = (uint8_t)(k - v_rand[y]); x < 320; x += 256) {
                c = v[y * 320u + x];
                if (v_class[c] == PX_SKY)
                    v[y * 320u + x] = on ? V_STAR : v_sky[y];
            }
    }
}

/* the field's DAC (1-143) from fdac */
static void field_dac(void)
{
    vga_dac(1, 143, fdac);
}

/* a whole field comes over a text screen: the stars as its sky's, the
 * field's colours dark till it opens */
void vid_field_loading(void)
{
    static const uint8_t dark[143 * 3];
    v_stars = (st[OFF_SKY] & 7) == SKY_STARS;
    vga_dac(1, 143, dark);
}

static void ramp(uint8_t *d, uint8_t n, const uint8_t *a, const uint8_t *b)
{
    uint8_t i, k;
    for (i = 0; i < n; ++i)
        for (k = 0; k < 3; ++k)
            *d++ = (uint8_t)(a[k] + ((int16_t)b[k] - a[k]) * i / (n - 1));
}

void vid_field_open(uint8_t sky, uint8_t pal)
{
    uint8_t stars = sky == SKY_STARS, i, k, *d;
    const uint8_t *c;
    if (stars != v_stars)               /* drawn on another sky's stars */
        stars_turn(stars);
    memset(fdac, 0, sizeof(fdac));
    ramp(fdac + (V_SKY - 1) * 3, V_NSKY, skies[sky], skies[sky] + 3);
    d = fdac + (V_STAR - 1) * 3;        /* a star: white on a starry sky */
    d[0] = d[1] = d[2] = stars ? 63 : 0;
    c = dirts[pal];
    for (i = 0; i < 8; ++i)             /* 8 strata, darker down; 4 shades each */
        for (k = 0; k < 4; ++k) {
            d = fdac + (V_DIRT - 1 + i * 4 + k) * 3;
            d[0] = (uint8_t)(c[0] * (16 - i) / 16 * (12 + k) / 14);
            d[1] = (uint8_t)(c[1] * (16 - i) / 16 * (12 + k) / 14);
            d[2] = (uint8_t)(c[2] * (16 - i) / 16 * (12 + k) / 14);
        }
    memcpy(fdac + (V_FIRE - 1) * 3, fires, sizeof(fires));
    fire_on = 0;
    field_dac();
    txt_show();
}

/* a flash: the sky and the dirt white for its frames */
void vid_flash(uint8_t frames)
{
    static uint8_t white[127 * 3];
    flash_n = frames;
    if (!frames)
        return;
    memset(white, 63, sizeof(white));
    vga_dac(1, 127, white);
}

void vid_fire(uint8_t on)
{
    fire_on = on;
    if (!on) {
        fire_ph = 0;
        vga_dac(V_FIRE, 16, fdac + (V_FIRE - 1) * 3);
    }
}

/* the CRTC's start address (mode 13h: 4 pixels a step): a VGA's, not an
 * MCGA's */
static void start_at(uint16_t w)
{
    if (!vga_only)
        return;
    outp(0x3D4, 12);
    outp(0x3D5, w >> 8);
    outp(0x3D4, 13);
    outp(0x3D5, w & 0xFF);
}

/* The fire's 16 colours a step on: straight into the DAC (3C8h, 3C9h: a
 * VGA's and an MCGA's alike), not through the BIOS. The user's exception
 * to "palettes through the BIOS" (2026-11-23): the IBM VGA BIOS blacks out
 * lines while it loads, and 15 loads a second on a 4.77 MHz PC flickered
 * the whole screen (FujiNet Go's 86Box, a frame 80 pixels wide now and
 * then). */
static void fire_step(void)
{
    const uint8_t *c;
    uint8_t i, k;
    fire_ph = (uint8_t)((fire_ph + 1) & 15);
    outp(0x3C8, V_FIRE);                /* (interrupts on: none touches the DAC) */
    for (i = 0; i < 16; ++i) {
        c = fires[(i + fire_ph) & 15];
        for (k = 0; k < 3; ++k)
            outp(0x3C9, c[k]);
    }
}

/* each pass (frame_wait, vbi_now): the flash's and the shake's frames, by
 * the jiffies gone since (a slow pass doesn't stretch them); the fire's
 * colours a step every 4 */
void video_frame(void)
{
    static uint16_t seen;
    static uint8_t fire_t;
    uint16_t now = *(volatile uint16_t *)&frame_count, d = now - seen;
    if (!d)
        return;
    seen = now;
    if (flash_n) {
        flash_n = d < flash_n ? (uint8_t)(flash_n - d) : 0;
        if (!flash_n)
            field_dac();
    }
    if (shake_n) {
        shake_n = d < shake_n ? (uint8_t)(shake_n - d) : 0;
        shaking = 1;
        start_at(shake_n & 2 ? 1 : 0);
    } else if (shaking) {
        shaking = 0;
        start_at(0);
    }
    if (fire_on && !flash_n) {
        fire_t = (uint8_t)(fire_t + (d > 4 ? 4 : d));
        if (fire_t >= 4) {
            fire_t -= 4;
            fire_step();
        }
    }
}
#else
void video_frame(void)          /* (the menu: no field) */
{
}
#endif

#ifdef VIDEO_CARD
/* M0's test card: all 256 colours (a ramp of each primary and greys
 * loaded for it), an ellipse that is a circle at the field's aspect (PAR
 * 55: rx 80, ry 69), a bar that moves with the clock. */
void video_card(void)
{
    static uint8_t dac[256 * 3];
    uint8_t __far *v = MK_FP(0xA000, 0);
    uint16_t i, t;
    int16_t x, y;
    for (i = 0; i < 256; ++i) {         /* 4 ramps of 64: red, green, blue, grey */
        uint8_t l = i & 63, k = i >> 6;
        dac[i * 3] = k == 0 || k == 3 ? l : 0;
        dac[i * 3 + 1] = k == 1 || k == 3 ? l : 0;
        dac[i * 3 + 2] = k == 2 || k == 3 ? l : 0;
    }
    vga_dac(0, 256, dac);
    for (y = 0; y < 200; ++y)
        for (x = 0; x < 320; ++x)
            v[y * 320u + x] = y < 64 ? (uint8_t)((y >> 4) * 64 + x / 5) : y < 176 ? 192 + 20 : 0;
    for (y = 0; y <= 69; ++y) {             /* x = 80 sqrt(1 - (y/69)^2) */
        uint32_t w = 6400ul - 6400ul * y * y / (69 * 69);
        for (x = 0; (uint32_t)(x + 1) * (x + 1) <= w; ++x)
            ;
        v[(88 + y) * 320u + 160 + x] = 255;
        v[(88 + y) * 320u + 160 - x] = 255;
        v[(88 - y) * 320u + 160 + x] = 255;
        v[(88 - y) * 320u + 160 - x] = 255;
    }
    for (;;) {
        t = frame_count;
        x = (t >> 1) % 320;
        for (y = 178; y < 198; ++y) {
            v[y * 320u + x] = 63;
            v[y * 320u + (x ? x - 1 : 319)] = 0;
        }
        while (*(volatile uint16_t *)&frame_count == t)
            ;
    }
}
#endif
