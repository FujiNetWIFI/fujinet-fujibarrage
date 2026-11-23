/*
 * video.c -- the CoCo 1/2's display: PMODE 4 (the VDG's RG6, 256x192 in
 * two colours) at $1000, which an NTSC composite TV shows as 128x192 in
 * four artifact colours: black, blue, orange, white. A pair of dots is a
 * colour pixel, and its two bits are the field's value (src/coco2/fld2.asm):
 * 00 tank (black), 01 sky, 10 dirt, 11 fire (white). Which of 01 and 10 a
 * TV shows as blue depends on the phase the machine came up in; coco_swap
 * swaps them (video_swap: CLEAR).
 *
 * Lines 0-167 are the field, 168-191 the HUD's three rows.
 */
#include <string.h>
#include "cocoplat.h"
#include "scr2.h"
#include "scr.h"
#include "input.h"
#include "plat.h"

#define SCREEN ((uint8_t *)0x1000)
#define PIA1B  (*(volatile uint8_t *)0xFF22)

/* The SAM: video mode V2..V0 = 110 (PMODE 4: 6K), the display at $1000
 * (F6..F0 = $1000 / 512 = 8: F3 set). Each bit has a clear address and a
 * set address one above it. */
static void sam_set(void)
{
    asm {
        sta     $FFC0           ; V0 = 0
        sta     $FFC3           ; V1 = 1
        sta     $FFC5           ; V2 = 1
        sta     $FFC6           ; F0 = 0
        sta     $FFC8           ; F1 = 0
        sta     $FFCA           ; F2 = 0
        sta     $FFCD           ; F3 = 1
        sta     $FFCE           ; F4 = 0
        sta     $FFD0           ; F5 = 0
        sta     $FFD2           ; F6 = 0
    }
}

void video_init(void)
{
    memset(SCREEN, 0, 6144);
    sam_set();
    PIA1B = (PIA1B & 0x07) | 0xF8;      /* graphics, RG6, CSS 1 (black and buff) */
#ifdef VIDEO_CARD
    {   /* M0: the test card. Bands of each value (sky, dirt, fire), and
         * squares of 16 colour pixels by 16 lines to measure the aspect. */
        uint8_t y, x;
        for (y = 0; y < 168; ++y) {
            uint8_t b = y < 56 ? 0x55 : y < 112 ? 0xAA : 0xFF;
            memset(SCREEN + (uint16_t)y * 32, b, 32);
        }
        for (y = 20; y < 36; ++y)               /* a black 16x16 square on the sky */
            for (x = 4; x < 8; ++x)
                SCREEN[(uint16_t)y * 32 + x] = 0x00;
        for (y = 60; y < 92; ++y)               /* a black 32x32 one on the dirt */
            for (x = 12; x < 20; ++x)
                SCREEN[(uint16_t)y * 32 + x] = 0x00;
        for (y = 168; y < 192; y += 2)          /* the HUD: stripes */
            memset(SCREEN + (uint16_t)y * 32, 0xFF, 32);
    }
#endif
}

void (*video_hook)(void);
uint8_t scr_seq, field_up;
uint8_t shake_n;                        /* (the player's OP_SHAKE: the CoCo 1/2 can't) */
static uint8_t flash_n;

/* a flash: the VDG's other colour set (CSS 0: the artifacts go green) */
void video_flash(uint8_t frames)
{
    flash_n = frames;
    if (frames)
        PIA1B &= ~0x08;
}

void video_frame(void)
{
    if (flash_n && !--flash_n)
        PIA1B |= 0x08;
}

/* The artifact phase the other way round: every pair 01 <-> 10, on the
 * screen and in the tables that draw it. */
extern const uint8_t flip2[];          /* gen/coco2/flip2.asm: a byte, its pairs swapped */

void video_flip(void)
{
    asm {
        pshs    u
        ldx     #$1000
        leau    _flip2+128,pcr
@l
        ldb     ,x
        eorb    #$80            ; (b as a signed index: b - 128)
        lda     b,u
        sta     ,x+
        cmpx    #$2800
        blo     @l
        puls    u
    }
}


void video_swap(void)
{
    coco_swap ^= 1;
    video_flip();
    scr2_recolour();
    if (video_hook)
        video_hook();
}

/* Which artifact colour is which: the text screen's ground is the sky's
 * colour, so ask whether it is blue; CLEAR swaps it (input.c), ENTER or the
 * button goes on (and so does waiting 8 s). */
void video_question(void)
{
    uint16_t i;
    scr_begin();
    scr_text(9, 6, "IS THE SCREEN BLUE?");
    scr_text(12, 4, "ENTER: YES     CLEAR: NO");
    scr_text(14, 4, "(CLEAR SWAPS IT LATER TOO)");
    scr_show();
    for (i = 0; i < 480; ++i) {
        frame_wait();
        input_poll();
        input_take();
        if (ev_btn || ev_key == KEY_RETURN)
            break;
    }
}
