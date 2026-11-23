/*
 * atari.h -- where the Atari client keeps the chips and the graphics
 * (barrage.cfg; hw.inc is the assembly's copy).
 */
#ifndef ATARI_H
#define ATARI_H

#include <stdint.h>

#ifdef __ATARI5200__
#define GTIA_BASE   0xC000
#define POKEY_BASE  0xE800
#define DLICODE_ADDR 0x2B00
#define PMG_ADDR    0x0800
#define FIELD_ADDR  0x1010
#define TEXT_ADDR   0x1010     /* a text screen: in the field's memory */
#define STATUS_ADDR 0x2A50
#define CHB_ROM_PG  0xF8        /* the BIOS's font */
#define RTCLOK_ADDR 0x02        /* the crt0's clock, its low byte */
#else
#define GTIA_BASE   0xD000
#define POKEY_BASE  0xD200
#define DLICODE_ADDR 0x9000
#define PMG_ADDR    0x9800
#define FIELD_ADDR  0xA010
#define TEXT_ADDR   0xA010     /* a text screen: in the field's memory */
#define STATUS_ADDR 0xBA50
#define CHB_ROM_PG  0xE0        /* the OS font */
#define RTCLOK_ADDR 0x14
#endif
#define F_LINES     168
#define F_COLS      160
#define F_TOP       56          /* the field's first scanline, a player's line index */

#ifdef __ATARI5200__
#define GTIA_PAL    0x0E        /* (NTSC only) */
#else
#define GTIA_PAL    (*(volatile uint8_t *)(GTIA_BASE + 0x14))
#endif
#define GTIA_GRACTL (*(volatile uint8_t *)(GTIA_BASE + 0x1D))
#define POKEY_RANDOM (*(volatile uint8_t *)(POKEY_BASE + 0x0A))

#endif
