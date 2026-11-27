/*
 * nesrun.cpp -- the NES game offline (or live) in FujiNet Go NES's core
 * (~/Workspace/fujinet-go-nes-desktop: MesenCE with the FujiNet cartridge's
 * board): the test scenes, the exactness checks, the pictures, as fast as
 * the host goes (tools/smssim/smsrun.c's language).
 *
 *   nesrun ROM MAP SCENARIO [OUTDIR]
 *
 * The machine is stepped a frame at a time: a listener of the emulator's
 * own holds its thread at the end of each frame (PpuFrameDone) until the
 * scenario wants more, so everything the scenario reads and writes is
 * between frames. $NESRUN_THROTTLE=1 paces the frames in real time (a
 * live game: the server's clock is the wall's); $NESRUN_BOIP a FujiNet to
 * talk to (host:port of fujinet-pc's BoIP; default none); $NESRUN_REGION
 * pal for a 50 Hz console. A symbol is a name from MAP (ld65's exports),
 * with +n, or a hex address (0x...). The scenario's lines:
 *
 *   wait N                    N frames
 *   until SYM V [N]           frames till the byte at SYM is V (N at most: 3600)
 *   untilne SYM V [N]         ... till it isn't V
 *   poke SYM V...             bytes (decimal or 0x..), as the CPU sees them
 *   pokew SYM V               a 16-bit word, little-endian
 *   load SYM FILE             the file's bytes at SYM
 *   peek SYM [N]              N bytes (1), printed in hex
 *   peekw SYM                 a word, printed in decimal
 *   dump FILE [ADDR LEN]      the CPU's 64K as it sees them (the banks in
 *                             place), to OUTDIR/FILE
 *   vram FILE                 the PPU's memory: CHR-RAM (32K), the name
 *                             tables (2K), the palettes (32), OAM (256)
 *   shot FILE                 the picture (PPM, 256x240) to OUTDIR/FILE
 *   pad MASK                  pad 1 held (hex: 1 up 2 down 4 left 8 right
 *                             10 A 20 B 40 SELECT 80 START; 0 none)
 *   serve SYM DIR             test.c's segments on request (FIX_EXTERN):
 *                             when the byte at SYM is 80h|k (k < 32),
 *                             DIR/segK.bin goes into _script and its
 *                             length into _seg_len, and SYM back to 0
 *   regs                      the CPU's registers, printed
 *   echo TEXT                 printed
 *
 * Exits 0 when the scenario ends; 2 if an `until` runs out.
 */
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <climits>
#include <unistd.h>
#include <sys/stat.h>

#include "nessession.h"
#include "session_internal.h"
#include "MesenHost.h"
#include "Shared/Emulator.h"
#include "Shared/EmuSettings.h"
#include "Shared/NotificationManager.h"
#include "Shared/Interfaces/INotificationListener.h"
#include "NES/NesConsole.h"
#include "NES/NesMemoryManager.h"
#include "NES/NesCpu.h"

static nessession *S;
static MesenHost *host;
static Emulator *emu;
static const char *outdir = ".";
static FILE *out;                       /* the real stdout: FujiNet captures fd 1 */
static int throttle;

/* ---- stepping: the emulator's thread waits at each frame's end ---- */
static std::mutex mu;
static std::condition_variable cv;
static uint64_t frames, target;
static bool held;

class Stepper : public INotificationListener
{
public:
	void ProcessNotification(ConsoleNotificationType type, void *) override
	{
		if(type != ConsoleNotificationType::PpuFrameDone)
			return;
		std::unique_lock<std::mutex> lk(mu);
		frames++;
		if(frames >= target) {
			held = true;
			cv.notify_all();
			cv.wait(lk, [] { return frames < target; });
			held = false;
		}
	}
};

/* run n frames: back when the machine is held again */
static void run(uint64_t n)
{
	std::unique_lock<std::mutex> lk(mu);
	cv.wait(lk, [] { return held; });
	target = frames + n;
	cv.notify_all();
	cv.wait(lk, [] { return held && frames >= target; });
}

static auto pace_t0 = std::chrono::steady_clock::now();
static uint64_t pace_n;

static void frame(void)
{
	run(1);
	if(throttle) {
		double hz = nessession_refresh_rate(S) == 50 ? 50.007 : 60.0988;
		auto due = pace_t0 + std::chrono::nanoseconds((int64_t)(++pace_n * 1e9 / hz));
		std::this_thread::sleep_until(due);
	}
}

/* ---- memory, between frames ---- */
static NesConsole *con(void)
{
	return (NesConsole *)emu->GetConsole().get();
}

static uint8_t rd(uint16_t a)
{
	return con()->GetMemoryManager()->DebugRead(a);
}

/* (the debugger's write doesn't reach the cartridge's WRAM: the RAMs are
   written in place) */
static void wr(uint16_t a, uint8_t v)
{
	ConsoleMemoryInfo m = { nullptr, 0 };
	uint32_t off = 0;
	if(a < 0x2000) {
		m = emu->GetMemory(MemoryType::NesInternalRam);
		off = a & 0x7FF;
	} else if(a >= 0x6000 && a < 0x8000) {
		m = emu->GetMemory(MemoryType::NesWorkRam);
		if(!m.Memory || !m.Size)
			m = emu->GetMemory(MemoryType::NesSaveRam);
		off = (uint32_t)(a - 0x6000);
	}
	if(m.Memory && off < m.Size)
		((uint8_t *)m.Memory)[off] = v;
	else
		con()->GetMemoryManager()->DebugWrite(a, v, true);
}

/* ---- the map ---- */
struct sym_t { std::string name; uint16_t a; };
static std::vector<sym_t> syms;

static void read_map(const char *path)
{
	FILE *f = fopen(path, "r");
	char line[1024];
	int in = 0;
	if(!f) {
		fprintf(stderr, "nesrun: %s: no map\n", path);
		fflush(stderr);
		_exit(1);
	}
	while(fgets(line, sizeof line, f)) {
		if(!strncmp(line, "Exports list by name:", 21)) {
			in = 1;
			continue;
		}
		if(!strncmp(line, "Exports list by value:", 22))
			break;
		if(!in)
			continue;
		char n1[256], f1[16], n2[256], f2[16];
		unsigned a1, a2;
		int k = sscanf(line, "%255s %x %15s %255s %x %15s", n1, &a1, f1, n2, &a2, f2);
		if(k >= 3)
			syms.push_back({ n1, (uint16_t)a1 });
		if(k >= 6)
			syms.push_back({ n2, (uint16_t)a2 });
	}
	fclose(f);
}

static uint16_t sym(const char *s)
{
	char name[256];
	int off = 0;
	const char *plus = strchr(s, '+');
	if(!strncmp(s, "0x", 2) || !strncmp(s, "0X", 2))
		return (uint16_t)strtol(s, NULL, 0);
	snprintf(name, sizeof name, "%.*s", plus ? (int)(plus - s) : (int)strlen(s), s);
	if(plus)
		off = (int)strtol(plus + 1, NULL, 0);
	for(auto &y : syms)
		if(y.name == name)
			return (uint16_t)(y.a + off);
	fprintf(stderr, "nesrun: no symbol %s\n", name);
	_exit(1);
}

static FILE *outf(const char *name, const char *mode)
{
	char p[1024];
	snprintf(p, sizeof p, "%s/%s", outdir, name);
	FILE *f = fopen(p, mode);
	if(!f) {
		fprintf(stderr, "nesrun: can't write %s\n", p);
		fflush(stderr);
		_exit(1);
	}
	return f;
}

/* ---- test.c's segments ---- */
static int serve_on;
static uint16_t serve_at;
static char serve_dir[512];

static void serve_check(void)
{
	uint8_t v;
	if(!serve_on)
		return;
	v = rd(serve_at);
	if((v & 0xE0) == 0x80) {
		char p[700];
		uint8_t buf[4096];
		snprintf(p, sizeof p, "%s/seg%d.bin", serve_dir, v & 31);
		FILE *f = fopen(p, "rb");
		size_t n = f ? fread(buf, 1, sizeof buf, f) : 0;
		uint16_t at = sym("_script");
		if(f)
			fclose(f);
		for(size_t i = 0; i < n; i++)
			wr((uint16_t)(at + i), buf[i]);
		at = sym("_seg_len");
		wr(at, (uint8_t)n);
		wr((uint16_t)(at + 1), (uint8_t)(n >> 8));
		wr(serve_at, 0);
	}
}

static void step(void)
{
	frame();
	serve_check();
}

/* ---- input ---- */
static unsigned pad_held;

static void pad(unsigned m)
{
	static const int act[8] = { NES_ACT_UP, NES_ACT_DOWN, NES_ACT_LEFT, NES_ACT_RIGHT,
	                            NES_ACT_A, NES_ACT_B, NES_ACT_SELECT, NES_ACT_START };
	for(int i = 0; i < 8; i++)
		if(((m ^ pad_held) >> i) & 1)
			nessession_press(S, NES_TARGET_PORT(0, act[i]), (m >> i) & 1);
	pad_held = m;
}

/* ---- the picture ---- */
static void shot(const char *name)
{
	PpuFrameInfo fi = con()->GetPpuFrame();
	uint16_t *px = (uint16_t *)fi.FrameBuffer;
	FILE *f = outf(name, "wb");
	fprintf(f, "P6\n%u %u\n255\n", fi.Width, fi.Height);
	for(uint32_t i = 0; i < fi.Width * fi.Height; i++) {
		uint32_t c = host->PaletteColor((uint8_t)(px[i] & 0x3F));
		fputc((c >> 16) & 255, f);
		fputc((c >> 8) & 255, f);
		fputc(c & 255, f);
	}
	fclose(f);
}

static void mem_out(FILE *f, MemoryType t, uint32_t want)
{
	ConsoleMemoryInfo m = emu->GetMemory(t);
	static uint8_t zero[65536];
	uint32_t n = m.Memory ? (m.Size < want ? m.Size : want) : 0;
	if(n)
		fwrite(m.Memory, 1, n, f);
	fwrite(zero, 1, want - n, f);
}

static int until(const char *a, const char *b, const char *c, int ne)
{
	uint16_t at = sym(a);
	uint8_t v = (uint8_t)strtol(b, NULL, 0);
	long n = c ? strtol(c, NULL, 0) : 3600;
	while(n-- > 0) {
		if((rd(at) == v) != ne)
			return 0;
		step();
	}
	fprintf(stderr, "nesrun: until %s %s: ran out\n", a, b);
	return 2;
}

int main(int argc, char **argv)
{
	char rom[PATH_MAX], cfgdir[1100];
	if(argc < 4) {
		fprintf(stderr, "usage: nesrun ROM MAP SCENARIO [OUTDIR]\n");
		return 1;
	}
	if(argc > 4)
		outdir = argv[4];
	mkdir(outdir, 0777);
	int fd = dup(1);
	out = fdopen(fd, "w");
	setvbuf(out, NULL, _IOLBF, 0);
	if(!realpath(argv[1], rom)) {
		fprintf(stderr, "nesrun: %s: no image\n", argv[1]);
		return 1;
	}
	read_map(argv[2]);
	FILE *scen = fopen(argv[3], "r");
	if(!scen) {
		fprintf(stderr, "nesrun: %s: no scenario\n", argv[3]);
		return 1;
	}
	throttle = getenv("NESRUN_THROTTLE") && atoi(getenv("NESRUN_THROTTLE"));
	const char *boip = getenv("NESRUN_BOIP");
	const char *region = getenv("NESRUN_REGION");

	/* a settings tree of its own, never the user's */
	snprintf(cfgdir, sizeof cfgdir, "%s/.nesrun", outdir);
	mkdir(cfgdir, 0777);
	nessession_paths paths = { cfgdir, cfgdir, "", NULL };
	S = nessession_new(&paths);
	nessession_start_opts o;
	nessession_default_opts(S, &o);
	o.cart_path = rom;
	o.region = region && !strcmp(region, "pal") ? NES_REGION_PAL : NES_REGION_NTSC;
	o.port_type[0] = NES_CTRL_STANDARD;
	o.port_type[1] = NES_CTRL_NONE;
	o.analog_joystick = 0;
	o.keyboard = NES_KBD_NONE;
	o.enable_fujinet = 0;
	o.external_fujinet = boip ? 1 : 0;
	o.boip_connect = boip;
	o.enable_audio = 0;
	o.enable_gamepad = 0;
	if(nessession_start(S, &o)) {
		fprintf(stderr, "nesrun: %s\n", nessession_last_error(S));
		_exit(1);
	}
	host = (MesenHost *)S->host;
	emu = host->GetEmulator();
	if(!throttle) {
		EmulationConfig ec = emu->GetSettings()->GetEmulationConfig();
		ec.EmulationSpeed = 0;
		emu->GetSettings()->SetEmulationConfig(ec);
	}
	static std::shared_ptr<Stepper> stepper = std::make_shared<Stepper>();
	emu->GetNotificationManager()->RegisterNotificationListener(stepper);
	{
		std::unique_lock<std::mutex> lk(mu);
		cv.wait(lk, [] { return held; });
	}
	pace_t0 = std::chrono::steady_clock::now();

	char line[1024];
	int rc = 0;
	while(!rc && fgets(line, sizeof line, scen)) {
		char cmd[64] = "", a[512] = "", b[512] = "", c[512] = "";
		int k = sscanf(line, "%63s %511s %511s %511s", cmd, a, b, c);
		if(k < 1 || cmd[0] == '#')
			continue;
		if(!strcmp(cmd, "wait")) {
			long n = strtol(a, NULL, 0);
			while(n-- > 0)
				step();
		} else if(!strcmp(cmd, "until") || !strcmp(cmd, "untilne")) {
			rc = until(a, b, k > 3 ? c : NULL, cmd[5] == 'n');
		} else if(!strcmp(cmd, "poke")) {
			uint16_t at = sym(a);
			char *p = line + strlen(cmd);
			while(*p == ' ' || *p == '\t')
				p++;
			p += strlen(a);
			for(char *t = strtok(p, " \t\n"); t; t = strtok(NULL, " \t\n"))
				wr(at++, (uint8_t)strtol(t, NULL, 0));
		} else if(!strcmp(cmd, "pokew")) {
			uint16_t at = sym(a), v = (uint16_t)strtol(b, NULL, 0);
			wr(at, (uint8_t)v);
			wr((uint16_t)(at + 1), (uint8_t)(v >> 8));
		} else if(!strcmp(cmd, "load")) {
			uint16_t at = sym(a);
			FILE *f = fopen(b, "rb");
			int ch;
			if(!f) {
				fprintf(stderr, "nesrun: %s: can't read\n", b);
				rc = 1;
				break;
			}
			while((ch = fgetc(f)) != EOF)
				wr(at++, (uint8_t)ch);
			fclose(f);
		} else if(!strcmp(cmd, "peek")) {
			uint16_t at = sym(a);
			long n = k > 2 ? strtol(b, NULL, 0) : 1;
			fprintf(out, "%s:", a);
			for(long i = 0; i < n; i++)
				fprintf(out, " %02X", rd((uint16_t)(at + i)));
			fprintf(out, "\n");
		} else if(!strcmp(cmd, "peekw")) {
			uint16_t at = sym(a);
			fprintf(out, "%s: %u\n", a, rd(at) | rd((uint16_t)(at + 1)) << 8);
		} else if(!strcmp(cmd, "dump")) {
			FILE *f = outf(a, "wb");
			uint32_t at = k > 3 ? sym(b) : 0, n = k > 3 ? (uint32_t)strtol(c, NULL, 0) : 65536;
			for(uint32_t i = 0; i < n; i++)
				fputc(rd((uint16_t)(at + i)), f);
			fclose(f);
		} else if(!strcmp(cmd, "vram")) {
			FILE *f = outf(a, "wb");
			mem_out(f, MemoryType::NesChrRam, 32768);
			mem_out(f, MemoryType::NesNametableRam, 2048);
			mem_out(f, MemoryType::NesPaletteRam, 32);
			mem_out(f, MemoryType::NesSpriteRam, 256);
			fclose(f);
		} else if(!strcmp(cmd, "shot")) {
			shot(a);
		} else if(!strcmp(cmd, "pad")) {
			pad((unsigned)strtol(a, NULL, 16));
		} else if(!strcmp(cmd, "serve")) {
			serve_on = 1;
			serve_at = sym(a);
			snprintf(serve_dir, sizeof serve_dir, "%s", b);
		} else if(!strcmp(cmd, "regs")) {
			NesCpuState &st = con()->GetCpu()->GetState();
			fprintf(out, "PC=%04X A=%02X X=%02X Y=%02X SP=%02X P=%02X frames=%llu\n", st.PC, st.A, st.X,
			        st.Y, st.SP, st.PS, (unsigned long long)frames);
		} else if(!strcmp(cmd, "echo")) {
			fprintf(out, "%s", line + 5);
		} else {
			fprintf(stderr, "nesrun: %s: what?\n", cmd);
			rc = 1;
		}
	}
	fflush(out);
	_exit(rc);
}
