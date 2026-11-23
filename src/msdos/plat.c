/*
 * plat.c -- an IBM PC, XT or PCjr (or compatible): start-up, the 60 Hz
 * clock and quitting. FujiBlast's src/msdos/plat.c (from M.U.L.E.'s).
 *
 * The clock is the PIT's channel 0 sped up from 18.2 to 60 Hz. Its
 * interrupt counts frame_count (the game's jiffies) and vbi_count (the
 * script's clock), steps the speaker (sound.c), and passes every 3.3rd
 * tick on to the BIOS, so the time of day and the floppy motor still work.
 * A VGA's screen refreshes at 70 Hz, a CGA's at 60: the game's frames are
 * the PIT's either way (plat_hz). FUJINET.SYS's calls may keep interrupts
 * off long enough to lose a tick: the server's round timer says what real
 * time did (clock_check, as on the CoCo).
 *
 * The screen is the field (src/msdos/cga, src/msdos/vga): what is drawn
 * is drawn at once. The game's loop is one pass a jiffy (src/main.c); when
 * it is behind, frame_wait returns at once and the next pass comes
 * sooner, a few at most, and a long stall (an appkey, a slow network
 * call) is let go, not raced after.
 *
 * The game and the menu are two programs (MENU: -DMENU), as on the Atari:
 * one hands over to the other by typing its name at DOS's prompt and
 * ending (plat_chain). Quitting goes back to the FujiNet lobby (lobby.c),
 * the way the FujiNet's other DOS games do.
 */
#include <string.h>
#include <stdlib.h>
#include <dos.h>
#include <i86.h>
#include <conio.h>
#include "game.h"
#include "plat.h"
#include "input.h"
#include "sound.h"
#include "nstrans.h"
#include "msdos.h"

#define CATCH_UP 30             /* jiffies behind: past this (a stall), let them go */
#define MAX_SKIP 4              /* passes in a row that don't wait, at most */
#define PIT_60HZ 19886u         /* 1193182 / 60 */
#define PIT_TICK (PIT_60HZ / SND_SUB)

uint16_t frame_count;
volatile uint8_t vbi_ticks;     /* the script's clock (vbi_now; timer.asm counts it) */
uint8_t plat_pcjr, plat_pal;
uint16_t plat_mem[4];
uint16_t plat_late;             /* passes that didn't wait (tools/dossim) */
uint8_t plat_drew;              /* this pass waited for its jiffy */
static uint16_t seed = 0x1234;
static uint8_t old_mode;

void (__interrupt __far *old_int8)(void);     /* (timer.asm chains to it) */
static void (__interrupt __far *old_int1b)(void);
static void (__interrupt __far *old_int23)(void);
static void (__interrupt __far *old_int24)(void);
static void (__interrupt __far *old_intf5)(void);
/* timer.asm's: the interrupt (SND_SUB ticks a jiffy, the speaker each) */
void __far timer_isr(void);
void timer_setup(void);
uint16_t bios_acc, pit_tick = PIT_TICK;
uint8_t sub_n, snd_sub = SND_SUB;
static uint8_t no_fn, hooked;

#ifdef STATS
/* A profile on a real PC's timing (86Box: tools/dosrig/86box.sh): where
 * the timer's interrupt finds the CPU, by 64 bytes of code; and the PIT
 * in mode 2 (a count down by one each tick) as a clock (ticks of 0.838 us). */
#define PIT_MODE 0x34
uint16_t prof_hist[512], prof_other, prof_uart;   /* (timer.asm counts them) */
uint16_t code_seg, uart_seg;
extern void __far fb_isr(void);
#pragma aux fb_isr "_*"
volatile uint8_t prof_off;      /* not while writing the files */
uint32_t t_wait, t_net, t_input, t_logic;       /* PIT ticks */
uint16_t getcs(void);
#pragma aux getcs = "mov ax, cs" value [ax];

volatile uint32_t tick_base;    /* PIT ticks at the last interrupt (timer.asm) */

uint32_t prof_ticks(void)
{
    uint16_t c;
    uint32_t b;
    _disable();
    outp(0x43, 0x00);           /* latch channel 0 */
    c = inp(0x40);
    c |= inp(0x40) << 8;
    b = tick_base;
    outp(0x20, 0x0A);           /* a tick due, not yet counted */
    if (inp(0x20) & 1)
        b += PIT_TICK;
    _enable();
    return b + (PIT_TICK - c);
}

#else
#define PIT_MODE 0x36
#endif

static void __interrupt __far no_break(void)
{
}

/* DOS's critical error (INT 24h): retry, rather than DOS's "Abort, Retry,
 * Ignore?" over the game. */
static void __interrupt __far crit_err(union INTPACK r)
{
    r.h.al = 1;
}

/* Without FUJINET.SYS nothing answers INT F5h, and its vector may point
 * anywhere (on the PCjr it is left over from BASIC): every call gets an
 * error instead, so the game says so rather than crashing (and the test
 * scenes, which need no network, still run). FUJINET.SYS's header is at
 * the start of INT F5's segment: a block device with a FAT BPB (attribute
 * 2002h, 6020h or 6002h by release), its own entry points, 1-26 units.
 * Loaded high, the segment can be past A000h. */
static uint8_t have_fujinet(void)
{
    void __far *v = _dos_getvect(0xF5);
    uint16_t seg = FP_SEG(v);
    uint16_t attr;
    uint8_t units;
    if (!v || seg < 0x60)
        return 0;
    attr = *(uint16_t __far *)MK_FP(seg, 4);
    units = *(uint8_t __far *)MK_FP(seg, 10);
    return (attr & 0xA000) == 0x2000
        && *(uint16_t __far *)MK_FP(seg, 6) && *(uint16_t __far *)MK_FP(seg, 8)
        && units && units <= 26;
}

static void __interrupt __far no_fujinet(union INTPACK r)
{
    r.w.ax = 'E';
    r.w.dx = 0;
}

uint8_t plat_net_probe(void)
{
    return !no_fn;
}

static void pit(uint16_t divisor)
{
    _disable();
    outp(0x43, divisor ? PIT_MODE : 0x36);      /* channel 0, lo/hi */
    outp(0x40, divisor & 0xFF);
    outp(0x40, divisor >> 8);
    _enable();
}

/* EGA or VGA: INT 10h AH=12h BL=10h returns BL changed. */
static uint8_t ega_or_vga(void)
{
    union REGS r;
    r.h.ah = 0x12;
    r.h.bl = 0x10;
    int86(0x10, &r, &r);
    return r.h.bl != 0x10;
}

/* A Tandy 1000: F000:C000 holds 21h and FFFF:000E FFh. */
static uint8_t tandy(void)
{
    return *(uint8_t __far *)MK_FP(0xF000, 0xC000) == 0x21
        && *(uint8_t __far *)MK_FP(0xFFFF, 0x000E) == 0xFF;
}

/* The stack's low-water mark: what is unused now is filled with a pattern
 * (tools/dossim finds how much stays untouched: plat_stack_free). */
extern unsigned _STACKLOW;

static void stack_fill(void)
{
    uint8_t here;
    uint8_t *p = (uint8_t *)_STACKLOW;
    while (p < &here - 64)
        *p++ = 0xA5;
}

uint16_t plat_stack_free(void)
{
    uint8_t *p = (uint8_t *)_STACKLOW;
    while (*p == 0xA5)
        ++p;
    return (uint16_t)(p - (uint8_t *)_STACKLOW);
}

void plat_init(void)
{
    union REGS r;
    extern uint8_t test_hook[];         /* test.c */

    stack_fill();
    test_hook[0] = 0x3C;        /* 3C C3: tools/dossim finds DGROUP (any build) */
    test_hook[1] = 0xC3;
    r.h.ah = 0x0F;
    int86(0x10, &r, &r);
    old_mode = r.h.al;
    plat_pcjr = *(uint8_t __far *)MK_FP(0xF000, 0xFFFE) == 0xFD;
    plat_pal = plat_pcjr ? PAL_PCJR : ega_or_vga() ? PAL_EGA : tandy() ? PAL_TANDY : PAL_CGA;

    seed ^= *(uint16_t __far *)MK_FP(0x40, 0x6C);   /* the BIOS tick count */
    old_int8 = _dos_getvect(0x08);
    old_int1b = _dos_getvect(0x1B);
    old_int23 = _dos_getvect(0x23);
    old_int24 = _dos_getvect(0x24);
    if (!have_fujinet()) {
        no_fn = 1;
        old_intf5 = _dos_getvect(0xF5);
        _dos_setvect(0xF5, no_fujinet);
    }
    _dos_setvect(0x1B, no_break);
    _dos_setvect(0x23, no_break);
    _dos_setvect(0x24, crit_err);
    plat_mem[0] = _psp;
    plat_mem[1] = *(uint16_t __far *)MK_FP(0x40, 0x13) * 64u;
    hooked = 1;
    video_init();               /* (the VGA build hands over here without one) */
    snd_init();
    _disable();
#ifdef STATS
    code_seg = getcs();
    uart_seg = FP_SEG((void __far *)fb_isr);
#endif
    timer_setup();
    _dos_setvect(0x08, (void (__interrupt __far *)())timer_isr);
    pit(PIT_TICK);
    input_init();
    {
        union REGS q;
        q.h.ah = 0x48;          /* (fails: says how much is left) */
        q.w.bx = 0xFFFF;
        intdos(&q, &q);
        plat_mem[3] = q.w.bx;
    }
#ifdef VIDEO_CARD
    {
        void video_card(void);
        video_card();           /* M0: the test card, the clock running */
    }
#endif
}

/* Everything back as DOS had it. */
void plat_restore(void)
{
    union REGS r;
    if (!hooked)
        return;
    hooked = 0;
    snd_off();
    fb_off();
    input_restore();
    pit(0);
    _dos_setvect(0x08, old_int8);
    _dos_setvect(0x1B, old_int1b);
    _dos_setvect(0x23, old_int23);
    _dos_setvect(0x24, old_int24);
    if (no_fn)
        _dos_setvect(0xF5, old_intf5);
    r.h.ah = 0;
    r.h.al = old_mode;
    int86(0x10, &r, &r);
}

void plat_quit(void)
{
    nst_close_quiet();
    plat_restore();
    lobby_return();
    exit(0);
}

/* Another program, typed at DOS's prompt as we end (the BIOS's keyboard
 * buffer: lobby.c's type_in), as FujiBlast's FBSEL starts the game. */
void plat_chain(const char *prog)
{
    static char line[12];
    nst_close();
    plat_restore();
    strcpy(line, prog);
    strcat(line, "\r");
    type_in(line);
    exit(0);
}

/* the menu (main.c): it doesn't come back */
uint8_t xload(void)
{
    plat_chain(MENU_PROG);
    return 0;
}

/* "boot the disk again": the game, which goes to the menu if no room is set */
void plat_reboot(void)
{
    plat_chain(GAME_PROG);
}

#ifndef MENU
/* The server's round timer says what real time did: each second it ticks,
 * the jiffies counted since are set against 60, and what is missing goes
 * back on the clock, one a pass (a count that came late is made up by the
 * next one coming early). */
static uint8_t tm_last = 0xFF;
static uint16_t tm_at;
static int8_t drift;

static void clock_check(void)
{
    uint8_t t = st[OFF_TIMER];
    int16_t d;
    if (t != tm_last) {
        if ((uint8_t)(tm_last - t) == 1) {
            d = drift + 60 - (int16_t)(frame_count - tm_at);
            drift = d > 90 ? 90 : d < -90 ? -90 : (int8_t)d;
        }
        tm_last = t;
        tm_at = frame_count;
    }
    if (drift > 0) {
        _disable();             /* (the interrupt changes it too) */
        ++frame_count;
        _enable();
        --drift;
    }
}
#endif

#if defined(STATS) && defined(TEST_SCENES)
/* STATS=1 TEST=1 TEST_SCENE=1 TEST_ARG=2 (tools/dosrig/86box.sh): once the
 * fixture's shot is played (test_hook[0] DDh), A:\LAG.TXT says how many
 * jiffies it took and A:\PROF.BIN where the timer found the CPU
 * (tools/dosrig/prof86.py). */
#include <fcntl.h>
#include <dos.h>
static void lag_out(void)
{
    extern uint8_t test_hook[];
    extern uint16_t test_t1;
    static uint8_t done;
    static char line[] = "PLAYED 00000\r\n";
    uint16_t n = test_t1;
    unsigned w;
    int fd;
    int8_t k;
    if (done || test_hook[0] != 0xDD)
        return;
    done = prof_off = 1;
    for (k = 11; k >= 7; --k, n /= 10)
        line[k] = (char)('0' + n % 10);
    if (!_dos_creat("A:\\LAG.TXT", _A_NORMAL, &fd)) {
        _dos_write(fd, line, sizeof(line) - 1, &w);
        _dos_close(fd);
    }
    if (!_dos_creat("A:\\PROF.BIN", _A_NORMAL, &fd)) {
        _dos_write(fd, prof_hist, sizeof(prof_hist), &w);
        _dos_write(fd, &prof_other, 2, &w);
        _dos_write(fd, &prof_uart, 2, &w);
        _dos_close(fd);
    }
}
#endif

#if defined(STATS) && !defined(TEST_SCENES)
/* STATS=1 (tools/dosrig/86box.sh LIVE=1): every 600 jiffies a line on
 * A:\STATS.TXT: jiffies, passes that didn't wait, the link's counters
 * (replies bad, never came, resends, reads lost), bytes the UART lost,
 * bytes received. */
#include <fcntl.h>
#include <io.h>
extern uint8_t fb_cnt[4];
extern volatile uint8_t fb_ovr;
#pragma aux fb_ovr "_*"
extern uint16_t fb_nrx;
#pragma aux fb_nrx "_*"

static char *num(char *p, uint16_t n)
{
    char b[6], *q = b + 5;
    *q = 0;
    do {
        *--q = (char)('0' + n % 10);
        n /= 10;
    } while (n);
    while (*q)
        *p++ = *q++;
    *p++ = ' ';
    return p;
}

static void stats_out(void)
{
    static uint16_t told;
    static char line[64];
    char *p = line;
    unsigned w;
    int fd;
    uint8_t i;
    if ((uint16_t)(frame_count - told) < 600)
        return;
    told = frame_count;
    p = num(p, frame_count);
    p = num(p, plat_late);
    for (i = 0; i < 4; ++i)
        p = num(p, fb_cnt[i]);
    p = num(p, fb_ovr);
    p = num(p, fb_nrx);
    *p++ = '\r';
    *p++ = '\n';
    if (_dos_open("A:\\STATS.TXT", O_WRONLY, &fd) && _dos_creat("A:\\STATS.TXT", _A_NORMAL, &fd))
        return;
    lseek(fd, 0, SEEK_END);
    _dos_write(fd, line, (unsigned)(p - line), &w);
    _dos_close(fd);
}
#endif

void frame_wait(void)
{
    static uint16_t seen;
    static uint8_t skipped;     /* passes since the last that waited */
    uint16_t d;
#ifdef STATS
    static uint32_t out;        /* when the last call returned */
    uint32_t in = prof_ticks();
    if (out)
        t_logic += in - out;
#endif
#ifndef MENU
    clock_check();
#endif
#if defined(STATS) && defined(TEST_SCENES)
    lag_out();
#elif defined(STATS)
    stats_out();
#endif
    d = frame_count - seen;
    if (d > CATCH_UP) {         /* a stall (a disk read, an appkey): let it go */
        seen = frame_count - 1;
        d = 1;
    }
    seed += frame_count;
    snd_tick();
    plat_drew = d < 2 || skipped >= MAX_SKIP;
    if (!plat_drew) {
        ++skipped;
        ++seen;                 /* behind: the next pass at once */
        ++plat_late;
    } else {
        skipped = 0;
        while (*(volatile uint16_t *)&frame_count == seen)
            ;                   /* (the timer interrupt counts it) */
        ++seen;
#ifdef STATS
        t_wait += prof_ticks() - in;
#endif
    }
    video_frame();
#ifdef STATS
    out = prof_ticks();
#endif
}

uint8_t plat_rand(void)
{
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return (uint8_t)seed;
}

uint8_t plat_hz(void)
{
    return 60;
}

/* The script's clock (play.c waits on it): a jiffy's flash, shake and fire
 * step here too, while a script plays. */
uint8_t vbi_now(void)
{
    snd_tick();
    video_frame();
    return vbi_ticks;
}
