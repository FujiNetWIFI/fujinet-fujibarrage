/*
 * plat.c -- the 7800: start-up, the 60 Hz clock, quitting (FujiBlast's
 * src/atari7800/plat.c with the Atari's frame loop).
 *
 * The clock is the frame's DLI (maria.s): frame_count in jiffies of 1/60 s
 * on NTSC and PAL alike, vbi_count every frame (the scripts' clock). It
 * counts on while a network call waits.
 *
 * The game's loop is one pass a jiffy (src/main.c). When it is behind,
 * frame_wait returns at once and the pass is logic only, a few at most.
 */
#ifdef __ATARI7800__

#include "a78.h"
#include "plat.h"
#include "scr.h"
#include "input.h"
#include "sound.h"
#include "nstrans.h"
#include "fujinet-atari7800.h"

#define CATCH_UP 30             /* jiffies behind: past this (a stall), let them go */
#define MAX_SKIP 4              /* passes without drawing, at most, when behind */

static uint16_t seed = 0x1234;
void scr_commit(void);          /* scr.c: the rows' staged colours */

void plat_init(void)
{
    inp_hook = inp_hook_dir = inp_hook_btn = inp_hook_key = 0;
    scr_init();                 /* (the display first: snd_init asks it NTSC or PAL) */
    snd_init();
    input_init();
}

/* Back to the cartridge's CONFIG, saying goodbye to the server. */
void plat_quit(void)
{
    nst_close();
    snd_stop();
    fuji_a7800_config();
}

/* one program: "boot again" is CONFIG's */
void plat_reboot(void)
{
    plat_quit();
}

void frame_wait(void)
{
    static uint16_t seen;
    static uint8_t skipped;     /* passes since the last drawn */
    uint16_t d;
    d = *(volatile uint16_t *)&frame_count - seen;
    if (d > CATCH_UP) {         /* a stall (a call, an appkey): let it go */
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
    scr_commit();
    while (*(volatile uint16_t *)&frame_count == seen)
        ;
    ++seen;
}

uint8_t plat_rand(void)
{
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return (uint8_t)seed ^ RANDOM;
}

uint8_t plat_hz(void)
{
    return m_ntsc ? 60 : 50;
}

#endif
