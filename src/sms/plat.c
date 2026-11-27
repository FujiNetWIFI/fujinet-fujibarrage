/*
 * plat.c -- the Sega Master System (a FujiNet cartridge): start-up, the 60 Hz
 * clock and quitting (the Adam's src/adam/plat.c, FujiBlast's src/sms/plat.c).
 *
 * The clock is the VDP's frame interrupt (smsint.asm): frame_count in 60ths
 * of a second (a 50 Hz console counts every fifth frame twice), vbi_ticks
 * the scripts' clock (display frames).
 *
 * The game's loop is one pass a jiffy (src/main.c). When it is behind,
 * frame_wait returns at once and the pass is logic only, a few at most.
 * While it waits, the field's changed cells are drawn (sms_idle: the tile
 * cache, src/sms/render.asm); each frame's sprites and colours are set up
 * for the interrupt to put in (sms_frame). A script's frames also draw
 * RENDER_MIN cells each, however late it runs: a blast's fireball, drawn
 * again as it grows, kept the client behind, so its cells waited for idle
 * time that never came and only the crater was seen.
 */
#include <string.h>
#include "sms.h"
#include "game.h"
#include "input.h"
#include "plat.h"
#include "sound.h"
#include "scr.h"
#include "nstrans.h"
#include "fujinet-sms.h"

#define CATCH_UP 30             /* jiffies behind: past this (a stall), let them go */
#ifndef RENDER_MIN
#define RENDER_MIN 6            /* the field's cells drawn each frame of a script, at least */
#endif
#define MAX_SKIP 4              /* passes in a row with no drawing, at most */

void sms_int_init(void);
uint8_t fld_flush1(void);       /* a dirty cell drawn: 0 none (render.asm) */
extern uint8_t fld_live;        /* the field is shown (src/tms9918/fld.asm) */
void sms_map_init(void);
void mus_init(void);
uint8_t sms_pal_probe(void);
void sms_vdp_init(void);
#ifdef VIDEO_CARD
void sms_card(void);
#endif

uint8_t gfx_fast;
uint8_t ram_ok;                 /* the cart's 32K of RAM is there */
static uint16_t seed = 0x1234;

/* the stack (down from DFF0h) painted, for the rigs' low-water mark: from
 * the variables' end (__BSS_END_tail) up to a little under here */
extern uint8_t _BSS_END_tail[];
static void stack_paint(void)
{
    uint8_t here, *p = (uint8_t *)_BSS_END_tail;
    while (p < &here - 32)
        *p++ = 0xA5;
}

void plat_init(void)
{
    sms_map_init();             /* slots 0-2; the cart's RAM half 0 in slot 2 */
    stack_paint();
    inp_hook = inp_hook_dir = inp_hook_btn = inp_hook_key = 0;
    ram_ok = sms_ram_ok();      /* (leaves half 0 in slot 2) */
    sms_vdp_init();             /* the VDP's tables; its interrupt on */
    sms_int_init();
    sms_pal = sms_pal_probe();
    snd_init();
    mus_init();                 /* (a YM2413? the tunes play on it) */
    input_init();
#ifdef VIDEO_CARD
    sms_card();                 /* M0: the test card, the clock running */
#endif
}

/* Back to the cartridge's CONFIG (its loader page, out of slot 2's way). */
void plat_quit(void)
{
    nst_close();
    snd_stop();
    if (fuji_sms_present())
        fuji_sms_exit_to_config();
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
        __asm__("di");          /* (the interrupt changes it too) */
        ++frame_count;
        __asm__("ei");
        --drift;
    }
}

static uint16_t tf_seen;        /* the frame sms_frame last had */

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
        sms_idle();             /* the field's changed cells, meanwhile */
    gfx_fast = d == 0;          /* we waited: the blank is ours */
    ++seen;
    tf_seen = frame_count;
    sms_frame();
}

/* the scripts' clock (play_segment waits on it): the field drawn meanwhile,
   and each frame's sms_frame (the sprites, the shake, the colours) */
uint8_t vbi_now(void)
{
    uint8_t i;
    if (*(volatile uint16_t *)&frame_count != tf_seen) {
        tf_seen = frame_count;
        gfx_fast = 0;           /* (how far into the frame: not known) */
        sms_frame();
        if (fld_live)           /* some cells however late (a blast's) */
            for (i = 0; i < RENDER_MIN && fld_flush1(); ++i)
                ;
    } else
        sms_idle();
    return vbi_ticks;
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
    return sms_pal ? 50 : 60;
}
