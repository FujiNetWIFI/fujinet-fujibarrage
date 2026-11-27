/*
 * input.c -- the NES's controller 1 ($4016), with FujiBarrage's keys (the
 * SMS's src/sms/input.c on the NES's buttons):
 *
 *   pad             aim (angle, power) / choose
 *   A               the button (fire, buy, take)
 *   B, tapped       TAB (9): the next weapon, the settings' next page
 *   B held + A      I on the field (the items line), S elsewhere (the seats
 *                   and the settings)
 *   START           KEY_RETURN (done: the shop, the defence window, a name)
 *   SELECT          KEY_ESC (back, leave, the lobby)
 *
 * B counts only when let go without A meanwhile, and A pressed while B is
 * held is no press of the button.
 */
#include <stdint.h>
#include "input.h"

#define JOY1 (*(volatile uint8_t *)0x4016)

#define J_A      0x80
#define J_B      0x40
#define J_SELECT 0x20
#define J_START  0x10
#define J_UP     0x08
#define J_DOWN   0x04
#define J_LEFT   0x02
#define J_RIGHT  0x01

uint8_t inp_dir, inp_btn, ev_btn, ev_key;
uint8_t inp_hook, inp_hook_dir, inp_hook_btn, inp_hook_key;
uint8_t inp_field;              /* the field is up: B+A is I, else S */
static uint8_t prev_j, btn_prev, btn_latch, key_latch;
static uint8_t b_used;          /* B held: A came meanwhile */

/* the pad's 8 buttons, A first (read twice: a DMC fetch can eat a bit on
 * a 2A03; until two reads agree) */
static uint8_t pad_read(void)
{
    uint8_t i, a, b;
    do {
        JOY1 = 1;
        JOY1 = 0;
        a = 0;
        for (i = 0; i < 8; ++i)
            a = (uint8_t)(a << 1 | (JOY1 & 1));
        JOY1 = 1;
        JOY1 = 0;
        b = 0;
        for (i = 0; i < 8; ++i)
            b = (uint8_t)(b << 1 | (JOY1 & 1));
    } while (a != b);
    return a;
}

void input_init(void)
{
    prev_j = pad_read();
}

void input_poll(void)
{
    uint8_t j = pad_read(), dir = 0, btn, up, down;

    up = prev_j & ~j;                   /* let go */
    down = j & ~prev_j;                 /* pressed */
    prev_j = j;
    if (j & J_UP)
        dir |= IN_UP;
    if (j & J_DOWN)
        dir |= IN_DOWN;
    if (j & J_LEFT)
        dir |= IN_LEFT;
    if (j & J_RIGHT)
        dir |= IN_RIGHT;
    btn = (j & J_A) != 0;
    if (j & J_B) {
        if (down & J_A) {               /* B held + A */
            key_latch = inp_field ? 'I' : 'S';
            b_used = 1;
        }
        if (btn)
            b_used = 1;
        btn = 0;                        /* (no press of the button meanwhile) */
    }
    if (up & J_B) {
        if (!b_used)
            key_latch = 9;              /* TAB */
        b_used = 0;
    }
    if (down & J_START)
        key_latch = KEY_RETURN;
    if (down & J_SELECT)
        key_latch = KEY_ESC;

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
