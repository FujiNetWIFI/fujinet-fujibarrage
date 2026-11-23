/*
 * sound.c -- the sound (sound.h) on the CoCo 1/2 and the CoCo 3: the
 * Apple's Mockingboard driver (src/apple2/mock.s) over three voices of the
 * machine's own engine, which makes them on the 6-bit DAC:
 *   CoCo 3    the GIME timer's IRQ at each voice's edges (src/coco3/snd3a.asm)
 *   CoCo 1/2  a mixer in the CPU's idle time: frame_wait's wait for the
 *             jiffy and the script player's (vbi_now) (src/coco2/snd2a.asm)
 *
 * The voices, as the AY's:
 *   0  an effect's body, or a tune's first channel
 *   1  the falling shell's whistle and a talker's blips, or a tune's second
 *   2  an effect's low end (a boom's rumble), or, while a tune plays, the
 *      effect's body (the menus' cues)
 * The effects are tools/mksfx_coco.py's steps (the Intellivision's), the
 * tunes its words (from music/), a jiffy at a time (snd_tick: a pass's
 * jiffies, 8 at most, so a late pass keeps the lengths).
 */
#include "cocoplat.h"
#include "plat.h"
#include "sound.h"
#include "proto.h"

/* tools/mksfx_coco.py: gen/coco{2,3}/sfx.asm, tunes_*.asm */
extern const uint8_t *const sx_cues[], *const sx_booms[], *const sx_ui[];
extern const uint8_t vol_lin[];
extern const uint16_t bl_units[], mus_per[], mus_nz[];
#define NOISE0 112

/* the engine */
void eng_init(void);
void eng_voice(uint8_t v, uint16_t units, uint8_t lvl, uint8_t noise);   /* lvl 0: quiet */
void eng_level(uint8_t v, uint8_t lvl);
void eng_gate(uint8_t go);
#ifndef COCO3
void eng_play(uint8_t seen);        /* sounds until the jiffy is over */
#endif

static const uint8_t *fx_p[2];      /* each effect's next step (0: none): 0 the body, 1 the low end */
static uint8_t fx_left[2], fx_vo[2], fx_e[2];
static uint16_t fx_env[2], fx_dv[2];    /* an envelope's volume (8.8), its fall a jiffy */
static const uint16_t *mu_p[2], *mu_r[2];
static uint8_t mu_on, mu_left[2];   /* a tune's channels (bits 0, 1) */
static uint8_t bl_left, muted, on;
static uint8_t last;                /* frame_count's low byte at the last tick */

static void quiet(uint8_t v)
{
    eng_voice(v, 0, 0, 0);
}

/* effect k's next step */
static void fx_next(uint8_t k)
{
    const uint8_t *p = fx_p[k];
    uint8_t f, vo = fx_vo[k];
    if (!p[0]) {
        fx_p[k] = 0;
        quiet(vo);
        return;
    }
    fx_left[k] = p[0];
    f = p[1];
    if (f & 0x20) {                     /* the envelope: from 15, falling */
        fx_e[k] = 1;
        fx_env[k] = 15u << 8;
        fx_dv[k] = (uint16_t)p[4] << 8 | p[5];
        fx_p[k] = p + 6;
    } else {
        fx_e[k] = 0;
        fx_p[k] = p + 4;
    }
    eng_voice(vo, (uint16_t)p[2] << 8 | p[3], vol_lin[f & 15], f & 0x10);
}

/* effect slot k: steps at p (0: none); the body on voice 0 (2 while a tune
 * plays), the low end on 2 */
static void start(uint8_t k, const uint8_t *p)
{
    if (!p)
        return;
    fx_vo[k] = k ? 2 : mu_on ? 2 : 0;
    fx_p[k] = p;
    fx_next(k);
}

void snd_cue(uint8_t id, uint8_t size)
{
    const uint8_t *const *t;
    if (muted || !on)
        return;
    if (id == UI_TICK)
        return;                         /* (the scroll tick slowed the lists down) */
    if (id >= UI_TICK) {
        id -= UI_TICK;
        if (id < 6)
            start(0, sx_ui[id]);
        return;
    }
    if (id >= 16)
        return;
    t = id == SND_BOOM ? sx_booms + ((size & 3) << 1) : sx_cues + (id << 1);
    start(0, t[0]);
    if (t[1] && !mu_on)
        start(1, t[1]);
}

void snd_full(uint8_t go)
{
    (void)go;                           /* (all three voices are the field's always) */
}

uint8_t snd_busy(void)
{
    return fx_p[0] || fx_p[1];
}

void snd_stop(void)
{
    uint8_t v;
    fx_p[0] = fx_p[1] = 0;
    mu_on = 0;
    bl_left = 0;
    for (v = 0; v < 3; ++v)
        quiet(v);
}

void snd_tune(const uint8_t *ch1, const uint8_t *ch2)
{
    if (muted || !on)
        return;
    mu_p[0] = (const uint16_t *)ch1;
    mu_p[1] = (const uint16_t *)ch2;
    mu_left[0] = mu_left[1] = 0;
    if (fx_p[1]) {                      /* (the low end gone: voice 2 is the cues') */
        fx_p[1] = 0;
        quiet(2);
    }
    if (fx_p[0] && !fx_vo[0])           /* a body on voice 0 ends */
        fx_p[0] = 0;
    mu_on = 3;
}

/* the falling shell's whistle: pitch the Atari's AUDF (play.c), voice 1 */
void snd_whistle(uint8_t go, uint8_t pitch)
{
    if (mu_on || bl_left || !on)
        return;
    if (go)
        eng_voice(1, bl_units[pitch & 127], vol_lin[6], 0);
    else
        quiet(1);
}

/* a talker's letter (0: off), voice 1 */
void snd_blip(uint8_t pitch)
{
    if (mu_on || !on)
        return;
    if (!pitch) {
        bl_left = 0;
        quiet(1);
        return;
    }
    bl_left = 3;
    eng_voice(1, bl_units[pitch & 127], vol_lin[10], 0);
}

/* a jiffy of tune channel ch (voice ch) */
static void mu_step(uint8_t ch)
{
    uint16_t w;
    uint8_t code, j, v;
    if (!(mu_on & (ch + 1)))
        return;
    if (mu_left[ch] && --mu_left[ch])
        return;
    for (;;) {
        w = *mu_p[ch];
        j = (uint8_t)(w >> 11);
        if (j)
            break;
        code = (uint8_t)w & 3;          /* a control */
        if (!code) {                    /* return */
            mu_p[ch] = mu_r[ch];
            continue;
        }
        if (code == 3) {                /* the end: the channel quiet */
            mu_on &= (uint8_t)~(ch + 1);
            quiet(ch);
            return;
        }
        if (code == 1)                  /* a call: back to the word after its address */
            mu_r[ch] = mu_p[ch] + 2;
        mu_p[ch] = (const uint16_t *)mu_p[ch][1];
    }
    mu_left[ch] = j;
    ++mu_p[ch];
    code = (uint8_t)w & 0x7F;
    v = (uint8_t)(w >> 7) & 15;
    if (code >= NOISE0)
        eng_voice(ch, mus_nz[code - NOISE0], vol_lin[v], 1);
    else if (code)
        eng_voice(ch, mus_per[code], vol_lin[v], 0);
    else
        quiet(ch);
}

/* a jiffy of everything */
static void jiffy(void)
{
    uint8_t k;
    for (k = 0; k < 2; ++k) {
        if (!fx_p[k])
            continue;
        if (fx_e[k]) {                  /* the envelope falls */
            fx_env[k] = fx_env[k] > fx_dv[k] ? fx_env[k] - fx_dv[k] : 0;
            eng_level(fx_vo[k], vol_lin[(uint8_t)(fx_env[k] >> 8)]);
        }
        if (!--fx_left[k])
            fx_next(k);
    }
    mu_step(0);
    mu_step(1);
    if (bl_left && !--bl_left)
        quiet(1);
}

/* once a pass (and from vbi_now): a jiffy of the sound for each that passed
 * (the steps are 60ths: at 50 a second, one more every 5) */
static uint8_t pal5;

void snd_tick(void)
{
    uint8_t n = (uint8_t)((uint8_t)frame_count - last);
    last = (uint8_t)frame_count;
    if (n > 8)
        n = 8;
    if (plat_hz() == 50) {
        pal5 += n;
        while (pal5 >= 5) {
            pal5 -= 5;
            ++n;
        }
    }
    if (!on || muted)
        return;
    while (n--)
        jiffy();
}

/* the wait for the end of jiffy seen (frame_wait), sounding meanwhile */
void snd_idle(uint8_t seen)
{
#ifdef COCO3
    while ((uint8_t)frame_count == seen)
        ;
#else
    eng_play(seen);
#endif
}

/* the script's clock (plat.h vbi_count): the player waits on it */
uint8_t vbi_now(void)
{
    snd_tick();
#ifndef COCO3
    eng_play((uint8_t)frame_count);     /* (the CoCo 1/2's sound: the wait for a jiffy) */
#endif
    return vbi_ticks;
}

/* the joystick's reads use the DAC and the mux (input.c) */
void snd_gate(uint8_t go)
{
    eng_gate(go && on && !muted);
}

void snd_mute_toggle(void)
{
    muted ^= 1;
    snd_stop();
    snd_gate(1);
}

void snd_init(void)
{
    eng_init();
    on = 1;
    last = (uint8_t)frame_count;
    snd_gate(1);
}
