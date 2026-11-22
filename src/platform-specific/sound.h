/* sound.h -- what the game asks for; each platform makes its own sound of
 * it (the server's OP_SOUND ids, proto.h SND_*, plus a few of the client's
 * own). On the Atari: two voices while the link runs, all four while it is
 * held for a script. */
#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

enum { UI_TICK = 0x40, UI_SELECT, UI_GO, UI_ERROR, UI_TURN };

void snd_init(void);
void snd_cue(uint8_t id, uint8_t size);   /* SND_* (size 0-3) or UI_* */
void snd_full(uint8_t on);                /* the link is held: four voices */
void snd_whistle(uint8_t on, uint8_t pitch);  /* a shell coming down */
void snd_blip(uint8_t pitch);             /* a tank talking, a letter */
void snd_stop(void);
uint8_t snd_busy(void);                  /* a held-link sound still playing */
void snd_tune(const uint8_t *ch1, const uint8_t *ch2);  /* a tune on voices 1 and 2 */
extern const uint8_t t_title1[], t_title2[];            /* (MENU.COM's) */
extern const uint8_t t_win1[], t_win2[], t_over1[], t_over2[], t_shop1[], t_shop2[];

#endif
