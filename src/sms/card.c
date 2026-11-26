/*
 * card.c -- the VDP set up for Mode 4 (sms_vdp_init) and, with VIDEO_CARD,
 * M0's test card: the 64 colours, the font, the cart's RAM and the
 * cartridge's mailbox, the clock running.
 */
#include <stdint.h>
#include <string.h>
#include "sms.h"
#include "plat.h"
#include "fujinet-sms.h"

extern const uint8_t font8[];   /* gen/sms/font.asm: ' ' to '_', 8 bytes each */

static const uint8_t regs[11] = {
    0x06,               /* 0: Mode 4, no line interrupt, column 0 shown */
    R1_OFF,             /* 1: blank, the frame interrupt on, 8x8 sprites */
    0xFF,               /* 2: the names at 3800h (the SMS1 wants the low bits set) */
    0xFF, 0xFF,         /* 3, 4: (all set, as the SMS1 wants) */
    0xFF,               /* 5: the sprite table at 3F00h */
    0xFB,               /* 6: sprite tiles 0-255 */
    0x00,               /* 7: the border: the sprites' colour 0 */
    0x00, 0x00,         /* 8, 9: no scroll */
    0xFF,               /* 10: (no line interrupt) */
};

void sms_vdp_init(void)
{
    uint8_t r;
    for (r = 0; r < 11; ++r)
        vdp_reg(r, regs[r]);
    va_addr = 0;                /* all of the VRAM: nothing */
    va_len = 0x4000;
    va_val = 0;
    va_fill();
    va_addr = VR_SAT;           /* no sprites */
    va_len = 1;
    va_val = 0xD0;
    va_fill();
}

#ifdef VIDEO_CARD
/* a glyph's tile: ink colour i on ground g (4 planes a line) */
static void glyph_tile(uint16_t tile, uint8_t ch, uint8_t ink, uint8_t ground)
{
    static uint8_t t[32];
    const uint8_t *f = font8 + (uint16_t)(ch - ' ') * 8;
    uint8_t y, p, b, row, i;
    for (y = 0; y < 8; ++y) {
        row = f[y];
        for (p = 0; p < 4; ++p) {
            b = 0;
            if (ink & (1 << p))
                b |= row;
            if (ground & (1 << p))
                b |= (uint8_t)~row;
            t[y * 4 + p] = b;
        }
    }
    va_addr = tile * 32;
    va_src = t;
    va_len = 32;
    va_write();
    (void)i;
}

static void name_at(uint8_t row, uint8_t col, uint16_t word)
{
    static uint8_t w[2];
    w[0] = (uint8_t)word;
    w[1] = word >> 8;
    va_addr = VR_NAMES + row * 64 + col * 2;
    va_src = w;
    va_len = 2;
    va_write();
}

/* text at row, col in the font's tiles 0-63 (white on black) */
static void text(uint8_t row, uint8_t col, const char *s)
{
    while (*s)
        name_at(row, col++, (uint8_t)(*s++ - ' '));
}

static void hex4(uint8_t row, uint8_t col, uint16_t v)
{
    static char b[5];
    uint8_t i, d;
    for (i = 0; i < 4; ++i) {
        d = (v >> (12 - i * 4)) & 15;
        b[i] = d < 10 ? '0' + d : 'A' + d - 10;
    }
    b[4] = 0;
    text(row, col, b);
}

extern uint8_t ram_ok;

void sms_card(void)
{
    static uint8_t pal[32];
    uint8_t i, c;
    for (c = 0; c < 64; ++c)        /* the font, white on black */
        glyph_tile(c, ' ' + c, 15, 0);
    for (i = 0; i < 16; ++i)        /* tiles 64-79: solid colour i */
        glyph_tile(64 + i, ' ', 0, i);
    /* palette 0: black, then 15 of the 64; palette 1 the next 16 */
    for (i = 0; i < 32; ++i)
        pal[i] = i == 0 ? 0 : (uint8_t)(i * 2 + (i > 15)) & 63;
    pal[15] = 0x3F;                 /* (white: the text) */
    va_addr = 0;
    va_src = pal;
    va_len = 32;
    va_cram();
    text(1, 2, "FUJIBARRAGE SMS TEST CARD");
    for (i = 0; i < 16; ++i) {      /* palette 0's 16, then palette 1's */
        name_at(4, i * 2, 64 + i);
        name_at(4, i * 2 + 1, 64 + i);
        name_at(5, i * 2, 0x800 | (64 + i));
        name_at(5, i * 2 + 1, 0x800 | (64 + i));
    }
    text(8, 2, "CART RAM");
    text(8, 12, ram_ok ? "32K OK" : "MISSING");
    text(9, 2, "MAILBOX");
    text(9, 12, fuji_sms_present() ? "PRESENT" : "NONE");
    text(10, 2, "HZ");
    text(10, 12, plat_hz() == 50 ? "50" : "60");
    text(12, 2, "FRAMES");
    vdp_reg(1, R1_ON);
    for (;;) {
        hex4(12, 12, frame_count);
        frame_wait();
    }
}
#endif
