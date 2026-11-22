/*
 * sound.c -- FujiBarrage's sounds as POKEY steps (snd.s): AUDF, AUDC, frames.
 * AUDF at the 64 kHz clock: 63921 / (2 (N + 1)) Hz. AUDC: $A0 a pure tone,
 * $80 noise, $C0 a buzz, $20 a rumble; the low nibble the volume.
 *
 * Slots: 0 the effect, 1 its body or the UI, 2 and 3 (link held only) the
 * low end of blasts and the whistle.
 */
#ifdef __ATARI__

#include <string.h>
#include "atari.h"
#include "sound.h"
#include "proto.h"

extern uint8_t snd_ptr[8];
#pragma zpsym("snd_ptr")
extern uint8_t snd_left[8], snd_all, snd_ctl, snd_f[8], snd_c[8], snd_mute;

#define END 0, 0, 0

static const uint8_t s_fire[]   = { 20, 0x8C, 2, 40, 0x8A, 2, 80, 0x86, 3, 120, 0x83, 4, END };
static const uint8_t s_thump[]  = { 200, 0xCA, 3, 230, 0xC8, 3, 255, 0xC5, 4, END };
static const uint8_t s_boom0[]  = { 30, 0x8C, 3, 50, 0x8A, 3, 80, 0x87, 4, 120, 0x84, 4, END };
static const uint8_t s_boom1[]  = { 40, 0x8F, 3, 60, 0x8E, 3, 80, 0x8C, 4, 100, 0x8A, 4, 140, 0x88, 5,
                                    180, 0x86, 6, 220, 0x84, 6, 255, 0x82, 8, END };
static const uint8_t s_low1[]   = { 240, 0x2F, 4, 250, 0x2D, 6, 255, 0x2A, 8, 255, 0x26, 10, 255, 0x23, 10, END };
static const uint8_t s_boom3[]  = { 60, 0x8F, 6, 90, 0x8F, 6, 120, 0x8E, 8, 160, 0x8C, 10, 200, 0x8A, 12,
                                    230, 0x88, 14, 255, 0x86, 16, 255, 0x84, 20, 255, 0x82, 24, END };
static const uint8_t s_low3[]   = { 255, 0x2F, 20, 255, 0x2E, 20, 255, 0x2C, 20, 255, 0x2A, 20, 255, 0x27, 20,
                                    255, 0x24, 20, 255, 0x22, 20, END };
static const uint8_t s_shield[] = { 24, 0xAC, 2, 18, 0xAA, 2, 12, 0xA8, 3, 24, 0xA4, 4, END };
static const uint8_t s_deflect[]= { 40, 0xAA, 2, 30, 0xAA, 2, 20, 0xA8, 2, 30, 0xA6, 2, 40, 0xA4, 3, END };
static const uint8_t s_death[]  = { 60, 0xAA, 4, 72, 0xAA, 4, 85, 0xA9, 4, 100, 0xA8, 5, 121, 0xA7, 6,
                                    144, 0xA6, 6, 170, 0xA5, 8, 200, 0xA4, 10, END };
static const uint8_t s_bounce[] = { 50, 0xA8, 2, 70, 0xA6, 2, END };
static const uint8_t s_dirt[]   = { 4, 0x84, 6, 6, 0x83, 8, 8, 0x82, 10, END };
static const uint8_t s_chute[]  = { 100, 0xA4, 10, 110, 0xA4, 10, 120, 0xA3, 10, END };
static const uint8_t s_tick[]   = { 60, 0xA6, 1, END };
static const uint8_t s_select[] = { 47, 0xA8, 3, END };
static const uint8_t s_go[]     = { 47, 0xAA, 6, 35, 0xAA, 6, 29, 0xAA, 12, END };
static const uint8_t s_error[]  = { 200, 0xC8, 10, END };
static const uint8_t s_turn[]   = { 60, 0xAA, 5, 47, 0xAA, 5, 40, 0xAA, 10, END };
static const uint8_t s_napalm[] = { 12, 0x88, 2, 30, 0x86, 2, 8, 0x87, 2, 40, 0x85, 3, 16, 0x86, 3,
                                    60, 0x84, 4, 20, 0x83, 4, 50, 0x82, 6, END };   /* a crackle */
static const uint8_t s_laser[]  = { 10, 0xAA, 1, 14, 0xAA, 1, 20, 0xA9, 1, 28, 0xA8, 1, 40, 0xA7, 2,
                                    60, 0xA5, 2, 90, 0xA3, 3, END };                /* a zap */
static const uint8_t s_plasma[] = { 30, 0xA8, 2, 34, 0xA8, 2, 30, 0xA8, 2, 34, 0xA7, 2, 30, 0xA6, 2,
                                    34, 0xA5, 2, 30, 0xA4, 3, END };                /* a warble */
static const uint8_t s_dig[]    = { 120, 0xC6, 3, 140, 0xC5, 3, 120, 0xC6, 3, 140, 0xC5, 3,
                                    160, 0xC4, 4, END };                            /* a grind */
static const uint8_t s_roll[]   = { 200, 0xC5, 4, 220, 0xC4, 4, 200, 0xC4, 4, 230, 0xC3, 6, END };

extern uint8_t snd_pal;

static void play(uint8_t slot, const uint8_t *s)
{
    uint8_t x = slot * 2;
    snd_ptr[x + 1] = 0;             /* the VBI leaves it alone meanwhile */
    snd_left[x] = 0;
    snd_ptr[x] = (uint8_t)(uint16_t)s;
    snd_ptr[x + 1] = (uint16_t)s >> 8;
}

/* a tune (tools/mkmus_atari.py): ch1 on voice 1, ch2 on voice 2 */
void snd_tune(const uint8_t *a, const uint8_t *b)
{
    play(0, a);
    play(1, b);
}

void snd_init(void)
{
    memset(snd_c, 0, sizeof(snd_c));
    snd_pal = (GTIA_PAL & 0x0E) == 0;   /* GTIA: 1 on NTSC, 0 on PAL */
}

/* the cues that are one sound on voice 1 (0: see snd_cue) */
static const uint8_t *const voice1[16] = {
    0, 0, 0, s_napalm, s_laser, s_plasma, s_dig, s_roll,
    0, s_shield, s_deflect, s_chute, s_death, s_bounce, 0, 0
};
static const uint8_t *const ui[5] = { s_tick, s_select, s_go, s_error, s_turn };

void snd_cue(uint8_t id, uint8_t size)
{
    const uint8_t *p;
    if (id >= UI_TICK) {
        id -= UI_TICK;
        if (id < 5)
            play(1, ui[id]);
        return;
    }
    if (id < 16 && (p = voice1[id])) {
        play(1, p);
        return;
    }
    switch (id) {
    case SND_FIRE:
        play(0, s_fire);
        if (snd_all)
            play(2, s_thump);
        break;
    case SND_NUKE:
        size = 3;
    case SND_BOOM:
        play(0, size >= 3 ? s_boom3 : size ? s_boom1 : s_boom0);
        if (snd_all && size)
            play(2, size >= 2 ? s_low3 : s_low1);
        if (snd_all && size >= 3)
            play(3, s_boom3 + 6);
        break;
    case SND_DIRT:
        play(snd_all ? 3 : 1, s_dirt);
        break;
    case SND_THUNDER:
        play(0, s_boom3);
        if (snd_all)
            play(2, s_low3);
        break;
    }
}

void snd_full(uint8_t on)
{
    if (on) {
        snd_ptr[5] = snd_ptr[7] = 0;
        snd_c[4] = snd_c[6] = 0;
        snd_ctl = 0x00;             /* 64 kHz, no joins (ours while held) */
        snd_all = 1;
    } else {
        snd_all = 0;
        snd_ptr[5] = snd_ptr[7] = 0;
    }
}

void snd_whistle(uint8_t on, uint8_t pitch)
{
    uint8_t x = snd_all ? 6 : 2;
    snd_f[x] = pitch;
    snd_c[x] = on ? 0xA3 : 0;
}

void snd_blip(uint8_t pitch)
{
    snd_f[2] = pitch;
    snd_c[2] = 0xA6;
}

uint8_t snd_busy(void)
{
    return snd_ptr[5] || snd_ptr[7] || snd_ptr[1];
}

void snd_stop(void)
{
    uint8_t i;
    for (i = 1; i < 8; i += 2)
        snd_ptr[i] = 0;
    memset(snd_c, 0, sizeof(snd_c));
}

#endif
