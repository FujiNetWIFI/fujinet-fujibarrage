/* cocoplat.h -- what the CoCo's modules share: the CoCo 1/2 and, with COCO3,
 * the CoCo 3. (Not "coco.h": CMOC's own <coco.h> would win.) */
#ifndef COCOPLAT_H
#define COCOPLAT_H

#include <stdint.h>

/* the direct page the player and the field engines keep their variables
 * in ($0E00-$0EFF: src/6809/dp.inc); the link's ring is the page after */
#define DP_PAGE   0x0E
#define NST_RING  ((uint8_t *)0x0F00)

/* bank.c */
const char *bank_init(void);        /* all RAM, the high core (and art) loaded; 0 or a message */
void boot_text(const char *s);      /* a message on the text screen, before the graphics */

/* plat.c */
void plat_irq_init(void);           /* the IRQ: the field sync */
extern volatile uint8_t vbi_ticks;  /* +1 each field sync (the script's clock) */
extern uint8_t kb_rows[8], kb_prev[8];  /* the keyboard matrix, by column (1 = down) */
extern uint8_t kb_q[8], kb_head, kb_tail;
extern uint8_t coco_swap;           /* CoCo 1/2: the artifact phase swapped; CoCo 3: composite */
extern uint16_t coco_asked;         /* COCO_ASKED: the question was answered (src/coco/fixed.asm) */
#define COCO_ASKED 0xC33C

/* the machine's display (src/coco2/video.c, src/coco3/video.c) */
void video_init(void);
void video_swap(void);              /* CLEAR: the artifact phase, or RGB <-> composite */
void video_question(void);          /* at start-up: is it blue? / which monitor? */
extern void (*video_hook)(void);    /* video_swap: the field's own (src/coco/field.c) */
void video_flash(uint8_t frames);   /* a flash (and on the CoCo 3 a shake) */
void video_frame(void);             /* once a drawn pass (frame_wait): the flash's count */
extern uint8_t scr_seq, field_up;   /* text screens begun; the field is showing (video.c) */
#ifdef COCO3
extern uint8_t fire_on, fire_cyc[8];    /* the IRQ cycles colour 5 through these */
#endif

/* the sound (src/coco2/sound2.c, src/coco3/sound3.c) */
void snd_tick(void);                /* once a pass: what the sound does between frames */
void snd_gate(uint8_t on);          /* the DAC and the mux for sound (0: for the joystick) */
void snd_idle(uint8_t seen);        /* wait for the end of jiffy seen, sounding meanwhile */
void snd_mute_toggle(void);

#ifdef COCO3
#define MMU6 (*(volatile uint8_t *)0xFFA6)
#define MMU7 (*(volatile uint8_t *)0xFFA7)
#define ROM_BLOCK 0x3E              /* slot 6's own block: the Disk ROM's copy (DriveWire) */
#define ART_BLOCK 0x37              /* slot 7: the art, palettes, patterns, sound tables */
#endif

#endif
