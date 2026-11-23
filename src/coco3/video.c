/*
 * video.c -- the CoCo 3's display: the GIME at 320x200 in 16 colours
 * ($FF99 = $3E), 160 bytes a line, two pixels a byte (the left one in the
 * high nibble). HVEN ($FF9F bit 7) makes the line stride 256 bytes, so an
 * 8K block holds 32 whole lines: line y is in block SCR_BLOCK + y / 32 at
 * $C000 + (y % 32) * 256 + x / 2 when that block is mapped into slot 6
 * ($C000-$DFFF). The screen is blocks $30-$36 (a 128K machine's first
 * seven; on a 512K one the same physical RAM), the video start $60000.
 *
 * Slot 6 is normally the Disk ROM's copy ($3E): DriveWire runs from it, so
 * whatever maps a screen block puts $3E back before the link is called
 * (fld_release).
 *
 * Lines 0-175 are the field, 176-199 the HUD's three rows.
 */
#include <string.h>
#include "cocoplat.h"
#include "scr3.h"
#include "scr.h"
#include "input.h"
#include "plat.h"

#define SCR_BLOCK 0x30
#define SLOT6 ((uint8_t *)0xC000)

static void set_palette(const uint8_t *p)
{
    uint8_t i;
    for (i = 0; i < 16; ++i)
        ((volatile uint8_t *)0xFFB0)[i] = p[i];
}

void video_init(void)
{
    uint8_t b;
    asm { orcc #$50 }
    for (b = 0; b < 7; ++b) {           /* the screen: all colour 0 */
        MMU6 = SCR_BLOCK + b;
        memset(SLOT6, 0, 8192);
    }
    MMU6 = ROM_BLOCK;
    asm { andcc #$EF }
    *(volatile uint8_t *)0xFF90 = 0x4C;     /* CoCo 3 mode, MMU on, constant page */
    *(volatile uint8_t *)0xFF98 = 0x80;     /* graphics, 1 line a row */
    *(volatile uint8_t *)0xFF99 = 0x3E;     /* 200 lines, 160 bytes, 16 colours */
    *(volatile uint8_t *)0xFF9A = 0x00;     /* the border: colour 0 */
    *(volatile uint8_t *)0xFF9C = 0x00;
    *(volatile uint8_t *)0xFF9D = 0xC0;     /* the screen at $60000 (/8) */
    *(volatile uint8_t *)0xFF9E = 0x00;
    *(volatile uint8_t *)0xFF9F = 0x80;     /* HVEN: 256-byte lines */
#ifdef VIDEO_CARD
    {   /* M0: the test card. 16 bands of the 16 colours, a white square of
         * 40 pixels by 40 lines and one of 80 by 80 to measure the aspect. */
        static const uint8_t rgb[16] = {
            0x00, 0x09, 0x12, 0x1B, 0x24, 0x2D, 0x36, 0x3F,
            0x38, 0x07, 0x26, 0x34, 0x19, 0x0B, 0x16, 0x20 };
        uint8_t y, i;
        uint16_t x;
        set_palette(rgb);
        for (y = 0; y < 200; ++y) {
            uint8_t *line;
            MMU6 = SCR_BLOCK + (y >> 5);
            line = SLOT6 + ((uint16_t)(y & 31) << 8);
            i = y < 176 ? (uint8_t)(y / 11) : 0;
            memset(line, (uint8_t)(i << 4) | i, 160);
            if (y >= 20 && y < 60)
                for (x = 10; x < 30; ++x)
                    line[x] = 0x77;
            if (y >= 80 && y < 160)
                for (x = 60; x < 100; ++x)
                    line[x] = 0x77;
        }
        MMU6 = ROM_BLOCK;
    }
#endif
}

void (*video_hook)(void);
uint8_t scr_seq, field_up;
uint8_t fire_on, fire_cyc[8];
uint8_t shake_n;                        /* frames to shake (the player's OP_SHAKE, a nuke) */
static uint8_t flash_n, shake;

/* a flash: the sky, the dirt and the fire white, the picture shaken (the
 * video's start a line or two down, a few pixels across); video_hook (the
 * field's palette) puts it back */
void video_flash(uint8_t frames)
{
    uint8_t i, w = coco_swap ? 0x30 : 0x3F;     /* white, composite or RGB */
    flash_n = frames;
    if (!frames)
        return;
    for (i = 1; i < 5; ++i)
        ((volatile uint8_t *)0xFFB0)[i] = w;
}

void video_frame(void)
{
    static const uint8_t hs[4] = { 0x81, 0x80, 0x82, 0x80 }, vs[4] = { 0x00, 0x20, 0x00, 0x40 };
    uint8_t was = flash_n | shake;
    if (shake_n)
        --shake_n;
    if (flash_n)
        --flash_n;
    if (flash_n || shake_n) {
        ++shake;
        *(volatile uint8_t *)0xFF9F = hs[shake & 3];
        *(volatile uint8_t *)0xFF9E = vs[(shake >> 1) & 3];
        return;
    }
    if (!was)
        return;
    shake = 0;
    *(volatile uint8_t *)0xFF9F = 0x80;
    *(volatile uint8_t *)0xFF9E = 0x00;
    if (video_hook)
        video_hook();
}

void video_swap(void)
{
    coco_swap ^= 1;
    if (video_hook)
        video_hook();
    else
        scr3_repalette();
}

/* Which monitor: R or C (the palette for it), ENTER or the button goes on
 * (and so does waiting 8 s: RGB). CLEAR switches it later too (input.c). */
void video_question(void)
{
    uint16_t i;
    scr_begin();
    scr_row(8, PAL_HEAD);
    scr_text(8, 16, "MONITOR?");
    scr_text(11, 9, "R: RGB    C: COMPOSITE");
    scr_text(13, 8, "CLEAR SWITCHES IT LATER");
    scr_text(15, 14, "ENTER: GO ON");
    scr_show();
    for (i = 0; i < 480; ++i) {
        frame_wait();
        input_poll();
        input_take();
        if ((ev_key == 'R' || ev_key == 'C') && (ev_key == 'C') != coco_swap)
            video_swap();
        if (ev_key == 'R' || ev_key == 'C' || ev_key == KEY_RETURN || ev_btn)
            break;
    }
}
