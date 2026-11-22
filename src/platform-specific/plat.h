/* plat.h -- start-up, the 60 Hz jiffy clock, randomness and quitting. */
#ifndef PLAT_H
#define PLAT_H

#include <stdint.h>

void plat_init(void);
void plat_quit(void);
void plat_reboot(void);         /* boot the disk again */

/* Jiffies since boot (60 a second; a 50 Hz machine counts every fifth twice). */
extern uint16_t frame_count;

/* Wait for the next frame. */
void frame_wait(void);

uint8_t plat_rand(void);
uint8_t plat_hz(void);          /* 50 or 60: the display's frames a second */
extern volatile uint8_t vbi_count;  /* +1 each display frame (the script's clock) */

#endif
