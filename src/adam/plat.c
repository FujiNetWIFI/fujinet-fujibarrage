/*
 * plat.c -- the Coleco Adam: start-up, the 60 Hz clock, the two programs
 * and quitting (FujiBlast's src/adam/plat.c, from M.U.L.E.'s).
 *
 * The clock is the VDP's frame interrupt, the Z80's NMI (nmi.asm): it
 * counts on while EOS waits for AdamNet, so a network call loses no time.
 * It counts frame_count (the game's 60ths) and vbi_ticks (the scripts'
 * clock, display frames).
 *
 * The game's loop is one pass a jiffy (src/main.c). When it is behind,
 * frame_wait returns at once and the pass is logic only, a few at most.
 * While it waits, the field's changed cells are drawn (src/tms9918/).
 */
#include <string.h>
#include "adam.h"
#include "tms.h"
#include "game.h"
#include "input.h"
#include "plat.h"
#include "sound.h"
#include "scr.h"
#include "nstrans.h"
#include "tmsgfx.h"
#include "fujinet-fuji.h"

#define CATCH_UP 30             /* jiffies behind: past this (a stall), let them go */
#define MAX_SKIP 4              /* passes in a row with no drawing, at most */

uint8_t gfx_fast;
static uint16_t seed = 0x1234;
static uint8_t hz = 60;
uint16_t hz_n;                  /* (for the rig: the loop's count over a frame) */

/* the stack (D100h-D38Fh, under EOS: 656 bytes; a live game used 178) painted,
 * for the rigs' low-water mark */
#define STACK_LOW 0xD100
static void stack_paint(void)
{
    uint8_t here, *p = (uint8_t *)STACK_LOW;
    while (p < &here - 32)      /* (the live frames over it) */
        *p++ = 0xA5;
}

void vdp_reg(uint8_t r, uint8_t v)
{
    VDP_LOCK();
    VDP_CTRL = v;
    VDP_CTRL = 0x80 | r;
    VDP_UNLOCK();
}

/* The field rate, 60 or 50 (a PAL machine): a loop counted from one frame
 * to the next. (adamcore is NTSC; at 3.58 MHz a frame is 59,736 T-states
 * at 60 and 71,364 at 50, the loop 26 T-states.) */
static void hz_measure(void)
{
    __asm
        ld      hl,_frame_count
        ld      a,(hl)
    hz_a:
        cp      (hl)            ; (a frame first)
        jr      z,hz_a
        ld      a,(hl)
        ld      bc,0
    hz_b:
        inc     bc              ; 6
        cp      (hl)            ; 7
        jr      z,hz_b          ; 12 (26 a turn)
        ld      (_hz_n),bc
    __endasm;
    hz = hz_n > 2600 ? 50 : 60;    /* (adamcore: 2373 at 60; about 2850 at 50) */
    nmi_pal = hz == 50;
}

void plat_init(void)
{
    /* Booted from CONFIG, the VDP's interrupt can be pending already: EOS
     * reads the boot block with the SmartWriter ROM paged in, whose NMI
     * handler leaves the status unread. The line then stays down and no
     * NMI comes again until the status is read: read it now. (And after a
     * chain, the loader left the interrupt off.) */
    vdp_catch_up();
    stack_paint();
    inp_hook = inp_hook_dir = inp_hook_btn = inp_hook_key = 0;
    tms_init();                 /* the VDP's tables; its interrupt on */
    art_load();                 /* the font into the VRAM (chain.asm) */
    hz_measure();
    snd_init();
    input_init();
#ifdef VIDEO_CARD
    tms_card();                 /* M0: the test card, the clock running */
#endif
#ifdef CHAIN_TEST
    {                           /* M0: each program shows its colour, counts, loads the other */
        uint16_t t = frame_count;
#ifdef MENU
        vdp_reg(7, C_MRED);
#else
        vdp_reg(7, C_MGREEN);
#endif
        tms_blank(0);
        ++*(volatile uint8_t *)0xCFFE;
        while ((uint16_t)(frame_count - t) < 90)
            ;
#ifdef MENU
        chain_run(CH_GAME);
#else
        chain_run(CH_MENU);
#endif
        vdp_reg(7, C_WHITE);    /* (not there) */
        for (;;)
            ;
    }
#endif
}

/* Back to the FujiNet's CONFIG, as the Adam's reset switch would. */
void plat_quit(void)
{
    fuji_set_boot_config(1);
    __asm
        di
        xor     a
        out     (0x7F),a        ; SmartWriter ROM and the lower 32K of RAM
        jp      0
    __endasm;
}

/* The DCB table EOS keeps (FEC3h: how many; FEC4h: 21 bytes each). The
 * FujiNet answers as device 0Fh. */
uint8_t plat_net_probe(void)
{
    uint8_t n = *(uint8_t *)0xFEC3, i;
    const uint8_t *d = (const uint8_t *)0xFEC4;
    for (i = 0; i < n; ++i, d += 21)
        if ((d[16] & 0x0F) == 0x0F)
            return 1;
    return 0;
}

/* The server's round timer says what real time did: each second it ticks,
 * the jiffies counted since are set against 60, and what is missing goes
 * back on the clock, one a pass (the CoCo's; the NMI hardly misses one,
 * but a long VDP burst holds it back a frame). */
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
        VDP_LOCK();             /* (the NMI changes it too) */
        ++frame_count;
        VDP_UNLOCK();
        --drift;
    }
}

static uint16_t tf_seen;        /* the frame tms_frame last had */

void frame_wait(void)
{
    static uint16_t seen;
    static uint8_t skipped;     /* passes since the last drawn */
    uint16_t d;
    clock_check();
    d = *(volatile uint16_t *)&frame_count - seen;
    if (d > CATCH_UP) {         /* a stall (a network call, an appkey): let it go */
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
        tms_idle();             /* the field's changed cells, meanwhile */
    gfx_fast = d == 0;          /* we waited: the blank is ours */
    ++seen;
    tf_seen = frame_count;
    tms_frame();
}

/* the scripts' clock (play_segment waits on it): the field drawn meanwhile,
   and each frame's tms_frame (the sprites, the shake, the fire's colours) */
uint8_t vbi_now(void)
{
    if (*(volatile uint16_t *)&frame_count != tf_seen) {
        tf_seen = frame_count;
        gfx_fast = 0;           /* (how far into the frame: not known) */
        tms_frame();
    } else
        tms_idle();
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
    return hz;
}

/* ---- the menu and the game: two programs (chain.asm) ------------------ */

/* the menu (main.c): it doesn't come back */
uint8_t xload(void)
{
    nst_close();
    snd_stop();
    chain_run(CH_MENU);
    return 0;
}

/* "boot again": the menu chains to the game; the game, which left its room
 * (main.c cleared it from the appkeys), straight to the menu */
void plat_reboot(void)
{
    nst_close();
    snd_stop();
#ifdef MENU
    chain_run(CH_GAME);
#else
    chain_run(CH_MENU);
#endif
    plat_quit();                /* (not on this medium) */
}
