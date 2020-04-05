/*
  VGMCK (Video Game Music Compiler Kit) version 1.0
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

static const unsigned char noise[16]={1,9,2,10,3,5,13,6,14,7,15,11,4,8,12,0};

typedef struct Event1 {
  Event;
  int address;
  int value;
  int period;
} Event1;

static int dual=0;
static long clock;
static unsigned char pan[2]={0xFF,0xFF};
static int vol;

static void x_chip_enable(ChipDef*info) {
  clock=info->options['H'];
}

static void x_file_begin(void) {
  fputc(0xB3,outfp);
  fputc(0x16,outfp);
  fputc(0xFF,outfp);
  fputc(0xB3,outfp);
  fputc(0x14,outfp);
  fputc(0x77,outfp);
  fputc(0xB3,outfp);
  fputc(0x15,outfp);
  fputc(0xFF,outfp);
  if(dual) {
    fputc(0xB3,outfp);
    fputc(0x96,outfp);
    fputc(0xFF,outfp);
    fputc(0xB3,outfp);
    fputc(0x94,outfp);
    fputc(0x77,outfp);
    fputc(0xB3,outfp);
    fputc(0x95,outfp);
    fputc(0xFF,outfp);
  }
  return;
}

static void x_file_end(void) {
  long k=ftell(outfp);
  fseek(outfp,0x80,SEEK_SET);
  fputc(clock&255,outfp); fputc((clock>>8)&255,outfp);
  fputc((clock>>16)&255,outfp); fputc(((!!dual)<<6)|(clock>>24)&255,outfp);
  fseek(outfp,k,SEEK_SET);
}

static void x_loop_start(void) {
  return;
}

static void x_send(Event*ev) {
  Event1*e=(Event1*)ev;
  int a=channel[e->chan].chip_sub;
  int b=channel[e->chan].chan_sub;
  int c=b>!a;
  int d=a+(b&!a)+!!a;
  dual|=c;
  if(e->address<0) {
    switch(~e->address) {
      case 0: // Stereo
        fputc(0xB3,outfp);
        fputc((c<<7)|0x15,outfp);
        fputc(pan[c]=(pan[c]&~e->value)|e->period,outfp);
        break;
      case 1: // Wave table
        for(a=0;a<16;a++) {
          b=macro_env[MC_Waveform][e->value].data[a<<1]<<4;
          b|=macro_env[MC_Waveform][e->value].data[(a<<1)|1];
          fputc(0xB3,outfp);
          fputc((c<<7)|0x20|a,outfp);
          fputc(b,outfp);
        }
        break;
      case 2: // Note on
      case 3: // Note change
        if(e->address==~2) {
          fputc(0xB3,outfp);
          fputc((c<<7)|(d*5+2*(a!=1)),outfp);
          fputc(e->value,outfp);
        }
        fputc(0xB3,outfp);
        fputc((c<<7)|(d*5+3),outfp);
        fputc(e->period&255,outfp);
        fputc(0xB3,outfp);
        fputc((c<<7)|(d*5+4),outfp);
        fputc((e->period>>8)|((e->address==~2)<<7),outfp);
        break;
    }
  } else {
    fputc(0xB3,outfp);
    fputc(e->address,outfp);
    fputc(e->value,outfp);
  }
}

static void x_start_channel(ChipDef*info,int chan) {
  info->clock_div=channel[chan].chip_sub==2?0:-clock;
  vol=0xF0;
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
  Event1*e;
  int a=channel[chan].chip_sub;
  int b=channel[chan].chan_sub;
  int c=b>!a;
  int d=a+(b&!a)+!!a;
  switch(command) {
    case MC_Panning:
      e=malloc(sizeof(Event1));
      e->address=~0;
      e->value=0x11<<d;
      if(value<0) e->period=0x10<<d;
      if(value>0) e->period=0x01<<d;
      if(value==0) e->period=0x11<<d;
      return (Event*)e;
    case MC_Volume:
      if(a==1) {
        if(vol!=value) {
          vol=value;
          e=malloc(sizeof(Event1));
          e->address=(c<<7)|0x0C;
          e->value=(4-value)<<5;
          return (Event*)e;
        }
      } else {
        vol&=0x0F;
        vol|=value<<4;
      }
      return 0;
    case MC_VolumeEnv:
      vol&=0xF0;
      vol|=value<=0?(-value):(value|8);
      return 0;
    case MC_Waveform:
      e=malloc(sizeof(Event1));
      e->address=~1;
      e->value=value&255;
      return (Event*)e;
    case MC_Tone:
      e=malloc(sizeof(Event1));
      e->address=(c<<7)|(b*5+1);
      e->value=value<<6;
      return (Event*)e;
    default:
      return 0;
  }
}

static Event*x_note_on(int chan,long note,int oct,long dur) {
  Event1*e=malloc(sizeof(Event1));
  int a=channel[chan].chip_sub;
  if(a==2) note=noise[note&15]|((15^oct)<<4);
  e->address=~2;
  e->value=vol|((a==1)<<7);
  e->period=note^0x7FF;
  return (Event*)e;
}

static Event*x_note_change(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  int a=channel[chan].chip_sub;
  int b=channel[chan].chan_sub;
  int c=b>!a;
  if(a==2) {
    e->address=(c<<7)|0x12;
    e->value=noise[note&15]|((15^oct)<<4);
  } else {
    e->address=~3;
    e->period=note^0x7FF;
  }
  return (Event*)e;
}

static Event*x_note_off(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  int a=channel[chan].chip_sub;
  int b=channel[chan].chan_sub;
  int c=b>!a;
  int d=a+(b&!a)+!!a;
  e->address=(c<<7)|(a==1?0x0A:d*5+2);
  e->value=0x00;
  return (Event*)e;
}

static Event*x_rest(int chan,long dur) {
  return 0;
}

static Event*x_direct(int chan,int address,int value) {
  Event1*e=malloc(sizeof(Event1));
  e->address=address;
  e->value=value;
  return (Event*)e;
}

Auto_Constructor(
  .name="GAMEBOY",
  .chip_id=CH_GB_DMG,
  .note_bits=11,
  .basic_octave=1,
  .options={ ['H']=4194304 }
)
