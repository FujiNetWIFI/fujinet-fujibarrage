/*
 * scr.c -- the NES's text screens (scr.h): 32 columns by 24 rows (name
 * rows 3-26) of the Namco font in CHR-RAM's font table (banks 20-23, both
 * of the background's halves): a glyph (' ' to '_') in four forms, ink 1,
 * 2 or 3 on the backdrop, and ink 1's inverse (tile = glyph + 64 * form).
 *
 * A row's colour is its attribute pair's palette and the ink its letters
 * are drawn in (pal_of: PAL_TEXT white, labels yellow; a seat's its own
 * colour; headings a white bar). Two rows share an attribute pair (16
 * lines): the screens give a seat's row a pair to itself, and a row's
 * palette is the last scr_row's for its pair.
 *
 * The screen isn't kept in RAM (there is none to spare): a row inked again
 * (scr_row), cells inverted (scr_inverse) and rows moved (scr_scroll) are
 * the queue's reads of the name table (crt0.s's Q_MAP, Q_MOVE). While the
 * display is off (scr_begin to scr_show) the PPU is written directly and
 * the queue drained at once; while it shows, the interrupts drain it.
 */
#include <string.h>
#include "nes.h"
#include "scr.h"
#include "plat.h"
#include "layout.h"

#define ROWS 24
#define COLS 32
#define NT_ROW(r) (0x2000 + (uint16_t)((r) + NT_TEXT) * 32)

#define PPUADDR (*(volatile uint8_t *)0x2006)
#define PPUDATA (*(volatile uint8_t *)0x2007)
#define PPUSTAT (*(volatile uint8_t *)0x2002)

extern const uint8_t font8[];           /* R7's bank (gen/nes/font.s) */
extern const uint8_t spr_chr[];         /* the ART bank (gen/nes/spr.s) */
#define ART_BANK 1

/* the text's colours: palette 0 white, yellow (labels), grey; 1-3 the
 * seats' but seat 1's (yellow: palette 0's) */
static const uint8_t text_pal[16] = {
    0x0F, 0x30, 0x28, 0x10,
    0x0F, 0x16, 0x2C, 0x2A,             /* red, cyan, green */
    0x0F, 0x24, 0x27, 0x21,             /* magenta, orange, sky */
    0x0F, 0x35, 0x3A, 0x13,             /* pink, lime, violet */
};
/* a seat's palette and ink */
static const uint8_t seat_pal[10] = { 1, 0, 1, 1, 2, 2, 2, 3, 3, 3 };
static const uint8_t seat_ink[10] = { 1, 2, 2, 3, 1, 2, 3, 1, 2, 3 };

static uint8_t row_form[ROWS];          /* a row's ink (form 0-2), its labels' */
static uint8_t row_lform[ROWS];
static uint8_t attr[64];                /* the attribute table's copy */
static uint8_t direct;                  /* the display is off: write the PPU */

/* ---- the PPU: directly or queued ---- */

static void q_wait(uint8_t n)
{
    while (Q_ROOM() < n)
        ;
}

/* n bytes from p to VRAM address a */
static void put(uint16_t a, const uint8_t *p, uint8_t n)
{
    uint8_t h;
    if (direct) {
        (void)PPUSTAT;
        PPUADDR = (uint8_t)(a >> 8);
        PPUADDR = (uint8_t)a;
        while (n--)
            PPUDATA = *p++;
        return;
    }
    q_wait(n + 4);
    h = qhead;
    ppuq[h++] = n;
    ppuq[h++] = Q_NOBANK;
    ppuq[h++] = (uint8_t)(a >> 8);
    ppuq[h++] = (uint8_t)a;
    while (n--)
        ppuq[h++] = *p++;
    qhead = h;
}

#define Q_READ 0x80             /* (crt0.s: its "bank" says which) */
#define Q_MAP  1
#define Q_MOVE 2

/* n cells at a: their tiles t become (t & 63) | map[t >> 6] */
static void q_map(uint16_t a, uint8_t n, const uint8_t *map)
{
    uint8_t h, i;
    q_wait(9);
    h = qhead;
    ppuq[h++] = Q_READ;
    ppuq[h++] = Q_MAP;
    ppuq[h++] = (uint8_t)(a >> 8);
    ppuq[h++] = (uint8_t)a;
    ppuq[h++] = n;
    for (i = 0; i < 4; ++i)
        ppuq[h++] = map[i];
    qhead = h;
    if (direct)
        ppu_drain(255);
}

/* n bytes of VRAM from s to a */
static void q_move(uint16_t a, uint16_t s, uint8_t n)
{
    uint8_t h;
    q_wait(7);
    h = qhead;
    ppuq[h++] = Q_READ;
    ppuq[h++] = Q_MOVE;
    ppuq[h++] = (uint8_t)(a >> 8);
    ppuq[h++] = (uint8_t)a;
    ppuq[h++] = n;
    ppuq[h++] = (uint8_t)(s >> 8);
    ppuq[h++] = (uint8_t)s;
    qhead = h;
    if (direct)
        ppu_drain(255);
}

/* n bytes of v at a */
static void fill(uint16_t a, uint8_t v, uint8_t n)
{
    uint8_t h;
    if (direct) {
        (void)PPUSTAT;
        PPUADDR = (uint8_t)(a >> 8);
        PPUADDR = (uint8_t)a;
        while (n--)
            PPUDATA = v;
        return;
    }
    q_wait(5);
    h = qhead;
    ppuq[h++] = 0x80 | n;
    ppuq[h++] = Q_NOBANK;
    ppuq[h++] = (uint8_t)(a >> 8);
    ppuq[h++] = (uint8_t)a;
    ppuq[h++] = v;
    qhead = h;
}

/* ---- the font ---- */

static void font_load(void)
{
    uint8_t g, form, i, b, m0, m1, save = r6_bank;
    const uint8_t *p;
    prg_r6(ART_BANK);
    (void)PPUSTAT;
    PPUADDR = 0x00;                     /* R0 at bank 20 (the font), $0000 */
    PPUADDR = 0x00;
    for (form = 0; form < 4; ++form) {
        if (form == 2) {                /* (tiles 128-255: R1's half) */
            (void)PPUSTAT;
            PPUADDR = 0x08;
            PPUADDR = 0x00;
        }
        for (g = 0; g < 64; ++g) {
            p = font8 + g * 8;
            for (i = 0; i < 8; ++i) {   /* plane 0 */
                b = p[i];
                m0 = form == 0 ? b : form == 1 ? 0 : form == 2 ? b : (uint8_t)~b;
                PPUDATA = m0;
            }
            for (i = 0; i < 8; ++i) {   /* plane 1 */
                b = p[i];
                m1 = form == 0 ? 0 : form == 1 ? b : form == 2 ? b : 0;
                PPUDATA = m1;
            }
        }
    }
    prg_r6(save);
}

#define MMC3_SEL  (*(volatile uint8_t *)0x8000)
#define MMC3_BANK (*(volatile uint8_t *)0x8001)

void scr_init(void)
{
    ppu_direct = 1;
    while (!ppu_off)
        ;
    MMC3_SEL = 0;                       /* the font's 4K in R0, R1 */
    MMC3_BANK = CHR_FONT;
    MMC3_SEL = 1;
    MMC3_BANK = CHR_FONT + 2;
    MMC3_SEL = mmc3_sel;
    font_load();
    {                                   /* the sprites' 4K ($1000: R2-R5 hold banks 16-19) */
        uint16_t i;
        uint8_t save = r6_bank;
        prg_r6(ART_BANK);
        (void)PPUSTAT;
        PPUADDR = 0x10;
        PPUADDR = 0x00;
        for (i = 0; i < 4096; ++i)
            PPUDATA = spr_chr[i];
        prg_r6(save);
    }
    memcpy(pal_bg, text_pal, 16);
}

/* ---- rows ---- */

static uint8_t cell(uint8_t row, char c)
{
    uint8_t ch = (uint8_t)c, form = row_form[row];
    if (ch & 0x80) {                    /* a label (SCR_INK) */
        ch &= 0x7F;
        form = row_lform[row];
    }
    if (ch >= 'a' && ch <= 'z')
        ch -= 32;
    if (ch < ' ' || ch > '_')
        ch = ' ';
    return (uint8_t)(ch - ' ' + form * 64);
}


extern uint8_t field_shown;             /* field.c */
extern uint8_t fld_live;                /* fld.s */

void scr_begin(void)
{
    field_shown = 0;                    /* (the field isn't shown: its changes aren't drawn) */
    fld_live = 0;
    direct = 1;
    ppu_direct = 1;
    ppu_off = 0;
    while (!ppu_off)
        ;
    disp_mode = DM_OFF;
    qhead = qtail = 0;                  /* (the queue: nothing left for a screen that's gone) */
    memset(row_form, 0, sizeof(row_form));
    memset(row_lform, 1, sizeof(row_lform));
    memset(attr, 0, sizeof(attr));
    (void)PPUSTAT;
    PPUADDR = 0x20;
    PPUADDR = 0x00;
    {
        uint16_t i;
        for (i = 0; i < 960 + 64; ++i)
            PPUDATA = 0;                /* (tile 0: ' ', ink 1) */
    }
    memcpy(pal_bg, text_pal, 16);
}

void scr_show(void)
{
    direct = 0;
    disp_mode = DM_TEXT;
    ppu_direct = 0;
}

static uint8_t pal_of(uint8_t pal, uint8_t *ink, uint8_t *lab)
{
    *lab = 1;                           /* labels yellow (palette 0's ink 2) */
    switch (pal) {
    case PAL_TEXT:
        *ink = 0;
        return 0;
    case PAL_TITLE:
        *ink = 1;
        return 0;
    case PAL_HEAD:
        *ink = 3;                       /* a white bar */
        *lab = 3;
        return 0;
    case PAL_HILITE:
        *ink = 1;
        return 0;
    case PAL_DIM:
        *ink = 2;
        return 0;
    }
    pal -= PAL_SEAT;
    if (pal > 9)
        pal = 0;
    *ink = seat_ink[pal] - 1;
    *lab = *ink;
    return seat_pal[pal];
}

void scr_row(uint8_t row, uint8_t pal)
{
    uint8_t p, ink, lab, c, a, q, sh, map[4];
    if (row >= ROWS)
        return;
    p = pal_of(pal, &ink, &lab);
    if (row_form[row] != ink || row_lform[row] != lab) {
        for (c = 0; c < 4; ++c)         /* the letters again in the new ink */
            map[c] = (uint8_t)((c == row_lform[row] && row_lform[row] != row_form[row] ? lab : ink) * 64);
        q_map(NT_ROW(row), COLS, map);
        row_form[row] = ink;
        row_lform[row] = lab;
    }
    /* the attribute pair: name row row+3, quadrant row (row+3)/2 */
    q = (uint8_t)((row + NT_TEXT) >> 1);
    sh = (q & 1) ? 4 : 0;
    a = (uint8_t)((q >> 1) * 8);
    for (c = 0; c < 8; ++c) {
        attr[a + c] = (uint8_t)((attr[a + c] & ~(0x0F << sh)) | ((p | p << 2) << sh));
    }
    put(0x23C0 + a, &attr[a], 8);
}

static void text_at(uint8_t row, uint8_t col, const char *s)
{
    uint8_t b[COLS], n = 0;
    if (row >= ROWS)
        return;
    while (*s && col + n < COLS)
        b[n++] = cell(row, *s++);
    if (n)
        put(NT_ROW(row) + col, b, n);
}

void scr_text(uint8_t row, uint8_t col, const char *s)
{
    text_at(row, col, s);
}

void scr_label(uint8_t row, uint8_t col, const char *s)
{
    char b[COLS + 1];
    uint8_t i = 0;
    while (s[i] && i < COLS) {
        b[i] = (char)(s[i] | 0x80);
        ++i;
    }
    b[i] = 0;
    text_at(row, col, b);
}

static const uint8_t inv_map[4] = { 3 * 64, 3 * 64, 3 * 64, 3 * 64 };

void scr_inverse(uint8_t row, uint8_t col, uint8_t n)
{
    if (row >= ROWS || col >= COLS)
        return;
    if (col + n > COLS)
        n = COLS - col;
    if (n)
        q_map(NT_ROW(row) + col, n, inv_map);
}

void scr_clear_row(uint8_t row)
{
    if (row >= ROWS)
        return;
    fill(NT_ROW(row), (uint8_t)(row_form[row] * 64), COLS);
}

/* (a row moved keeps its tiles: its letters' inks come with it) */
void scr_scroll(uint8_t row, uint8_t n, uint8_t up)
{
    uint8_t i;
    if (!n)
        return;
    if (up) {
        for (i = row; i + 1 < row + n; ++i)
            q_move(NT_ROW(i), NT_ROW(i + 1), COLS);
        scr_clear_row(row + n - 1);
    } else {
        for (i = row + n - 1; i > row; --i)
            q_move(NT_ROW(i), NT_ROW(i - 1), COLS);
        scr_clear_row(row);
    }
}

void field_title(void);                 /* src/nes/fieldo.c (bank 7) */

/* the title: a picture on the field's screen (field_title), its words in
 * the HUD's rows */
void scr_picture(void)
{
    field_title();
}
