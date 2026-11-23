/*
 * input.c -- the Adam's keyboard and controller 1 (FujiBlast's, from
 * M.U.L.E.'s).
 *
 * The keyboard is an AdamNet device that sends characters, not keys held:
 * an arrow counts as held for a while after each one it sends (its repeat
 * keeps it held). The controller is read from its port (eoscall.asm: EOS's
 * POLLER leaves the stick dead): the stick, either fire button, and the
 * keypad, whose keys stand for the keyboard's where there is no keyboard
 * to hand:
 *
 *   #  RETURN      *  ESCAPE       0  TAB (the next weapon, the next page)
 *   1  I (items)   2  S (the waiting room's settings)
 *
 * The SmartKeys say the same: I is I, II is TAB, III is S.
 */
#include "input.h"
#include "adam.h"

uint8_t inp_dir, inp_btn, ev_btn, ev_key;
uint8_t inp_hook, inp_hook_dir, inp_hook_btn, inp_hook_key;

static uint8_t btn_prev, btn_latch, key_latch, pad_prev = 0x0F;
static uint8_t held_dir, held_t, space_t;

#define K_UP     0xA0
#define K_RIGHT  0xA1
#define K_DOWN   0xA2
#define K_LEFT   0xA3
#define K_SK1    0x81           /* SmartKey I */
#define K_SK2    0x82
#define K_SK3    0x83
#define HOLD     20             /* frames an arrow stays held: its first
                                 * repeat comes in about a third of a second */

void input_init(void)
{
    eos_kbd_start();
}

static void latch(uint8_t c)
{
    if (key_latch == KEY_NONE)
        key_latch = c;
}

/* A character from the keyboard, if one came. */
static void typed(void)
{
    uint8_t c = eos_kbd_end();
    if (c == 0)
        return;                 /* still waiting */
    eos_kbd_start();
    if (c == 1)
        return;                 /* it failed: asked again */
    switch (c) {
    case K_UP:    held_dir = IN_UP;    held_t = HOLD; return;
    case K_RIGHT: held_dir = IN_RIGHT; held_t = HOLD; return;
    case K_DOWN:  held_dir = IN_DOWN;  held_t = HOLD; return;
    case K_LEFT:  held_dir = IN_LEFT;  held_t = HOLD; return;
    case ' ':
        btn_latch = 1;
        space_t = 6;
        return;
    case K_SK1:   latch('I');      return;
    case K_SK2:   latch(9);        return;
    case K_SK3:   latch('S');      return;
    }
    if (c >= 'a' && c <= 'z')
        c -= 32;
    if (c == 13 || c == 27 || c == 8 || c == 9 || (c > 32 && c < 96))
        latch(c);
}

/* the keypad's raw code (09h *, 0Ah 0, 0Fh none) as a key */
static uint8_t pad_key(uint8_t k)
{
    switch (k) {
    case 0x06: return KEY_RETURN;   /* # */
    case 0x09: return KEY_ESC;      /* * */
    case 0x0A: return 9;            /* 0 */
    case 0x0D: return 'I';          /* 1 */
    case 0x07: return 'S';          /* 2 */
    }
    return KEY_NONE;
}

void input_poll(void)
{
    uint8_t dir = 0, btn = 0, j, k;

    typed();
    if (held_t) {
        --held_t;
        dir = held_dir;
    }
    if (space_t) {
        --space_t;
        btn = 1;
    }
    joy_read();
    j = joy_ctl[0];             /* 1 north, 2 east, 4 south, 8 west */
    if (j & 1) dir |= IN_UP;
    if (j & 2) dir |= IN_RIGHT;
    if (j & 4) dir |= IN_DOWN;
    if (j & 8) dir |= IN_LEFT;
    if (joy_ctl[1] || joy_ctl[2])   /* left, right fire */
        btn = 1;
    k = joy_ctl[3];
    if (k != pad_prev) {
        pad_prev = k;
        if ((k = pad_key(k)) != KEY_NONE)
            latch(k);
    }

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
