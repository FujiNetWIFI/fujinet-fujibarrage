/*
 * sound.c -- the H89's one sound: the H19's bell (BEL), a fixed beep. It
 * rings for a blast, a death, your turn, the go and an error, at most every
 * RING_GAP jiffies; the rest is silent. f1 (input.c) turns it off and on.
 */
#include "sound.h"
#include "plat.h"
#include "proto.h"
#include "h19.h"
#include "h89.h"

#define RING_GAP 30

static uint8_t muted;
static uint16_t rung;

void snd_init(void)
{
}

void snd_toggle_mute(void)
{
    muted ^= 1;
}

static void ring(void)
{
    if (muted || (uint16_t)(frame_count - rung) < RING_GAP)
        return;
    rung = frame_count;
    h19_bell();
}

void snd_cue(uint8_t id, uint8_t size)
{
    switch (id) {
    case SND_BOOM:
    case SND_NUKE:
        if (size >= 2 || id == SND_NUKE)
            ring();
        break;
    case SND_DEATH:
    case UI_TURN:
    case UI_GO:
    case UI_ERROR:
        ring();
    }
}

void snd_full(uint8_t on)
{
    (void)on;
}

void snd_whistle(uint8_t on, uint8_t pitch)
{
    (void)on;
    (void)pitch;
}

void snd_blip(uint8_t pitch)
{
    (void)pitch;
}

void snd_stop(void)
{
}

uint8_t snd_busy(void)
{
    return 0;
}

void snd_tune(const uint8_t *ch1, const uint8_t *ch2)
{
    (void)ch1;
    (void)ch2;
}

/* (the tunes are silent here) */
const uint8_t t_title1[1], t_title2[1], t_win1[1], t_win2[1], t_over1[1], t_over2[1], t_shop1[1], t_shop2[1];
