"""astroart.py -- FujiBarrage's Astrocade colours in one place, for
tools/astro/mklook_astro.py (the ROM's tables) and the previews: the skies
(a colour a band, 14 bands down the field), the dirt (a colour a round, ST's
palette), the seats' tanks, the fire's cycle and the HUD. A colour is a
register byte, hue<<3 | luma (tools/astro/astropal.py shows them as MAME
does). The seats are the Intellivision's (tools/intvart.py), the skies the
Atari's kinds (src/atari/dlc.s)."""

NFB = 14        # field bands (equ.inc)

# each sky (SKY_*: plain, gradient, stars, storm, sunset, cavern, black), a
# colour a band, top first
SKIES = [
    [0xF6] * NFB,                                                           # plain
    [0xD8, 0xE0, 0xE1, 0xE9, 0xEA, 0xF2, 0xF3, 0xEC, 0xF4, 0xED, 0xF5, 0xEE, 0xF6, 0xF7],  # night to day
    [0x00] * 7 + [0x08, 0xD0, 0xD0, 0xD8, 0xD8, 0xE0, 0xE0],                # stars: black above
    [0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04],  # storm
    [0xF2, 0x0A, 0x13, 0x2B, 0x3B, 0x4B, 0x5B, 0x5C, 0x64, 0x65, 0x6D, 0x6E, 0x76, 0x77],  # sunset
    [0x01, 0x01, 0x01, 0x70, 0x70, 0x70, 0x71, 0x71, 0x71, 0x70, 0x70, 0x70, 0x01, 0x01],  # cavern
    [0x00] * NFB,                                                           # black
]
SKY_NAMES = ["plain", "gradient", "stars", "storm", "sunset", "cavern", "black"]

# the dirt (ST's palette 0-7): brown, green, grey, red, gold, snow, purple, olive
DIRT = [0x72, 0x99, 0x04, 0x61, 0x74, 0x06, 0x20, 0x7B]
# the same, lit (a flash)
DIRT_LIT = [0x75, 0x9C, 0x07, 0x65, 0x77, 0x07, 0x24, 0x7F]

# the seats' tanks: red, yellow, white, green, purple, orange, pink,
# yellow-green, rust, sea green (none of the skies' blues, none a dirt's)
SEATS = [0x52, 0x7E, 0x07, 0xA4, 0x13, 0x64, 0x4E, 0x86, 0x6A, 0xC4]
LIGHT = {0x7E, 0x07, 0xA4, 0x64, 0x4E, 0x86, 0xC4}     # black text on them in the HUD

# the fire (colour 3): its cycle while fire is live, and still
FIRE = [0x77, 0x6E, 0x66, 0x5D, 0x5B, 0x5D, 0x66, 0x6E]
FIRE_STILL = 0x76

# the HUD: the left half the turn's seat colour (its text black or white),
# the right half dark grey with white text; the border under the field the
# ground's colour; the bars (power, energy) and their trough
HUD_RIGHT = (0x01, 0x07)
BAR_POWER = 0x76
BAR_ENERGY = 0xA4
BAR_TROUGH = 0x01
