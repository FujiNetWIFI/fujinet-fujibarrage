/* cga.h -- the CGA build's screen (src/msdos/cga) */
#ifndef CGA_H
#define CGA_H

#include <stdint.h>

void cga_palette(uint8_t bg, uint8_t set, uint8_t bright);

/* scra.asm: a text cell (text.c) */
void cga_cell(uint16_t off, const uint8_t *glyph, const uint8_t *tab);
#pragma aux cga_cell "*_" parm [ax] [dx] [bx] modify [ax cx dx si di es]

#endif
