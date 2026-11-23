/*
 * msdos.h -- what src/msdos's files share (the PC: CGA in src/msdos/cga,
 * VGA and MCGA in src/msdos/vga; the rest is both's).
 */
#ifndef MSDOS_H
#define MSDOS_H

#include <stdint.h>

/* the four programs (tools/msdos/build.mk): each build's game and menu */
#ifdef DOS_VGA
#define GAME_PROG "BARRVGA"
#define MENU_PROG "BMENUVGA"
#define TWIN_GAME "BARRAGE"     /* the CGA build's: a machine without VGA */
#define TWIN_MENU "BMENU"
#else
#define GAME_PROG "BARRAGE"
#define MENU_PROG "BMENU"
#endif
#ifdef MENU
#define THIS_PROG MENU_PROG
#else
#define THIS_PROG GAME_PROG
#endif

/* plat.c: the machine */
enum { PAL_CGA, PAL_EGA, PAL_PCJR, PAL_TANDY };     /* who sets the palette */
extern uint8_t plat_pcjr;       /* F000:FFFE is FDh */
extern uint8_t plat_pal;        /* PAL_*: a CGA by INT 10h AH=0Bh; the others'
                                 * palette registers by AH=10h too */
extern uint8_t plat_drew;       /* this pass drew */
uint8_t plat_net_probe(void);   /* FUJINET.SYS is there */
/* (tools/dossim) our PSP, the memory DOS has (paragraphs), our far block,
 * the largest block left after it */
extern uint16_t plat_mem[4];
void plat_restore(void);
void plat_chain(const char *prog);  /* the program typed at DOS's prompt: we end */

/* video.c (each build's): the screen */
void video_init(void);          /* the mode, the palette (plat_init) */
void video_frame(void);         /* a jiffy drawn: the flash, the shake, the fire */
extern uint8_t shake_n;         /* frames of shaking left (the player's OP_SHAKE) */
void vid_field_init(void);      /* the field's tables (field_init) */
void vid_field_loading(void);   /* a whole field comes over a text screen */
void vid_field_open(uint8_t sky, uint8_t pal);  /* the round's look */
void vid_flash(uint8_t frames);
void vid_fire(uint8_t on);

/* the speaker: the timer ticks SND_SUB times a jiffy (plat.c), and the top
 * two voices take turns on it, a tick each (sound.c; SND_SUB=1: one voice) */
#ifndef SND_SUB
#define SND_SUB 4
#endif
#if SND_SUB > 1 && !defined(SND_MIX)
#define SND_MIX
#endif

/* sound.c */
void snd_tick(void);            /* the jiffies since: the effects' and tunes' steps */
void snd_off(void);
void snd_mute_toggle(void);

/* input.c */
void input_restore(void);

/* nstrans.c, fbport.c */
void nst_close_quiet(void);     /* the socket closed, before quitting */
void fb_off(void);              /* the serial port given back to FUJINET.SYS */
uint8_t fb_busy(void);          /* a reply is on its way */

/* lobby.c */
void lobby_return(void);
void type_in(const char *k);

#endif
