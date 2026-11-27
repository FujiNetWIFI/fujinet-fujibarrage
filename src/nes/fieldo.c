/*
 * fieldo.c -- the NES's field screen's set-up, HUD and talk (field.h): an
 * overlay (bank 7: tools/nes/far.txt), called from the field's (src/play.c,
 * whose words are in R7's bank so these can read them) through the far
 * wrappers. The rest is src/nes/field.c's.
 *
 * The HUD is white on black, labels yellow, the subject's name in its
 * seat's colour (the HUD's palette 1, ink 3), each cell a tile of band 7
 * written when it changes.
 */
#include <string.h>
#include "nes.h"
#include "game.h"
#include "field.h"
#include "plat.h"
#include "scr.h"
#include "fld.h"
#include "layout.h"

#define PPUADDR (*(volatile uint8_t *)0x2006)
#define PPUDATA (*(volatile uint8_t *)0x2007)
#define PPUSTAT (*(volatile uint8_t *)0x2002)
#define MMC3_SEL (*(volatile uint8_t *)0x8000)
#define MMC3_BANK (*(volatile uint8_t *)0x8001)

uint8_t fld_flush1(void);       /* render.s */
void fld_mark_all(void);        /* fld.s */
void field_palettes(void);      /* field.c */
void pt_clear(void);            /* pt.s */
extern uint8_t rnd_stars;
extern uint8_t bln_cells[21 * 4];
extern const uint8_t font8[];
extern uint8_t band_pal[16], drawn[SEATS], fire_on, flash_n, field_shown;
extern uint8_t fires[3];
/* the fire's three for each dirt: never the dirt's colours (white and
 * yellows over browns and greens, reds over sand and ice) */
static const uint8_t fire_of[8][3] = {
    { 0x30, 0x38, 0x28 },               /* earth */
    { 0x30, 0x38, 0x28 },               /* grass */
    { 0x16, 0x26, 0x28 },               /* rock */
    { 0x30, 0x38, 0x3C },               /* mars */
    { 0x16, 0x06, 0x26 },               /* sand */
    { 0x16, 0x26, 0x27 },               /* ice */
    { 0x28, 0x38, 0x30 },               /* alien */
    { 0x30, 0x31, 0x3C },               /* lava */
};

/* ---- the round's look (SKY_*, the dirt palette) ---- */
static const uint8_t skies[8][4] = {
    { 0x21, 0x21, 0x21, 0x21 },         /* plain */
    { 0x01, 0x11, 0x21, 0x31 },         /* gradient: deep blue to pale */
    { 0x0F, 0x0F, 0x0F, 0x01 },         /* stars: black, a navy horizon */
    { 0x2D, 0x00, 0x10, 0x10 },         /* storm: slate to grey */
    { 0x12, 0x14, 0x15, 0x27 },         /* sunset: blue, purple, magenta, gold */
    { 0x0F, 0x0F, 0x06, 0x16 },         /* cavern: black to a red glow */
    { 0x0F, 0x0F, 0x0F, 0x0F },         /* black */
    { 0x01, 0x11, 0x21, 0x31 },         /* random: as gradient */
};
/* the crust, then the strata light to dark */
static const uint8_t dirts[8][4] = {
    { 0x1A, 0x27, 0x17, 0x07 },         /* earth: grass over ochre, brown */
    { 0x2A, 0x1A, 0x0A, 0x09 },         /* grass */
    { 0x3D, 0x10, 0x00, 0x2D },         /* rock */
    { 0x28, 0x27, 0x16, 0x06 },         /* mars */
    { 0x38, 0x37, 0x27, 0x17 },         /* sand */
    { 0x30, 0x31, 0x21, 0x11 },         /* ice */
    { 0x35, 0x25, 0x14, 0x13 },         /* alien */
    { 0x38, 0x28, 0x27, 0x16 },         /* lava */
};
/* a band palette's dirt stratum */
static const uint8_t stratum[4] = { 1, 1, 2, 3 };

/* the seats' colours as sprites: palette 0 seats 0-2, 1 3-5, 2 6-8, 3 seat
 * 9, white, grey */
static const uint8_t spr_pal[16] = {
    0x0F, 0x16, 0x28, 0x2C,
    0x0F, 0x2A, 0x24, 0x27,
    0x0F, 0x21, 0x35, 0x3A,
    0x0F, 0x13, 0x30, 0x10,
};
static const uint8_t seat_rgb[10] = { 0x16, 0x28, 0x2C, 0x2A, 0x24, 0x27, 0x21, 0x35, 0x3A, 0x13 };
/* the HUD: white, yellow (labels), and palette 0 grey, 1 the subject's
 * colour, 2 red, 3 green */
static const uint8_t hud_pal[16] = {
    0x0F, 0x30, 0x28, 0x10,             /* grey */
    0x0F, 0x30, 0x28, 0x16,             /* rows 0-1's name */
    0x0F, 0x30, 0x28, 0x16,             /* row 2's name */
    0x0F, 0x30, 0x28, 0x2A,             /* green */
};

static uint8_t hud[3][32];              /* the HUD's letters shown (bit 6: inverse, 7: label) */
static uint8_t nm_col[3], nm_n[3];      /* a row's name, for its next write (field_name) */
static uint8_t hud_attr[16];            /* its attribute bytes (rows 26-29's: 8 + 8) */

/* ---- the PPU: directly (the display off) or queued ---- */

static void q_wait(uint8_t n)
{
    while (Q_ROOM() < n)
        ;
}

static void put(uint16_t a, const uint8_t *p, uint8_t n)
{
    uint8_t h;
    if (ppu_direct) {
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

static uint8_t tile[16];

/* a tile into CHR bank b (2K, as R0) at t (0-127) */
static void put_tile(uint8_t b, uint8_t t)
{
    uint8_t h, i;
    uint16_t a = (uint16_t)t * 16;
    if (ppu_direct) {
        MMC3_SEL = 0;
        MMC3_BANK = b;
        MMC3_SEL = mmc3_sel;
        (void)PPUSTAT;
        PPUADDR = (uint8_t)(a >> 8);
        PPUADDR = (uint8_t)a;
        for (i = 0; i < 16; ++i)
            PPUDATA = tile[i];
        return;
    }
    q_wait(20);
    h = qhead;
    ppuq[h++] = Q_TILE;
    ppuq[h++] = b;
    ppuq[h++] = (uint8_t)(a >> 8);
    ppuq[h++] = (uint8_t)a;
    for (i = 0; i < 16; ++i)
        ppuq[h++] = tile[i];
    qhead = h;
}

static void solid(uint8_t c)
{
    memset(tile, (c & 1) ? 0xFF : 0, 8);
    memset(tile + 8, (c & 2) ? 0xFF : 0, 8);
}

/* a letter ' '..'_' in ink fg on 0, or ink fg's inverse */
static void glyph(uint8_t ch, uint8_t fg, uint8_t inv)
{
    uint8_t i, g;
    const uint8_t *p;
    if (ch >= 'a' && ch <= 'z')
        ch -= 32;
    if (ch < ' ' || ch > '_')
        ch = ' ';
    p = font8 + (uint8_t)(ch - ' ') * 8;
    for (i = 0; i < 8; ++i) {
        g = p[i];
        if (inv)
            g = (uint8_t)~g;
        tile[i] = (fg & 1) ? g : 0;
        tile[i + 8] = (fg & 2) ? g : 0;
    }
}

void field_open(uint8_t sky, uint8_t pal)
{
    uint8_t b, t, r, c, a;
    sky &= 7;
    pal &= 7;
    ppu_direct = 1;
    ppu_off = 0;
    while (!ppu_off)
        ;
    disp_mode = DM_OFF;
    qhead = qtail = 0;
    rnd_stars = sky == SKY_STARS;
    memcpy(fires, fire_of[pal], 3);
    for (b = 0; b < 4; ++b) {
        band_pal[b * 4] = b ? 0x0F : dirts[pal][0];     /* (4, 8, 12: black, the gap's parking) */
        band_pal[b * 4 + 1] = skies[sky][b];
        band_pal[b * 4 + 2] = dirts[pal][stratum[b]];
        band_pal[b * 4 + 3] = fires[2];
    }
    fire_on = flash_n = 0;
    field_palettes();
    memcpy(pal_hud, hud_pal, 16);
    memcpy(pal_spr, spr_pal, 16);
    pal_spr_dirty = 1;

    /* every band's constant tiles: sky, dirt, crust, fire, the stars */
    for (b = 0; b < 7; ++b) {
        static const uint8_t col[4] = { 1, 2, 0, 3 };
        for (t = 0; t < 4; ++t) {
            solid(col[t]);
            put_tile(CHR_BAND(b), BAND_CELLS + t);
        }
        for (t = 0; t < 8; ++t) {       /* a star: a fire-coloured pixel in the sky */
            solid(1);
            tile[((t * 3) & 7) + 8] |= (uint8_t)(0x80 >> ((t * 5 + 2) & 7));
            put_tile(CHR_BAND(b), BAND_CELLS + 4 + t);
        }
    }
    /* the HUD: blank */
    solid(0);
    for (t = 0; t < 96; ++t)
        put_tile(CHR_BAND(7), t);
    memset(hud, ' ', sizeof(hud));

    /* the name table: the margin (band 0's sky), the field (its cells,
       drawn below), the gap, the HUD's cells */
    (void)PPUSTAT;
    PPUADDR = 0x20;
    PPUADDR = 0x00;
    for (r = 0; r < 30; ++r)
        for (c = 0; c < 32; ++c) {
            if (r < NT_FIELD)
                t = T_SKY;
            else if (r < NT_GAP)
                t = (uint8_t)((((r - NT_FIELD) / 3) & 1) ? 128 + T_SKY : T_SKY);
            else if (r == NT_GAP)
                t = T_CRUST;            /* (its first line shows: the crust's colour) */
            else if (r < NT_HUD + 3)
                t = (uint8_t)(128 + (r - NT_HUD) * 32 + c);
            else
                t = 0;
            PPUDATA = t;
        }
    /* the attributes: quadrant rows 0-12 the field's bands (3, 3, 3, 4),
       13-14 the HUD's palette 0 */
    for (r = 0; r < 8; ++r) {
        uint8_t q0 = (uint8_t)(r * 2), q1 = (uint8_t)(r * 2 + 1), p0, p1;
        p0 = q0 < 5 ? 0 : q0 < 8 ? 1 : q0 < 11 ? 2 : 3;
        p1 = q1 < 5 ? 0 : q1 < 8 ? 1 : q1 < 11 ? 2 : 3;
        if (q0 >= 13)
            p0 = 0;
        if (q1 >= 13)
            p1 = 0;
        a = (uint8_t)(p0 | p0 << 2 | p1 << 4 | p1 << 6);
        for (c = 0; c < 8; ++c)
            PPUDATA = a;
    }
    memset(hud_attr, 0, sizeof(hud_attr));
    for (c = 0; c < 8; ++c)
        hud_attr[c] = 0x0F;             /* rows 24-25 band 3, 26-27 the HUD's 0 */
    memset(bln_cells, 0, sizeof(bln_cells));
    pt_clear();                         /* (no sparks: their places start at 0, not nowhere) */

    /* the field, all of it */
    fld_live = 1;
    fld_mark_all();
    while (fld_flush1() || fld_nd)
        if (Q_ROOM() < 30)
            ppu_drain(255);
    ppu_drain(255);
    memset(drawn, 0, sizeof(drawn));
    field_shown = 1;
    disp_mode = DM_FIELD;
    ppu_direct = 0;
}

/* ---- the HUD ---- */

static void hud_cell(uint8_t row, uint8_t col, uint8_t ch, uint8_t ink, uint8_t inv)
{
    uint8_t k = (uint8_t)(ch | (inv ? 0x40 : 0) | (ink == 2 ? 0x80 : 0));
    if (row > 2 || col > 31)
        return;
    if (ink == 3)
        k = (uint8_t)(ch | 0xC0);       /* (a name's: drawn again whenever asked) */
    else if (hud[row][col] == k)
        return;
    hud[row][col] = k;
    glyph(ch, ink, inv);
    put_tile(CHR_BAND(7), (uint8_t)(row * 32 + col));
}

static void status(uint8_t row, uint8_t col, const char *s, uint8_t inv)
{
    uint8_t c;
    if (row > 2)
        return;
    while ((c = (uint8_t)*s++) != 0 && col < 32) {
        hud_cell(row, col, c & 0x7F,
                 (uint8_t)(col - nm_col[row] < nm_n[row] ? 3 : (c & 0x80) ? 2 : 1), inv);
        ++col;
    }
    nm_n[row] = 0;                      /* (the name was this write's) */
}

void field_status(uint8_t row, uint8_t col, const char *s)
{
    status(row, col, s, 0);
}

void field_status_hi(uint8_t row, uint8_t col, const char *s)
{
    status(row, col, s, 1);
}

void field_status_clear(void)
{
    uint8_t r, c;
    for (r = 0; r < 3; ++r)
        for (c = 0; c < 32; ++c)
            hud_cell(r, c, ' ', 1, 0);
}

/* a name in its seat's colour: the row's next write draws cells col ..
 * col+n-1 in ink 3, whose colour is the seat's in the HUD's palette 1
 * (rows 0, 1) or 2 (row 2), over those cells (inks 1 and 2 are white and
 * yellow in every HUD palette) */
void field_name(uint8_t row, uint8_t col, uint8_t n, uint8_t seat)
{
    uint8_t i, q, a, sh, p;
    if (seat >= SEATS || row > 2)
        return;
    p = row < 2 ? 1 : 2;
    pal_hud[p * 4 + 3] = seat_rgb[seat];
    nm_col[row] = col;
    nm_n[row] = n;
    /* its quadrants: HUD row r is name row 26 + r: quadrant row 13 (rows
       26-27, an attribute byte's bottom half) or 14 (28-29, the next's top) */
    q = (uint8_t)((NT_HUD + row) >> 1);
    sh = (q & 1) ? 4 : 0;
    for (i = col & ~1; i < col + n && i < 32; i += 2) {
        a = (uint8_t)((q >> 1) - 6) * 8 + (i >> 2);
        sh = (uint8_t)(((q & 1) ? 4 : 0) + ((i & 2) ? 2 : 0));
        hud_attr[a] = (uint8_t)((hud_attr[a] & ~(3 << sh)) | (p << sh));
    }
    put(0x23C0 + 6 * 8, hud_attr, 16);
}

/* ---- talk ---- */

void __fastcall__ fld_touch(uint16_t rc);       /* fld.s: (row << 8 | column) changed */

static uint8_t bl_on, bl_r, bl_c0, bl_w;

/* a balloon's cell: a letter in the crust's colour (0) on the fire's (3),
 * its own tile in its band, the cell the balloon's (bln_cells) */
static void bl_cell(uint8_t r, uint8_t c, uint8_t ch)
{
    uint8_t b = (uint8_t)(r / 3), k = (uint8_t)(r % 3), t, i, g;
    const uint8_t *p;
    if (ch >= 'a' && ch <= 'z')
        ch -= 32;
    if (ch < ' ' || ch > '_')
        ch = ' ';
    p = font8 + (uint8_t)(ch - ' ') * 8;
    for (i = 0; i < 8; ++i) {
        g = (uint8_t)~p[i];
        tile[i] = g;
        tile[i + 8] = g;
    }
    t = (uint8_t)(k * 32 + c);
    bln_cells[r * 4 + (c >> 3)] |= (uint8_t)(0x80 >> (c & 7));
    put_tile(CHR_BAND(b), t);
    t |= (uint8_t)((b & 1) << 7);
    put(0x2000 + (uint16_t)(r + NT_FIELD) * 32 + c, &t, 1);
}

/* the balloon's cells back to the field's */
static void bl_off(void)
{
    uint8_t c;
    if (!bl_on)
        return;
    for (c = bl_c0; c < bl_c0 + bl_w; ++c) {
        bln_cells[bl_r * 4 + (c >> 3)] &= (uint8_t)~(0x80 >> (c & 7));
        fld_touch((uint16_t)bl_r << 8 | c);
    }
    bl_on = 0;
}

/* seat's talk, n letters of s, in a balloon of whole cells over its tank
 * (n 0: the balloon goes) */
void field_balloon(uint8_t seat, const char *s, uint8_t n)
{
    int16_t r;
    uint8_t c, i, w;
    bl_off();
    if (!n || seat >= SEATS || !tk[seat].on)
        return;
    if (n > 30)
        n = 30;
    w = (uint8_t)(n + 2);
    r = (int16_t)(tk[seat].y - TANK_H - 10) / 8 - 1;   /* the row over the tank's barrel */
    if (r < 0)
        r = 0;
    c = (uint8_t)(tk[seat].x >> 3);
    c = c > w / 2 ? (uint8_t)(c - w / 2) : 0;
    if (c + w > 32)
        c = (uint8_t)(32 - w);
    bl_r = (uint8_t)r;
    bl_c0 = c;
    bl_w = w;
    bl_on = 1;
    bl_cell(bl_r, c, ' ');
    for (i = 0; i < n; ++i)
        bl_cell(bl_r, (uint8_t)(c + 1 + i), s[i]);
    bl_cell(bl_r, (uint8_t)(c + w - 1), ' ');
}

/* ---- the title ---- */

extern const uint8_t title_hills[256];  /* gen/nes/title.s */

static void big(uint8_t x, uint8_t y, char ch)  /* a letter twice its size, in fire */
{
    const uint8_t *p = font8 + (uint8_t)(ch - ' ') * 8;
    uint8_t i, j;
    for (i = 0; i < 8; ++i)
        for (j = 0; j < 8; ++j)
            if (p[i] & (0x80 >> j)) {
                fy = y + i * 2;
                fx = x + j * 2;
                fx1 = fx + 1;
                fv = PX_FX;
                fld_hspan();
                fy++;
                fx = x + j * 2;
                fld_hspan();
            }
}

/* the title: a sunset over the hills, FUJIBARRAGE in fire, a shell's arc
 * from one tank to the other and its burst; the words on the HUD's rows
 * (menus.c's title, through scr_picture) */
void field_title(void)
{
    static const char name[] = "FUJIBARRAGE";
    uint16_t x;
    int16_t y, k;
    uint8_t i;
    fld_init();
    fld_clear();
    for (x = 0; x < 256; ++x) {
        fx = x;
        fy = title_hills[x];
        fx1 = FIELD_H;
        fv = PX_DIRT;
        fld_vrun();
    }
    for (i = 0; i < 11; ++i)
        big((uint8_t)(40 + i * 16), 20, name[i]);
    tk[0].on = tk[1].on = 1;            /* two tanks, an arc between them */
    tk[0].x = 44;
    tk[0].y = title_hills[44];
    tk[0].angle = 60;
    tk[1].x = 212;
    tk[1].y = title_hills[212];
    tk[1].angle = 120;
    for (k = 0; k <= 168; k += 2) {     /* y = a parabola over 60 */
        x = (uint16_t)(46 + k);
        y = (int16_t)(60 + ((k - 84) * (k - 84)) / 80);
        if (y < (int16_t)title_hills[x] - 6)
            field_plot((int16_t)x, y, PX_FX);
    }
    field_ellipse(214, (int16_t)title_hills[214] - 8, 12, 9, PX_FX, 0);
    field_open(SKY_SUNSET, 4);
    drawn[0] = drawn[1] = 1;
    field_status(0, 2, TXT_TAGLINE);
    field_status(1, 12, TXT_PRESS);
    field_status(2, 9, QUIT_HINT);
}
