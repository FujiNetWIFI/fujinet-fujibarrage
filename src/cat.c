/*
 * cat.c -- the CATALOG (docs/PROTOCOL.md): the server's names for the
 * arsenal, the host's settings and the bots' tiers. We keep it as it came
 * and index it once it is whole (its length and sum are in the STATE).
 */
#include <string.h>
#include "game.h"

uint8_t cat_ok;
#pragma bss-name (push, "HIGH")
static uint16_t item_at[N_ITEMS], set_at[N_SETTINGS], page_at[4], tier_at[N_TIERS];
#pragma bss-name (pop)

/* past a length-prefixed string */
static uint16_t skip(uint16_t at)
{
    return at + 1 + catalog[at];
}

uint8_t cat_check(void)
{
    uint8_t i, k, n;
    uint16_t at, len = st[OFF_CAT_LEN] | st[OFF_CAT_LEN + 1] << 8;
    if (cat_ok)
        return 1;
    if (!len || cat_len != len)
        return 0;
    at = 4;
    n = catalog[3];
    for (i = 0; i < n && i < 4; ++i) {
        page_at[i] = at;
        at = skip(at);
    }
    for (i = 0; i < N_ITEMS; ++i) {
        item_at[i] = at;
        at = skip(at + 6);
    }
    for (k = 0; k < N_SETTINGS; ++k) {
        set_at[k] = at;
        if (catalog[at + 1] == 0) {          /* a choice: label, n, names */
            at = skip(at + 2);
            n = catalog[at++];
            for (i = 0; i < n; ++i)
                at = skip(at);
        } else {                            /* a number: label, min, step, count, decimals */
            at = skip(at + 2) + 6;
        }
    }
    n = catalog[at++];
    for (i = 0; i < n && i < N_TIERS; ++i) {
        tier_at[i] = at;
        at = skip(at);
    }
    cat_ok = at <= len;
    return cat_ok;
}

const uint8_t *cat_item(uint8_t i)
{
    return catalog + item_at[i];
}

const uint8_t *cat_setting(uint8_t k)
{
    return catalog + set_at[k];
}

const uint8_t *cat_tier(uint8_t t)
{
    return catalog + tier_at[t < N_TIERS ? t : 0];
}

const uint8_t *cat_page(uint8_t p)
{
    return catalog + page_at[p & 3];
}

void cat_name(const uint8_t *l, char *out, uint8_t max)
{
    uint8_t n = l[0];
    if (!cat_ok) {
        out[0] = 0;
        return;
    }
    if (n > max)
        n = max;
    memcpy(out, l + 1, n);
    out[n] = 0;
}

#ifndef __APPLE2__                       /* (the Apple's: src/apple2/txt.s) */
static const uint16_t tens16[] = { 10000, 1000, 100, 10 };

/* v in decimal: by subtraction (a 6502 has no divide, and the HUD's
 * numbers come every aim step) */
void numstr(char *out, uint16_t v)
{
    uint8_t i, d, any = 0;
    for (i = 0; i < 4; ++i) {
        for (d = '0'; v >= tens16[i]; ++d)
            v -= tens16[i];
        if (d != '0' || any) {
            *out++ = d;
            any = 1;
        }
    }
    *out++ = '0' + (uint8_t)v;
    *out = 0;
}
#endif

static const uint32_t tens[] = { 1000000000UL, 100000000UL, 10000000UL, 1000000UL, 100000UL,
                                 10000UL, 1000UL, 100UL, 10UL, 1UL };

/* v in decimal (it may be over 65535: cash) */
void num32(char *out, uint32_t v)
{
    uint8_t i, d, any = 0;
    for (i = 0; i < 10; ++i) {
        for (d = 0; v >= tens[i]; ++d)
            v -= tens[i];
        if (d || any || i == 9) {
            *out++ = '0' + d;
            any = 1;
        }
    }
    *out = 0;
}

/* how many values setting k has */
uint8_t cat_count(uint8_t k)
{
    const uint8_t *s = cat_setting(k);
    const uint8_t *p = s + 2 + 1 + s[2];
    return s[1] == 0 ? p[0] : p[4];
}

/* setting k at value v as the host's forms show it */
void cat_value(uint8_t k, uint8_t v, char *out)
{
    const uint8_t *s = cat_setting(k);
    const uint8_t *p;
    uint16_t step;
    uint32_t n;
    uint8_t i, dec, len;
    if (!cat_ok) {
        out[0] = 0;
        return;
    }
    p = s + 2 + 1 + s[2];                   /* past page, type, label */
    if (s[1] == 0) {
        if (v >= p[0])
            v = p[0] - 1;
        ++p;
        for (i = 0; i < v; ++i)
            p += 1 + p[0];
        cat_name(p, out, 16);
        return;
    }
    n = p[0] | p[1] << 8;                   /* min + v steps (cash: past 16 bits) */
    step = p[2] | p[3] << 8;
    dec = p[5];
    for (i = 0; i < v; ++i)
        n += step;
    num32(out, n);
    if (dec) {                              /* e.g. 20 with 2 decimals: 0.20 */
        len = strlen(out);
        while (len <= dec) {
            memmove(out + 1, out, len + 1);
            out[0] = '0';
            ++len;
        }
        memmove(out + len - dec + 1, out + len - dec, dec + 1);
        out[len - dec] = '.';
    }
}
