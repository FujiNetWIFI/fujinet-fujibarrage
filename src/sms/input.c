/*
 * input.c -- the Master System's control pad 1 (port DCh), its Pause button
 * (the Z80's NMI: z88dk's crt0 flips pause_flag) and the SMS1's Reset (port
 * DDh); FujiBlast's src/sms/input.c with FujiBarrage's keys:
 *
 *   pad             aim (angle, power) / choose
 *   1               the button (fire, buy, take)
 *   2, tapped       TAB (9): the next weapon, the settings' next page
 *   2 held + 1      I on the field (the items line), S elsewhere (the seats
 *                   and the settings)
 *   Pause           KEY_RETURN (done: the shop, the defence window, a name)
 *   2 held + Pause  KEY_ESC (back, leave, the lobby)
 *   Reset           KEY_ESC
 *
 * 2 counts only when let go without 1 or Pause meanwhile, and 1 pressed
 * while 2 is held is no press of the button.
 */
#include <stdint.h>
#include "input.h"

__sfr __at 0xDC PAD_A;          /* 0 = down: up down left right 1 2 */
__sfr __at 0xDD PAD_B;          /* bit 4: Reset (an SMS1's), 0 = down */

#define J_1     0x10
#define J_2     0x20
#define J_RESET 0x10

extern volatile uint8_t pause_flag;     /* crt0 */

uint8_t inp_dir, inp_btn, ev_btn, ev_key;
uint8_t inp_hook, inp_hook_dir, inp_hook_btn, inp_hook_key;
uint8_t inp_field;              /* the field is up: 2+1 is I, else S (field.asm, scr.asm) */
static uint8_t prev_j, prev_r, pause_seen, btn_prev, btn_latch, key_latch;
static uint8_t two_used;        /* 2 held: something else came (1, Pause) */

/* Port DCh's bits 0-3 (up, down, left, right) as IN_ bits (a table: zsdcc
 * -SO3 once dropped the stores of "if (j & ..) dir |= .." in a row). */
static const uint8_t dir_of[16] = {
    0, IN_UP, IN_DOWN, IN_UP | IN_DOWN,
    IN_LEFT, IN_LEFT | IN_UP, IN_LEFT | IN_DOWN, IN_LEFT | IN_UP | IN_DOWN,
    IN_RIGHT, IN_RIGHT | IN_UP, IN_RIGHT | IN_DOWN, IN_RIGHT | IN_UP | IN_DOWN,
    IN_RIGHT | IN_LEFT, IN_RIGHT | IN_LEFT | IN_UP, IN_RIGHT | IN_LEFT | IN_DOWN,
    IN_RIGHT | IN_LEFT | IN_UP | IN_DOWN,
};

void input_init(void)
{
    prev_j = ~PAD_A;
    prev_r = ~PAD_B & J_RESET;
    pause_seen = pause_flag;
}

void input_poll(void)
{
    uint8_t j = ~PAD_A, r = ~PAD_B & J_RESET, p = pause_flag, dir, btn, up, down;

    up = prev_j & ~j;                   /* let go */
    down = j & ~prev_j;                 /* pressed */
    prev_j = j;
    dir = dir_of[j & 15];
    btn = (j & J_1) != 0;
    if (j & J_2) {
        if (down & J_1) {               /* 2 held + 1 */
            key_latch = inp_field ? 'I' : 'S';
            two_used = 1;
        }
        if (btn)
            two_used = 1;
        btn = 0;                        /* (no press of the button meanwhile) */
    }
    if (up & J_2) {
        if (!two_used)
            key_latch = 9;              /* TAB */
        two_used = 0;
    }
    if (p != pause_seen) {
        pause_seen = p;
        if (j & J_2) {
            key_latch = KEY_ESC;
            two_used = 1;
        } else {
            key_latch = KEY_RETURN;
        }
    }
    if (r && !prev_r)
        key_latch = KEY_ESC;
    prev_r = r;

    if (inp_hook) {
        dir = inp_hook_dir;
        btn = inp_hook_btn;
        if (inp_hook_key) {
            key_latch = inp_hook_key;
            inp_hook_key = 0;
        }
    }
    inp_dir = dir;
    inp_btn = btn;
    if (btn && !btn_prev)
        btn_latch = 1;
    btn_prev = btn;
}

void input_take(void)
{
    ev_btn = btn_latch;
    btn_latch = 0;
    ev_key = key_latch;
    key_latch = KEY_NONE;
}
