/*
 * plat.c -- the Heathkit H89 (Heath/Zenith CP/M 2.2.04 or HDOS 2.0, the H19
 * terminal, the H89-ESP32 FujiNet card): start-up, the frame clock and
 * quitting.
 *
 * The clock is the 2 ms interrupt, counted in 60ths of a second by
 * h89io.asm. One pass of the game is a jiffy of logic (the walking, the
 * timers), so a pass that comes after the next jiffy has already come is
 * made up by one that neither waits nor draws (frame_wait returns at
 * once): the game keeps real time though the drawing -- a 2 MHz Z80
 * working out what the 9600-baud line must carry -- can't keep up with 60
 * a second. A long stall (a FujiBus call, an appkey) is let go rather than
 * raced after.
 */
#include "plat.h"
#include "proto.h"
#include "scr.h"
#include "field.h"
#include "fld.h"
#include "input.h"
#include "sound.h"
#include "h19.h"
#include "h89.h"

#define CATCH_UP   20           /* jiffies behind we make up; further, we let go */
#define STACK_ROOM 384

uint16_t h89_passes, h89_skip, h89_drawn;   /* for a rig: passes, made up, drawn */
uint16_t h89_idle_ticks, h89_draw_ticks;    /* 2 ms ticks spent waiting for the clock, drawing */
uint8_t h89_stall;                          /* most jiffies one pass took (a rig clears it) */
uint8_t plat_will_draw = 1;                 /* (play.c: PLAT_SEATS_WHEN_DRAWN) */
static uint16_t seed = 0x1234;
extern uint8_t _BSS_END_tail[];

void plat_init(void)
{
#ifndef __HDOS__
    /* (HDOS: src/h89/tpa.asm has made sure of the room) */
    uint16_t top = *(uint16_t *)6, end = (uint16_t)_BSS_END_tail;
    if (top < end + STACK_ROOM) {
        os_print("FUJIBARRAGE NEEDS MORE MEMORY: A 62K OR 64K CP/M\r\n$");
        os_exit();
    }
#endif
    if (!con_install()) {
        os_print("FUJIBARRAGE: THIS SYSTEM IS NOT ONE IT KNOWS\r\n$");
        os_exit();
    }
    h19_init();
    snd_init();
    input_init();
    scr_init();
}

/* the terminal and the keyboard back as they were (plat_quit; the HDOS
 * overlay loader's fatal path) */
void plat_shutdown(void)
{
    h19_exit();
    con_remove();
}

void plat_quit(void)
{
    plat_shutdown();
    os_exit();
}

static uint16_t now(void)
{
    uint16_t n;
    __asm__("di");              /* (the interrupt changes it) */
    n = frame_count;
    __asm__("ei");
    return n;
}

void frame_wait(void)
{
    static uint16_t seen, d, t;
    d = now() - seen;
    ++h89_passes;
    if (d > CATCH_UP) {
        seen = now() - 1;
        d = 1;
    }
    if (d >= 2) {
        ++seen;                 /* behind: a pass with no drawing */
        ++h89_skip;
        con_pump();
    } else {
        ++h89_drawn;
        t = ticks_now();
#ifndef H89_NODRAW
        h89_frame();
#endif
        h89_draw_ticks += ticks_now() - t;
        t = ticks_now();
        while (now() == seen)
            con_pump();
        h89_idle_ticks += ticks_now() - t;
        d = now() - seen;
        if (d > h89_stall && d < 256)
            h89_stall = (uint8_t)d;
        ++seen;
        seed += seen;
    }
    /* the next pass is likely drawn if it starts this soon (play.c lays
     * the seats out only then) */
    plat_will_draw = (uint16_t)(now() - seen) < 2;
}

/* what the frame changed, composed into the screen and sent (as much as the
 * line takes) */
void h89_frame(void)
{
    if (field_up)
        field_sync();
    h19_flush();
}

/* the script's clock (play_segment waits on it): the screen sent meanwhile */
static uint16_t tf_seen;
uint8_t vbi_now(void)
{
    uint16_t n = now();
    if (n != tf_seen) {
        tf_seen = n;
        seed += n;
        ++h89_drawn;
        h89_frame();
    } else
        con_pump();
    return (uint8_t)n;
}

/* menus.c's tick and play.c's entry_len, here: resident, since net.c and
 * the main loop call them every pass and the HDOS build's overlays hold
 * those files */
#include "input.h"
void tick(void)
{
    frame_wait();
    input_poll();
    input_take();
}

uint16_t entry_len(const uint8_t *e)
{
    return *e <= T_EMPTY ? 1 : 1 + 2 * (*e - T_SPANS);
}

uint8_t plat_hz(void)
{
    return 60;
}

void plat_reboot(void)
{
    plat_quit();
}

uint8_t plat_rand(void)
{
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return (uint8_t)seed;
}
