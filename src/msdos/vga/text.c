/*
 * text.c -- the VGA build's text cells (text.h): each glyph line's lit
 * pixels its ink's colour and the rest its ground's, a ramp of 8 for each
 * (scra.asm vga_cell), so letters and rows are shaded top to bottom. The
 * ramps are the DAC's V_UI block (vga.h), loaded by txt_show; while a
 * text screen is drawn they are black.
 */
#include <string.h>
#include <i86.h>
#include <dos.h>
#include "scr.h"
#include "plat.h"
#include "text.h"
#include "vga.h"

static uint8_t ramp[8][8];          /* V_UI's ramps as indices: ramp[k][line] */
static uint8_t seat_g[8];
static uint8_t black8[8];
static uint8_t ui_dac[64 * 3];      /* (6-bit RGB) */
static uint8_t made;

/* a ramp of 8 from (r0,g0,b0) at the top to (r1,g1,b1) at the bottom */
static void ramp_dac(uint8_t k, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1)
{
    uint8_t i, *d = ui_dac + k * 24;
    for (i = 0; i < 8; ++i) {
        *d++ = (uint8_t)(r0 + (r1 - r0) * i / 7);
        *d++ = (uint8_t)(g0 + (g1 - g0) * i / 7);
        *d++ = (uint8_t)(b0 + (b1 - b0) * i / 7);
        ramp[k][i] = (uint8_t)(V_UI + k * 8 + i);
    }
}

static void make(void)
{
    made = 1;
    ramp_dac(0, 63, 63, 63, 44, 48, 56);    /* text ink: white to blue-grey */
    ramp_dac(1, 4, 8, 30, 2, 4, 18);        /* blue ground */
    ramp_dac(2, 44, 8, 8, 28, 4, 4);        /* red ground */
    ramp_dac(3, 28, 28, 32, 16, 16, 20);    /* grey ground */
    ramp_dac(4, 63, 56, 16, 56, 40, 4);     /* yellow ground */
    ramp_dac(5, 36, 40, 52, 24, 28, 40);    /* dim ink */
    ramp_dac(6, 63, 63, 40, 63, 44, 8);     /* title ink: cream to amber */
    ramp_dac(7, 6, 6, 8, 2, 2, 3);          /* the HUD's ground: near black */
}

/* TEXT, TITLE, HEAD, HILITE, DIM: ink and ground ramps */
static const uint8_t pal_ink[] = { 0, 6, 0, 0xFF, 5 };     /* (0xFF: black) */
static const uint8_t pal_ground[] = { 1, 2, 3, 4, 1 };

static void colours(uint8_t row, const uint8_t **ink, const uint8_t **ground)
{
    uint8_t p = txt_pal[row];
    if (p == TP_HUD) {
        *ink = ramp[0];
        *ground = ramp[7];
    } else if (p >= PAL_SEAT) {
        memset(seat_g, V_SEAT + (p - PAL_SEAT) * 3, 8);
        *ink = black8;
        *ground = seat_g;
    } else {
        *ink = pal_ink[p] == 0xFF ? black8 : ramp[pal_ink[p]];
        *ground = ramp[pal_ground[p]];
    }
}

/* A row's ink for a kind of cell (text.h): a label's yellow (on the text,
 * title, heading and HUD looks; the others' labels plain), a name's its
 * seat's body colour. (Labels yellow, values white, names in their seat's
 * colour: the user, 2026-11-23.) */
static uint8_t seat_i[8];

static uint8_t lab_ink(uint8_t p)
{
    return p == PAL_TEXT || p == PAL_TITLE || p == PAL_HEAD || p == TP_HUD;
}

static void colours_k(uint8_t row, uint8_t kind, const uint8_t **ink, const uint8_t **ground)
{
    colours(row, ink, ground);
    if (kind == INK_LABEL && lab_ink(txt_pal[row]))
        *ink = ramp[4];
    else if (kind >= INK_SEAT) {
        memset(seat_i, V_SEAT + (kind - INK_SEAT) * 3, 8);
        *ink = seat_i;
    }
}

/* vga_cell's tables: for an ink and a ground (8 lines each), each line's
 * 16 nibbles as 4 pixels; the last few kept (a row's look, its inverse,
 * the next row's), the one used last tried first */
#define SLOTS 4
static uint8_t vt[SLOTS][512], vt_key[SLOTS][16], vt_ok[SLOTS], vt_last, vt_next;

static const uint8_t *table(const uint8_t *ink, const uint8_t *ground)
{
    uint8_t s, l, n, x, i, g, *t;
    for (n = 0, s = vt_last; n < SLOTS; ++n, s = (uint8_t)((s + 1) & (SLOTS - 1)))
        if (vt_ok[s] && !memcmp(vt_key[s], ink, 8) && !memcmp(vt_key[s] + 8, ground, 8)) {
            vt_last = s;
            return vt[s];
        }
    s = vt_next;
    vt_next = (uint8_t)((s + 1) & (SLOTS - 1));
    vt_ok[s] = 1;
    memcpy(vt_key[s], ink, 8);
    memcpy(vt_key[s] + 8, ground, 8);
    for (t = vt[s], l = 0; l < 8; ++l) {
        i = ink[l];
        g = ground[l];
        for (n = 0; n < 16; ++n)
            for (x = 8; x; x >>= 1)
                *t++ = n & x ? i : g;
    }
    vt_last = s;
    return vt[s];
}

static void nib_cells(uint8_t row, uint8_t col, uint8_t n, uint8_t inv, uint8_t ink);
static uint8_t last_pal = 0xFE, last_inv, last_ink; /* (nib_cells: a row like the last, its table) */
static const uint8_t *last_tab;

/* The text's own looks (TEXT, TITLE, HEAD, HILITE, DIM and the HUD's),
 * and their labels', with all 96 glyphs drawn, 6K each (far, each got at
 * first use): a cell is then 32 word moves (vga_grun), a third of the
 * nibbles' time. Seat rows, names and inverse cells keep the nibbles.
 * (TODO 2026-11-23.) */
#define LOOKS (PAL_SEAT + 1)            /* (the last: the HUD's) */
static uint16_t gseg[LOOKS * 2];
static uint8_t gfail;

static uint16_t glyphs(uint8_t k, uint8_t row, uint8_t kind)
{
    const uint8_t *ink, *ground, *t, *g;
    uint8_t __far *d;
    uint8_t b, l;
    uint16_t c;
    unsigned seg;
    if (gseg[k] || gfail)
        return gseg[k];
    if (_dos_allocmem(384, &seg)) {
        gfail = 1;
        return 0;
    }
    colours_k(row, kind, &ink, &ground);
    t = table(ink, ground);
    last_pal = 0xFE;                    /* (nib_cells's table may be this one's slot now) */
    d = MK_FP(seg, 0);
    for (g = font8, c = 0; c < 96 * 8; ++c) {
        b = *g++;
        l = (uint8_t)(c & 7);
        _fmemcpy(d, t + l * 64 + (b >> 4) * 4, 4);
        _fmemcpy(d + 4, t + l * 64 + (b & 15) * 4, 4);
        d += 8;
    }
    return gseg[k] = seg;
}

void txt_cells(uint8_t row, uint8_t col, uint8_t n, uint8_t inv, uint8_t ink)
{
    uint8_t p = txt_pal[row], k;
    uint16_t seg;
    if (!made)
        make();
    if (ink == INK_LABEL && !lab_ink(p))
        ink = INK_PLAIN;
    if (!inv && ink <= INK_LABEL && (p < PAL_SEAT || p == TP_HUD)) {
        k = (uint8_t)((p == TP_HUD ? PAL_SEAT : p) + (ink == INK_LABEL ? LOOKS : 0));
        seg = glyphs(k, row, ink);
        if (seg) {
            vga_grun((uint16_t)row * 2560 + col * 8, scr_ch[row] + col, n, seg);
            return;
        }
    }
    nib_cells(row, col, n, inv, ink);
}

static void nib_cells(uint8_t row, uint8_t col, uint8_t n, uint8_t inv, uint8_t ink)
{
    const uint8_t *in, *ground;
    uint8_t p = txt_pal[row];
    if (p != last_pal || inv != last_inv || ink != last_ink || p >= PAL_SEAT || ink >= INK_SEAT) {
        colours_k(row, ink, &in, &ground);
        last_tab = inv ? table(ground, in) : table(in, ground);
        last_pal = p;
        last_inv = inv;
        last_ink = ink;
    }
    vga_run((uint16_t)row * 2560 + col * 8, scr_ch[row] + col, n, last_tab);
}

void txt_blank(uint8_t row)
{
    const uint8_t *ink, *ground;
    uint8_t k;
    if (!made)
        make();
    colours(row, &ink, &ground);
    for (k = 0; k < 8; ++k)
        _fmemset(MK_FP(0xA000, (uint16_t)row * 2560 + k * 320), ground[k], 320);
}

void txt_begin(void)
{
    static uint8_t dark[64 * 3];
    if (!made)
        make();
    vga_dac(V_UI, 64, dark);            /* (drawn in the dark) */
}

void txt_show(void)
{
    vga_dac(V_UI, 64, ui_dac);
}

#ifdef MENU
/* the title (menus.c): tools/msdos/mktitle_dos.py's picture, drawn in the
 * dark, then its 192 colours; ESC: LOBBY is in it */
extern const uint8_t title_vga[], title_vga_dac[];

void scr_picture(void)
{
    static uint8_t dark[192 * 3];
    uint8_t r;
    vga_dac(0, 192, dark);
    lz_unpack(0xA000, title_vga);
    frame_wait();
    vga_dac(0, 192, title_vga_dac);
    for (r = 0; r < TXT_ROWS; ++r)
        txt_pal[r] = 0xFF;              /* (the rows aren't text now) */
    memset(scr_ch, 0, sizeof(scr_ch));  /* (nor their cells: scr_text draws what it's given) */
}
#endif
