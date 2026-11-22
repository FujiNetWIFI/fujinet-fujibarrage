/*
 * test.c -- offline test scenes for the emulator rigs (tools/atarisim): the
 * screens on canned states and a real shot's bytes (gen/fixture.h, from
 * the server's refview -fixture), no network: a TEST=1 build (make atari
 * TEST=1) only. At start-up we mark
 * test_hook[1] $C3 and wait a third of a second for a rig to poke $5A into
 * test_hook[0], the scene into [2] and its argument into [3]:
 *   1  the field: the fixture's ground, ten tanks, its shot played every
 *      5 seconds with the held link's sound ([3] bit 0: stream sound only)
 *   2  the waiting room       3  a round's end       4  the game's end
 *   5  the field, ten tanks, no shot: aim with the stick (seat 0's turn)
 *   6  a tune (ARG 0 a round's end, 1 the game's end, 2 the shop)
 */
#ifndef TEST_SCENES
#include "game.h"
uint8_t test_hook[4];
void test_check(void)
{
}
#else
#include <string.h>
#include "game.h"
#include "scr.h"
#include "input.h"
#include "plat.h"
#include "sound.h"
#include "field.h"
#include "fixture.h"

/* a fixture bigger than the script buffer can't be played: make it with
 * refview -buf SCRIPT_BUF */
typedef char fixture_fits[sizeof(fix_script) <= SCRIPT_BUF ? 1 : -1];

uint8_t test_hook[4];

static const char names[SEATS][NAME_LEN] = {
    "ANN", "BOB", "CAT", "DOT", "EVE", "FAY", "GUS", "HAL", "IVY", "JOE" };

static void canned(void)
{
    uint8_t s;
    memset(st, 0, sizeof(st));
    st[OFF_PHASE] = PH_AIM;
    st[OFF_ROUND] = 2;
    st[OFF_ROUNDS] = 10;
    st[OFF_HOST] = 0;
    st[OFF_TURN] = 3;
    st[OFF_TIMER] = 27;
    st[OFF_TANKS] = 10;
    st[OFF_WIND] = 0xF4;
    st[OFF_WIND + 1] = 0xFF;
    st[OFF_SKY] = FIX_SKY;
    st[OFF_PALETTE] = FIX_PAL;
    st[OFF_RESULT] = 3;
    st[OFF_MY_SEAT] = 0;
    for (s = 0; s < SEATS; ++s) {
        memcpy((char *)st + OFF_SEATS + seat_off[s] + P_NAME, names[s], NAME_LEN);
        PL(s, P_KIND) = s < 3 ? KIND_HUMAN : KIND_BOT;
        PL(s, P_TIER) = s % N_TIERS;
        PL(s, P_ENERGY) = 100 - s * 7;
        PL(s, P_FLAGS) = PF_ALIVE | PF_CONN;
        PL16(s, P_X) = fix_tx[s];
        PL(s, P_Y) = fix_ty[s];
        PL(s, P_ANGLE) = fix_ta[s];
        PL16(s, P_POWER) = 400;
        PL(s, P_WINS) = s & 3;
    }
    INV(I_MINI_MISSILE) = 99;
    my_seat = 0;
}

static void ground(void)
{
    uint16_t x, at = 0;
    for (x = 0; x < FIELD_W; ++x) {
        field_column(x, fix_terrain + at);
        at += entry_len(fix_terrain + at);
    }
}

static void field_scene(uint8_t shots, uint8_t stream_only)
{
    uint16_t t;
    ground();
    play_enter();
    for (;;) {
        for (t = 0; t < 300; ++t) {
            tick();
            play_frame();
        }
        if (!shots)
            continue;
        memcpy(script, fix_script, sizeof(fix_script));
        seg_len = sizeof(fix_script);
        seg_no = 0;
        seg_flags = S_LAST_FRAME | S_LAST_SEG;
        if (!stream_only)
            snd_full(1);
        play_segment();
        for (t = 0; t < 40 && snd_busy(); ++t)
            frame_wait();
        snd_full(0);
        if (test_hook[3] & 2)           /* stop here: the field after the shot */
            for (;;)
                tick();
        for (t = 0; t < 120; ++t)
            tick();
        ground();                       /* the field as it was: again */
        canned();
        net_terrain = 1;
        play_state();
    }
}

void test_check(void)
{
    uint8_t n;
    test_hook[1] = 0xC3;
#ifdef TEST_SCENE                        /* a rig that can't poke memory: built in */
    test_hook[0] = 0x5A;
    test_hook[2] = TEST_SCENE;
    test_hook[3] = 0;
#endif
    for (n = 0; n < 20 && test_hook[0] != 0x5A; ++n)
        frame_wait();
    if (test_hook[0] != 0x5A)
        return;
    test_hook[0] = 0xA5;
    field_init();
    canned();
    strcpy(my_name, "ANN");
    strcpy(srv_table, "room1");
    inp_hook = 1;
    cat_ok = 0;
    switch (test_hook[2]) {
    case 1:
        field_scene(1, test_hook[3] & 1);
    case 5:
        st[OFF_TURN] = 0;
        field_scene(0, 0);
    case 6:                             /* a tune: 0 the round's, 1 the game's end, 2 the shop */
        snd_tune(test_hook[3] == 1 ? t_over1 : test_hook[3] == 2 ? t_shop1 : t_win1,
                 test_hook[3] == 1 ? t_over2 : test_hook[3] == 2 ? t_shop2 : t_win2);
        for (;;)
            tick();
    case 2:
        st[OFF_PHASE] = PH_WAIT;
        wait_enter();
        break;
    case 3:
    case 4:
        st[OFF_PHASE] = test_hook[2] == 3 ? PH_ROUNDEND : PH_GAMEOVER;
        results_enter(test_hook[2] == 4);
        break;
    }
    for (;;)
        tick();
}
#endif
