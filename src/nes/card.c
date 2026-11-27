/*
 * card.c -- M0's test card (make nes CARD=1): the field's seven bands of
 * tiles, each cell its own tile showing its band's digit (a band shown
 * from the wrong CHR bank shows the wrong digit), on the four palettes by
 * attribute rows (sky, dirt, fire), the black gap and the HUD's own
 * palettes after the split, nine sprites on one line (the ninth must
 * vanish), one moving, and the clocks: frame_count and the interrupts a
 * frame in the HUD, a cell's tile turned every frame through the queue.
 */
#include <stdint.h>
#include <string.h>
#include "nes.h"
#include "plat.h"

extern const uint8_t font8[];           /* the ART bank (R6, bank 1) */

#define PPUADDR (*(volatile uint8_t *)0x2006)
#define PPUDATA (*(volatile uint8_t *)0x2007)
#define PPUSTAT (*(volatile uint8_t *)0x2002)

static const uint8_t field_pal[16] = {
    0x06, 0x21, 0x17, 0x27,             /* the crust (the backdrop: dark red, so a leak shows); sky, dirt, fire by band */
    0x0F, 0x22, 0x07, 0x28,
    0x0F, 0x12, 0x18, 0x16,
    0x0F, 0x02, 0x08, 0x26,
};
static const uint8_t hud_pal[16] = {
    0x0F, 0x30, 0x28, 0x16,             /* black; white, yellow, red */
    0x0F, 0x21, 0x2A, 0x25,
    0x0F, 0x30, 0x11, 0x19,
    0x0F, 0x27, 0x14, 0x3C,
};
static const uint8_t spr_pal[16] = {
    0x0F, 0x16, 0x2A, 0x12,
    0x0F, 0x28, 0x14, 0x30,
    0x0F, 0x27, 0x1C, 0x24,
    0x0F, 0x19, 0x06, 0x38,
};

static uint8_t tile[16];

static void chr_r0(uint8_t bank)
{
    mmc3_chr((uint16_t)bank << 8);       /* R0 */
}

static void vaddr(uint16_t a)
{
    (void)PPUSTAT;
    PPUADDR = (uint8_t)(a >> 8);
    PPUADDR = (uint8_t)a;
}

/* a glyph (' '..'_') in colour fg on bg, into tile[] */
static void glyph(uint8_t ch, uint8_t fg, uint8_t bg)
{
    uint8_t i, g, m0, m1;
    const uint8_t *p = font8 + (uint8_t)(ch - ' ') * 8;
    for (i = 0; i < 8; ++i) {
        g = p[i];
        m0 = ((fg & 1) ? g : 0) | ((bg & 1) ? (uint8_t)~g : 0);
        m1 = ((fg & 2) ? g : 0) | ((bg & 2) ? (uint8_t)~g : 0);
        tile[i] = m0;
        tile[i + 8] = m1;
    }
}

static void solid(uint8_t c)
{
    memset(tile, (c & 1) ? 0xFF : 0, 8);
    memset(tile + 8, (c & 2) ? 0xFF : 0, 8);
}

static void put_tile(void)
{
    uint8_t i;
    for (i = 0; i < 16; ++i)
        PPUDATA = tile[i];
}

/* a tile through the queue: band bank's tile t (0-127 of its 2K) */
static void q_tile(uint8_t bank, uint8_t t)
{
    uint8_t i, h = qhead;
    uint16_t a = (uint16_t)t * 16;
    while (Q_ROOM() < 20)
        ;
    ppuq[h++] = Q_TILE;
    ppuq[h++] = bank;
    ppuq[h++] = (uint8_t)(a >> 8);
    ppuq[h++] = (uint8_t)a;
    for (i = 0; i < 16; ++i)
        ppuq[h++] = tile[i];
    qhead = h;
}

static const char hud_text[3][33] = {
    "FUJIBARRAGE NES  M0 TEST CARD   ",
    "FRAME 0000   IRQS 00   NES 0000 ",
    "BANDS 0-6 FIELD  7 HUD  GAP ABOV",
};

static const char hex[] = "0123456789ABCDEF";

/* the HUD's cell c of row r: its tile (band 7's tile r*32+c) */
static void hud_char(uint8_t r, uint8_t c, char ch, uint8_t direct)
{
    glyph((uint8_t)ch, 1 + (r & 1), 0);
    if (direct) {
        vaddr((uint16_t)(r * 32 + c) * 16);
        put_tile();
    } else
        q_tile(CHR_BAND(7), r * 32 + c);
}

static void hud_hex(uint8_t r, uint8_t c, uint16_t v, uint8_t n)
{
    while (n--) {
        hud_char(r, c + n, hex[v & 15], 0);
        v >>= 4;
    }
}

void main(void)
{
    uint8_t b, k, c, r, t;
    uint16_t seen;

    /* the display is off (ppu_direct from crt0): the CHR-RAM directly */
    for (b = 0; b < 7; ++b) {
        chr_r0(CHR_BAND(b));
        vaddr(0);
        for (k = 0; k < 3; ++k)
            for (c = 0; c < 32; ++c) {
                /* the band's digit; row k of the band on colour 1 or 2,
                   every 8th column the fire's colour */
                glyph((uint8_t)('0' + b), (c & 7) == 7 ? 3 : 0, 1 + (k == 1));
                put_tile();
            }
        for (t = 0; t < 4; ++t) {   /* T_SKY, T_DIRT, T_CRUST, T_FIRE */
            static const uint8_t col[4] = { 1, 2, 0, 3 };
            solid(col[t]);
            put_tile();
        }
    }
    chr_r0(CHR_BAND(7));            /* the HUD: its text */
    for (r = 0; r < 3; ++r)
        for (c = 0; c < 32; ++c)
            hud_char(r, c, hud_text[r][c], 1);
    vaddr(BAND_CELLS * 16);
    for (t = 0; t < 4; ++t) {
        static const uint8_t col[4] = { 1, 2, 0, 3 };
        solid(col[t]);
        put_tile();
    }
    /* the sprites ($1000: R2-R5 hold banks 16-19): tile 1 a tank-ish block */
    vaddr(0x1000);
    solid(0);
    put_tile();
    for (k = 0; k < 8; ++k)
        tile[k] = k < 3 ? 0x18 : 0x7E, tile[k + 8] = k < 3 ? 0x18 : (k & 1 ? 0x42 : 0x7E);
    put_tile();
    chr_r0(CHR_BAND(0));

    /* the name table: margin, the field's bands, the gap, the HUD */
    vaddr(0x2000);
    for (r = 0; r < 30; ++r)
        for (c = 0; c < 32; ++c) {
            if (r < NT_FIELD)
                t = T_SKY;
            else if (r < NT_GAP) {
                k = r - NT_FIELD;
                b = k / 3;
                t = (uint8_t)((b & 1) * 128 + (k % 3) * 32 + c);
            } else if (r == NT_GAP)
                t = 128 + T_CRUST;
            else if (r < NT_HUD + 3)
                t = (uint8_t)(128 + (r - NT_HUD) * 32 + c);
            else
                t = T_CRUST;
            PPUDATA = t;
        }
    /* the attributes: the field's palettes by rows (two name rows each),
       the HUD's by columns */
    for (r = 0; r < 8; ++r)
        for (c = 0; c < 8; ++c) {
            uint8_t top = r * 2, bot = r * 2 + 1, pt, pb;
            /* attribute row a covers name rows 2a, 2a+1 */
            pt = top < 2 ? 0 : (uint8_t)((top - 2) / 3 > 3 ? 3 : (top - 2) / 3);
            pb = bot < 2 ? 0 : (uint8_t)((bot - 2) / 3 > 3 ? 3 : (bot - 2) / 3);
            if (top == 12) {        /* rows 24 (the field's last) and 26-27 (HUD) */
                pt = 3;
                pb = (c & 3);
            }
            if (top == 14)
                pt = pb = (c & 3);
            PPUDATA = (uint8_t)(pt | pt << 2 | pb << 4 | pb << 6);
        }

    memcpy(pal_bg, field_pal, 16);
    memcpy(pal_hud, hud_pal, 16);
    memcpy(pal_spr, spr_pal, 16);
    pal_spr_dirty = 1;

    /* nine sprites on line 100 (the ninth must vanish) */
    for (k = 0; k < 9; ++k) {
        oam2[k * 4] = 99;
        oam2[k * 4 + 1] = 1;
        oam2[k * 4 + 2] = k & 3;
        oam2[k * 4 + 3] = 40 + k * 20;
    }
    memset(oam2 + 40, 0xF8, 256 - 40);
    oam_next = 0x04;                /* (oam2's page) */

    disp_mode = DM_FIELD;
    ppu_direct = 0;
    seen = nes_frames;
    for (;;) {
        while (nes_frames == seen)
            ;
        seen = nes_frames;
        hud_hex(1, 6, frame_count, 4);
        hud_hex(1, 18, irq_hits, 2);
        hud_hex(1, 28, nes_frames, 4);
        /* band 3's cell 5 of its middle row: a bar turning */
        solid(1);
        k = (uint8_t)(seen & 7);
        tile[k] = 0xFF;
        tile[k + 8] = 0xFF;
        q_tile(CHR_BAND(3), 32 + 5);
        /* a sprite moving under the line of nine (the page shown: a
           test card can race the DMA) */
        oam2[36] = 120;
        oam2[37] = 1;
        oam2[38] = 1;
        oam2[39] = (uint8_t)seen;
    }
}
