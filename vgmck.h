/*
  VGMCK (Video Game Music Compiler Kit) version 1.1
  Licensed under GNU GPL v3 or later version.
*/

#define VERSION "1.1"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// List of chips (numbered to use with stream control)
#define CH_SN76489 0 // Sega Game Gear PSG
#define CH_YM2413 1 // OPLL
#define CH_YM2612 2 // OPN2
#define CH_YM2151 3 // OPM
#define CH_SegaPCM 4
#define CH_RF5C68 5 // FM Towns
#define CH_YM2203 6 // OPN
#define CH_YM2608 7 // OPNA
#define CH_YM2610_B 8 // OPNB
#define CH_YM3812 9 // OPL2
#define CH_YM3526 10 // OPL
#define CH_Y8950 11 // MSX
#define CH_YMF262 12 // OPL3
#define CH_YMF278B 13 // OPL4
#define CH_YMF271 14 // OPX
#define CH_YMZ280B 15 // 8-voice PCM/ADPCM
#define CH_RF5C164 16 // Mega-CD
#define CH_PWM 17
#define CH_AY8910 18 // AY-3-8910
#define CH_GB_DMG 19 // GameBoy
#define CH_NES_APU 20 // Famicom 2A03
#define CH_MultiPCM 21
#define CH_uPD7759 22 // Sega System C-2
#define CH_OKIM2658 23
#define CH_OKIM6295 24
#define CH_K051649 25
#define CH_K054539 26
#define CH_HuC6280 27
#define CH_C140 28
#define CH_K053260 29
#define CH_Pokey 30 // Atari HCS
#define CH_QSound 31
#define MAX_CHIP 32

// List of macro commands
#define MC_Volume 0 // v @v @vr
#define MC_Panning 1 // P @P .
#define MC_Tone 2 // @ @@ .
#define MC_Option 3 // . @x @xr
#define MC_Arpeggio 4 // . @EN .
#define MC_Global 5 // @G . .
#define MC_Multiply 6 // M @M .
#define MC_Waveform 7 // @W @W .
#define MC_ModWaveform 8 // @WM . .
#define MC_VolumeEnv 9 // ve . .
#define MC_Sample 10 // @S @S .
#define MC_SampleList 11 // @SL @SL .
#define MC_MIDI 12 // . @MIDI .
#define MAX_MACRO 13

// Event structure
typedef struct Event {
  struct Event*next;
  signed char chan;
  long time;
  char _data[0];
} Event;

// Chip definition structure
typedef struct ChipDef {
  struct ChipDef*next;
  char name[16];
  unsigned char chip_id;
  unsigned char used;
  signed int options[128];
  int clock_div; // negative for note periods, positive for frequencies, 0 for note numbers only
  int note_bits; // negative to not shift by octave
  int basic_octave; // basic octave number
  void(*chip_enable)(struct ChipDef*info);
  void(*file_begin)(void);
  void(*file_end)(void);
  void(*loop_start)(void);
  void(*send)(Event*ev);
  void(*start_channel)(struct ChipDef*info,int chan);
  Event*(*set_macro)(int chan,int dyn,int command,signed short value);
  Event*(*note_on)(int chan,long note,int oct,long dur);
  Event*(*note_change)(int chan,long note,int oct);
  Event*(*note_off)(int chan,long note,int oct);
  Event*(*rest)(int chan,long dur);
  Event*(*direct)(int chan,int address,int value);
} ChipDef;

// Channel definition structure
typedef struct ChanDef {
  ChipDef*chip;
  int chip_sub;
  int chan_sub;
  long loop_point;
  long duration;
  char*text;
} ChanDef;

// Macro envelope structure
#define MAX_MACROENV 2048
typedef struct MacroEnv {
  signed int loopstart; // -1 if no loop
  signed int loopend;
  signed short data[MAX_MACROENV];
  char text[64];
} MacroEnv;

// Macro type structure
typedef struct MacroDef {
  char stat[8];
  char dyn[8];
  char dynrel[8];
} MacroDef;

// Sample loading structure
typedef struct SampleLoader {
  unsigned char id;
  FILE*fp;
  signed char bit_file; // 8 or 16 (negative for signed samples)
  signed char bit_conv; // 8 or 16, or zero for raw data
  char endian; // 0=small 1=big
  long count; // total number of sample frames (not related to Mayan calendars)
  char loop_mode; // 0=off 1=on 2=bidirectional
  long loop_start;
  long loop_end; // 1 beyond last sample frame!
  unsigned long clock; // clock to calculate required sample rate
  unsigned char header_size;
  long head_start;
  union {
    long data_start;
    void*data;
  };
} SampleLoader;

// Global variables
extern FILE*outfp;
extern ChipDef*chipsets;
extern ChanDef channel[52];
extern Event*events;
extern long total_samples;
extern MacroEnv macro_env[MAX_MACRO][256];

// More macros
#define VGM_VERSION 0x161
#define VGM_MAX_HEADER 48
#define Constructor(n) static void __attribute__((constructor(n))) constructorfunc(void)
#define Auto_Constructor(...) \
  Constructor(200) { \
    ChipDef x={__VA_ARGS__}; \
    ChipDef*y=malloc(sizeof(ChipDef)); \
    x.chip_enable=x_chip_enable; \
    x.file_begin=x_file_begin; \
    x.file_end=x_file_end; \
    x.loop_start=x_loop_start; \
    x.send=x_send; \
    x.start_channel=x_start_channel; \
    x.set_macro=x_set_macro; \
    x.note_on=x_note_on; \
    x.note_change=x_note_change; \
    x.note_off=x_note_off; \
    x.rest=x_rest; \
    x.direct=x_direct; \
    x.next=chipsets; \
    *y=x; \
    chipsets=y; \
  }

// Functions
void figure_out_note_values(ChipDef*chip);
SampleLoader*sam_open(int id,long clock,int bits);
void sam_close(SampleLoader*sam);
void sam_read(SampleLoader*sam,void*dest,int start,int count);
void sam_header(SampleLoader*sam,int max,signed short*data);
