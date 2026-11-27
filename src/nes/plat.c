/*
 * plat.c -- the NES: start-up, the 60 Hz clock, quitting (FujiBlast's
 * src/nes/plat.c with the SMS's frame loop).
 *
 * The clock is the NMI's (crt0.s): frame_count is the server's 60 Hz (one
 * NES frame in 608 isn't counted), nes_frames every frame (the scripts'
 * clock, vbi_count).
 *
 * The game's loop is one pass a jiffy (src/main.c). When it is behind,
 * frame_wait returns at once and the pass is logic only, a few at most.
 * While it waits, the field's changed cells are drawn (field.c); a frame
 * of a script draws RENDER_MIN however late it runs (the SMS's lesson: a
 * blast's cells must not wait for idle time).
 */
#include <string.h>
#include "nes.h"
#include "game.h"
#include "plat.h"
#include "input.h"
#include "sound.h"
#include "scr.h"
#include "nstrans.h"
#include "field.h"

#define CATCH_UP 30             /* jiffies behind: past this (a stall), let them go */
#define MAX_SKIP 4              /* passes in a row with no drawing, at most */

void field_idle(void);          /* field.c: a changed cell drawn, if any */
void field_frame(void);         /* the sprites, the shake, the fire's colours */
#ifdef CARD
void card_main(void);
#endif

uint8_t gfx_fast;
static uint16_t seed = 0x1234;

void plat_init(void)
{
    inp_hook = inp_hook_dir = inp_hook_btn = inp_hook_key = 0;
    scr_init();                 /* the font into CHR-RAM, the display off */
    snd_init();
    input_init();
}

/* A console has nowhere to go back to (its Reset doesn't reset the
 * cartridge): a goodbye. */
void plat_quit(void)
{
    nst_close();
    snd_stop();
    scr_begin();
    scr_row(10, PAL_HEAD);
    scr_text(10, 0, "            GOODBYE!            ");
    scr_text(13, 4, "YOU CAN TURN THE NES OFF");
    scr_show();
    for (;;)
        ;
}

/* one program: "boot again" is the lobby's */
void plat_reboot(void)
{
    plat_quit();
}

/* The server's round timer says what real time did: each second it ticks,
 * the jiffies counted since are set against 60, and what is missing goes
 * back on the clock, one a pass (the Adam's). */
static uint8_t tm_last = 0xFF;
static uint16_t tm_at;
static int8_t drift;

static void clock_check(void)
{
    uint8_t t = st[OFF_TIMER];
    int16_t d;
    if (t != tm_last) {
        if ((uint8_t)(tm_last - t) == 1) {
            d = drift + 60 - (int16_t)(frame_count - tm_at);
            drift = d > 90 ? 90 : d < -90 ? -90 : (int8_t)d;
        }
        tm_last = t;
        tm_at = frame_count;
    }
    if (drift > 0) {
        __asm__("sei");         /* (the NMI changes it too: a 16-bit add) */
        ++frame_count;
        __asm__("cli");
        --drift;
    }
}

static uint16_t tf_seen;        /* the frame field_frame last had */

void frame_wait(void)
{
    static uint16_t seen;
    static uint8_t skipped;     /* passes since the last drawn */
    uint16_t d;
    clock_check();
    d = *(volatile uint16_t *)&frame_count - seen;
    if (d > CATCH_UP) {         /* a stall (an appkey, a lost link): let it go */
        seen = frame_count - 1;
        d = 1;
    }
    seed += frame_count;
    if (d >= 2 && skipped < MAX_SKIP) {
        ++skipped;
        ++seen;                 /* behind: a pass with no drawing */
        return;
    }
    skipped = 0;
    while (*(volatile uint16_t *)&frame_count == seen)
        field_idle();           /* the field's changed cells, meanwhile */
    gfx_fast = d == 0;
    ++seen;
    tf_seen = frame_count;
    field_frame();
}

/* the scripts' clock (play_segment waits on it): the field drawn meanwhile,
   and each frame's field_frame (the sprites, the shake, the colours) */
uint8_t vbi_now(void)
{
    if (*(volatile uint16_t *)&frame_count != tf_seen) {
        tf_seen = frame_count;
        gfx_fast = 0;
        field_frame();
    } else
        field_idle();
    return (uint8_t)nes_frames;
}

uint8_t plat_rand(void)
{
    /* xorshift, stirred by the frame count */
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return (uint8_t)seed;
}

uint8_t plat_hz(void)
{
    return nes_region ? 50 : 60;        /* (a PAL console or a Dendy: crt0.s counts 60ths) */
}
