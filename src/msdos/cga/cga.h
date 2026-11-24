/* cga.h -- the CGA build's screen (src/msdos/cga) */
#ifndef CGA_H
#define CGA_H

#include <stdint.h>

void cga_palette(uint8_t bg, uint8_t set, uint8_t bright);

/* scra.asm: a text cell (text.c) */
/* n cells of characters from off, through a 16-entry table of the row's colours (scra.asm) */
void cga_run(uint16_t off, const uint8_t *chars, uint16_t n, const uint8_t *tab);
#pragma aux cga_run "*_" parm [ax] [dx] [cx] [bx] modify [ax bx cx dx si di es]
/* n cells, a look's glyphs drawn at seg (text.c, scra.asm) */
void cga_grun(uint16_t off, const uint8_t *chars, uint16_t n, uint16_t seg);
#pragma aux cga_grun "*_" parm [ax] [dx] [cx] [bx] modify [ax bx cx dx si di es]
/* bytes from offset from to offset to in both banks, forward (scra.asm) */
void cga_move(uint16_t to, uint16_t from, uint16_t n);
#pragma aux cga_move "*_" parm [ax] [dx] [cx] modify [ax bx cx si di es]

#endif
