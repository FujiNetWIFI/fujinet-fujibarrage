/*
 * h19.c -- the H19's screen: what the game draws (bg), the sprites over
 * it, what the terminal shows (have), and for each row the span of
 * columns that may differ. h19_flush() composes a row's span (bg with the
 * sprites over it), and sends the cells that differ from have, with as few
 * escape sequences as it can:
 *
 *   - graphics mode stays on (ESC F once): text is upper case, which
 *     graphics mode prints as text, so the only attribute ever switched is
 *     reverse video (ESC p / ESC q);
 *   - the cursor moves the cheapest way: across up to three cells by
 *     sending them again, back by BS, down a row by LF, else ESC Y r c;
 *   - the rest of a row gone blank is ESC K;
 *   - a new screen with much to wipe is ESC E first.
 *
 * At 9600 baud the line takes a byte a millisecond, about 16 a frame. So
 * nothing goes twice: a cell is sent only when it differs from what the
 * terminal shows (M.U.L.E.'s client, short of memory, kept only dirty bits
 * and sent every cell a sprite crossed). A flush puts no more than FILL
 * bytes in the console's ring (h89io.asm), so whatever is queued is on the
 * screen within a few frames; the rows of our own bomber (h19_first) go
 * first, so it answers the keys at once even while flames are still being
 * drawn. A row not finished keeps the rest of its span for the next flush.
 *
 * The 2 MHz Z80 is the other limit: the cell writers (h19cell.asm) and the
 * sending of a row (h19run.asm) are assembly; this file decides what goes.
 */
#include <string.h>
#include "h19.h"
#include "h89.h"

#define ESC     0x1B
#define UNKNOWN 0xFF
#ifndef FILL
#define FILL    96              /* bytes in the ring after a flush, at most */
#endif
#define OPMAX   7               /* one cell's worst: ESC Y r c, ESC p, c */
#define STALE_FOR_ERASE 4
#define WIPE_FOR_CLEAR 64       /* cells to blank that make ESC E worth it */
#define ERASE_SPAN 12           /* a span this wide may end in ESC K */

uint8_t bg[H19_ROWS][H19_COLS];
static uint8_t have[H19_ROWS][H19_COLS];
uint8_t *rowp[H19_ROWS];                   /* (h19cell.asm reads these) */
static uint8_t *havep[H19_ROWS];
uint8_t d_lo[H19_ROWS], d_hi[H19_ROWS];       /* the span that may differ: lo..hi-1 */
uint8_t any_dirty;
uint8_t line[H19_COLS];                    /* (h19run.asm reads these) */
uint8_t sw_x, sw_hi, *sw_h;
uint8_t cur_x, cur_y, cur_rev;          /* (h19run.asm moves them) */
static uint8_t new_screen, active, rr;
int8_t budget;                          /* (h19run.asm spends it) */
uint8_t h89_idle;
uint16_t h89_flushes;
uint16_t h89_sweeps, h89_cells, h89_queued;     /* (counted for a rig) */

/* Columns a..b-1 of row y may differ. */
static void span(uint8_t y, uint8_t a, uint8_t b)
{
    if (a < d_lo[y])
        d_lo[y] = a;
    if (b > d_hi[y])
        d_hi[y] = b;
    any_dirty = 1;
}

static void out(uint8_t c)
{
    con_put(c);
    --budget;
}

static void esc(uint8_t c)
{
    con_put(ESC);
    con_put(c);
    budget -= 2;
}

static void setrev(uint8_t r)
{
    if (r != cur_rev) {
        cur_rev = r;
        esc(r ? 'p' : 'q');
    }
}

/* Sent as they are. */
static void send_all(const char *s)
{
    while (*s)
        con_put((uint8_t)*s++);
}

static void all_spans(uint8_t y1)
{
    uint8_t y;
    for (y = 0; y <= y1; ++y) {
        d_lo[y] = 0;
        d_hi[y] = H19_COLS;
    }
    any_dirty = 1;
}

/* The modes: the power-up ones (ESC z: no hold screen...), then, once the
 * terminal is back (settle), no reverse, cursor off, no key click, no LF
 * after CR nor CR after LF, discard at the end of a line (never wrap),
 * keypad shifted (its arrows are ESC A-D), the 25th line on; lines 1-24
 * cleared, and the 25th; graphics mode, for good. */
static const char init_seq[] =
    "\033q\033x5\033x2\033y8\033y9\033w\033t\033x1\033E\033Y8 \033l\033F";
/* Graphics off, the 25th line off (which erases it), the DIP switches'
 * modes and a clear screen. */
static const char exit_seq[] = "\033G\033q\033y1\033z";

/* ESC z gone out and done: a real H19 resetting drops what comes meanwhile
 * (the ESC x5 after it was lost on a real H89: the cursor showed). The
 * emulator's takes it all at once. */
static void settle(void)
{
    uint16_t t;
    while (!con_flushed())
        con_pump();
    t = ticks_now();
    while ((uint16_t)(ticks_now() - t) < 125)       /* 250 ms */
        ;
}

void h19_init(void)
{
    uint8_t y;
    for (y = 0; y < H19_ROWS; ++y) {
        rowp[y] = bg[y];
        havep[y] = have[y];
        d_lo[y] = H19_COLS;
        d_hi[y] = 0;
    }
    memset(bg, ' ', sizeof(bg));
    memset(have, ' ', sizeof(have));
    send_all("\033z");
    settle();
    send_all(init_seq);
    cur_x = cur_y = UNKNOWN;
    cur_rev = 0;
    active = 1;
}

void h19_exit(void)
{
    if (!active)
        return;
    active = 0;
    send_all(exit_seq);
    settle();
}

void h19_cleared(void)
{
    new_screen = 1;
}

/* Columns a..b-1 of row y were changed by the caller (field.c writes bg). */
void h19_span(uint8_t y, uint8_t a, uint8_t b)
{
    span(y, a, b);
}

/* Rows y..y+n-1 blank. */
void h19_blank(uint8_t y, uint8_t n)
{
    while (n--) {
        if (y < H19_ROWS) {
            memset(bg[y], ' ', H19_COLS);
            span(y, 0, H19_COLS);
        }
        ++y;
    }
}

/* ---- drawing into bg -------------------------------------------------- */
/* (static variables: zsdcc's frame variables cost 19 T-states a touch) */

/* (h19_put, h19_putn, h19_fill: h19cell.asm) */

/* Text: lower case is raised; what graphics mode would show as a graphic
 * ('^' '_' '`' and above) is shown as something like it, or '?'. */
uint8_t h19_char(uint8_t c)
{
    if (c >= 'a' && c <= 'z')
        return c - 32;
    if (c == '_')
        return G_BAR_B;
    if (c == '|')
        return G_VLINE;
    if (c < 0x20 || c > 0x5D)
        return '?';
    return c;
}

void h19_str(uint8_t x, uint8_t y, const char *s, uint8_t attr)
{
    static uint8_t c, *p, a, b;
    static const char *q;
    if (y >= H19_ROWS)
        return;
    p = rowp[y] + x;
    q = s;
    a = UNKNOWN;
    for (; x < H19_COLS && (c = (uint8_t)*q) != 0; ++q, ++x, ++p) {
        if (c < 0x20 || c > 0x5D)       /* (the characters that show as themselves: no call) */
            c = h19_char(c);
        c |= attr;
        if (*p != c) {
            *p = c;
            if (a == UNKNOWN)
                a = x;
            b = x;
        }
    }
    if (a != UNKNOWN)
        span(y, a, b + 1);
}

/* Row y's columns lo..hi-1 as they should look (bg: no sprites here). */
static void compose(uint8_t y, uint8_t lo, uint8_t hi)
{
    memcpy(line + lo, rowp[y] + lo, hi - lo);
}

/* ---- sending ---------------------------------------------------------- */
/* (h19run.asm: the cursor moves and the cells) */
uint8_t sw_y, span_lo;
uint8_t h19_run(void);
void h19_goto(uint8_t x) __z88dk_fastcall;

/* Where the row as it should be (line within lo..hi-1, h past it) is blank
 * to its end. */
static uint8_t blank_from(uint8_t lo, uint8_t hi, const uint8_t *h)
{
    static uint8_t i;
    static const uint8_t *p;
    i = H19_COLS;
    for (p = h + H19_COLS; i > hi; --i)
        if (*--p != ' ')
            return i;
    for (p = line + hi; i > lo; --i)
        if (*--p != ' ')
            return i;
    return i;
}

/* Row y's span, while the budget lasts: 0 if it ran out (the rest of the
 * span stays). A wide span (a new screen's, a blast's) whose row ends
 * blank where the terminal shows enough there has that wiped by ESC K
 * first. */
static uint8_t sweep(uint8_t y)
{
    static uint8_t lo, hi, *h, tail, i, n;

    lo = d_lo[y];
    hi = d_hi[y];
    if (lo >= hi) {
        d_lo[y] = H19_COLS;
        d_hi[y] = 0;
        return 1;
    }
    compose(y, lo, hi);
    ++h89_sweeps;
    h89_cells += hi - lo;
    sw_h = h = havep[y];
    sw_y = y;
    span_lo = lo;
    if (hi - lo >= ERASE_SPAN && budget >= OPMAX) {
        tail = blank_from(lo, hi, h);
        if (tail < lo)
            tail = lo;
        if (tail < H19_COLS - 1) {
            for (n = 0, i = tail; i < H19_COLS; ++i)
                if (h[i] != ' ')
                    ++n;
            if (n >= STALE_FOR_ERASE) {
                h19_goto(tail);
                setrev(0);
                esc('K');
                memset(h + tail, ' ', H19_COLS - tail);
                if (tail < hi)
                    hi = tail;          /* (the rest is done) */
            }
        }
    }
    sw_x = lo;
    sw_hi = hi;
    if (!h19_run()) {
        d_lo[y] = sw_x;
        d_hi[y] = hi;
        return 0;
    }
    d_lo[y] = H19_COLS;
    d_hi[y] = 0;
    return 1;
}

/* A new screen: if much of the old one must be wiped, ESC E does it. */
static void maybe_clear(void)
{
    static uint16_t wipe;
    static uint8_t y, x, *b, *h;
    new_screen = 0;
    wipe = 0;
    for (y = 0; y < ROW25; ++y) {
        b = rowp[y];
        h = havep[y];
        for (x = d_lo[y]; x < d_hi[y]; ++x)
            if (h[x] != ' ' && b[x] == ' ')
                ++wipe;
    }
    if (wipe >= WIPE_FOR_CLEAR) {
        setrev(0);
        esc('E');
        cur_x = cur_y = 0;
        memset(have, ' ', ROW25 * H19_COLS);
        all_spans(ROW25 - 1);
    }
}

/* ---- scrolling ---------------------------------------------------------- */

/* A region of rows scrolled a row (the shop's list): the terminal does it
 * itself (delete a line, insert one: ten bytes), where a redraw would send
 * the whole list. h19_scroll is called with bg already scrolled; the
 * terminal's side is done at the next flush, before anything is sent, and
 * have follows, so what remains to send is only what differs. */
#ifdef H89_NO_SCROLL
/* (the HDOS build has no room for it: the rows are sent as they are) */
void h19_scroll(uint8_t row, uint8_t n, uint8_t up)
{
    static uint8_t y;
    (void)up;
    for (y = row; y < row + n && y < ROW25; ++y)
        span(y, 0, H19_COLS);
}
#define sc_cnt 0
static void do_scrolls(void)
{
}
#else
#define SC_MAX 4
static uint8_t sc_row[SC_MAX], sc_n[SC_MAX], sc_up[SC_MAX], sc_cnt;

void h19_scroll(uint8_t row, uint8_t n, uint8_t up)
{
    static uint8_t y;
    for (y = row; y < row + n && y < ROW25; ++y)
        span(y, 0, H19_COLS);
    if (!active || sc_cnt >= SC_MAX)
        return;                 /* (the rows are marked: sent as they are) */
    sc_row[sc_cnt] = row;
    sc_n[sc_cnt] = n;
    sc_up[sc_cnt] = up;
    ++sc_cnt;
}

/* the terminal's cursor to a row, column 0, then ESC M (delete the line:
 * those below move up) or ESC L (insert one: they move down) */
static void line_op(uint8_t y, uint8_t op)
{
    static uint8_t i;
    setrev(0);
    con_put(ESC);
    con_put('Y');
    con_put(' ' + y);
    con_put(' ');
    con_put(ESC);
    con_put(op);
    budget -= 6;
    cur_x = 0;
    cur_y = y;
    (void)i;
}

static void do_scrolls(void)
{
    static uint8_t k, y, r, n;
    for (k = 0; k < sc_cnt; ++k) {
        r = sc_row[k];
        n = sc_n[k];
        if (sc_up[k]) {
            line_op(r, 'M');                    /* the rows below move up ... */
            line_op(r + n - 1, 'L');            /* ... and the ones past the region back */
            for (y = r; y + 1 < r + n; ++y)
                memcpy(have[y], have[y + 1], H19_COLS);
            memset(have[r + n - 1], ' ', H19_COLS);
        } else {
            line_op(r + n - 1, 'M');
            line_op(r, 'L');
            for (y = r + n - 1; y > r; --y)
                memcpy(have[y], have[y - 1], H19_COLS);
            memset(have[r], ' ', H19_COLS);
        }
    }
    sc_cnt = 0;
}

#endif

void h19_flush(void)
{
    static uint8_t y, k, more;

    if (!active)
        return;
    ++h89_flushes;
    con_pump();
    budget = (int8_t)(FILL - (TX_RING - 1 - con_free()));
    h89_queued += (uint8_t)(TX_RING - 1 - con_free());
    if (sc_cnt && budget >= 4 * 6)
        do_scrolls();
    if (new_screen)
        maybe_clear();
    more = 0;
    if (any_dirty) {
        for (k = 0; k < H19_ROWS; ++k) {
            y = rr + k;
            if (y >= H19_ROWS)
                y -= H19_ROWS;
            if (d_lo[y] >= d_hi[y])
                continue;
            if (budget < OPMAX || !sweep(y)) {
                rr = y;
                more = 1;
                goto done;
            }
        }
        any_dirty = 0;
    }
done:
    h89_idle = !more && con_flushed();
}

void h19_repaint(void)
{
    if (!active)
        return;
    setrev(0);
    send_all("\033E\033Y8 \033l");
    cur_x = cur_y = UNKNOWN;
    memset(have, ' ', sizeof(have));
    all_spans(H19_ROWS - 1);
}

void h19_bell(void)
{
    con_put(7);
}
