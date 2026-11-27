/*
 * sound.c -- the NES's sound (sound.h). M1: quiet (M5: the APU).
 */
#include <stdint.h>
#include "sound.h"

const uint8_t t_title1[] = { 0 }, t_title2[] = { 0 };
const uint8_t t_win1[] = { 0 }, t_win2[] = { 0 }, t_over1[] = { 0 }, t_over2[] = { 0 };
const uint8_t t_shop1[] = { 0 }, t_shop2[] = { 0 };

void snd_init(void) {}
void snd_cue(uint8_t id, uint8_t size) { (void)id; (void)size; }
void snd_full(uint8_t on) { (void)on; }
void snd_whistle(uint8_t on, uint8_t pitch) { (void)on; (void)pitch; }
void snd_blip(uint8_t pitch) { (void)pitch; }
void snd_stop(void) {}
uint8_t snd_busy(void) { return 0; }
void snd_tune(const uint8_t *ch1, const uint8_t *ch2) { (void)ch1; (void)ch2; }
