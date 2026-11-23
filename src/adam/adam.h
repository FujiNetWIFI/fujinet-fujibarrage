/*
 * adam.h -- the Coleco Adam: the TMS9918A's ports, the NMI that counts
 * frames, and the routines written in assembler (vdp.asm, nmi.asm,
 * eoscall.asm, chain.asm). From FujiBlast's src/adam.
 *
 * The VDP's interrupt is the Z80's NMI (nmi.asm) and can't be masked. It
 * reads the VDP's status, which also resets the address latch, so it must
 * not land between the two bytes of an address or inside a burst of data:
 * while the game is at the VDP (vdp_busy) it leaves the status alone and
 * says so in vdp_pend, and the unlock takes the frame then.
 */
#ifndef ADAM_H
#define ADAM_H

#include <stdint.h>

__sfr __at 0xBE VDP_DATA;
__sfr __at 0xBF VDP_CTRL;
extern volatile uint8_t vdp_busy, vdp_pend;
void vdp_catch_up(void);
#define VDP_LOCK()   (vdp_busy = 1)
#define VDP_UNLOCK() do { vdp_busy = 0; if (vdp_pend) vdp_catch_up(); } while (0)

/* nmi.asm: display frames (the script's clock), 60ths, the effects the
 * NMI writes to the VDP between frames (the flash: R1 blanked and R7
 * white for flash_n frames; the shake: R2 through shake_seq) */
extern volatile uint8_t vbi_ticks;
extern volatile uint8_t flash_n, shake_n;
extern uint8_t nmi_pal;         /* a 50 Hz machine: every fifth frame counts twice */

/* eoscall.asm */
extern uint8_t joy_ctl[4];      /* controller 1: stick, left fire, right fire, keypad code */
extern uint8_t eos_dev;
extern uint16_t eos_blk;
extern uint8_t *eos_dst;
void eos_kbd_start(void);       /* ask the keyboard for a character */
uint8_t eos_kbd_end(void);      /* 0 none yet, 1 failed (ask again), else the character */
void joy_read(void);            /* controller 1 into joy_ctl, read from its port */
uint8_t eos_read_blk(void);     /* block eos_blk of eos_dev to eos_dst: 0 or an error */

/* chain.asm: load the other program from the boot medium and run it */
void chain_run(uint8_t which) __z88dk_fastcall;  /* CH_GAME or CH_MENU: back only if not there */
enum { CH_GAME, CH_MENU };
void art_load(void);            /* the font (its own block) into the VRAM */

/* the sound's frame (nmi.asm calls it; src/sound/) */
void snd_nmi(void);

uint8_t plat_net_probe(void);

#endif
