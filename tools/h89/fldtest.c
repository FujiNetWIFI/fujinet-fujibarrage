/*
 * fldtest.c -- the field's primitives (src/h89/fld.c) on the host: a long run of
 * random operations, the store hashed after each; two builds of fld.c (an old one
 * and the new one) must print the same lines. With CHECK_MARKS every block an
 * operation changed must lie in a cell fld_cd has marked.
 *
 *   cc -D__H89__ -Isrc -Isrc/platform-specific -Isrc/h89 -DCHECK_MARKS \
 *      tools/h89/fldtest.c src/h89/fld.c -o /tmp/fldtest && /tmp/fldtest SEED N
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fld.h"

static uint32_t rs = 1;
static uint32_t rnd(void)
{
    rs = rs * 1664525u + 1013904223u;
    return rs >> 8;
}

static uint8_t entry[64];

static uint32_t hash(void)
{
    uint32_t h = 2166136261u;
    size_t i;
    for (i = 0; i < sizeof(fld_store); ++i)
        h = (h ^ ((uint8_t *)fld_store)[i]) * 16777619u;
    {
        int cx, cy;                     /* (and the characters the cells show) */
        for (cy = 0; cy < 22; ++cy)
            for (cx = 0; cx < 80; ++cx)
                h = (h ^ fld_quad(cx, cy)) * 16777619u;
    }
    return h;
}

static int val(int a, int b, int c)
{
    return (int)(rnd() % (unsigned)(b - a + 1)) + a + c;
}

int main(int argc, char **argv)
{
    int n = argc > 2 ? atoi(argv[2]) : 1000, k, i, bad = 0;
    static uint8_t prev[sizeof(fld_store)];
    rs = argc > 1 ? (uint32_t)atoi(argv[1]) : 1;
    fld_init();
    for (k = 0; k < n; ++k) {
        int op = rnd() % 9;
        memcpy(prev, fld_store, sizeof(prev));
#ifdef CHECK_MARKS
        memset(fld_cd, 0, sizeof(fld_cd));
#endif
        fv = 1 + rnd() % 3;
        fold = 1 + rnd() % 3;
        switch (op) {
        case 0: fx = val(-5, 165, 0); fy = val(-5, 48, 0); c_fld_plot(); break;
        case 1: fx = val(-5, 165, 0); fy = val(-5, 48, 0); c_fld_swap(); break;
        case 2: fy = val(-3, 46, 0); fx = val(-10, 170, 0); fx1 = fx + val(0, 90, 0); c_fld_hspan(); break;
        case 3: fx = val(-3, 162, 0); fy = val(-3, 48, 0); fx1 = fy + val(0, 50, 0); c_fld_vrun(); break;
        case 4:
            fx = val(0, 159, 0);
            if (rnd() & 1) {
                entry[0] = rnd() % 50;
            } else {
                int m = rnd() % 5;
                entry[0] = 240 + m;
                for (i = 0; i < m; ++i) {
                    int a = rnd() % 44, b = a + rnd() % 20;
                    entry[1 + 2 * i] = a;
                    entry[2 + 2 * i] = b;
                }
            }
            fent = entry;
            c_fld_col();
            break;
        case 5: {
            uint8_t *e = entry;
            fx = val(0, 150, 0);
            fld_ncols = 1 + rnd() % 4;
            for (i = 0; i < fld_ncols; ++i) {
                *e++ = rnd() % 60;
            }
            fent = entry;
            c_fld_cols();
            break;
        }
        default:
            ex = val(-20, 180, 0);
            ey = val(-10, 55, 0);
            erx = 1 + rnd() % 70;
            ery = 1 + rnd() % 40;
            emode = rnd() % 3;
            if (emode == 2) {
                int x, y;
                for (y = 0; y < 44; ++y)
                    for (x = 0; x < 160; x += 3)
                        if (rnd() % 3 == 0) {
                            fx = x; fy = y; fv = 3; c_fld_plot();
                        }
                memcpy(prev, fld_store, sizeof(prev));
#ifdef CHECK_MARKS
                memset(fld_cd, 0, sizeof(fld_cd));
#endif
                fv = 1;
            }
            c_fld_ellipse();
        }
#ifdef CHECK_MARKS
        {
            int x, y;
            for (y = 0; y < 44; ++y)
                for (x = 0; x < 160; ++x) {
                    uint8_t o = (prev[y * 40 + x / 4] >> ((x & 3) * 2)) & 3;
                    uint8_t w = (fld_store[y][x / 4] >> ((x & 3) * 2)) & 3;
                    int cx = x / 2, cy = y / 2;
                    if (o != w && !(fld_cd[cy][cx / 8] & (1 << (cx & 7)))) {
                        if (bad++ < 5)
                            printf("op %d: block %d,%d changed, cell not marked\n", op, x, y);
                    }
                }
        }
#endif
        printf("%d %d %08x\n", k, op, hash());
    }
    return bad != 0;
}
