/*
 * input.c -- keyboard and joystick on the Apple II (FujiBlast's, from
 * M.U.L.E.).
 *
 * The arrows aim (left and right the angle, up and down the power); space,
 * Open-Apple or Solid-Apple (a joystick's buttons) fire; every key is a key
 * too (I the items line, W or TAB the next weapon, RETURN done, ESC back;
 * the left arrow and DELETE rub out a name's letter). The //e's "a key is
 * down" bit ($C010 bit 7) holds a direction until the keys are let go;
 * emulators that never set it (AppleWin's libretro core) send the host's
 * key repeats instead, so a direction also lasts KEY_HOLD polls after its
 * last press (a tap is still one step). frame_tick latches the buttons,
 * so a press during a long draw counts.
 * The joystick (if one was there, centred, at start-up) is read every
 * other pass: both axes at once (about 3K cycles), against that centre.
 */
#ifdef __APPLE2__

#include <stdint.h>
#include "input.h"
#include "plat.h"

#define KBD     (*(volatile uint8_t *)0xC000)
#define KBDSTRB (*(volatile uint8_t *)0xC010)
#define BUTN0   (*(volatile uint8_t *)0xC061)
#define BUTN1   (*(volatile uint8_t *)0xC062)
#define CLEAR_STROBE() __asm__("bit $C010")
#define KEY_HOLD 2

/* lowlevel.s */
void joy_read(void);
extern uint8_t joy_x, joy_y, btn_latch;
#pragma zpsym("joy_x")
#pragma zpsym("joy_y")
#pragma zpsym("btn_latch")

uint8_t inp_dir, inp_btn, ev_btn, ev_key;
uint8_t inp_hook, inp_hook_dir, inp_hook_btn, inp_hook_key;

static uint8_t key_dir, key_btn, btn_prev, btn_ev, key_latch, key_hold;
static uint8_t joy_on, jx_lo, jx_hi, jy_lo, jy_hi, joy_dir, joy_skip;

void input_init(void)
{
    joy_read();
    joy_on = joy_x > 8 && joy_x < 200 && joy_y > 8 && joy_y < 200;
    jx_lo = joy_x >> 1;
    jx_hi = joy_x + (joy_x >> 1);
    jy_lo = joy_y >> 1;
    jy_hi = joy_y + (joy_y >> 1);
    CLEAR_STROBE();
    btn_latch = 0;
}

void input_poll(void)
{
    uint8_t k, btn, dir;
    frame_tick();

    if (KBD & 0x80) {
        k = KBD & 0x7F;
        CLEAR_STROBE();
        if (k >= 'a' && k <= 'z')
            k -= 32;
        switch (k) {
        case 8:  key_dir = IN_LEFT; break;
        case 21: key_dir = IN_RIGHT; break;
        case 11: key_dir = IN_UP; break;
        case 10: key_dir = IN_DOWN; break;
        case ' ': key_btn = 1; break;
        default: key_dir = 0;
        }
        key_latch = k == 127 ? KEY_BACK : k;
        key_hold = KEY_HOLD;
    } else if (!(KBDSTRB & 0x80)) {
        if (key_hold)
            --key_hold;         /* (no "a key is down": a repeat may come) */
        else {
            key_dir = 0;        /* every key let go */
            key_btn = 0;
        }
    }
    dir = key_dir;
    btn = key_btn || ((BUTN0 | BUTN1) & 0x80);

    if (joy_on) {
        if (joy_skip ^= 1) {
            joy_read();
            joy_dir = 0;
            if (joy_x < jx_lo)
                joy_dir |= IN_LEFT;
            else if (joy_x > jx_hi)
                joy_dir |= IN_RIGHT;
            if (joy_y < jy_lo)
                joy_dir |= IN_UP;
            else if (joy_y > jy_hi)
                joy_dir |= IN_DOWN;
        }
        dir |= joy_dir;
    }

    if (inp_hook) {
        dir = inp_hook_dir;
        btn = inp_hook_btn;
        btn_latch = 0;
        if (inp_hook_key) {
            key_latch = inp_hook_key;
            inp_hook_key = 0;
        }
    }
    if ((btn || (btn_latch & 0x80)) && !btn_prev)
        btn_ev = 1;
    btn_latch = 0;
    btn_prev = btn;
    inp_dir = dir;
    inp_btn = btn;
}

void input_take(void)
{
    ev_btn = btn_ev;
    btn_ev = 0;
    ev_key = key_latch;
    key_latch = KEY_NONE;
    if (ev_key == ' ')
        ev_key = KEY_NONE;      /* the space bar is fire, not a key */
}

#endif
