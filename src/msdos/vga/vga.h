/* vga.h -- the VGA build's screen (src/msdos/vga): mode 13h, its DAC */
#ifndef VGA_H
#define VGA_H

#include <stdint.h>

/* the DAC's 256 entries:
 *   0        black (the tanks' treads: the tank class)
 *   1-40     the sky's ramp, top to horizon (the sky class to 63)
 *   48       a star (white on a starry sky)
 *   64-95    the dirt: 8 strata of 4 shades (the dirt class to 127)
 *   128-143  fire (cycled)
 *   144-175  the ten seats (body, shade, highlight; the tank class)
 *   176-191  spare
 *   192-255  the text and the HUD (V_UI): ramps of 8, one a glyph line */
#define V_SKY   1
#define V_NSKY  40
#define V_STAR  48
#define V_DIRT  64
#define V_FIRE  128
#define V_SEAT  144
#define V_UI    192
#define V_INK_TEXT   192
#define V_GND_BLUE   200
#define V_GND_RED    208
#define V_GND_GREY   216
#define V_GND_YELLOW 224
#define V_INK_DIM    232
#define V_INK_TITLE  240
#define V_GND_HUD    248

void vga_dac(uint16_t first, uint16_t n, const uint8_t *tab);

/* scra.asm: a text cell (text.c): ink and ground a glyph line each */
/* n cells of characters from off, through a table of the row's colours
 * (text.c): line k's 4-pixel nibbles at tab[k * 64 + nibble * 4] */
void vga_run(uint16_t off, const uint8_t *chars, uint16_t n, const uint8_t *tab);
#pragma aux vga_run "*_" parm [ax] [dx] [cx] [bx] modify [ax bx cx dx si di es]
/* n cells, a look's glyphs drawn at seg (text.c): 32 word moves a cell */
void vga_grun(uint16_t off, const uint8_t *chars, uint16_t n, uint16_t seg);
#pragma aux vga_grun "*_" parm [ax] [dx] [cx] [bx] modify [ax bx cx dx si di es]

#endif
