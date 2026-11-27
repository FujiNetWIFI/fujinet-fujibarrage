/*
 * a78.h -- FujiBarrage on the Atari 7800: the memory map and what the
 * display's C and asm share (a78.inc is the asm's copy; maria.s).
 */
#ifndef A78_H
#define A78_H

#include <stdint.h>

#define MSTAT    (*(volatile uint8_t *)0x28)
#define RANDOM   (*(volatile uint8_t *)0x045A)

#define FLDPG    0x40           /* the field's first page (a78.inc) */
#define FONTPG   0x80
#define TXTPG    0x58
#define TXTLO    0x78
#define NZONES   21
#define NTROWS   24
#define NHUD     3
#define NSLOTZ   24
#define SLOTSZ   64

/* the text row r's 40 bytes */
#define TXT_ROW(r) ((uint8_t *)(((uint16_t)(TXTPG + (r) / 3) << 8) + TXTLO + 40 * ((r) % 3)))

/* the screens (maria.s) */
#define M_BLANK  0
#define M_TEXT   1
#define M_FIELD  2

/* maria.s */
void m_init(void);                      /* tvdetect, the DLLs, the clock on */
void m_show(uint8_t kind);              /* that screen from the next frame (and waits for it) */
void m_wait(void);                      /* a whole frame: the next frame DLI */
uint8_t *m_back(uint8_t z);             /* zone z's slot not shown */
void m_flip(uint8_t z);                 /* zone z's back slot shown from the next frame */
void m_put(uint8_t z);                  /* m_nd (m_ndlen bytes) zone z's list, if it isn't already */
extern uint8_t m_nd[SLOTSZ], m_ndlen;
extern uint8_t m_ntsc;                  /* 1 NTSC (60 Hz), 0 PAL */
extern uint8_t m_cur;                   /* the screen shown */
extern volatile uint8_t m_busy;         /* 1: a back slot is being written (no flips) */
extern uint8_t r_bg[NTROWS + NHUD], r_ink[NTROWS + NHUD], r_lab[NTROWS + NHUD], r_nam[NTROWS + NHUD];
extern uint8_t z_sky[NZONES], z_dirt[NZONES];      /* a field zone's sky and dirt as shown (a flash's white) */
extern uint8_t zb_sky[NZONES], zb_dirt[NZONES];    /* the zone's own (fieldm.c sets both) */
extern uint8_t m_seat[15];              /* P3-P7's colours: the underlays' (and names') */
extern volatile uint8_t m_seatnew;      /* 1: m_seat changed (the frame's DLI copies it) */
extern uint8_t dsp_flash, dsp_shake, fx_col, fx_cycle;

/* a colour as this console shows it: PAL's hues one on */
uint8_t hue(uint8_t c);

#endif
