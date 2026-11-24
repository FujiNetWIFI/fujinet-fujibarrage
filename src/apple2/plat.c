/*
 * plat.c -- Apple II start-up, the 60 Hz clock and quitting (FujiBlast's
 * src/apple2/plat.c, from M.U.L.E.).
 *
 * A 128K //e, //c or IIgs (/RAM unplugged: the aux RAM is the game's). The
 * clock is lowlevel.s's frame_tick: a Mockingboard's timer when there is
 * one, else the vertical blank. The game's loop is one pass a jiffy: when
 * a pass took longer, frame_wait returns at once (a pass of logic only),
 * as the Atari's does; a long stall (a SmartPort call, an appkey) is let
 * go rather than raced after. A clock can miss jiffies (the //e's polled
 * blank in a long draw, any of them in a SmartPort call longer than the
 * 6522 wraps): the server's round timer puts them back (clock_check).
 */
#ifdef __APPLE2__

#include <apple2.h>
#include <stdlib.h>
#include "plat.h"
#include "scr.h"
#include "input.h"
#include "sound.h"
#include "nstrans.h"
#include "game.h"

#define CATCH_UP 30             /* jiffies behind: past this (a stall), let them go */
#define MAX_SKIP 4              /* passes without drawing, at most, when behind */

/* lowlevel.s */
void clk_init(uint8_t mode);
uint8_t mb_detect(void);
void hgr_on(void);
void text_on(void);
extern uint8_t mb_slot, clk_mode;
#pragma zpsym ("mb_slot")
#pragma zpsym ("clk_mode")

static uint16_t seed = 0x1234;

/* kept in DATA: the constructor runs before BSS is cleared */
static uint8_t boot_mode = 0xFF;

#define MACHID   (*(volatile uint8_t *)0xBF98)
#define DEVCNT   (*(volatile uint8_t *)0xBF31)
#define DEVLST   ((volatile uint8_t *)0xBF32)
#define DEVADR32 (*(volatile uint16_t *)0xBF26)      /* slot 3, drive 2 */
#define NODEV    (*(volatile uint16_t *)0xBF10)

/* ---- start-up, once: a constructor (before main, before BSS is cleared
 * and the language card's code copied: nothing here may write BSS), in
 * the ONCE segment, which BSS takes over after. ----------------------- */
#pragma code-name (push, "ONCE")
#pragma rodata-name (push, "ONCE")
#pragma static-locals (push, off)

/* A message on the text screen, then back to ProDOS. */
static void fail(const char *s)
{
    uint8_t *p = (uint8_t *)0x0400, i;
    for (i = 0; i < 40; ++i)
        p[i] = ' ' | 0x80;
    for (i = 0; s[i] && i < 40; ++i)
        p[i] = s[i] | 0x80;
    *(volatile uint8_t *)0xC051 = 0;
    while (!(*(volatile uint8_t *)0xC000 & 0x80))
        ;
    *(volatile uint8_t *)0xC010 = 0;
    exit(1);
}

/* /RAM lives in aux RAM: unplug it (ProDOS TN #8). */
static void ram_off(void)
{
    uint8_t i, j;
    if (DEVADR32 == NODEV)
        return;
    for (i = DEVCNT + 1; i--;) {
        if ((DEVLST[i] & 0xF0) == 0xB0) {
            for (j = i; j < DEVCNT; ++j)
                DEVLST[j] = DEVLST[j + 1];
            DEVLST[DEVCNT] = 0;
            --DEVCNT;
            DEVADR32 = NODEV;
            return;
        }
    }
}

void plat_boot(void)
{
    uint8_t t = get_ostype();
    if (t < APPLE_IIE)
        fail("FUJIBARRAGE NEEDS AN APPLE //E, //C OR IIGS");
    if ((MACHID & 0x30) != 0x30)
        fail("FUJIBARRAGE NEEDS 128K");
    *(volatile uint8_t *)0xC000 = 0;    /* 80STORE off */
    ram_off();
    boot_mode = t >= APPLE_IIGS ? 1 : (t >= APPLE_IIC && t < APPLE_IIGS) ? 2 : 0;
}

#pragma static-locals (pop)
#pragma rodata-name (pop)
#pragma code-name (pop)

void plat_init(void)
{
#ifdef FORCE_CLK                        /* (a test build: one clock, whatever is there) */
    mb_detect();
    clk_init(FORCE_CLK);
#else
    clk_init(mb_detect() ? 3 : boot_mode);
#endif
    snd_init();
    input_init();
    scr_init();
}

void plat_quit(void)
{
    nst_close();
    text_on();
    exit(0);
}

/* the menu and the game are two programs (src/apple2/chain.s): "boot the
 * disk again" is the game's loader, run as ProDOS would */
uint8_t chain(const char *name);

void plat_reboot(void)
{
    nst_close();
    chain("BARRAGE.SYSTEM");
    plat_quit();                /* (not on this disk) */
}

/* Jiffies can go uncounted: the //e's polled blank misses those in a
 * stretch longer than a frame, the //c's latch all but one, the 6522 each
 * 65536 cycles in one stretch (a slow SmartPort call). The server's round
 * timer says what real time did: each second it ticks, the jiffies counted
 * since are set against 60, and what is missing goes back on the clock,
 * one a pass. (A clock that loses none is left alone: it balances out.)
 * Only inside a phase, and not a shot's: a shot's deadline isn't whole
 * seconds, so its first second is short, and the jiffies that put on (up
 * to a second's, at once) played the sound at double speed. */
static uint8_t tm_last = 0xFF, tm_ph = 0xFF;
static uint16_t tm_at;
static int8_t drift;

static void clock_check(void)
{
    uint8_t t = st[OFF_TIMER];
    int16_t d;
    if (st[OFF_PHASE] != tm_ph || tm_ph == PH_SHOT) {
        tm_ph = st[OFF_PHASE];
        tm_last = 0xFF;
    } else if (t != tm_last) {
        if ((uint8_t)(tm_last - t) == 1) {
            d = drift + 60 - (int16_t)(frame_count - tm_at);
            drift = d > 90 ? 90 : d < -90 ? -90 : (int8_t)d;
        }
        tm_last = t;
        tm_at = frame_count;
    }
    if (drift > 0) {
        ++frame_count;
        --drift;
    }
}

void frame_wait(void)
{
    static uint16_t seen;
    static uint8_t skipped;     /* passes since the last drawn */
    uint16_t d;
    frame_tick();
    clock_check();
    d = frame_count - seen;
    if (d > CATCH_UP) {         /* a stall (a SmartPort call, an appkey): let it go */
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
    while (frame_count == seen)
        frame_tick();
    ++seen;
}

uint8_t plat_rand(void)
{
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return (uint8_t)seed;
}

uint8_t plat_hz(void)
{
    return 60;
}

#endif
