/* field.h -- the playfield: a bitmap at the client's own resolution
 * (FIELD_W x FIELD_H), four pixel values, the tanks drawn into it; and the
 * status rows above it. play.c draws the server's TERRAIN and SCRIPT
 * through these; each platform implements them (src/atari/field.c). */
#ifndef FIELD_H_
#define FIELD_H_

#include <stdint.h>

#define PX_TANK 0
#define PX_SKY  1
#define PX_DIRT 2
#define PX_FX   3

#ifdef __ATARI__
#define FIELD_W 160
#define FIELD_H 168
#define TANK_W  7
#define TANK_H  5
#define PAR     109             /* pixel aspect, 64ths */
#ifndef SCRIPT_BUF                  /* (a rig's build may make it smaller) */
#define SCRIPT_BUF 1536
#endif
#define BANDS   4
#endif

void field_init(void);                      /* at start-up: all sky */
void field_loading(void);                   /* a whole field is coming */
void field_open(uint8_t sky, uint8_t pal);  /* the field screen on */
void field_column(uint16_t x, const uint8_t *entry);
void field_plot(int16_t x, int16_t y, uint8_t v);
uint8_t field_get(int16_t x, int16_t y);
void field_swap(int16_t x, int16_t y, uint8_t from, uint8_t to);
void field_span(int16_t y, int16_t x0, int16_t x1, uint8_t v);
void field_tank(uint8_t seat, uint8_t draw);    /* tk[seat] drawn (1) or erased (0) */
void field_colours(void);                       /* the tanks moved: their colours follow */
void field_balloon(uint8_t seat, const char *s, uint8_t n);  /* n 0: take it away */
/* sparks from a tank: seat in the low byte, the kind in the high (0-7 a
 * death's DEATH_* effect, 8 + a shield's SH_* event). Only ever on sky, and
 * gone by the segment's end. */
void __fastcall__ field_sparks(uint16_t seat_kind);
void field_status(uint8_t row, uint8_t col, const char *s);
void field_status_clear(void);
void field_flash(uint8_t frames);
void field_fire(uint8_t on);                    /* the fire colours cycle */
/* an ellipse: mode 0 all of it v, 1 its sky v, 2 its colour 3 sky */
void field_ellipse(int16_t x, int16_t y, uint8_t rx, uint8_t ry, uint8_t v, uint8_t mode);

#ifndef __CC65__
#define __fastcall__
#endif
/* the script's tracks (OP_ARC): sixteen moving heads, drawn as they go */
void trk_reset(void);
void __fastcall__ trk_load(const uint8_t *args);    /* an OP_ARC's arguments */
void trk_step(void);                                /* all of them, a step */
void __fastcall__ trk_stop(uint8_t slot);
uint8_t trk_fall(void);                             /* the lowest falling one's line | 1, or 0 */

#endif
