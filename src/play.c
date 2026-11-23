/*
 * play.c -- the field: the tanks as the STATE has them, the status rows,
 * aiming on our turn, and each shot's SCRIPT played (docs/PROTOCOL.md).
 *
 * The script player is server/internal/view's, op for op: each frame
 * every track, blast and settle moves a step, then the ops due that frame
 * run. Frames are the clock: a frame that took too long to draw is caught
 * up (steps run, the drawing only of the last), so the shot keeps the
 * server's time on every machine.
 */
#include <string.h>
#include "game.h"
#include "field.h"
#include "input.h"
#include "plat.h"
#include "sound.h"

#ifdef __ATARI__
#define CONSOL (*(volatile uint8_t *)0xD01F)
#define CONSOL_KEYS() (~CONSOL & 7)     /* START, SELECT, OPTION held */
#else
#define CONSOL_KEYS() 0                 /* (none: the keys do it) */
#endif

tank_t tk[SEATS];
uint8_t my_seat = NONE;

/* ---- the STATE's tanks ------------------------------------------------------ */

static uint8_t nlen(const char *s, uint8_t max)
{
    uint8_t n = 0;
    while (n < max && s[n])
        ++n;
    return n;
}

uint16_t entry_len(const uint8_t *e)
{
    return *e <= T_EMPTY ? 1 : 1 + 2 * (*e - T_SPANS);
}

static uint8_t in_script;
#pragma bss-name (push, "HIGH")
static uint8_t energy[SEATS];
#pragma bss-name (pop)

/* the tanks as the STATE has them: those that changed redrawn */
static void tanks_from_state(uint8_t all)
{
    uint8_t s, moved = 0, on, y, a;
    int16_t x;
    for (s = 0; s < SEATS; ++s) {
        on = KIND(s) != KIND_EMPTY && ALIVE(s);
        x = (int16_t)PL16(s, P_X);
        y = PL(s, P_Y);
        a = PL(s, P_ANGLE);
        energy[s] = PL(s, P_ENERGY);
        if (all || on != tk[s].on || x != tk[s].x || y != tk[s].y || a != tk[s].angle) {
            if (on != tk[s].on || x != tk[s].x || y != tk[s].y) {
                moved = 1;
                if (tk[s].on)
                    field_tank(s, 0);       /* (a drive: from where it was) */
            }
            tk[s].on = on;
            tk[s].x = x;
            tk[s].y = y;
            tk[s].angle = a;
            field_tank(s, 1);
        }
    }
    if (moved || all)
        field_colours();
}

/* ---- the status rows ------------------------------------------------------ */

#pragma bss-name (push, "PMGFREE")
static char line[41];
#pragma bss-name (pop)

static void put(uint8_t col, const char *s)
{
    while (*s && col < 40)
        line[col++] = *s++;
}

static void putn(uint8_t col, uint16_t v)
{
    char b[6];
    numstr(b, v);
    put(col, b);
}

static uint8_t aim_a;
static uint16_t aim_p;
static uint8_t my_turn, weapon;

/* the items line (OPTION): what is open, the item, guidance and its target */
enum { M_OFF, M_ITEM, M_TARGET, M_DRIVE };
static uint8_t menu, item, guide = I_NONE, target = NO_SEAT;

static uint8_t defending(void)
{
    return st[OFF_PHASE] == PH_DEFENSE && my_seat < SEATS &&
           (PL(my_seat, P_FLAGS) & (PF_DEFENSE | PF_DONE)) == PF_DEFENSE;
}

/* ---- your turn, the defence window: what stands out ------------------------ */

#define BANNER_J 180                    /* YOUR TURN! flashes this long */
#define REMIND_J 600                    /* idle this long on your turn: the cue again */
#define TXT_DEFENCES "SET UP DEFENCES, RETURN: DONE"

/* one HUD field flashes (inverse and back, 16 jiffies each): the banner
 * (row 2, "YOUR TURN!") or the defence window's prompt (row 1) */
enum { FL_NONE, FL_BANNER, FL_DEFENCE };
static uint8_t fl_kind, fl_lit, def_open;
static uint16_t turn_at, idle_at;

static void hud(void);
static void flash_frame(void)
{
    uint8_t lit, row, col;
    const char *t;
    if (fl_kind == FL_BANNER) {
        if (!my_turn || (uint16_t)(frame_count - turn_at) >= BANNER_J) {
            fl_kind = FL_NONE;
            hud();                      /* (its usual words) */
            return;
        }
#if ITEMS_ROW == 2
        if (menu)
            return;                     /* (row 2 is the items line) */
#endif
        row = 2;
        col = 16;
        t = "YOUR TURN!";
    } else if (fl_kind == FL_DEFENCE) {
        if (!defending()) {
            fl_kind = FL_NONE;
            return;
        }
        row = 1;
        col = 0;
        t = "SET UP DEFENCES";
    } else
        return;
    lit = !(frame_count & 16);
    if (lit == fl_lit)
        return;
    fl_lit = lit;
    if (lit)
        field_status_hi(row, col, t);
    else
        field_status(row, col, t);
}

/* the turn's arrow: blinking over the tank whose turn it is (ours, when
 * everyone aims and we haven't fired); off the field before any script */
static uint8_t mk_seat = NONE, mk_on;
static int16_t mk_x, mk_y;
static uint16_t mk_at;

/* a down arrow, 5, 3 and 1 pixels wide: fire over sky (only sky changes) */
static void mk_draw(uint8_t on)
{
    uint8_t from = on ? PX_SKY : PX_FX, to = on ? PX_FX : PX_SKY, r;
    int8_t c;
    for (r = 0; r < 3; ++r)
        for (c = r - 2; c <= 2 - r; ++c)
            field_swap(mk_x + c, mk_y + r, from, to);
    mk_on = on;
}

static void turn_mark(uint8_t s)
{
    if (s >= SEATS || !tk[s].on)
        s = NONE;
    if (mk_seat != NONE && (s != mk_seat || tk[s].x != mk_x || tk[s].y - MARK_UP != mk_y)) {
        if (mk_on)
            mk_draw(0);
        mk_seat = NONE;
    }
    if (s == NONE)
        return;
    if (mk_seat == NONE) {
        mk_seat = s;
        mk_x = tk[s].x;
        mk_y = tk[s].y - MARK_UP;
        mk_at = frame_count;
        mk_draw(1);
        return;
    }
    if ((uint16_t)(frame_count - mk_at) >= 15) {
        mk_at = frame_count;
        mk_draw(!mk_on);
    }
}

/* the items line (OPTION) into line: what is open, the item, guidance and
 * its target */
static void items_line(void)
{
    char nm[17];
    memset(line, ' ', 40);
    switch (menu) {
    case M_ITEM:
        cat_name(cat_item(item) + 6, nm, 16);
        put(0, nm);
        put(19, "X");
        putn(20, INV(item));
        if (item == I_PARACHUTE || item == I_IMPACT_FUSE)
            put(24, PL(my_seat, P_FLAGS) & (item == I_PARACHUTE ? PF_CHUTE : PF_FUSE) ? "ON" : "OFF");
        put(29, "<> FIRE USE");
        break;
    case M_TARGET:
        put(0, "TARGET");
        memcpy(line + 7, NAME(target), nlen(NAME(target), NAME_LEN));
        put(29, "<> FIRE ON");
        break;
    default:
        put(0, "DRIVE <>  FUEL");
        putn(15, INV(I_FUEL));
        put(27, "FIRE DONE");
    }
}

static void hud(void)
{
    uint8_t s = st[OFF_TURN];
    int16_t w = (int16_t)(st[OFF_WIND] | st[OFF_WIND + 1] << 8);
    char nm[17];
    fl_lit = 0;                         /* (all of it in plain text again) */
    if (s == NO_SEAT || s >= SEATS)
        s = my_seat < SEATS ? my_seat : 0;
    memset(line, ' ', 40);
    line[40] = 0;
    memcpy(line, NAME(s), nlen(NAME(s), NAME_LEN));
    put(10, "ANGLE");
    putn(16, my_turn && s == my_seat ? aim_a : PL(s, P_ANGLE));
    put(21, "POWER");
    putn(27, my_turn && s == my_seat ? aim_p : PL16(s, P_POWER));
    put(33, "E");
    putn(35, energy[s]);
    field_status(0, 0, line);
    memset(line, ' ', 40);
    if (defending()) {                  /* the defence window: what to do */
        put(0, TXT_DEFENCES);
        putn(37, st[OFF_TIMER]);
    } else {
        cat_name(cat_item(my_turn && s == my_seat ? weapon : PL(s, P_WEAPON)) + 6, nm, 16);
        put(0, nm);
        if (s == my_seat && weapon != I_MINI_MISSILE) {
            put(17, "X");
            putn(18, INV(weapon));
        }
        put(24, "WIND");
        if (w < 0) {
            put(29, "<");
            w = -w;
        } else if (w > 0)
            put(29, ">");
        putn(31, (uint16_t)w);
#if ITEMS_ROW == 2
        if (menu)
            putn(37, st[OFF_TIMER]);    /* (row 2 is the items line) */
#endif
    }
    field_status(1, 0, line);
    memset(line, ' ', 40);
    put(0, "ROUND");
    putn(6, st[OFF_ROUND]);
    put(9, "OF");
    putn(12, st[OFF_ROUNDS]);
#if ITEMS_ROW == 2
    if (menu)
        items_line();
    else
#endif
    {
        if (defending())
            put(16, TXT_ITEMS);
        else if (my_turn)
            put(16, fl_kind == FL_BANNER ? "YOUR TURN!" : "YOUR TURN: AIM, FIRE");
        else if (st[OFF_PHASE] == PH_AIM && st[OFF_TURN] < SEATS) {
            memcpy(line + 16, NAME(st[OFF_TURN]), nlen(NAME(st[OFF_TURN]), NAME_LEN));
            put(16 + nlen(NAME(st[OFF_TURN]), NAME_LEN), " AIMS");
        } else if (st[OFF_PHASE] == PH_AIM)
            put(16, "EVERYONE AIMS");
        if (!defending())
            putn(37, st[OFF_TIMER]);    /* (defending: row 1 has it) */
    }
    field_status(2, 0, line);
#if ITEMS_ROW == 3
    if (menu) {                         /* its own row (talk's, in a script) */
        items_line();
        field_status(3, 0, line);
    } else if (!in_script) {
        memset(line, ' ', 40);
        field_status(3, 0, line);
    }
#endif
}

/* an aim step changes two numbers of the first row: those alone (a whole
 * HUD is slow on a 1 MHz machine), each padded over the last */
static void hud_num(uint8_t col, uint16_t v, uint8_t w)
{
    char b[6];
    uint8_t n;
    numstr(b, v);
    for (n = (uint8_t)strlen(b); n < w; ++n)
        b[n] = ' ';
    b[n] = 0;
    field_status(0, col, b);
}

static void hud_aim(void)
{
    hud_num(16, aim_a, 3);
    hud_num(27, aim_p, 4);
}

/* ---- entering the field, and the STATE changing ------------------------- */

static void turn_check(void);

void play_enter(void)
{
    in_script = 0;
    my_turn = 0;
    fl_kind = FL_NONE;
    def_open = 0;
    mk_seat = NONE;                     /* (the field is drawn afresh: no arrow) */
    memset(tk, 0, sizeof(tk));
    field_open(st[OFF_SKY], st[OFF_PALETTE]);
    tanks_from_state(1);
    turn_check();                       /* (a round may open on our turn) */
    hud();
}

static uint8_t is_my_turn(void)
{
    if (my_seat >= SEATS || st[OFF_PHASE] != PH_AIM || !ALIVE(my_seat))
        return 0;
    if (st[OFF_TURN] == my_seat)
        return 1;
    return st[OFF_TURN] == NO_SEAT && !(PL(my_seat, P_FLAGS) & PF_DONE);
}

/* our turn begins: the aim as the server has it, the cue, YOUR TURN! */
static void turn_check(void)
{
    uint8_t t = is_my_turn();
    if (t && !my_turn) {
        aim_a = PL(my_seat, P_ANGLE);
        aim_p = PL16(my_seat, P_POWER);
        weapon = PL(my_seat, P_WEAPON);
        snd_cue(UI_TURN, 0);
        turn_at = idle_at = frame_count;
        fl_kind = FL_BANNER;
    }
    my_turn = t;
}

void play_state(void)
{
    if (in_script)
        return;
    tanks_from_state(net_terrain);
    net_terrain = 0;
    turn_check();
    hud();
}

/* ---- aiming --------------------------------------------------------------- */

static uint8_t held_n, sent_a, last_send;
static uint16_t sent_p;
static uint8_t consol_prev;

static void next_weapon(void)
{
    uint8_t w = weapon;
    do {
        if (++w >= N_WEAPONS)
            w = 0;
    } while (w != I_MINI_MISSILE && !INV(w) && w != weapon);
    weapon = w;
    snd_cue(UI_SELECT, 0);
    net_act(ACT_WEAPON, &weapon, 1);
}

/* the next item owned from i (the Defense Computer works by itself) */
static uint8_t next_item(uint8_t i, int8_t d)
{
    uint8_t n;
    for (n = 0; n < N_ITEMS - N_WEAPONS; ++n) {
        i += d;
        if (i < N_WEAPONS)
            i = N_ITEMS - 1;
        else if (i >= N_ITEMS)
            i = N_WEAPONS;
        if (INV(i) && i != I_DEFENSE_COMPUTER &&
            (!defending() || (i >= I_PARACHUTE && i <= I_SUPER_DEFLECTOR) || i == I_IMPACT_FUSE))
            return i;                   /* (the defence window: defences alone) */
    }
    return I_NONE;
}

static uint8_t next_target(uint8_t t, int8_t d)
{
    uint8_t n;
    for (n = 0; n < SEATS; ++n) {
        t += d;
        if (t >= SEATS)
            t = d < 0 ? SEATS - 1 : 0;
        if (t != my_seat && tk[t].on)
            return t;
    }
    return NO_SEAT;
}

/* an arrow over the target */
static void mark(uint8_t on)
{
    int8_t k;
    if (target >= SEATS)
        return;
    for (k = -1; k <= 1; ++k)
        field_swap(tk[target].x + k, tk[target].y - TANK_H - 4 + (k ? 0 : 1),
                   on ? PX_SKY : PX_FX, on ? PX_FX : PX_SKY);
}

static uint8_t dir_prev, drive_n;

/* ESC on the field: closes the items line if it is open (1), else 0 */
uint8_t play_esc(void)
{
    if (!menu)
        return 0;
    mark(0);
    menu = M_OFF;
    hud();
    return 1;
}

/* the items line has the stick and the button */
static void items_frame(uint8_t close)
{
    uint8_t d = inp_dir, e = d & ~dir_prev, a, was = menu, it = item, t = target;
    int8_t step = e & IN_LEFT ? -1 : e & IN_RIGHT ? 1 : 0;
    dir_prev = d;
    if (close) {
        mark(0);
        menu = M_OFF;
    } else if (menu == M_ITEM) {
        if (step) {
            item = next_item(item, step);
            snd_cue(UI_TICK, 0);
        }
        if (ev_btn) {
            snd_cue(UI_SELECT, 0);
            if (item <= I_AUTOPILOT && !defending()) {
                guide = item;
                target = next_target(target, 1);
                menu = M_TARGET;
                mark(1);
            } else if (item == I_FUEL && !defending())
                menu = M_DRIVE;
            else {
                net_act(ACT_USE, &item, 1);
                if (item != I_PARACHUTE && item != I_IMPACT_FUSE && !defending())
                    menu = M_OFF;       /* (defending: more to set up) */
            }
        }
    } else if (menu == M_TARGET) {
        if (step) {
            mark(0);
            target = next_target(target, step);
            mark(1);
            snd_cue(UI_TICK, 0);
        }
        if (ev_btn) {
            mark(0);
            menu = M_OFF;
            snd_cue(UI_SELECT, 0);
        }
    } else {
        if (d & (IN_LEFT | IN_RIGHT)) {
            if (!(drive_n++ & 7)) {
                a = (d & IN_RIGHT) != 0;
                net_act(ACT_DRIVE, &a, 1);
            }
        } else
            drive_n = 0;
        if (ev_btn)
            menu = M_OFF;
    }
    if (menu != was || item != it || target != t || ev_btn)
        hud();                          /* (only then: it takes frames) */
}

void play_frame(void)
{
    uint8_t d, step, args[6], c, def, opt;
    if (net_terrain) {
        tanks_from_state(1);
        net_terrain = 0;
    }
    turn_mark(my_turn ? my_seat : st[OFF_PHASE] == PH_AIM ? st[OFF_TURN] : NONE);
    flash_frame();
    def = defending();
    if (!def)
        def_open = 0;
    else if (!def_open) {               /* the window opens: its items line, at once */
        def_open = 1;
        snd_cue(UI_TURN, 0);
        fl_kind = FL_DEFENCE;
        item = next_item(N_ITEMS - 1, 1);
        if (item != I_NONE) {
            menu = M_ITEM;
            dir_prev = inp_dir;
        }
        hud();
    }
    if (my_turn) {                      /* idle on our turn: the cue again */
        if (inp_dir || ev_btn || ev_key)
            idle_at = frame_count;
        else if ((uint16_t)(frame_count - idle_at) >= REMIND_J) {
            idle_at = frame_count;
            snd_cue(UI_TURN, 0);
        }
    }
    c = CONSOL_KEYS();
    opt = ((c & 4) && !(consol_prev & 4)) || ev_key == 'I';
    if (!my_turn && !def) {
        if (menu) {
            mark(0);
            menu = M_OFF;
        }
        consol_prev = c;
        return;
    }
    if (def && ev_key == KEY_RETURN) {  /* the defence window: done */
        mark(0);
        menu = M_OFF;
        consol_prev = c;
        net_act(ACT_DONE, 0, 0);
        snd_cue(UI_GO, 0);
        hud();
        return;
    }
    if (menu) {
        consol_prev = c;
        items_frame(opt);
        return;
    }
    if (opt) {                          /* the items line */
        consol_prev = c;
        item = next_item(N_ITEMS - 1, 1);
        if (item == I_NONE)
            snd_cue(UI_ERROR, 0);
        else {
            menu = M_ITEM;
            dir_prev = inp_dir;
            hud();
        }
        return;
    }
    if (def) {                          /* the defence window: items only */
        consol_prev = c;
        return;
    }
    d = inp_dir;
    if (d)
        ++held_n;
    else
        held_n = 0;
    step = held_n > 40 ? 4 : held_n > 15 ? 2 : 1;
    if (d && (held_n == 1 || held_n > 15 || (held_n & 3) == 0)) {
        if ((d & IN_LEFT) && aim_a < 180)
            aim_a = aim_a + step > 180 ? 180 : aim_a + step;
        if ((d & IN_RIGHT) && aim_a > 0)
            aim_a = aim_a < step ? 0 : aim_a - step;
        if ((d & IN_UP) && aim_p < energy[my_seat] * 10u)
            aim_p += step * 5;
        if ((d & IN_DOWN) && aim_p > 0)
            aim_p = aim_p < step * 5u ? 0 : aim_p - step * 5;
        if (aim_p > energy[my_seat] * 10u)
            aim_p = energy[my_seat] * 10u;
        tk[my_seat].angle = aim_a;
        field_tank(my_seat, 1);
        hud_aim();
    }
    if ((c & 2) && !(consol_prev & 2))
        next_weapon();                  /* SELECT */
    consol_prev = c;
    if (ev_key == 'W' || ev_key == 9)
        next_weapon();
    if ((uint8_t)(frame_count - last_send) >= 8 && (aim_a != sent_a || aim_p != sent_p)) {
        args[0] = aim_a;
        args[1] = (uint8_t)aim_p;
        args[2] = aim_p >> 8;
        net_act(ACT_AIM, args, 3);
        sent_a = aim_a;
        sent_p = aim_p;
        last_send = (uint8_t)frame_count;
    }
    if (ev_btn) {
        args[0] = aim_a;
        args[1] = (uint8_t)aim_p;
        args[2] = aim_p >> 8;
        args[3] = weapon;
        args[4] = guide;
        args[5] = target;
        net_act(ACT_FIRE, args, 6);
        guide = I_NONE;
        target = NO_SEAT;
        my_turn = 0;
        hud();
    }
}

/* ---- the script player ------------------------------------------------------ */

/* ply.s plays the timeline; the ops about tanks, talk, damage and sound
 * come here (c_op), their arguments read at ply_pc */
extern uint16_t ply_pc, ply_wait;
#pragma zpsym ("ply_pc")
extern uint8_t ply_done;
void ply_reset(void);
void ply_step(void);
void ply_draw(void);
void ply_end(void);
void ops_beams_off(void);

static uint8_t talk_seat = NONE, talk_left, talk_n, talk_i;
static uint8_t recolour;        /* tanks moved: their colours at the next draw
                                 * (once a frame: five falls in one frame were
                                 * five DLI rebuilds, a second of frozen field) */

static uint8_t rd8(void)
{
    return script[ply_pc++];
}

static int16_t rd16(void)
{
    int16_t v = (int16_t)(script[ply_pc] | script[ply_pc + 1] << 8);
    ply_pc += 2;
    return v;
}

static void talk_end(void)
{
    field_balloon(talk_seat, 0, 0);
    talk_seat = NONE;
    snd_blip(0);
}

/* a frame of talk: a blip a letter, then the balloon goes */
void c_anim(void)
{
    if (talk_seat == NONE)
        return;
    if (talk_i < talk_n) {
        snd_blip(20 + (talk_seat << 2) + (talk_i & 3) * 3);
        ++talk_i;
    } else
        snd_blip(0);
    if (!--talk_left)
        talk_end();
}

/* tank s moved or turned: drawn again (rd: its x, y and angle from the op) */
static void tank_at(uint8_t s, uint8_t angle)
{
    field_tank(s, 0);
    tk[s].x = rd16();
    tk[s].y = rd8();
    if (angle)
        tk[s].angle = rd8();
    field_tank(s, 1);
    recolour = 1;
}

void __fastcall__ c_op(uint8_t o)
{
    uint8_t s = rd8(), n;
    if (s >= SEATS && o != OP_SOUND && o != OP_FLASH)
        s = NONE;
    switch (o) {
    case OP_TALK:
        n = rd8();
        if (s != NONE && tk[s].on) {
            field_balloon(s, (const char *)script + ply_pc, n);
            talk_seat = s;
            talk_n = n;
            talk_i = 0;
            talk_left = 120;
        }
        ply_pc += n;
        break;
    case OP_SOUND:
        snd_cue(s, rd8());
        break;
    case OP_SHIELD:
        n = rd8();
        ++ply_pc;
        if (s != NONE)
            field_sparks(s | (n + 8) << 8);
        break;
    case OP_DAMAGE:
        rd8();
        n = rd8();
        if (s != NONE)
            energy[s] = n;
        hud();
        break;
    case OP_FALL:
        if (s != NONE)
            tank_at(s, 0);
        else
            ply_pc += 3;
        ply_pc += 2;
        break;
    case OP_DEATH:
        n = rd8();
        if (s != NONE) {
            field_tank(s, 0);
            tk[s].on = 0;
            recolour = 1;
            field_sparks(s | n << 8);
        }
        break;
    case OP_TANK:
        if (s != NONE)
            tank_at(s, 1);
        else
            ply_pc += 4;
        break;
    case OP_FLASH:
        field_flash(rd8());
        break;
    case OP_FIRE:
        n = rd8();
        if (s != NONE) {
            tk[s].angle = n;
            field_tank(s, 1);
            if (talk_seat == s)
                talk_end();
        }
        ply_pc += 3;
        break;
    }
}

/* draw what moved: the blasts at their size now; the whistle */
static void draw(void)
{
    uint8_t f;
    ply_draw();
    if (recolour) {
        recolour = 0;
        field_colours();
    }
    f = trk_fall();
    if (talk_seat == NONE)
        snd_whistle(f != 0, 16 + (f >> 2));
}

/* the script's end: anything still going finishes now */
static void script_end(void)
{
    uint8_t i;
    for (i = 0; i < 16; ++i)
        trk_stop(i);
    ops_beams_off();
    if (talk_seat != NONE)
        field_balloon(talk_seat, 0, 0);
    talk_seat = NONE;
    field_fire(0);
    snd_whistle(0, 0);
    snd_blip(0);
    in_script = 0;
    net_terrain = 1;                    /* settles cut through tanks: all again */
}

/* play the segment in script[] (seg_len bytes) to its end */
void play_segment(void)
{
    uint8_t last, behind;
    in_script = 1;
    my_turn = 0;
    turn_mark(NONE);                    /* (no arrow in the field a script plays on) */
    if (seg_no == 0)                    /* a new shot */
        ply_reset();
    ply_pc = 0;
    ply_done = 0;
    hud();
    last = vbi_count;
    while (!ply_done) {
        while ((behind = (uint8_t)(vbi_count - last)) == 0)
            ;
        if (behind > PLY_CATCHUP)
            behind = PLY_CATCHUP;
        last += behind;
        while (behind--) {
            ply_step();
            if (ply_done)
                break;
        }
        draw();
    }
    ply_end();
    if (seg_flags & S_LAST_SEG)
        script_end();
}

/* a TERRAIN came while a script was under way: the server stopped waiting
   for our D and left the shot behind. The script ends here (1 if so), and
   the caller takes the field as not ours: all of it comes again. */
uint8_t play_skip(void)
{
    if (!in_script)
        return 0;
    script_end();
    return 1;
}

void play_script(void)
{
    uint8_t i;
    if (seg_flags & S_HOLD) {
        net_hold();
        snd_full(1);
    }
    play_segment();
    if (seg_flags & S_HOLD) {
        for (i = 0; i < 40 && snd_busy(); ++i)  /* a blast's tail first */
            frame_wait();
        snd_full(0);
    }
    net_done();
}
