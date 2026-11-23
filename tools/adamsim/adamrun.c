/*
 * adamrun.c -- the Adam game offline, in adamcore's library (FujiNet Go
 * Adam's core), with no FujiNet: the test scenes, the exactness checks,
 * the pictures and the sound, as fast as the host goes.
 *
 *   adamrun DDP MAP SCENARIO [OUTDIR]
 *
 * The ROMs come from $ROMS (default ~/Workspace/adam-roms: OS7.rom,
 * EOS.rom, WP.rom). The Adam powers on into SmartWriter (EOS then in RAM
 * above D390h); a stub switches the lower 32K to RAM, the game's blocks
 * (1..N of DDP, as the bootstrap would read them) go in at 0000h, and it
 * runs from 0 with SP D390h. A symbol is a name from MAP (z88dk's), with
 * +n or a hex address (0x...). The scenario's lines:
 *
 *   wait N                    N frames
 *   until SYM V [N]           frames till the byte at SYM is V (N at most: 3600)
 *   untilne SYM V [N]         ... till it isn't V
 *   poke SYM V...             bytes (decimal or 0x..)
 *   pokew SYM V               a 16-bit word, little-endian
 *   load SYM FILE             the file's bytes at SYM
 *   peek SYM [N]              N bytes (1), printed in hex
 *   peekw SYM                 a word, printed in decimal
 *   dump FILE [ADDR LEN]      RAM (all 64K) to OUTDIR/FILE
 *   vram FILE                 the 16K of VRAM, then the 8 registers
 *   shot FILE                 the picture (PPM, 256x212) to OUTDIR/FILE
 *   key C                     a character on the keyboard (13 RETURN...)
 *   joy HI LO                 controller 1's word (idle 0x7F 0x7F)
 *   wav FILE                  the sound from here to the end, OUTDIR/FILE
 *   serve SYM DIR             test.c's segments on request (FIX_EXTERN):
 *                             while running, when the byte at SYM is
 *                             80h|k (k < 32), DIR/segK.bin goes into script[] and
 *                             its length into seg_len, and SYM back to 0
 *   prof LO HI N              N frames, the cycles spent with the PC in
 *                             LO..HI-1 (symbols) and the times it came in
 *   hist FILE N               N frames, the cycles at each PC (a profile:
 *                             FILE has "PC cycles" lines, most first)
 *   regs                      the CPU's registers, printed
 *   echo TEXT                 printed
 *
 * Exits 0 when the scenario ends; 2 if an `until` runs out.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "adamcore.h"
#include "adamcore_debug.h"

static adamcore *mc;
static const char *outdir = ".";
static FILE *wavf;
static uint32_t wav_n;
static uint64_t frames;

/* ---- the map ---- */
typedef struct { char *name; uint16_t a; } sym_t;
static sym_t *syms;
static int nsyms;

static void read_map(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[512], name[256];
    unsigned a;
    if (!f) { perror(path); exit(1); }
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "%255s = $%x", name, &a) == 2) {
            syms = realloc(syms, (nsyms + 1) * sizeof *syms);
            syms[nsyms].name = strdup(name);
            syms[nsyms].a = (uint16_t)a;
            ++nsyms;
        }
    fclose(f);
}

static uint16_t addr_of(const char *s)
{
    char base[256];
    const char *plus = strchr(s, '+');
    long off = plus ? strtol(plus + 1, NULL, 0) : 0;
    int i, n = plus ? (int)(plus - s) : (int)strlen(s);
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        return (uint16_t)(strtol(s, NULL, 16) + off);
    snprintf(base, sizeof base, "%.*s", n, s);
    for (i = 0; i < nsyms; ++i)
        if (!strcmp(syms[i].name, base))
            return (uint16_t)(syms[i].a + off);
    fprintf(stderr, "adamrun: no symbol %s\n", base);
    exit(1);
}

/* ---- profiling: an exec hook charges each instruction's cycles to its PC */
static uint16_t pr_lo, pr_hi, pr_last;
static uint64_t pr_cyc, pr_prev, pr_in;
static uint64_t *pr_hist;

static int prof_hook(void *ud, adamcore *c, uint16_t pc)
{
    adamcore_z80_regs r;
    (void)ud;
    adamcore_get_regs(c, &r);
    if (pr_prev) {
        uint64_t d = r.cycles - pr_prev;
        if (pr_hist)
            pr_hist[pr_last] += d;
        else if (pr_last >= pr_lo && pr_last < pr_hi) {
            pr_cyc += d;
            if (!(pc >= pr_lo && pc < pr_hi))
                ++pr_in;
        }
    }
    pr_prev = r.cycles;
    pr_last = pc;
    return 0;
}

/* ---- running ---- */
static char serve_dir[512];
static uint16_t serve_at, serve_script, serve_len;

static void after(void);

static void frame(void)
{
    adamcore_run_frame(mc);
    after();
}

/* a frame done: the sound kept, a segment served */
static void after(void)
{
    ++frames;
    if (wavf) {
        int16_t buf[736];
        adamcore_render_audio(mc, buf, 736);
        fwrite(buf, 2, 736, wavf);
        wav_n += 736;
    }
    if (serve_dir[0]) {
        uint8_t r = adamcore_peek(mc, serve_at);
        if ((r & 0xE0) == 0x80) {       /* (80h-9Fh: A5h and DDh are test.c's own marks) */
            char path[600];
            uint8_t seg[4096];
            size_t n, i;
            FILE *f;
            snprintf(path, sizeof path, "%s/seg%d.bin", serve_dir, r & 0x7F);
            f = fopen(path, "rb");
            if (!f) { perror(path); exit(1); }
            n = fread(seg, 1, sizeof seg, f);
            fclose(f);
            for (i = 0; i < n; ++i)
                adamcore_poke(mc, (uint16_t)(serve_script + i), seg[i]);
            adamcore_poke(mc, serve_len, (uint8_t)n);
            adamcore_poke(mc, (uint16_t)(serve_len + 1), (uint8_t)(n >> 8));
            adamcore_poke(mc, serve_at, 0);
        }
    }
}

static void wav_close(void)
{
    uint32_t bytes = wav_n * 2, v;
    if (!wavf)
        return;
    fseek(wavf, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, wavf); v = 36 + bytes; fwrite(&v, 4, 1, wavf);
    fwrite("WAVEfmt ", 1, 8, wavf); v = 16; fwrite(&v, 4, 1, wavf);
    { uint16_t h[2] = { 1, 1 }; fwrite(h, 2, 2, wavf); }
    v = 44100; fwrite(&v, 4, 1, wavf); v = 88200; fwrite(&v, 4, 1, wavf);
    { uint16_t h[2] = { 2, 16 }; fwrite(h, 2, 2, wavf); }
    fwrite("data", 1, 4, wavf); fwrite(&bytes, 4, 1, wavf);
    fclose(wavf);
    wavf = NULL;
}

static FILE *out(const char *name, const char *mode)
{
    char path[1024];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s", outdir, name);
    f = fopen(path, mode);
    if (!f) { perror(path); exit(1); }
    return f;
}

static void shot(const char *name)
{
    int w, h, i;
    const uint16_t *fb = adamcore_framebuffer(mc, &w, &h);
    FILE *f = out(name, "wb");
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (i = 0; i < w * h; i++) {
        uint16_t p = fb[i];
        unsigned char rgb[3] = { (unsigned char)(((p >> 11) & 0x1F) << 3),
                                 (unsigned char)(((p >> 5) & 0x3F) << 2),
                                 (unsigned char)((p & 0x1F) << 3) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/* power on into SmartWriter, the lower 32K to RAM, the program in, run it */
static void boot(const char *ddp_path, const char *map_path)
{
    static uint8_t ddp[256 * 1024];
    adamcore_z80_regs r;
    FILE *f = fopen(ddp_path, "rb");
    size_t n, i, end = addr_of("__DATA_END_tail");
    static const uint8_t stub[] = { 0x3E, 0x01, 0xD3, 0x7F, 0x76 };   /* LD A,1; OUT (7Fh),A; HALT */
    (void)map_path;
    if (!f) { perror(ddp_path); exit(1); }
    n = fread(ddp, 1, sizeof ddp, f);
    fclose(f);
    if (n < 1024 + end) { fprintf(stderr, "adamrun: %s is short\n", ddp_path); exit(1); }
    for (i = 0; i < 240; ++i)
        adamcore_run_frame(mc);
    for (i = 0; i < sizeof stub; ++i)
        adamcore_poke(mc, (uint16_t)(0xC800 + i), stub[i]);
    adamcore_get_regs(mc, &r);
    r.pc = 0xC800;
    r.halted = 0;
    adamcore_set_regs(mc, &r);
    adamcore_debug_run(mc, 3, 0, NULL);
    for (i = 0; i < (end / 1024 + 1) * 1024; ++i)
        adamcore_poke(mc, (uint16_t)i, ddp[1024 + i]);
    {   /* the art block (the font): at A700h, as art_load reads it (chain.asm) */
        uint16_t t = addr_of("_chain_table");
        size_t blk = ddp[1024 + t + 10] | ddp[1024 + t + 11] << 8;
        if (ddp[1024 + t + 12] && (blk + 1) * 1024 <= n)
            for (i = 0; i < 1024; ++i)
                adamcore_poke(mc, (uint16_t)(0xA700 + i), ddp[blk * 1024 + i]);
    }
    adamcore_get_regs(mc, &r);
    r.pc = 0;
    r.sp = 0xD390;
    r.halted = 0;
    r.iff1 = r.iff2 = 0;
    adamcore_set_regs(mc, &r);
}

static uint8_t *slurp(const char *path, size_t max, size_t *n)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    if (!f) { perror(path); exit(1); }
    b = malloc(max);
    *n = fread(b, 1, max, f);
    fclose(f);
    return b;
}

int main(int argc, char **argv)
{
    adamcore_config cfg;
    char line[1024], dir[1024];
    const char *roms = getenv("ROMS");
    FILE *sc;
    if (argc < 4) {
        fprintf(stderr, "usage: adamrun DDP MAP SCENARIO [OUTDIR]\n");
        return 1;
    }
    if (argc > 4)
        outdir = argv[4];
    if (!roms) {
        snprintf(dir, sizeof dir, "%s/Workspace/adam-roms", getenv("HOME"));
        roms = dir;
    }
    read_map(argv[2]);
    memset(&cfg, 0, sizeof cfg);
    {
        static char p1[1100], p2[1100], p3[1100];
        snprintf(p1, sizeof p1, "%s/OS7.rom", roms);
        snprintf(p2, sizeof p2, "%s/EOS.rom", roms);
        snprintf(p3, sizeof p3, "%s/WP.rom", roms);
        cfg.os7_rom_path = p1;
        cfg.eos_rom_path = p2;
        cfg.wp_rom_path = p3;
    }
    cfg.start_machine = ADAMCORE_MACHINE_ADAM;
    cfg.joystick_mode = 1;
    cfg.audio_rate = 44100;
    mc = adamcore_create(&cfg);
    if (!mc) { fprintf(stderr, "adamrun: adamcore_create failed (the ROMs?)\n"); return 1; }
    boot(argv[1], argv[2]);
    sc = fopen(argv[3], "r");
    if (!sc) { perror(argv[3]); return 1; }
    while (fgets(line, sizeof line, sc)) {
        char cmd[64], a[512], b[512], c[64];
        int k = sscanf(line, "%63s %511s %511s %63s", cmd, a, b, c);
        if (k < 1 || cmd[0] == '#')
            continue;
        if (!strcmp(cmd, "wait")) {
            long n = strtol(a, NULL, 0);
            while (n-- > 0)
                frame();
        } else if (!strcmp(cmd, "until") || !strcmp(cmd, "untilne")) {
            uint16_t at = addr_of(a);
            uint8_t v = (uint8_t)strtol(b, NULL, 0);
            long n = k > 3 ? strtol(c, NULL, 0) : 3600;
            int ne = cmd[5] == 'n';
            while (n-- > 0 && ((adamcore_peek(mc, at) == v) == ne))
                frame();
            if ((adamcore_peek(mc, at) == v) == ne) {
                fprintf(stderr, "adamrun: %s %s %s ran out (frame %llu)\n", cmd, a, b,
                        (unsigned long long)frames);
                wav_close();
                return 2;
            }
        } else if (!strcmp(cmd, "poke")) {
            uint16_t at = addr_of(a);
            char *p = strchr(line, ' ');
            p = p ? strchr(p + 1, ' ') : NULL;
            while (p && *p) {
                char *e;
                long v = strtol(p, &e, 0);
                if (e == p)
                    break;
                adamcore_poke(mc, at++, (uint8_t)v);
                p = e;
            }
        } else if (!strcmp(cmd, "pokew")) {
            uint16_t at = addr_of(a);
            long v = strtol(b, NULL, 0);
            adamcore_poke(mc, at, (uint8_t)v);
            adamcore_poke(mc, (uint16_t)(at + 1), (uint8_t)(v >> 8));
        } else if (!strcmp(cmd, "load")) {
            size_t n, i;
            uint16_t at = addr_of(a);
            uint8_t *d = slurp(b, 65536, &n);
            for (i = 0; i < n; ++i)
                adamcore_poke(mc, (uint16_t)(at + i), d[i]);
            free(d);
        } else if (!strcmp(cmd, "peek")) {
            uint16_t at = addr_of(a);
            long n = k > 2 ? strtol(b, NULL, 0) : 1, i;
            printf("%s:", a);
            for (i = 0; i < n; ++i)
                printf(" %02X", adamcore_peek(mc, (uint16_t)(at + i)));
            printf("\n");
        } else if (!strcmp(cmd, "peekw")) {
            uint16_t at = addr_of(a);
            printf("%s: %u\n", a, adamcore_peek(mc, at) | adamcore_peek(mc, (uint16_t)(at + 1)) << 8);
        } else if (!strcmp(cmd, "dump")) {
            static uint8_t ram[65536];
            FILE *f = out(a, "wb");
            if (k > 3) {
                uint16_t at = addr_of(b);
                long n = strtol(c, NULL, 0);
                adamcore_peek_block(mc, at, ram, (uint32_t)n);
                fwrite(ram, 1, (size_t)n, f);
            } else {
                adamcore_peek_block(mc, 0, ram, 65536);
                fwrite(ram, 1, 65536, f);
            }
            fclose(f);
        } else if (!strcmp(cmd, "vram")) {
            uint8_t regs[8], st;
            uint16_t va;
            FILE *f = out(a, "wb");
            fwrite(adamcore_vdp_vram(mc), 1, 16384, f);
            adamcore_vdp_state(mc, regs, &st, &va);
            fwrite(regs, 1, 8, f);
            fclose(f);
        } else if (!strcmp(cmd, "shot")) {
            shot(a);
        } else if (!strcmp(cmd, "key")) {
            adamcore_inject_key(mc, (uint8_t)strtol(a, NULL, 0));
        } else if (!strcmp(cmd, "joy")) {
            adamcore_set_joystick(mc, 0, (uint16_t)(strtol(a, NULL, 0) << 8 | strtol(b, NULL, 0)));
        } else if (!strcmp(cmd, "wav")) {
            static const uint8_t hdr[44];
            wavf = out(a, "wb");
            fwrite(hdr, 1, 44, wavf);
        } else if (!strcmp(cmd, "serve")) {
            serve_at = addr_of(a);
            serve_script = addr_of("_script");
            serve_len = addr_of("_seg_len");
            snprintf(serve_dir, sizeof serve_dir, "%s", b);
        } else if (!strcmp(cmd, "prof") || !strcmp(cmd, "hist")) {
            long n, i;
            int ch;
            if (!strcmp(cmd, "prof")) {
                pr_lo = addr_of(a);
                pr_hi = addr_of(b);
                n = strtol(c, NULL, 0);
            } else {
                pr_hist = calloc(65536, sizeof *pr_hist);
                n = strtol(b, NULL, 0);
            }
            pr_cyc = pr_in = pr_prev = 0;
            adamcore_set_exec_hook(mc, prof_hook, NULL);
            for (i = 0; i < n; ++i) {
                adamcore_debug_run(mc, 0, 0, &ch);
                after();
            }
            adamcore_set_exec_hook(mc, NULL, NULL);
            if (pr_hist) {
                FILE *f = out(a, "w");
                long k;
                for (k = 0; k < 65536; ++k)
                    if (pr_hist[k])
                        fprintf(f, "%04lX %llu\n", k, (unsigned long long)pr_hist[k]);
                fclose(f);
                free(pr_hist);
                pr_hist = NULL;
            } else {
                printf("prof %s..%s: %llu cycles over %ld frames, %llu times in\n", a, b,
                       (unsigned long long)pr_cyc, n, (unsigned long long)pr_in);
            }
        } else if (!strcmp(cmd, "regs")) {
            adamcore_z80_regs r;
            adamcore_get_regs(mc, &r);
            printf("PC %04X SP %04X AF %02X%02X BC %02X%02X DE %02X%02X HL %02X%02X IX %04X IY %04X\n",
                   r.pc, r.sp, r.a, r.f, r.b, r.c, r.d, r.e, r.h, r.l, r.ix, r.iy);
        } else if (!strcmp(cmd, "echo")) {
            printf("%s", line + 5);
        } else {
            fprintf(stderr, "adamrun: what is \"%s\"?\n", cmd);
            return 1;
        }
    }
    wav_close();
    return 0;
}
