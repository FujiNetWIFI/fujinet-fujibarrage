/* ply.h -- the script player each platform supplies (src/6502/ply.s,
 * src/6809/ply.asm): it plays the timeline; the ops about tanks, talk,
 * damage and sound come back to play.c (c_op), their arguments read at
 * ply_pc. */
#ifndef PLY_H
#define PLY_H

#include <stdint.h>

extern uint16_t ply_pc, ply_wait;
#ifdef __CC65__
#pragma zpsym ("ply_pc")
#endif
extern uint8_t ply_done;
void ply_reset(void);
void ply_step(void);
void ply_draw(void);
void ply_end(void);
void ops_beams_off(void);

#endif
