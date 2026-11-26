/*
 * smsrun.c -- the Master System game offline in FujiNet Go SMS's core
 * (~/Workspace/fujinet-go-sms-desktop: libsms_core, the FujiNet cartridge's
 * model with it): the test scenes, the exactness checks, the pictures and
 * the sound, as fast as the host goes (tools/adamsim/adamrun.c's).
 *
 *   smsrun ROM MAP SCENARIO [OUTDIR]
 *
 * $SMSRUN_MODEL picks the console (sms1 default; sms, smspal, sms1pal,
 * smsj: the Japanese one, its YM2413); $SMSRUN_BOIP a FujiNet to talk to
 * (host:port of fujinet-pc's BoIP; default none). The ROM runs as the cart
 * serves a claimed image, with no BIOS. $SMSRUN_THROTTLE=1 runs it in real
 * time (a live game: the server's clock is the wall's). A symbol is a name from MAP
 * (z88dk's), with +n, or a hex address (0x...). The scenario's lines:
 *
 *   wait N                    N frames
 *   until SYM V [N]           frames till the byte at SYM is V (N at most: 3600)
 *   untilne SYM V [N]         ... till it isn't V
 *   poke SYM V...             bytes (decimal or 0x..), on the bus
 *   pokew SYM V               a 16-bit word, little-endian
 *   load SYM FILE             the file's bytes at SYM (on the bus)
 *   peek SYM [N]              N bytes (1), printed in hex
 *   peekw SYM                 a word, printed in decimal
 *   dump FILE [ADDR LEN]      the CPU's 64K as it sees them, to OUTDIR/FILE
 *   cartram FILE H            the cart's RAM, half H (16K), to OUTDIR/FILE
 *   vram FILE                 the 16K of VRAM, the 32 colours, the 11 registers
 *   shot FILE                 the picture (PPM) to OUTDIR/FILE
 *   pad MASK                  pad 1 held (hex: 1 up 2 down 4 left 8 right
 *                             10 button 1, 20 button 2; 0 none)
 *   pause                     the Pause button, a frame
 *   wav FILE                  the sound from here to the end, OUTDIR/FILE
 *                             (48 kHz, 16-bit mono)
 *   serve SYM DIR             test.c's segments on request (FIX_EXTERN):
 *                             when the byte at SYM is 80h|k (k < 32) (run on to
 *                             when the cart's RAM half 0 is in slot 2), DIR/segK.bin
 *                             goes into script[] and its length into
 *                             seg_len, and SYM back to 0
 *   prof LO HI N              N frames, the cycles spent with the PC in
 *                             LO..HI-1 and the times it came in
 *   hist FILE N               N frames, the cycles at each PC (with the
 *                             bank in slot 2: "PC bank cycles" lines)
 *   regs                      the CPU's registers, printed
 *   echo TEXT                 printed
 *
 * Exits 0 when the scenario ends; 2 if an `until` runs out.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "sms_internal.h"

static sms_machine_t m;
static sms_cart_t *cart;
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
    fprintf(stderr, "smsrun: no symbol %s\n", base);
    exit(1);
}

static uint8_t peek(uint16_t a) { return sms_machine_peek(&m, a); }
static void poke(uint16_t a, uint8_t v) { sms_machine_poke(&m, a, v); }

/* what the mapper's control (FFFCh) last had: its RAM mirror, DFFCh */
static uint8_t slot2_ctl(void) { return m.mainram[0x1FFC]; }

/* ---- running ---- */
static char serve_dir[512];
static uint16_t serve_at, serve_script, serve_len;
static int pause_frames;

static void after(void)
{
    ++frames;
    sms_cart_frame(cart);
    if (wavf) {
        int i;
        for (i = 0; i < m.mix.out_frames; ++i) {
            float s = (m.mix.out[2 * i] + m.mix.out[2 * i + 1]) * 0.5f;
            int v = (int)(s * 32767.0f);
            int16_t o = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
            fwrite(&o, 2, 1, wavf);
            ++wav_n;
        }
    }
    m.mix.out_frames = 0;
    sms_machine_latch_inputs(&m);
    if (pause_frames && !--pause_frames)
        m.pause_live = false;
    if (serve_dir[0] && (peek(serve_at) & 0xE0) == 0x80) {
        uint8_t r;
        long k;
        for (k = 0; k < 200000 && slot2_ctl() != 0x08; ++k)
            sms_machine_step_instruction(&m);   /* (on to the moment the store's half is in) */
        r = peek(serve_at);
        if (slot2_ctl() == 0x08 && (r & 0xE0) == 0x80) {
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
                poke((uint16_t)(serve_script + i), seg[i]);
            poke(serve_len, (uint8_t)n);
            poke((uint16_t)(serve_len + 1), (uint8_t)(n >> 8));
            poke(serve_at, 0);
        }
    }
}

static int throttle;
static struct timespec t0;

static void frame(void)
{
    sms_machine_run_frame(&m);
    after();
    if (throttle) {             /* the frame's moment on the wall clock */
        double hz = m.vdp.is_pal ? 50.0 : 60.0, due = (double)frames / hz, now;
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        now = (double)(t.tv_sec - t0.tv_sec) + (double)(t.tv_nsec - t0.tv_nsec) / 1e9;
        if (due > now) {
            struct timespec d;
            d.tv_sec = (time_t)(due - now);
            d.tv_nsec = (long)((due - now - (double)d.tv_sec) * 1e9);
            nanosleep(&d, NULL);
        }
    }
}

/* a frame an instruction at a time, each one's cycles to fn(pc) */
static void frame_steps(void (*fn)(uint16_t pc, uint8_t bank, uint64_t cyc))
{
    uint64_t f0 = m.vdp.frame_count;
    while (m.vdp.frame_count == f0) {
        uint16_t pc = m.cpu.pc;
        uint8_t bank = (slot2_ctl() & 8) ? 0x80 | ((slot2_ctl() >> 2) & 1) : m.mainram[0x1FFF];
        uint64_t c0 = m.cycles;
        sms_machine_step_instruction(&m);
        fn(pc, bank, m.cycles - c0);
    }
    after();
}

static uint16_t pr_lo, pr_hi;
static int pr_was;
static uint64_t pr_cyc, pr_in;
static uint64_t *pr_hist;

static void prof_fn(uint16_t pc, uint8_t bank, uint64_t cyc)
{
    int in = pc >= pr_lo && pc < pr_hi;
    (void)bank;
    if (in) {
        pr_cyc += cyc;
        if (!pr_was)
            ++pr_in;
    }
    pr_was = in;
}

static void hist_fn(uint16_t pc, uint8_t bank, uint64_t cyc)
{
    pr_hist[(pc >= 0x8000 ? (uint32_t)bank << 16 : 0) | pc] += cyc;
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
    v = SMS_AUDIO_RATE; fwrite(&v, 4, 1, wavf); v = SMS_AUDIO_RATE * 2; fwrite(&v, 4, 1, wavf);
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
    int w = SMS_FB_WIDTH, h = m.vdp.fb_height, i;
    FILE *f = out(name, "wb");
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (i = 0; i < w * h; i++) {
        uint32_t p = m.vdp.fb[i];
        unsigned char rgb[3] = { (unsigned char)(p >> 16), (unsigned char)(p >> 8), (unsigned char)p };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
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

static sms_model_t model_by_id(const char *id)
{
    int i;
    for (i = 0; i < SMS_MODEL_COUNT; i++)
        if (strcmp(sms_models[i].id, id) == 0)
            return (sms_model_t)i;
    fprintf(stderr, "smsrun: no model %s\n", id);
    exit(1);
}

int main(int argc, char **argv)
{
    char line[1024], why[200];
    const char *model = getenv("SMSRUN_MODEL"), *boip = getenv("SMSRUN_BOIP");
    size_t romlen;
    uint8_t *rom;
    FILE *sc;
    if (argc < 4) {
        fprintf(stderr, "usage: smsrun ROM MAP SCENARIO [OUTDIR]\n");
        return 1;
    }
    if (argc > 4)
        outdir = argv[4];
    read_map(argv[2]);
    rom = slurp(argv[1], 4 << 20, &romlen);
    cart = sms_cart_create();
    if (sms_cart_power_on(cart, rom, (uint32_t)romlen, NULL, boip ? boip : "127.0.0.1:1", true,
                          why, sizeof why) != 0) {
        fprintf(stderr, "smsrun: cart: %s\n", why);
        return 1;
    }
    sms_machine_init(&m, model_by_id(model ? model : "sms1"), NULL, 0, NULL, false, false, cart);
    throttle = getenv("SMSRUN_THROTTLE") && atoi(getenv("SMSRUN_THROTTLE"));
    clock_gettime(CLOCK_MONOTONIC, &t0);
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
            while (n-- > 0 && ((peek(at) == v) == ne))
                frame();
            if ((peek(at) == v) == ne) {
                fprintf(stderr, "smsrun: %s %s %s ran out (frame %llu)\n", cmd, a, b,
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
                poke(at++, (uint8_t)v);
                p = e;
            }
        } else if (!strcmp(cmd, "pokew")) {
            uint16_t at = addr_of(a);
            long v = strtol(b, NULL, 0);
            poke(at, (uint8_t)v);
            poke((uint16_t)(at + 1), (uint8_t)(v >> 8));
        } else if (!strcmp(cmd, "load")) {
            size_t n, i;
            uint16_t at = addr_of(a);
            uint8_t *d = slurp(b, 65536, &n);
            for (i = 0; i < n; ++i)
                poke((uint16_t)(at + i), d[i]);
            free(d);
        } else if (!strcmp(cmd, "peek")) {
            uint16_t at = addr_of(a);
            long n = k > 2 ? strtol(b, NULL, 0) : 1, i;
            printf("%s:", a);
            for (i = 0; i < n; ++i)
                printf(" %02X", peek((uint16_t)(at + i)));
            printf("\n");
        } else if (!strcmp(cmd, "peekw")) {
            uint16_t at = addr_of(a);
            printf("%s: %u\n", a, peek(at) | peek((uint16_t)(at + 1)) << 8);
        } else if (!strcmp(cmd, "dump")) {
            FILE *f = out(a, "wb");
            long at = k > 3 ? addr_of(b) : 0, n = k > 3 ? strtol(c, NULL, 0) : 65536, i;
            for (i = 0; i < n; ++i)
                fputc(peek((uint16_t)(at + i)), f);
            fclose(f);
        } else if (!strcmp(cmd, "cartram")) {
            static uint8_t sram[1 << 20];
            FILE *f = out(a, "wb");
            long h = strtol(b, NULL, 0);
            uint32_t n = sms_cart_copy_sram(cart, sram, sizeof sram);
            if (n < sizeof sram) { fprintf(stderr, "smsrun: the SRAM is %u bytes\n", n); return 1; }
            fwrite(sram + 0xF8000 + h * 0x4000, 1, 0x4000, f);
            fclose(f);
        } else if (!strcmp(cmd, "vram")) {
            FILE *f = out(a, "wb");
            fwrite(m.vdp.vram, 1, 16384, f);
            fwrite(m.vdp.CRAM, 1, 32, f);
            fwrite(m.vdp.reg, 1, 11, f);
            fclose(f);
        } else if (!strcmp(cmd, "shot")) {
            shot(a);
        } else if (!strcmp(cmd, "pad")) {
            m.pad_live[0] = (uint8_t)strtol(a, NULL, 16);
        } else if (!strcmp(cmd, "pause")) {
            m.pause_live = true;
            pause_frames = 2;
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
            if (!strcmp(cmd, "prof")) {
                pr_lo = addr_of(a);
                pr_hi = addr_of(b);
                n = strtol(c, NULL, 0);
            } else {
                pr_hist = calloc(1 << 24, sizeof *pr_hist);
                n = strtol(b, NULL, 0);
            }
            pr_cyc = pr_in = 0;
            pr_was = 0;
            for (i = 0; i < n; ++i)
                frame_steps(pr_hist ? hist_fn : prof_fn);
            if (pr_hist) {
                FILE *f = out(a, "w");
                long q;
                for (q = 0; q < (1 << 24); ++q)
                    if (pr_hist[q])
                        fprintf(f, "%04lX %02lX %llu\n", q & 0xFFFF, q >> 16, (unsigned long long)pr_hist[q]);
                fclose(f);
                free(pr_hist);
                pr_hist = NULL;
            } else {
                printf("prof %s..%s: %llu cycles over %ld frames, %llu times in\n", a, b,
                       (unsigned long long)pr_cyc, n, (unsigned long long)pr_in);
            }
        } else if (!strcmp(cmd, "regs")) {
            printf("PC %04X SP %04X AF %04X BC %04X DE %04X HL %04X IX %04X IY %04X slot2 %02X/%02X\n",
                   m.cpu.pc, m.cpu.sp, m.cpu.af, m.cpu.bc, m.cpu.de, m.cpu.hl, m.cpu.ix, m.cpu.iy,
                   slot2_ctl(), m.mainram[0x1FFF]);
        } else if (!strcmp(cmd, "echo")) {
            printf("%s", line + 5);
        } else {
            fprintf(stderr, "smsrun: what is \"%s\"?\n", cmd);
            return 1;
        }
    }
    wav_close();
    return 0;
}
