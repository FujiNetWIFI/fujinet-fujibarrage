/* sms.h -- the Sega Master System as FujiBarrage uses it (src/sms/).
 *
 * The VDP's Mode 4: a name table of 32x28 words at 3800h (rows 0-20 the
 * field, 21-23 the HUD, 24-27 under the screen for the shake's wrap), the
 * sprite table at 3F00h, tiles 0-447 below 3800h shared by everything
 * (src/sms/tcache.asm hands them out). The cart's 32K of RAM is paged into
 * slot 2 (8000h-AFFFh: B000h up is the FujiNet's mailbox): half 0 holds the
 * field's store and the script, half 1 the tile cache's tables
 * (src/sms/map.asm).
 */
#ifndef SMS_H
#define SMS_H

#include <stdint.h>

__sfr __at 0xBE VDP_DATA;
__sfr __at 0xBF VDP_CTRL;
__sfr __at 0x7E VDP_VCOUNT;
__sfr __at 0x7F PSG_PORT;

/* VRAM */
#define VR_NAMES  0x3800
#define VR_SAT    0x3F00
#define N_TILES   448

/* VDP register 1: display on, frame interrupt on (bit 7 set, as the crt's) */
#define R1_ON     0xE0
#define R1_OFF    0xA0          /* (blank; the interrupt still on) */

extern uint8_t vdp_r1;          /* register 1 as last set */
extern uint8_t vdp_seen;        /* the status bits the frame interrupt saw */
extern uint8_t vbi_ticks;       /* +1 each display frame */
extern uint8_t sms_pal;         /* a 50 Hz console */
extern uint8_t flash_n, shake_n;

void vdp_reg(uint8_t r, uint8_t v);

/* the VRAM writers (vdp.asm): va_addr the address, va_src the bytes,
 * va_len their count; va_val va_fill's byte. Safe with the display on. */
extern uint16_t va_addr, va_len;
extern const uint8_t *va_src;
extern uint8_t va_val;
void va_write(void);
void va_fill(void);
void va_cram(void);             /* va_addr = the colour (0-31) */
void va_read(void);             /* va_len bytes from va_addr into va_src */

/* slot 2 (map.asm): the cart's RAM, a half, or a ROM bank */
void sms_ram0(void);
void sms_ram1(void);
void sms_rom(uint8_t bank) __z88dk_fastcall;
uint8_t sms_ram_ok(void);       /* the cart's 32K of RAM is there (the test card) */

/* the frame (plat.c): the field's cells drawn while waiting, a frame's work */
void sms_idle(void);
void sms_frame(void);

#endif
