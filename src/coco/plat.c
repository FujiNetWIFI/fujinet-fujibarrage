/*
 * plat.c -- CoCo 1/2 and CoCo 3 start-up, the 60 Hz clock and quitting
 * (FujiBlast's src/coco/plat.c, from M.U.L.E.'s).
 *
 * The clock is the VDG's field sync interrupt (PIA0 CB1), counted by
 * irq_isr: frame_count (the game's jiffies) and vbi_ticks (the script's
 * clock). The DriveWire ROM masks interrupts while it talks to the FujiNet,
 * so jiffies can go uncounted: the server's round timer says what real time
 * did (clock_check, as on the Apple II).
 *
 * The game's loop is one pass a jiffy (src/main.c). When it is behind,
 * frame_wait returns at once and the pass is logic only, a few at most.
 *
 * By the time plat_init has run bank_init, the machine is in all-RAM mode
 * (see bank.c): the BASIC ROMs are gone, so the IRQ handler must not chain
 * to them.
 *
 * The CoCo 3 (COCO3) uses the same interrupt, vector ($010C) and keyboard,
 * and runs at 1.79 MHz.
 */
#include <string.h>
#include "cocoplat.h"
#include "game.h"
#include "input.h"
#include "plat.h"
#include "sound.h"
#include "scr.h"
#include "nstrans.h"

#define CATCH_UP 30             /* jiffies behind: past this (a stall), let them go */
#define MAX_SKIP 4              /* passes in a row with no drawing, at most */

/* frame_count, vbi_ticks: src/coco/fixed.asm (the same address in every program) */
static uint16_t seed = 0x1234;

/* The keyboard, scanned here 60 times a second so that no key press is
 * missed however long the game's own pass takes (input.c reads these):
 * kb_rows[c] is column c's rows (1 = down, joystick buttons masked out),
 * and each key that went down is queued as column * 8 + row. */
uint8_t kb_rows[8], kb_prev[8];
uint8_t kb_q[8], kb_head, kb_tail;
static uint8_t kb_any;          /* a key was down at the last scan */

/* The IRQ: on the field sync count a frame and scan the keyboard; clear
 * whichever PIA0 flag brought us here. */
interrupt asm void irq_isr(void)
{
    asm {
        lda     $FF03
        lbpl    irq_hsync
        lda     $FF02           ; clears the field sync flag
        ldd     _frame_count
        addd    #1
        std     _frame_count
        inc     _vbi_ticks      ; the clock of the scripts
#ifdef COCO3
        andcc   #$EF            ; (acknowledged: the sound timer may cut in, or its
                                ;  edges come late, a frame worth flat)
        lda     _fire_on        ; colour 5 (fire) cycles, a step every other frame
        beq     @nofire
        ldb     _frame_count+1
        lsrb
        andb    #7
        leax    _fire_cyc,pcr
        lda     b,x
        sta     $FFB5
@nofire
#endif
        ; the keyboard: rows low with no column driven are joystick buttons
        lda     #$FF
        sta     $FF02
        lda     $FF00
        coma
        anda    #$7F
        pshs    a               ; 0,s: the buttons
        clr     $FF02           ; every column low: no key down now nor at the last
        lda     $FF00           ;  scan, and the rows have nothing new (most jiffies)
        coma
        anda    #$7F
        cmpa    ,s
        bne     irq_scan
        tst     _kb_any
        bne     irq_scan
        leas    1,s
        lda     #$FF
        sta     $FF02
        bra     irq_out
irq_scan
        leax    _kb_rows,pcr
        leay    _kb_prev,pcr
        ldb     #$FE            ; column 0 low
        clr     ,-s             ; 0,s: column; 1,s: buttons
@col
        stb     $FF02
        lda     $FF00
        coma
        anda    #$7F
        pshs    b
        ldb     2,s
        comb
        pshs    b
        anda    ,s+             ; not the buttons
        puls    b
        sta     ,x
        eora    ,y              ; changed ...
        anda    ,x              ; ... and down now: pressed
        beq     @same
        pshs    b,x,y
        ldb     5,s             ; the column
        aslb
        aslb
        aslb
@bit
        lsra
        bcc     @nobit
        pshs    a,b
        lda     _kb_head
        leax    _kb_q,pcr
        stb     a,x             ; queue column * 8 + row
        inca
        anda    #7
        cmpa    _kb_tail
        beq     @full
        sta     _kb_head
@full
        puls    a,b
@nobit
        incb
        tsta
        bne     @bit
        puls    b,x,y
@same
        lda     ,x+
        sta     ,y+
        inc     ,s
        orcc    #$01            ; next column low
        rolb
        bcs     @col
        leas    2,s
        lda     #$FF
        sta     $FF02
        leax    _kb_rows,pcr    ; any down?
        clra
        ldb     #8
irq_or
        ora     ,x+
        decb
        bne     irq_or
        sta     _kb_any
        bra     irq_out
irq_hsync
        lda     $FF00           ; (not enabled; clear it anyway)
irq_out
    }
}

/* (the CoCo 3's sound takes the vector over once it is loaded: its timer's
 * IRQ first, then irq_isr; src/coco3/snd3a.asm eng_init) */
#define IRQ_TARGET _irq_isr

void plat_irq_init(void)
{
    asm {
        orcc    #$50
        lda     #$7E            ; JMP irq_isr at the RAM vector ($010C)
        sta     $010C
        leax    IRQ_TARGET,pcr
        stx     $010D
        lda     $FF01           ; PIA0 CA1 (hsync) interrupt off
        anda    #$FE
        sta     $FF01
        lda     $FF03           ; CB1 (field sync) on, falling edge
        ora     #$01
        anda    #$FD
        sta     $FF03
        lda     $FF02
        andcc   #$EF            ; IRQ on (FIRQ stays off)
    }
}

/* The field rate, 60 or 50 (a PAL machine): a loop of 13 cycles counted
 * from one field sync to the next, less the IRQ's time (MAME: a CoCo 1/2
 * 1138 at 60, about 1355 at 50; a CoCo 3 2285 and 2724) */
static uint8_t hz = 60;
static uint16_t hz_n;

static void hz_measure(void)
{
    asm {
        ldx     #0
        lda     _frame_count+1
hz_a    cmpa    _frame_count+1  ; (a field sync first)
        beq     hz_a
        lda     _frame_count+1
hz_b    leax    1,x
        cmpa    _frame_count+1
        beq     hz_b
        stx     _hz_n
    }
#ifdef COCO3
    hz = hz_n > 2500 ? 50 : 60;
#else
    hz = hz_n > 1250 ? 50 : 60;
#endif
}

void plat_init(void)
{
    const char *err = bank_init();
    if (err) {
        boot_text(err);
        for (;;)
            ;
    }
#ifdef COCO3
    *(volatile uint8_t *)0xFFD9 = 0;    /* 1.79 MHz (HDB-DOS has it on already) */
#endif
    inp_hook = inp_hook_dir = inp_hook_btn = inp_hook_key = 0;   /* (fixed.asm: no bss) */
    hz_measure();
    video_init();
    snd_init();
    input_init();
    if (coco_asked != COCO_ASKED) {     /* power-on: the colours not known yet */
        coco_swap = 0;
#ifndef TEST_SCENES                     /* (the rigs set the colours themselves) */
        video_question();
        coco_asked = COCO_ASKED;        /* (the other program knows: fixed.asm) */
#endif
    }
#ifdef VIDEO_CARD
    for (;;)                            /* M0: the test card, the clock running */
        ;
#endif
}

void plat_quit(void)
{
    asm {
        orcc    #$50
        clr     $0071           ; cold start
#ifndef COCO3
        sta     $FFDE           ; ROM back in
#endif
        jmp     [$FFFE]
    }
}

/* The server's round timer says what real time did: each second it ticks,
 * the jiffies counted since are set against the field rate (hz), and what is missing goes
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
            d = drift + hz - (int16_t)(frame_count - tm_at);
            drift = d > 90 ? 90 : d < -90 ? -90 : (int8_t)d;
        }
        tm_last = t;
        tm_at = frame_count;
    }
    if (drift > 0) {
        asm { orcc #$10 }           /* the IRQ changes it too */
        ++frame_count;
        asm { andcc #$EF }
        --drift;
    }
}

void frame_wait(void)
{
    static uint16_t seen;
    static uint8_t skipped;     /* passes since the last drawn */
    uint16_t d;
    clock_check();
    d = frame_count - seen;
    if (d > CATCH_UP) {         /* a stall (a disk read, an appkey): let it go */
        seen = frame_count - 1;
        d = 1;
    }
    seed += frame_count;
    snd_tick();
    if (d >= 2 && skipped < MAX_SKIP) {
        ++skipped;
        ++seen;                 /* behind: a pass with no drawing */
        return;
    }
    skipped = 0;
    snd_idle((uint8_t)seen);    /* the wait for the jiffy, with the sound in it */
    while (frame_count == seen)
        ;
    ++seen;
    video_frame();
}

uint8_t plat_rand(void)
{
    /* xorshift, stirred by the frame count */
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return (uint8_t)seed;
}

uint8_t plat_hz(void)
{
    return hz;
}

/* ---- the menu and the game: two programs (src/coco/chain.asm) --------- */
uint8_t chain(const char *name);

#ifdef COCO3
#define MENU_FILE "BMENU3  "
#define GAME_FILE "BARRAGE3"
#else
#define MENU_FILE "BMENU   "
#define GAME_FILE "BARRAG2 "     /* (BARRAGE.BIN is the launcher: launch.asm) */
#endif

/* the menu (main.c): it doesn't come back */
uint8_t xload(void)
{
    nst_close();
    return chain(MENU_FILE);
}

/* "boot the disk again": the game, which goes to the menu if no room is set */
void plat_reboot(void)
{
    nst_close();
    chain(GAME_FILE);
    plat_quit();                /* (not on this disk) */
}
